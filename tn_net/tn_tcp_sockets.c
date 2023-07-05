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
#include "tn_protosw.h"
#include "bsd_socket.h"
#include "tn_socket.h"
#include "tn_net_utils.h"
#include "tn_tcp.h"
#include "tn_sockets_tcp.h"
#include "tn_net_func.h"
#include "tn_in_pcb_func.h"

#include "dbg_func.h"

#if defined (__ICCARM__)   // IAR ARM
#pragma diag_suppress=Pa039
#endif

//----------------------------------------------------------------------------
// tcp_socket() returns socket id(actually,socket address) on success.
//  On error, 0 is returned
//----------------------------------------------------------------------------
int tcp_s_socket(TN_NET * tnet, int domain, int type, int protocol)
{
   struct socket_tcp * so;
   int rc;
   int sm;

   if(domain != AF_INET)
      return -EINVAL;
   if(type != SOCK_STREAM)
      return -EINVAL;

   if(tn_tcp_check_avaliable_mem(tnet) != 0)
      return -EINVAL;

   so = tn_socket_tcp_alloc(tnet, MB_WAIT);
   if(so == NULL)
      return -EINVAL;

   so->so_type = type;

   sm = splnet(tnet);

   rc = tcp_attach(tnet, so);
   if(rc)
   {
      so->so_state |= SS_NOFDREF;
      sofree(tnet, so); //so->so_pcb = 0 here
      splx(tnet, sm);
      return -EINVAL;   //-- Error
   }

   if((so->so_options & SO_LINGER) && so->so_linger == 0)
      so->so_linger = TCP_LINGERTIME;

   splx(tnet, sm);

   return (int)so;
}

//----------------------------------------------------------------------------
int tcp_s_close(TN_NET * tnet, int s)
{
   struct socket_tcp * so;
   struct inpcb * inp;
   struct tcpcb * tp;
   int error;
   int sm;

   error = 0;

   so = (struct socket_tcp *)s;
   inp = sotoinpcb(so);
   if(inp == NULL)
      return -EINVAL;
   tp = intotcpcb(inp);
   if(tp == NULL)
      return -EINVAL;

   sm = splnet(tnet);

   if(so->so_state & SS_NOFDREF)
   {
      splx(tnet, sm);
      return 0;
   }

   if(so->so_options & SO_ACCEPTCONN)
   {
      while(so->so_q0)
         tcp_drop(tnet, tp, ECONNABORTED);      //-- PRU_ABORT

      while(so->so_q)
         tcp_drop(tnet, tp, ECONNABORTED);     //-- PRU_ABORT
   }


   if(so->so_pcb == NULL)
   {
      so->so_state |= SS_NOFDREF;
      sofree(tnet, so);
      splx(tnet, sm);
      return 0;
   }


   if(so->so_state & SS_ISCONNECTED)
   {
      if((so->so_state & SS_ISDISCONNECTING) == 0)
      {
         error = sodisconnect(tnet, so);
         if(error)
            goto drop;
      }

      if(so->so_options & SO_LINGER)
      {
         if((so->so_state & SS_ISDISCONNECTING) && (so->so_state & SS_NBIO))
            goto drop;

         while(so->so_state & SS_ISCONNECTED)
         {
     // ???
            splx(tnet, sm);
            tn_socket_wait(&so->socket_ex->so_wait_sem);
     // ??
            sm = splnet(tnet);
         }
      }
   }

drop:

   if(so->so_pcb != NULL)
   {
        //-- PRU_DETACH

      if(tp->t_state > TCPS_LISTEN)
         tcp_disconnect(tnet, tp);
      else
         tcp_close(tnet, tp);
   }

   so->so_state |= SS_NOFDREF;

   sofree(tnet, so);

   splx(tnet, sm);

   return  -error;
}

