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
#include "tn_protosw.h"
#include "tn_tcp.h"
#include "tn_sockets_tcp.h"
#include "tn_in_pcb_func.h"
#include "tn_net_mem_func.h"

#include "dbg_func.h"

#if defined (__ICCARM__)   // IAR ARM
#pragma diag_suppress=Pa039
#endif

//-- BSD  related only

#define MSIZE            128
#define MCLBYTES        1024

//----------------------------------------------------------------------------
int tn_socket_tcp_release(TN_NET * tnet, struct socket_tcp * so)
{
   struct tn_socketex * so_ex;

   //-- Remove from the sockets list
   if(tnet->so_tcp_ex == so->socket_ex)
      tnet->so_tcp_ex = so->socket_ex->next;
   else
   {
      for(so_ex = tnet->so_tcp_ex; so_ex != NULL; so_ex = so_ex->next)
      {
         if(so_ex->next == so->socket_ex)
         {
            so_ex->next = so->socket_ex->next;
            break;
         }
      }
   }

//-- Checking the operation validity - inside tn_sem_delete()

   tn_sem_delete(&so->socket_ex->so_wait_sem);

   tn_net_free_min(tnet, (void*)so->socket_ex); //-- release ex memory

//---------

   tn_sem_delete(&so->so_rcv.sb_sem);
   tn_sem_delete(&so->so_snd.sb_sem);

   tn_net_free_mid1(tnet, so); //-- release socket mem

//---------
   return 0;
}

//----------------------------------------------------------------------------
int soconnect(TN_NET * tnet,
              struct socket_tcp * so,
              struct mbuf * nam)
{
   struct inpcb * inp;
   struct tcpcb * tp;
   int rc;
   int sm;

   inp = sotoinpcb(so);
   if(inp == NULL)
      return EINVAL;
   tp = intotcpcb(inp);
   if(tp == NULL)
      return EINVAL;

   if(so->so_options & SO_ACCEPTCONN)
      return EOPNOTSUPP;

   sm = splnet(tnet);

   //-- If protocol is connection-based, can only connect once.
   //-- Otherwise, if connected, try to disconnect first.
   //-- This allows user to disconnect by connecting to, e.g.,
   //-- a null address.

   for(;;) //-- Single iteration loop
   {
      if(so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING))
      {
         rc = sodisconnect(tnet, so);
         if(rc)
         {
            rc = EISCONN;
            break;
         }
      }

//--- PRU_CONNECT

 //  rc = tcp_usrreq(tnet, so, PRU_CONNECT, nam);
      if(inp->inp_lport == 0)
      {
         rc = in_pcbbind(tnet, inp, NULL);
         if(rc)
            break;
      }
      rc = in_pcbconnect(tnet, inp, nam);
      if(rc)
         break;

      tp->t_template = tcp_template(tnet, tp);
      if(tp->t_template == NULL)
      {
         in_pcbdisconnect(tnet, inp);
         rc = ENOBUFS;
         break;
      }

      soisconnecting(tnet, so);

     //tcpstat.tcps_connattempt++;

      tp->t_state = TCPS_SYN_SENT;
      tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
      tp->iss = tnet->tcp_iss;
      tnet->tcp_iss += TCP_ISSINCR/2;

      tcp_sendseqinit(tp);

      rc = tcp_output(tnet, tp);

      break; //-- Leave any case
   }

   splx(tnet, sm);

   return rc;
}

//----------------------------------------------------------------------------
void sorflush(TN_NET * tnet, struct socket_tcp *so)
{
   struct sockbuf * sb;
   struct sockbuf * sb_tmp;

   sb_tmp = (struct sockbuf *)tn_net_alloc_mid1(tnet, MB_WAIT);

   sb = &so->so_rcv;
   so->so_state |= SS_CANTRCVMORE;

   //-- save  so->so_rcv
   bcopy(sb, sb_tmp, sizeof(struct sockbuf));

 //-- 1. Release all objects waiting for 'so_rcv' semaphore
 //-- 2. Prevent sem TN_SEM memory address changing problem

   tn_sem_delete(&sb->sb_sem);
   sb_tmp->sb_sem.id_sem = 0;

 //------
   bzero((unsigned char *)sb, sizeof (struct sockbuf));

   sbrelease(tnet, sb_tmp); //--- use sb_rcv org data to release

   tn_net_free_mid1(tnet, (void*)sb_tmp);
}

