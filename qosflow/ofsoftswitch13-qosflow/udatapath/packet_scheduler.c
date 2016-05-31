/*
 * Copyright (c) 2013 Federal University of Pará (UFPA) - Brazil
 * Research Group on Computer Networks and Multimedia Communication (GERCOM)
 * Home page: http://gercom.ufpa.br
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

#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "packet_scheduler.h"
#include "vlog.h"

#define LOG_MODULE VLM_packet_scheduler

/* Adapted from TC */
static int red_eval_ewma (uint32_t qmin, uint32_t burst, uint32_t avpkt);
static int red_eval_P (uint32_t qmin, uint32_t qmax, double prob);
static int red_eval_idle_damping (int Wlog, uint32_t avpkt, uint32_t bps, uint8_t *sbuf);


static ofl_err pkt_sched_htb (struct datapath *dp, struct ofl_psched_msg_htb *msg, const struct sender *sender);
static ofl_err pkt_sched_sfq (struct datapath *dp, struct ofl_psched_msg_sfq *msg, const struct sender *sender);
static ofl_err pkt_sched_red (struct datapath *dp, struct ofl_psched_msg_red *msg, const struct sender *sender);


/**
 * \brief Process an unpacked packet-scheduler message and configure it into the kernel.
 */
ofl_err
packet_scheduler (struct datapath *dp, struct ofl_msg_header *msg, const struct sender *sender)
{
	struct ofl_psched_msg_header *qos_h;
	ofl_err error = 0;

	core_init();
    if (rtnl_open(&rth, 0) < 0) {
		; // TODO:
    }

	qos_h = (void *) ( (uint8_t *)(void *) msg  ) + sizeof(struct ofl_msg_header);

	switch (qos_h->sched_type) {
	case OFPPSCHED_SCHED_HTB:
		pkt_sched_htb(dp, (struct ofl_psched_msg_htb *) msg, sender);
		break;

	case OFPPSCHED_SCHED_SFQ:
		pkt_sched_sfq(dp, (struct ofl_psched_msg_sfq *) msg, sender);
		break;

	case OFPPSCHED_SCHED_RED:
		pkt_sched_red(dp, (struct ofl_psched_msg_red *) msg, sender);
		break;
		
	default:
		// TODO:
		break;
	}

	rtnl_close(&rth);
	
	
	/* If the execution reaches here, so everything is okay. It means that
	 * the OF datapath must know about the queue that was created into the kernel.
	 */
	error = dp_ports_handle_pkt_sched(dp, msg, NULL);


    /* Resetting global variables */
    dptime_clock_factor = 1;
    dptime_tick_in_usec = 1;
    bzero(&rth, sizeof(rth));
	
	return error;
}


ofl_err
packet_scheduler_get_config_request (struct datapath *dp, struct ofl_psched_msg_get_config_request *msg, const struct sender *sender)
{
	struct sw_port *p;
	struct sw_queue *q;

	struct ofl_psched_msg_get_config_reply reply = {
		{.type = OFPT_PSCHED_GET_CONFIG_REPLY},
		.params = NULL
	};

	if (msg->port_no == OFPP_ANY || msg->port_no == OFPP_ALL) {
		size_t i, idx = 0, num = 0;

		if (msg->queue_id == OFPQ_ALL) {
			LIST_FOR_EACH(p, struct sw_port, node, &dp->port_list) {
				num += p->num_queues;
			}

			reply.params = xmalloc(sizeof(struct ofl_psched_params *) * num);
			
			LIST_FOR_EACH(p, struct sw_port, node, &dp->port_list) {
				for (i = 0; i < p->max_queues; i++) {
					if (p->queues[i].port != NULL) {
						reply.params[idx] = p->queues[i].sched;
						idx++;
					}
				}
			}
		} else {
			/* specific queue of each available port */
			LIST_FOR_EACH(p, struct sw_port, node, &dp->port_list) {
				reply.params = xrealloc(reply.params, sizeof(struct ofl_psched_params *) * idx);
				q = dp_ports_lookup_queue(p, msg->queue_id);
				reply.params[idx] = q->sched;
				idx++;
			}
		}
	} else {
		/* specific port */
		p = dp_ports_lookup(dp, msg->port_no);
		if (p == NULL || (p->stats->port_no != msg->port_no)) {
			free(reply.params);
			ofl_msg_free((struct ofl_msg_header *)msg, NULL);
			return ofl_error(OFPET_QUEUE_OP_FAILED, OFPQOFC_BAD_PORT);
		} else if (msg->queue_id == OFPQ_ALL) {
			int i, idx = 0;
			reply.params = xmalloc(sizeof(struct ofl_psched_params *) * p->num_queues);
			for (i = 0; i < p->max_queues; i++) {
				if (p->queues[i].port != NULL) {
					reply.params[idx] = p->queues[i].sched;
					idx++;
				}
			}
		} else {
			/* specific queue */
			reply.params = xmalloc(sizeof(struct ofl_psched_params *));
			q = dp_ports_lookup_queue(p, msg->queue_id);
            if (q == NULL) {
                reply.params = NULL;
            } else {
			    //reply.params[0] = q->sched;
                memcpy(reply.params[0], q->sched, sizeof(q->sched));
            }
		}
	}

	dp_send_message(dp, (struct ofl_msg_header *) &reply, sender);

	free(reply.params);
	ofl_msg_free((struct ofl_msg_header *) msg, NULL);

	
	return 0;
}

