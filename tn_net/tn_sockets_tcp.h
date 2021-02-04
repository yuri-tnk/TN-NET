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

#ifndef _TN_SOCKETS_TCP_H_
#define _TN_SOCKETS_TCP_H_

//--- TN NET - case tcp socket does not fit to the 128 bytes, an additional
//--- memory block (32 bytes) is allocated

struct socket_tcp;

struct tn_socketex
{
   TN_SEM so_wait_sem;
   struct socket_tcp * so; //-- ptr to the main socket buf body
   struct tn_socketex * next; //-- ptr to the next socket in the list
};
typedef struct tn_socketex TN_SOCKETEX;


// Contains send and receive buffer queues etc.

struct sockbuf
{
   u32_t  sb_cc;           /* actual chars in buffer */
   u32_t  sb_hiwat;        /* max actual char count */
   long    sb_lowat;       /* low water mark */
   struct  mbuf * sb_mb;   /* the mbuf chain */
   TN_SEM  sb_sem;
   short   sb_flags;       /* flags, see below */
};

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(push, 1)
#endif

struct socket_tcp
{
    __pkt_field short   so_type;                 /* generic type, see socket.h */
    __pkt_field short   so_options;              /* from socket call, see socket.h */
    __pkt_field short   so_linger;               /* time to linger while closing */
    __pkt_field short   so_state;                /* internal state flags SS_*, below */
    __pkt_field unsigned char * so_pcb;          /* protocol control block */
  __pkt_field   sofree_func sofree;              // 4

   //-- Variables for connection queueing.
   //-- Socket where accepts occur is so_head in all subsidiary sockets.
   //-- If so_head is 0, socket is not related to an accept.
   //-- For head socket so_q0 queues partially completed connections,
   //-- while so_q is a queue of connections ready to be accepted.
   //-- If a connection is aborted and it has so_head set, then
   //-- it has to be pulled out of either so_q0 or so_q.
   //-- We allow connections to queue up based on current queue lengths
   //-- and limit on number of queued connections for this socket.

   struct socket_tcp * so_head;             /* back pointer to accept socket */
   struct socket_tcp * so_q0;               /* queue of partial connections */
   struct socket_tcp * so_q;                /* queue of incoming connections */

   __pkt_field short so_q0len;              /* partials on so_q0 */
   __pkt_field short so_qlen;               /* number of connections on so_q */
   __pkt_field short so_qlimit;             /* max number queued connections */
   __pkt_field unsigned short so_error;     /* error affecting connection */
   unsigned int so_oobmark;                 /* chars to oob mark */

  //-- Variables for socket buffering.

  struct sockbuf  so_rcv;
  struct sockbuf  so_snd;
//------------------------------

  TN_SOCKETEX * socket_ex;
//-------------------------------

 //  __pkt_field struct protosw so_proto; //-- My

}__pkt_struct_ex;

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(pop)
#endif

#define   SB_MAX           (256*1024)        /* default for max chars in sockbuf */
#define   SB_LOCK               0x01         /* lock on data queue */
#define   SB_WANT               0x02         /* someone is waiting to lock */
#define   SB_WAIT               0x04         /* someone is waiting for data/space */
#define   SB_SEL                0x08         /* someone is selecting */
#define   SB_ASYNC              0x10         /* ASYNC I/O, need signals */
#define   SB_NOTIFY            (SB_WAIT|SB_SEL|SB_ASYNC)
#define   SB_NOINTR             0x40         /* operations not interruptible */

/*
 * Socket state bits.
 */

#define   SS_NOFDREF            0x001        /* no file table ref any more */
#define   SS_ISCONNECTED        0x002        /* socket connected to a peer */
#define   SS_ISCONNECTING       0x004        /* in process of connecting to peer */
#define   SS_ISDISCONNECTING    0x008        /* in process of disconnecting */
#define   SS_CANTSENDMORE       0x010        /* can't send more data to peer */
#define   SS_CANTRCVMORE        0x020        /* can't receive more data from peer */
#define   SS_RCVATMARK          0x040        /* at mark on input */

#define   SS_PRIV               0x080        /* privileged for broadcast, raw... */
#define   SS_NBIO               0x100        /* non-blocking ops */
#define   SS_ASYNC              0x200        /* async i/o notify */

//----------------------------------------------

#define   UIO_READ                  0
#define   UIO_WRITE                 1

struct iovec
{
   unsigned char * iov_base;
   int iov_len;
};

struct uio
{
   struct iovec * uio_iov;
   int uio_resid;    //-- total len
   int uio_rw;       // UIO_READ, UIO_WRITE
};

//----
#define   _FIONREAD                  1       // get num chars available to read
#define   _FIONBIO                  16       // set non-blocking socket

//----

#define   SHUT_RD                    0
#define   SHUT_WR                    1
#define   SHUT_RDWR                  2

#define   MAX_LINK_HEADER           16       //-- My

//----------------------------------------------------------------------------

/*
 * How much space is there in a socket buffer (so->so_snd or so->so_rcv)?
 * This is problematical if the fields are unsigned, as the space might
 * still be negative (cc > hiwat or mbcnt > mbmax).  Should detect
 * overflow and return 0.  Should use "lmin" but it doesn't exist now.
 */
/*
  #define        sbspace(sb) \
    ((long) _min((int)((sb)->sb_hiwat - (sb)->sb_cc), \
         (int)((sb)->sb_mbmax - (sb)->sb_mbcnt)))
*/

