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
#include "tn_s_udp.h"


#if defined (__ICCARM__)   // IAR ARM
#pragma diag_suppress=Pa039
#endif

//--- External globals ---

  //-- Just to avoid extra argument in tne sockets functions

extern TN_NET * g_tnet_addr;

//----------------------------------------------------------------------------
int s_socket(int domain, int type, int protocol)
{
   TN_NET * tnet;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EPERM;
   if(domain != AF_INET)
      return -EPERM;

   switch(type)
   {
      case SOCK_DGRAM:
         if(protocol != IPPROTO_UDP)
            return -EPERM;
         return udp_s_socket(tnet, domain, type, protocol);

      case SOCK_STREAM:
#ifdef TN_TCP
         return tcp_s_socket(tnet, domain, type, protocol);
#else
         break;
#endif
  //    case SOCK_RAW:
  //       break;
      default:
         break;
   }

   return  -EPERM; // -1 - Operation not permitted
}

//----------------------------------------------------------------------------
int s_close(int s)
{
   TN_NET * tnet;
   struct socket * so;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(so->so_type)
   {
      case SOCK_DGRAM:
         return udp_s_close(tnet, so);
      case SOCK_STREAM:
#ifdef TN_TCP
         return tcp_s_close(tnet, s);
#else
         break;
#endif
  //    case SOCK_RAW:
  //       break;
      default:
         break;
   }

   return  -EPERM; // -1 - Operation not permitted
}

//----------------------------------------------------------------------------
int s_bind(int s, const struct _sockaddr * name, int namelen)
{
   TN_NET * tnet;
   struct socket * so;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(so->so_type)
   {
      case SOCK_DGRAM:
         return udp_s_bind(tnet, so, name, namelen);
      case SOCK_STREAM:
#ifdef TN_TCP
         return tcp_s_bind(tnet, s, name, namelen);
#else
         break;
#endif
  //    case SOCK_RAW:
  //       break;
      default:
         break;
   }

   return  -EPERM; // -1 - Operation not permitted
}

//----------------------------------------------------------------------------
int s_connect(int s, struct _sockaddr * name, int namelen)
{
   TN_NET * tnet;
   struct socket * so;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(so->so_type)
   {
      case SOCK_STREAM:
#ifdef TN_TCP
         return tcp_s_connect(tnet, s, name, namelen);
#else
         break;
#endif
//      case SOCK_RAW:
//       return ()
//         break;
      case SOCK_DGRAM:
         return udp_s_connect(tnet, so, name, namelen);
      default:
         break;
   }

   return  -EPERM; // -1 - Operation not permitted

}
//----------------------------------------------------------------------------
int s_accept(int s,                     // socket descriptor
             struct _sockaddr * addr,   // peer address
             int * addrlen)             // peer address length
{
   TN_NET * tnet;
   struct socket * so;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(so->so_type)
   {
      case SOCK_STREAM:
#ifdef TN_TCP
         return tcp_s_accept(tnet, s, addr, addrlen);
#else
         break;
#endif
  //    case SOCK_DGRAM:
  //       break;
  //    case SOCK_RAW:
  //       break;
      default:
         break;
   }

   return  -EPERM; // -1 - Operation not permitted
}

//----------------------------------------------------------------------------
int s_listen(int s,
             int backlog)        // number of connections to queue
{
   TN_NET * tnet;
   struct socket * so;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(so->so_type)
   {
      case SOCK_STREAM:
#ifdef TN_TCP
         return tcp_s_listen(tnet, s, backlog);
#else
         break;
#endif
  //   case SOCK_DGRAM:
  //      break;
  //    case SOCK_RAW:
  //       break;
      default:
         break;
   }

   return  -EPERM; // -1 - Operation not permitted
}

//----------------------------------------------------------------------------
int s_recv(int s,
           unsigned char * buf,   // buffer to write data to
           int nbytes,            // length of buffer
           int flags)
{

   TN_NET * tnet;
   struct socket * so;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(so->so_type)
   {
      case SOCK_STREAM:
#ifdef TN_TCP
         return tcp_s_recv(tnet, s, buf, nbytes, flags);
#else
         break;
#endif
  //    case SOCK_DGRAM:
  //    case SOCK_RAW:
  //       break;
      default:
         break;
   }

   return  -EPERM; // -1 - Operation not permitted
}

