#ifndef _TN_ETH_H_
#define _TN_ETH_H_


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
 * Copyright (c) 1982, 1986, 1988, 1993
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

#define ETHERTYPE_IP    0x0800        /* IP protocol */
#define ETHERTYPE_ARP   0x0806        /* Addr. resolution protocol */

//----------------------------------------------------------------------------

   //--- Ethernet header

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(push, 1)
#endif

struct ether_header
{
   u_char   ether_dhost[6];
   u_char   ether_shost[6];
   u_short  ether_type;

}__pkt_struct;

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(pop)
#endif

// Address Resolution Protocol.

// See RFC 826 for protocol description.  ARP packets are variable
// in size; the arphdr structure defines the fixed-length portion.
// Protocol type values are the same as those for 10 Mb/s Ethernet.
// It is followed by the variable-sized fields ar_sha, arp_spa,
// arp_tha and arp_tpa in that order, according to the lengths
// specified.  Field names used correspond to RFC 826.

#define ARPHRD_ETHER          1      // ethernet hardware format
#define ARPHRD_FRELAY        15      // frame relay hardware format

                          //-- ar_op - one of:

#define ARPOP_REQUEST         1      // request to resolve address
#define ARPOP_REPLY           2      // response to previous request
#define ARPOP_REVREQUEST      3      // request protocol address given hardware
#define ARPOP_REVREPLY        4      // response giving protocol address
#define ARPOP_INVREQUEST      8      // request to identify peer
#define ARPOP_INVREPLY        9      // response identifying peer

   //--- ARP header
#if defined (__ICCARM__)   // IAR ARM
#pragma pack(push, 1)
#endif

struct  arphdr
{
   u_short   ar_hrd;                 // format of hardware address
   u_short   ar_pro;                 // format of protocol address
   u_char    ar_hln;                 // length of hardware address
   u_char    ar_pln;                 // length of protocol address
   u_short   ar_op;

 // The remaining fields are variable in size according to the sizes above.
}__pkt_struct;

      //-- ARP msg

// See RFC 826 for protocol description.  Structure below is adapted
// to resolving internet addresses.  Field names used correspond to RFC 826.


struct ether_arp
{
   struct  arphdr ea_hdr;     // fixed-size header
   u_char  arp_sha[6];        // sender hardware address
   u_char  arp_spa[4];        // sender protocol address
   u_char  arp_tha[6];        // target hardware address
   u_char  arp_tpa[4];        // target protocol address
}__pkt_struct;

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(pop)
#endif

#define   arp_hrd       ea_hdr.ar_hrd
#define   arp_pro       ea_hdr.ar_pro
#define   arp_hln       ea_hdr.ar_hln
#define   arp_pln       ea_hdr.ar_pln
#define   arp_op        ea_hdr.ar_op



//----------------------------------------------------------------------------
// TN NET
//----------------------------------------------------------------------------

#define  ARP_FREE             0   //-- Valid only after queue creation
#define  ARP_PENDING          1
#define  ARP_SOLVED           2
#define  ARP_STATE_MASK    0x03

#define  ARP_PERMANENT     0x08   //-- Independed Flag

#define  ARP_MAX_TRIES        5
#define  ARP_TIME_DOWN       20   //-- once declared down, don't send for 20 secs
#define  ARP_TIME_KEEP   (20*60)  //-- once resolved, good for 20 more minutes

#define  MAC_SIZE             6

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(push, 1)
#endif

struct _TN_ARPENTRY            //-- 20 bytes
{
   unsigned int rt_expire;           //-- 4
   TN_MBUF * la_hold;                //-- 4  mbuf to send(only last)
#ifdef TN_ARP_EXTRA_LAHOLD   
   TN_MBUF * la_hold1;               //-- 4  
#endif
 //---
   struct in__addr itaddr;           //-- 4
   unsigned char mac_addr[MAC_SIZE]; //-- 6
 //---
   unsigned char flags;              //-- 1
   unsigned char la_asked;           //-- 1  counter Num arp req send to calc pause
} __pkt_struct;

typedef struct _TN_ARPENTRY TN_ARPENTRY;

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(pop)
#endif

     //--- tn_eth.c

TN_MBUF * add_eth_header(TN_NET * tnet,
                         TN_MBUF * mb,             //-- mbuf to send
                         unsigned char * eth_dst,  //-- Net order
                         unsigned char * eth_src,  //-- Net order
                         int eth_type);             //-- Host order
void arp_init(TN_NETIF * ni,
              TN_ARPENTRY * arp_arr,
              int nelem,
              TN_SEM * p_arp_sem);
void arp_input(TN_NET * tnet, TN_NETIF * ni, struct mbuf * mb);
void arp_request(TN_NET * tnet,
                        TN_NETIF * ni,
                        unsigned long tip);       //--- Target IP addr (grt hw to it)
 int arp_resolve(TN_NET * tnet,
                 TN_NETIF * ni,
                 TN_MBUF * mb,
                 struct in__addr arp_ip_addr,
                 unsigned char * dst); //-- [OUT]

//void  arp_timer_func(TN_NETINFO * tneti, int cnt);

#endif



