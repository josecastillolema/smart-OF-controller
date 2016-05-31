 /*
 * Copyright (c) 2012 Federal University of Pará (UFPA) - Brazil
 * Research Group on Computer Networks and Multimedia Communication (GERCOM)
 * Home Page: http://gercom.ufpa.br
 * Author: Airton Ishimori
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * The name and trademarks of copyright holder(s) may NOT be used in
 * advertising or publicity pertaining to the Software or any derivatives
 * without specific, written prior permission.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <lib/libnetlink.h>
#include <lib/ll_map.h>
#include <lib/utils.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>
#include <linux/pkt_cls.h>
#include <linux/gen_stats.h>

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
/*#include <xtoxll.h>*/
#include <math.h>

#include "packet-scheduler.h"
#include "traffic-shaping.h"


/* Timer functions - TEST */

#include <sys/time.h>

/* Added especifically to evaluate QoS functions response time.
   The 'filename' is the path or name of the file where the performance
   information will be write.
 */
char *filename;


static double get_timer (void);
static double timer_diff (double end, double begin);
static void timer_print_to_file (double value, const char *filename);

/*****************************************************************************************/

static int process_qos_clear (struct datapath *dp, const void *msg);
static int process_qos_pfifo (struct datapath *dp, const void *msg);
static int process_qos_bfifo (struct datapath *dp, const void *msg);
static int process_qos_sfq (struct datapath *dp, const void *msg);
static int process_qos_red (struct datapath *dp, const void *msg);
static int process_qos_htb (struct datapath *dp, const void *msg);
static int parse_qos_htb_qdisc (struct datapath *dp, const struct ofp_qos_htb_qdisc *htb);
static int parse_qos_htb_class (struct datapath *dp, const struct ofp_qos_htb_class *htb);

static int find_device_name (struct datapath * const dp, uint16_t port, char *devname);
static void handle_qos_error (struct datapath *, const struct sender *sender, uint16_t err);
static void send_errors_to_logfile (char *msg);
static double extract_probability_value (uint32_t value);

/* Adapted from tc Linux tool */
static int red_eval_ewma (uint32_t qmin, uint32_t burst, uint32_t avpkt);
static int red_eval_P (uint32_t qmin, uint32_t qmax, double prob);
static int red_eval_idle_damping (int Wlog, uint32_t avpkt, uint32_t bps, uint8_t *sbuf);

static double dptime_tick_in_usec = 1;
static double dptime_clock_factor = 1;

/* Main data structure to talk with the kernel */
static struct rtnl_handle rth;


/**************************************************************************************
 *
 * QoS functions - Management
 *
 **************************************************************************************/

/**
 * This function receive qos control messages sent by 'sender' and begin
 * processing of the message 'msg' calling adequated handler for each type
 * of the qos message.
 */
int
recv_packet_scheduler (struct datapath *dp, const struct sender *sender, const void *oh)
{        
        const struct ofp_qos_msg *ofqos_msg;
        double begin, end;
        int error;
        
        /* Handler to qos messages */
        int (*qos_handler) (struct datapath *, const void *msg) = NULL;

        core_init();
        if (rtnl_open(&rth, 0) < 0) {
                send_errors_to_logfile("recv_qos_control_msg(): Cannot open rtnetlink.");
                handle_qos_error(dp, sender, OFP_QOS_ERROR_UNKNOWN_SCHED);
        }

        /* Identify wich type of qos message is */
        ofqos_msg = (struct ofp_qos_msg *) oh;
        switch (ofqos_msg->sched_type) {
        case OFP_QOS_SCHED_HTB:
                qos_handler = process_qos_htb;
                break;

        case OFP_QOS_SCHED_PFIFO:
                qos_handler = process_qos_pfifo;
                break;

        case OFP_QOS_SCHED_BFIFO:
                qos_handler = process_qos_bfifo;
                break;

        case OFP_QOS_SCHED_SFQ:
                qos_handler = process_qos_sfq;
                break;

        case OFP_QOS_SCHED_RED:
                qos_handler = process_qos_red;
                break;

        case OFP_QOS_SCHED_NONE:
                qos_handler = process_qos_clear; 
                break;
 
        default:
                handle_qos_error(dp, sender, OFP_QOS_ERROR_UNKNOWN_SCHED);
                return 0;
        }

        /* Response Time Evaluation */
        /* begin = get_timer(); */
        error = qos_handler(dp, ofqos_msg->body);
        /* end = get_timer(); */
        /*if (ofqos_msg->sched_type != OFP_QOS_SCHED_NONE)
          timer_print_to_file(timer_diff(end, begin), filename);*/

        /* The 'error' variable can be OFP_QOS_ERROR_NONE */
        handle_qos_error(dp, sender, error);

        rtnl_close(&rth);

        /* Resetting global variables */
        dptime_clock_factor = 1;
        dptime_tick_in_usec = 1;
        bzero(&rth, sizeof(rth));

        return 0;
}

/**
 * Clear any rules (qdisc) associated to one port.
 */
