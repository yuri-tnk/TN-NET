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
#include "ip_icmp.h"
#include "tn_net_utils.h"

#include "tn_net_func.h"


//----------------------------------------------------------------------------
// mb->m_data - points to the IP header
//----------------------------------------------------------------------------
void icmp_input(TN_NET * tnet, TN_NETIF * ni, struct mbuf * mb)
{
   struct icmp *icp;
   struct ip * ip;
   TN_MBUF * mb_tx;
   int icmplen;
   int i;
   int hlen;

   ip =  (struct ip *)(mb->m_data);
   hlen = ip->ip_hl << 2;

   icmplen = ntohs(ip->ip_len);
   if(icmplen < ICMP_MINLEN)
   {
      // icmpstat.icps_tooshort++;
      m_freem(tnet, mb);
      return;
   }

   i = hlen + _min(icmplen, ICMP_ADVLENMIN);
   if(mb->m_len < i)
   {
      // icmpstat.icps_tooshort++;
      m_freem(tnet, mb);
      return;
   }

   mb->m_len  -= hlen;
   mb->m_data += hlen;

   if(in_cksum(mb, icmplen))
   {
   //      icmpstat.icps_checksum++;
      m_freem(tnet, mb);
      return;
   }

   icp = (struct icmp *)mb->m_data;
   switch(icp->icmp_type)
   {
      case ICMP_ECHO:

      //-- To Do - add chk bcast & mcast src ip addr

  //  if(ip_addr_isbroadcast(&iphdr->dest, inp) || ip_addr_ismulticast(&iphdr->dest))
  //  {
  //     m_freem(tnet, mb);
  //     return;
  //  }

         icp->icmp_type = ICMP_ECHOREPLY;
         mb->m_len  += hlen;
         mb->m_data -= hlen; //-- now m_data points to the IP header


      //-- Replace buf to tx pool buf

         if((unsigned int)mb->m_dbuf < MEM_DRV_START_ADDR &&
                 ((mb->m_flags & M_DBHDR) && mb->m_dbtype == MB_MID1))
         {
            mb_tx = mb_get(tnet, MB_MID1, MB_NOWAIT, TRUE); //-- From Tx pool
            if(mb_tx == NULL)
            {
               m_freem(tnet, mb);
               return;
            }
            mb_tx->m_flags = mb->m_flags; // ????
            mb_tx->m_len   = mb->m_len;
            mb_tx->m_tlen  = mb->m_tlen;
            bcopy(mb->m_data, mb_tx->m_data, mb_tx->m_len);

            m_freem(tnet, mb);
            //g_tx_mr++;

            mb = mb_tx;
         }

         icmp_reflect(tnet, ni, mb);
         return;

      default:
         m_freem(tnet, mb);
         break;
   }
}

//----------------------------------------------------------------------------
void icmp_reflect(TN_NET * tnet, TN_NETIF * ni, struct mbuf * mb)
{
   struct in__addr tmp_ip_addr;
   struct ip * ip;

   //-- now m_data points to the IP header

   ip = (struct ip *)mb->m_data;

 //--- Exchange IP src & dst addresses

   tmp_ip_addr.s__addr = ip->ip_dst.s__addr;
   ip->ip_dst.s__addr  = ip->ip_src.s__addr;
   ip->ip_src.s__addr  = tmp_ip_addr.s__addr;

   ip->ip_ttl  = MAXTTL;

   mb->m_flags &= ~(M_BCAST|M_MCAST);

   icmp_send(tnet, ni, mb);
}

//----------------------------------------------------------------------------
void icmp_send(TN_NET * tnet, TN_NETIF * ni, struct mbuf * mb)
{
   //-- now mb->m_data poits to the IP header

   struct ip * ip;
   struct icmp * icp;
   int hlen;
   int ip_len;

   ip     = (struct ip *)mb->m_data;
   hlen   = ip->ip_hl << 2;
   ip_len = ntohs(ip->ip_len);

   //-- now m_data will points to the ICMP header

   mb->m_data += hlen;
   mb->m_len  -= hlen;

   icp = (struct icmp *)mb->m_data;

   icp->icmp_cksum = 0;
   icp->icmp_cksum = in_cksum(mb, ip_len - hlen);

   //-- now mb->m_data will points to the IP header again

   mb->m_data -= hlen;
   mb->m_len  += hlen;

   ip_output(tnet, ni, mb);
}