//----------------------------------------------------------------------------
int s_recvfrom(int  s,
               unsigned char *  buf,
               int  len,
               int  flags,
               struct _sockaddr * from,
               int * fromlenaddr)
{
   TN_NET * tnet;
   struct socket * so;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(so->so_type)
   {
      case SOCK_DGRAM:
         return udp_s_recvfrom(tnet,
                               so,
                               buf,
                               len,
                               flags,
                               from, // unsigned char *  from,
                               fromlenaddr);
  //    case SOCK_STREAM:
  //       break;
  //    case SOCK_RAW:
  //       break;
      default:
         break;
   }

   return  -EPERM; // -1 - Operation not permitted

}

//----------------------------------------------------------------------------
int s_send(int  s,
           unsigned char * buf,  // pointer to buffer to transmit
           int  nbytes,          // num bytes to transmit
           int  flags)
{
   TN_NET * tnet;
   struct socket * so;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(so->so_type)
   {
      case SOCK_STREAM:
#ifdef TN_TCP
         return  tcp_s_send(tnet, s, buf, nbytes, flags);
#else
         break;
#endif
  //    case SOCK_DGRAM:
  //    case SOCK_RAW:
  //       break;
      default:
         break;
   }

   return  -EPERM; // -1 - Operation not permitted
}

//----------------------------------------------------------------------------
int s_sendto(int s,
             unsigned char * buf,
             int len,
             int flags,
             struct _sockaddr * dst_addr,
             int dst_addr_len)
{
   TN_NET * tnet;
   struct socket * so;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(so->so_type)
   {
      case SOCK_DGRAM:
        return udp_s_sendto(tnet,
                            so,
                            buf,
                            len,
                            flags,
                            dst_addr,
                            dst_addr_len);
  //       break;
  //    case SOCK_STREAM:
  //    case SOCK_RAW:
  //       break;
      default:
         break;
   }
   return  -EPERM; // -1 - Operation not permitted
}
//----------------------------------------------------------------------------
int s_ioctl(int s,
            int cmd,
            void * data) //-- [IN]/[OUT]
{
   TN_NET * tnet;
   struct socket * so;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(so->so_type)
   {
      case SOCK_DGRAM:
         return udp_s_ioctl(tnet, so, cmd, data);
      case SOCK_STREAM:
#ifdef TN_TCP
         return tcp_s_ioctl(tnet, s, cmd, data);
#else
         break;
#endif
  //    case SOCK_RAW:
  //       break;
   }

   return  -EPERM; // -1 - Operation not permitted
}
 //----------------------------------------------------------------------------