//----------------------------------------------------------------------------
int tcp_s_accept(TN_NET * tnet,
                 int s,                     // socket descriptor
                 struct _sockaddr * addr,   // peer address
                 int * addrlen)             // peer address length
{
   struct mbuf * nam;
   struct socket_tcp * so;
   struct inpcb * inp;
   int namelen;
   int sm;

   namelen = *addrlen;
   so = (struct socket_tcp *)s;
   inp = sotoinpcb(so);
   if(inp == NULL)
      return -EINVAL;

   sm = splnet(tnet);

   if((so->so_options & SO_ACCEPTCONN) == 0)
   {
      splx(tnet, sm);
      return -EINVAL;
   }

   if((so->so_state & SS_NBIO) && so->so_qlen == 0)
   {
      splx(tnet, sm);
      return -EINVAL;
   }

   while(so->so_qlen == 0 && so->so_error == 0)
   {
      if(so->so_state & SS_CANTRCVMORE)
      {
         so->so_error = ECONNABORTED;
         break;
      }
      splx(tnet, sm);

      tn_socket_wait(&so->socket_ex->so_wait_sem);

      sm = splnet(tnet);
   }

   if(so->so_error)
   {
      so->so_error = 0;
      splx(tnet, sm);
      return -EINVAL;
   }

   nam = mb_get(tnet, MB_MID1, MB_NOWAIT, FALSE); //-- Not from Tx pool
   //We can't block while owning spl
   if(nam == NULL)
   {
      //drops.accept++
      splx(tnet, sm);
      return -EINVAL;
   }

   {
      struct socket_tcp * aso;
      aso = so->so_q;
      if(soqremque(tnet, aso, 1) == 0)
         tn_net_panic(TNP_ACCEPT);
      aso->rx_timeout = so->rx_timeout;
      so = aso;
      inp = sotoinpcb(so);
   }

  //---- Start soaccept (so, nam);

   if((so->so_state & SS_NOFDREF) == 0)
      tn_net_panic(TNP_SOACCEPT_1);

   so->so_state &= ~SS_NOFDREF;

   //-- PRU_ACCEPT

   in_setpeeraddr(inp, nam);

  //----- End soaccept() ------------------------------

   if(addr)
   {
      if(namelen > nam->m_len)
         namelen = nam->m_len;
      bcopy(mtod(nam, unsigned char *), (unsigned char *)addr, namelen);
      bcopy((unsigned char *)&namelen, (unsigned char *)addrlen, sizeof (*addrlen));
   }

   if(m_freem(tnet, nam) == INV_MEM_VAL)
      tn_net_panic(INV_MEM_VAL_1);

   splx(tnet, sm);

   return (int)so;
}

//----------------------------------------------------------------------------
int tcp_s_bind(TN_NET * tnet, int s, const struct _sockaddr * name, int namelen)
{
   struct mbuf * nam;
   int rc;
   struct socket_tcp * so;
   struct inpcb * inp;
   int sm;

   if(s == 0 || s== -1)
      return -EINVAL;

   so = (struct socket_tcp *)s;
   inp = sotoinpcb(so);
   if(inp == NULL)
      return -EINVAL;

   rc = sockargs(tnet,
                 (struct mbuf **)&nam,
                 (unsigned char *) name,
                 namelen,
                 MT_SONAME);
   if(rc)
      return -EINVAL;

   //-- PRU_BIND
   sm = splnet(tnet);

   rc = in_pcbbind(tnet, inp, nam);

   splx(tnet, sm);
  //-----
   if(rc)
   {
      if(m_freem(tnet, (struct mbuf *)nam) == INV_MEM_VAL)
         tn_net_panic(INV_MEM_VAL_16);

      return -EINVAL;
   }

   if(m_freem(tnet, (struct mbuf *)nam) == INV_MEM_VAL)
      tn_net_panic(INV_MEM_VAL_17);

   return 0; //-- O.K.
}