ofl_err
packet_scheduler_get_config_reply (struct datapath *dp, struct ofl_psched_msg_get_config_reply *msg, const struct sender *sender)
{
    struct ofp_psched_get_config_reply reply;
	// TODO:

	return 0;
}


static ofl_err
pkt_sched_htb (struct datapath *dp, struct ofl_psched_msg_htb *msg, const struct sender *sender)
{	 
    /* Structure of the netlink packet */
    struct {
        struct nlmsghdr nl;
        struct tcmsg tc;
        char buf[4096];
    } req;
	struct tc_htb_opt opt;
    struct rtattr *tail;

	uint32_t mtu, rtab[256], ctab[256];
	int cell_log = -1, ccell_log = -1;
   	ofl_err error = 0;
	
	char hex[16];
    uint32_t buffer = 0, cbuffer = 0;
    int ifidx;
    struct sw_port *swport;

	/* Search OF port */
	swport = dp_ports_lookup(dp, msg->qos_header.port_no);
	if (swport == NULL) {
		VLOG_ERR(LOG_MODULE, "Failed to operate queue %d", msg->qos_header.queue_id);
		return ofl_error(OFPET_QUEUE_OP_FAILED, OFPQOFC_BAD_QUEUE);
	}
	
	bzero(&req, sizeof(req));
    bzero(&opt, sizeof(opt));

    req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
    req.nl.nlmsg_flags = NLM_F_REQUEST;

    req.tc.tcm_family = AF_UNSPEC;

	switch (msg->qos_header.command) {
	case OFPPSCHED_CMD_ADD:
		req.nl.nlmsg_flags |= NLM_F_EXCL | NLM_F_CREATE;
        req.nl.nlmsg_type = RTM_NEWTCLASS;
		break;

	case OFPPSCHED_CMD_DEL:
		req.nl.nlmsg_type = RTM_DELTCLASS;
		break;

	case OFPPSCHED_CMD_MODIFY:
		req.nl.nlmsg_type = RTM_NEWTCLASS;
		break;
		
	default:
		VLOG_ERR(LOG_MODULE, "Invalid command %d", msg->qos_header.command);
		return 0; // TODO: 
	}


	sprintf(hex, "%d", msg->qos_header.queue_id);
	req.tc.tcm_parent = MAJOR_MINOR(MAJOR, ROOT_CLASS);                /* 1:0xFFFF */  
	req.tc.tcm_handle = MAJOR_MINOR(MAJOR, strtoul(hex, NULL, 16));    /* 1:queue_id */

	addattr_l(&req.nl, sizeof(req), TCA_KIND, "htb", strlen("htb") + 1);

	/* The min and max value *must* be always in bits/s format  */
	opt.rate.rate = msg->min / 8.0;
	opt.ceil.rate = msg->max / 8.0;
	opt.prio = 0;

	/* If ceil params are missing, use the same as rate */
    if (!opt.ceil.rate) opt.ceil = opt.rate;

	/* Get mtu size */
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
    ifidx = ll_name_to_index(swport->conf->name);
    req.tc.tcm_ifindex = ifidx;

	if (rtnl_talk(&rth, &req.nl, 0, 0, NULL, NULL, NULL) < 0) {
        return 0; // TODO
	}					  
	
	return error;
}

