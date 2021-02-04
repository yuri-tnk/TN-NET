#ifndef _TN_IP_H_
#define _TN_IP_H_

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
 * Copyright (c) 1982, 1985, 1986, 1988, 1993, 1994
 *        The Regents of the University of California.  All rights reserved.
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
 *        This product includes software developed by the University of
 *        California, Berkeley and its contributors.
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


/*
 * Constants and structures defined by the internet system,
 * Per RFC 790, September 1981, and numerous additions.
 */


#define IP_MAXPACKET    65535

/*
 * Local port number conventions:
 * Ports < IPPORT_RESERVED are reserved for
 * privileged processes (e.g. root).
 * Ports > IPPORT_USERRESERVED are reserved
 * for servers, not necessarily privileged.
 */

#define        IPPORT_RESERVED            1024
#define        IPPORT_USERRESERVED        5000

//---------------------------------------------------------------------------

#define  IN_CLASSD(i)        (((long)(i) & 0xf0000000) == 0xe0000000)
#define  IN_CLASSD_NET       0xf0000000   //-- These ones aren't really
#define  IN_CLASSD_NSHIFT    28                      //-- net and host fields, but
#define  IN_CLASSD_HOST      0x0fffffff   //-- routing needn't know.
#define  IN_MULTICAST(i)     IN_CLASSD(i)

#define  _INADDR_ANY          (unsigned long)0x00000000
#define  _INADDR_BROADCAST    (unsigned long)0xffffffff   // must be masked

//-- Protocols - OpenBSD

#define  IPPROTO_IP             0       /* dummy for IP */
#define  IPPROTO_HOPOPTS        0       /* IP6 hop-by-hop options */
#define  IPPROTO_ICMP           1       /* control message protocol */
#define  IPPROTO_IGMP           2       /* group mgmt protocol */
#define  IPPROTO_GGP            3       /* gateway^2 (deprecated) */
#define  IPPROTO_IPV4           4       /* IP header */
#define  IPPROTO_IPIP           4       /* IP inside IP */
#define  IPPROTO_TCP            6       /* tcp */
#define  IPPROTO_EGP            8       /* exterior gateway protocol */
#define  IPPROTO_PUP           12       /* pup */
#define  IPPROTO_UDP           17       /* user datagram protocol */
#define  IPPROTO_IDP           22       /* xns idp */
#define  IPPROTO_TP            29       /* tp-4 w/ class negotiation */
#define  IPPROTO_IPV6          41       /* IP6 header */
#define  IPPROTO_ROUTING       43       /* IP6 routing header */
#define  IPPROTO_FRAGMENT      44       /* IP6 fragmentation header */
#define  IPPROTO_RSVP          46       /* resource reservation */
#define  IPPROTO_GRE           47       /* GRE encaps RFC 1701 */
#define  IPPROTO_ESP           50       /* encap. security payload */
#define  IPPROTO_AH            51       /* authentication header */
#define  IPPROTO_MOBILE        55       /* IP Mobility RFC 2004 */
#define  IPPROTO_IPV6_ICMP     58       /* IPv6 ICMP */
#define  IPPROTO_ICMPV6        58       /* ICMP6 */
#define  IPPROTO_NONE          59       /* IP6 no next header */
#define  IPPROTO_DSTOPTS       60       /* IP6 destination option */
#define  IPPROTO_EON           80       /* ISO cnlp */
#define  IPPROTO_ENCAP         98       /* encapsulation header */
#define  IPPROTO_PIM          103       /* Protocol indep. multicast */
#define  IPPROTO_IPCOMP       108       /* IP Payload Comp. Protocol */
#define  IPPROTO_VRRP         112       /* VRRP RFC 2338 */

#define  IPPROTO_RAW          255       /* raw IP packet */
#define  IPPROTO_MAX          256

// last return value of *_input(), meaning "all job for this pkt is done".

#define  IPPROTO_DONE         257

//--------------------------------------------------------

#define  IPOPT_EOL                  0   /* end of option list */
#define  IPOPT_NOP                  1   /* no operation */

#define  IPOPT_RR                   7   /* record packet route */
#define  IPOPT_TS                  68   /* timestamp */
#define  IPOPT_SECURITY           130   /* provide s,c,h,tcc */
#define  IPOPT_LSRR               131   /* loose source route */
#define  IPOPT_SATID              136   /* satnet id */
#define  IPOPT_SSRR               137   /* strict source route */

 //-- Offsets to fields in options other than EOL and NOP.

#define  IPOPT_OPTVAL               0   /* option ID */
#define  IPOPT_OLEN                 1   /* option length */
#define  IPOPT_OFFSET               2   /* offset within option */
#define  IPOPT_MINOFF               4   /* min value of above */


//-- BSD code

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(push, 1)
#endif

/*
 * Structure of an internet header, naked of options.
 *
 * We declare ip_len and ip_off to be short, rather than u_short
 * pragmatically since otherwise unsigned comparisons can result
 * against negative integers quite easily, and fail in subtle ways.
 */

