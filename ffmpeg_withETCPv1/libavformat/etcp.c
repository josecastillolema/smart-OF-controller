/*
 * Etcp socket protocol
 * Copyright (c) 2013 Luca Barbato
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Mr. Fynn <fynn@fr4c74l.com>
 */

/**
 * @file
 * Format:
 * Header (14b) + id (1b) + packetSize (2b) + offset(1b) + more (1b) + data (packetSize)
 * Etcp socket protocol
 *
 */

#include "libavutil/avstring.h"
#include "libavutil/opt.h"
#include "os_support.h"
#include "network.h"
#include <sys/un.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "url.h"

#define headerSize 14
#define controlSize 1481

typedef struct EtcpContext {
	const AVClass *class;
	struct sockaddr_ll addr;
	uint8_t header[headerSize];
	uint8_t id;
	int timeout;
	int listen;
	int type;
	int fd;
} EtcpContext;

#define OFFSET(x) offsetof(EtcpContext, x)
#define ED AV_OPT_FLAG_DECODING_PARAM|AV_OPT_FLAG_ENCODING_PARAM
static const AVOption etcp_options[] = {
	{ "listen",	"Open socket for listening",			 OFFSET(listen),  AV_OPT_TYPE_INT,   { .i64 = 0 },					0,	   1, ED },
	{ "timeout",   "Timeout in ms",						 OFFSET(timeout), AV_OPT_TYPE_INT,   { .i64 = -1 },				  -1, INT_MAX, ED },
	{ "type",	  "Socket type",						   OFFSET(type),	AV_OPT_TYPE_INT,   { .i64 = SOCK_STREAM },	INT_MIN, INT_MAX, ED, "type" },
	{ "stream",	"Stream (reliable stream-oriented)",	 0,			   AV_OPT_TYPE_CONST, { .i64 = SOCK_STREAM },	INT_MIN, INT_MAX, ED, "type" },
	{ "datagram",  "Datagram (unreliable packet-oriented)", 0,			   AV_OPT_TYPE_CONST, { .i64 = SOCK_DGRAM },	 INT_MIN, INT_MAX, ED, "type" },
	{ "seqpacket", "Seqpacket (reliable packet-oriented",   0,			   AV_OPT_TYPE_CONST, { .i64 = SOCK_SEQPACKET }, INT_MIN, INT_MAX, ED, "type" },
	{ NULL }
};

static const AVClass etcp_class = {
	.class_name = "etcp",
	.item_name  = av_default_item_name,
	.option	 = etcp_options,
	.version	= LIBAVUTIL_VERSION_INT,
};

