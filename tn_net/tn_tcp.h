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
 * 3. Neither the name of the University nor the names of its contributors
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

#ifndef _TN_TCP_VARS_H_
#define _TN_TCP_VARS_H_

#include "queue.h"

typedef unsigned int tcp_seq;

//----------------------------------------------------------------------------

//-- TCP header.
//-- Per RFC 793, September, 1981.

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(push, 1)
#endif

struct tcphdr
{
   u_short   th_sport;            // source port
   u_short   th_dport;            // destination port
   tcp_seq   th_seq;              // sequence number
   tcp_seq   th_ack;              // acknowledgement number
#if CPU_BYTE_ORDER == LITTLE_ENDIAN
   u_char    th_x2:4,             // (unused)
             th_off:4;            // data offset
#endif

#if CPU_BYTE_ORDER == BIG_ENDIAN
   u_char    th_off:4,            // data offset
             th_x2:4;             // (unused)
#endif

   u_char    th_flags;

   u_short   th_win;              // window
   u_short   th_sum;              // checksum
   u_short   th_urp;              // urgent pointer
}__pkt_struct;

#define  th_reseqlen  th_urp   // TCP data length for resequencing/reassembly
typedef struct tcphdr TN_TCPHDR;

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(pop)
#endif

#define   TH_FIN                0x01
#define   TH_SYN                0x02
#define   TH_RST                0x04
#define   TH_PUSH               0x08
#define   TH_ACK                0x10
#define   TH_URG                0x20

#define   TCPOPT_EOL                0
#define   TCPOPT_NOP                1
#define   TCPOPT_MAXSEG             2
#define   TCPOLEN_MAXSEG            4
#define   TCPOPT_WINDOW             3
#define   TCPOLEN_WINDOW            3
#define   TCPOPT_SACK_PERMITTED     4                /* Experimental */
#define   TCPOLEN_SACK_PERMITTED    2
#define   TCPOPT_SACK               5                /* Experimental */
#define   TCPOPT_TIMESTAMP          8
#define   TCPOLEN_TIMESTAMP        10
#define   TCPOLEN_TSTAMP_APPA      (TCPOLEN_TIMESTAMP+2) /* appendix A */

#define   TCPOPT_TSTAMP_HDR \
           (TCPOPT_NOP<<24|TCPOPT_NOP<<16|TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP)

/*
 * Default maximum segment size for TCP.
 * With an IP MSS of 576, this is 536,
 * but 512 is probably more convenient.
 * This should be defined as MIN(512, IP_MSS - sizeof (struct tcpiphdr)).
 */

#define  TCP_MSS            512

#define  TCP_MAXWIN       65535     /* largest value for (unscaled) window */

#define  TCP_MAX_WINSHIFT    14     /* maximum window shift */


// User-settable options (used with setsockopt).


#define  _TCP_NODELAY      0x01     /* don't delay send to coalesce packets */
#define  _TCP_MAXSEG       0x02     /* set maximum segment size */

//----------------------------------------------------------------------------

struct tcpqehead
{
   struct tcpqent *tqh_first;
   struct tcpqent **tqh_last;
};

struct tcpqent
{
   TAILQ_ENTRY(tcpqent) tcpqe_q;
   struct tcphdr * tcpqe_tcp;
   struct mbuf   * tcpqe_m;       //-- mbuf contains packet
};


// Definitions of the TCP timers.  These timers are counted
// down PR_SLOWHZ times a second.

#define  TCPT_NTIMERS      4

#define  TCPT_REXMT        0                /* retransmit */
#define  TCPT_PERSIST      1                /* retransmit persistance */
#define  TCPT_KEEP         2                /* keep alive */
#define  TCPT_2MSL         3                /* 2*msl quiet time timer */


//---  TCP control block, one per tcp (113 (???) bytes)

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(push, 1)
#endif

struct tcpcb
{
   struct tcpqehead t_segq;      //-- 8!!! sequencing queue
   struct mbuf * t_template;     //-- 4 skeletal packet for transmit
   struct inpcb * t_inpcb;       //-- 4  back pointer to internet pcb

//-- send sequence variables

   tcp_seq  snd_una;          /* 4 send unacknowledged */
   tcp_seq  snd_nxt;          /* 4 send next */
   tcp_seq  snd_up;           /* 4 send urgent pointer */
   tcp_seq  snd_wl1;          /* 4 window update seg seq number */
   tcp_seq  snd_wl2;          /* 4 window update seg ack number */
   tcp_seq  iss;              /* 4 initial send sequence number */
   u32_t   snd_wnd;           /* 4 send window */

//-- receive sequence variables

