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
#include <sys/ioctl.h>

#include <lib/libnetlink.h>
#include <lib/ll_map.h>
#include <lib/utils.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_sched.h>
#include <linux/pkt_cls.h>
#include <linux/gen_stats.h>

#include <netpacket/packet.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_packet.h>


#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#include "lib/socket-util.h"

#define THIS_MODULE VLM_datapath
#include "vlog.h"
#include "packet-scheduler.h"
#include "traffic-shaping.h"

/* Maximum physical port bandwidth */
uint32_t bandwidth = 0;

/* See http://physics.nist.gov/cuu/Units/binary.html */
static const struct rate_suffix {
	const char *name;
	double scale;
} suffixes[] = {
	{ "bit",	1. },
	{ "kibit",	1024. },
	{ "kbit",	1000. },
	{ "mibit",	1024.*1024. },
	{ "mbit",	1000000. },
	{ "gibit",	1024.*1024.*1024. },
	{ "gbit",	1000000000. },
	{ "tibit",	1024.*1024.*1024.*1024. },
	{ "tbit",	1000000000000. },
        
	{ "Bps",	8. },
	{ "KiBps",	8.*1024. },
	{ "KBps",	8000. },
	{ "MiBps",	8.*1024*1024. },
	{ "MBps",	8000000. },
	{ "GiBps",	8.*1024.*1024.*1024. },
	{ "GBps",	8000000000. },
	{ "TiBps",	8.*1024.*1024.*1024.*1024. },
	{ "TBps",	8000000000000. },
	{ NULL }
};


static int remove_qdisc_rules (const char *netdev_name);
static int add_root_qdisc (const char *netdev_name);
static int add_shaping_class (const struct netdev *netdev, int rate, int ceil, int class_id);


uint32_t
parse_bw (const char *str)
{
	char *p;
        uint32_t rate = 0;
	double bps = strtod(str, &p);
	const struct rate_suffix *s;
	
	if (*p == '\0') {
		return bps / 8.;	/* assume bytes/sec */
        }

	for (s = suffixes; s->name; ++s) {
		if (strcasecmp(s->name, p) == 0) {
			rate = (bps * s->scale) / 8.;
                }
	}

        if (rate == 0) {
                printf("Bandwidth size must be larger than zero ");
                printf("or suffix is wrong (bandwidth unit)\n");
                exit(1);
        }
        
        return rate;
}


/**
 * \brief Is the same function of 'open_queue_socket()'. This function
 *        will attach one socket to one queue (class_id) present in
 *        Linux kernel.
 */
int
attach_queue_socket (const char *name, uint16_t class_id, int *fd)
{
        int error;
        struct ifreq ifr;
        struct sockaddr_ll sll;
        uint32_t priority;
        char hex[16];

        *fd = socket(PF_PACKET, SOCK_RAW, htons(0)); /* this is a write-only sock */
        if (*fd < 0) {
                return errno;
        }

        /* Set non-blocking mode */
        error = set_nonblocking(*fd);
        if (error) {
                goto error_already_set;
        }

        /* Get ethernet device index. */
        strncpy(ifr.ifr_name, name, sizeof ifr.ifr_name);
        if (ioctl(*fd, SIOCGIFINDEX, &ifr) < 0) {
                VLOG_ERR("ioctl(SIOCGIFINDEX) on %s device failed: %s",
                    name, strerror(errno));
                goto error;
        }

        /* Bind to specific ethernet device. */
        memset(&sll, 0, sizeof sll);
        sll.sll_family = PF_PACKET;
        sll.sll_ifindex = ifr.ifr_ifindex;
        if (bind(*fd, (struct sockaddr *) &sll, sizeof sll) < 0) {
                VLOG_ERR("bind to %s failed: %s", name, strerror(errno));
                goto error;
        }

        
        /* set the priority so that packets from this socket will go to the
         * respective class_id/queue. */
        if (class_id == DEFAULT_CLASS_ID)
                priority = MAJOR_MINOR(MAJOR_ID, DEFAULT_CLASS_ID);
        else {
                sprintf(hex, "%d", class_id);
                priority = MAJOR_MINOR(MAJOR_ID, strtoul(hex, NULL, 16));
        }
        
        if (set_socket_priority(*fd, priority) < 0) {
                VLOG_ERR("set socket priority failed for %s : %s",name,strerror(errno));
                goto error;
        }

        return 0;

error:
        error = errno;
        
error_already_set:
        close(*fd);
        
        return error;
}


