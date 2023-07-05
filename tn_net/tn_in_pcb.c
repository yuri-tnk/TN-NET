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
 * Copyright (c) 1982, 1986, 1991, 1993
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
#include "tn_in_pcb_func.h"
#include "tn_net_mem_func.h"

#include "dbg_func.h"

//---- Local functions prototypes

TN_NETIF * tn_ifwithaddr(TN_NET * tnet, struct in__addr dst_addr);
TN_NETIF * tn_ip_route(TN_NET * tnet, struct in__addr * dst);

//----------------------------------------------------------------------------
int in_pcballoc(TN_NET * tnet, struct socket * so, struct inpcb * head)
{
   struct inpcb * inp;

   inp = (struct inpcb *)tn_net_alloc_min(tnet, MB_NOWAIT);
   if(inp == NULL)
      return (ENOBUFS);

   bzero((unsigned char *)inp, sizeof(*inp));
   inp->inp_head = head;
   inp->inp_socket = so;

   insque(inp, head);

   so->so_pcb = (unsigned char *)inp;

   return (0);
}

//----------------------------------------------------------------------------
int in_pcbbind(TN_NET * tnet, struct inpcb * inp, struct mbuf * nam)
{
   struct socket *so;
   struct inpcb *head;
   struct sockaddr__in *sin;
   u_short lport;
   int wild;
   int reuseport;
   struct in__addr zeroin_addr;

   if(inp == NULL)
      return EINVAL;

   zeroin_addr.s__addr = 0;

   so    = inp->inp_socket;
   head  = inp->inp_head;
   lport = 0;
   wild  = 0,
   reuseport = (so->so_options & SO_REUSEPORT);

   if(inp->inp_lport || inp->inp_laddr.s__addr != _INADDR_ANY)
      return (EINVAL);

   if(nam)
   {
      sin = mtod(nam, struct sockaddr__in *);
      if(nam->m_len != sizeof (*sin))
         return (EINVAL);

      lport = sin->sin_port;
      if(IN_MULTICAST(ntohl(sin->sin_addr.s__addr)))
      {
   /*
   * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
   * allow complete duplication of binding if
   * SO_REUSEPORT is set, or if SO_REUSEADDR is set
   * and a multicast address is bound on both
   * new and duplicated sockets.
   */
      }
      else if(sin->sin_addr.s__addr != _INADDR_ANY)
      {
         sin->sin_port = 0;                /* yech... */

       //-- the address (IP only, port ignored here )
       //--  corresponds to a local interface.

         if(tn_ifwithaddr(tnet, sin->sin_addr) == NULL)
            return (EADDRNOTAVAIL);
      }

      if(lport)
      {
         struct inpcb *t;

   /* GROSS */

         t = in_pcblookup(tnet, head, zeroin_addr.s__addr, 0, sin->sin_addr.s__addr, lport, wild);
         if(t && (reuseport & t->inp_socket->so_options) == 0)
            return (EADDRINUSE);
      }
      inp->inp_laddr = sin->sin_addr;
   }

   if(lport == 0)
   {
      do
      {
         if(head->inp_lport++ < IPPORT_RESERVED ||
               head->inp_lport > IPPORT_USERRESERVED)
            head->inp_lport = IPPORT_RESERVED;

         lport = htons(head->inp_lport);
      }while (in_pcblookup(tnet, head, zeroin_addr.s__addr, 0, inp->inp_laddr.s__addr, lport, wild));
   }


   inp->inp_lport = lport;
   return 0; //-- O.K.
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */

//----------------------------------------------------------------------------
int in_pcbconnect(TN_NET * tnet,
                  struct inpcb * inp,
                  struct mbuf * nam)
{
   struct sockaddr__in *sin;
   struct in__addr if_addr;
   TN_NETIF * ni;

   sin = mtod(nam, struct sockaddr__in *);  //-- with foreign addr & port
   if_addr.s__addr = 0;

   if(inp == NULL)
      return EINVAL;
   if(nam->m_len != sizeof (*sin))
      return EINVAL;

   if(sin->sin_family != AF_INET)
      return (EAFNOSUPPORT);

   if(sin->sin_port == 0)
      return (EADDRNOTAVAIL);


   if(tnet->netif_default != NULL) //-- iface exsists
   {
      if(tnet->netif_default->if_flags & IFF_UP) //-- iface is up
      {
         if(sin->sin_addr.s__addr == _INADDR_ANY)
            sin->sin_addr = tnet->netif_default->ip_addr;
         else if(sin->sin_addr.s__addr == (unsigned long)_INADDR_BROADCAST &&
                  (tnet->netif_default->if_flags & IFF_BROADCAST))
            sin->sin_addr = tnet->netif_default->if_broadaddr;
      }
   }

   if(inp->inp_laddr.s__addr == _INADDR_ANY)
   {
      //-- Find iface for foreign IP addr

      ni = tn_ip_route(tnet, &sin->sin_addr);
      if(ni == NULL)
         return EADDRNOTAVAIL;

      if_addr = ni->ip_addr;
   }

  //-- Verify that socket pair is unique

   if(in_pcblookup(tnet,
            inp->inp_head,
            sin->sin_addr.s__addr,
            sin->sin_port,
            inp->inp_laddr.s__addr ? inp->inp_laddr.s__addr : if_addr.s__addr, //  YVT ifaddr->sin_addr
            inp->inp_lport,
            0))
       return (EADDRINUSE);

   if(inp->inp_laddr.s__addr == _INADDR_ANY)
   {
      if(inp->inp_lport == 0)
         in_pcbbind(tnet, inp, NULL);

      inp->inp_laddr = if_addr; //-- See above
   }

   inp->inp_faddr = sin->sin_addr;
   inp->inp_fport = sin->sin_port;

   return 0;
}

//----------------------------------------------------------------------------
void in_pcbdetach(TN_NET * tnet, struct inpcb * inp)
{
   struct socket * so;

   if(inp == NULL)
      return;

   so = inp->inp_socket;

   so->so_pcb = NULL;  //-- Force to free memory of the socket itself

   if(so->sofree)
      so->sofree(tnet, so);

   remque(inp);

   tn_net_free_min(tnet, (void*)inp); // Yes
}

//----------------------------------------------------------------------------
void in_pcbdisconnect(TN_NET * tnet, struct inpcb *inp)
{
   if(inp == NULL)
      return;

   inp->inp_faddr.s__addr = _INADDR_ANY;
   inp->inp_fport = 0;
   if(inp->inp_socket->so_state & SS_NOFDREF)
      in_pcbdetach(tnet, inp);
}

//----------------------------------------------------------------------------
void in_setsockaddr(struct inpcb * inp, struct mbuf * nam)
{
   struct sockaddr__in *sin;

   if(inp == NULL || nam == NULL)
      return;

   nam->m_len = sizeof (*sin);

   sin = mtod(nam, struct sockaddr__in *);
   bzero((unsigned char *)sin, sizeof (*sin));

   sin->sin_family = AF_INET;
   sin->sin_len    = sizeof(*sin);
   sin->sin_port   = inp->inp_lport;
   sin->sin_addr   = inp->inp_laddr;
}

//----------------------------------------------------------------------------
void in_setpeeraddr(struct inpcb * inp, struct mbuf * nam)
{
   struct sockaddr__in *sin;

   if(inp == NULL || nam == NULL)
      return;

   nam->m_len = sizeof (*sin);
   sin = mtod(nam, struct sockaddr__in *);
   bzero((unsigned char *)sin, sizeof (*sin));

   sin->sin_family = AF_INET;
   sin->sin_len    = sizeof(*sin);
   sin->sin_port   = inp->inp_fport;
   sin->sin_addr   = inp->inp_faddr;
}

//----------------------------------------------------------------------------
struct inpcb * in_pcblookup(TN_NET * tnet,
                            struct inpcb * head,
                            unsigned int faddr, // struct in__addr faddr,
                            unsigned int fport_arg,
                            unsigned int laddr, //struct in__addr laddr,
                            unsigned int lport_arg,
                            int flags)
{
   struct inpcb *inp;
   struct inpcb * match = NULL;
   int matchwild = 3;
   int wildcard;
   u_short fport = fport_arg;
   u_short lport = lport_arg;

   for(inp = head->inp_next; inp != head; inp = inp->inp_next)
   {
      if(inp->inp_lport != lport)
         continue;
      wildcard = 0;

      if(inp->inp_laddr.s__addr != _INADDR_ANY)
      {
         if(laddr == _INADDR_ANY)
            wildcard++;
         else if(inp->inp_laddr.s__addr != laddr)
            continue;
      }
      else
      {
         if(laddr != _INADDR_ANY)
            wildcard++;
      }

      if(inp->inp_faddr.s__addr != _INADDR_ANY)
      {
         if(faddr == _INADDR_ANY)
            wildcard++;
         else if(inp->inp_faddr.s__addr != faddr ||
                   inp->inp_fport != fport)
            continue;
      }
      else
      {
         if(faddr != _INADDR_ANY)
            wildcard++;
      }

      if(wildcard && (flags & INPLOOKUP_WILDCARD) == 0)
         continue;

      if(wildcard < matchwild)
      {
         match = inp;
         matchwild = wildcard;
         if(matchwild == 0)
            break;
      }
   }

   return match;
}

//----------------------------------------------------------------------------
TN_NETIF * tn_ip_route(TN_NET * tnet, struct in__addr * dst)
{
   TN_NETIF * ni;

   if(dst == NULL)
      return NULL;

   for(ni = tnet->netif_list; ni != NULL; ni = ni->next)
   {
      if(ni->if_flags & IFF_UP)
      {
         if((dst->s__addr & ni->netmask.s__addr) ==
                              (ni->ip_addr.s__addr & ni->netmask.s__addr))
            return ni;
      }
   }

   if(tnet->netif_default == NULL)
      return NULL;
   else if((tnet->netif_default->if_flags & IFF_UP) == 0)
      return NULL;


   return tnet->netif_default;  //-- no matching netif found - use default netif
}

//-----------------------------------------------------------------------------
// Locate an interface based on a IP address only
//-----------------------------------------------------------------------------

TN_NETIF * tn_ifwithaddr(TN_NET * tnet, struct in__addr dst_addr)
{
   TN_NETIF * ni;

   for(ni = tnet->netif_list; ni != NULL; ni  = ni->next)
   {
       if(dst_addr.s__addr == ni->ip_addr.s__addr)
          return ni;

        if((ni->if_flags & IFF_BROADCAST) && ni->if_broadaddr.s__addr != 0
                      && ni->if_broadaddr.s__addr == dst_addr.s__addr)
            return ni;
   }
   return NULL;
}

//----------------------------------------------------------------------------
// TCP - Return 1 if the address might be a local broadcast address.
//----------------------------------------------------------------------------
int in_broadcast(struct in__addr in, TN_NETIF * ni)
{
   if(in.s__addr == _INADDR_BROADCAST || in.s__addr == _INADDR_ANY)
      return 1;
   if((ni->if_flags & IFF_BROADCAST) == 0)
      return 0;

//??? Check interface broadcast ???

   return 0;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------