   u32_t   rcv_wnd;           /* 4 receive window */
   tcp_seq  rcv_nxt;          /* 4 receive next */
   tcp_seq  rcv_up;           /* 4 receive urgent pointer */
   tcp_seq  irs;              /* 4 initial receive sequence number */

//-- receive variables

   tcp_seq   rcv_adv;         /* 4 advertised window */

//-- retransmit variables

   tcp_seq   snd_max;         // 4 highest sequence number sent;
                              //   used to recognize retransmits

//-- congestion control (for slow start, source quench, retransmit after loss)

   u32_t  snd_cwnd;         /* 4 congestion-controlled window */
   u32_t  snd_ssthresh;     /* 4 snd_cwnd size threshhold for
                                    * for slow start exponential to
                                    * linear switch
                                    */
 //--My

   TN_NETIF * ni;

   tcp_seq    t_rtseq;            /* 4 sequence number being timed */
   u32_t      max_sndwnd;         /* 4 largest window peer has offered */

   __pkt_field short    t_state;                /* 2 state of this connection */
   __pkt_field short    t_rxtshift;             /* 2 log(2) of rexmt exp. backoff */
   __pkt_field short    t_rxtcur;               /* 2 current retransmit value */
   __pkt_field short    t_dupacks;              /* 2 consecutive dup acks recd */
   __pkt_field u_short  t_maxseg;               /* 2  maximum segment size */
   __pkt_field u_short  t_flags;                // 2

   __pkt_field short    t_timer[TCPT_NTIMERS];  /* 8  tcp timers */

//-- transmit timing stuff.  See below for scale of srtt and rttvar.
//-- "Variance" is actually smoothed difference.

   __pkt_field short        t_idle;             /* 2 inactivity time */
   __pkt_field short        t_rtt;              /* 2 round trip time */
   __pkt_field short        t_srtt;             /* 2 smoothed round-trip time */
   __pkt_field short        t_rttvar;           /* 2 variance in round-trip time */
   __pkt_field u_short      t_rttmin;           /* 2 minimum rtt allowed */

   __pkt_field char     t_force;                /* 1 if forcing out a byte */

//-- out-of-band data

   __pkt_field char        t_oobflags;          /* 1 have some */

   __pkt_field short       t_softerror;         /* 2 possible error not yet reported */

}__pkt_struct_ex;

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(pop)
#endif

//-- Handy way of passing around TCP option info.

struct tcp_opt_info
{
   int ts_present;
   unsigned int ts_val;
   unsigned int ts_ecr;
   unsigned short maxseg;
};


#define   TF_ACKNOW              0x0001       /* ack peer immediately */
#define   TF_DELACK              0x0002       /* ack, but try to delay it */
#define   TF_NODELAY             0x0004       /* don't delay packets to coalesce */
#define   TF_NOOPT               0x0008       /* don't use tcp options */
#define   TF_SENTFIN             0x0010       /* have sent FIN */
#define   TF_REQ_SCALE           0x0020       /* have/will request window scaling */
#define   TF_RCVD_SCALE          0x0040       /* other side has requested scaling */
#define   TF_REQ_TSTMP           0x0080       /* have/will request timestamps */
#define   TF_RCVD_TSTMP          0x0100       /* a timestamp was received in SYN */
#define   TF_SACK_PERMIT         0x0200       /* other side said I could SACK */

#define   TCPOOB_HAVEDATA          0x01
#define   TCPOOB_HADDATA           0x02


#define   intotcpcb(ip)          ((struct tcpcb *)(ip)->inp_ppcb)
#define   sototcpcb(so)          (intotcpcb(sotoinpcb(so)))

/*
 * The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 3 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 2 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */

#define   TCP_RTT_SCALE               8          /* multiplier for srtt; 3 bits frac. */
#define   TCP_RTT_SHIFT               3          /* shift for srtt; 3 bits frac. */
#define   TCP_RTTVAR_SCALE            4          /* multiplier for rttvar; 2 bits */
#define   TCP_RTTVAR_SHIFT            2          /* multiplier for rttvar; 2 bits */

/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This macro assumes that the value of TCP_RTTVAR_SCALE
 * is the same as the multiplier for rttvar.
 */

#define   TCP_REXMTVAL(tp) \
        (((tp)->t_srtt >> TCP_RTT_SHIFT) + (tp)->t_rttvar)

/* XXX
 * We want to avoid doing m_pullup on incoming packets but that
 * means avoiding dtom on the tcp reassembly code.  That in turn means
 * keeping an mbuf pointer in the reassembly queue (since we might
 * have a cluster).  As a quick hack, the source & destination
 * port numbers (which are no longer needed once we've located the
 * tcpcb) are overlayed with an mbuf pointer.
 */

