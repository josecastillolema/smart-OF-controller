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

#include <netinet/in.h>

#include <string.h>
#include <stdlib.h>

#include "openflow/openflow.h"
#include "../oflib/ofl-log.h"
#include "ofl-qos.h"

#define LOG_MODULE ofl_msg_u
OFL_LOG_INIT(LOG_MODULE)

static void ofl_psched_msg_unpack_htb (uint8_t *buf, size_t *len, struct ofl_msg_header **msg);
static void ofl_psched_msg_unpack_sfq (uint8_t *buf, size_t *len, struct ofl_msg_header **msg);
static void ofl_psched_msg_unpack_red (uint8_t *buf, size_t *len, struct ofl_msg_header **msg);

static void ofl_psched_msg_pack_htb (struct ofl_msg_header *msg, uint8_t *buf, size_t *len);
static void ofl_psched_msg_pack_sfq (struct ofl_msg_header *msg, uint8_t *buf, size_t *len);
static void ofl_psched_msg_pack_red (struct ofl_msg_header *msg, uint8_t *buf, size_t *len);

static void ofl_psched_msg_print_htb (struct ofl_msg_header *msg, FILE *stream);
static void ofl_psched_msg_print_sfq (struct ofl_msg_header *msg, FILE *stream);
static void ofl_psched_msg_print_red (struct ofl_msg_header *msg, FILE *stream);


/**
 * \brief Print packet scheduler message.
 * @param msg     host format message
 * @param stream  output place to print
 */
void
ofl_psched_msg_print_packet_scheduler (struct ofl_msg_header *msg, FILE *stream)
{
	struct ofl_psched_msg_header *qos_hdr = (struct ofl_psched_msg_header *)
		(void *) (((uint8_t *) (void *) msg) + sizeof(struct ofl_msg_header));

	
	fprintf(stream, "{cmd=");
	switch (qos_hdr->command) {
	case OFPPSCHED_CMD_ADD:
		fprintf(stream, "\"add\"");
		break;
	case OFPPSCHED_CMD_DEL:
		fprintf(stream, "\"del\"");
		break;
	case OFPPSCHED_CMD_MODIFY:
		fprintf(stream, "\"mod\"");
		break;
	}
	fprintf(stream, ", port=%d", qos_hdr->port_no);
	fprintf(stream, ", queue=%d", qos_hdr->queue_id);
	fprintf(stream, ", type=");
	switch (qos_hdr->sched_type) {
	case OFPPSCHED_SCHED_HTB:
		fprintf(stream, "\"htb\"");
		ofl_psched_msg_print_htb(msg, stream);
		break;
	case OFPPSCHED_SCHED_SFQ:
		fprintf(stream, "\"sfq\"");
		ofl_psched_msg_print_sfq(msg, stream);
		break;
	case OFPPSCHED_SCHED_RED:
		fprintf(stream, "\"red\"");
		ofl_psched_msg_print_red(msg, stream);
		break;
	}
	fprintf(stream, "}");
}

/**
 *  \brief Pack packet scheduler message.
 *  @param buf  message buffer
 *  @param len  variable length 
 *  @param msg  host format message (source)
 */
int 
ofl_psched_msg_pack_packet_scheduler (struct ofl_msg_header *msg, uint8_t **buf, size_t *buf_len)
{
	struct ofl_psched_msg_header *src_qos_hdr = NULL;
	struct ofp_psched_header *dst_qos_hdr = NULL;

	uint32_t of_hdr_len = sizeof(struct ofl_msg_header);
	src_qos_hdr = (void *) (((uint8_t *) (void *) msg) + of_hdr_len);

	  
	*buf_len = sizeof(struct ofp_header) + sizeof(struct ofp_psched_header);
	*buf = xmalloc(*buf_len);
	
	dst_qos_hdr = (void *) (*buf + sizeof(struct ofp_header));
	dst_qos_hdr->sched_type = src_qos_hdr->sched_type;
	dst_qos_hdr->command = src_qos_hdr->command;
	dst_qos_hdr->port_no = htons(src_qos_hdr->port_no);
	dst_qos_hdr->queue_id = htonl(src_qos_hdr->queue_id);
	
	switch (src_qos_hdr->sched_type) {
	case OFPPSCHED_SCHED_HTB:
		ofl_psched_msg_pack_htb(msg, *buf, buf_len);
		break;

	case OFPPSCHED_SCHED_SFQ:
		ofl_psched_msg_pack_sfq(msg, *buf, buf_len);
		break;

	case OFPPSCHED_SCHED_RED:
		ofl_psched_msg_pack_red(msg, *buf, buf_len);
		break;
	}

	
	return 0;
}

/**
 *  \brief Pack packet-scheduler get config request message.
 *  @param msg  host format message
 *  @param buf  network format buffer (dest)
 */
