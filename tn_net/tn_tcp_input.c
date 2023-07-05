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
#include "tn_protosw.h"
#include "bsd_socket.h"
#include "tn_socket.h"
#include "tn_net_utils.h"
#include "ip_icmp.h"
#include "tn_udp.h"
#include "tn_tcp.h"
#include "tn_sockets_tcp.h"
#include "tn_net_func.h"
#include "tn_in_pcb_func.h"
#include "tn_net_mem_func.h"

#include "dbg_func.h"

//--- Local functions prototypes

int tcp_dooptions(struct tcpcb * tp,
                        u_char * cp,
                           int   cnt,
                 struct tcphdr * th,
                   struct mbuf  * m,
                         int iphlen,
            struct tcp_opt_info * oi,
                       TN_NETIF * ni);

void tcp_xmit_timer(struct tcpcb *tp, short rtt);

void tcpdropoldhalfopen(TN_NET * tnet,
                        struct tcpcb * avoidtp,
                        unsigned short port);

#if defined (__ICCARM__)   // IAR ARM
#pragma diag_suppress=Pa039
#endif


//----------------------------------------------------------------------------
//  Insert segment ti into reassembly queue of tcp with
//  control block tp.  Return TH_FIN if reassembly now includes
//  a segment with FIN.  The macro form does the common case inline
//  (segment is the next to be received on an established connection,
//  and the queue is empty), avoiding linkage into and removal
//  from the queue and repetition of various conversions.
//  Set DELACK for segments received in order, but ack immediately
//  when segments are out of order (so fast retransmit can work).
//----------------------------------------------------------------------------
int tcp_reass(TN_NET * tnet, struct tcpcb *tp, struct tcphdr *th, struct mbuf *m, int *tlen)
{
   struct tcpqent *p, *q, *nq, *tiqe;
   struct socket_tcp * so;
   int flags;
   int rc;

   so = (struct socket_tcp *)tp->t_inpcb->inp_socket;

   //-- Call with th==0 after become established to
   //-- force pre-ESTABLISHED data up to user socket.

   if(th == 0)
      goto present;

   //-- Allocate a new queue entry, before we throw away any data.
   //-- If we can't, just drop the packet.  XXX

   tiqe = (struct tcpqent *)tn_net_alloc_min(tnet, MB_NOWAIT);
   if(tiqe == NULL)
   {
         //-- Flush segment queue for this connection
      tcp_freeq(tnet, tp);
//         tcpstat.tcps_rcvmemdrop++;

      if(m_freem(tnet, m) == INV_MEM_VAL)
         tn_net_panic(INV_MEM_VAL_7);

      return 0;
   }

   //-- Find a segment which begins after this one does.

   for(p = NULL, q = TAILQ_FIRST(&tp->t_segq); q != NULL;
                                     p = q, q = TAILQ_NEXT(q, tcpqe_q))
      if(SEQ_GT(q->tcpqe_tcp->th_seq, th->th_seq))
         break;

   //-- If there is a preceding segment, it may provide some of
   //-- our data already.  If so, drop the data from the incoming
   //-- segment.  If it provides all of our data, drop us.

   if(p != NULL)
   {
      struct tcphdr * phdr = p->tcpqe_tcp;
      int i;

      //-- conversion to int (in i) handles seq wraparound
      i = phdr->th_seq + phdr->th_reseqlen - th->th_seq;
      if(i > 0)
      {
         if(i >= *tlen)
         {
          //  tcpstat.tcps_rcvduppack++;
          //  tcpstat.tcps_rcvdupbyte += *tlen;
            if(m_freem(tnet, m) == INV_MEM_VAL)
               tn_net_panic(INV_MEM_VAL_8);

            rc = tn_net_free_min(tnet, (void *)tiqe);
            if(rc != 0)
               tn_net_panic(INV_MEM_VAL_8);

            return (0);
         }
         m_adj(m, i);
         *tlen -= i;
         th->th_seq += i;
      }
   }
//   tcpstat.tcps_rcvoopack++;
//   tcpstat.tcps_rcvoobyte += *tlen;


   //-- While we overlap succeeding segments trim them or,
   //-- if they are completely covered, dequeue them.

   for(; q != NULL; q = nq)
   {
      struct tcphdr *qhdr = q->tcpqe_tcp;
      int i = (th->th_seq + *tlen) - qhdr->th_seq;

      if(i <= 0)
         break;
      if(i < qhdr->th_reseqlen)
      {
         qhdr->th_seq += i;
         qhdr->th_reseqlen -= i;
         m_adj(q->tcpqe_m, i);
         break;
      }
      nq = TAILQ_NEXT(q, tcpqe_q);
      if(m_freem(tnet, q->tcpqe_m) == INV_MEM_VAL)
         tn_net_panic(INV_MEM_VAL_9);

      TAILQ_REMOVE(&tp->t_segq, q, tcpqe_q);
      rc = tn_net_free_min(tnet, (void *)q);
      if(rc != 0)
         tn_net_panic(INV_MEM_VAL_9);
   }

   //-- Insert the new segment queue entry into place.
   tiqe->tcpqe_m = m;
   th->th_reseqlen = *tlen;
   tiqe->tcpqe_tcp = th;
   if(p == NULL)
   {
      TAILQ_INSERT_HEAD(&tp->t_segq, tiqe, tcpqe_q);
   }
   else
   {
      TAILQ_INSERT_AFTER(&tp->t_segq, p, tiqe, tcpqe_q);
   }

present:

   //-- Present data to user, advancing rcv_nxt through
   //-- completed sequence space.

   if(TCPS_HAVEESTABLISHED(tp->t_state) == 0)
      return (0);
   q = TAILQ_FIRST(&tp->t_segq);
   if(q == NULL || q->tcpqe_tcp->th_seq != tp->rcv_nxt)
      return (0);
   if(tp->t_state == TCPS_SYN_RECEIVED && q->tcpqe_tcp->th_reseqlen)
      return (0);

   do
   {
      tp->rcv_nxt += q->tcpqe_tcp->th_reseqlen;
      flags = q->tcpqe_tcp->th_flags & TH_FIN;

      nq = TAILQ_NEXT(q, tcpqe_q);
      TAILQ_REMOVE(&tp->t_segq, q, tcpqe_q);

      if(so->so_state & SS_CANTRCVMORE)
      {
         if(m_freem(tnet, q->tcpqe_m) == INV_MEM_VAL)
            tn_net_panic(INV_MEM_VAL_10);
      }
      else
      {
         sbappend(tnet, &so->so_rcv, q->tcpqe_m);
      }
      rc = tn_net_free_min(tnet, (void *)q);
      if(rc != 0)
         tn_net_panic(INV_MEM_VAL_10);

      q = nq;
   }while(q != NULL && q->tcpqe_tcp->th_seq == tp->rcv_nxt);

   sorwakeup(so);

   return flags;
}

