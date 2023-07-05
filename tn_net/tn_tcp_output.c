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
 *        @(#)tcp_output.c        8.3 (Berkeley) 12/30/93
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
#include "tn_protosw.h"
#include "bsd_socket.h"
#include "tn_socket.h"
#include "tn_net_utils.h"
#include "ip_icmp.h"
#include "tn_tcp.h"
#include "tn_sockets_tcp.h"
#include "tn_net_func.h"

#include "dbg_func.h"

#if defined (__ICCARM__)   // IAR ARM
#pragma diag_suppress=Pa039
#endif


#define MAX_TCPOPTLEN    32     /* max # bytes that go in options */


extern const int tcp_backoff[];

// Flags used when sending segments in tcp_output.
// Basic flags (TH_RST,TH_ACK,TH_SYN,TH_FIN) are totally
// determined by state, with the proviso that TH_FIN is sent only
// if all data queued for output is included in the segment.

const u_char tcp_outflags[TCP_NSTATES] =
{
    TH_RST | TH_ACK,
    0,
    TH_SYN,
    TH_SYN | TH_ACK,
    TH_ACK,
    TH_ACK,
    TH_FIN | TH_ACK,
    TH_FIN | TH_ACK,
    TH_FIN | TH_ACK,
    TH_ACK,
    TH_ACK
};

//----------------------------------------------------------------------------
void tcp_setpersist(struct tcpcb *tp)
{
   int t = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;

   //-- Start/restart persistance timer.

   TCPT_RANGESET(tp->t_timer[TCPT_PERSIST],
                  t * tcp_backoff[tp->t_rxtshift],
                        TCPTV_PERSMIN, TCPTV_PERSMAX);

   if(tp->t_rxtshift < TCP_MAXRXTSHIFT)
      tp->t_rxtshift++;
}

//----------------------------------------------------------------------------
// Tcp output routine: figure out what should be sent and send it.
//----------------------------------------------------------------------------
int tcp_output(TN_NET * tnet, struct tcpcb * tp)
{
   long len, win;
   int off, flags, error;
   struct mbuf * m;
   struct tcphdr * th;
   u_char opt[MAX_TCPOPTLEN];
   unsigned int optlen, hdrlen;
   int idle, sendalot;
   struct ip * ip;
   struct socket_tcp * so;

   error = 0;
   so = (struct socket_tcp *)tp->t_inpcb->inp_socket;


   //-- Determine length of data that should be transmitted,
   //-- and flags that will be used.
   //-- If there is some data or critical controls (SYN, RST)
   //-- to send, then transmit; otherwise, investigate further.

   idle = (tp->snd_max == tp->snd_una);

   //-- We have been idle for "a while" and no acks are
   //-- expected to clock out any data we send --
   //-- slow start to get ack "clock" running again.

   if(idle && tp->t_idle >= tp->t_rxtcur)
      tp->snd_cwnd = tp->t_maxseg;

   //-- OpenBSD
    //   tp->snd_cwnd = 2 * tp->t_maxseg;

again:

   sendalot = 0;
   off = tp->snd_nxt - tp->snd_una;
   win = _min(tp->snd_wnd, tp->snd_cwnd);

   flags = tcp_outflags[tp->t_state];

   //-- If in persist timeout with window of 0, send 1 byte.
   //-- Otherwise, if window is small but nonzero
   //-- and timer expired, we will send what we can
   //-- and go to transmit state.

   if(tp->t_force)
   {
      if(win == 0)
         win = 1;
      else
      {
         tp->t_timer[TCPT_PERSIST] = 0;
         tp->t_rxtshift = 0;
      }
   }

   len = _min((int)so->so_snd.sb_cc, win) - off;

   if(len < 0)
   {
      //-- If FIN has been sent but not acked,
      //-- but we haven't been called to retransmit,
      //-- len will be -1.  Otherwise, window shrank
      //-- after we sent into it.  If window shrank to 0,
      //-- cancel pending retransmit, pull snd_nxt back
      //-- to (closed) window, and set the persist timer
      //-- if it isn't already going.  If the window didn't
      //-- close completely, just wait for an ACK.

      len = 0;
      if(win == 0)
      {
         tp->t_timer[TCPT_REXMT] = 0;
         tp->snd_nxt = tp->snd_una;
         //-- Add-on to BSD4.4
       //  tp->t_rxtshift = 0;
       //  if(tp->t_timer[TCPT_PERSIST] == 0)
       //    tcp_setpersist(tp);
      }
   }

   if(len > tp->t_maxseg)
   {
      len = tp->t_maxseg;
      sendalot = 1;
   }

   if(SEQ_LT(tp->snd_nxt + len, tp->snd_una + so->so_snd.sb_cc))
      flags &= ~TH_FIN;

   win = sbspace(&so->so_rcv);

   //-- Sender silly window avoidance.  If connection is idle
   //-- and can send all data, a maximum segment,
   //-- at least a maximum default-size segment do it,
   //-- or are forced, do it; otherwise don't bother.
   //-- If peer's buffer is tiny, then send
   //-- when window is at least half open.
   //-- If retransmitting (possibly after persist timer forced us
   //-- to send into a small window), then must resend.

   if(len)
   {
       if(len == tp->t_maxseg)
         goto send;
      if((idle || tp->t_flags & TF_NODELAY) &&
            len + off >= (int)so->so_snd.sb_cc)
          goto send;
      if(tp->t_force)  //-- 4.3
          goto send;
      if(len >= (int)(tp->max_sndwnd / 2) && tp->max_sndwnd > 0) //-- OpenBSD
         goto send;
      if(SEQ_LT(tp->snd_nxt, tp->snd_max))
         goto send;
   }

   //-- Compare available window to amount of window
   //-- known to peer (as advertised window less
   //-- next expected input).  If the difference is at least two
   //-- max size segments, or at least 50% of the maximum possible
   //-- window, then want to send a window update to peer.

   if(win > 0)
   {
      int adv = win - (tp->rcv_adv - tp->rcv_nxt);

      if(so->so_snd.sb_cc == 0 && adv >= (int) (2 * tp->t_maxseg))
         goto send;

      if((adv<<1) >= (long) so->so_rcv.sb_hiwat)
         goto send;
   }

   //-- Send if we owe peer an ACK.

   if(tp->t_flags & TF_ACKNOW)
      goto send;
   if(flags & (TH_SYN|TH_RST))
      goto send;
   if(SEQ_GT(tp->snd_up, tp->snd_una))
      goto send;

   //-- If our state indicates that FIN should be sent
   //-- and we have not yet done so, or we're retransmitting the FIN,
   //-- then we need to send.

   if(flags & TH_FIN &&
            ((tp->t_flags & TF_SENTFIN) == 0 || tp->snd_nxt == tp->snd_una))
      goto send;

   //-- TCP window updates are not reliable, rather a polling protocol
   //-- using ``persist'' packets is used to insure receipt of window
   //-- updates.  The three ``states'' for the output side are:
   //--        idle                        not doing retransmits or persists
   //--        persisting                to move a small or zero window
   //--        (re)transmitting        and thereby not persisting
   //--
   //-- tp->t_timer[TCPT_PERSIST]
   //--        is set when we are in persist state.
   //-- tp->t_force
   //--        is set when we are called to send a persist packet.
   //-- tp->t_timer[TCPT_REXMT]
   //--        is set when we are retransmitting
   //-- The output side is idle when both timers are zero.
   //--
   //-- If send window is too small, there is data to transmit, and no
   //-- retransmit or persist is pending, then go to persist state.
   //-- If nothing happens soon, send when timer expires:
   //-- if window is nonzero, transmit what we can,
   //-- otherwise force out a byte.

   if(so->so_snd.sb_cc && tp->t_timer[TCPT_REXMT] == 0 &&
                           tp->t_timer[TCPT_PERSIST] == 0)
   {
      tp->t_rxtshift = 0;
      tcp_setpersist(tp);
   }

   //-- No reason to send a segment, just return.

   return 0;

send:

   //-- Before ESTABLISHED, force sending of initial options
   //-- unless TCP set not to do any options.
   //-- NOTE: we assume that the IP/TCP header plus TCP options
   //-- always fit in a single mbuf, leaving room for a maximum
   //-- link header, i.e.
   //--        max_linkhdr + sizeof (struct tcpiphdr) + optlen <= MHLEN

   optlen = 0;
   hdrlen = sizeof(struct ip) + sizeof(struct tcphdr);  //--  sizeof(tcpiphdr);
   if(flags & TH_SYN)
   {
      tp->snd_nxt = tp->iss;
      if((tp->t_flags & TF_NOOPT) == 0)
      {
         u_short mss;

         opt[0] = TCPOPT_MAXSEG;
         opt[1] = 4;
         mss = htons((u_short) tcp_mss(tp, 0, NULL));
         bcopy((unsigned char *)&mss, (unsigned char *)(opt + 2), sizeof(mss));
         optlen = 4;
      }
   }

   hdrlen += optlen;

   //-- Adjust data length if insertion of options will
   //-- bump the packet length beyond the t_maxseg length.

   if(len > (int)(tp->t_maxseg - (unsigned short)optlen))
   {
      len = tp->t_maxseg - optlen;
      sendalot = 1;

   //-- OpenBSD - If there is still more to send, don't close the connection.
      flags &= ~TH_FIN;
   }

   //-- Grab a header mbuf, attaching a copy of data to
   //-- be transmitted, and initialize the header from
   //-- the template for sends on this connection.

   if(len)
   {

//      if(tp->t_force && len == 1)
//         tcpstat.tcps_sndprobe++;
//      else if (SEQ_LT(tp->snd_nxt, tp->snd_max))
//      {
//         tcpstat.tcps_sndrexmitpack++;
//         tcpstat.tcps_sndrexmitbyte += len;
//      }
//      else
//      {
//         tcpstat.tcps_sndpack++;
//         tcpstat.tcps_sndbyte += len;
//      }

      //-- TN NET -  hdrlen will fit to 128 bytes

      m = mb_get(tnet, MB_MID1, MB_NOWAIT, TRUE); //-- From Tx pool
       if(m == NULL)
      {
         error = ENOBUFS;
         goto out;
      }

      m->m_flags = M_PKTHDR;
      m->m_len  = hdrlen;
      m->m_next = m_copy(tnet, so->so_snd.sb_mb, off, (int) len);
      if(m->m_next == NULL)
      {
         if(m_free(tnet, m) == INV_MEM)
            tn_net_panic(INV_MEM_1);
         error = ENOBUFS;
         goto out;
      }

      //-- If we're sending everything we've got, set PUSH.
      //-- (This will keep happy those implementations which only
      //-- give data to the user when a buffer fills or
      //-- a PUSH comes in.)

      if(off + len == so->so_snd.sb_cc) //-- OpenBSD && !soissending(so))
         flags |= TH_PUSH;
   }
   else
   {
//      if(tp->t_flags & TF_ACKNOW)
//         tcpstat.tcps_sndacks++;
//      else if(flags & (TH_SYN|TH_FIN|TH_RST))
//         tcpstat.tcps_sndctrl++;
//      else if(SEQ_GT(tp->snd_up, tp->snd_una))
//         tcpstat.tcps_sndurg++;
//      else
//         tcpstat.tcps_sndwinup++;

      //-- TN NET -  hdrlen will fit to 128 bytes

      m = mb_get(tnet, MB_MID1, MB_NOWAIT, TRUE); //-- From Tx pool
      if(m == NULL)
      {
         error = ENOBUFS;
         goto out;
      }
      m->m_len = hdrlen;
   }

   //-- Copy IP/TCP template into allocated header mbuf

   bcopy(mtod(tp->t_template, unsigned char *), mtod(m, unsigned char *),
                                                     tp->t_template->m_len);
   th = (struct tcphdr *)(mtod(m, unsigned char *) + tp->t_template->m_len -
                                                      sizeof(struct tcphdr));

   m->m_tlen = hdrlen + len;

   //-- Fill in fields, remembering maximum advertised
   //-- window for use in delaying messages about window sizes.
   //-- If resending a FIN, be sure not to use a new sequence number.

   if(flags & TH_FIN && tp->t_flags & TF_SENTFIN && tp->snd_nxt == tp->snd_max)
      tp->snd_nxt--;

   //-- If we are doing retransmissions, then snd_nxt will
   //-- not reflect the first unsent octet.  For ACK only
   //-- packets, we do not want the sequence number of the
   //-- retransmitted packet, we want the sequence number
   //-- of the next unsent octet.  So, if there is no data
   //-- (and no SYN or FIN), use snd_max instead of snd_nxt
   //-- when filling in ti_seq.  But if we are in persist
   //-- state, snd_max might reflect one byte beyond the
   //-- right edge of the window, so use snd_nxt in that
   //-- case, since we know we aren't doing a retransmission.
   //-- (retransmit and persist are mutually exclusive...)

 //  if(len || (flags & (TH_SYN|TH_FIN)) || tp->t_timer[TCPT_PERSIST])
 //     th->th_seq = htonl(tp->snd_nxt);
 //  else
 //     th->th_seq = htonl(tp->snd_max);

//---- 4.3 -----------------------------------
   if(flags & TH_FIN && tp->t_flags & TF_SENTFIN &&
                 tp->snd_nxt == tp->snd_max)
      tp->snd_nxt--;

   th->th_seq = htonl(tp->snd_nxt);
//--------------------------------------------

   th->th_ack = htonl(tp->rcv_nxt);
   if(optlen)
   {
      bcopy((unsigned char *)opt, (unsigned char *)(th + 1), optlen);
      th->th_off = (sizeof (struct tcphdr) + optlen) >> 2;
   }
   th->th_flags = flags;

   //-- Calculate receive window.  Don't shrink window,
   //-- but avoid silly window syndrome.

   if(win < (long)(so->so_rcv.sb_hiwat / 4) && win < (long)tp->t_maxseg)
      win = 0;

   if(win > tnet->tcp_recvspace) //(long)TCP_MAXWIN)
      win = tnet->tcp_recvspace; //(long)TCP_MAXWIN;
   if(win < (long)(tp->rcv_adv - tp->rcv_nxt))
      win = (long)(tp->rcv_adv - tp->rcv_nxt);

   if(flags & TH_RST)  //--- OpenBSD
      win = 0;

   th->th_win = htons((unsigned short)win); //-- no tp->rcv_scale*
   if(SEQ_GT(tp->snd_up, tp->snd_nxt))
   {
      u_int32_t urp = tp->snd_up - tp->snd_nxt;
      if(urp > IP_MAXPACKET)
         urp = IP_MAXPACKET;
      th->th_urp = htons((unsigned short)urp);
      th->th_flags |= TH_URG;
   }
   else
      //-- If no urgent pointer to send, then we pull
      //-- the urgent pointer to the left edge of the send window
      //-- so that it doesn't drift into the send window on sequence
      //-- number wraparound.

      tp->snd_up = tp->snd_una;                /* drag it along */

   //-- Put TCP length in extended header, and then
   //-- checksum extended header and data.

   //-- In transmit state, time the transmission and arrange for
   //-- the retransmit.  In persist state, just set snd_max.

   if(tp->t_force == 0 || tp->t_timer[TCPT_PERSIST] == 0)
   {
      tcp_seq startseq = tp->snd_nxt;

     //-- Advance snd_nxt over sequence space of this segment.

      if(flags & (TH_SYN|TH_FIN))
      {
         if(flags & TH_SYN)
            tp->snd_nxt++;
         if(flags & TH_FIN)
         {
            tp->snd_nxt++;
            tp->t_flags |= TF_SENTFIN;
         }
      }
      tp->snd_nxt += len;
      if(SEQ_GT(tp->snd_nxt, tp->snd_max))
      {
         tp->snd_max = tp->snd_nxt;

        //-- Time this transmission if not a retransmission and
        //-- not currently timing anything.

         if(tp->t_rtt == 0)
         {
            tp->t_rtt = 1;
            tp->t_rtseq = startseq;
            //tcpstat.tcps_segstimed++;
         }
      }

      //-- Set retransmit timer if not currently set,
      //-- and not doing an ack or a keep-alive probe.
      //-- Initial value for retransmit timer is smoothed
      //-- round-trip time + 2 * round-trip time variance.
      //-- Initialize shift counter which is used for backoff
      //-- of retransmit time.

      if(tp->t_timer[TCPT_REXMT] == 0 && tp->snd_nxt != tp->snd_una)
      {
         tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
         if(tp->t_timer[TCPT_PERSIST])
         {
            tp->t_timer[TCPT_PERSIST] = 0;
            tp->t_rxtshift = 0;
         }
      }
   }
   else
      if(SEQ_GT(tp->snd_nxt + len, tp->snd_max))
         tp->snd_max = tp->snd_nxt + len;

   //-- Fill in IP length and desired time to live and
   //-- send to IP level.  There should be a better way
   //-- to handle ttl and tos; we could keep them in
   //-- the template, but need a way to checksum without them.

   //--- OpenBSD

   ip = mtod(m, struct ip *);
   ip->ip_len = htons(m->m_tlen);
   ip->ip_ttl = MAXTTL;    // tnet->ip_defttl or tp->t_inpcb->inp_ip->ip_ttl;
   ip->ip_tos = 0;         // tp->t_inpcb->inp_ip->ip_tos;

  // XXX Case TN NET does not uses here(and does not expects) IP options,
  // here off = sizeof(IP header)

   th->th_sum = in4_cksum(m,
                      IPPROTO_TCP,  //-- to force calc TCP pseudo header
                      sizeof(struct ip),   //-- off; see above
                      m->m_tlen - sizeof(struct ip)); //-- len = TCP header + data
   if(m == m->m_next)
     error++;

   error = ip_output(tnet, tp->ni, m);  //-- YVT

   if(error)
   {
out:
      if(error == ENOBUFS)
      {
         tp->snd_cwnd = tp->t_maxseg; //-- 4.4 tcp_quench()
         return 0;
      }

      if((error == EHOSTUNREACH || error == ENETDOWN)
                              && TCPS_HAVERCVDSYN(tp->t_state))
      {
         tp->t_softerror = error;
         return 0;
      }

      return error;
   }

   //tcpstat.tcps_sndtotal++;

   //-- Data sent (as far as we can tell).
   //-- If this advertises a larger window than any other segment,
   //-- then remember the size of the advertised window.
   //-- Any pending ACK has now been sent.

   if(win > 0 && SEQ_GT(tp->rcv_nxt+win, tp->rcv_adv))
      tp->rcv_adv = tp->rcv_nxt + win;

   tp->t_flags &= ~(TF_ACKNOW|TF_DELACK);

   if(sendalot)
      goto again;

   return 0; //-- OK
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------




