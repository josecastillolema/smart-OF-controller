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

#ifndef _PACKET_SCHEDULER_H_
#define _PACKET_SCHEDULER_H_

#include "openflow/openflow-qos.h"
#include "datapath.h"

/*
 * Some fields of the qos data structures defined, are partitioned
 * logically in bit fields to represent the major and minor 'fields',
 * on this way, 32 bit words partitioned in 16 bit each are used to
 * represent major and minor side.
 *
 * major:minor ==> 16 bit:16 bit ==> 32 bit word
 */

#define MAJOR(word) (word >> 16)                         /* get */ 
#define MINOR(word) (word & 0xFFFF)                      /* get */
#define MAJOR_MINOR(left, right) ((left << 16) | right)  /* set */

#define MAJOR_ID              0x0001
#define MINOR_ID              0x0000
#define ROOT_CLASS_ID         0xFFFF
#define DEFAULT_CLASS_ID      0xFFFE

#define UNIT_TEST_F0(msg)           {printf("TEST: "); printf(msg);}
#define UNIT_TEST_F1(msg, a1)       {printf("TEST: "); printf(msg, a1);}
#define UNIT_TEST_F2(msg, a1, a2)   {printf("TEST: "); printf(msg, a1, a2);}

#define TIME_UNITS_PER_SEC    1000000
#define TCA_BUF_MAX           (64*1024)
#define MAX_MSG               16384


int recv_packet_scheduler (struct datapath *, const struct sender *, const void *oh);

/* Adapted from tc */
int core_init (void);
int calc_rate_table (struct tc_ratespec *rate, uint32_t *rtable, int cell_log, uint32_t mtu);
uint32_t adjust_size (uint32_t sz, uint32_t mpu);
uint32_t calc_xmittime (uint32_t rate, uint32_t size);
uint32_t core_time2tick (uint32_t time);
uint32_t calc_xmitsize (uint32_t rate, uint32_t ticks);
uint32_t core_tick2time (uint32_t tick);

#endif /* DATAPATH_QOS_H_ */