static int
process_qos_clear (struct datapath *dp, const void *msg)
{
        struct {
		struct nlmsghdr n;
		struct tcmsg t;
		char buf[TCA_BUF_MAX];
	} req;
        
        int found, ifidx;
        char dev[OFP_MAX_PORT_NAME_LEN];
        struct ofp_qos_clear *clear = (struct ofp_qos_clear *) msg;

        bzero(&dev, sizeof(dev));
        bzero(&req, sizeof(req));
        
        req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_DELQDISC;
	req.t.tcm_family = AF_UNSPEC;

        req.t.tcm_parent = TC_H_ROOT;
        
        found = find_device_name(dp, ntohs(clear->port_no), dev);
        if (!found) {
                return OFP_QOS_ERROR_INVALID_DEV;
        }

        ll_init_map(&rth);
        ifidx = ll_name_to_index(dev);
        req.t.tcm_ifindex = ifidx;
                
        if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
                send_errors_to_logfile("process_qos_clear_msg(): Cannot talk to talk rtnetlink.");
                return OFP_QOS_ERROR_RTNETLINK;
        }

        return OFP_QOS_ERROR_NONE;
}

/**
 * Classless queueing discipline.
 */
static int
process_qos_pfifo (struct datapath *dp, const void *msg)
{
        /* Structure of the netlink packet */
        struct {
                struct nlmsghdr nl;
                struct tcmsg tc;
                char buf[TCA_BUF_MAX];
        } req;

        char dev[OFP_MAX_PORT_NAME_LEN], hex[16];
        int found, ifidx;
        struct sw_port *swport;
        int *fd;
        struct sw_queue *swqueue;
        int add = 0, del = 0, chg = 0;
        
        struct tc_fifo_qopt opt;
        
        struct ofp_qos_pfifo *pfifo = (struct ofp_qos_pfifo *) msg;

        bzero(&req, sizeof(req));
        bzero(&opt, sizeof(opt));
        bzero(dev, OFP_MAX_PORT_NAME_LEN);

        req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
        req.nl.nlmsg_flags = NLM_F_REQUEST;

        req.tc.tcm_family = AF_UNSPEC;
        
        switch (pfifo->qos_hdr.action_type) {
        case OFP_QOS_ACTION_ADD:
                req.nl.nlmsg_flags |= NLM_F_EXCL | NLM_F_CREATE;
                req.nl.nlmsg_type = RTM_NEWQDISC;  /* command */
                add = 1;
                break;
                
        case OFP_QOS_ACTION_DEL:
                req.nl.nlmsg_type = RTM_DELQDISC;  /* command */
                del = 1;
                break;

        case OFP_QOS_ACTION_CHANGE:
                req.nl.nlmsg_type = RTM_NEWQDISC;
                chg = 1;
                break;
                
        default:
                return OFP_QOS_ERROR_UNKNOWN_ACTION;
        }

        /* Find device name */
        found = find_device_name(dp, ntohs(pfifo->port_no), dev);
        if (!found) {
                return OFP_QOS_ERROR_INVALID_DEV;
        }

        if (pfifo->class_id == 0) {
                req.tc.tcm_parent = TC_H_ROOT;
                req.tc.tcm_handle = MAJOR_MINOR(MAJOR_ID, MINOR_ID); /* 1:0 */
        } else {
                sprintf(hex, "%d", ntohl(pfifo->class_id));
                req.tc.tcm_parent = MAJOR_MINOR(MAJOR_ID, strtoul(hex, NULL, 16)); /* 1:class_id */
                req.tc.tcm_handle = MAJOR_MINOR(strtoul(hex, NULL, 16) * 10, MINOR_ID); /* class_id * 10: */
        }

        addattr_l(&req.nl, sizeof(req), TCA_KIND, "pfifo", strlen("pfifo") + 1);

        /* opts */
        opt.limit = ntohl(pfifo->limit);  /* in packets */

        addattr_l(&req.nl, 1024, TCA_OPTIONS, &opt, sizeof(opt));

        ll_init_map(&rth);
        ifidx = ll_name_to_index(dev);
        req.tc.tcm_ifindex = ifidx;

        if (rtnl_talk(&rth, &req.nl, 0, 0, NULL, NULL, NULL) < 0) {
                send_errors_to_logfile("process_qos_pfifo_msg(): Cannot talk to talk rtnetlink.");
                return OFP_QOS_ERROR_RTNETLINK;
        }

        swport = dp_lookup_port(dp, ntohs(pfifo->port_no));
        if (add) {
                /* TODO: add property? */
        } else if (chg) {
                /* TODO: change property? */
        } else {
                /* TODO: del property? */
        }
            

        return OFP_QOS_ERROR_NONE;
}

/**
 * Classless queueing discipline.
 */