//----------------------------------------------------------------------------
int sofree(TN_NET * tnet, void * so_in)
{
   struct socket_tcp * so = (struct socket_tcp *) so_in;

   if(so->so_state & SS_NOFDREF)
   {
    //-- To fully release all objects waiting for semaphores -
    //-- just delete semaphores !!!

      tn_sem_delete(&so->socket_ex->so_wait_sem);
      tn_sem_delete(&so->so_rcv.sb_sem);
      tn_sem_delete(&so->so_snd.sb_sem);

    //-- When sofree() is called from soclose()
    //-- it is possible to so->so_pcb != NULL (at the some late TCP states )
      if(so->so_pcb != NULL)
         return -1;
   }
   else if(so->so_pcb != NULL)
      return -1;

/*

   if(so->so_state & SS_NOFDREF)
   {
    //-- To fully release all objects waiting for semaphores -
    //-- just delete semaphores !!!

      tn_sem_delete(&so->socket_ex->so_wait_sem);
      tn_sem_delete(&so->so_rcv.sb_sem);
      tn_sem_delete(&so->so_snd.sb_sem);

    //-- When sofree() is called from soclose()
    //-- it is possible to so->so_pcb != NULL (at the some late TCP states )
      if(so->so_pcb != NULL)
         return -1;
   }
   else
      return -1;
*/

   if(so->so_head != NULL)
   {
      if(!soqremque(tnet, so, 0) && !soqremque(tnet, so, 1))
         tn_net_panic(TNP_SOFREE_DQ);

      so->so_head = NULL;
   }

   sbrelease(tnet, &so->so_snd);

   sorflush(tnet, so);

  //-- Free memory

   tn_socket_tcp_release(tnet, so);

   return 0;
}

//----------------------------------------------------------------------------
void soqinsque(TN_NET * tnet,
               struct socket_tcp * head,
               struct socket_tcp * so,
               int q)
{
   struct socket_tcp ** prev;

   so->so_head = head;
   if(q == 0)   //-- ins to 'so_q0' queue
   {
      head->so_q0len++;
      so->so_q0 = 0;
      for(prev = &(head->so_q0); *prev;)
         prev = &((*prev)->so_q0);
   }
   else     //-- ins to 'so_q' queue
   {
      head->so_qlen++;
      so->so_q = 0;
      for(prev = &(head->so_q); *prev;)
          prev = &((*prev)->so_q);
   }
   *prev = so;

}

//----------------------------------------------------------------------------
int soqremque(TN_NET * tnet, struct socket_tcp *so, int q)
{
   struct socket_tcp *head, *prev, *next;

   head = so->so_head;
   prev = head;

   for(;;)
   {
      next = q ? prev->so_q : prev->so_q0;
      if(next == so)
         break;
      if(next == 0)
         return (0);
      prev = next;
   }

   if(q == 0)
   {
      prev->so_q0 = next->so_q0;
      head->so_q0len--;
   }
   else
   {
      prev->so_q = next->so_q;
      head->so_qlen--;
   }

   next->so_q0 = next->so_q = 0;
   next->so_head = 0;

   return 1;
}

//----------------------------------------------------------------------------
void socantsendmore(TN_NET * tnet, struct socket_tcp *so)
{
   so->so_state |= SS_CANTSENDMORE;

   sowwakeup(so);
}

//----------------------------------------------------------------------------
void socantrcvmore(TN_NET * tnet, struct socket_tcp * so)
{
   so->so_state |= SS_CANTRCVMORE;

   sorwakeup(so);
}

//----------------------------------------------------------------------------
void soisconnected(TN_NET * tnet, struct socket_tcp *so)
{
   struct socket_tcp * head;

   head = so->so_head;
   so->so_state &= ~(SS_ISCONNECTING|SS_ISDISCONNECTING); //YVT |SS_ISCONFIRMING);
   so->so_state |= SS_ISCONNECTED;

   if(head && soqremque(tnet, so, 0))
   {
      soqinsque(tnet, head, so, 1);
      sorwakeup(head);

      tn_net_wakeup(&head->socket_ex->so_wait_sem);
   }
   else
   {
      tn_net_wakeup(&so->socket_ex->so_wait_sem);

      sorwakeup(so);
      sowwakeup(so);
   }
}

