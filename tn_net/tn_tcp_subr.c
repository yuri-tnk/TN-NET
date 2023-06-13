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
#include "tn_tcp.h"
#include "tn_sockets_tcp.h"
#include "tn_net_func.h"
#include "tn_in_pcb_func.h"
#include "tn_net_mem_func.h"

#include "dbg_func.h"

#if defined (__ICCARM__)   // IAR ARM
#pragma diag_suppress=Pa039
#endif

//----------------------------------------------------------------------------
// Tcp initialization
//----------------------------------------------------------------------------
void tcp_init(TN_NET * tnet)
{
   tnet->tcp_iss = 1;                /* wrong - should be random*/

//-----------------------------
   tnet->tcp_sendspace  = 1024*4; //1024*8;
   tnet->tcp_recvspace  = 1536; //1024*2;

  //-- patchable/settable parameters for tcp

   tnet->sb_max         = SB_MAX;                // patchable


   tnet->tcp_keepidle   = TCPTV_KEEP_IDLE;
   tnet->tcp_keepintvl  = TCPTV_KEEPINTVL;
   tnet->tcp_maxidle    = 2* tnet->tcp_keepintvl;   //inside tcp_slowtimo() also

   tnet->tcp_mssdflt    = TCP_MSS;
   tnet->tcp_rttdflt    = TCPTV_SRTTDFLT / PR_SLOWHZ;
   tnet->tcp_do_rfc1323 = 0; //-- No support for the RFC1323
}

//----- OpenBSD --------------------------------------------------------------
struct mbuf * tcp_template(TN_NET * tnet, struct tcpcb * tp)
{
   struct inpcb *inp = tp->t_inpcb;
   struct mbuf * m;
   struct tcphdr * th;
   struct ipovly * ipovly;

   if(tp == NULL)
      return NULL;

   m = tp->t_template;
   if(m == NULL)
   {
      m = mb_get(tnet, MB_MID1, MB_NOWAIT, FALSE); //-- Not from Tx pool
      if(m == NULL)
         return NULL;
      m->m_len = sizeof(struct ip) + sizeof (struct tcphdr);
   }

 //--- Fill IP header

   ipovly = mtod(m, struct ipovly *);

   bzero((void*)ipovly, sizeof(struct ip) + sizeof(struct tcphdr));
   ipovly->ih_pr  = IPPROTO_TCP;
   ipovly->ih_len = htons(sizeof (struct tcphdr));
   ipovly->ih_src = inp->inp_laddr;
   ipovly->ih_dst = inp->inp_faddr;

 //--- Fill TCP header

   th = (struct tcphdr *)(mtod(m, unsigned char *) +  sizeof(struct ip));

   th->th_sport = inp->inp_lport;
   th->th_dport = inp->inp_fport;
   th->th_seq = 0;
   th->th_ack = 0;
   th->th_x2  = 0;
   th->th_off = 5;  //--
   th->th_flags = 0;
   th->th_win = 0;
   th->th_urp = 0;

   return (m);
}

//--- OpenBSD ----------------------------------------------------------------
void tcp_respond(TN_NET * tnet,
                 struct mbuf * mb,
                 struct tcpcb *tp,
                 unsigned char * t_template,
                 struct tcphdr * th0,
                 tcp_seq ack,
                 tcp_seq seq,
                 int flags)
{
   int tlen;
   int win;
   struct mbuf * m;
   struct tcphdr *th;
   struct ip *ip;
   struct ipovly *ih;

   m = NULL;
   if(tp)
   {
      struct socket_tcp * so = (struct socket_tcp *)tp->t_inpcb->inp_socket;
      win = sbspace(&so->so_rcv);
   }
   else
      win = 0;

   m = mb_get(tnet, MB_MID1, MB_NOWAIT, TRUE); //-- From Tx pool
   if(m == NULL)
   {
      if(mb)
         m_freem(tnet, mb);
      return;
   }
   tlen = 0;

#define xchg(a,b,type) do { type t; t=a; a=b; b=t; } while (0)

   ip = mtod(m, struct ip *);
   th = (struct tcphdr *)(ip + 1);
   tlen = sizeof(*ip) + sizeof(*th);
   if(th0)
   {
      bcopy(t_template, ip, sizeof(*ip));
      bcopy(th0, th, sizeof(*th));
      xchg(ip->ip_dst.s__addr, ip->ip_src.s__addr, unsigned int);
   }
   else //-- tcp_timer(), t_template = tp->t_template
   {
      bcopy(t_template, ip, tlen);
   }
//--- !!!!! -------------------
   if(mb)
      m_freem(tnet, mb);
//-----------------------------
   if(th0)
      xchg(th->th_dport, th->th_sport, unsigned short);
   else
      flags = TH_ACK;
#undef xchg

   m->m_len    = tlen;
   m->m_tlen   = tlen;  // m->m_pkthdr.len   = tlen;

   th->th_seq   = htonl(seq);
   th->th_ack   = htonl(ack);
   th->th_x2    = 0;
   th->th_off   = sizeof (struct tcphdr) >> 2;
   th->th_flags = flags;

   if(win > TCP_MAXWIN)
      win = TCP_MAXWIN;

   th->th_win = htons((u_int16_t)win);
   th->th_urp = 0;

 //------
   ih = (struct ipovly *)ip;
   bzero(&ih->ih_x1, sizeof(ih->ih_x1));
   ih->ih_len = htons((u_short)tlen - sizeof(struct ip));

   //-- There's no point deferring to hardware checksum processing
   //-- here, as we only send a minimal TCP packet whose checksum
   //-- we need to compute in any case.

   th->th_sum = 0;
   th->th_sum = in4_cksum(m,
                          IPPROTO_TCP,
                          sizeof(struct ip),
                          m->m_tlen - sizeof(struct ip));


   ip->ip_len = htons(tlen);
   ip->ip_ttl = MAXTTL;

   ip_output(tnet, tp ? tp->ni : NULL, m);
}