/**
 * \brief Configures a port to support traffic shaping.
 *        The shape will be done by remote controller.
 *
 * @param netdev  the device under configuration
 * @return 0  on success
 */
int
setup_traffic_shaping (struct netdev *netdev)
{
        int error, *fd;
           
        if (bandwidth == 0) {
                fprintf(stderr, "Parameter --bw <arg> must be passed or use --no-qos to disable QoSFlow datapath.\n"
                    "Usage: --help\n");
                exit(1);
        }
                        
        error = remove_qdisc_rules(netdev_get_name(netdev));
        if (error) return error;
        
        error = add_root_qdisc(netdev_get_name(netdev));
        if (error) return error;
        
        error = add_shaping_class(netdev, bandwidth, bandwidth, ROOT_CLASS_ID);
        if (error) return error;
        
        //error = add_shaping_class(netdev, (bandwidth * 8)/40, (bandwidth * 8)/40, DEFAULT_CLASS_ID);
        //if (error) return error;


        /* We assume that default queue stay at 'queue_fd' array position 0.
         * So, the traffic that does not has "any class" must be enqueued
         * into default queue.
         *  The default queue is not part of 'num_queues', in other words,
         * the default queue does not increment in queue counting.
         */
        //netdev_update_num_queues(netdev, 0);  /* num_queues = 0 */
        //fd = netdev_get_queue_fd(netdev, 0);  /* &queue_fd[0] */
        //error = attach_queue_socket(netdev_get_name(netdev), DEFAULT_CLASS_ID, fd);
        //if (error) {
        //        return error;
        //}
        //netdev_update_num_queues(netdev, 1); /* = 1, default queue */
            
        return 0;
}


/**
 * \brief Remove any qdisc rules attached to one port.
 *
 * @param netdev_name  device name
 * @return 0  on success
 */
static int
remove_qdisc_rules (const char *netdev_name)
{
        struct rtnl_handle rth;
        struct {
		struct nlmsghdr n;
		struct tcmsg t;
		char buf[TCA_BUF_MAX];
	} req;
        
        int ifidx;

        bzero(&req, sizeof(req));
        
        core_init();
        if (rtnl_open(&rth, 0) < 0) {
                VLOG_ERR("Cannot open rtnetlink.");
                return 1;
        }
        
        req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_DELQDISC;
	req.t.tcm_family = AF_UNSPEC;

        req.t.tcm_parent = TC_H_ROOT;
        
        ll_init_map(&rth);
        ifidx = ll_name_to_index(netdev_name);
        req.t.tcm_ifindex = ifidx;
                
        if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0) {
                VLOG_INFO("Attached scheduler on port %s was cleaned.", netdev_name);
                //return 1;
        }

        rtnl_close(&rth);
        
        return 0;
}


/**
 * \brief Add HTB qdisc on one port.
 *
 * @param netdev_name  device name
 * @return 0  on success
 */
static int
add_root_qdisc (const char *netdev_name)
{
        struct rtnl_handle rth;
        struct {
                struct nlmsghdr nl;
                struct tcmsg tc;
                char buf[TCA_BUF_MAX];
        } req;
    
        struct tc_htb_glob opt;
        struct rtattr *tail;
        int ifidx;
        
        bzero(&req, sizeof(req));
        bzero(&opt, sizeof(opt));
    
        core_init();
        if (rtnl_open(&rth, 0) < 0) {
                VLOG_ERR("Cannot open rtnetlink.");
                return 1;
        }

        req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
        req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_EXCL | NLM_F_CREATE;
        req.nl.nlmsg_type = RTM_NEWQDISC;  
        req.tc.tcm_family = AF_UNSPEC;
                        
        req.tc.tcm_parent = TC_H_ROOT;
        req.tc.tcm_handle = MAJOR_MINOR(MAJOR_ID, MINOR_ID); /* 1:0 */
        
        addattr_l(&req.nl, sizeof(req), TCA_KIND, "htb", strlen("htb") + 1);

        /* htb options */
        opt.version = 3;
        opt.rate2quantum = 10;
        opt.defcls = DEFAULT_CLASS_ID;

        
        tail = NLMSG_TAIL(&req.nl);
        addattr_l(&req.nl, 1024, TCA_OPTIONS, NULL, 0);
        addattr_l(&req.nl, 2024, TCA_HTB_INIT, &opt, NLMSG_ALIGN(sizeof(opt)));
        tail->rta_len = (void *) NLMSG_TAIL(&req.nl) - (void *) tail;
        
        /* talk to kernel */
        ll_init_map(&rth);
        ifidx = ll_name_to_index(netdev_name); 
        req.tc.tcm_ifindex = ifidx;
        
        if (rtnl_talk(&rth, &req.nl, 0, 0, NULL, NULL, NULL) < 0) {
                VLOG_ERR("Cannot talk to kernel through rtnetlink.");
                return 1;
        }

        rtnl_close(&rth);
        
        return 0;
}