static int
process_qos_bfifo (struct datapath *dp, const void *msg)
{
        /* Structure of the netlink packet */
        struct {
                struct nlmsghdr nl;
                struct tcmsg tc;
                char buf[TCA_BUF_MAX];
        } req;

        char dev[OFP_MAX_PORT_NAME_LEN], hex[16];
        int found, ifidx;
        struct sw_port *swport;
        int *fd;
        struct sw_queue *swqueue;
        int add = 0, del = 0, chg = 0;
        
        struct tc_fifo_qopt opt;
        
        struct ofp_qos_bfifo *bfifo = (struct ofp_qos_bfifo *) msg;
        
        bzero(&req, sizeof(req));
        bzero(&opt, sizeof(opt));
        bzero(dev, OFP_MAX_PORT_NAME_LEN);

        req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
        req.nl.nlmsg_flags = NLM_F_REQUEST;

        req.tc.tcm_family = AF_UNSPEC;
        
        switch (bfifo->qos_hdr.action_type) {
        case OFP_QOS_ACTION_ADD:
                req.nl.nlmsg_flags |= NLM_F_EXCL | NLM_F_CREATE;
                req.nl.nlmsg_type = RTM_NEWQDISC;  /* command */
                add = 1;
                break;
                
        case OFP_QOS_ACTION_DEL:
                req.nl.nlmsg_type = RTM_DELQDISC;  /* command */
                del = 1;
                break;

        case OFP_QOS_ACTION_CHANGE:
                req.nl.nlmsg_type = RTM_NEWQDISC;
                chg = 1;
                break;
                
        default:
                return OFP_QOS_ERROR_UNKNOWN_ACTION;
        }

        /* Find device name */
        found = find_device_name(dp, ntohs(bfifo->port_no), dev);
        if (!found) {
                return OFP_QOS_ERROR_INVALID_DEV;
        }

        if (bfifo->class_id == 0) {
                req.tc.tcm_parent = TC_H_ROOT;
                req.tc.tcm_handle = MAJOR_MINOR(MAJOR_ID, MINOR_ID);
        } else {
                sprintf(hex, "%d", ntohl(bfifo->class_id));
                req.tc.tcm_parent = MAJOR_MINOR(MAJOR_ID, strtoul(hex, NULL, 16)); /* 1:class_id */
                req.tc.tcm_handle = MAJOR_MINOR(strtoul(hex, NULL, 16) * 10, MINOR_ID); /* class_id * 10: */
        }

        addattr_l(&req.nl, sizeof(req), TCA_KIND, "bfifo", strlen("bfifo") + 1);

        /* opts */
        opt.limit = ntohl(bfifo->limit);  /* in bytes */

        addattr_l(&req.nl, 1024, TCA_OPTIONS, &opt, sizeof(opt));

        ll_init_map(&rth);
        ifidx = ll_name_to_index(dev);
        req.tc.tcm_ifindex = ifidx;

        if (rtnl_talk(&rth, &req.nl, 0, 0, NULL, NULL, NULL) < 0) {
                send_errors_to_logfile("process_qos_bfifo_msg(): Cannot talk to talk rtnetlink.");
                return  OFP_QOS_ERROR_RTNETLINK;
        }

        swport = dp_lookup_port(dp, ntohs(bfifo->port_no));
        if (add) {
                /* TODO: add property? */
        } else if (chg) {
                /* TODO: change property? */
        } else {
                /* TODO: del property? */
        }


        return OFP_QOS_ERROR_NONE;
}

/**
 * Classless queueing discipline
 */
