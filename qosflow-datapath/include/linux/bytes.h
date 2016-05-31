/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Thomas Lopatic (thomas@lopatic.de)
 * All rights reserved.
 *
 * Adapted by Airton Ishimori from olsrd (http://www.olsrd.org)
 * Federal University of Pará (UFPA) - Brazil - 2012
 * Research Group on Computer Networks and Multimedia Communication (GERCOM)
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#ifndef _BYTES_H_
#define _BYTES_H_

#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#define INLINE inline __attribute__((always_inline))

static INLINE void
bytes_get_u8 (const uint8_t ** p, uint8_t * var)
{
        *var = *(const uint8_t *)(*p);
        *p += sizeof(uint8_t);
}

static INLINE void
bytes_get_u16 (const uint8_t ** p, uint16_t * var)
{
        *var = ntohs(**((const uint16_t **)p));
        *p += sizeof(uint16_t);
}

static INLINE void
bytes_get_u32 (const uint8_t ** p, uint32_t * var)
{
        *var = ntohl(**((const uint32_t **)p));
        *p += sizeof(uint32_t);
}

static INLINE void
bytes_get_s8 (const uint8_t ** p, int8_t * var)
{
        *var = *(const int8_t *)(*p);
        *p += sizeof(int8_t);
}

static INLINE void
bytes_get_s16 (const uint8_t ** p, int16_t * var)
{
        *var = ntohs(**((const int16_t **)p));
        *p += sizeof(int16_t);
}

static INLINE void
bytes_get_s32 (const uint8_t ** p, int32_t * var)
{
        *var = ntohl(**((const int32_t **)p));
        *p += sizeof(int32_t);
}

static INLINE void
bytes_ignore_u8 (const uint8_t ** p)
{
        *p += sizeof(uint8_t);
}

static INLINE void
bytes_ignore_u16 (const uint8_t ** p)
{
        *p += sizeof(uint16_t);
}
static INLINE void
bytes_ignore_u32 (const uint8_t ** p)
{
        *p += sizeof(uint32_t);
}

static INLINE void
bytes_ignore_s8 (const uint8_t ** p)
{
        *p += sizeof(int8_t);
}

static INLINE void
bytes_ignore_s16 (const uint8_t ** p)
{
        *p += sizeof(int16_t);
}

static INLINE void
bytes_ignore_s32 (const uint8_t ** p)
{
        *p += sizeof(int32_t);
}

static INLINE void
bytes_put_u8 (uint8_t ** p, uint8_t var)
{
        **((uint8_t **) p) = var;
        *p += sizeof(uint8_t);
}

static INLINE void
bytes_put_u16 (uint8_t ** p, uint16_t var)
{
        **((uint16_t **) p) = htons(var);
        *p += sizeof(uint16_t);
}

static INLINE void
bytes_put_u32 (uint8_t ** p, uint32_t var)
{
        **((uint32_t **) p) = htonl(var);
        *p += sizeof(uint32_t);
}

static INLINE void
bytes_put_s8 (uint8_t ** p, int8_t var)
{
        **((int8_t **) p) = var;
        *p += sizeof(int8_t);
}

static INLINE void
bytes_put_s16 (uint8_t ** p, int16_t var)
{
        **((int16_t **) p) = htons(var);
        *p += sizeof(int16_t);
}

static INLINE void
bytes_put_s32 (uint8_t ** p, int32_t var)
{
        **((int32_t **) p) = htonl(var);
        *p += sizeof(int32_t);
}

#endif
