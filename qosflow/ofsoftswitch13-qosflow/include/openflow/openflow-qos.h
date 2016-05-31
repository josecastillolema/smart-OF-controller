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

#ifndef OPENFLOW_QOS_H_
#define OPENFLOW_QOS_H_

#include "openflow/openflow.h"

enum ofp_psched_command_type {
        OFPPSCHED_CMD_ADD,      /* add packet scheduler */
        OFPPSCHED_CMD_DEL,      /* delete packet scheduler */
        OFPPSCHED_CMD_MODIFY,   /* modify packet scheduler */
};

enum ofp_psched_sched_type {
        OFPPSCHED_SCHED_HTB,    /* HTB scheduler (classful) */
        OFPPSCHED_SCHED_SFQ,    /* SFQ scheduler (classless) */
        OFPPSCHED_SCHED_RED     /* RED scheduler (classless) */
};


/* OF QoS header */
struct ofp_psched_header {
	uint8_t sched_type;
	uint8_t command;
	uint16_t port_no;
	uint32_t queue_id;
};
OFP_ASSERT(sizeof(struct ofp_psched_header) == 8);



/* The HTB scheduler is used to create bandwidth slices (queues) */
struct ofp_psched_htb {
	struct ofp_header of_header;
	struct ofp_psched_header qos_header;
	uint32_t min;     /* minimum rate guaranteed */
	uint32_t max;     /* maximum rate guaranteed */
};
OFP_ASSERT(sizeof(struct ofp_psched_htb) == 24);


/* SFQ - stochastic Fairness Queuing */
struct ofp_psched_sfq {
	struct ofp_header of_header;
	struct ofp_psched_header qos_header;
	uint32_t perturb;  /* interval in seconds for queue algorithm perturbation */
        uint32_t quantum;
};
OFP_ASSERT(sizeof(struct ofp_psched_sfq) == 24);

/* RED - Random Early Detection. More details on RED may be find
 in http://www.icir.org/floyd/papers/red/red.html */
struct ofp_psched_red {
	struct ofp_header of_header;
	struct ofp_psched_header qos_header;
	uint32_t min_len;  /* average queue size at which marking becomes a possibility */
	uint32_t max_len;  /* at this average queue size, the marking probability is maximal */
	uint32_t limit;    /* 'queue' length of red in bytes */
	uint32_t avpkt;    /* average packet size in bytes */
	uint32_t rate;     /* bitrate of a queue in bit/s */
	uint8_t ecn;       /* RED can either 'mark' (ecn = 1) or 'drop' (ecn = 0) */
	uint8_t pad[3];
};
OFP_ASSERT(sizeof(struct ofp_psched_red) == 40);


/* Request/Reply messages */

struct ofp_psched_params {
	uint16_t sched_type;  /* OFPPSCHED_SCHED_* */
	uint16_t length;      /* parameters length including size of 'ofp_psched_params' */
	uint32_t queue_id;
	uint16_t port_no;
	uint8_t pad[2];
};

struct ofp_psched_params_htb {
	struct ofp_psched_params sched;
	uint32_t min;
	uint32_t max;
};
OFP_ASSERT(sizeof(struct ofp_psched_params_htb) == 20);

struct ofp_psched_params_sfq {
	struct ofp_psched_params sched;
	uint32_t perturb;  
    uint32_t quantum;
};
OFP_ASSERT(sizeof(struct ofp_psched_params_sfq) == 20);

struct ofp_psched_params_red {
	struct ofp_psched_params sched;
	uint32_t min_len;  
	uint32_t max_len;  
	uint32_t limit;    
	uint32_t avpkt;    
	uint32_t rate;     
	uint8_t ecn;       
	uint8_t pad[3];
};
OFP_ASSERT(sizeof(struct ofp_psched_params_red) == 36);

struct ofp_psched_get_config_request {
	struct ofp_header of_header;
	uint32_t queue_id;  /* valid range [0, OFPQ_MAX) or OFPQ_ANY (all queues) */
	uint16_t port_no;   /* valid range [0, OFPP_MAX) or OFPP_ANY (all ports) */
	uint8_t pad[2];
};
OFP_ASSERT(sizeof(struct ofp_psched_get_config_request) == 16);

struct ofp_psched_get_config_reply {
	struct ofp_header of_header;
	struct ofp_psched_params **params;
};
//OFP_ASSERT(sizeof(struct ofp_psched_get_config_reply) == 12);

#endif
