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

#include <tnkernel/tn.h>

#include "tn_net_cfg.h"
#include "tn_net_types.h"
#include "tn_net_pcb.h"
#include "tn_net_mem.h"
#include "tn_ip.h"
#include "tn_net.h"
#include "errno.h"
#include "tn_mbuf.h"
#include "tn_netif.h"
#include "bsd_socket.h"
#include "tn_socket.h"
#include "tn_net_utils.h"
#include "ip_icmp.h"
#include "tn_udp.h"
#include "tn_net_func.h"
#include "tn_in_pcb_func.h"

//----------------------------------------------------------------------------
void udp_init(TN_NET * tnet)
{
   tnet->udb.inp_next = tnet->udb.inp_prev = &tnet->udb;
   tnet->udp_last_inpcb = &tnet->udb;
}

//----------------------------------------------------------------------------
void udp_input(TN_NET * tnet, TN_NETIF * ni, struct mbuf * mb)
{
   struct ip * ip;
   struct udphdr * uh;
   struct inpcb * inp;
   struct socket * so;
   struct udp_socket * so_udp;
   int len;
   int iphlen;
   volatile int rc;
   int sm;
   struct ip save_ip;  //-- 20 bytes

  // udpstat.udps_ipackets++;

   //-- Strip IP options, if any; should skip this,
   //-- make available to user, and use on returned packets,
   //-- but we don't yet have a way to check the checksum
   //-- with options still present.

   ip = (struct ip*)mb->m_data;
   iphlen = ip->ip_hl << 2;

   if(iphlen > sizeof (struct ip))
   {
      ip_stripoptions(mb);
      iphlen = sizeof(struct ip);
   }

  // In TN NET, IP and UDP header are always in the first mbuf both

   uh = (struct udphdr *)((unsigned char *)ip + iphlen);

   // Make mbuf data length reflect UDP length.
   // If not enough data to reflect UDP length, drop.

   len  = ntohs((u_short)uh->uh_ulen);

   if(len > ntohs(ip->ip_len))
   {
        // udpstat.udps_badlen++;
       m_freem(tnet, mb);
       return;
   }

   // Save a copy of the IP header in case we want restore it
   // for sending an ICMP error message in response.

   bcopy(ip, &save_ip, sizeof(struct ip));

//---- OB

   if(uh->uh_sum) //-- Do UDP checksum calc
   {
      uh->uh_sum = in4_cksum(mb, IPPROTO_UDP, iphlen, len);
      if(uh->uh_sum != 0)
      {
        // udpstat.udps_badsum++;
         m_freem(tnet, mb);
         return;
      }
   }
   else
   {
 //     udpstat.udps_nosum++;
   }

   //-- Locate pcb for datagram.

   sm = splnet(tnet);   //-- v.0.8.7

   inp = in_pcblookup(tnet,
                       &tnet->udb,
                       ip->ip_src.s__addr,
                       uh->uh_sport,
                       ip->ip_dst.s__addr,
                       uh->uh_dport,
                       INPLOOKUP_WILDCARD);
   if(inp == NULL)
   {
      splx(tnet, sm);

     //   udpstat.udps_noport++;
      if(mb->m_flags & (M_BCAST | M_MCAST))
      {
         m_freem(tnet, mb);
     //    udpstat.udps_noportbcast++;
         return;
      }

      //-- Restore original IP header for the ICMP error message
      bcopy(&save_ip, ip, sizeof(struct ip));

      ip->ip_len =  htons(ntohs(ip->ip_len) + iphlen);

      icmp_error(tnet, ni, mb, ICMP_UNREACH, ICMP_UNREACH_PORT, 0);

      return;
   }

   splx(tnet, sm);

   //-- Add Rxed mbuf to the UDP rx queue

   so = inp->inp_socket;
   if(so == NULL)
   {
      m_freem(tnet, mb);
      return;
   }

   so_udp = (struct udp_socket *)so->so_tdata;
   if(so_udp == NULL)
   {
      m_freem(tnet, mb);
      return;
   }

   rc = tn_queue_send_polling(&so_udp->queueRx, (void*)mb);
   if(rc != TERR_NO_ERR) //-- Any err
   {
     // udpstat.udps_fullsock++;
      m_freem(tnet, mb);
   }
}