static int etcp_open(URLContext *h, const char *filename, int flags)
{
	EtcpContext *s = h->priv_data;
	int i, fd, ret;
	char interface[40];
	struct ifreq ifr;
	//Init ID
	s->id = 0x00;
	av_strstart(filename, "etcp:", &filename);
	s->addr.sll_family = AF_PACKET;

	//Create raw socket 
	if ((fd = ff_socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
		return ff_neterrno();
	//Append hash to header
	sscanf(filename, "%02x%02x%02x%02x%02x%02x", (unsigned int *)&s->header[0], (unsigned int *)&s->header[1], (unsigned int *)&s->header[2], 
												 (unsigned int *)&s->header[3], (unsigned int *)&s->header[4], (unsigned int *)&s->header[5]);
	strcpy (interface, filename + 13);

	s->addr.sll_ifindex = if_nametoindex(interface);
	if (!s->addr.sll_ifindex)
	{
		perror("if_nametoindex() failed to obtain interface index");
		goto fail;
	}
	printf("INTERFACE: %s, %d\n", interface, s->addr.sll_ifindex);
	memset (&ifr, 0, sizeof (ifr));
	snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
	//Get src MAC address
	if (ioctl (fd, SIOCGIFHWADDR, &ifr) < 0)
	{
		perror("ioctl() failed to get source MAC address");
		goto fail;
	}
	close(fd);
	if ((fd = ff_socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
		return ff_neterrno();
	//src mac
	memcpy (s->header+6, ifr.ifr_hwaddr.sa_data, 6);
	memcpy (s->addr.sll_addr, s->header + 6, 6);
	s->addr.sll_halen = htons(6);
	//Ethertype
	s->header[12] = 0x08;
	s->header[13] = 0x80;
	printf("HASH: ");
	for (i = 0; i < headerSize; ++i)
		printf("%x", s->header[i]);
	printf("\n");

	s->fd = fd;
	return 0;

fail:
	//if (s->listen && AVUNERROR(ret) != EADDRINUSE)
	//	unlink(s->addr.sun_path);
	if (fd >= 0)
		closesocket(fd);
	return ret;
}

static int etcp_read(URLContext *h, uint8_t *buf, int size)
{
	int ret, nSize;
	uint8_t tmpbuf[1500];
	//printf("SIZE: %d\n", size);
	EtcpContext *s = h->priv_data;
	nSize = 0;
	if (!(h->flags & AVIO_FLAG_NONBLOCK)) {
		ret = ff_network_wait_fd(s->fd, 0);
		if (ret < 0)
		return ret;
	}
	nSize = 0;
	//printf("AQUI DE NOVO\n");
	while(1)
	{
		ret = recv(s->fd, tmpbuf, 1500, 0);
		if (!memcmp(s->header, tmpbuf+6, headerSize-8))
		{
			//printf("SIZE: %x %x %d\n", tmpbuf[15], tmpbuf[16], (tmpbuf[15] << 8) | tmpbuf[16]);

			nSize += (tmpbuf[15] << 8) | tmpbuf[16];
			memcpy(buf + tmpbuf[17]*controlSize, tmpbuf+headerSize+5, (tmpbuf[15] << 8) | tmpbuf[16]);
			if (!tmpbuf[18])
				return nSize; 
		}
	}
	/*recv(s->fd, tmpbuf, 90, 0);
	  if (!strncmp(s->header, tmpbuf, 14))
		memcpy(buf+32692, tmpbuf+14, 76);
	*/
	return ret < 0 ? ff_neterrno() : ret;
}

static int etcp_write(URLContext *h, const uint8_t *buf, int size)
{
	EtcpContext *s = h->priv_data;
	uint8_t frame[1500], offset, more;
	uint16_t sendSize, totalSend;
	int ret;
	if (!(h->flags & AVIO_FLAG_NONBLOCK)) {
		ret = ff_network_wait_fd(s->fd, 1);
		if (ret < 0)
			return ret;
	}
	offset = 0;
	while(1)
	{
		more = 0x01;
		if ((offset + 1)*controlSize > size)
			more = 0x00;
		sendSize = (more) ? controlSize : (size - offset * controlSize);
		memcpy(frame, s->header, headerSize);
		memcpy(frame+headerSize, &s->id, 1);
		frame[15] = (sendSize & 0xff00) >> 8;
		frame[16] = sendSize & 0x00ff;
		frame[17] = offset;
		frame[18] = more;
		//printf("OFFSET: %d\nSIZE: %d\nControl: %d\n", offset, size, size - offset * controlSize);
		memcpy(frame + headerSize + 5, buf + offset*controlSize, sendSize);
		ret = sendto(s->fd, frame, (more) ? 1500 : (size - offset * controlSize + headerSize + 5), 0, (struct sockaddr *)&s->addr, sizeof(s->addr));
		totalSend+=ret;
		//printf("SIZE: %x %x %d - %d\n", frame[15], frame[16], ((frame[15] << 8) | frame[16]), sendSize);
		if (ret == -1)
			perror ("sendto() failed");
		if ((++offset)*controlSize > size)
			break;
	}
	s->id = (s->id++)%256;
	if (ret > 0)
		return totalSend;
	return ret < 0 ? ff_neterrno() : ret;
	//cicle id
}

static int etcp_close(URLContext *h)
{
	EtcpContext *s = h->priv_data;
	//if (s->listen)
	//	unlink(s->addr.sun_path);
	closesocket(s->fd);
	return 0;
}

static int etcp_get_file_handle(URLContext *h)
{
	EtcpContext *s = h->priv_data;
	return s->fd;
}

URLProtocol ff_etcp_protocol = {
	.name				= "etcp",
	.url_open			= etcp_open,
	.url_read			= etcp_read,
	.url_write		   = etcp_write,
	.url_close		   = etcp_close,
	.url_get_file_handle = etcp_get_file_handle,
	.priv_data_size	  = sizeof(EtcpContext),
	.priv_data_class	 = &etcp_class,
	.flags			   = URL_PROTOCOL_FLAG_NETWORK,
};