static ofl_err
pkt_sched_sfq (struct datapath *dp, struct ofl_psched_msg_sfq *msg, const struct sender *sender)
{
	struct {
		struct nlmsghdr nl;
		struct tcmsg tc;
		char buf[TCA_BUF_MAX];
	} req;
	struct tc_sfq_qopt opt;
        char hex[16];
        int ifidx;
	ofl_err error = 0;
	struct sw_port *swport = NULL;
	

	/* Search OF port */
	swport = dp_ports_lookup(dp, msg->qos_header.port_no);
	if (swport == NULL) {
		VLOG_ERR(LOG_MODULE, "Failed to operate queue %d", msg->qos_header.queue_id);
		return ofl_error(OFPET_QUEUE_OP_FAILED, OFPQOFC_BAD_QUEUE);
	}
	
    bzero(&req, sizeof(req));
    bzero(&opt, sizeof(opt));

    req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.nl.nlmsg_flags = NLM_F_REQUEST;

	req.tc.tcm_family = AF_UNSPEC;

    switch(msg->qos_header.command) {
        case OFPPSCHED_CMD_ADD:
            req.nl.nlmsg_flags |= NLM_F_EXCL | NLM_F_CREATE;
            req.nl.nlmsg_type = RTM_NEWQDISC;  
            break;
                
        case OFPPSCHED_CMD_DEL:
            req.nl.nlmsg_type = RTM_DELQDISC;  
            break;

        case OFPPSCHED_CMD_MODIFY:
            req.nl.nlmsg_type = RTM_NEWQDISC;
            break;

        default:
		    VLOG_ERR(LOG_MODULE, "Invalid command %d", msg->qos_header.command);
		    return 0; // TODO: 
    }

	sprintf(hex, "%d", msg->qos_header.queue_id);
	req.tc.tcm_parent = MAJOR_MINOR(MAJOR, ROOT_CLASS);                /* 1:0xFFFF */  
	req.tc.tcm_handle = MAJOR_MINOR(MAJOR, strtoul(hex, NULL, 16));    /* 1:queue_id */

	addattr_l(&req.nl, sizeof(req), TCA_KIND, "sfq", strlen("sfq") + 1);

	/* sfq opts */
    opt.perturb_period = (msg->perturb == 0) ? 10 : msg->perturb;
    opt.quantum = (msg->quantum == 0) ? netdev_get_mtu(swport->netdev) : msg->quantum;
        
    addattr_l(&req.nl, 1024, TCA_OPTIONS, &opt, sizeof(opt));
        
    ll_init_map(&rth);
    ifidx = ll_name_to_index(swport->conf->name);
    req.tc.tcm_ifindex = ifidx;

	if (rtnl_talk(&rth, &req.nl, 0, 0, NULL, NULL, NULL) < 0) {
        return 0; // TODO:
	}	
	
	return error;
}