//----------------------------------------------------------------------------
// Create a new TCP control block, making an
// empty reassembly queue and hooking it to the argument
// protocol control block.
//----------------------------------------------------------------------------
struct tcpcb * tcp_newtcpcb(TN_NET * tnet, struct inpcb * inp)
{
   struct tcpcb * tp;

   if(inp == NULL)
      return NULL;

   tp = (struct tcpcb*) tn_net_alloc_mid1(tnet, MB_NOWAIT);
   if(tp == NULL)
      return NULL;

   bzero((char *) tp, sizeof(struct tcpcb));
   TAILQ_INIT(&tp->t_segq);
   tp->t_maxseg = tnet->tcp_mssdflt;

   tp->t_flags = 0;  //--  no rfc1323
   tp->t_inpcb = inp;

   //-- Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
   //-- rtt estimate.  Set rttvar so that srtt + 2 * rttvar gives
   //-- reasonable initial retransmit time.

   tp->t_srtt   = TCPTV_SRTTBASE;
   tp->t_rttvar = tnet->tcp_rttdflt * PR_SLOWHZ << 2;
   tp->t_rttmin = TCPTV_MIN;

   TCPT_RANGESET(tp->t_rxtcur,
                   ((TCPTV_SRTTBASE >> 2) + (TCPTV_SRTTDFLT << 2)) >> 1,
                      TCPTV_MIN, TCPTV_REXMTMAX);

   tp->snd_cwnd     = TCP_MAXWIN;
   tp->snd_ssthresh = TCP_MAXWIN;

   inp->inp_ppcb = (unsigned char *)tp;

   return tp;
}

//----------------------------------------------------------------------------
// Drop a TCP connection, reporting
// the specified error.  If connection is synchronized,
// then send a RST to peer.
//----------------------------------------------------------------------------
struct tcpcb * tcp_drop(TN_NET * tnet, struct tcpcb * tp, int errno)
{
   struct socket_tcp * so;

   if(tp == NULL)
      return NULL;

   so = (struct socket_tcp *)tp->t_inpcb->inp_socket;
   if(TCPS_HAVERCVDSYN(tp->t_state))
   {
      tp->t_state = TCPS_CLOSED;
      tcp_output(tnet, tp);
      // tcpstat.tcps_drops++;
   }
   else
   {
      // tcpstat.tcps_conndrops++;
   }

   if(errno == ETIMEDOUT && tp->t_softerror)
      errno = tp->t_softerror;

   so->so_error = errno;

   return tcp_close(tnet, tp);
}

//----------------------------------------------------------------------------
//  Close a TCP control block:
//         discard all space held by the tcp
//         discard internet protocol block
//         wake up any sleepers
//----------------------------------------------------------------------------
struct tcpcb * tcp_close(TN_NET * tnet, struct tcpcb * tp)
{
   struct inpcb *inp;
   struct socket_tcp *so;

   if(tp == NULL)
      return NULL;

   inp = tp->t_inpcb;
   so  = (struct socket_tcp *)inp->inp_socket;

   // free the reassembly queue, if any

   tcp_freeq(tnet, tp);

   tcp_canceltimers(tp);

   if(tp->t_template)
   {
      m_free(tnet, tp->t_template);
      tp->t_template = NULL;
   }

   tn_net_free_mid1(tnet, (void*)tp);

   inp->inp_ppcb = NULL;
   soisdisconnected(tnet, so);

   in_pcbdetach(tnet, inp);