static int
process_qos_sfq (struct datapath *dp, const void *msg)
{
        struct {
		struct nlmsghdr n;
		struct tcmsg t;
		char buf[TCA_BUF_MAX];
	} req;
        
        struct tc_sfq_qopt opt;

        char dev[OFP_MAX_PORT_NAME_LEN], hex[16];
        int found, ifidx;
        struct sw_port *swport = NULL;
        int *fd;
        struct sw_queue *swqueue;
        int add = 0, del = 0, chg = 0;
        
        struct ofp_qos_sfq *sfq = (struct ofp_qos_sfq *) msg;

        bzero(&req, sizeof(req));
        bzero(&opt, sizeof(opt));
        bzero(dev, OFP_MAX_PORT_NAME_LEN);

        req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;

	req.t.tcm_family = AF_UNSPEC;

        switch(sfq->qos_hdr.action_type) {
        case OFP_QOS_ACTION_ADD:
                req.n.nlmsg_flags |= NLM_F_EXCL | NLM_F_CREATE;
                req.n.nlmsg_type = RTM_NEWQDISC;  /* command */
                add = 1;
                break;
                
        case OFP_QOS_ACTION_DEL:
                req.n.nlmsg_type = RTM_DELQDISC;  /* command */
                del = 1;
                break;

        case OFP_QOS_ACTION_CHANGE:
                req.n.nlmsg_type = RTM_NEWQDISC;
                chg = 1;
                break;

        default:
                return OFP_QOS_ERROR_UNKNOWN_ACTION;
        }

        /* Find device name */
        found = find_device_name(dp, ntohs(sfq->port_no), dev);
        if (!found) {
                return OFP_QOS_ERROR_INVALID_DEV;
        }

        if (sfq->class_id == 0) {
                req.t.tcm_parent = TC_H_ROOT;
                req.t.tcm_handle = MAJOR_MINOR(MAJOR_ID, MINOR_ID); /* 1:0 */
        } else {
                sprintf(hex, "%d", ntohl(sfq->class_id));
                req.t.tcm_parent = MAJOR_MINOR(MAJOR_ID, strtoul(hex, NULL, 16)); /* 1:class_id */
                req.t.tcm_handle = MAJOR_MINOR(strtoul(hex, NULL, 16) * 10, MINOR_ID); /* class_id * 10: */
        }

        addattr_l(&req.n, sizeof(req), TCA_KIND, "sfq", strlen("sfq") + 1);

        /* sfq opts */
        opt.perturb_period = (sfq->perturb == 0) ? 10 : ntohl(sfq->perturb);
        swport = dp_lookup_port(dp, ntohs(sfq->port_no));
        opt.quantum = (sfq->quantum == 0) ? netdev_get_mtu(swport->netdev) : ntohl(sfq->quantum);
        
        addattr_l(&req.n, 1024, TCA_OPTIONS, &opt, sizeof(opt));

        
        ll_init_map(&rth);
        ifidx = ll_name_to_index(dev);
        req.t.tcm_ifindex = ifidx;

        if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
                send_errors_to_logfile("process_qos_sfq_msg(): Cannot talk to kernel.");
                return OFP_QOS_ERROR_RTNETLINK;
        }

        swport = dp_lookup_port(dp, ntohs(sfq->port_no));
        if (add) {
                /* TODO: add property? */
        } else if (chg) {
                /* TODO: change property? */
        } else {
                /* TODO: del property? */
        }
        

        return OFP_QOS_ERROR_NONE;
}

/**
 * Classless queueing discipline
 */
static int
process_qos_red (struct datapath *dp, const void *msg)
{
        struct {
		struct nlmsghdr n;
		struct tcmsg t;
		char buf[TCA_BUF_MAX];
	} req;
        
        struct tc_red_qopt opt;
        struct rtattr *tail;

        char dev[OFP_MAX_PORT_NAME_LEN], hex[16];
        int found, ifidx, rate, avpkt, burst;
        uint8_t sbuf[256];
        struct sw_port *swport;
        int *fd;
        struct sw_queue *swqueue;
        int add = 0, del = 0, chg = 0;
        
        struct ofp_qos_red *red = (struct ofp_qos_red *) msg;

        bzero(&req, sizeof(req));
        bzero(&opt, sizeof(opt));
        bzero(dev, OFP_MAX_PORT_NAME_LEN);

        req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;

	req.t.tcm_family = AF_UNSPEC;

        switch(red->qos_hdr.action_type) {
        case OFP_QOS_ACTION_ADD:
                req.n.nlmsg_flags |= NLM_F_EXCL | NLM_F_CREATE;
                req.n.nlmsg_type = RTM_NEWQDISC;  /* command */
                add = 1;
                break;
                
        case OFP_QOS_ACTION_DEL:
                req.n.nlmsg_type = RTM_DELQDISC;  /* command */
                del = 1;
                break;

        case OFP_QOS_ACTION_CHANGE:
                req.n.nlmsg_type = RTM_NEWQDISC;
                chg = 1;
                break;

        default:
                return OFP_QOS_ERROR_UNKNOWN_ACTION;
        }

        /* Find device name */
        found = find_device_name(dp, ntohs(red->port_no), dev);
        if (!found) {
                return OFP_QOS_ERROR_INVALID_DEV;
        }

        if (red->class_id == 0) {
                req.t.tcm_parent = TC_H_ROOT;
                req.t.tcm_handle = MAJOR_MINOR(MAJOR_ID, MINOR_ID); 
        } else {
                sprintf(hex, "%d", ntohl(red->class_id));
                req.t.tcm_parent = MAJOR_MINOR(MAJOR_ID, strtoul(hex, NULL, 16)); /* 1:class_id */
                req.t.tcm_handle = MAJOR_MINOR(strtoul(hex, NULL, 16) * 10, MINOR_ID); /* class_id: */ 
        }

        addattr_l(&req.n, sizeof(req), TCA_KIND, "red", strlen("red") + 1);

        /* red opts */
        opt.limit = ntohl(red->limit);       /* in bytes */
        opt.qth_min = ntohl(red->treshold);  /* in bytes */

        /* This rate is used for calculating the average queue size
           after some idle time. Should be set to the bandwidth of
           your interface. Does not mean that RED will shape for you!
           More information: http://linux.die.net/man/8/tc-red
        */
        rate = 64000; /* 64 KB/s */
        
        /* Recommendations from http://opalsoft.net/qos/DS-26.htm */
        opt.qth_max = rate * 0.5;
        avpkt = 1000;
        burst = (2 * opt.qth_min + opt.qth_max) / (3 * avpkt);
        
        opt.Wlog = red_eval_ewma(opt.qth_min, burst, avpkt);
        opt.Plog = red_eval_P(opt.qth_min, opt.qth_max, 0.02);
        opt.Scell_log = red_eval_idle_damping(opt.Wlog, avpkt, rate, sbuf);

        addattr_l(&req.n, 1024, TCA_OPTIONS, &opt, sizeof(opt));

        tail = NLMSG_TAIL(&req.n);
	addattr_l(&req.n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(&req.n, 1024, TCA_RED_PARMS, &opt, sizeof(opt));
	addattr_l(&req.n, 1024, TCA_RED_STAB, sbuf, 256);
	tail->rta_len = (void *) NLMSG_TAIL(&req.n) - (void *) tail;

        
        ll_init_map(&rth);
        ifidx = ll_name_to_index(dev);
        req.t.tcm_ifindex = ifidx;

        if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
                send_errors_to_logfile("process_qos_red_msg(): Cannot talk to kernel.");
                return OFP_QOS_ERROR_RTNETLINK;
        }

        swport = dp_lookup_port(dp, ntohs(red->port_no));
        if (add) {
                /* TODO: add property in sw_queue? */
        } else if (chg) {
                /* TODO: change property? */
        } else {
                /* TODO: del property? */
        }
        
        return OFP_QOS_ERROR_NONE;
}


