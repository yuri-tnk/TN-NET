/*
Copyright © 2009 Yuri Tiomkin
All rights reserved.

Permission to use, copy, modify, and distribute this software in source
and binary forms and its documentation for any purpose and without fee
is hereby granted, provided that the above copyright notice appear
in all copies and that both that copyright notice and this permission
notice appear in supporting documentation.

THIS SOFTWARE IS PROVIDED BY THE YURI TIOMKIN AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL YURI TIOMKIN OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef  _TN_UDP_H_
#define  _TN_UDP_H_

//-- Udp protocol header.
//-- Per RFC 768, September, 1981.

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(push, 1)
#endif

struct udphdr
{
   u_short  uh_sport;             // source port
   u_short  uh_dport;             // destination port
   short    uh_ulen;              // udp length
   u_short  uh_sum;               // udp checksum
}__pkt_struct;

struct udpiphdr
{
   struct ipovly  ui_i;           // overlaid ip structure
   struct udphdr  ui_u;           // udp header
}__pkt_struct;

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(pop)
#endif


#define  ui_next   ui_i.ih_next
#define  ui_prev   ui_i.ih_prev
#define  ui_x1     ui_i.ih_x1
#define  ui_pr     ui_i.ih_pr
#define  ui_len    ui_i.ih_len
#define  ui_src    ui_i.ih_src
#define  ui_dst    ui_i.ih_dst
#define  ui_sport  ui_u.uh_sport
#define  ui_dport  ui_u.uh_dport
#define  ui_ulen   ui_u.uh_ulen
#define  ui_sum    ui_u.uh_sum


#define DHCP_T1_EXPIRED_EVENT    0xFFFFFFFEu
#define DHCP_T2_EXPIRED_EVENT    0xFFFFFFFFu

void udp_input(TN_NET * tnet, TN_NETIF * ni, struct mbuf * mb);
 int udp_output(TN_NET * tnet,
                struct inpcb * inp,
                struct mbuf * mb,
                struct mbuf * mb_addr);
#endif

