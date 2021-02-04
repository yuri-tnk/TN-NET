#ifndef _TN_NETIF_H_
#define _TN_NETIF_H_

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

#define   IFF_UP                          0x1       /* interface is up */
#define   IFF_BROADCAST                   0x2       /* broadcast address valid */
#define   IFF_DEBUG                       0x4       /* turn on debugging */
#define   IFF_LOOPBACK                    0x8       /* is a loopback net */
#define   IFF_POINTOPOINT                0x10       /* interface is point-to-point link */
#define   IFF_NOTRAILERS                 0x20       /* avoid use of trailers */
#define   IFF_RUNNING                    0x40       /* resources allocated */
#define   IFF_NOARP                      0x80       /* no address resolution protocol */
#define   IFF_PROMISC                   0x100       /* receive all packets */
#define   IFF_ALLMULTI                  0x200       /* receive all multicast packets */
#define   IFF_OACTIVE                   0x400       /* transmission in progress */
#define   IFF_SIMPLEX                   0x800       /* can't hear own transmissions */
#define   IFF_LINK0                    0x1000       /* per link layer defined bit */
#define   IFF_LINK1                    0x2000       /* per link layer defined bit */
#define   IFF_LINK2                    0x4000       /* per link layer defined bit */
#define   IFF_MULTICAST                0x8000       /* supports multicast */

#define   TNNET_IF_RX_QUEUE_SIZE           16

#define   ETHERTYPE_IP                  0x0800        // IP protocol
#define   ETHERTYPE_ARP                 0x0806        // Addr. resolution protocol

typedef void (* tn_netif_task_func)(void * par);

struct tn_netif;
struct _TN_ARPENTRY;

typedef int (*tn_netif_output_func)(TN_NET * tnet, struct tn_netif * ni, TN_MBUF * mb);
typedef int (*tn_netif_drv_wr_func)(TN_NET * tnet, struct tn_netif * ni, TN_MBUF * mb);
typedef int (*tn_netif_drv_ioctl_func)(TN_NET * tnet, struct tn_netif * ni,
                                     int req_type, void * par);

struct _DHCPINFO;

struct tn_netif
{
   struct tn_netif * next;

   struct in__addr ip_addr;
   struct in__addr netmask;
   struct in__addr ip_gateway;
   struct in__addr if_broadaddr;
   unsigned char hw_addr[6];

   int if_flags;
   int if_mtu;

   struct inpcb * tcp_last_inpcb;  //-- Minimalistic Routing cache

   tn_netif_output_func  output_func;   //-- IP output

 //---  Rx Task

   TN_TCB  task_rx;
   unsigned int * task_rx_stack;

 //--- Rx & Tx queues

   TN_DQUE  queueIfaceRx;
   unsigned int * queueIfaceRxMem;

   TN_DQUE  queueIfaceTx;
   unsigned int * queueIfaceTxMem;

 //--  Hardware (Tx/Rx driver) info

   void * drv_info;

   // Call From a task - do not call from interuupt
   tn_netif_drv_wr_func     drv_wr;
   tn_netif_drv_ioctl_func  drv_ioctl;

 //-- interface extention(s)

      //-- ARP

   struct _TN_ARPENTRY * arp_arr;             //-- ARP cache
   int           arp_arr_capacity;    //-- ARP cache capacity
   TN_SEM      * arp_sem;             //-- for the ARP lock/unlock
   int  dup_own_ip_addr;

     //--- DHCP
   struct _DHCPINFO * dhi;

#ifdef TN_DHCP
   TN_TCB  dhcp_task_rx;
   unsigned int * dhcp_task_rx_stack;
   TN_SEM    dhcp_sem;             
#endif

};
typedef struct tn_netif TN_NETIF;


#define IOCTL_ETH_IS_TX_DISABLED  1
#define IOCTL_ETH_IS_LINK_UP      2

#endif