int
ofl_psched_msg_pack_packet_scheduler_get_config_request (struct ofl_psched_msg_get_config_request *msg, uint8_t **buf, size_t *buf_len)
{
	struct ofp_psched_get_config_request *dst;
		
	*buf_len = sizeof(struct ofp_psched_get_config_request);
	*buf = xmalloc(*buf_len);
	memset(*buf, 0, *buf_len);

	dst = (struct ofp_psched_get_config_request *) *buf;
	dst->queue_id = ntohl(msg->queue_id);
    dst->port_no = ntohs(msg->port_no);
	
	return 0;
}


/**
 *  \brief Pack packet-scheduler get config reply message.
 *  @param msg  host format message
 *  @param buf  network format buffer (dest)
 */
int
ofl_psched_msg_pack_packet_scheduler_get_config_reply (struct ofl_psched_msg_get_config_reply *msg, uint8_t **buf, size_t *buf_len)
{
	// TODO:
	
	return 0;
}


/**
 *  \brief Unpack received packet scheduler message.
 *  @param buf  message buffer
 *  @param len  variable length (debug aim)
 *  @param msg  host format message (dest)
 */
ofl_err 
ofl_psched_msg_unpack_packet_scheduler (uint8_t *buf, size_t *len, struct ofl_msg_header **msg)
{
	struct ofp_psched_header *qos_h;
	ofl_err error = 0;

	qos_h = (struct ofp_psched_header *) (buf + sizeof(struct ofp_header));

	switch (qos_h->sched_type) {
	case OFPPSCHED_SCHED_HTB:
		ofl_psched_msg_unpack_htb(buf, len, msg);
		break;
		
	case OFPPSCHED_SCHED_SFQ:
		ofl_psched_msg_unpack_sfq(buf, len, msg);
		break;
		
	case OFPPSCHED_SCHED_RED:
		ofl_psched_msg_unpack_red(buf, len, msg);
		break;
		
	default:
		// TODO:
		break;
	}


	return error;
}


/**
 *  \brief Unpack received packet-scheduler request message.
 *  @param src  network format message 
 *  @param len  variable length
 *  @param msg  host format message (dest)
 */
ofl_err
ofl_psched_msg_unpack_packet_scheduler_get_config_request (struct ofp_header *src, size_t *len, struct ofl_msg_header **msg)
{
	struct ofp_psched_get_config_request *sr;
	struct ofl_psched_msg_get_config_request *dr;


	sr = (struct ofp_psched_get_config_request *) src;

	if (ntohs(sr->port_no) == 0 || ntohs(sr->port_no) > OFPP_ANY) {
		OFL_LOG_WARN(LOG_MODULE, "Received GET_CONFIG_REQUEST message has invalid port (%u).", ntohs(sr->port_no));
		return ofl_error(OFPET_QUEUE_OP_FAILED, OFPQOFC_BAD_PORT);
	}

	*len -= sizeof(struct ofp_psched_get_config_request);
	dr = (struct ofl_psched_msg_get_config_request *) malloc(sizeof(struct ofl_psched_msg_get_config_request));

	dr->port_no = ntohs(sr->port_no);
	dr->queue_id = ntohl(sr->queue_id);

	*msg = (struct ofl_msg_header *) dr;
    
	return 0;
}

/**
 *  \brief Unpack received packet-scheduler reply message.
 *  @param src  network format message
 *  @param len  variable length
 *  @param msg  host format message (dest)
 */