 //  tcpstat.tcps_closed++;

   return NULL;
}

//--- OpenBSD ----------------------------------------------------------------
int tcp_freeq(TN_NET * tnet, struct tcpcb * tp)
{
   struct tcpqent *qe;
   int rv = 0;

   if(tp == NULL)
      return rv;

   while((qe = TAILQ_FIRST(&tp->t_segq)) != NULL)
   {
      TAILQ_REMOVE(&tp->t_segq, qe, tcpqe_q);

      if(m_freem(tnet, qe->tcpqe_m) == INV_MEM_VAL)
         tn_net_panic(INV_MEM_VAL_31);

      tn_net_free_min(tnet, (void *)qe);
      rv = 1;
   }
   return rv;
}

//----------------------------------------------------------------------------
// Attach TCP protocol to socket, allocating
// internet protocol control block, tcp control block,
// bufer space, and entering LISTEN state if to accept connections.
//----------------------------------------------------------------------------
int tcp_attach(TN_NET * tnet, struct socket_tcp *so)
{
   struct tcpcb *tp;
   struct inpcb *inp;
   int error;

   if(so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0)
   {
      error = soreserve(tnet, so, tnet->tcp_sendspace, tnet->tcp_recvspace);
      if(error)
         return error;
   }
   error = in_pcballoc(tnet, (struct socket *)so, &tnet->tcb);
   if(error)
      return error;

   inp = sotoinpcb(so);
   tp = tcp_newtcpcb(tnet, inp);

   if(tp == NULL)
   {
      int nofd = so->so_state & SS_NOFDREF;        // XXX

      so->so_state &= ~SS_NOFDREF;        // don't free the socket yet
      in_pcbdetach(tnet, inp);
      so->so_state |= nofd;
      return ENOBUFS;
   }
   tp->t_state = TCPS_CLOSED;
   return 0; //-- O.K
}

//----------------------------------------------------------------------------
// Initiate (or continue) disconnect.
// If embryonic state, just send reset (once).
// If in ``let data drain'' option and linger null, just drop.
// Otherwise (hard), mark socket disconnecting and drop
// current input data; switch states based on user close, and
// send segment to peer (with FIN).
//----------------------------------------------------------------------------
struct tcpcb * tcp_disconnect(TN_NET * tnet, struct tcpcb *tp)
{
   struct socket_tcp * so = (struct socket_tcp *)tp->t_inpcb->inp_socket;

   if(tp->t_state < TCPS_ESTABLISHED)
      tp = tcp_close(tnet, tp);
   else if((so->so_options & SO_LINGER) && so->so_linger == 0)
      tp = tcp_drop(tnet, tp, 0);
   else
   {
      soisdisconnecting(tnet, so);

      sbflush(tnet, &so->so_rcv);

      tp = tcp_usrclosed(tnet, tp);
      if(tp)
         tcp_output(tnet, tp);
   }

   return tp;
}

//----------------------------------------------------------------------------
// User issued close, and wish to trail through shutdown states:
// if never received SYN, just forget it.  If got a SYN from peer,
// but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
// If already got a FIN from peer, then almost done; go to LAST_ACK
// state.  In all other cases, have already sent FIN to peer (e.g.
// after PRU_SHUTDOWN), and just have to play tedious game waiting
// for peer to send FIN or not respond to keep-alives, etc.
// We can let the user exit from the close as soon as the FIN is acked.
//----------------------------------------------------------------------------
struct tcpcb * tcp_usrclosed(TN_NET * tnet, struct tcpcb *tp)
{
   switch (tp->t_state)
   {
      case TCPS_CLOSED:
      case TCPS_LISTEN:
      case TCPS_SYN_SENT:

         tp->t_state = TCPS_CLOSED;
         tp = tcp_close(tnet, tp);
         break;

      case TCPS_SYN_RECEIVED:
      case TCPS_ESTABLISHED:

         tp->t_state = TCPS_FIN_WAIT_1;
         break;

      case TCPS_CLOSE_WAIT:

         tp->t_state = TCPS_LAST_ACK;
         break;
   }

   if(tp && tp->t_state >= TCPS_FIN_WAIT_2)
      soisdisconnected(tnet, (struct socket_tcp*) tp->t_inpcb->inp_socket);

   return tp;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void tcp_timer_func(TN_NETINFO * tneti, int cnt)
{
   TN_NET * tnet = NULL;

   tnet = tneti->tnet;
   if(tnet == NULL)
       return;

   if(cnt%2 == 0)        //-- Fast TCP timer - 200 ms
      tcp_fasttimo(tnet);
   else if(cnt%5 == 0)   //-- Slow TCP timer  - 500 ms
      tcp_slowtimo(tnet);

}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------