//#define REASS_MBUF(ti) (*(struct mbuf **)&((ti)->ti_t))

///*
// * TCP statistics.
// * Many of these should be kept per connection,
// * but that's inconvenient at the moment.
// */
//struct        tcpstat {
//        u_long        tcps_connattempt;        /* connections initiated */
//        u_long        tcps_accepts;                /* connections accepted */
//        u_long        tcps_connects;                /* connections established */
//        u_long        tcps_drops;                /* connections dropped */
//        u_long        tcps_conndrops;                /* embryonic connections dropped */
//        u_long        tcps_closed;                /* conn. closed (includes drops) */
//        u_long        tcps_segstimed;                /* segs where we tried to get rtt */
//        u_long        tcps_rttupdated;        /* times we succeeded */
//        u_long        tcps_delack;                /* delayed acks sent */
//        u_long        tcps_timeoutdrop;        /* conn. dropped in rxmt timeout */
//        u_long        tcps_rexmttimeo;        /* retransmit timeouts */
//        u_long        tcps_persisttimeo;        /* persist timeouts */
//        u_long        tcps_keeptimeo;                /* keepalive timeouts */
//        u_long        tcps_keepprobe;                /* keepalive probes sent */
//        u_long        tcps_keepdrops;                /* connections dropped in keepalive */
//
//        u_long        tcps_sndtotal;                /* total packets sent */
//        u_long        tcps_sndpack;                /* data packets sent */
//        u_long        tcps_sndbyte;                /* data bytes sent */
//        u_long        tcps_sndrexmitpack;        /* data packets retransmitted */
//        u_long        tcps_sndrexmitbyte;        /* data bytes retransmitted */
//        u_long        tcps_sndacks;                /* ack-only packets sent */
//        u_long        tcps_sndprobe;                /* window probes sent */
//        u_long        tcps_sndurg;                /* packets sent with URG only */
//        u_long        tcps_sndwinup;                /* window update-only packets sent */
//        u_long        tcps_sndctrl;                /* control (SYN|FIN|RST) packets sent */
//
//        u_long        tcps_rcvtotal;                /* total packets received */
//        u_long        tcps_rcvpack;                /* packets received in sequence */
//        u_long        tcps_rcvbyte;                /* bytes received in sequence */
//        u_long        tcps_rcvbadsum;                /* packets received with ccksum errs */
//        u_long        tcps_rcvbadoff;                /* packets received with bad offset */
//        u_long        tcps_rcvshort;                /* packets received too short */
//        u_long        tcps_rcvduppack;        /* duplicate-only packets received */
//        u_long        tcps_rcvdupbyte;        /* duplicate-only bytes received */
//        u_long        tcps_rcvpartduppack;        /* packets with some duplicate data */
//        u_long        tcps_rcvpartdupbyte;        /* dup. bytes in part-dup. packets */
//        u_long        tcps_rcvoopack;                /* out-of-order packets received */
//        u_long        tcps_rcvoobyte;                /* out-of-order bytes received */
//        u_long        tcps_rcvpackafterwin;        /* packets with data after window */
//        u_long        tcps_rcvbyteafterwin;        /* bytes rcvd after window */
//        u_long        tcps_rcvafterclose;        /* packets rcvd after "close" */
//        u_long        tcps_rcvwinprobe;        /* rcvd window probe packets */
//        u_long        tcps_rcvdupack;                /* rcvd duplicate acks */
//        u_long        tcps_rcvacktoomuch;        /* rcvd acks for unsent data */
//        u_long        tcps_rcvackpack;        /* rcvd ack packets */
//        u_long        tcps_rcvackbyte;        /* bytes acked by rcvd acks */
//        u_long        tcps_rcvwinupd;                /* rcvd window update packets */
//        u_long        tcps_pawsdrop;                /* segments dropped due to PAWS */
//        u_long        tcps_predack;                /* times hdr predict ok for acks */
//        u_long        tcps_preddat;                /* times hdr predict ok for data pkts */
//        u_long        tcps_pcbcachemiss;
//};
//

//----------------------------------


// TCP sequence numbers are 32 bit integers operated
// on with modular arithmetic.  These macros can be
// used to compare such integers.


#define        SEQ_LT(a,b)        ((int)((a)-(b)) < 0)
#define        SEQ_LEQ(a,b)        ((int)((a)-(b)) <= 0)
#define        SEQ_GT(a,b)        ((int)((a)-(b)) > 0)
#define        SEQ_GEQ(a,b)        ((int)((a)-(b)) >= 0)

// Macros to initialize tcp sequence numbers for
// send and receive from initial send and receive
// sequence numbers.