//----------------------------------------------------------------------------
void tcp_input(TN_NET * tnet, TN_NETIF * ni, struct mbuf *m)
{
   struct ip * ip;
   struct tcphdr * th;
   struct inpcb * inp = NULL;
   struct tcpcb * tp = 0;
   struct socket_tcp * so = NULL;
   u_int8_t * optp = NULL;
   int optlen = 0;
   int tlen, off;
   int tiflags;
   int todrop, acked, ourfinisacked, needoutput = 0;
   int hdroptlen = 0;
   int iss = 0;
   int iphlen;
   unsigned long tiwin;
   struct tcp_opt_info opti;
   int dropsocket = 0;

   opti.ts_present = 0;
   opti.maxseg = 0;

  // RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN
  // See below for AF specific multicast.

   if(m->m_flags & (M_BCAST|M_MCAST))
      goto drop;

   if(ni == NULL)  //-- YVT
      goto drop;
//---------------

   ip   = (struct ip *)m->m_data;
   iphlen = ip->ip_hl << 2;

   th   = (struct tcphdr *)(((unsigned char *)m->m_data) + iphlen);
   tlen = m->m_tlen - iphlen;         //-- TCP header + data

   if(IN_MULTICAST(ntohl(ip->ip_dst.s__addr)) || in_broadcast(ip->ip_dst, ni))
      goto drop;

   if(in4_cksum(m, IPPROTO_TCP, iphlen, tlen))
   {
     // tcpstat.tcps_rcvbadsum++;
      goto drop;
   }

   //-- Check that TCP offset makes sense,
   //-- pull out TCP options and adjust length.                XXX

   off = th->th_off << 2;
   if(off < sizeof(struct tcphdr) || off > tlen)
   {
      //tcpstat.tcps_rcvbadoff++;
      goto drop;
   }

   tlen -= off;
   if(off > sizeof(struct tcphdr))
   {
      optlen = off - sizeof(struct tcphdr);
      optp = (u_int8_t *)(th + 1);
   }
   tiflags = th->th_flags;

  //-- Convert TCP protocol specific fields to host format.

   NTOHL(th->th_seq);
   NTOHL(th->th_ack);
   NTOHS(th->th_win);
   NTOHS(th->th_urp);

  //-- Locate pcb for segment.

findpcb:

   inp = ni->tcp_last_inpcb; //-- YVY -  tcp_last_inpcb - per interface

   if(inp != NULL)
   {
      if(inp->inp_lport != th->th_dport ||
          inp->inp_fport != th->th_sport ||
             inp->inp_faddr.s__addr != ip->ip_src.s__addr ||
               inp->inp_laddr.s__addr != ip->ip_dst.s__addr)
      {
         inp = NULL; //-- searching request
      }
   }

   if(inp == NULL) //-- Need searching
   {
      inp = in_pcblookup(tnet,
                         &tnet->tcb,
                          ip->ip_src.s__addr,
                          th->th_sport,
                          ip->ip_dst.s__addr,
                          th->th_dport,
                          INPLOOKUP_WILDCARD);
      if(inp)
         ni->tcp_last_inpcb = inp; //-- YVT - per inteface
   }

   //-- If the state is CLOSED (i.e., TCB does not exist) then
   //-- all data in the incoming segment is discarded.
   //-- If the TCB exists but is in CLOSED state, it is embryonic,
   //-- but should either do a listen or a connect soon.

   if(inp == NULL)
      goto dropwithreset;
   tp = intotcpcb(inp);
   if(tp == NULL)
      goto dropwithreset;
   if(tp->t_state == TCPS_CLOSED)
      goto drop;

   tiwin = th->th_win;

   so = (struct socket_tcp *)inp->inp_socket;

   if(so->so_options & SO_ACCEPTCONN)
   {
      if((tiflags & (TH_RST|TH_ACK|TH_SYN)) != TH_SYN)
      {
         //-- 'dropwithreset' makes sure we don't
         //-- send a reset in response to a RST.

         if(tiflags & TH_ACK)
         {
            goto dropwithreset;
         }
         goto drop;
      }

//--- For the systems with a low memory resources --------

      if(tn_tcp_check_avaliable_mem(tnet) != 0)
      {
        //drops.syn++;
        goto drop;
      }

//--------------------------------------------------------
      so = sonewconn(tnet, so);
      if(so == NULL)
#ifdef TN_TCP_RESET_ON_SONEWCONN_FAIL      
        goto dropwithreset;
#else        
        goto drop;
#endif        
      //-- This is ugly, but ....

      //-- Mark socket as temporary until we're
      //-- committed to keeping it.  The code at
      //-- ``drop'' and ``dropwithreset'' check the
      //-- flag dropsocket to see if the temporary
      //-- socket created here should be discarded.
      //-- We mark the socket as discardable until
      //-- we're committed to it below in TCPS_LISTEN.

      dropsocket++;

      inp = (struct inpcb *)so->so_pcb;
      inp->inp_laddr = ip->ip_dst;
      inp->inp_lport = th->th_dport;

      tp = intotcpcb(inp);
      tp->t_state = TCPS_LISTEN;
   }

   //-- Segment received on connection.
   //-- Reset idle time and keep-alive timer.

   tp->t_idle = 0;
   tp->t_timer[TCPT_KEEP] = tnet->tcp_keepidle;

   //-- Process options if not in LISTEN state,
   //-- else do it below (after getting remote address).

   if(optp && tp->t_state != TCPS_LISTEN)
       if(tcp_dooptions(tp, optp, optlen, th, m, iphlen, &opti, ni)) //-- Open BSD
          goto drop;

   //-- Compute mbuf offset to TCP data segment.

   hdroptlen = iphlen + off;

   //-- Calculate amount of space in receive window,
   //-- and then do TCP input processing.
   //-- Receive window is amount of space in rcv queue,
   //-- but not less than advertised window.

   {
      int win;

      win = sbspace(&so->so_rcv);
      if(win < 0)
         win = 0;
      tp->rcv_wnd = _max(win, (int)(tp->rcv_adv - tp->rcv_nxt));
   }

   switch(tp->t_state)
   {
      //-- If the state is LISTEN then ignore segment if it contains an RST.
      //-- If the segment contains an ACK then it is bad and send a RST.
      //-- If it does not contain a SYN then it is not interesting; drop it.
      //-- Don't bother responding if the destination was a broadcast.
      //-- Otherwise initialize tp->rcv_nxt, and tp->irs, select an initial
      //-- tp->iss, and send a segment:
      //--     <SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
      //-- Also initialize tp->snd_nxt to tp->iss+1 and tp->snd_una to tp->iss.
      //-- Fill in remote peer address fields if not previously specified.
      //-- Enter SYN_RECEIVED state, and process any other fields of this
      //-- segment in this state.

      case TCPS_LISTEN:
      {
         if(tiflags & TH_RST)
            goto drop;
         if(tiflags & TH_ACK)
            goto dropwithreset;
         if((tiflags & TH_SYN) == 0)
            goto drop;

         //--  RFC1122 4.2.3.10, p. 104: discard bcast/mcast SYN
         //--  in_broadcast() should never return true on a received
         //--  packet with M_BCAST not set.

         if(m->m_flags & (M_BCAST|M_MCAST) ||
                                    IN_MULTICAST(ntohl(ip->ip_dst.s__addr)))
            goto drop;

       //---- Digest of the in_pcbconnect() for tcp_input()

         inp->inp_faddr = ip->ip_src;
         inp->inp_fport = th->th_sport;
         inp->inp_laddr = ni->ip_addr;

         tp->ni = ni;

       //-----------------------------------------------------

         tp->t_template = tcp_template(tnet, tp);
         if(tp->t_template == NULL)
         {
            tp = tcp_drop(tnet, tp, ENOBUFS);
            dropsocket = 0;                // socket is already gone
            goto drop;
         }
         if(optp)
            tcp_dooptions(tp, optp, optlen, th, m, iphlen, &opti, ni); //-- OpenBSD

         if(iss)
            tp->iss = iss;
         else
            tp->iss = tnet->tcp_iss;

         tnet->tcp_iss += TCP_ISSINCR/2;
         tp->irs = th->th_seq;
         tcp_sendseqinit(tp);
         tcp_rcvseqinit(tp);

         tp->t_flags |= TF_ACKNOW;
         tp->t_state = TCPS_SYN_RECEIVED;
         tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
         dropsocket = 0;                // committed to socket
        // tcpstat.tcps_accepts++;

         goto trimthenstep6;
      }

   //-- If the state is SYN_SENT:
   //--        if seg contains an ACK, but not for our SYN, drop the input.
   //--        if seg contains a RST, then drop the connection.
   //--        if seg does not contain SYN, then drop it.
   //-- Otherwise this is an acceptable SYN segment
   //--        initialize tp->rcv_nxt and tp->irs
   //--        if seg contains ack then advance tp->snd_una
   //--        if SYN has been acked change to ESTABLISHED else SYN_RCVD state
   //--        arrange for segment to be acked (eventually)
   //--        continue processing rest of data/controls, beginning with URG

      case TCPS_SYN_SENT:

         if((tiflags & TH_ACK) &&
             (SEQ_LEQ(th->th_ack, tp->iss) ||
                 SEQ_GT(th->th_ack, tp->snd_max)))
            goto dropwithreset;

         if(tiflags & TH_RST)
         {
            if(tiflags & TH_ACK)
               tp = tcp_drop(tnet, tp, ECONNREFUSED);
            goto drop;
         }
         if((tiflags & TH_SYN) == 0)
            goto drop;
         if(tiflags & TH_ACK)
         {
            tp->snd_una = th->th_ack;
            if(SEQ_LT(tp->snd_nxt, tp->snd_una))
               tp->snd_nxt = tp->snd_una;
         }

         tp->t_timer[TCPT_REXMT] = 0;
         tp->irs =  th->th_seq;
         tcp_rcvseqinit(tp);

         tp->t_flags |= TF_ACKNOW;

         //-- ToDo - check IP iface own addr was changed (for ex, PPP can change it)

         if(tiflags & TH_ACK && SEQ_GT(tp->snd_una, tp->iss))
         {
            //tcpstat.tcps_connects++;

            soisconnected(tnet, so);

            tp->t_state = TCPS_ESTABLISHED;
            tcp_reass(tnet, tp, NULL, NULL, &tlen);

            //-- if we didn't have to retransmit the SYN,
            //-- use its rtt as our initial srtt & rtt var.

            if(tp->t_rtt)
               tcp_xmit_timer(tp, tp->t_rtt);
         }
         else
            tp->t_state = TCPS_SYN_RECEIVED;

trimthenstep6:

            //-- Advance ti->ti_seq to correspond to first data byte.
            //-- If data, trim to stay within window,
            //-- dropping FIN if necessary.

         th->th_seq++;
         if(tlen > (int)tp->rcv_wnd)
         {
            todrop = tlen - tp->rcv_wnd;
            m_adj(m, -todrop);
            tlen = tp->rcv_wnd;
            tiflags &= ~TH_FIN;
            //tcpstat.tcps_rcvpackafterwin++;
            //tcpstat.tcps_rcvbyteafterwin += todrop;
         }
         tp->snd_wl1 = th->th_seq - 1;
         tp->rcv_up = th->th_seq;
         goto step6;
   }

   //-- States other than LISTEN or SYN_SENT.

   todrop = tp->rcv_nxt - th->th_seq;
   if(todrop > 0)
   {
      if(tiflags & TH_SYN)
      {
         tiflags &= ~TH_SYN;
         th->th_seq++;
         if(th->th_urp > 1)
            th->th_urp--;
         else
            tiflags &= ~TH_URG;
         todrop--;
      }

      //-- OpenBSD

      if(todrop > tlen || (todrop == tlen && (tiflags & TH_FIN) == 0))
      {

         //-- Any valid Fin must be to the left of the window.
         //-- At this point the FIN must be a duplicate or
         //-- out of sequence, so drop it.

         tiflags &= ~TH_FIN;

         //-- Send an ACK to resynchronize and drop any data.
         //-- But keep on processing for RST or ACK.

         tp->t_flags |= TF_ACKNOW;
         todrop = tlen;
      }
      //tcpstat.tcps_rcvdupbyte += todrop;
      //tcpstat.tcps_rcvduppack++;
      if(todrop)
      {
         hdroptlen  += todrop;      //-- drop from head afterwards
         th->th_seq += todrop;
         tlen       -= todrop;
      }

      if(th->th_urp > todrop)
         th->th_urp -= todrop;
      else
      {
         tiflags &= ~TH_URG;
         th->th_urp = 0;
      }
   }

   //-- If new data are received on a connection after the
   //-- user processes are gone, then RST the other end.

   if((so->so_state & SS_NOFDREF) && tp->t_state > TCPS_CLOSE_WAIT && tlen)
   {
      tp = tcp_close(tnet, tp);
      //tcpstat.tcps_rcvafterclose++;
      goto dropwithreset;
   }

   //-- If segment ends after window, drop trailing data
   //-- (and PUSH and FIN); if nothing left, just ACK.

   todrop = (th->th_seq + tlen) - (tp->rcv_nxt + tp->rcv_wnd);
   if(todrop > 0)
   {
      // tcpstat.tcps_rcvpackafterwin++;
      if(todrop >= tlen)     //-- ?????
      {
         //tcpstat.tcps_rcvbyteafterwin += ti->ti_len;

         //-- If a new connection request is received
         //-- while in TIME_WAIT, drop the old connection
         //-- and start over if the sequence numbers
         //-- are above the previous ones.

         if(tiflags & TH_SYN && tp->t_state == TCPS_TIME_WAIT &&
                                              SEQ_GT(th->th_seq, tp->rcv_nxt))
         {
            iss = tp->rcv_nxt + TCP_ISSINCR;
            tp  = tcp_close(tnet, tp);
            goto findpcb;
         }

         //-- If window is closed can only take segments at
         //-- window edge, and have to drop data and PUSH from
         //-- incoming segments.  Continue processing, but
         //-- remember to ack.  Otherwise, drop segment and ack.

         if(tp->rcv_wnd == 0 && th->th_seq == tp->rcv_nxt)
         {
            tp->t_flags |= TF_ACKNOW;
            //tcpstat.tcps_rcvwinprobe++;
         }
         else
            goto dropafterack;
      }
      else
      {
         //tcpstat.tcps_rcvbyteafterwin += todrop;
      }

      m_adj(m, -todrop);  //  m->m_len -= todrop;
      tlen -= todrop;

      tiflags &= ~(TH_PUSH|TH_FIN);
   }

   //-- If the RST bit is set examine the state:
   //--    SYN_RECEIVED STATE:
   //--        If passive open, return to LISTEN state.
   //--        If active open, inform user that connection was refused.
   //--    ESTABLISHED, FIN_WAIT_1, FIN_WAIT2, CLOSE_WAIT STATES:
   //--        Inform user that connection was reset, and close tcb.
   //--    CLOSING, LAST_ACK, TIME_WAIT STATES
   //--        Close the tcb.

   if(tiflags & TH_RST)
   {
      switch(tp->t_state)
      {
         case TCPS_SYN_RECEIVED:

            so->so_error = ECONNREFUSED;
            goto close;

         case TCPS_ESTABLISHED:
if(th->th_seq != tp->rcv_nxt)
{
   tiflags &= ~TH_RST;
   goto dropafterack;
}
     //--- No break;
         case TCPS_FIN_WAIT_1:
         case TCPS_FIN_WAIT_2:
         case TCPS_CLOSE_WAIT:

            so->so_error = ECONNRESET;
close:
            tp->t_state = TCPS_CLOSED;
            //tcpstat.tcps_drops++;
            tp = tcp_close(tnet, tp);
            goto drop;

         case TCPS_CLOSING:
         case TCPS_LAST_ACK:
         case TCPS_TIME_WAIT:

            tp = tcp_close(tnet, tp);
            goto drop;
      }
   }
   //-- If a SYN is in the window, then this is an
   //-- error and we send an RST and drop the connection.

   if(tiflags & TH_SYN)
   {

if(tp->t_state == TCPS_ESTABLISHED)
   goto drop;

      tp = tcp_drop(tnet, tp, ECONNRESET);
      goto dropwithreset;
   }

   //-- If the ACK bit is off we drop the segment and return.

   if((tiflags & TH_ACK) == 0)
      goto drop;

   //--  Ack processing.

   switch (tp->t_state)
   {
      //-- In SYN_RECEIVED state if the ack ACKs our SYN then enter
      //-- ESTABLISHED state and continue processing, otherwise
      //-- send an RST.

      case TCPS_SYN_RECEIVED:

         if (SEQ_GT(tp->snd_una, th->th_ack) ||
                    SEQ_GT(th->th_ack, tp->snd_max))
                        goto dropwithreset;
         //tcpstat.tcps_connects++;

         soisconnected(tnet, so);

         tp->t_state = TCPS_ESTABLISHED;
         tcp_reass(tnet, tp, NULL, NULL, &tlen);
         tp->snd_wl1 = th->th_seq - 1;

         // fall into ...

      //--  In ESTABLISHED state: drop duplicate ACKs; ACK out of range
      //--  ACKs.  If the ack is in the range
      //--         tp->snd_una < ti->ti_ack <= tp->snd_max
      //--  then advance tp->snd_una to ti->ti_ack and drop
      //--  data from the retransmission queue.  If this ACK reflects
      //--  more up to date window information we update our window information.

      case TCPS_ESTABLISHED:
      case TCPS_FIN_WAIT_1:
      case TCPS_FIN_WAIT_2:
      case TCPS_CLOSE_WAIT:
      case TCPS_CLOSING:
      case TCPS_LAST_ACK:
      case TCPS_TIME_WAIT:

         if(SEQ_LEQ(th->th_ack, tp->snd_una))
         {
            if(tlen == 0 && tiwin == tp->snd_wnd)
            {
              // tcpstat.tcps_rcvdupack++;

              //-- If we have outstanding data (other than
              //-- a window probe), this is a completely
              //-- duplicate ack (ie, window info didn't
              //-- change), the ack is the biggest we've
              //-- seen and we've seen exactly our rexmt
              //-- threshhold of them, assume a packet
              //-- has been dropped and retransmit it.
              //-- Kludge snd_nxt & the congestion
              //-- window so we send only this one
              //-- packet.

              //-- We know we're losing at the current
              //-- window size so do congestion avoidance
              //-- (set ssthresh to half the current window
              //-- and pull our congestion window back to
              //-- the new ssthresh).

              //-- Dup acks mean that packets have left the
              //-- network (they're now cached at the receiver)
              //-- so bump cwnd by the amount in the receiver
              //-- to keep a constant cwnd packets in the  network.

               if(tp->t_timer[TCPT_REXMT] == 0 || th->th_ack != tp->snd_una)
                  tp->t_dupacks = 0;
               else if (++tp->t_dupacks == 3)//tcprexmtthresh)
               {
                  tcp_seq onxt = tp->snd_nxt;
                  unsigned int win = _min(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;

                  if(win < 2)
                     win = 2;
                  tp->snd_ssthresh = win * tp->t_maxseg;
                  tp->t_timer[TCPT_REXMT] = 0;
                  tp->t_rtt    = 0;
                  tp->snd_nxt  = th->th_ack;
                  tp->snd_cwnd = tp->t_maxseg;

                  tcp_output(tnet, tp);

                  tp->snd_cwnd = tp->snd_ssthresh + tp->t_maxseg * tp->t_dupacks;
                  if(SEQ_GT(onxt, tp->snd_nxt))
                     tp->snd_nxt = onxt;
                  goto drop;
               }
               else if (tp->t_dupacks > 3)//tcprexmtthresh)
               {
                  tp->snd_cwnd += tp->t_maxseg;

                  tcp_output(tnet, tp);
                  goto drop;
               }
            }
            else
               tp->t_dupacks = 0;
            break;
         }

         //-- If the congestion window was inflated to account
         //-- for the other side's cached packets, retract it.

         if(tp->t_dupacks > 3/*YVT tcprexmtthresh*/ &&
                                              tp->snd_cwnd > tp->snd_ssthresh)
            tp->snd_cwnd = tp->snd_ssthresh;
         tp->t_dupacks = 0;
         if(SEQ_GT(th->th_ack, tp->snd_max))
         {
            //tcpstat.tcps_rcvacktoomuch++;
            goto dropafterack;
         }
         acked = th->th_ack - tp->snd_una;
         // tcpstat.tcps_rcvackpack++;
         // tcpstat.tcps_rcvackbyte += acked;

         //-- If no timestamp is present but
         //-- transmit timer is running and timed sequence
         //-- number was acked, update smoothed round trip time.
         //-- Since we now have an rtt measurement, cancel the
         //-- timer backoff (cf., Phil Karn's retransmit alg.).
         //-- Recompute the initial retransmit timer.

         if(tp->t_rtt && SEQ_GT(th->th_ack, tp->t_rtseq))
            tcp_xmit_timer(tp, tp->t_rtt);

         //-- If all outstanding data is acked, stop retransmit
         //-- timer and remember to restart (more output or persist).
         //-- If there is more data to be acked, restart retransmit
         //-- timer, using current (possibly backed-off) value.

         if(th->th_ack == tp->snd_max)
         {
            tp->t_timer[TCPT_REXMT] = 0;
            needoutput = 1;
         }
         else if(tp->t_timer[TCPT_PERSIST] == 0)
            tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

         //-- When new data is acked, open the congestion window.
         //-- If the window gives us less than ssthresh packets
         //-- in flight, open exponentially (maxseg per packet).
         //-- Otherwise open linearly: maxseg per window
         //-- (maxseg^2 / cwnd per packet), plus a constant
         //-- fraction of a packet (maxseg/8) to help larger windows
         //-- open quickly enough.

         {
            unsigned int cw   = tp->snd_cwnd;
            unsigned int incr = tp->t_maxseg;

            if(cw > tp->snd_ssthresh)
               incr = incr * incr / cw;
            tp->snd_cwnd = _min(cw + incr, TCP_MAXWIN); //-- scale does not used
         }

         if(acked > (int)so->so_snd.sb_cc)
         {
            tp->snd_wnd -= so->so_snd.sb_cc;
            sbdrop(tnet, &so->so_snd, (int)so->so_snd.sb_cc);
            ourfinisacked = 1;
         }
         else
         {
            sbdrop(tnet, &so->so_snd, acked);
            tp->snd_wnd -= acked;
            ourfinisacked = 0;
         }

         //--- Notify a socket with running s_send()

         if(tp->t_state == TCPS_ESTABLISHED && acked > 0)
            sowwakeup(so);

         tp->snd_una = th->th_ack;
         if(SEQ_LT(tp->snd_nxt, tp->snd_una))
            tp->snd_nxt = tp->snd_una;

         switch (tp->t_state)
         {
           //-- In FIN_WAIT_1 STATE in addition to the processing
           //-- for the ESTABLISHED state if our FIN is now acknowledged
           //-- then enter FIN_WAIT_2.

            case TCPS_FIN_WAIT_1:

               if(ourfinisacked)
               {
                  //-- If we can't receive any more
                  //-- data, then closing user can proceed.
                  //-- Starting the timer is contrary to the
                  //-- specification, but if we don't get a FIN
                  //-- we'll hang forever.

                  if(so->so_state & SS_CANTRCVMORE)
                  {
                     soisdisconnected(tnet, so);
#ifdef TN_TCP_SUPRESS_FW2_MAX_IDLE
                     tp->t_timer[TCPT_2MSL] = 1;
#else
                     tp->t_timer[TCPT_2MSL] = tnet->tcp_maxidle;
#endif
                  }

                  tp->t_state = TCPS_FIN_WAIT_2;
               }
               break;

            //-- In CLOSING STATE in addition to the processing for
            //-- the ESTABLISHED state if the ACK acknowledges our FIN
            //-- then enter the TIME-WAIT state, otherwise ignore the segment.

            case TCPS_CLOSING:

               if(ourfinisacked)
               {
                  tp->t_state = TCPS_TIME_WAIT;
                  tcp_canceltimers(tp);
                  tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
                  soisdisconnected(tnet, so);
               }
               break;

            //-- In LAST_ACK, we may still be waiting for data to drain
            //-- and/or to be acked, as well as for the ack of our FIN.
            //-- If our FIN is now acknowledged, delete the TCB,
            //-- enter the closed state and return.

            case TCPS_LAST_ACK:

               if(ourfinisacked)
               {
                  tp = tcp_close(tnet, tp);
                  goto drop;
               }
               break;

            //-- In TIME_WAIT state the only thing that should arrive
            //-- is a retransmission of the remote FIN.  Acknowledge
            //-- it and restart the finack timer.

            case TCPS_TIME_WAIT:

               tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
               goto dropafterack;
         }
   }

step6:

   //-- Update window information.
   //-- Don't look at window if no ACK: TAC's send garbage on first SYN.

   if((tiflags & TH_ACK) &&
       (SEQ_LT(tp->snd_wl1, th->th_seq) ||
         (tp->snd_wl1 == th->th_seq &&
            (SEQ_LT(tp->snd_wl2, th->th_ack) ||
               (tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd)))))
   {
      // keep track of pure window updates
      if(tlen == 0 && tp->snd_wl2 == th->th_ack && tiwin > tp->snd_wnd)
      {
         //tcpstat.tcps_rcvwinupd++;
      }
      tp->snd_wnd = tiwin;
      tp->snd_wl1 = th->th_seq;
      tp->snd_wl2 = th->th_ack;
      if(tp->snd_wnd > tp->max_sndwnd)
         tp->max_sndwnd = tp->snd_wnd;

      needoutput = 1;
   }

   //-- Process segments with URG.

      if(SEQ_GT(tp->rcv_nxt, tp->rcv_up))
         tp->rcv_up = tp->rcv_nxt;

//dodata:                                                        // XXX

   //-- Process the segment text, merging it into the TCP sequencing queue,
   //-- and arranging for acknowledgment of receipt if necessary.
   //-- This process logically involves adjusting tp->rcv_wnd as data
   //-- is presented to the user (this happens in tcp_usrreq.c,
   //-- case PRU_RCVD).  If a FIN has already been received on this
   //-- connection then we just ignore the text.

   if((tlen || (tiflags & TH_FIN)) && TCPS_HAVERCVDFIN(tp->t_state) == 0)
   {
      if(th->th_seq == tp->rcv_nxt && TAILQ_EMPTY(&tp->t_segq) &&
                    tp->t_state == TCPS_ESTABLISHED)
      {
         tp->t_flags |= TF_DELACK;

    //     tp->t_flags |= TF_ACKNOW;

         tp->rcv_nxt += tlen;
         tiflags = th->th_flags & TH_FIN;
         // tcpstat.tcps_rcvpack++;
         // tcpstat.tcps_rcvbyte += tlen;

         if(so->so_state & SS_CANTRCVMORE)
         {
            if(m_freem(tnet, m) == INV_MEM_VAL)
               tn_net_panic(INV_MEM_VAL_11);
         }
         else
         {
            m_adj(m, hdroptlen);

         //  m->m_data += hdroptlen;
         //  m->m_len  -= hdroptlen;

            sbappend(tnet, &so->so_rcv, m);
         }
         sorwakeup(so);
      }
      else
      {
         m_adj(m, hdroptlen);

        // m->m_data += hdroptlen;
        // m->m_len  -= hdroptlen;

         tiflags = tcp_reass(tnet, tp, th, m, &tlen);

         tp->t_flags |= TF_ACKNOW;
      }
   }
   else
   {
      if(m_freem(tnet, m) == INV_MEM_VAL)
         tn_net_panic(INV_MEM_VAL_12);

      tiflags &= ~TH_FIN;
   }


   //-- If FIN is received ACK the FIN and let the user know
   //-- that the connection is closing.

   if(tiflags & TH_FIN)
   {
      if(TCPS_HAVERCVDFIN(tp->t_state) == 0)
      {
         socantrcvmore(tnet, so);
         tp->t_flags |= TF_ACKNOW;
         tp->rcv_nxt++;
      }

      switch(tp->t_state)
      {
         //-- In SYN_RECEIVED and ESTABLISHED STATES
         //-- enter the CLOSE_WAIT state.

         case TCPS_SYN_RECEIVED:
         case TCPS_ESTABLISHED:

            tp->t_state = TCPS_CLOSE_WAIT;
            break;

        //-- If still in FIN_WAIT_1 STATE FIN has not been acked so
        //-- enter the CLOSING state.

         case TCPS_FIN_WAIT_1:

            tp->t_state = TCPS_CLOSING;
            break;

         //-- In FIN_WAIT_2 state enter the TIME_WAIT state,
         //-- starting the time-wait timer, turning off the other
         //-- standard timers.

         case TCPS_FIN_WAIT_2:

            tp->t_state = TCPS_TIME_WAIT;
            tcp_canceltimers(tp);
            tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
            soisdisconnected(tnet, so);
            break;

         //-- In TIME_WAIT state restart the 2 MSL time_wait timer.

         case TCPS_TIME_WAIT:

            tp->t_timer[TCPT_2MSL] = 2 * TCPTV_MSL;
            break;
      }
   }

   //-- Return any desired output.

   if(needoutput || (tp->t_flags & TF_ACKNOW))
   {
      tcp_output(tnet, tp);
   }

   return;

dropafterack:

   //-- Generate an ACK dropping incoming segment if it occupies
   //-- sequence space, where the ACK reflects our state.

   if(tiflags & TH_RST)
      goto drop;

   if(m_freem(tnet, m) == INV_MEM_VAL)
      tn_net_panic(INV_MEM_VAL_13);

   tp->t_flags |= TF_ACKNOW;

   tcp_output(tnet, tp);

   return;

dropwithreset:

   //-- Generate a RST, dropping incoming segment.
   //-- Make ACK acceptable to originator of segment.
   //-- Don't bother to respond if destination was broadcast/multicast.

   if((tiflags & TH_RST) || m->m_flags & (M_BCAST|M_MCAST) ||
                                  IN_MULTICAST(ntohl(ip->ip_dst.s__addr)))
      goto drop;

   if(tiflags & TH_ACK)
   {
      tcp_respond(tnet,
                    m,
                    tp,
                    mtod(m, unsigned char *),
                    th,
                     0,
                     th->th_ack,
                     TH_RST);
   }
   else
   {
      if(tiflags & TH_SYN)
         tlen++;

      tcp_respond(tnet,
                    m,
                    tp,
                    mtod(m, unsigned char *),
                    th,
                    th->th_seq + tlen,
                    NULL,
                    TH_RST | TH_ACK);
   }

   if(dropsocket)  //-- destroy temporarily created socket
      tcp_drop(tnet, tp, ECONNABORTED);  //-- PRU_ABORT

   return;

drop:

   //-- Drop space held by incoming segment and return.

   if(m_freem(tnet, m) == INV_MEM_VAL)
      tn_net_panic(INV_MEM_VAL_14);

   if(dropsocket)  // destroy temporarily created socket
      tcp_drop(tnet, tp, ECONNABORTED); //-- PRU_ABORT
}

//----------------------------------------------------------------------------
int tcp_dooptions(struct tcpcb * tp,    //
                        u_char * cp,    //
                           int   cnt,   //
                 struct tcphdr * th,    //
                   struct mbuf  * m,    // N
                         int iphlen,    // N
            struct tcp_opt_info * oi,   //
                       TN_NETIF * ni)   //
{
   u_int16_t mss = 0;
   int opt, optlen;

   for(; cp && cnt > 0; cnt -= optlen, cp += optlen)
   {
      opt = cp[0];
      if(opt == TCPOPT_EOL)
         break;

      if(opt == TCPOPT_NOP)
         optlen = 1;
      else
      {
         if(cnt < 2)
            break;
         optlen = cp[1];
         if(optlen < 2 || optlen > cnt)
            break;
      }

      switch(opt)
      {
         default:
            continue;

         case TCPOPT_MAXSEG:

            if(optlen != TCPOLEN_MAXSEG)
               continue;
            if(!(th->th_flags & TH_SYN))
               continue;
            bcopy((unsigned char *) cp + 2, (unsigned char *) &mss, sizeof(mss));
            NTOHS(mss);
            tcp_mss(tp, mss, ni);        // sets t_maxseg
            oi->maxseg = mss;
            break;
      }
   }

   return 0; //-- O.K.
}

//----------------------------------------------------------------------------
// Collect new round-trip time estimate
// and update averages and current timeout.
//----------------------------------------------------------------------------
void tcp_xmit_timer(struct tcpcb *tp, short rtt)
{
   short delta;

//   tcpstat.tcps_rttupdated++;
   if(tp->t_srtt != 0)
   {
     //-- srtt is stored as fixed point with 3 bits after the
     //-- binary point (i.e., scaled by 8).  The following magic
     //-- is equivalent to the smoothing algorithm in rfc793 with
     //-- an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
     //-- point).  Adjust rtt to origin 0.

      delta = rtt - 1 - (tp->t_srtt >> TCP_RTT_SHIFT);
      if((tp->t_srtt += delta) <= 0)
         tp->t_srtt = 1;

     //-- We accumulate a smoothed rtt variance (actually, a
     //-- smoothed mean difference), then set the retransmit
     //-- timer to smoothed rtt + 4 times the smoothed variance.
     //-- rttvar is stored as fixed point with 2 bits after the
     //-- binary point (scaled by 4).  The following is
     //-- equivalent to rfc793 smoothing with an alpha of .75
     //-- (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
     //-- rfc793's wired-in beta.

      if(delta < 0)
         delta = -delta;
      delta -= (tp->t_rttvar >> TCP_RTTVAR_SHIFT);
      if((tp->t_rttvar += delta) <= 0)
         tp->t_rttvar = 1;
   }
   else
   {
      //-- No rtt measurement yet - use the unsmoothed rtt.
      //-- Set the variance to half the rtt (so our first
      //-- retransmit happens at 3*rtt).

      tp->t_srtt = rtt << TCP_RTT_SHIFT;
      tp->t_rttvar = rtt << (TCP_RTTVAR_SHIFT - 1);
   }
   tp->t_rtt = 0;
   tp->t_rxtshift = 0;

   //-- the retransmit should happen at rtt + 4 * rttvar.
   //-- Because of the way we do the smoothing, srtt and rttvar
   //-- will each average +1/2 tick of bias.  When we compute
   //-- the retransmit timer, we want 1/2 tick of rounding and
   //-- 1 extra tick because of +-1/2 tick uncertainty in the
   //-- firing of the timer.  The bias will give us exactly the
   //-- 1.5 tick we need.  But, because the bias is
   //-- statistical, we have to test that we don't drop below
   //-- the minimum feasible timer (which is 2 ticks).

   TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
                                    tp->t_rttmin, TCPTV_REXMTMAX);

   //-- We received an ack for a packet that wasn't retransmitted;
   //-- it is probably safe to discard any error indications we've
   //-- received recently.  This isn't quite right, but close enough
   //-- for now (a route might have failed after we sent a segment,
   //-- and the return path might not be symmetrical).

   tp->t_softerror = 0;
}

//----------------------------------------------------------------------------
//  We initialize the congestion/slow start window to be a single segment
//----------------------------------------------------------------------------
int tcp_mss(struct tcpcb * tp, unsigned int offer, TN_NETIF * ni)
{
   int mss;

   if(ni)
      mss = ni->if_mtu - (sizeof(struct ip) + sizeof(struct tcphdr));
   else
      mss = TCP_MSS;

   if(offer)
      mss = _min(mss, (int)offer);
   mss = _max(mss, 32);                //-- sanity check

   if(mss < tp->t_maxseg || offer != 0)
   {
      tp->t_maxseg = mss;
   }
   tp->snd_cwnd = mss;

   return mss;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