//----------------------------------------------------------------------------
void soisdisconnected(TN_NET * tnet, struct socket_tcp *so)
{
   so->so_state &= ~(SS_ISCONNECTING|SS_ISCONNECTED|SS_ISDISCONNECTING);
   so->so_state |= (SS_CANTRCVMORE|SS_CANTSENDMORE);

   tn_net_wakeup(&so->socket_ex->so_wait_sem);

   sowwakeup(so);
   sorwakeup(so);
}

//----------------------------------------------------------------------------
int sodisconnect(TN_NET * tnet, struct socket_tcp *so)
{
   struct inpcb * inp = NULL;
   struct tcpcb * tp = NULL;

   inp = sotoinpcb(so);
   if(inp == NULL)
      return EINVAL;
   tp = intotcpcb(inp);
   if(tp == NULL)
      return EINVAL;

   if((so->so_state & SS_ISCONNECTED) == 0)
      return ENOTCONN;

   if(so->so_state & SS_ISDISCONNECTING)
      return EALREADY;

  //-- PRU_DISCONNECT

   tcp_disconnect(tnet, tp);

   return 0;
}

//----------------------------------------------------------------------------
void soisconnecting(TN_NET * tnet, struct socket_tcp *so)
{
   so->so_state &= ~(SS_ISCONNECTED|SS_ISDISCONNECTING);
   so->so_state |= SS_ISCONNECTING;
}

//----------------------------------------------------------------------------
void soisdisconnecting(TN_NET * tnet, struct socket_tcp *so)
{
   so->so_state &= ~SS_ISCONNECTING;
   so->so_state |= (SS_ISDISCONNECTING | SS_CANTRCVMORE | SS_CANTSENDMORE);

   tn_net_wakeup(&so->socket_ex->so_wait_sem);

   sowwakeup(so);
   sorwakeup(so);
}

//----------------------------------------------------------------------------
int soreserve(TN_NET * tnet,
              struct socket_tcp * so,
              unsigned long sndcc,
              unsigned long rcvcc)
{

   if(sbreserve(tnet, &so->so_snd, sndcc) == 0)
      goto bad;

   if(sbreserve(tnet, &so->so_rcv, rcvcc) == 0)
      goto bad2;

   if(so->so_rcv.sb_lowat == 0)
      so->so_rcv.sb_lowat = 1;

   if(so->so_snd.sb_lowat == 0)
      so->so_snd.sb_lowat = MCLBYTES;

   if(so->so_snd.sb_lowat > (long)so->so_snd.sb_hiwat)
      so->so_snd.sb_lowat = so->so_snd.sb_hiwat;

   return 0; //-- O.K.

bad2:

   sbrelease(tnet, &so->so_snd);

bad:

   return ENOBUFS;
}

//----------------------------------------------------------------------------
// Must be called at splnet()
//----------------------------------------------------------------------------
struct socket_tcp * sonewconn(TN_NET * tnet, struct socket_tcp * head)
{
   struct socket_tcp * so;
   int rc;

   if(head->so_qlen + head->so_q0len > 3 * head->so_qlimit / 2)
      return NULL;

   so = tn_socket_tcp_alloc(tnet, MB_NOWAIT);  //--- Create sem - inside
   if(so == NULL)
      return NULL;


   //-- Clone

   so->so_type    = head->so_type;
   so->so_options = head->so_options &~ SO_ACCEPTCONN;
   so->so_linger  = head->so_linger;
   so->so_state   = head->so_state | SS_NOFDREF;
   //so->so_proto   = head->so_proto;

   soreserve(tnet, so, head->so_snd.sb_hiwat, head->so_rcv.sb_hiwat);
   soqinsque(tnet, head, so, 0); //-- insert to so_q0

   rc = tcp_attach(tnet, so); //-- PRU_ATTACH
   if(rc)
   {
      soqremque(tnet, so, NULL);
      tn_socket_tcp_release(tnet, so);
      return NULL;
   }

   if((so->so_options & SO_LINGER) && so->so_linger == 0)
       so->so_linger = TCP_LINGERTIME;

   return so;
}


//----------------------------------------------------------------------------
//    sb(socket buffer) routines
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Free mbufs held by a socket, and reserved mbuf space.
//----------------------------------------------------------------------------
void sbrelease(TN_NET * tnet, struct sockbuf * sb)
{
   sbflush(tnet, sb);
   sb->sb_hiwat = 0;
   //YVT sb->sb_mbmax = 0;
}