/**
 * This function is called to process 'msg'. Here the 'msg' is HTB message.
 */
static int
process_qos_htb (struct datapath *dp, const void *msg)
{
        struct ofp_qos_header *qos_hdr = (struct ofp_qos_header *) msg;
        struct ofp_qos_htb_qdisc *qdisc_msg = NULL;
        struct ofp_qos_htb_class *class_msg = NULL;

        int val = OFP_QOS_ERROR_NONE;

        /* Verify type of qos object and call adequated function */
        switch (qos_hdr->object_type) {
        case OFP_QOS_QDISC:
                qdisc_msg = (struct ofp_qos_htb_qdisc *) msg;
                val = parse_qos_htb_qdisc(dp, qdisc_msg);
                break;
                
        case OFP_QOS_CLASS:
                class_msg = (struct ofp_qos_htb_class *) msg;
                val = parse_qos_htb_class(dp, class_msg);
                break;
                
        default:
                return OFP_QOS_ERROR_UNKNOWN_OBJECT;
        }

        return val;
}

/**
 * This function parse OFP_QOS_QDISC object type.
 *
 * If success return 0, otherwise return a positive value.
 */
static int
parse_qos_htb_qdisc (struct datapath *dp, const struct ofp_qos_htb_qdisc *htb)
{
        /* Structure of the netlink packet */
        struct {
                struct nlmsghdr nl;
                struct tcmsg tc;
                char buf[TCA_BUF_MAX];
        } req;
    
        struct tc_htb_glob opt;
        struct rtattr *tail;
        char dev[OFP_MAX_PORT_NAME_LEN], hex[16];
        int found, ifidx;
        
        bzero(&req, sizeof(req));
        bzero(&opt, sizeof(opt));
        bzero(dev, OFP_MAX_PORT_NAME_LEN);

        req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
        req.nl.nlmsg_flags = NLM_F_REQUEST;

        req.tc.tcm_family = AF_UNSPEC;
                       
        /* Check type of action */
        switch (htb->qos_hdr.action_type) {
        case OFP_QOS_ACTION_ADD:
                req.nl.nlmsg_flags |= NLM_F_EXCL | NLM_F_CREATE;
                req.nl.nlmsg_type = RTM_NEWQDISC;  /* command */
                break;
                
        case OFP_QOS_ACTION_DEL:
                req.nl.nlmsg_type = RTM_DELQDISC;  /* command */
                break;

        case OFP_QOS_ACTION_CHANGE:
                req.nl.nlmsg_type = RTM_NEWQDISC;
                break;
                
        default:
                return OFP_QOS_ERROR_UNKNOWN_ACTION;
        }

        /* Find device name */
        found = find_device_name(dp, ntohs(htb->port_no), dev);
        if (!found) {
                return OFP_QOS_ERROR_INVALID_DEV;
        }
                
        /* IMPORTANT: Associate root qdisc (one interface). We will
         * restringe htb to act only as a root scheduler.
         */
        req.tc.tcm_parent = TC_H_ROOT;
        req.tc.tcm_handle = MAJOR_MINOR(MAJOR_ID, MINOR_ID); /* 1:0 */
        
        addattr_l(&req.nl, sizeof(req), TCA_KIND, "htb", strlen("htb") + 1);

        /* htb options */
        opt.version = 3;
        opt.rate2quantum = 10;
        if (htb->class_id == 0)
                opt.defcls = DEFAULT_CLASS_ID;
        else {
                sprintf(hex, "%d", ntohl(htb->class_id));
                opt.defcls = strtoul(hex, NULL, 16);
        }

        
        tail = NLMSG_TAIL(&req.nl);
        addattr_l(&req.nl, 1024, TCA_OPTIONS, NULL, 0);
        addattr_l(&req.nl, 2024, TCA_HTB_INIT, &opt, NLMSG_ALIGN(sizeof(opt)));
        tail->rta_len = (void *) NLMSG_TAIL(&req.nl) - (void *) tail;
                
        /* talk to kernel */
        ll_init_map(&rth);
        ifidx = ll_name_to_index(dev); 
        req.tc.tcm_ifindex = ifidx;
        
        if (rtnl_talk(&rth, &req.nl, 0, 0, NULL, NULL, NULL) < 0) {
                send_errors_to_logfile("parse_qos_htb_qdisc(): Cannot talk to rtnetlink.");
                return OFP_QOS_ERROR_RTNETLINK;
        }
        
        return OFP_QOS_ERROR_NONE;
}

