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
#ifndef OFL_QOS_H
#define	OFL_QOS_H

#include "openflow/openflow-qos.h"

#include "oflib/ofl-structs.h"
#include "oflib/ofl-messages.h"

struct ofl_psched_msg_header {
	enum ofp_psched_sched_type sched_type;
	uint8_t command;
	uint16_t port_no;
	uint32_t queue_id;
};


struct ofl_psched_msg_htb {
	struct ofl_msg_header header;          /* OFPT_PSCHED */
	struct ofl_psched_msg_header qos_header;  /* OFPPSCHED_SCHED_* */
	uint32_t min;   
	uint32_t max;  
};

struct ofl_psched_msg_sfq {
	struct ofl_msg_header header;          /* OFPT_PSCHED */
	struct ofl_psched_msg_header qos_header;  /* OFPPSCHED_SCHED_* */
	uint32_t perturb; 
	uint32_t quantum;
};

struct ofl_psched_msg_red {
	struct ofl_msg_header header;          /* OFPT_PSCHED */
	struct ofl_psched_msg_header qos_header;  /* OFPQ_SCHED_* */
	uint32_t min_len;  
	uint32_t max_len;
	uint32_t limit;    
	uint32_t avpkt;    
	uint32_t rate;     
	uint8_t ecn; 
};

int ofl_psched_msg_pack_packet_scheduler (struct ofl_msg_header *msg, uint8_t **buf, size_t *buf_len);
ofl_err ofl_psched_msg_unpack_packet_scheduler (uint8_t *buf, size_t *len, struct ofl_msg_header **msg);
void ofl_psched_msg_print_packet_scheduler (struct ofl_msg_header *msg, FILE *stream);


/* Packet scheduler properties. */

struct ofl_psched_params {
	enum ofp_psched_sched_type sched_type;
	uint32_t queue_id;
	uint16_t port_no;
};

struct ofl_psched_htb {
	struct ofl_psched_params ps;
	uint32_t min;
	uint32_t max;
};

struct ofl_psched_sfq {
	struct ofl_psched_params ps;
	uint32_t perturb;
	uint32_t quantum;
};

struct ofl_psched_red {
	struct ofl_psched_params ps;
	uint32_t min_len;
	uint32_t max_len;
    uint32_t limit;
    uint32_t avpkt;
    uint32_t rate;
	uint8_t ecn;
};


struct ofl_psched_msg_get_config_request;
struct ofl_psched_msg_get_config_reply;

ofl_err ofl_psched_msg_unpack_packet_scheduler_get_config_request (struct ofp_header *src, size_t *len, struct ofl_msg_header **msg);
ofl_err ofl_psched_msg_unpack_packet_scheduler_get_config_reply (struct ofp_header *src, size_t *len, struct ofl_msg_header **msg);

int ofl_psched_msg_pack_packet_scheduler_get_config_request (struct ofl_psched_msg_get_config_request *msg, uint8_t **buf, size_t *buf_len);
int ofl_psched_msg_pack_packet_scheduler_get_config_reply (struct ofl_psched_msg_get_config_reply *msg, uint8_t **buf, size_t *buf_len); 

struct ofl_psched_msg_get_config_request {
	struct ofl_msg_header header;
	uint32_t queue_id;
	uint16_t port_no;
};

struct ofl_psched_msg_get_config_reply {
	struct ofl_msg_header header;
	struct ofl_psched_params **params;
}; 	



#endif	/* OFL_QOS_H */