#define  IP_RF         0x8000      //-- reserved fragment flag
#define  IP_DF         0x4000      //-- dont fragment flag
#define  IP_MF         0x2000      //-- more fragments flag
#define  IP_OFFMASK    0x1fff      //-- mask for fragmenting bits

struct ip
{
#if CPU_BYTE_ORDER == LITTLE_ENDIAN
   unsigned char  ip_hl:4,     // header length
                  ip_v:4;      // version
#endif

#if CPU_BYTE_ORDER == BIG_ENDIAN
    unsigned char  ip_v:4,     // version
           ip_hl:4;            // header length
#endif
    unsigned char  ip_tos;     // type of service
    short   ip_len;            // total length
    unsigned short ip_id;      // identification
    short   ip_off;            // fragment offset field
    unsigned char  ip_ttl;     // time to live
    unsigned char  ip_p;       // protocol
    unsigned short ip_sum;     // checksum
    struct  in__addr ip_src;   // source and dest address
    struct  in__addr ip_dst;
}__pkt_struct;
typedef struct ip TN_IPHDR;

// Overlay for ip header used by other protocols (tcp, udp).

struct ipovly
{
   unsigned char * ih_next;              // 8 for protocol sequence q's
   unsigned char * ih_prev;
   unsigned char ih_x1;                  // 1(unused)
   unsigned char ih_pr;                  // 1 protocol
   short  ih_len;                        // 2 protocol length
   struct in__addr ih_src;               // 4 source internet address
   struct in__addr ih_dst;               // 4 destination internet address
}__pkt_struct;

//-- Fragment descriptor in a reassembly list

struct ip_frag   //-- 20 bytes
{
   struct ip_frag * prev;      //-- 4 Previous fragment on list
   struct ip_frag * next;      //-- 4 Next fragment

   struct mbuf * buf;              //-- 4 Actual fragment data
   int offset;                 //-- 4 Starting offset of fragment
   int last;                   //-- 4 Ending offset of fragment
};
typedef struct ip_frag TN_IPFRAG;

//-- Reassembly descriptor

struct ip_reasm   //-- 26 bytes
{
   struct ip_reasm * next;     //-- 4 Linked list pointer
   TN_IPFRAG * frag_list;      //-- 4 Head of data fragment chain
    u_short timeout;           //-- 2 Reassembly timeout timer
    u_short length;            //-- 2 Entire datagram length, if known

//-- src/dest/id/protocol uniquely describe a datagram (in network order)

   struct in__addr ip_src;     //-- 4
   struct in__addr ip_dst;     //-- 4
   u_short ip_id;              //-- 2
   u_char protocol;            //-- 1
//-----------------------------------------

   u_char status;              //-- 1 Uses by timer when was dropped by Max allowed fragments
   short  max_frag;            //-- 2 Max allowed fragments
}__pkt_struct;
typedef struct ip_reasm TN_IPREASM;

//-- Socket address, internet style.

struct sockaddr__in
{
   u_char  sin_len;
   u_char  sin_family;
   u_short sin_port;
   struct  in__addr sin_addr;
   char sin_zero[8];
}__pkt_struct;

#define IP4_SIN_ADDR_SIZE  (1 + 1 + 2 + 4)  //  sin->sin_len +  sin->sin_family +
                                            //  sin->sin_port +  sin->sin_addr.s__addr

// Internet implementation parameters.

#define  MAXTTL        255    //-- maximum time to live (seconds)
#define  IPDEFTTL       64    //-- default ttl, from RFC 1340

//#define  IPFRAGTTL      60    //-- time to live for frags, slowhz

#define  IPFRAGTTL       2    //-- time to live for frags - 1 sec

#define  IPTTLDEC        1    //-- subtracted when forwarding

#define  IP_MSS        576    //-- default maximum segment size

struct frag_entry
{
   struct frag_entry * next;
   struct mbuf * mb;
   int last;
   int offset;
   int flags;
}__pkt_struct;

struct reasm_entry
{
   struct frag_entry * frag_list;   //-- 4
   struct in__addr ra_ip_src;       //-- 4
   struct in__addr ra_ip_dst;       //-- 4
   int nfrag;                       //-- 4
   __pkt_field unsigned char  ra_ttl;  //-- 1 time for reass q to live
   __pkt_field unsigned char  ra_p;    //-- 1 protocol of this fragment
   __pkt_field unsigned short ra_id;   //-- 2 sequence id for reassembly

}__pkt_struct;


#if defined (__ICCARM__)   // IAR ARM
#pragma pack(pop)
#endif

//-- OK
#define  IN_CLASSA_NSHIFT        24
//-- OK
#define  IN_LOOPBACKNET         127  // Network number for local host loopback

 //---- tn_ip_input.c

void ip_stripoptions(struct mbuf * m);


#endif



