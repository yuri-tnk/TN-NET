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

#include <stddef.h>  //-- NULL, etc

#include "../tnkernel/tn.h"

#include "tn_net_cfg.h"
#include "tn_net_types.h"
#include "tn_net_pcb.h"
#include "tn_net_mem.h"
#include "tn_ip.h"
#include "tn_net.h"
#include "errno.h"
#include "tn_mbuf.h"
#include "tn_netif.h"
#include "tn_net_utils.h"
#include "ip_icmp.h"
#include "tn_udp.h"
#include "tn_net_func.h"
#include "tn_net_mem_func.h"

#ifdef TN_DHCP
#include "tn_dhcp.h"
#endif

#include "dbg_func.h"


//-- Local functions prototypes

static TN_MBUF * ip4_reasm(TN_NET * tnet, TN_MBUF * mb_in, int iphlen);
static void ip_freef(TN_NET * tnet, struct reasm_entry * re);

//---- This is a rare global var - IP reaasembly list

static struct reasm_entry g_ip_reasm_arr[IP4_REASM_PACKETS];


#define IP_LOCK       tn_sem_acquire(&tnet->ip_sem, TN_WAIT_INFINITE);
#define IP_UNLOCK     tn_sem_signal(&tnet->ip_sem);

//----------------------------------------------------------------------------
void ip_init(TN_NET * tnet)
{
   tnet->ip_reasm_list = &g_ip_reasm_arr[0];
}

//----------------------------------------------------------------------------
int ip_addr_isbroadcast(TN_NETIF * ni, unsigned int addr)
{
  //-- all IP addresses - in network order here

   if(addr == 0 || addr == 0xFFFFFFFF)
      return TRUE;
   else if((ni->if_flags & IFF_BROADCAST) == 0)
      return FALSE;
   else if (addr == ni->ip_addr.s__addr)
      return FALSE;
   else if(((ni->ip_addr.s__addr & ni->netmask.s__addr) ==
                                             (addr & ni->netmask.s__addr)) &&
           ((addr & (~ni->netmask.s__addr)) == ~ni->netmask.s__addr))
      return TRUE;

   return FALSE;
}