static ofl_err
pkt_sched_red (struct datapath *dp, struct ofl_psched_msg_red *msg, const struct sender *sender)
{
	struct {
		struct nlmsghdr nl;
		struct tcmsg tc;
		char buf[TCA_BUF_MAX];
	} req;
	struct tc_red_qopt opt;
        struct rtattr *tail;
		
	char hex[16];
        int ifidx, rate, avpkt, burst;
        uint8_t sbuf[256];
	ofl_err error = 0;
	struct sw_port *swport = NULL;
	

	/* Search OF port */
	swport = dp_ports_lookup(dp, msg->qos_header.port_no);
	if (swport == NULL) {
		VLOG_ERR(LOG_MODULE, "Failed to operate queue %d", msg->qos_header.queue_id);
		return ofl_error(OFPET_QUEUE_OP_FAILED, OFPQOFC_BAD_QUEUE);
	}
	
	bzero(&req, sizeof(req));
    bzero(&opt, sizeof(opt));

    req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.nl.nlmsg_flags = NLM_F_REQUEST;

	req.tc.tcm_family = AF_UNSPEC;

	switch(msg->qos_header.command) {
        case OFPPSCHED_CMD_ADD:
            req.nl.nlmsg_flags |= NLM_F_EXCL | NLM_F_CREATE;
            req.nl.nlmsg_type = RTM_NEWQDISC;
            break;
                
        case OFPPSCHED_CMD_DEL:
            req.nl.nlmsg_type = RTM_DELQDISC;  
            break;

        case OFPPSCHED_CMD_MODIFY:
            req.nl.nlmsg_type = RTM_NEWQDISC;
            break;

        default:
		    VLOG_ERR(LOG_MODULE, "Invalid command %d", msg->qos_header.command);
		    return 0; // TODO: 
    }

	sprintf(hex, "%d", msg->qos_header.queue_id);
	req.tc.tcm_parent = MAJOR_MINOR(MAJOR, ROOT_CLASS);                /* 1:0xFFFF */  
	req.tc.tcm_handle = MAJOR_MINOR(MAJOR, strtoul(hex, NULL, 16));    /* 1:queue_id */

	addattr_l(&req.nl, sizeof(req), TCA_KIND, "red", strlen("red") + 1);

	
	/* red opts */

	if (!msg->rate) {
		VLOG_ERR(LOG_MODULE, "Invalid bitrate %d", msg->rate);
		return 0; // TODO:
	} else {
		rate = msg->rate;
	}
	
	/* Recommendations from Linux Man for default values:
	 * http://linux.die.net/man/8/tc-red*/

	if (!msg->avpkt) {
		avpkt = 1000; /* bytes */
	} else {
		avpkt = msg->avpkt;
	}
	if (!msg->limit) {
		msg->limit = 1000; /* bytes */
	} else {
		opt.limit = msg->limit;
	} 

	/* Recomendations from Sally Floyd for min/max thresholds:
	 * http://www.icir.org/floyd/REDparameters.txt*/

	if (!msg->max_len) {
		opt.qth_max = opt.qth_min ? opt.qth_min * 3 : opt.limit / 4;
	} else {
		opt.qth_max = msg->max_len;  /* in bytes */
	}
	if (!msg->min_len) {
		opt.qth_min = opt.qth_max / 3;
	} else {
		opt.qth_min = msg->min_len;  /* in bytes */
	}
	
	burst = (2 * opt.qth_min + opt.qth_max) / (3 * avpkt);

	opt.Wlog = red_eval_ewma(opt.qth_min, burst, avpkt);
	opt.Plog = red_eval_P(opt.qth_min, opt.qth_max, 0.02);
	opt.Scell_log = red_eval_idle_damping(opt.Wlog, avpkt, rate, sbuf);
	if (msg->ecn)
		opt.flags |= TC_RED_ECN;

	addattr_l(&req.nl, 1024, TCA_OPTIONS, &opt, sizeof(opt));

        tail = NLMSG_TAIL(&req.nl);
	addattr_l(&req.nl, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(&req.nl, 1024, TCA_RED_PARMS, &opt, sizeof(opt));
	addattr_l(&req.nl, 1024, TCA_RED_STAB, sbuf, 256);
	tail->rta_len = (void *) NLMSG_TAIL(&req.nl) - (void *) tail;
        
        ll_init_map(&rth);
        ifidx = ll_name_to_index(swport->conf->name);
        req.tc.tcm_ifindex = ifidx;

	if (rtnl_talk(&rth, &req.nl, 0, 0, NULL, NULL, NULL) < 0) {
                return 0; // TODO:
	}	

	return error;
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
		if (a <= (1 - pow(1 - W, burst)) / W) 
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
	double lW = -log(1.0 - 1.0 / (1 << Wlog)) / xmit_time; 
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