ofl_err
ofl_psched_msg_unpack_packet_scheduler_get_config_reply (struct ofp_header *src, size_t *len, struct ofl_msg_header **msg)
{
	struct ofp_psched_get_config_reply *sr;
	struct ofl_psched_msg_get_config_reply *dr;
	int size, length = 0, idx = 0;
	struct ofp_psched_params_htb *htb = NULL;
	struct ofp_psched_params_sfq *sfq = NULL;
	struct ofp_psched_params_red *red = NULL;

	sr = (struct ofp_psched_get_config_reply *) src;
	size = ntohs(sr->of_header.length) - sizeof(struct ofp_psched_get_config_reply); /* .params */

	if (size == 0)
		OFL_LOG_WARN(LOG_MODULE, "Received GET_CONFIG_REPLY message has any packet-scheduler parameters.");

	dr = xmalloc(sizeof(struct ofl_msg_header) + size);
	dr->header.type = sr->of_header.type;

	while((size -= length) > 0) {
		switch (((struct ofp_psched_params *) sr->params[idx])->sched_type) {
		case OFPPSCHED_SCHED_HTB:
			length = sizeof(struct ofp_psched_params_htb);
			htb = xmalloc(length);
		    htb->min = ntohl( ((struct ofp_psched_params_htb *) sr->params[idx])->min );
			htb->max = ntohl( ((struct ofp_psched_params_htb *) sr->params[idx])->max );
			dr->params[idx] = htb;
			break;
			
		case OFPPSCHED_SCHED_SFQ:
			length = sizeof(struct ofp_psched_params_sfq);
			sfq = xmalloc(length);
			sfq->perturb = ntohl( ((struct ofp_psched_params_sfq *) sr->params[idx])->perturb );
			sfq->quantum = ntohl( ((struct ofp_psched_params_sfq *) sr->params[idx])->quantum );
			dr->params[idx] = sfq;
			break;
			
		case OFPPSCHED_SCHED_RED:
			length = sizeof(struct ofp_psched_params_red);
			red = xmalloc(length);
			red->min_len = ntohl( ((struct ofp_psched_params_red *) sr->params[idx])->min_len );
			red->max_len = ntohl( ((struct ofp_psched_params_red *) sr->params[idx])->max_len );
			red->limit = ntohl( ((struct ofp_psched_params_red *) sr->params[idx])->limit );
			red->avpkt = ntohl( ((struct ofp_psched_params_red *) sr->params[idx])->avpkt );
			red->rate = ntohl( ((struct ofp_psched_params_red *) sr->params[idx])->rate );
			red->ecn = ((struct ofp_psched_params_red *) sr->params[idx])->ecn;
			break;
		}
		idx++;
		htb = sfq = red = NULL;
	}
		
	return 0;
}

/**
 *  \brief Unpack htb data.
 *  @param buf  message buffer
 *  @param len  variable length (debug aim)
 *  @param msg  host format message (dest)
 */
static void
ofl_psched_msg_unpack_htb (uint8_t *buf, size_t *len, struct ofl_msg_header **msg)
{
	struct ofp_psched_htb *src;
	struct ofl_psched_msg_htb *dst = (struct ofl_psched_msg_htb *) xmalloc(sizeof(struct ofl_psched_msg_htb));

	src = (struct ofp_psched_htb *) ((void *) buf);

	dst->qos_header.sched_type = OFPPSCHED_SCHED_HTB;
	dst->qos_header.command = src->qos_header.command;
	dst->qos_header.port_no = ntohs(src->qos_header.port_no);
	dst->qos_header.queue_id = ntohl(src->qos_header.queue_id);

	if (src->qos_header.command != OFPPSCHED_CMD_DEL) {
		dst->min = ntohl(src->min);
		dst->max = ntohl(src->max);
	}

	*len -= sizeof(struct ofp_psched_htb);
	
	*msg = (struct ofl_msg_header *) dst;
}

/**
 *  \brief Unpack sfq data.
 *  @param buf  message buffer
 *  @param len  variable length (debug aim)
 *  @param msg  host format message (dest)
 */
static void
ofl_psched_msg_unpack_sfq (uint8_t *buf, size_t *len, struct ofl_msg_header **msg)
{
	struct ofp_psched_sfq *src;
	struct ofl_psched_msg_sfq *dst = (struct ofl_psched_msg_sfq *) xmalloc(sizeof(struct ofl_psched_msg_sfq));

	src = (struct ofp_psched_sfq *) buf;

	dst->qos_header.sched_type = OFPPSCHED_SCHED_SFQ;
	dst->qos_header.command = src->qos_header.command;
	dst->qos_header.port_no = ntohs(src->qos_header.port_no);
	dst->qos_header.queue_id = ntohl(src->qos_header.queue_id);

	if (src->qos_header.command != OFPPSCHED_CMD_DEL) {
		dst->perturb = ntohl(src->perturb);
		dst->quantum = ntohl(src->quantum);
	}

	*len -= sizeof(struct ofp_psched_sfq);

	*msg = (struct ofl_msg_header *) dst;
}

/**
 *  \brief Unpack red data.
 *  @param buf  message buffer
 *  @param len  variable length (debug aim)
 *  @param msg  host format message (dest)
 */
static void
ofl_psched_msg_unpack_red (uint8_t *buf, size_t *len, struct ofl_msg_header **msg)
{
	struct ofp_psched_red *src;
	struct ofl_psched_msg_red *dst = (struct ofl_psched_msg_red *) xmalloc(sizeof(struct ofl_psched_msg_red));

	src = (struct ofp_psched_red *) buf;

	dst->qos_header.sched_type = OFPPSCHED_SCHED_RED;
	dst->qos_header.command = src->qos_header.command;
	dst->qos_header.port_no = ntohs(src->qos_header.port_no);
	dst->qos_header.queue_id = ntohl(src->qos_header.queue_id);

	if (src->qos_header.command != OFPPSCHED_CMD_DEL) {
		dst->min_len = ntohl(src->min_len);
		dst->max_len = ntohl(src->max_len);
		dst->limit = ntohl(src->limit);
		dst->avpkt = ntohl(src->avpkt);
		dst->rate = ntohl(src->rate);
		dst->ecn = ntohl(src->ecn);
	}

	*len -= sizeof(struct ofp_psched_red);

	*msg = (struct ofl_msg_header *) dst;
}

