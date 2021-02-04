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
#include "bsd_socket.h"
#include "tn_socket.h"
#include "tn_net_utils.h"
#include "tn_protosw.h"
#include "tn_tcp.h"
#include "tn_sockets_tcp.h"
#include "tn_net_func.h"

#include "dbg_func.h"

const int tcp_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };

//----------------------------------------------------------------------------
// Fast timeout routine for processing delayed acks
//----------------------------------------------------------------------------
void tcp_fasttimo(TN_NET * tnet)
{
   struct inpcb *inp;
   struct tcpcb *tp;
   int sm;

   sm = splnet(tnet);

   inp = tnet->tcb.inp_next;
   if(inp)
   {
      for(; inp != &tnet->tcb; inp = inp->inp_next)
      {
         tp = (struct tcpcb *)inp->inp_ppcb;
         if(tp)
         {
            if(tp->t_flags & TF_DELACK)
            {
               tp->t_flags &= ~TF_DELACK;
               tp->t_flags |= TF_ACKNOW;
            //   tcpstat.tcps_delack++;

               tcp_output(tnet, tp);
            }
         }
      }
   }
   splx(tnet, sm);
}

//----------------------------------------------------------------------------
// Tcp protocol timeout routine called every 500 ms.
// Updates the timers in all active tcb's and
// causes finite state machine actions if timers expire.
//----------------------------------------------------------------------------
void tcp_slowtimo(TN_NET * tnet)
{
   struct inpcb *ip, *ipnxt;
   struct tcpcb *tp;
   struct socket_tcp * so;
   int sm;
   int i;

   sm = splnet(tnet);

   tnet->tcp_maxidle = TCPTV_KEEPCNT * tnet->tcp_keepintvl;

        // Search through tcb's and update active timers.

   ip = tnet->tcb.inp_next;
   if(ip == 0)
   {
      splx(tnet, sm);
      return;
   }

   ipnxt = NULL;
   for(; ip != &tnet->tcb; ip = ipnxt)
   {
      if(ip == NULL)
         continue;
      ipnxt = ip->inp_next;
      tp = intotcpcb(ip);
      if(tp == 0)
         continue;
//--------------------------------------
      so = (struct socket_tcp *)ip->inp_socket;
      if(so->so_state & SS_NOFDREF)
      {
         if(tp->t_state > TCPS_CLOSE_WAIT && tp->t_timer[TCPT_2MSL] <= 0)
         {
            tcp_close(tnet, tp);
            continue;
         }
      }
//--------------------------------------

      for(i = 0; i < TCPT_NTIMERS; i++)
      {
         if(tp->t_timer[i] && --tp->t_timer[i] == 0)
         {
            tcp_timers(tnet, tp, i);//(int)nam);
            if(ipnxt->inp_prev != ip)
               goto tpgone;
         }
      }
      tp->t_idle++;
      if(tp->t_rtt)
         tp->t_rtt++;
tpgone:
           ;
//--------------------------------------
   }

   tnet->tcp_iss += TCP_ISSINCR>>1; //-- increment iss  (Timer -each 0.5 s)

   splx(tnet, sm);
}

//----------------------------------------------------------------------------
// Cancel all timers for TCP tp.
//----------------------------------------------------------------------------
void tcp_canceltimers(struct tcpcb *tp)
{
   int i;

   for(i = 0; i < TCPT_NTIMERS; i++)
      tp->t_timer[i] = 0;
}

//----------------------------------------------------------------------------
// TCP timer processing.
//----------------------------------------------------------------------------
struct tcpcb * tcp_timers(TN_NET * tnet, struct tcpcb * tp, int timer)
{
   int rexmt;
   struct socket_tcp * so;