/**
 * This function parse OFP_QOS_CLASS object type.
 *
 * If success return 0, otherwise return positive value.
 */
static int
parse_qos_htb_class (struct datapath *dp, const struct ofp_qos_htb_class *htb)
{
        int cell_log = -1, ccell_log = -1;
        uint32_t mtu;
        uint32_t rtab[256], ctab[256];
        
        /* Structure of the netlink packet */
        struct {
                struct nlmsghdr nl;
                struct tcmsg tc;
                char buf[4096];
        } req;
    
        struct tc_htb_opt opt;
        struct rtattr *tail;
        char dev[OFP_MAX_PORT_NAME_LEN], hex[16];
        uint32_t buffer = 0, cbuffer = 0;
        int found, ifidx, nqueue;
        struct sw_port *swport;
        int *fd;
        struct sw_queue *swqueue;
        int add = 0, del = 0, chg = 0;
                
        bzero(&req, sizeof(req));
        bzero(&opt, sizeof(opt));
        bzero(dev, OFP_MAX_PORT_NAME_LEN);

        req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
        req.nl.nlmsg_flags = NLM_F_REQUEST;

        req.tc.tcm_family = AF_UNSPEC;
        
        /* Check type of action */
        switch (htb->qos_hdr.action_type) {                
        case OFP_QOS_ACTION_ADD:
                req.nl.nlmsg_flags |= NLM_F_EXCL | NLM_F_CREATE;
                req.nl.nlmsg_type = RTM_NEWTCLASS;   /* command */
                add = 1;
                break;
                
        case OFP_QOS_ACTION_DEL:
                req.nl.nlmsg_type = RTM_DELTCLASS;
                del = 1;
                break;
                
        case OFP_QOS_ACTION_CHANGE:
                req.nl.nlmsg_type = RTM_NEWTCLASS;
                chg = 1;
                break;
                
        default:
                return OFP_QOS_ERROR_UNKNOWN_ACTION;
        }

        /* Find device name */
        found = find_device_name(dp, ntohs(htb->port_no), dev);
        if (!found) {
                return OFP_QOS_ERROR_INVALID_DEV;
        }

        /* When class_id == 0, we'll consider as a root class */
        if (htb->class_id == ROOT_CLASS_ID) { // 0
                req.tc.tcm_parent = MAJOR_MINOR(MAJOR_ID, MINOR_ID);      /* 1:0 */
                req.tc.tcm_handle = MAJOR_MINOR(MAJOR_ID, ROOT_CLASS_ID); /* 1:0xffff */
        } else {
                sprintf(hex, "%d", ntohl(htb->class_id));
                req.tc.tcm_parent = MAJOR_MINOR(MAJOR_ID, ROOT_CLASS_ID);             /* 1:0xffff */  
                req.tc.tcm_handle = MAJOR_MINOR(MAJOR_ID, strtoul(hex, NULL, 16));    /* 1:class_id */
        } 

                
        /* It is always like this 1:0 or 1:n */
        addattr_l(&req.nl, sizeof(req), TCA_KIND, "htb", strlen("htb") + 1);

                
        /* Class options */
        
        /* IMPORTANT: Always consider Bytes/sec unit to rate. (comentario errado)
         * htb->rate must be in bits/sec.
         */
        opt.rate.rate = ntohl(htb->rate) / 8.0; 

        /* IMPORTANT: Always consider Bytes/sec unit to rate. (comentario errado)
         * htb->ceil must be in bits/sec.
         */
        opt.ceil.rate = ntohl(htb->ceil) / 8.0; 

        /* Used in round-robin to match a class.
         * Integer number. It is optional, can be 0.
         */
        opt.prio = ntohl(htb->prio);
        
        /* If ceil params are missing, use the same as rate */
        if (!opt.ceil.rate) opt.ceil = opt.rate;

        /* Get mtu size */
        swport = dp_lookup_port(dp, ntohs(htb->port_no));
        mtu = netdev_get_mtu(swport->netdev);

        /* Compute minimal allowed burst from rate; mtu is added here to make
           sute that buffer is larger than mtu and to have some safeguard space */
        buffer = opt.rate.rate / get_hz() + mtu;
        cbuffer = opt.ceil.rate / get_hz() + mtu;
        
        if (calc_rate_table(&opt.rate, rtab, cell_log, mtu) < 0) return -1;
        opt.buffer = calc_xmittime(opt.rate.rate, buffer);
        
        if (calc_rate_table(&opt.ceil, ctab, ccell_log, mtu) < 0) return -1;
        opt.cbuffer = calc_xmittime(opt.ceil.rate, cbuffer);
        
                
        tail = NLMSG_TAIL(&req.nl);
        addattr_l(&req.nl, 1024, TCA_OPTIONS, NULL, 0);
        addattr_l(&req.nl, 2024, TCA_HTB_PARMS, &opt, sizeof(opt));
        addattr_l(&req.nl, 3024, TCA_HTB_RTAB, rtab, 1024);
        addattr_l(&req.nl, 4024, TCA_HTB_CTAB, ctab, 1024);
        tail->rta_len = (void *) NLMSG_TAIL(&req.nl) - (void *) tail; 

                        
        ll_init_map(&rth);
        ifidx = ll_name_to_index(dev);
        req.tc.tcm_ifindex = ifidx;
                
        if (rtnl_talk(&rth, &req.nl, 0, 0, NULL, NULL, NULL) < 0) {
                send_errors_to_logfile("parse_qos_class_qdisc(): Cannot talk to rtnetlink.");
                return OFP_QOS_ERROR_RTNETLINK;
        }

        UNIT_TEST_F1("old number of queues %d\n", netdev_get_num_queues(swport->netdev));

        swport = dp_lookup_port(dp, ntohs(htb->port_no));
        if (add) {
                /* Attach class_id (queue_id) in sw_queue list - high level */
                swqueue = &swport->queues[ntohl(htb->class_id)];
                swqueue->port = swport;
                swqueue->queue_id = ntohl(htb->class_id); /* yes, is the same number */
                swqueue->class_id = ntohl(htb->class_id);
                /* TODO: property ?*/
        
                list_push_back(&swport->queue_list, &swqueue->node);

                /* Attach class_id (queue_fd array) - low level. 
                 * Attach class_id to queue_fd array.
                 * This is a special case. When a class is added, the Linux
                 * kernel automatically attach a pfifo_fast queueing discipline.
                 */
                fd = netdev_get_queue_fd(swport->netdev, ntohl(htb->class_id));
                attach_queue_socket(netdev_get_name(swport->netdev), ntohl(htb->class_id), fd);
                nqueue = netdev_get_num_queues(swport->netdev);
                netdev_update_num_queues(swport->netdev, ++nqueue);
        } else if (chg) {
                /* TODO: change property? */
        } else {
                /* TODO: del property? and queues[i]? queue_fd[j]?*/
        }
        
        UNIT_TEST_F1("number of queues %d\n", netdev_get_num_queues(swport->netdev));

        return OFP_QOS_ERROR_NONE;
}