//----------------------------------------------------------------------------
int tcp_s_connect(TN_NET * tnet, int s, struct _sockaddr * name, int namelen)
{
   struct socket_tcp * so;
   struct mbuf * nam;
   int sm;
   int rc;

   so = (struct socket_tcp *)s;
   if(so == NULL)
      return -EINVAL;

   //-- For non-blocking sockets, exit immediately if a previous connection
   //-- attempt is still pending.

   sm = splnet(tnet);

   if((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING))
   {
      splx(tnet, sm);
      return -1;
   }
   splx(tnet, sm);

   rc = sockargs(tnet, &nam, (unsigned char *)name, namelen, MT_SONAME);
   if(rc)
      return -1;

   //-- Attempt to establish the connection. For TCP sockets, this routine
   //-- just begins the three-way handshake process. For all other sockets,
   //-- it succeeds or fails immediately.

   rc = soconnect(tnet, so, nam);
   if(rc)
   {
   //-- Fatal error: unable to record remote address or (TCP only)
   //-- couldn't send initial SYN segment.
      so->so_state &= ~SS_ISCONNECTING;

      if(m_freem(tnet, nam) == INV_MEM_VAL)
         tn_net_panic(INV_MEM_VAL_18);

      return -EINVAL;
   }

   //-- For non-blocking sockets, exit if the connection attempt is
   //-- still pending. This condition only occurs for TCP sockets if
   //-- no SYN reply arrives before the soconnect() routine completes.

   sm = splnet(tnet);

   if((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING))
   {
      if(m_freem(tnet, nam) == INV_MEM_VAL)
         tn_net_panic(INV_MEM_VAL_19);

      splx(tnet, sm);
      return -EINVAL;
   }

   //-- Wait until a pending connection completes or a timeout occurs.
   //-- This delay might occur for TCP sockets. Other socket types
   //-- "connect" or fail instantly.

   while((so->so_state & SS_ISCONNECTING) && so->so_error == 0)
   {
      splx(tnet, sm);
      tn_socket_wait(&so->socket_ex->so_wait_sem);
      sm = splnet(tnet);
   }

   if(so->so_error)
   {
        // Connection attempt failed immediately or (TCP only) timed out.
      rc = -1;
      so->so_error = 0;
   }
   else
      rc = 0; //-- o.k.

   splx(tnet, sm);

//dbg_send("s_connect() - OK.\r\n");

   if(m_freem(tnet, nam) == INV_MEM_VAL)
      tn_net_panic(INV_MEM_VAL_20);

   return rc;
}

//----------------------------------------------------------------------------
int tcp_s_listen(TN_NET * tnet,
                 int s,
                 int backlog)        // number of connections to queue
{
   int rc;
   struct socket_tcp *so;
   struct inpcb * inp;
   struct tcpcb * tp;
   int sm;


   so = (struct socket_tcp *)s;
   inp = sotoinpcb(so);
   if(inp == NULL)
      return -EINVAL;

   rc = 0;
  //-- PRU_LISTEN

   sm = splnet(tnet);

   if(inp->inp_lport == 0)
      rc = in_pcbbind(tnet, inp, NULL);
   if(rc == 0)
   {
      tp = intotcpcb(inp);
      if(tp == NULL)
         return -EINVAL;
      tp->t_state = TCPS_LISTEN;
   }
 //---------------------

   if(rc)
   {
      splx(tnet, sm);
      return -rc;
   }

   if(so->so_q == 0)
      so->so_options |= SO_ACCEPTCONN;
   if(backlog < 0)
      backlog = 0;

   so->so_qlimit = _min(backlog, SOMAXCONN);

   splx(tnet, sm);

   return 0; //-- O.K
}