#define  tcp_rcvseqinit(tp) \
           (tp)->rcv_adv = (tp)->rcv_nxt = (tp)->irs + 1

#define  tcp_sendseqinit(tp) \
           (tp)->snd_una = (tp)->snd_nxt = (tp)->snd_max = (tp)->snd_up = \
           (tp)->iss

#define  TCP_ISSINCR    (125*1024)    // increment for tcp_iss each second

      //-- TCP FSM state definitions.
      //-- Per RFC793, September, 1981.

#define   TCP_NSTATES            11

#define   TCPS_CLOSED             0        /* closed */
#define   TCPS_LISTEN             1        /* listening for connection */
#define   TCPS_SYN_SENT           2        /* active, have sent syn */
#define   TCPS_SYN_RECEIVED       3        /* have send and received syn */

// states < TCPS_ESTABLISHED are those where connections not established

#define   TCPS_ESTABLISHED        4        /* established */
#define   TCPS_CLOSE_WAIT         5        /* rcvd fin, waiting for close */

// states > TCPS_CLOSE_WAIT are those where user has closed

#define   TCPS_FIN_WAIT_1         6        /* have closed, sent fin */
#define   TCPS_CLOSING            7        /* closed xchd FIN; await FIN ACK */
#define   TCPS_LAST_ACK           8        /* had fin and close; await FIN ACK */

// states > TCPS_CLOSE_WAIT && < TCPS_FIN_WAIT_2 await ACK of FIN

#define   TCPS_FIN_WAIT_2         9        /* have closed, fin is acked */
#define   TCPS_TIME_WAIT         10        /* in 2*msl quiet wait after close */

#define   TCPS_HAVERCVDSYN(s)        ((s) >= TCPS_SYN_RECEIVED)
#define   TCPS_HAVEESTABLISHED(s)    ((s) >= TCPS_ESTABLISHED)
//#define   TCPS_HAVERCVDFIN(s)        ((s) >= TCPS_TIME_WAIT)


#define	TCPS_HAVERCVDFIN(s) \
    ((s) == TCPS_CLOSE_WAIT || ((s) >= TCPS_CLOSING && (s) != TCPS_FIN_WAIT_2))


//----------------------------------------------------------------------------

//-- Time constants.


#define  TCPTV_MSL         2 // (30*PR_SLOWHZ)      /* max seg lifetime (hah!) */
#define  TCPTV_SRTTBASE               0        /* base roundtrip time;
                                                   if 0, no idea yet */
#define  TCPTV_SRTTDFLT    (3*PR_SLOWHZ)       /* assumed RTT if no info */

#define  TCPTV_PERSMIN     (5*PR_SLOWHZ)       /* retransmit persistance */
#define  TCPTV_PERSMAX     (60*PR_SLOWHZ)      /* maximum persist interval */

#define  TCPTV_KEEP_INIT  2 // (75*PR_SLOWHZ)      /* initial connect keep alive */
#define  TCPTV_KEEP_IDLE  2 // (120*60*PR_SLOWHZ)  /* dflt time before probing */
#define  TCPTV_KEEPINTVL  2 // (75*PR_SLOWHZ)      /* default probe interval */
#define  TCPTV_KEEPCNT            2        /* max probes before drop */

#define  TCPTV_MIN         (1*PR_SLOWHZ)       /* minimum allowable value */
#define  TCPTV_REXMTMAX    (64*PR_SLOWHZ)      /* max allowable REXMT value */

#define  TCP_LINGERTIME             120        /* linger at most 2 minutes */

#define  TCP_MAXRXTSHIFT             12        /* maximum retransmits */

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

int tcp_output(TN_NET * tnet, struct tcpcb *tp);

struct tcpcb * tcp_close(TN_NET * tnet, struct tcpcb *tp);
struct mbuf * tcp_template(TN_NET * tnet, struct tcpcb *tp);
struct tcpcb * tcp_drop(TN_NET * tnet, struct tcpcb *tp, int errno);
struct tcpcb * tcp_newtcpcb(TN_NET * tnet, struct inpcb *inp);

  //--- tn_tcp_timer.c

struct tcpcb * tcp_timers(TN_NET * tnet, struct tcpcb * tp, int timer);
  void tcp_canceltimers(struct tcpcb * tp);

  //---

 int tcp_freeq(TN_NET * tnet, struct tcpcb *tp);
 int tcp_mss(struct tcpcb * tp, unsigned int offer, TN_NETIF * ni);
void tcp_fasttimo(TN_NET * tnet);
void tcp_slowtimo(TN_NET * tnet);
struct tcpcb * tcp_usrclosed(TN_NET * tnet, struct tcpcb * tp);

#endif