/**
 * Handle all qos errors types. Report to the controller.
 */
static void
handle_qos_error (struct datapath *dp, const struct sender *sender, uint16_t err)
{
        //struct ofp_qos_error_msg *error = (struct ofp_qos_error_msg *) malloc(sizeof(struct ofp_qos_error_msg));
        
        //error->qos_err_hdr.error_type = htons(err);
        /*dp_send_error_msg(dp, sender, OFPET_QUEUE_OP_FAILED, OFPQOFC_BAD_PORT, error, ntohs(sizeof(error)));*/
        return;
}

/**
 * This function save a log from 'msg'.
 */
static void
send_errors_to_logfile (char *msg)
{
        FILE *logfile;

        logfile = fopen("/var/log/openflow-qos_errors.log", "w");
        if (logfile != NULL) {
                fprintf(logfile, "%s\n", msg);
                fclose(logfile);
        }
}

/**
 * Try to find a port number 'port' into the datapath 'dp'.
 * If find, return the device name in 'devname'.
 *
 * Return 1 if success, otherwise return 0 value.
 */
static int
find_device_name (struct datapath * const dp, uint16_t port, char *devname)
{
        struct sw_port *p = dp_lookup_port(dp, port);
        
        if (p == NULL) return 0;
        strcpy(devname, netdev_get_name(p->netdev));

        return 1;
}

/**
 * This function is used to extract probability value
 * from a "special" integer value. This value is divided
 * in integer:decimal_size (16bit:16bit) format.
 * For example 0.02 => 2:2 and this function converts
 * this special integer to double format.
 * Currently, the function is used to RED qdisc.
 */
static double
extract_probability_value (uint32_t value)
{
        double retval;
        int base = 1, n = (value & 0xFFFF);
        
        while (n-- > 0)
                base *= 10;

        retval = (double) ((value >> 16) / (double) base);
            
        return retval;
}

/**
 *  Exponential Weighted Moving Average (EWMA)
 *  burst + 1 - qmin/avpkt < (1 - (1 - W) ^ burst) / W
 */