//----------------------------------------------------------------------------
// icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT, 0, 0);
// Generate an error packet of type error in response to bad packet ip.
//----------------------------------------------------------------------------
void icmp_error(TN_NET * tnet,
                TN_NETIF * ni,
                struct mbuf * n, //-- will be free here
                int type,        // ICMP_UNREACH
                int code,        // ICMP_UNREACH_PORT
                unsigned long dest)
{
   struct ip * oip;
   struct ip * nip;
   unsigned int oiplen;
   struct icmp * icp;
   struct mbuf * m;
   unsigned int icmplen;

   oip = (struct ip *)n->m_data;
   oiplen = oip->ip_hl << 2;

// icmpstat.icps_error++;

  // Don't send error if not the first fragment of message.
  // Don't error if the old packet protocol was ICMP
  // error message, only known informational types.

   if(oip->ip_off &  ~htons(IP_MF|IP_DF))
   {
      m_freem(tnet, n);
      return;
   }

   if(oip->ip_p == IPPROTO_ICMP &&  type != ICMP_REDIRECT &&
       n->m_len >= oiplen + ICMP_MINLEN &&
          !ICMP_INFOTYPE(((struct icmp *)((unsigned char *)oip + oiplen))->icmp_type))
   {
     // icmpstat.icps_oldicmp++;
      m_freem(tnet, n);
      return;
   }

   //-- Don't send error in response to a multicast or broadcast packet

   if(n->m_flags & (M_BCAST|M_MCAST))
   {
      m_freem(tnet, n);
      return;
   }

  //-- First, formulate icmp message

   m = mb_get(tnet, MB_MID1, MB_NOWAIT, TRUE); //-- From Tx pool
   if(m == NULL)
   {
      m_freem(tnet, n);
      return;
   }

   icmplen = oiplen + _min(8, ntohs(oip->ip_len));
   m->m_len = icmplen + ICMP_MINLEN;

   icp = (struct icmp *)(m->m_data + sizeof(struct ip));

   //--  icmpstat.icps_outhist[type]++;

   icp->icmp_type = type;
   if(type == ICMP_REDIRECT)
      icp->icmp_gwaddr.s__addr = htonl(dest);
   else
   {
      icp->icmp_void = 0;

     //-- The following assignments assume an overlay with the
     //-- zeroed icmp_void field.

      if(type == ICMP_PARAMPROB)
      {
         icp->icmp_pptr = code;
         code = 0;
      }
      else if (type == ICMP_UNREACH &&
               code == ICMP_UNREACH_NEEDFRAG &&
               ni   != NULL)
      {
         icp->icmp_nextmtu = htons(ni->if_mtu);
      }
   }

   icp->icmp_code = code;
   bcopy((unsigned char *)oip, (unsigned char *)&icp->icmp_ip, icmplen);

   nip = &icp->icmp_ip;
   nip->ip_len = htons((unsigned short)n->m_len); //-- At the Rx - single mbuf

  //-- Now, copy old ip header (without options) in the front of the icmp message.

   nip = (struct ip *)m->m_data;

   bcopy((unsigned char *)oip, (unsigned char *)nip, sizeof(struct ip));

   m_freem(tnet, n); //-- Not needs 'n' anymore

   nip->ip_len = htons(m->m_len + sizeof(struct ip));
   nip->ip_hl  = sizeof(struct ip) >> 2;
   nip->ip_p   = IPPROTO_ICMP;
   nip->ip_tos = 0;

   m->m_len  += sizeof(struct ip);
   m->m_tlen = m->m_len;

   icmp_reflect(tnet, ni, m);
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