//----------------------------------------------------------------------------
int udp_output(TN_NET * tnet,
               struct inpcb * inp,
               struct mbuf * mb,
               struct mbuf * mb_addr)
{
   struct udpiphdr * ui;
   int len;
   int rc;
   struct in__addr laddr;
   TN_MBUF * m;
   int sm = 1;

   laddr.s__addr = 0;
   len = mb->m_tlen;

   //--  Compute the packet length of the IP header, and
   //--  punt if the length looks bogus.

   if((len + sizeof(struct udpiphdr)) > IP_MAXPACKET)
   {
      m_freem(tnet, mb);
      if(mb_addr)
         m_freem(tnet, mb_addr);
      return EMSGSIZE;
   }

   if(mb_addr)
   {
      laddr.s__addr = inp->inp_laddr.s__addr;    //-- save local addr
      if(inp->inp_faddr.s__addr != _INADDR_ANY)  //-- socket must be disconnected
      {
         m_freem(tnet, mb);
         m_freem(tnet, mb_addr);
         return EISCONN;
      }

    //-- Must block input while temporarily connected.

      sm = splnet(tnet);

      rc = in_pcbconnect(tnet, inp, mb_addr);
      if(rc)
      {
         splx(tnet, sm);
         m_freem(tnet, mb);
         m_freem(tnet, mb_addr);
         return rc;
      }
   }
   else
   {
      if(inp->inp_faddr.s__addr == _INADDR_ANY)  //-- socket must be connected
      {
         m_freem(tnet, mb);
         return ENOTCONN;
      }
   }

   //-- Get a mbuf for UDP and IP headers.
   //-- In TN NET, header len = 20 (IP header len) + 8 (UDP header len)

   m = mb_get(tnet, MB_MIN, MB_NOWAIT, TRUE); //-- Use Drv Pool
   if(m == NULL)
   {
      if(mb_addr)
      {
         inp->inp_laddr.s__addr = laddr.s__addr;  //--- restore local addr
         inp->inp_faddr.s__addr = _INADDR_ANY;   //-- in_pcbdisconnect();
         inp->inp_fport = 0;

         splx(tnet, sm);
         m_freem(tnet, mb_addr);
      }
      m_freem(tnet, mb);
      return ENOBUFS;
   }

   m->m_next = mb;
   m->m_len  = sizeof(struct udpiphdr);
   m->m_tlen = m->m_len + len;

   m->m_flags |= M_PKTHDR;
   mb->m_flags &= ~M_PKTHDR;

   //--  Fill in mbuf with extended UDP header
   //--  and addresses and length put into network format.

   ui = (struct udpiphdr *)m->m_data;
   s_memset(&ui->ui_x1, 0, sizeof(ui->ui_x1));
   ui->ui_pr    = IPPROTO_UDP;
   ui->ui_len   = htons((u_int16_t)len + sizeof (struct udphdr));
   ui->ui_src   = inp->inp_laddr;
   ui->ui_dst   = inp->inp_faddr;
   ui->ui_sport = inp->inp_lport;
   ui->ui_dport = inp->inp_fport;
   ui->ui_ulen  = ui->ui_len;

   ui->ui_sum = 0;

#ifdef USE_UDP_CHKSUM

   //-- Compute the pseudo-header checksum

   ui->ui_sum = in4_cksum(m,
                          IPPROTO_UDP,
                          sizeof(struct ip),   //-- off; see above
                          m->m_tlen - sizeof(struct ip));
   if(ui->ui_sum == 0)
      ui->ui_sum = 0xffff;
#endif

   ((struct ip *)ui)->ip_len = htons(sizeof (struct udpiphdr) + len);
   ((struct ip *)ui)->ip_off = 0;
   ((struct ip *)ui)->ip_ttl = MAXTTL;
   ((struct ip *)ui)->ip_tos = 0;

   //-- ToDo - add statistics here  udpstat.udps_opackets++;

   rc = ip_output(tnet, NULL, m);

   if(mb_addr)
   {
      inp->inp_laddr.s__addr = laddr.s__addr; //-- Restore local address
      inp->inp_faddr.s__addr = _INADDR_ANY;   //-- in_pcbdisconnect();
      inp->inp_fport = 0;

      m_freem(tnet, mb_addr);   //-- v.0.8.7

      splx(tnet, sm);
   }

   return rc;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