/**
 * \brief Add HTB root class on one port.
 *
 * @param rate  maximum rate on a port
 * @return 0  on success
 */
static int
add_shaping_class (const struct netdev *netdev, int rate, int ceil, int class_id)
{
        struct rtnl_handle rth;
        int cell_log = -1, ccell_log = -1;
        uint32_t rtab[256], ctab[256];
        
        struct {
                struct nlmsghdr nl;
                struct tcmsg tc;
                char buf[4096];
        } req;
    
        struct tc_htb_opt opt;
        struct rtattr *tail;
        uint32_t buffer = 0, cbuffer = 0;
        int ifidx;
                
        bzero(&req, sizeof(req));
        bzero(&opt, sizeof(opt));
      
        core_init();
        if (rtnl_open(&rth, 0) < 0) {
                VLOG_ERR("Cannot open rtnetlink.");
                return 1;
        }

        req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct tcmsg));
        req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_EXCL | NLM_F_CREATE;
        req.nl.nlmsg_type = RTM_NEWTCLASS; 
        req.tc.tcm_family = AF_UNSPEC;

        /* setup root class */
        if (class_id == ROOT_CLASS_ID) {
                req.tc.tcm_parent = MAJOR_MINOR(MAJOR_ID, MINOR_ID);   /* 1:0 */
                req.tc.tcm_handle = MAJOR_MINOR(MAJOR_ID, class_id);   /* 1:0xffff */
        } else {
                req.tc.tcm_parent = MAJOR_MINOR(MAJOR_ID, ROOT_CLASS_ID);   /* 1:0xffff */
                req.tc.tcm_handle = MAJOR_MINOR(MAJOR_ID, class_id);        /* 1:0xfffe */
        }
        
        addattr_l(&req.nl, sizeof(req), TCA_KIND, "htb", strlen("htb") + 1);

        /* Class options */
        
        opt.rate.rate = rate; /* [k,m,g,..]bytes/s */
        opt.ceil.rate = ceil;

        /* Compute minimal allowed burst from rate; mtu is added here to make
           sute that buffer is larger than mtu and to have some safeguard space */
        buffer = opt.rate.rate / get_hz() + netdev_get_mtu(netdev);
        cbuffer = opt.ceil.rate / get_hz() + netdev_get_mtu(netdev);
        
        if (calc_rate_table(&opt.rate, rtab, cell_log, netdev_get_mtu(netdev)) < 0) return 1;
        opt.buffer = calc_xmittime(opt.rate.rate, buffer);
        
        if (calc_rate_table(&opt.ceil, ctab, ccell_log, netdev_get_mtu(netdev)) < 0) return 1;
        opt.cbuffer = calc_xmittime(opt.ceil.rate, cbuffer);

        tail = NLMSG_TAIL(&req.nl);
        addattr_l(&req.nl, 1024, TCA_OPTIONS, NULL, 0);
        addattr_l(&req.nl, 2024, TCA_HTB_PARMS, &opt, sizeof(opt));
        addattr_l(&req.nl, 3024, TCA_HTB_RTAB, rtab, 1024);
        addattr_l(&req.nl, 4024, TCA_HTB_CTAB, ctab, 1024);
        tail->rta_len = (void *) NLMSG_TAIL(&req.nl) - (void *) tail; 

                        
        ll_init_map(&rth);
        ifidx = ll_name_to_index(netdev_get_name(netdev));
        req.tc.tcm_ifindex = ifidx;
                
        if (rtnl_talk(&rth, &req.nl, 0, 0, NULL, NULL, NULL) < 0) {
                VLOG_ERR("Cannot talk to kernel through rtnetlink");
                return 1;
        }

        rtnl_close(&rth);
           
        return 0;
}

