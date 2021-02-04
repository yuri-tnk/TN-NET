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
#include "tn_net_func.h"
#include "tn_net_mem_func.h"

#include "dbg_func.h"

#if defined (__ICCARM__)   // IAR ARM
#pragma diag_suppress=Pa039
#endif

//----- Local functions prototypes

static TN_NETIF * ip_route_out(TN_NET * tnet, struct in__addr * dst);

static int ip4_fragment(TN_NET * tnet,
                        TN_MBUF  *  m,       //-- mbuf chain to fragment
                        TN_NETIF *  ni);

static int ip_optcopy(TN_IPHDR * org_iphdr, TN_IPHDR * new_iphdr);

//----------------------------------------------------------------------------
// IP4 output.
// The packet in mbuf chain 'mb' contains a skeletal
// IP header (with len, off, ttl, proto, tos, src, dst).
//  m0->m_data - points to the IP header
//----------------------------------------------------------------------------
int ip_output(TN_NET * tnet, TN_NETIF * ni,  TN_MBUF * m0)
{
   int hlen;
   TN_IPHDR * iphdr;
   int rc;
   TN_MBUF * m;

   rc = 0;
   m = m0;

   iphdr = (TN_IPHDR *)m0->m_data;
   hlen =  sizeof(TN_IPHDR); //-- No options

   iphdr->ip_v  = 4;         //--IPVERSION;
   iphdr->ip_off &= IP_DF;
   iphdr->ip_id = htons(tn_net_random_id());
   iphdr->ip_hl = hlen >> 2;

  //--ipstat.ips_localout++;

   if(ni == NULL)
   {
      ni = ip_route_out(tnet, &iphdr->ip_dst);
      if(ni == NULL)
      {
         m_freem(tnet, m0);
         return EINVAL;
      }
   }

   //-- Look for broadcast address and and verify user is allowed to send
   //-- such a packet.

   if(ntohl(iphdr->ip_dst.s__addr) == _INADDR_BROADCAST)
   {
      if((ni->if_flags & IFF_BROADCAST) == 0)
      {
         m_freem(tnet, m0);
         return EADDRNOTAVAIL;
      }

      //-- don't allow broadcast messages to be fragmented

      if(ntohs(iphdr->ip_len) > ni->if_mtu)
      {
         m_freem(tnet, m0);
         return EMSGSIZE;
      }
      m->m_flags |= M_BCAST;
   }
   else
      m->m_flags &= ~M_BCAST;

   //-- If small enough for interface MTU - send it directly.

   if(ntohs(iphdr->ip_len) <= ni->if_mtu)
   {
      iphdr->ip_sum = 0;
      iphdr->ip_sum = in_cksum(m, hlen);

      if(ni->output_func)
         rc = ni->output_func(tnet, ni, m);
      else
      {
         m_freem(tnet, m);
         rc = TERR_WSTATE;
      }
      return  rc;
   }

 //---- Too large for the interface - fragment if possible ------

   //-- Must be able to put at least 8 bytes per fragment.

   if(iphdr->ip_off & htons(IP_DF))
   {
      //ipstat.ips_cantfrag++;
      m_freem(tnet, m0);
      return EMSGSIZE;
   }

   rc = ip4_fragment(tnet, m, ni);
   if(rc != 0) //error
   {
      m_freem(tnet, m0);
      return rc;
   }

   //-- The original mbuf 'm' still first in the chain

   for(; m != NULL; m = m0)
   {
      m0 = m->m_nextpkt;
      m->m_nextpkt = NULL;
      if(rc == 0) //-- O.K
      {
         if(ni->output_func)
            rc = ni->output_func(tnet, ni, m);
         else
         {
            m_freem(tnet, m);
            rc = EINVAL;
         }
      }
      else
         m_freem(tnet, m);
   }

   if(rc == 0)
   {
    //  ipstat.ips_fragmented++;
   }
   return  rc;
}

//----------------------------------------------------------------------------
// Finds the appropriate network interface for a given IP address.
// dst - in network byte order
//----------------------------------------------------------------------------
static TN_NETIF * ip_route_out(TN_NET * tnet, struct in__addr * dst)
{
   TN_NETIF * ni;

   if(dst == NULL)
      return NULL;

   for(ni = tnet->netif_list; ni != NULL; ni = ni->next)
   {
      if(ni->if_flags & IFF_UP)
      {
         if((dst->s__addr & ni->netmask.s__addr) ==
                              (ni->ip_addr.s__addr & ni->netmask.s__addr))
            return ni;
      }
   }
   //-- If here - not found netif

   if(tnet->netif_default == NULL)
      return NULL;

   if((tnet->netif_default->if_flags & IFF_UP) == 0)
      return NULL;

  //-- no matching netif found - use default netif

   return tnet->netif_default;
}

//----------------------------------------------------------------------------
//  No options; input - in the host byte order
//----------------------------------------------------------------------------
void ip4_fill_header(TN_IPHDR * iphdr,
                    struct in__addr * src,
                    struct in__addr * dst,
                    int ttl,
                    int proto)
{
   if(iphdr == NULL)
      return;

   iphdr->ip_hl  = (sizeof(TN_IPHDR))>>2;        // header length
   iphdr->ip_v   = 4;                            // version
   iphdr->ip_tos = 0;                            // type of service
   iphdr->ip_len = 0;                            // total length
   iphdr->ip_id  = htons(tn_net_random_id());    // identification
   iphdr->ip_off = 0;                            // fragment offset field
   iphdr->ip_ttl = 255;                          // time to live
   iphdr->ip_p   = proto;                        // protocol
   iphdr->ip_sum = 0;                            // checksum
   iphdr->ip_src.s__addr = htons(src->s__addr);  // source and dest address
   iphdr->ip_dst.s__addr = htons(dst->s__addr);

}