//----------------------------------------------------------------------------
// Free all mbufs in a sockbuf.
// Check that all resources are reclaimed.
//----------------------------------------------------------------------------
void sbflush(TN_NET * tnet, struct sockbuf *sb)
{
  //YVT while(sb->sb_mbcnt)
   sbdrop(tnet, sb, (int)sb->sb_cc);

   if(sb->sb_cc || sb->sb_mb)
      tn_net_panic(TNP_SBGLUSH_1);
}

//----------------------------------------------------------------------------
// Drop data from (the front of) a sockbuf.
//----------------------------------------------------------------------------
void sbdrop(TN_NET * tnet, struct sockbuf * sb, int len)
{
   struct mbuf *m, *mn;
   struct mbuf *next;

   //next = (m = sb->sb_mb) ? m->m_nextpkt : NULL;

   m = sb->sb_mb;
   if(m)
      next = m->m_nextpkt;
   else
      next = NULL;

   while(len > 0)
   {
      if(m == NULL)
      {
         if(next == NULL)
            tn_net_panic(TNP_SBDROP_1);
         m = next;
         next = m->m_nextpkt;
         continue;
      }

      if(m->m_len > len)
      {
         m->m_len  -= len;
         m->m_data += len;
         sb->sb_cc -= len;
         break;
      }

      len -= m->m_len;
      sb->sb_cc -= m->m_len;

      mn = m_free(tnet, m);
      if(mn == INV_MEM)
         tn_net_panic(INV_MEM_3);
      m = mn;
   }

   while(m && m->m_len == 0)
   {
      sb->sb_cc -= m->m_len;

      mn = m_free(tnet, m);
      if(mn == INV_MEM)
         tn_net_panic(INV_MEM_4);
      m = mn;
   }

   if(m)
   {
      sb->sb_mb = m;
      m->m_nextpkt = next;
   }
   else
      sb->sb_mb = next;
}

//----------------------------------------------------------------------------
// Allot mbufs to a sockbuf.
// Attempt to scale mbmax so that mbcnt doesn't become limiting
// if buffering efficiency is near the normal case.
//----------------------------------------------------------------------------
int sbreserve(TN_NET * tnet, struct sockbuf * sb, unsigned long cc)
{
   if(cc > tnet->sb_max * MCLBYTES / (MSIZE + MCLBYTES))
     return 0;

   sb->sb_hiwat = cc;
//YVT   sb->sb_mbmax = _min(cc * 2, sb_max);
   if(sb->sb_lowat > (long)sb->sb_hiwat)
      sb->sb_lowat = sb->sb_hiwat;

   return 1;
}

//----------------------------------------------------------------------------
// Append mbuf chain 'm' to the last record in the socket buffer 'sb'.
//
// The additional space associated
// the mbuf chain is recorded in sb.  Empty mbufs are
// discarded and mbufs are compacted where possible.
//----------------------------------------------------------------------------
void sbappend(TN_NET * tnet, struct sockbuf * sb, struct mbuf *m)
{
   struct mbuf * n;

   if(m == NULL || sb == NULL)
      return;

   n = sb->sb_mb;
   if(n)
   {
      while(n->m_nextpkt)
      {
         n = n->m_nextpkt;
         if(n == NULL)
            break;
      }
      while(n->m_next) //-- YVT  //  while(n->m_next && (n = n->m_next));
      {
         n = n->m_next;
         if(n == NULL)
            break;
      }
   }

   sbcompress(tnet, sb, m, n);
}

//----------------------------------------------------------------------------
// Compress mbuf chain m into the socket buffer 'sb' following mbuf 'n'.
// If 'n' is null, the buffer is presumed empty.
//----------------------------------------------------------------------------
void sbcompress(TN_NET * tnet,
                struct sockbuf * sb,
                struct mbuf * m,
                struct mbuf * n)
{

#define MAX_COPY_SIZE   (128/2)

   struct mbuf * p_databuf_hdr;
   int dbuf_size;

   struct mbuf * mb;

//-----------------------------

#ifdef  EXP_RX_BUF  //-- A rx buffers (1536 bytes each) are "very expensive"

   //-- If buf is hi(Eth mem pool), copy it to regular mid1 bufs and free

   if(m)
   {
      if((m->m_flags & M_DBHDR) && m->m_dbtype == MB_HI)
      {
         mb = m_hi_to_mid1(tnet, m);
         if(mb != NULL) //-- free hi buf here
         {
            if(m_freem(tnet, m) == INV_MEM_VAL)
               tn_net_panic(INV_MEM_VAL_25);
            m = mb;
         }
      }
   }
#endif

//-----------------------------

