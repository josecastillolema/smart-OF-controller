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

#ifndef _TRAFFIC_SHAPING_H_
#define _TRAFFIC_SHAPING_H_

#include "lib/netdev.h"

int setup_traffic_shaping (struct netdev *netdev);
int attach_queue_socket (const char *name, uint16_t class_id, int *fd);
uint32_t parse_bw (const char *str);

extern uint32_t bandwidth;

#endif