int s_shutdown(int s, int how)
{
   TN_NET * tnet;
   struct socket * so;

   tnet = g_tnet_addr;
   if(tnet == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(so->so_type)
   {
      case SOCK_STREAM:
#ifdef TN_TCP
         return tcp_s_shutdown(tnet, s, how);
#else
         break;
#endif
  //    case SOCK_DGRAM:
  //       break;
  //    case SOCK_RAW:
  //       break;
   }

   return  -EPERM; // -1 - Operation not permitted
}
//----------------------------------------------------------------------------
int  s_getpeername( int s,
                    struct _sockaddr * name,
                    int * namelen)
{
   TN_NET * tnet;
   struct socket * so;
   struct inpcb * inp;
   struct mbuf * mb;
   int len;

   tnet = g_tnet_addr;
   if(tnet == NULL || namelen == NULL)
      return -EINVAL;

   len = *namelen;
   if(len > sizeof(struct _sockaddr))
      len = sizeof(struct _sockaddr);

   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   inp = (struct inpcb *)so->so_pcb;
   if(inp == NULL)
      return -EINVAL;

   if((so->so_state & SS_ISCONNECTED) == 0)
      return -ENOTCONN;

   mb = mb_get(tnet, MB_MID1, MB_WAIT, FALSE);  //--Not from Drv bufs
   if(mb == NULL)
      return -ENOBUFS;

   s_memset(mb->m_data, 0, sizeof(struct _sockaddr));

   in_setpeeraddr(inp, mb);

   bcopy(mb->m_data, (unsigned char *)name, len);

   m_freem(tnet, mb);

   return 0;
}

//----------------------------------------------------------------------------
int s_getsockopt(int  s,
                 int level,
                 int name,
                 unsigned char * val,
                 int * avalsize)
{
   TN_NET * tnet;
   struct socket * so;
   unsigned int rc;
   unsigned char * buf = NULL;
   int res_len = sizeof(unsigned int);

   tnet = g_tnet_addr;
   if(tnet == NULL || val == NULL || avalsize == NULL)
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(name)
   {
      case SO_SNDTIMEO:
      case SO_RCVTIMEO:

         if(so->so_type == SOCK_STREAM)
         {
            return -ENOPROTOOPT;
         }
         else if(so->so_type == SOCK_DGRAM)
         {
            if(name == SO_RCVTIMEO)
            {
               if(val == NULL)
                  return -EINVAL;
               bcopy((unsigned char*)&so->rx_timeout, val, sizeof(int));
               if(avalsize)
                 *avalsize = sizeof(int);
            }
            else
               return -ENOPROTOOPT;
         }
         break;

      case SO_LINGER:

         if(so->so_type == SOCK_STREAM)
         {
#ifdef TN_TCP
            buf = tn_net_alloc_mid1(tnet, MB_WAIT);
            if(buf == NULL)
               return -ENOBUFS;

            res_len = sizeof (struct _linger);
            ((struct _linger *)buf)->l_onoff  = so->so_options & SO_LINGER;
            ((struct _linger *)buf)->l_linger = so->so_linger;

            //*((unsigned char **)val) = buf;
            //*avalsize = res_len;
#endif
         }
         else
            return -ENOPROTOOPT;

         break;

      case SO_USELOOPBACK:
      case SO_DONTROUTE:
      case SO_DEBUG:
      case SO_KEEPALIVE:
      case SO_REUSEADDR:
      case SO_REUSEPORT:
      case SO_BROADCAST:
      case SO_OOBINLINE:

         rc = so->so_options & name;
         bcopy((unsigned char*)&rc, val, sizeof(unsigned int));
         *avalsize = sizeof(unsigned int);
         break;

      case SO_TYPE:

         rc = so->so_type;
         bcopy((unsigned char*)&rc, val, sizeof(unsigned int));
         *avalsize = sizeof(unsigned int);
         break;

      case SO_ERROR:

         if(so->so_type == SOCK_STREAM)
         {
#ifdef TN_TCP
            struct socket_tcp * so_tcp = (struct socket_tcp *)s;
            rc = so_tcp->so_error;
            so_tcp->so_error = 0;
            bcopy(&rc, val, sizeof(unsigned int));
            *avalsize = sizeof(unsigned int);
#endif
         }
         else
            return -ENOPROTOOPT;

         break;

      case SO_SNDBUF:

         if(so->so_type == SOCK_STREAM)
         {
#ifdef TN_TCP
            struct socket_tcp * so_tcp = (struct socket_tcp *)s;
            rc  = so_tcp->so_snd.sb_hiwat;
            bcopy(&rc, val, sizeof(unsigned int));
            *avalsize = sizeof(unsigned int);
#endif
         }
         else
            return -ENOPROTOOPT;
         break;

      case SO_RCVBUF:

         if(so->so_type == SOCK_STREAM)
         {
#ifdef TN_TCP
            struct socket_tcp * so_tcp = (struct socket_tcp *)s;
            rc  = so_tcp->so_rcv.sb_hiwat;
            bcopy(&rc, val, sizeof(unsigned int));
            *avalsize = sizeof(unsigned int);
#endif
         }
         else
            return -ENOPROTOOPT;

         break;

      case SO_SNDLOWAT:

         if(so->so_type == SOCK_STREAM)
         {
#ifdef TN_TCP
            struct socket_tcp * so_tcp = (struct socket_tcp *)s;
            rc = so_tcp->so_snd.sb_lowat;
            bcopy(&rc, val, sizeof(unsigned int));
            *avalsize = sizeof(unsigned int);
#endif
         }
         else
            return -ENOPROTOOPT;

         break;

      case SO_RCVLOWAT:

         if(so->so_type == SOCK_STREAM)
         {
#ifdef TN_TCP
            struct socket_tcp * so_tcp = (struct socket_tcp *)s;
            rc = so_tcp->so_rcv.sb_lowat;
            bcopy(&rc, val, sizeof(unsigned int));
           *avalsize = sizeof(unsigned int);
#endif
         }
         else
            return -ENOPROTOOPT;
         break;

      default:

         return -ENOPROTOOPT;
   }

   if(buf)
   {
      if(res_len > *avalsize)
         res_len = *avalsize;

      *avalsize = res_len;
      bcopy(buf, val, res_len);

      tn_net_free_mid1(tnet, buf);
   }

   return 0; //-- o.k.
}

//---------------------------------------------------------------------------
int s_setsockopt(int s,
                 int level,
                 int name,
                 unsigned char * val,
                 int avalsize)
{
   TN_NET * tnet;
   struct socket * so;
   unsigned int rc;
   struct _linger lg;

   tnet = g_tnet_addr;
   if(tnet == NULL || val == NULL )
      return -EINVAL;
   so = (struct socket *)s;
   if(so == NULL)
      return -EINVAL;

   switch(name)
   {
      case SO_SNDTIMEO:
      case SO_RCVTIMEO:
        if(name == SO_RCVTIMEO)
        {
          if(val == NULL)
            return -EINVAL;
          bcopy(val, (unsigned char*)&rc, sizeof(int));
          so->rx_timeout = rc;
        }
        else
          return -ENOPROTOOPT;
         break;

      case SO_LINGER:

         if(so->so_type == SOCK_STREAM)
         {
            if(avalsize != sizeof (struct _linger))
               return -EINVAL;
            bcopy(val, (unsigned char*)&lg, sizeof(struct _linger));

            if(lg.l_onoff)
               so->so_options |= name;
            else
               so->so_options &= ~name;
            so->so_linger = lg.l_linger;  //-- if linger time = 0 - force RST instead FIN
         }

         break;


      case SO_DEBUG:
      case SO_KEEPALIVE:
      case SO_DONTROUTE:
      case SO_USELOOPBACK:
      case SO_BROADCAST:
      case SO_REUSEADDR:
      case SO_REUSEPORT:
      case SO_OOBINLINE:

         if(avalsize < sizeof(int))
            return -EINVAL;

         bcopy(val, (unsigned char*)&rc, sizeof(unsigned int));

         if(rc)
            so->so_options |= name;
         else
            so->so_options &= ~name;

         break;

      case SO_SNDBUF:
      case SO_RCVBUF:
      case SO_SNDLOWAT:
      case SO_RCVLOWAT:

         if(avalsize < sizeof(int))
            return -EINVAL;

         switch(name)
         {
            case SO_SNDBUF:
            case SO_RCVBUF:
               if(so->so_type == SOCK_STREAM)
               {
#ifdef TN_TCP
                  int sm;
                  struct socket_tcp * so_tcp = (struct socket_tcp *)s;

                  sm = splnet(tnet);
                  bcopy(val, &rc, sizeof(unsigned int));
                  if(sbreserve(tnet, name == SO_SNDBUF ?
                       &so_tcp->so_snd : &so_tcp->so_rcv, rc) == 0)
                  {
                    splx(tnet, sm);
                    return -ENOBUFS;
                  }
                  splx(tnet, sm);
#endif
               }
               else
                  return -ENOPROTOOPT;

             break;

            case SO_SNDLOWAT:

               if(so->so_type == SOCK_STREAM)
               {
#ifdef TN_TCP
                  struct socket_tcp * so_tcp = (struct socket_tcp *)s;
                  bcopy(val, &rc, sizeof(unsigned int));
                  so_tcp->so_snd.sb_lowat = rc;
#endif
               }
               else
                  return -ENOPROTOOPT;

               break;

            case SO_RCVLOWAT:

               if(so->so_type == SOCK_STREAM)
               {
#ifdef TN_TCP
                  struct socket_tcp * so_tcp = (struct socket_tcp *)s;
                  bcopy(val, &rc, sizeof(unsigned int));
                  so_tcp->so_rcv.sb_lowat = rc;
#endif
               }
               else
                  return -ENOPROTOOPT;
           break;
         }

         break;

      default:

         return  -ENOPROTOOPT;
   }

   return 0; //-- o.k.
}

//----------------------------------------------------------------------------
int  sockargs(TN_NET * tnet,
              struct mbuf ** mp,
              unsigned char * buf,
              int buflen,
              int type)
{
   struct _sockaddr * sa;
   struct mbuf * mb;

   if(buflen > TNNET_MID1_BUF_SIZE)
      return EINVAL;

   mb = mb_get(tnet, MB_MID1, MB_NOWAIT, FALSE); //-- Not from Drv pool
   if(mb == NULL)
      return ENOBUFS;

   bcopy(buf, mb->m_data, buflen);
   mb->m_len = buflen;

   *mp = mb;

   if(type == MT_SONAME) // TN NET - always
   {
      sa = (struct _sockaddr *)mb->m_data;
      sa->sa_len = buflen;
   }

   return 0; //-- O.K.
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


