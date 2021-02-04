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
#include "ip_icmp.h"
#include "tn_udp.h"
#include "tn_net_func.h"
#include "tn_in_pcb_func.h"
#include "tn_net_mem_func.h"

//----------------------------------------------------------------------------
static int udp_sofree_func(TN_NET * tnet, void * so)
{
   struct udp_socket * so_udp;
   volatile TN_MBUF * mb;
   int rc;

   so_udp = (struct udp_socket *)(((struct socket*)so)->so_tdata);
   if(so_udp == NULL)
      return -EPERM;

 //-- so->so_pcb - do not free !!!

 //-- Free all mbuf in queueRx

   for(;;)
   {
      rc = tn_queue_receive_polling(&so_udp->queueRx, (void**)&mb);
      if(rc == TERR_NO_ERR)
      {
         if(mb)
            m_freem(tnet, (TN_MBUF *)mb);
      }
      else
         break;
   }

 //-- Delete rx queue

   so_udp->queueRx.id_dque = 0;

   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
static void udp_detach(TN_NET * tnet, struct inpcb * inp)
{
   if(inp == tnet->udp_last_inpcb)
       tnet->udp_last_inpcb = &tnet->udb;
   in_pcbdetach(tnet, inp);  //-- free socket mem & pcb mem inside
}

//----------------------------------------------------------------------------
int udp_s_socket(TN_NET * tnet, int domain, int type, int protocol)
{
   struct socket * so;
   struct udp_socket * so_udp;
   int rc;
   int sm;

   sm = splnet(tnet);

   so = (struct socket *)tn_net_alloc_mid1(tnet, MB_WAIT);
   if(so == NULL)
   {
      splx(tnet, sm);
      return -1;  //-- Err
   }

   s_memset(so, 0, TNNET_MID1_BUF_SIZE); //--

   so->so_type     = type;
   so->sofree      = udp_sofree_func;
   so->rx_timeout  = TN_WAIT_INFINITE;

   //-- Alloc mem for pcb, put it to the pcb queue, fill so->so_pcb

   rc = in_pcballoc(tnet, so, &tnet->udb);
   if(rc) //--Error
   {
      tn_net_free_mid1(tnet, (void*)so);
      splx(tnet, sm);

      return -1;  //-- Err
   }

   //-- 'so_udp' is placed at the same memory block

   so->so_tdata = (void*)((unsigned char *)so + sizeof(struct socket));

   so_udp = (struct udp_socket *)so->so_tdata;

   //-- Check memory size validity in socket memory block(sanity???)

   rc = sizeof(struct socket) + sizeof(TN_DQUE) + sizeof(so_udp->queueRxMem);
   if(rc > TNNET_MID1_BUF_SIZE) //-- Fatal error
   {
      udp_detach(tnet, (struct inpcb *)so->so_pcb); // free socket mem & pcb mem inside
      tn_net_free_mid1(tnet, (void*)so);
      splx(tnet, sm);

      return -1;  //-- Err
   }

   //--- Create UDP socket RX queue.

   tn_queue_create(&so_udp->queueRx,
                   (void**)&so_udp->queueRxMem[0],
                    UDP_SOCK_RX_QUEUE_SIZE);
   splx(tnet, sm);

   return (int)so;
}

//----------------------------------------------------------------------------
int udp_s_close(TN_NET * tnet, struct socket * so)
{
   struct inpcb * inp;
   int sm;

   inp = (struct inpcb *)so->so_pcb;
   if(inp == NULL)
      return -EFAULT;

   sm = splnet(tnet);

   udp_detach(tnet, inp);              //-- free socket mem & pcb mem inside
   tn_net_free_mid1(tnet, (void*)so);  //-- free socket mem block

   splx(tnet, sm);

   return 0;   //-- OK
}

//----------------------------------------------------------------------------
int udp_s_connect(TN_NET * tnet,
                  struct socket * so,
                  struct _sockaddr * name,
                  int namelen)
{
   struct inpcb * inp;
   volatile struct mbuf * nam;
   int sm;
   int rc;

   if(name == NULL || namelen <= 0)
      return -EINVAL;

   inp = (struct inpcb *)so->so_pcb;
   if(inp == NULL)
      return -EPERM;

   rc = sockargs(tnet, (struct mbuf **)&nam, (unsigned char *)name, namelen, MT_SONAME);
   if(rc)
      return -rc;

   sm = splnet(tnet);

   //-- If connected, try to disconnect first.
   //-- This allows user to disconnect by connecting to, e.g., a null address.

   if(so->so_state & SS_ISCONNECTED)
   {
      //-- disconnect

      in_pcbdisconnect(tnet, inp);
      inp->inp_laddr.s__addr = _INADDR_ANY;
      so->so_state &= ~SS_ISCONNECTED;
   }

   if(inp->inp_faddr.s__addr != _INADDR_ANY)
   {
      m_freem(tnet, (struct mbuf *)nam);
      splx(tnet, sm);
      return -EISCONN;
   }

   rc = in_pcbconnect(tnet, inp, (struct mbuf *)nam);   //-- if not binded,  in_pcbbind() inside
   if(rc == 0) //-- O.K.
      so->so_state |= SS_ISCONNECTED;

   splx(tnet, sm);

   m_freem(tnet, (struct mbuf *)nam);

   if(rc > 0)
      rc = -rc;
   return rc;
}

//----------------------------------------------------------------------------
int udp_s_bind(TN_NET * tnet,
               struct socket * so,
               const struct _sockaddr * name,
               int namelen)
{
   struct inpcb * inp;
   volatile struct mbuf * nam;
   int sm;
   int rc;

   inp = (struct inpcb *)so->so_pcb;
   if(inp == NULL)
      return -EPERM;

   rc = sockargs(tnet, (struct mbuf **)&nam, (unsigned char *)name, namelen, MT_SONAME);
   if(rc)
      return -rc;

   sm = splnet(tnet);
   rc = in_pcbbind(tnet, inp, (struct mbuf *)nam);
   splx(tnet, sm);

   m_freem(tnet, (struct mbuf *)nam);

   if(rc != 0) //-- Err
      return -rc;
   return 0;  //-- O.K.
}

//----------------------------------------------------------------------------
int udp_s_recvfrom(TN_NET * tnet,
                   struct socket * so,
                   unsigned char *  buf,
                   int  len,
                   int  flags,
                   struct _sockaddr * from, // unsigned char *  from,
                   int * fromlenaddr)
{

   struct udp_socket * so_udp;
   unsigned char * ptr;
   TN_MBUF * mb;
   TN_MBUF * mb0;
   struct sockaddr__in * sin;
   struct udpiphdr * ui;

   int rc;
   int rx_len;
   int rd_len;
   int nbytes;
   int udp_msg_len;
   int bytes_to_copy;

   so_udp = (struct udp_socket *)so->so_tdata;
   if(so_udp == NULL)
      return -EINVAL;

   if(so->so_state & SS_NBIO)  //-- Non-blocking socket
   {
      rc = tn_queue_receive_polling(&so_udp->queueRx, (void**)&mb);
      if(rc != TERR_NO_ERR)  //-- Rx queue is empty
         return -EWOULDBLOCK;
   }
   else //-- Blocking socket
   {
      rc = tn_queue_receive(&so_udp->queueRx, (void**)&mb, so->rx_timeout);
      if(rc != TERR_NO_ERR) //-- Timeout
         return -ETIMEOUT;
   }

  //--- If mb == 0xFFFFFFF/0xFFFFFFFE, this is a signal, not a data

   if((unsigned int)mb == DHCP_T1_EXPIRED_EVENT) 
      return -EDHCPT1;
   else if((unsigned int)mb == DHCP_T2_EXPIRED_EVENT) 
      return -EDHCPT2;
   else if(mb == NULL)
      return -ENOTDATA;

  //---- If here - transfer data/addr to user

   ui = (struct udpiphdr *)mb->m_data;
   udp_msg_len = ntohs((unsigned short)ui->ui_ulen) - sizeof(struct udphdr);

    //-- Sender Addr & port

   if(from != NULL)
   {
      sin = (struct sockaddr__in *)from;
      sin->sin_len          = sizeof(struct sockaddr__in); //-- ???
      sin->sin_family       = AF_INET;
      sin->sin_port         = ntohs(ui->ui_sport);
      sin->sin_addr.s__addr = ntohl(ui->ui_src.s__addr);

      if(fromlenaddr != NULL)
         *fromlenaddr = IP4_SIN_ADDR_SIZE;
   }

  //-- Data

   mb->m_data += sizeof(struct udpiphdr);

   rd_len = _min(udp_msg_len, len);
   rx_len = 0;
   ptr = buf;
   mb0 = mb;
   for( ; mb != NULL; mb = mb->m_next)
   {
      bytes_to_copy = rd_len - rx_len;
      if(bytes_to_copy <= 0)
         break;

      if(mb->m_len > bytes_to_copy)
         nbytes = bytes_to_copy;
      else
         nbytes = mb->m_len;

      bcopy(mb->m_data, ptr, nbytes);

      rx_len += nbytes;
      ptr    += nbytes;
   }

   m_freem(tnet, mb0);  //-- Free Rx mbuf chain

   if(udp_msg_len > rx_len && rx_len != 0)
      return -EMSGSIZE;
   return rx_len;
}

//----------------------------------------------------------------------------
int udp_s_sendto(TN_NET * tnet,
                 struct socket * so,
                 unsigned char * buf,
                 int len,
                 int flags,
                 struct _sockaddr * to,
                 int tolen)
{

   struct inpcb * inp;
   volatile struct mbuf * nam;
   int rc;
   TN_MBUF * mb;
   TN_MBUF * mb_prev;
   TN_MBUF * mb_first;
   int tx_len;
   int nbytes;
   unsigned char * ptr;

   inp = (struct inpcb *)so->so_pcb;
   if(inp == NULL)
      return -EINVAL;
   if(buf == NULL || len <= 0 || len > UDP_MAX_TX_SIZE)
      return -EINVAL;

   if(to == NULL || tolen <= 0)
      nam = NULL;
   else
   {
      rc = sockargs(tnet, (struct mbuf **)&nam, (unsigned char *)to, tolen, MT_SONAME);
      if(rc)
         return -rc;
   }

//-------------------------

   mb_prev = NULL;
   tx_len  = len;
   ptr     = buf;
   mb_first = NULL;
   while(tx_len)
   {
      if(tx_len > TNNET_MID1_BUF_SIZE)
         nbytes = TNNET_MID1_BUF_SIZE;
      else
         nbytes = tx_len;

      if(so->so_state & SS_NBIO)  //-- Non - blocking socket
         mb = mb_get(tnet, MB_MID1, MB_NOWAIT, TRUE); //-- From Tx pool
      else
         mb = mb_get(tnet, MB_MID1, MB_WAIT, TRUE);   //-- From Tx pool
      if(mb == NULL)
      {
         if(mb_first)
            m_freem(tnet, mb_first);

         m_freem(tnet, (struct mbuf *)nam);
         return -ENOBUFS;
      }

      bcopy(ptr, mb->m_data, nbytes);
      mb->m_len = nbytes;

      if(mb_prev == NULL) //-- First mbuf in the chain
         mb_first = mb;
      else                //-- != NULL
         mb_prev->m_next = mb;

      mb_prev = mb;

      ptr    += nbytes;
      tx_len -= nbytes;
   }

   mb_first->m_flags |= M_PKTHDR;
   mb_first->m_tlen = len;

//----------------

   rc = udp_output(tnet,
                  (struct inpcb *)so->so_pcb,
                   mb_first,
                   (struct mbuf *)nam);     // mb_addr);
   if(rc > 0) //-- Err
      rc = -rc;
   return rc;
}

//----------------------------------------------------------------------------
int udp_s_ioctl(TN_NET * tnet,
                struct socket * so,
                int cmd,
                void * data)
{
   volatile unsigned int rc;

   switch(cmd)
   {
      case _FIONBIO:

         if(data == NULL)
            return -EINVAL;
         bcopy(data, (unsigned char*)&rc, sizeof(int));
         if(rc)
            so->so_state |= SS_NBIO;
         else
            so->so_state &= ~SS_NBIO;

         return 0; //-- O.K

      case _FIORXTIMEOUT:

         if(data == NULL)
            return -EINVAL;

         bcopy(data, (unsigned char*)&rc, sizeof(int));
         so->rx_timeout = rc;

         return 0; //-- O.K

      default:
         break;
   }

   return -ENOPROTOOPT; //-- Unknown/unsupported cmd
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