static int
red_eval_ewma (uint32_t qmin, uint32_t burst, uint32_t avpkt)
{
	int wlog = 1;
	double W = 0.5;
	double a = (double) burst + 1 - (double) qmin / avpkt;

	if (a < 1.0)
		return -1;
        
	for (wlog = 1; wlog < 32; wlog++, W /= 2) {
		/* if (a <= (1 - pow(1 - W, burst)) / W) */ // BUG: 22112012
			return wlog;
	}
        
	return -1;
}

/**
 * Probability to drop packets
 * Plog = log(prob / (qmax - qmin))
 */
static int
red_eval_P (uint32_t qmin, uint32_t qmax, double prob)
{
	int i = qmax - qmin;

	if (i <= 0) return -1;

	prob /= i;
	for (i = 0; i < 32; i++) {
		if (prob > 1.0)
			break;
		prob *= 2;
	}
        
	if (i >= 32) return -1;
        
	return i;
}

/**
 * Stab[t>>Scell_log] = -log(1-W) * t/xmit_time
 */
static int
red_eval_idle_damping (int Wlog, uint32_t avpkt, uint32_t bps, uint8_t *sbuf)
{
	double xmit_time = calc_xmittime(bps, avpkt);
	double lW = 1;/*-log(1.0 - 1.0 / (1 << Wlog)) / xmit_time;*/ //BUG: 22112012
	double maxtime = 31 / lW;
	int clog;
	int i;

	for (clog = 0; clog < 32; clog++) {
		if (maxtime / (1 << clog) < 512)
			break;
	}
	if (clog >= 32) return -1;

	sbuf[0] = 0;
	for (i = 1; i < 255; i++) {
		sbuf[i] = ( i << clog) * lW;
		if (sbuf[i] > 31)
			sbuf[i] = 31;
	}
	sbuf[255] = 31;
        
	return clog;
}


/********************************************************
 *
 * Timers necessary to calculate transmission rates.
 *
 ********************************************************/

int
core_init (void)
{
        FILE *fp;
        uint32_t clock_res;
        uint32_t t2us;
        uint32_t us2t;

        fp = fopen ("/proc/net/psched", "r");
        if (fp == NULL) return -1;

        if (fscanf(fp, "%08x%08x%08x", &t2us, &us2t, &clock_res) != 3) {
                fclose(fp); 
                return -1;
        }
        fclose(fp);

       	/* Compatibility hack: for old iproute binaries (ignoring
	 * the kernel clock resolution) the kernel advertises a
	 * tick multiplier of 1000 in case of nano-second resolution,
	 * which really is 1.
         */
        if (clock_res == 1000000000) t2us = us2t;

        dptime_clock_factor = (double) clock_res / TIME_UNITS_PER_SEC;
        dptime_tick_in_usec = (double) t2us / us2t * dptime_clock_factor;

        return 0;
}

int
calc_rate_table (struct tc_ratespec *rate, uint32_t *rtable, int cell_log, uint32_t mtu)
{
        uint32_t size, bps, mpu;
        int i;

        bps = rate->rate;
        mpu = rate->mpu;
        
        if (mtu == 0) mtu = 2047;

        if (cell_log < 0) {
                cell_log = 0;
                while ((mtu >> cell_log) > 255)
                        cell_log++;
        }

        for (i = 0; i < 256; i++) {
                size =  adjust_size((i + 1) << cell_log, mpu);
                rtable[i] = calc_xmittime(bps, size);
        }

        rate->cell_align = -1;
        rate->cell_log = cell_log;
        
        return cell_log;
}

uint32_t
adjust_size(uint32_t sz, uint32_t mpu)
{
	if (sz < mpu) sz = mpu;
        return sz;
}

uint32_t
calc_xmittime (uint32_t rate, uint32_t size)
{
	return core_time2tick(TIME_UNITS_PER_SEC * ( (double) size / rate) );
}

uint32_t
core_time2tick (uint32_t time)
{
	return time * dptime_tick_in_usec;
}

uint32_t
calc_xmitsize (uint32_t rate, uint32_t ticks)
{
	return ((double) rate * core_tick2time(ticks)) / TIME_UNITS_PER_SEC;
}

uint32_t
core_tick2time (uint32_t tick)
{
	return tick / dptime_tick_in_usec;
}


/* Timer Functions - TEST */

/**
 * @return ms
 */
static double
get_timer (void)
{
        struct timeval t;
        
        gettimeofday(&t, NULL);
        return (t.tv_sec + (t.tv_usec/1000000.0)) * 1000;
}

static double
timer_diff (double end, double begin)
{
        return end - begin;
}

static void
timer_print_to_file (double value, const char *filename)
{
        FILE *fp = fopen(filename, "a");

        if (fp == NULL) {
                fprintf(stderr, "Error: cannot open timer file %s\n", filename);
                exit(1);
        }

        /* format */
        fprintf(fp, "%lf\n", value);

        fclose(fp);
}
