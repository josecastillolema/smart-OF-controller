/*
 * This file was inserted in OpenFlow by Airton Ishimori from Iproute2 linux package.
 * Federal University of Pará (UFPA) - Brazil - 2012
 * Research Group on Computer Networks and Multimedia Communication (GERCOM)
 */

#ifndef __LL_MAP_H__
#define __LL_MAP_H__ 1

extern int ll_remember_index(const struct sockaddr_nl *who,
			     struct nlmsghdr *n, void *arg);
extern int ll_init_map(struct rtnl_handle *rth);
extern unsigned ll_name_to_index(const char *name);
extern const char *ll_index_to_name(unsigned idx);
extern const char *ll_idx_n2a(unsigned idx, char *buf);
extern int ll_index_to_type(unsigned idx);
extern unsigned ll_index_to_flags(unsigned idx);
extern unsigned ll_index_to_addr(unsigned idx, unsigned char *addr,
				 unsigned alen);

#endif /* __LL_MAP_H__ */
