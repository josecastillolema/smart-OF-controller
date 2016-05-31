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

#ifndef _PACKET_SCHEDULER_H_
#define _PACKET_SCHEDULER_H_

#include "openflow/openflow-qos.h"
#include "oflib-qos/ofl-qos.h"
#include "datapath.h"


ofl_err packet_scheduler (struct datapath *dp, struct ofl_msg_header *msg, const struct sender *sender);
ofl_err packet_scheduler_get_config_request (struct datapath *dp, struct ofl_psched_msg_get_config_request *msg, const struct sender *sender);
ofl_err packet_scheduler_get_config_reply (struct datapath *dp, struct ofl_psched_msg_get_config_reply *msg, const struct sender *sender);

#endif /* _PACKET_SCHEDULER_H_ */