/**
 * \brief Pack htb parameters.
 * @param msg  local format message
 * @param buf  serialized buffer (net format)
 * @param len  buffer lenght
 */
static void
ofl_psched_msg_pack_htb (struct ofl_msg_header *msg, uint8_t *buf, size_t *len)
{
	struct ofl_psched_msg_htb *src_htb = (struct ofl_psched_msg_htb *) msg;
	size_t params_len = sizeof(struct ofp_psched_htb) - *len;
	struct {        
		uint32_t min;
		uint32_t max;
	} *params;
	buf = xrealloc(buf, sizeof(struct ofp_psched_htb));
	params = (void *) (buf + (sizeof(struct ofp_psched_htb) - params_len));
        
	params->min = htonl(src_htb->min);
	params->max = htonl(src_htb->max);
	
	*len += params_len;
	
	//free(params);
        params = NULL;
        free(params);
}

/**
 * \brief Pack sfq parameters.
 * @param msg  local format message
 * @param buf  serialized buffer (net format)
 * @param len  buffer lenght
 */
static void
ofl_psched_msg_pack_sfq (struct ofl_msg_header *msg, uint8_t *buf, size_t *len)
{
	struct ofl_psched_msg_sfq *src_sfq = (struct ofl_psched_msg_sfq *) msg;
	size_t params_len = sizeof(struct ofp_psched_sfq) - *len;
	struct {
		uint32_t perturb;
		uint32_t quantum;
	} *params;
	params = (void *) (buf + (sizeof(struct ofp_psched_sfq) - params_len));
	buf = xrealloc(buf, params_len);
	
	params->perturb = htonl(src_sfq->perturb);
	params->quantum = htonl(src_sfq->quantum);

	*len += params_len;
		
	params = NULL;
        free(params);
}

/**
 * \brief Pack red parameters.
 * @param msg  local format message
 * @param buf  serialized buffer (net format)
 * @param len  buffer lenght
 */
static void
ofl_psched_msg_pack_red (struct ofl_msg_header *msg, uint8_t *buf, size_t *len)
{
	struct ofl_psched_msg_red *src_red = (struct ofl_psched_msg_red *) msg;
	size_t params_len = sizeof(struct ofp_psched_red) - *len;
	struct {
		uint32_t min_len;
		uint32_t max_len;
		uint32_t limit;  
		uint32_t avpkt;  
		uint32_t rate;   
		uint8_t ecn;     
		uint8_t pad[3];	
	} *params;
	params = (void *) (buf + (sizeof(struct ofp_psched_sfq) - params_len));
	buf = xrealloc(buf, params_len);
	
	params->min_len = htonl(src_red->min_len);
	params->max_len = htonl(src_red->max_len);
	params->limit = htonl(src_red->limit);
	params->avpkt = htonl(src_red->avpkt);
	params->rate = htonl(src_red->rate);
	params->ecn = src_red->ecn;

	*len += params_len;

	params = NULL;
        free(params);
}

static void
ofl_psched_msg_print_htb (struct ofl_msg_header *msg, FILE *stream)
{
	struct ofl_psched_msg_htb *htb = (struct ofl_psched_msg_htb *) msg;
	
	fprintf(stream, ", params=[{");
	fprintf(stream, "min=%d", htb->min);
	fprintf(stream, ", max=%d", htb->max);
	fprintf(stream, "}]");
}

static void
ofl_psched_msg_print_sfq (struct ofl_msg_header *msg, FILE *stream)
{
	struct ofl_psched_msg_sfq *sfq = (struct ofl_psched_msg_sfq *) msg;
	
	fprintf(stream, ", params=[{");
	fprintf(stream, "perturb=%d", sfq->perturb);
	fprintf(stream, ", quantum=%d", sfq->quantum);
	fprintf(stream, "}]");
}

static void
ofl_psched_msg_print_red (struct ofl_msg_header *msg, FILE *stream)
{
	struct ofl_psched_msg_red *red = (struct ofl_psched_msg_red *) msg;
	
	fprintf(stream, ", params=[{");
	fprintf(stream, "min_len=%d", red->min_len);
	fprintf(stream, ", max_len=%d", red->max_len);
	fprintf(stream, ", limit=%d", red->limit);
	fprintf(stream, ", avpkt=%d", red->avpkt);
	fprintf(stream, ", rate=%d", red->rate);
	fprintf(stream, ", ecn=%d", red->ecn);
	fprintf(stream, "}]");
}

