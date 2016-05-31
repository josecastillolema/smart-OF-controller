/* Copyright (c) 2008, 2009 The Board of Trustees of The Leland Stanford
 * Junior University
 * 
 * We are making the OpenFlow specification and associated documentation
 * (Software) available for public use and benefit with the expectation
 * that others will use, modify and enhance the Software and contribute
 * those enhancements back to the community. However, since we would
 * like to make the Software available for broadest use, with as few
 * restrictions as possible permission is hereby granted, free of
 * charge, to any person obtaining a copy of this Software to deal in
 * the Software under the copyrights without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * The name and trademarks of copyright holder(s) may NOT be used in
 * advertising or publicity pertaining to the Software or any
 * derivatives without specific, written prior permission.
 */

#ifndef NETDEV_H
#define NETDEV_H 1

#include <linux/pkt_sched.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Generic interface to network devices.
 *
 * Currently, there is a single implementation of this interface that supports
 * Linux.  The interface should be generic enough to be implementable on other
 * operating systems as well. */

struct ofpbuf;
struct in_addr;
struct in6_addr;
struct svec;

enum netdev_feature_type {
    NETDEV_FEAT_CURRENT,
    NETDEV_FEAT_ADVERTISED,
    NETDEV_FEAT_SUPPORTED,
    NETDEV_FEAT_PEER
};

enum netdev_flags {
    NETDEV_UP = 0x0001,         /* Device enabled? */
    NETDEV_PROMISC = 0x0002,    /* Promiscuous mode? */
    NETDEV_CARRIER = 0x0004     /* Carrier detected? */
};

enum netdev_pseudo_ethertype {
    NETDEV_ETH_TYPE_NONE = -128, /* Receive no frames. */
    NETDEV_ETH_TYPE_ANY,         /* Receive all frames. */
    NETDEV_ETH_TYPE_802_2        /* Receive all IEEE 802.2 frames. */
};

#define NETDEV_MAX_QUEUES 8

#define TIME_UNITS_PER_SEC    1000000
#define MAJOR_MINOR(left, right) ((left << 16) | right)
#define MAJOR         0x0001
#define MINOR         0x0000
#define ROOT_CLASS    0xFFFF
#define DEFAULT_CLASS 0xFFFE

/* Main data structure to talk with the kernel */
extern struct rtnl_handle rth;
extern double dptime_tick_in_usec;
extern double dptime_clock_factor;

#define TCA_BUF_MAX           (64*1024)
#define MAX_MSG               16384

struct netdev;

int netdev_open(const char *name, int ethertype, struct netdev **);
int netdev_open_tap(const char *name, struct netdev **);
void netdev_close(struct netdev *);

int netdev_recv(struct netdev *, struct ofpbuf *);
void netdev_recv_wait(struct netdev *);
int netdev_drain(struct netdev *);
int netdev_send(struct netdev *, const struct ofpbuf *, uint16_t class_id);
void netdev_send_wait(struct netdev *);
int netdev_set_etheraddr(struct netdev *, const uint8_t mac[6]);
const uint8_t *netdev_get_etheraddr(const struct netdev *);
const char *netdev_get_name(const struct netdev *);
int netdev_get_mtu(const struct netdev *);
uint32_t netdev_get_features(struct netdev *, int);
bool netdev_get_in4(const struct netdev *, struct in_addr *);
int netdev_set_in4(struct netdev *, struct in_addr addr, struct in_addr mask);
int netdev_add_router(struct in_addr router);
bool netdev_get_in6(const struct netdev *, struct in6_addr *);
int netdev_get_flags(const struct netdev *, enum netdev_flags *);
int netdev_set_flags(struct netdev *, enum netdev_flags, bool permanent);
int netdev_turn_flags_on(struct netdev *, enum netdev_flags, bool permanent);
int netdev_turn_flags_off(struct netdev *, enum netdev_flags, bool permanent);
int netdev_arp_lookup(const struct netdev *, uint32_t ip, uint8_t mac[6]);
int netdev_setup_slicing(struct netdev *, uint16_t);
int netdev_setup_class(const struct netdev *, uint16_t , uint16_t);
int netdev_change_class(const struct netdev *, uint16_t , uint16_t);
int netdev_delete_class(const struct netdev *, uint16_t);

void netdev_enumerate(struct svec *);
int netdev_nodev_get_flags(const char *netdev_name, enum netdev_flags *);

/* Generic interface for monitoring network devices.
 *
 * A network device monitor keeps track of the state of network devices and
 * reports when that state changes.  At a minimum, it must report when a
 * device's link status changes.
 */
struct netdev_monitor;
int netdev_monitor_create(struct netdev_monitor **);
void netdev_monitor_destroy(struct netdev_monitor *);
void netdev_monitor_set_devices(struct netdev_monitor *, char **, size_t);
const char *netdev_monitor_poll(struct netdev_monitor *);
void netdev_monitor_run(struct netdev_monitor *);
void netdev_monitor_wait(struct netdev_monitor *);

/* Timer functions to calculate bitrates - QoS */
int core_init (void);
int calc_rate_table (struct tc_ratespec *rate, uint32_t *rtable, int cell_log, uint32_t mtu);
uint32_t adjust_size (uint32_t sz, uint32_t mpu);
uint32_t calc_xmittime (uint32_t rate, uint32_t size);
uint32_t core_time2tick (uint32_t time);
uint32_t calc_xmitsize (uint32_t rate, uint32_t ticks);
uint32_t core_tick2time (uint32_t tick);

#endif /* netdev.h */