#define   sbspace(sb)       ( (sb)->sb_hiwat - (sb)->sb_cc )

//#define   sorwakeup(so)     tn_net_wakeup(&so->so_rcv.sb_sem)
#define   sowwakeup(so)     tn_net_wakeup(&so->so_snd.sb_sem)

void sorwakeup(struct socket_tcp * so);
//----

/*
 * Force a time value to be in a certain range.
 */
#define   TCPT_RANGESET(tv, value, tvmin, tvmax) { \
            (tv) = (value); \
            if((tv) < (tvmin)) \
               (tv) = (tvmin); \
            else if((tv) > (tvmax)) \
               (tv) = (tvmax); \
            }

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

void tcp_respond(TN_NET * tnet,
                 struct mbuf * mb,
                 struct tcpcb * tp,
                 unsigned char * t_template,
                 struct tcphdr * th0,
                 tcp_seq ack,
                 tcp_seq seq,
                 int flags);

void tcp_setpersist(struct tcpcb *tp);

//----------------------------------------------------------------------------

int sofree(TN_NET * tnet, void * so_in);
 int soclose(TN_NET * tnet, struct socket_tcp *so);
void soqinsque(TN_NET * tnet,
               struct socket_tcp * head,
               struct socket_tcp * so,
               int q);
 int soqremque(TN_NET * tnet, struct socket_tcp *so, int q);
// int sockargs(TN_NET * tnet, struct mbuf ** mp, unsigned char * buf, int buflen, int type);
 int soconnect(TN_NET * tnet, struct socket_tcp * so, struct mbuf * nam);
 int soshutdown(TN_NET * tnet, struct socket_tcp * so, int how);

 int sodisconnect(TN_NET *tnet, struct socket_tcp *so);
void sorflush(TN_NET *tnet, struct socket_tcp *so);
void socantrcvmore(TN_NET * tnet, struct socket_tcp *so);
void soisconnected(TN_NET * tnet, struct socket_tcp *so);
void soisconnecting(TN_NET * tnet, struct socket_tcp *so);
void soisdisconnected(TN_NET * tnet, struct socket_tcp *so);
void soisdisconnecting(TN_NET * tnet, struct socket_tcp *so);
void socantsendmore(TN_NET * tnet, struct socket_tcp *so);

struct socket_tcp * sonewconn(TN_NET * tnet, struct socket_tcp * head);

int sbreserve(TN_NET * tnet, struct sockbuf * sb, unsigned long cc);
void sbrelease(TN_NET * tnet, struct sockbuf *sb);
void sbflush(TN_NET * tnet, struct sockbuf *sb);
void sbappend(TN_NET * tnet, struct sockbuf * sb, struct mbuf *m);
void sbcompress(TN_NET * tnet,
                struct sockbuf * sb,
                struct mbuf * m,
                struct mbuf * n);
void sbdrop(TN_NET * tnet, struct sockbuf *sb, int len);

struct tcpcb * tcp_disconnect(TN_NET * tnet, struct tcpcb *tp);
int tcp_attach(TN_NET * tnet, struct socket_tcp *so);
int tcp_usrreq(TN_NET * tnet, struct socket_tcp * so, int mode, struct mbuf * nam);

void tn_socketex_release(TN_SOCKETEX * ssem);

//----------------------------------------------------------------------------

int tcp_s_socket(TN_NET * tnet, int domain, int type, int protocol);
int tcp_s_close(TN_NET * tnet, int s);
int tcp_s_accept(TN_NET * tnet,
                 int s,
                 struct _sockaddr * addr,
                 int * addrlen);
int tcp_s_bind(TN_NET * tnet, int s, const struct _sockaddr * name, int namelen);
int tcp_s_connect(TN_NET * tnet, int s, struct _sockaddr * name, int namelen);
int tcp_s_listen(TN_NET * tnet,
                 int s,
                 int backlog);
int tcp_s_recv(TN_NET * tnet,
               int s,
               unsigned char * buf,
               int nbytes,
               int flags);
int tcp_s_send(TN_NET * tnet,
               int  s,
               unsigned char * buf,
               int  nbytes,
               int  flags);
int tcp_s_getsockopt(TN_NET * tnet,
                     int  s,
                     int level,
                     int name,
                     unsigned char * val,
                     int * avalsize);
int tcp_s_setsockopt(TN_NET * tnet,
                     int s,
                     int level,
                     int name,
                     unsigned char * val,
                     int avalsize);
int  tcp_s_getpeername(TN_NET * tnet,
                       int s,
                       struct _sockaddr * name,
                       int * namelen);
int tcp_s_ioctl(TN_NET * tnet,
                int s,
                int cmd,
                void * data);
int tcp_s_shutdown(TN_NET * tnet, int s, int how);

  //--- tn_tcp_sockets2.c

int tn_tcp_check_avaliable_mem(TN_NET * tnet);
int tn_socket_tcp_release(TN_NET * tnet, struct socket_tcp * so);
struct socket_tcp * tn_socket_tcp_alloc(TN_NET * tnet, int wait);
TN_SOCKETEX * tn_socketex_create(TN_NET * tnet, int wait);
int soreserve(TN_NET * tnet,
              struct socket_tcp * so,
              unsigned long sndcc,
              unsigned long rcvcc);

#endif