   switch(timer)
   {
      //-- 2 MSL timeout in shutdown went off.  If we're closed but
      //-- still waiting for peer to close and connection has been idle
      //-- too long, or if 2MSL time is up from TIME_WAIT, delete connection
      //-- control block.  Otherwise, check again in a bit.

      case TCPT_2MSL:

         tp = tcp_close(tnet, tp);

/*
         if(tp->t_state == TCPS_TIME_WAIT)
            tp = tcp_close(tnet, tp);
         else
         {
            if(tp->t_idle <= tnet->tcp_maxidle)
               tp->t_timer[TCPT_2MSL] = tnet->tcp_keepintvl;
            else
               tp = tcp_close(tnet, tp);
         }
*/
         break;

      //-- Retransmission timer went off.  Message has not
      //-- been acked within retransmit interval.  Back off
      //-- to a longer retransmit interval and retransmit one segment.

      case TCPT_REXMT:

         tp->t_rxtshift++;
         if(tp->t_rxtshift > TCP_MAXRXTSHIFT)
         {
            tp->t_rxtshift = TCP_MAXRXTSHIFT;
            //tcpstat.tcps_timeoutdrop++;

            tp = tcp_drop(tnet, tp, tp->t_softerror ?
                                   tp->t_softerror : ETIMEDOUT);
            break;
         }
         //tcpstat.tcps_rexmttimeo++;
         rexmt = TCP_REXMTVAL(tp) * tcp_backoff[tp->t_rxtshift];
         TCPT_RANGESET(tp->t_rxtcur, rexmt, tp->t_rttmin, TCPTV_REXMTMAX);
         tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;

      //-- If losing, let the lower level know and try for
      //-- a better route.  Also, if we backed off this far,
      //-- our srtt estimate is probably bogus.  Clobber it
      //-- so we'll take the next rtt measurement as our srtt;
      //-- move the current srtt into rttvar to keep the current
      //-- retransmit times until then.

         if(tp->t_rxtshift > TCP_MAXRXTSHIFT / 4)
         {
            // Not used - invalidate cached  routing information
                  // in_losing(tp->t_inpcb);
            tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
            tp->t_srtt = 0;
         }
         tp->snd_nxt = tp->snd_una;

      //-- If timing a segment in this window, stop the timer.

         tp->t_rtt = 0;

      //-- Close the congestion window down to one segment
      //-- (we'll open it by one segment for each ack we get).
      //-- Since we probably have a window's worth of unacked
      //-- data accumulated, this "slow start" keeps us from
      //-- dumping all that data as back-to-back packets (which
      //-- might overwhelm an intermediate gateway).

      //-- There are two phases to the opening: Initially we
      //-- open by one mss on each ack.  This makes the window
      //-- size increase exponentially with time.  If the
      //-- window is larger than the path can handle, this
      //-- exponential growth results in dropped packet(s)
      //-- almost immediately.  To get more time between
      //-- drops but still "push" the network to take advantage
      //-- of improving conditions, we switch from exponential
      //-- to linear window opening at some threshhold size.
      //-- For a threshhold, we use half the current window
      //-- size, truncated to a multiple of the mss.

      //-- (the minimum cwnd that will give us exponential
      //-- growth is 2 mss.  We don't allow the threshhold
      //-- to go below this.)

         {
/*
so = (struct socket_tcp *) tp->t_inpcb->inp_socket;
tn_snprintf(dbg_buf, 95, "cc=%d sp=%d snd_wnd=%d snd_cwnd=%d\r\n",
                   so->so_snd.sb_cc,
                   sbspace(&so->so_snd),
                   tp->snd_wnd,
                   tp->snd_cwnd);
dbg_send((unsigned char *) dbg_buf);
*/
            unsigned int win = _min(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;
            if(win < 2)
               win = 2;
            tp->snd_cwnd = tp->t_maxseg;
            tp->snd_ssthresh = win * tp->t_maxseg;
            tp->t_dupacks = 0;
         }

         tcp_output(tnet, tp);
         break;

      //-- Persistance timer into zero window.
      //-- Force a byte to be output, if possible.

      case TCPT_PERSIST:

         //tcpstat.tcps_persisttimeo++;
         tcp_setpersist(tp);

         tp->t_force = 1;
         tcp_output(tnet, tp);
         tp->t_force = 0;

         break;

      //-- Keep-alive timer went off; send something
      //-- or drop connection if idle for too long.

      case TCPT_KEEP:

         //tcpstat.tcps_keeptimeo++;
         if(tp->t_state < TCPS_ESTABLISHED)
            goto dropit;
         so = (struct socket_tcp *) tp->t_inpcb->inp_socket;
         if(so->so_options & SO_KEEPALIVE && tp->t_state <= TCPS_CLOSE_WAIT)
         {
            if(tp->t_idle >= tnet->tcp_keepidle + tnet->tcp_maxidle)
               goto dropit;

      //-- Send a packet designed to force a response
      //-- if the peer is up and reachable:
      //-- either an ACK if the connection is still alive,
      //-- or an RST if the peer has closed the connection
      //-- due to timeout or reboot.
      //-- Using sequence number tp->snd_una-1
      //-- causes the transmitted zero-length segment
      //-- to lie outside the receive window;
      //-- by the protocol spec, this requires the
      //-- correspondent TCP to respond.

            // tcpstat.tcps_keepprobe++;

            //-- OpenBSD
            tcp_respond(tnet, NULL, tp, mtod(tp->t_template, unsigned char *),
                                       NULL, tp->rcv_nxt, tp->snd_una - 1, 0);
            tp->t_timer[TCPT_KEEP] = tnet->tcp_keepintvl;
         }
         else
            tp->t_timer[TCPT_KEEP] = tnet->tcp_keepidle;

         break;

dropit:
         //tcpstat.tcps_keepdrops++;
         tp = tcp_drop(tnet, tp, ETIMEDOUT);

         break;

       default:
         break;
   }

   return tp;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------