//----------------------------------------------------------------------------
static int ip_optcopy(TN_IPHDR * ip, TN_IPHDR * jp)
{
 // Copy options from ip to jp,
 // omitting those not copied during fragmentation.

   u_char *cp, *dp;
   int opt, optlen, cnt;

   cp = (u_char *)(ip + 1);
   dp = (u_char *)(jp + 1);
   cnt = (ip->ip_hl << 2) - sizeof (struct ip);
   for(; cnt > 0; cnt -= optlen, cp += optlen)
   {
      opt = cp[0];
      if(opt == IPOPT_EOL)
         break;
      if(opt == IPOPT_NOP)
      {
         //-- Preserve for IP mcast tunnel's LSRR alignment.

         *dp++ = IPOPT_NOP;
         optlen = 1;
         continue;
      }
      optlen = cp[IPOPT_OLEN];

     //-- bogus lengths should have been caught by ip_dooptions

      if(optlen > cnt)
         optlen = cnt;

#define  IPOPT_COPIED(o)   ((o)&0x80)

      if(IPOPT_COPIED(opt))
      {
         bcopy((unsigned char *)cp, (unsigned char *)dp, (unsigned)optlen);
         dp += optlen;
      }
   }

   for(optlen = dp - (u_char *)(jp+1); optlen & 0x3; optlen++)
       *dp++ = IPOPT_EOL;

   return optlen;
}

//----------------------------------------------------------------------------
static int ip4_fragment(TN_NET * tnet,
                        TN_MBUF  *  m,           //-- mbuf chain to fragment
                        TN_NETIF *  ni)
{
   int mhlen;
   int firstlen;
   struct mbuf ** mnext;
   TN_IPHDR * ip;
   TN_IPHDR * mhip;
   TN_MBUF * m0;
   int hlen;
   int frag_len;
   int off;
   int rc;

   ip = (TN_IPHDR*)m->m_data; //-- Orignal ip
   hlen = ip->ip_hl << 2;     //-- Actual original IP header len(with options, if any)

   if(hlen > TNNET_MID1_BUF_SIZE) //-- Huge options - discard
   {
      m_freem(tnet, m);
      return EMSGSIZE;
   }

   frag_len = (ni->if_mtu - hlen) & ~7;   //-- Each fragment capacity
   if(frag_len < 8)
   {
      m_freem(tnet, m);
      return EMSGSIZE;
   }

   //--  Loop through length of segment after first fragment,
   //--  make new header and copy data of each part and link onto chain.

   mnext    = &m->m_nextpkt;
   firstlen = frag_len;

   m0 = m; //-- Save original mbuf ptr
   rc = 0; //--  preset to O.K;
   mhlen = sizeof(struct ip);
   for(off = hlen + frag_len; off < ntohs(ip->ip_len); off += frag_len)
   {
      m = mb_get(tnet, MB_MID1, MB_NOWAIT, TRUE); //-- From Tx pool
      if(m == NULL)
      {
         //ipstat.ips_odropped++;
         rc = ENOBUFS;
         break;
      }

      *mnext = m;
      mnext  = &m->m_nextpkt;

      mhip = (struct ip *)m->m_data;

    //-- Copy original IP header to do the reference IP header for the fragment

      bcopy(ip, mhip, sizeof(struct ip));

      m->m_flags |= m0->m_flags & (M_MCAST|M_BCAST);
      m->m_flags |= M_PKTHDR;

      if(hlen > sizeof(struct ip)) //-- There are options in the original IP header
      {
         mhlen = ip_optcopy(ip, mhip) + sizeof(struct ip);
         mhip->ip_hl = mhlen >> 2;
      }
      m->m_len     = mhlen;
      mhip->ip_off = ((off - hlen) >> 3) + (ntohs(ip->ip_off) & ~IP_MF);

      if(ip->ip_off & htons(IP_MF))
         mhip->ip_off |= IP_MF;

      if(off + frag_len >= ntohs(ip->ip_len))
         frag_len = ntohs(ip->ip_len) - off;
      else
         mhip->ip_off |= IP_MF;

      mhip->ip_len = htons((u_short)(frag_len + mhlen));
      m->m_next    = m_copym(tnet, m0, off, frag_len, MB_NOWAIT);
      if(m->m_next == NULL)
      {
         // ipstat.ips_odropped++;
         rc = ENOBUFS;
         break;
      }

      m->m_tlen = mhlen + frag_len;

      mhip->ip_off = htons((u_short)mhip->ip_off);
      mhip->ip_sum = 0;
      mhip->ip_sum = in_cksum(m, mhlen);

      //ipstat.ips_ofragments++;
   }

   if(rc != 0) //-- No memory for all fragments - do not send any fragment
   {
      for(m = m0; m != NULL; m = m0)
      {
         m0 = m->m_nextpkt;
         m->m_nextpkt = NULL;
         m_freem(tnet, m);
      }
      return rc;
   }

   //-- Update first fragment by trimming what's been copied out
   //-- and updating header

   m = m0;   //-- Restore original mbuf
   m->m_flags |= M_PKTHDR;

   m_adj_free(tnet, m, hlen + firstlen - ntohs(ip->ip_len)); //--

   m->m_tlen = hlen + firstlen;

   ip->ip_len  = htons((u_short)m->m_tlen);
   ip->ip_off |= htons(IP_MF);
   ip->ip_sum  = 0;
   ip->ip_sum  = in_cksum(m, hlen);

   return rc;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