//----------------------------------------------------------------------------
int tcp_s_recv(TN_NET * tnet,
               int s,
               unsigned char * buf,   // buffer to write data to
               int nbytes,            // length of buffer
               int flags)
{

   struct socket_tcp * so;
   unsigned char * ptr;
   int tsize;
   int len = 0;
   struct mbuf ** mp;
   struct mbuf * m;
   struct mbuf * nextrecord;
   int sm;

//dbg_pin_on(P1_29_MASK);

   so = (struct socket_tcp *)s;
   if(so == NULL)
      return -EINVAL;
   if(nbytes <= 0)
      return -EINVAL;
   if(buf == NULL)
      return -EINVAL;

   if(flags & MSG_ZEROCOPY)  //-- if zero copy
      mp = (struct mbuf**)buf;
   else
      mp = NULL;

//---------------------------------
   for(;;)
   {
      sm = splnet(tnet);

    //--- Check - exit or wait/ready to tx

      m = so->so_rcv.sb_mb;

      if(so->so_state & SS_CANTRCVMORE)
      {
         if(m)
            break;
         else
         {
            splx(tnet, sm);
            return 0;
         }
      }

      if((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0)
      {
         splx(tnet, sm);
         return  -ENOTCONN;
      }
      if(m)
         break;
      else
      {
         splx(tnet, sm);
         if (tn_net_wait(&so->so_rcv.sb_sem, so->rx_timeout) != 0)
           return -ETIMEDOUT;
      }
   }

  //--- if we are here - we have some data in the 'so_rcv'

   ptr = buf;
   tsize = nbytes;

   nextrecord = m->m_nextpkt;

   while(m && nbytes)
   {
      len = _min(nbytes, m->m_len);

        //--- Do copy

      bcopy(m->m_data, ptr, len);
      ptr += len;
      nbytes -= len;

    //  sm = splnet(tnet);

      if(len == m->m_len)  //-- the overall mbuf was copied - free it now
      {
         nextrecord = m->m_nextpkt;
         so->so_rcv.sb_cc -= m->m_len;

         if(mp)     // if zero copy interface enabled
         {
            *mp = m;
             mp = &m->m_next;
             m = m->m_next;
             so->so_rcv.sb_mb = m;
             *mp = NULL;
         }
         else
         {

            so->so_rcv.sb_mb = m_free(tnet, m);
            if(so->so_rcv.sb_mb == INV_MEM)
               tn_net_panic(INV_MEM_2);
            m = so->so_rcv.sb_mb;
/*
            so->so_rcv.sb_mb = m->m_next;

            if(mb_rel == NULL)
            {
               mb_rel = m;

               mb_rel_last = m;
               mb_rel_last->m_next = NULL;
            }
            else
            {
               mb_rel_last->m_next = m;
               mb_rel_last = mb_rel_last->m_next;
               mb_rel_last->m_next = NULL;
            }
*/
         }
         if(m)
            m->m_nextpkt = nextrecord;
      }
      else  // not an overall mbuf, just part of mbuf data
      {
         if(mp)        // if zero copy interface enabled
         {
            *mp = m_copym(tnet, m, 0, len, MB_WAIT);
            if(*mp == NULL)
            {
               splx(tnet,sm);
               return -ENOBUFS;
            }
         }

         m->m_data += len;
         m->m_len -= len;

         so->so_rcv.sb_cc -= len;
      }
      if(m == NULL)
         so->so_rcv.sb_mb = nextrecord;
   }

   tcp_output(tnet, intotcpcb(sotoinpcb(so)));

   splx(tnet,sm);

//dbg_pin_off(P1_29_MASK);

   return (tsize - nbytes);
}

//----------------------------------------------------------------------------
int tcp_s_send(TN_NET * tnet,
               int  s,
               unsigned char * buf,  // pointer to buffer to transmit
               int  nbytes,          // num bytes to transmit
               int  flags)
{
   struct socket_tcp * so;
   int sm;
   int len = 0;
   int tsize;
   int space;
   int error;
   int fExit;
   unsigned char * ptr;
   struct tcpcb * tp;

   struct mbuf * mb    = NULL;
   struct mbuf * mb_tx;

   so = (struct socket_tcp *)s;
   if(so == NULL)
      return -EINVAL;
   if(nbytes <= 0)
      return -EINVAL;

   ptr = buf;
   mb_tx = NULL;
   tsize = nbytes;  //-- to save
   error = EINVAL;

   for(;;)
   {
    //--- Check - exit or wait/ready to tx

      sm = splnet(tnet);

      if(so->so_state & SS_CANTSENDMORE)
      {
         error = EPIPE;
         break;
      }
      if(so->so_error)
      {
         error = so->so_error;
         break;
      }

      if((so->so_state & SS_ISCONNECTED) == 0)
      {
         error = ENOTCONN;
         break;
      }

      space = sbspace(&so->so_snd);
//      if(space < nbytes &&
//         ((flags & MSG_ZEROCOPY) == 0) && //-- Not zero-copy
//         (space < so->so_snd.sb_lowat || space < 0))
      if(space <= 0)
      {
         if(so->so_state & SS_NBIO)
         {
            error = EWOULDBLOCK;
            break;
         }

         splx(tnet, sm);

         tn_net_wait(&so->so_snd.sb_sem, 0);

         continue;
      }

    //---- If here - do tx (still a hold splnet())

      fExit = FALSE;

      for(;;)
      {
         if(flags & MSG_ZEROCOPY) //-- Zero-copy (no allocation, just send)
         {
            if(mb_tx == NULL) //-- first
               mb_tx = mb;
            else
               mb_tx = mb_tx->m_next;

            if(mb_tx == NULL)   //-- Not expected here
            {
               error = EINVAL;
               fExit = TRUE;
               break;
            }
         }
         else //-- Allocate mbuf, copy user data and send a single mbuf at a time
         {
//-------------------------------------------------------------------------------
            len = _min(128/*TCP_MSS*/, nbytes);
            mb_tx = buf_to_mbuf(tnet, ptr, len, TRUE); //-- From Tx pool
            if(mb_tx == NULL)
            {
               error = ENOBUFS;
               fExit = TRUE;
               break;
            }
            space -= len;
            ptr   += len;
         }
//--------------------------------------------------

         tp = intotcpcb(sotoinpcb(so));
         if(flags & MSG_OOB)
         {
            if((int)sbspace(&so->so_snd) < -512)
            {
               if(m_freem(tnet, mb_tx) == INV_MEM_VAL)
                  tn_net_panic(INV_MEM_VAL_32);

               error = ENOBUFS;
            }
            else
            {

         //-- According to RFC961 (Assigned Protocols),
         //-- the urgent pointer points to the last octet
         //-- of urgent data.  We continue, however,
         //-- to consider it to indicate the first octet
         //-- of data past the urgent section.
         //-- Otherwise, snd_up should be one lower.

               sbappend(tnet, &so->so_snd, mb_tx);
               tp->snd_up = tp->snd_una + so->so_snd.sb_cc;

               tp->t_force = 1;
               error = tcp_output(tnet, tp);
               tp->t_force = 0;

               nbytes -= len;
            }
         }
         else
         {
            sbappend(tnet, &so->so_snd, mb_tx);
            error = tcp_output(tnet, tp);

            nbytes -= len;
         }

//--------------------------------------------------

         if(error || nbytes <= 0 || space <= 0)
         {
            fExit = TRUE;
            break;
         }
      }

      if(fExit || nbytes <= 0)
         break;
   }

   splx(tnet, sm);

   if(error)
   {
      if(tsize != nbytes && (error == EINTR || error == EWOULDBLOCK))
         error = 0;  //-- O.K.
   }

   if(error == 0) //-- O.K.
      return (tsize - nbytes);

   return -error;
}

//----------------------------------------------------------------------------
int  tcp_s_getpeername(TN_NET * tnet,
                       int s,
                       struct _sockaddr * name,
                       int * namelen)
{
   struct socket_tcp * so;
   struct inpcb * inp;
   struct mbuf * m;
   int rc = 0;

   so = (struct socket_tcp *)s;
   inp = sotoinpcb(so);
   if(inp == NULL)
      return -EINVAL;


   if((so->so_state & SS_ISCONNECTED) == 0)
      return -ENOTCONN;

   m = mb_get(tnet, MB_MID1, MB_WAIT, FALSE);  //-- Not from TX pool
   if(m == NULL)
      return -ENOBUFS;
   bzero(mtod(m, unsigned char *), TNNET_MID1_BUF_SIZE);

   in_setpeeraddr(inp, m);  //-- PRU_PEERADDR

   if(*namelen > m->m_len)
      *namelen = m->m_len;

   bcopy(mtod(m, unsigned char *), (unsigned char *)name, *namelen);

   if(m_freem(tnet, m) == INV_MEM_VAL)
      tn_net_panic(INV_MEM_VAL_22);

   return rc;
}

//----------------------------------------------------------------------------
int tcp_s_ioctl(TN_NET * tnet,
                int s,
                int cmd,
                void * data)
{
   struct socket_tcp * so;
   int val;
   int sm;

   so = (struct socket_tcp *)s;

   switch(cmd)
   {
      case _FIONBIO:

         bcopy(data, &val, sizeof(int)); //-- safe for any 'data' aligment

         sm = splnet(tnet);

         if(val)
            so->so_state |= SS_NBIO;
         else
            so->so_state &= ~SS_NBIO;

         splx(tnet, sm);

         return 0; //-- OK

      case _FIONREAD:

         val = so->so_rcv.sb_cc;
         bcopy(&val, data, sizeof(int)); //-- safe for any 'data' aligment

         return 0; //-- OK
   }

   return -EINVAL; //-- Err
}

//----------------------------------------------------------------------------
int tcp_s_shutdown(TN_NET * tnet, int s, int how)
{
   struct inpcb * inp;
   struct tcpcb * tp;
   struct socket_tcp * so;
   int sm;
   int rc;

   if(!(how == SHUT_RD || how == SHUT_WR || how == SHUT_RDWR))
      return -EINVAL;

   if(s == -1 || s == 0)
      return -EINVAL;

   so = (struct socket_tcp *)s;

   inp = sotoinpcb(so);
   if(inp == NULL)
      return -EINVAL;
   tp = intotcpcb(inp);
   if(tp == NULL)
      return -EINVAL;

   how++;
   rc = 0;

   sm = splnet(tnet);

   if(how & 1)    //-- FREAD == 1
      sorflush(tnet, so);

   if(how & 2)   //-- FWRITE == 2
   {
     //-- PRU_SHUTDOWN

      socantsendmore(tnet, so);
      tp = tcp_usrclosed(tnet, tp);
      if(tp)
         rc = tcp_output(tnet, tp);
   }

   splx(tnet, sm);

   return rc;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------