//----------------------------------------------------------------------------
// Ip input routine.
// The mbuf 'm' m_data member points to the IP header.
//----------------------------------------------------------------------------
void ipv4_input(TN_NET * tnet, TN_NETIF * ni, struct mbuf * m)
{
   struct ip * ip;
   int hlen;
   volatile int h_ip_off;
   int i;
   int len;
   TN_NETIF * netif;
   int sm;
#if TN_DHCP
   int fChkIpSrc;
#endif

   if(m->m_len < sizeof(struct ip))
   {
     // ipstat.ips_toosmall++;
      m_freem(tnet, m);
      return;
   }

   ip = (struct ip *)m->m_data;
   if(ip->ip_v != IPPROTO_IPV4) //-- IPPROTO_IPV4 = 4
   {
    // ipstat.ips_badvers++;
      m_freem(tnet, m);
      return;
   }

#define MAX_IP_HDR_LEN  (128-40)

   hlen = ip->ip_hl << 2;
   if(hlen < sizeof(struct ip) ||  //-- minimum header length
          hlen > MAX_IP_HDR_LEN)      //-- (huge IP Options???) - discard
   {
     // ipstat.ips_badhlen++;
      m_freem(tnet, m);
      return;
   }

        //-- 127/8 must not appear on wire - RFC1122
   if((ntohl(ip->ip_dst.s__addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
            (ntohl(ip->ip_src.s__addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
   {
      if((ni->if_flags & IFF_LOOPBACK) == 0)
      {
       //  ipstat.ips_badaddr++;
         m_freem(tnet, m);
         return;
      }
   }

  //-- Do checksum here

   ip->ip_sum = in_cksum(m, hlen);
   if(ip->ip_sum)// != 0)
   {
    //  ipstat.ips_badsum++;
      m_freem(tnet, m);
      return;
   }

  //-- Retrieve the packet length.

   len = ntohs(ip->ip_len);
   if(len < hlen)
   {
     // ipstat.ips_badlen++;
      m_freem(tnet, m);
      return;
   }

  // Check that the amount of data in the buffers
  // is at least as much as the IP header would have us expect.
  // Trim mbufs if longer than we expect.
  // Drop packet if shorter than we expect.

   if((int)m->m_tlen < len)
   {
      // ipstat.ips_tooshort++;
      m_freem(tnet, m);
      return;
   }

   if((int)m->m_tlen > len)
   {
      if(m->m_len == m->m_tlen)
      {
         m->m_len = len;
         m->m_tlen = len;
      }
      else
         m_adj(m, len - m->m_tlen);
   }

#if NPF > 0

        // Packet filter

//        pfrdr = ip->ip_dst.s__addr;
//        if(pf_test(PF_IN, ni, &m, NULL) != PF_PASS)
//           goto bad;
//        if(m == NULL)
//           return;
//
//        ip = mtod(m, struct ip *);
//        hlen = ip->ip_hl << 2;
//        pfrdr = (pfrdr != ip->ip_dst.s_addr);
#endif


   //-- Process options and, if not destined for us,
   //-- ship it on.  ip_dooptions returns 1 when an
   //-- error was detected (causing an icmp message
   //-- to be sent and the original packet to be freed).

         //--  TN NET does not supports IP options now


   //-- Check our list of addresses, to see if the packet is for us.

   netif = NULL;

#if TN_IGMP
   if(IN_MULTICAST(ntohl(iphdr->ip_dst.s_addr)))
   {
     if((in_netif->if_flags & IFF_BROADCAST) && (igmp_lookfor_group(in_netif,
                                                           &(iphdr->ip_dst))))
        netif = in_netif;
     else
        netif = NULL;
   }
   else
#endif
   {
      for(netif = tnet->netif_list; netif != NULL; netif = netif->next)
      {
         if(netif->if_flags & IFF_UP) //-- Iface is up
         {
               // unicast to this interface address?
               // or broadcast on this interface network address?
            if(ip->ip_dst.s__addr == netif->ip_addr.s__addr || //-- both - in network order
              ip_addr_isbroadcast(netif, ip->ip_dst.s__addr))
               break;  //-- found, OK
         }
      }
   }

#if TN_DHCP

     // Pass DHCP messages regardless of the destination address
     // (just verify the DHCP port)

   fChkIpSrc = TRUE;
   if(netif == NULL)
   {
      if(ip->ip_p == IPPROTO_UDP)
      {
         if(ntohs(((struct udphdr *)((unsigned char *)ip + hlen))->uh_dport)
                                                          == DHCP_CLIENT_PORT)
         {
            netif = ni;
            fChkIpSrc = FALSE;
         }
      }
   }

   if(fChkIpSrc) //-- if FALSE - this is DHCP packet, not discard any case
#endif //-- TN_DHCP

       //--  broadcast or multicast packet source address is not valid
       //--  Compliant with RFC 1122: 3.2.1.3
   {
      if(ip_addr_isbroadcast(ni, ip->ip_src.s__addr) ||
                                 IN_MULTICAST(ntohl(ip->ip_src.s__addr)))
      {
         m_freem(tnet, m);
         return;
      }
   }

   if(netif == NULL)  // The packet not for us - route or discard
   {
      //-- No routing support here, discard only
      m_freem(tnet, m);
      return;
   }

  //--  If offset or IP_MF are set, must reassemble.
  //--  Otherwise, nothing need be done.

   h_ip_off = ntohs(ip->ip_off);

   if(h_ip_off & ~(IP_DF | IP_RF))
   {
      if((h_ip_off & IP_MF) != 0) //-- Not last IP fragment
      {
         // Make sure that fragments have a data length
         // that's a non-zero multiple of 8 bytes.

         i = ntohs(ip->ip_len) - hlen;
         if(i == 0 || (i & 0x7) != 0)
         {
            // ipstat.ips_badfrags++;
            m_freem(tnet, m);
            return;
         }
      }

      // ipstat.ips_fragments++;

      IP_LOCK

      m = ip4_reasm(tnet, m, hlen);

      IP_UNLOCK

      if(m == NULL)
         return;
   } //-- end reassemling

   //-- Switch out to protocol's input routine.

   switch(ip->ip_p)
   {
      case IPPROTO_ICMP:       //-- control message protocol

         icmp_input(tnet, ni, m);
         break;
#if TN_IGMP
      case IPPROTO_IGMP:         //-- group mgmt protocol
         break;
#endif

      case IPPROTO_UDP:          //-- user datagram protocol

         udp_input(tnet, ni, m);
         break;

      case IPPROTO_TCP:          //-- tcp

//dbg_pin_on(P1_26_MASK);

         sm = splnet(tnet);

         tcp_input(tnet, ni, m);

         splx(tnet, sm);

//dbg_pin_off(P1_26_MASK);

         break;
/*
      case IPPROTO_RAW:          //-- raw IP packet
         raw_input(tnet, ni, m);
         break;
*/
      default:  //-- Unsupported/unknown (includes IPPROTO_IP, IPPROTO_IPV4 etc)
         m_freem(tnet, m);
       //-- Statistics
         break;
   }
}

//----------------------------------------------------------------------------
// Take incoming datagram fragment and try to
// reassemble it into whole datagram.
//----------------------------------------------------------------------------
static TN_MBUF * ip4_reasm(TN_NET * tnet, TN_MBUF * mb_in, int iphlen)
{
   struct reasm_entry * re;
   struct frag_entry * new_frag;
   struct frag_entry * prev_frag;
   struct frag_entry * next_frag;
   TN_MBUF * mb;
   TN_MBUF * mb_last;
   TN_MBUF * mb_first;
   int free_reasm_entry;
   struct ip * ip;
   int i;
   int len;

   if(mb_in  == NULL || tnet == NULL)
      return NULL;
   if(tnet->ip_reasm_list == NULL)
      return NULL;

   ip = (struct ip *)mb_in->m_data;

   free_reasm_entry = -1;
   for(i = 0; i < IP4_REASM_PACKETS; i++)
   {
      re = &tnet->ip_reasm_list[i];
      if(re)
      {
         if(re->frag_list == NULL) //-- Entry is free
            free_reasm_entry = i;
         else
         {
            if(ip->ip_id == re->ra_id &&
                ip->ip_src.s__addr == re->ra_ip_src.s__addr &&
                   ip->ip_dst.s__addr == re->ra_ip_dst.s__addr &&
                      ip->ip_p == re->ra_p)
               break;
         }
      }
   }
  //------------------

   if(i == IP4_REASM_PACKETS) //-- Entry not found
   {
      //--- There are no fragments like current and no free entries
      if(free_reasm_entry == -1)
      {
         //--- Free ALL entries
         for(i = 0; i < IP4_REASM_PACKETS; i++)
         {
            re = &tnet->ip_reasm_list[i];
            ip_freef(tnet, re);
         }
         free_reasm_entry = 0; //-- Use first
      }

      //-- Fill new 'packet to reasm' entry

      re = &tnet->ip_reasm_list[free_reasm_entry];

      re->ra_ip_src.s__addr = ip->ip_src.s__addr;
      re->ra_ip_dst.s__addr = ip->ip_dst.s__addr;
      re->ra_ttl = IPFRAGTTL;   //--  time for reass q to live
      re->ra_p   = ip->ip_p;    //--  protocol of this fragment
      re->ra_id  = ip->ip_id;   //--  sequence id for reassembly
      re->nfrag  = 0;
   }

//--  We have a limited quantity of Hi mbufs and we set
//--  max reasm bufs qty as 2 (and max input packet size is 2 MTU (1500 * 2 = 3000 bytes)

   re->nfrag++;
   if(re->nfrag > IP4_MAX_REASM_FRAGMENTS)
   {
      ip_freef(tnet, re);
      m_freem(tnet, mb_in);
      return NULL;
   }
//--------- Debug only -----------
//--------- End Debug only -----------

   //--- Create new fragmen entry

   new_frag = (struct frag_entry *)tn_net_alloc_min(tnet, MB_NOWAIT);
   if(new_frag == NULL)
   {
      m_freem(tnet, mb_in);
      return NULL;
   }
   new_frag->next = NULL;
   new_frag->mb   = NULL;
   new_frag->offset = (ntohs(ip->ip_off) & IP_OFFMASK) << 3;
   new_frag->last   = ntohs(ip->ip_len) - iphlen + new_frag->offset;
   new_frag->flags  = ntohs(ip->ip_off) & ~IP_OFFMASK;

   //-- Set next_frag to the first fragment which begins after us,
   //-- and prev_frag to the last fragment which begins before us

   prev_frag = NULL;
   for(next_frag = re->frag_list; next_frag != NULL; next_frag = next_frag->next)
   {
      if(next_frag->offset > new_frag->offset)
         break;
      prev_frag = next_frag;
   }

   //-- Cut IP header if segment is not first( offset !=0)

   if(new_frag->offset != 0)  //-- segment is not first
   {
      mb_in->m_len  -= iphlen;
      mb_in->m_data += iphlen;
      if(mb_in->m_len <= 0)
      {
         m_freem(tnet, mb_in);
         tn_net_free_min(tnet, new_frag);
         return  NULL;
      }
   }

   //-- Check for overlap with preceeding fragment

   if(prev_frag)
   {
      len = prev_frag->last - new_frag->offset;
      if(len > 0)
      {
      // Strip overlap from new fragment

         if(mb_in->m_len <= len)// Nothing left
         {
            m_freem(tnet, mb_in);
            tn_net_free_min(tnet, new_frag);
            return  NULL;
         }
         new_frag->offset += len;
         mb_in->m_len  -= len;
         mb_in->m_data += len;
      }
   }

   //-- Look for the overlap with succeeding segments

   if(next_frag)
   {
      len = new_frag->last - next_frag->offset;
      if(len > 0)
      {
         if(len >= mb_in->m_len)
         {
            m_freem(tnet, mb_in);
            tn_net_free_min(tnet, new_frag);
            return  NULL;
         }
         mb_in->m_len -= len;
      }
   }

  //--- Add to the reasm queue

   new_frag->mb = mb_in;

   if(re->frag_list == NULL)
      re->frag_list = new_frag;

   if(prev_frag != NULL)
      prev_frag->next = new_frag;

   new_frag->next = next_frag;

   //-- Check for the reassembly complete:
   //     1. no holes
   //     2. The last fragment in the queue has a flag MF = 0

   prev_frag = NULL;
   for(next_frag = re->frag_list; next_frag != NULL; next_frag = next_frag->next)
   {
      if(prev_frag)
      {
         if(next_frag->offset != prev_frag->last)
         {
            prev_frag = NULL; //-- like 'hole exists' flag
            break;
         }
      }
      prev_frag = next_frag;
   }

 //-- No holes;  The 'prev_frag' here is a last fragment in the reasm chain

   if(prev_frag)
   {
      if((prev_frag->flags & IP_MF) == 0)
      {
       //-- Reassembly finished - prepare resulting mbuf

         len = 0;
         prev_frag = NULL;
         mb_last   = NULL;
         mb_first  = re->frag_list->mb;
         next_frag = re->frag_list;
         for(;;)
         {
            if(next_frag == NULL)
               break;

            prev_frag = next_frag->next;

           //-- In fact, a single mbuf at the entry is here

            mb = next_frag->mb;
            len += mb->m_len;
            while(mb->m_next)  //-- Go to last mbuf in chain 'mb'
            {
               mb = mb->m_next;
               len += mb->m_len;
            }

            if(mb_last) //-- Not first entry
               mb_last->m_next = next_frag->mb;

            mb_last = mb;

            tn_net_free_min(tnet, next_frag);

            next_frag = prev_frag;
         }

         re->frag_list = NULL; //-- Free reasm packet entry

        //--- Update IP header

         mb_first->m_tlen = len;

         ip = (struct ip*)mb_first->m_data;
         ip->ip_len = htons((unsigned short)len);
         ip->ip_off &= htons(IP_DF | IP_RF);

         return mb_first;
      }
   }

   return NULL;
}


//----------------------------------------------------------------------------
// IP timer processing; If a timer expires on a reassembly queue, discard it.
//----------------------------------------------------------------------------
void ip_slowtimo(TN_NETINFO * tneti, int cnt)
{
   TN_NET * tnet = NULL;
   struct reasm_entry * re;
   int i;

   if(cnt%5 == 0) //-- each 500 ms
   {
      tnet = tneti->tnet;
      if(tnet == NULL)
         return;

      IP_LOCK

      for(i = 0; i < IP4_REASM_PACKETS; i++)
      {
         re = &tnet->ip_reasm_list[i];
         if(re->ra_ttl)
         {
            re->ra_ttl--;
            if(re->ra_ttl == 0)
            {
          //  ipstat.ips_fragtimeout++;
               ip_freef(tnet, re);
            }
         }
      }

      IP_UNLOCK
   }
}

//----------------------------------------------------------------------------
void ip_stripoptions(struct mbuf * m)
{
   int i;
   struct ip * ip;
   unsigned char * opts;
   int olen;

 //-- OpenBSD

   ip = mtod(m, struct ip *);

   olen = (ip->ip_hl<<2) - sizeof (struct ip);
   opts = (unsigned char *)(ip + 1);
   i = m->m_len - (sizeof(struct ip) + olen);
   bcopy(opts  + olen, opts, (unsigned int)i);
   m->m_len -= olen;
   if(m->m_flags & M_PKTHDR)
      m->m_tlen -= olen;

   ip->ip_hl = sizeof(struct ip) >> 2;
}

//----------------------------------------------------------------------------
// Free a fragment reassembly header and all associated datagrams.
//----------------------------------------------------------------------------
static void ip_freef(TN_NET * tnet, struct reasm_entry * re)
{
   struct frag_entry * frag;
   struct frag_entry * tmp_frag;

   if(re == NULL)
      return;

   frag = re->frag_list;
   for(;;)
   {
      if(frag == NULL)
         break;

      tmp_frag = frag->next;

      m_freem(tnet, frag->mb);
      tn_net_free_min(tnet, frag);

      frag = tmp_frag;
   }

   re->frag_list = NULL; //-- Free reasm entry
   re->ra_ttl = 0;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------




