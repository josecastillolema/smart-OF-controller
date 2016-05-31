 /*
 * Copyright (c) 2012 Federal University of Pará (UFPA) - Brazil
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

#ifndef OPENFLOW_QOS_H_
#define OPENFLOW_QOS_H_

#include "openflow/openflow.h"


enum ofp_qos_object_type {
        OFP_QOS_QDISC,       /* Queueing discipline */
        OFP_QOS_CLASS        /* Class of queueing discipline */
};

enum ofp_qos_action_type {
        OFP_QOS_ACTION_ADD,      /* Add qdisc, class or filter */
        OFP_QOS_ACTION_DEL,      /* Delete qdisc or class */
        OFP_QOS_ACTION_CHANGE,   /* Change an existing configuration*/
};

enum ofp_qos_error_type {
        OFP_QOS_ERROR_NONE,             /* Success */
        OFP_QOS_ERROR_RTNETLINK,        /* Some error with rtnetlink */
        OFP_QOS_ERROR_INVALID_DEV,      /* There isn't device name */
        OFP_QOS_ERROR_UNKNOWN_ACTION,   /* Just add, del and change */
        OFP_QOS_ERROR_UNKNOWN_OBJECT,   /* Qdisc or class */
        OFP_QOS_ERROR_UNKNOWN_SCHED     /* There isn't a sched name */
};

enum ofp_qos_type {
        OFP_QOS_SCHED_NONE,        /* Clear any rules associated to one or more ports */
        OFP_QOS_SCHED_HTB,         /* HTB scheduler (queueing discipline - classful) */
        OFP_QOS_SCHED_PFIFO,       /* PFIFO scheduler (classless) */
        OFP_QOS_SCHED_BFIFO,       /* BFIFO scheduler (classless) */
        OFP_QOS_SCHED_SFQ,         /* SFQ scheduler (classless) */
        OFP_QOS_SCHED_RED          /* RED scheduler (classless) */
};

/* Header to encapsulate all qos messages (controller --> datapath) */
struct ofp_qos_header {
        uint8_t object_type;      /* QoS object type that message will carry */
        uint8_t action_type;      /* QoS action type of message */
};
OFP_ASSERT(sizeof(struct ofp_qos_header) == 2);


typedef void* ofqos_t;

/**
 * Clear message. It will remove any scheduler attached to one port.
 */
struct ofp_qos_clear {
        struct ofp_qos_header qos_hdr;
        uint16_t port_no;
};
OFP_ASSERT(sizeof(struct ofp_qos_clear) == 4);

/* Classless queueing discipline */

/**
 * Simplest usable qdisc, pure First In, First Out behaviour.
 */
struct ofp_qos_pfifo {
        struct ofp_qos_header qos_hdr;
        uint16_t port_no;       /* Port number */
        uint32_t class_id;      /* Leaf class where a sched will be associated.
                                     Must be 0 if is root qdisc */
        
        uint32_t limit;         /* Maximum queue length. For pfifo, defaults to the interface txqueuelen. */
};
OFP_ASSERT(sizeof(struct ofp_qos_pfifo) == 12);


/**
 * Simplest usable qdisc, pure First In, First Out behaviour.
 */
struct ofp_qos_bfifo {
        struct ofp_qos_header qos_hdr;
        uint16_t port_no;       /* Port number */
        uint32_t class_id;      /* Leaf class where a sched will be associated.
                                     Must be 0 if is root qdisc */

        uint32_t limit;         /* Maximum queue length. For bfifo, it defaults to the (txqueuelen x interface MTU). */
};
OFP_ASSERT(sizeof(struct ofp_qos_bfifo) == 12);

/**
 * Stochastic Fairness Queueing reorders  queued  traffic  so  each
 * 'session' gets to send a packet in turn.
 */
struct ofp_qos_sfq {
        struct ofp_qos_header qos_hdr;
        uint16_t port_no;      /* Port number */
        uint32_t class_id;     /* Leaf class where a sched will be associated.
                                  Must be 0 if is root qdisc */
        
        uint32_t perturb;      /* Interval in seconds for queue algorithm perturbation. */
        uint32_t quantum;      /* Amount of bytes a flow is allowed to dequeue during a round of the round robin process. */   
};
OFP_ASSERT(sizeof(struct ofp_qos_sfq) == 16);

/**
 * Random Early Detection simulates physical congestion by randomly
 * dropping  packets  when nearing configured bandwidth allocation.
 */
struct ofp_qos_red {
        struct ofp_qos_header qos_hdr;
        uint16_t port_no;      /* Port number */
        uint32_t class_id;     /* Leaf class where a sched will be associated.
                                  Must be 0 if is root qdisc */

        uint32_t treshold;      /* Average queue size in bytes at which marking becomes a possibility. */
        uint32_t limit;         /* Hard limit on the real (not average) queue size in bytes. */
};
OFP_ASSERT(sizeof(struct ofp_qos_red) == 16);


/* Classfull queueing discipline */

/**
 * The Hierarchy Token Bucket implements a rich linksharing hierarchy
 * of classes with an emphasis on conforming to existing practices.
 * HTB facilitates guaranteeing bandwidth to classes, while also
 * allowing specification of upper limits to inter-class sharing.
 * It contains shaping elements, based on TBF and can prioritize classes.
 *
 * To see how HTB works, see: http://luxik.cdi.cz/~devik/qos/htb/manual/userg.htm
 */
struct ofp_qos_htb_qdisc {
        struct ofp_qos_header qos_hdr;
        uint16_t port_no;         /* Port number */
        uint32_t class_id;        /* Default class */
};
OFP_ASSERT(sizeof(struct ofp_qos_htb_qdisc) == 8);

struct ofp_qos_htb_class {
        struct ofp_qos_header qos_hdr;
        uint16_t port_no;         /* Port number */
        uint32_t class_id;        /* Child class where a sched will be associated. */
                                     
        
        uint32_t rate;            /* maximum rate this class and all its children are guaranteed. */
        uint32_t ceil;            /* maximum rate at wich a class can send, if its parent has to spare. */
        uint32_t prio;            /* In the round-robin process, classes with the lowest priority field are tried for packets first. */
};
OFP_ASSERT(sizeof(struct ofp_qos_htb_class) == 20); 


/**
 * The * MAIN * data structure to "agregate" queuing discipline
 * into the 'msg' field. The sender must fill this message
 * with one queueing discipline type before to send.
 */
struct ofp_qos_msg {
        struct ofp_header hdr;    /* OpenFlow protocol message header */
        uint8_t sched_type;       /* If is HTB, SFQ, PFIFO, BFIFO, RED */
        uint8_t reserved[3];
        uint8_t body[0];          /* The 'body' can be one of the scheduler message (HTB, SFQ, ...) */
};

#endif /*OPENFLOW_QOS_H_*/
     