   while(m)
   {
//if(m == n)
//{
//  dbg_send("m==n \r\n");
//}
      if(m->m_len <= 0) //-- 'm' buf is empty
      {
         m = m_free(tnet, m);
         if(m == INV_MEM)
            tn_net_panic(INV_MEM_VAL_5);

         continue;
      }

     //-- Copy conditions

      if(n != NULL && m->m_len < MAX_COPY_SIZE) //-- The 'm' contents is not too big one
      {
        //-- Get 'n' max avaliable space

         if(n->m_flags & M_DBHDR)    //-- A data buf header ?
            p_databuf_hdr = n;        //-- self
         else
            p_databuf_hdr = n->m_pri;

         if(p_databuf_hdr != NULL)   //-- sanity check
         {
            dbuf_size = m_get_dbuf_size(p_databuf_hdr->m_dbtype);

          //-- It is enought space in 'n' to fit all 'm' contents ?

          //-- Thanks to Audrius Urmanavicius for the bug fixing here

            if(n->m_len + m->m_len +
               (int)(n->m_data - n->m_dbuf) <=    //-- expected
                             dbuf_size)           //-- avaliable
            {
            //--  copy full 'm' contents to 'n'

               bcopy((unsigned char *)m->m_data,
                     (unsigned char *)n->m_data + n->m_len,
                      m->m_len);

               n->m_len  += m->m_len;
               sb->sb_cc += m->m_len;

               m = m_free(tnet, m);
               if(m == INV_MEM)
                  tn_net_panic(INV_MEM_VAL_6);

               continue;
            }
         }
      }

    //-- when here, m != NULL

      if(n)
         n->m_next = m;
      else
         sb->sb_mb = m;

      sb->sb_cc += m->m_len;
      n = m;

      m = m->m_next;
      n->m_next = NULL;
   }

}

//----------------------------------------------------------------------------
struct socket_tcp * tn_socket_tcp_alloc(TN_NET * tnet, int wait)
{
   struct socket_tcp * so;
   struct tn_socketex * p_so_ex;

   so = (struct socket_tcp *)tn_net_alloc_mid1(tnet, wait);
   if(so == NULL)
      return NULL;

   bzero_dw((unsigned int*)so, (TNNET_MID1_BUF_SIZE)>>2);

   so->socket_ex = (TN_SOCKETEX *) tn_net_alloc_min(tnet, wait);
   if(so->socket_ex == NULL)
   {
      tn_net_free_mid1(tnet, (void*)so);
      return NULL;
   }

   bzero_dw((unsigned int*)so->socket_ex, (TNNET_SMALL_BUF_SIZE)>>2);

  //----------
   so->socket_ex->so = so;
   so->sofree = sofree;

//-- Add to the socket list

   if(tnet->so_tcp_ex == NULL)
      tnet->so_tcp_ex = so->socket_ex;
   else
   {
      for(p_so_ex = tnet->so_tcp_ex; ; p_so_ex = p_so_ex->next)
      {
         if(p_so_ex->next == NULL)
         {
            p_so_ex->next = so->socket_ex;
            break;
         }
      }
   }

  //--- Create sem

   tn_sem_create(&so->so_rcv.sb_sem,
                 0, //1,   //-- Start value
                 1);  //-- Max value

   tn_sem_create(&so->so_snd.sb_sem,
                 1, //1,   //-- Start value
                 1);  //-- Max value

  //--- Socket ex

   tn_sem_create(&so->socket_ex->so_wait_sem,
                 0, //1,   //-- Start value
                 1);  //-- Max value

   return so;
}

//----------------------------------------------------------------------------
void dbg_conv_to_socket_tcp( int s)
{
//   static volatile struct socket_tcp * so;
//   so = (struct socket_tcp *)s;
}

//----------------------------------------------------------------------------
void sorwakeup(struct socket_tcp * so)
{
   tn_net_wakeup(&so->so_rcv.sb_sem);
}

//----------------------------------------------------------------------------
int tn_tcp_check_avaliable_mem(TN_NET * tnet)
{
   if(tnet->m1buf_mpool.fblkcnt < TCP_MIN_FREE_BUF_FOR_NEWCONN
               || tnet->lobuf_mpool.fblkcnt < (TCP_MIN_FREE_BUF_FOR_NEWCONN) - 2)
      return -1;
   return 0;  //-- OK
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

