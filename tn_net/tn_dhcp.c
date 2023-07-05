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

/*  Copyright (c) 1997-2002 Gisle Vanem <giva@bgnett.no>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. All advertising materials mentioning features or use of this software
 *     must display the following acknowledgement:
 *       This product includes software developed by Gisle Vanem
 *       Bergen, Norway.
 *
 *  THIS SOFTWARE IS PROVIDED BY ME (Gisle Vanem) AND CONTRIBUTORS ``AS IS''
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL I OR CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>  //-- NULL, etc
#include <string.h>  //-- memcpy

#include <tnkernel/tn.h>

#include "tn_net_cfg.h"
#include "tn_net_types.h"
#include "tn_net_pcb.h"
#include "tn_net_mem.h"
#include "tn_ip.h"
#include "tn_net.h"
#include "errno.h"
#include "tn_mbuf.h"
#include "bsd_socket.h"
#include "tn_socket.h"
#include "tn_netif.h"
#include "tn_net_utils.h"
#include "ip_icmp.h"
#include "tn_eth.h"
#include "tn_udp.h"
#include "tn_net_func.h"
#include "tn_socket_func.h"
#include "tn_net_mem_func.h"

#include "tn_dhcp.h"

#include "dbg_func.h"

#ifdef TN_DHCP

#if defined (__ICCARM__)   // IAR ARM
#pragma diag_suppress=Pa039
#endif

//-- Default list of DHCP request values

static const unsigned char g_default_request_list[] =
{
   DHCP_OPT_SUBNET_MASK,
   DHCP_OPT_ROUTERS_ON_SNET,
   DHCP_OPT_DNS_SRV,
   DHCP_OPT_COOKIE_SRV,
   DHCP_OPT_LPR_SRV,
   DHCP_OPT_HOST_NAME,
   DHCP_OPT_DOMAIN_NAME,
   DHCP_OPT_IP_DEFAULT_TTL,
   DHCP_OPT_IF_MTU,
   DHCP_OPT_ARP_CACHE_TIMEOUT,
   DHCP_OPT_ETHERNET_ENCAPSULATION,
   DHCP_OPT_SRV_IDENTIFIER,
   DHCP_OPT_BROADCAST_ADDR,
   DHCP_OPT_TCP_DEFAULT_TTL
};

//---

void dhcp_task_func(void * par);

//-----------------

#define DHCP_LOCK       tn_sem_acquire(&ni->dhcp_sem, TN_WAIT_INFINITE);
#define DHCP_UNLOCK     tn_sem_signal(&ni->dhcp_sem);

//----------------------------------------------------------------------------
static unsigned char * dhcp_fill_header(TN_NETIF * ni)
{
   volatile unsigned int tmp_ui;
   DHCPINFO * dhi;
   struct dhcp * dh;
   unsigned char * ptr;

   if(ni == NULL)
      return NULL;
   dhi = ni->dhi;
   if(dhi == NULL)
      return NULL;
   dh = dhi->dhcp_dh;
   if(dh == NULL)
      return NULL;

   memset(dh, 0, sizeof(struct dhcp));

   dh->dh_op     = DHCP_BOOTP_REQUEST;    //-- 1
   dh->dh_htype  = DHCP_HTYPE_ETHERNET;   //-- 1
   dh->dh_hlen   = DHCP_HLEN_ETHERNET;    //-- 6

   tmp_ui = htonl(dhi->d_xid);
   memcpy((char*)&dh->dh_xid, (void*)&tmp_ui, 4);

   dh->dh_secs   = 0;
   dh->dh_flags  = 0; //dhi->bcast_flag ? BROADCAST_FLAG : 0;
   dh->dh_yiaddr = 0;

   if(dhi->state == DHCP_ST_BOUND    ||
         dhi->state == DHCP_ST_RENEWING ||
             dhi->state == DHCP_ST_REBINDING)
      tmp_ui = ni->ip_addr.s__addr;
   else
      tmp_ui = 0;
   memcpy((char*)&dh->dh_ciaddr, (void*)&tmp_ui, 4);

   dh->dh_giaddr = 0;  //-- Suppose that we are at the same subnet with the server

   memcpy(&dh->dh_chaddr[0], ni->hw_addr, DHCP_HLEN_ETHERNET);

   ptr = &dh->dh_opt[0];
   *ptr++ = 0x63;
   *ptr++ = 0x82;
   *ptr++ = 0x53;
   *ptr++ = 0x63;

   return ptr;
}

//----------------------------------------------------------------------------
static int dhcp_request(TN_NET * tnet, TN_NETIF * ni)
{
   unsigned char * opt;
   volatile unsigned int tmp_ui;
   DHCPINFO * dhi;
   struct sockaddr__in s_a;

   if(ni == NULL)
      return -EINVAL;
   dhi = ni->dhi;
   if(dhi == NULL)
      return -EINVAL;

//  inside  dhcp_fill_header() we check the condition:
//   if(DHCP_ST_BOUND || DHCP_ST_RENEWING || DHCP_ST_REBINDING)
//      ciaddr set as ni->ip_addr, else - ciaddr = 0;

   //-- if DHCP_ST_SELECTING we send request
   //-- for the DHCPOFFER - xid must be the same

   if(dhi->state != DHCP_ST_SELECTING)
      dhi->d_xid = tn_rand(&tnet->rnd_gen);    // New random exchange ID

   opt = dhcp_fill_header(ni);
   if(opt == NULL)
      return -EINVAL;

   *opt++ = DHCP_OPT_MSG_TYPE;
   *opt++ = 1;
   *opt++ = DHCP_REQUEST;

   if(dhi->state == DHCP_ST_SELECTING)
   {
     *opt++ = DHCP_OPT_SRV_IDENTIFIER;
     *opt++ = sizeof(unsigned int);
      memcpy(opt, &dhi->d_dhcp_server_addr, sizeof(unsigned int));
      opt += sizeof(unsigned int);
   }

   if(dhi->state == DHCP_ST_SELECTING ||
       dhi->state == DHCP_ST_INITREBOOT)
   {
      *opt++ = DHCP_OPT_REQUESTED_IP_ADDR;
      *opt++ = sizeof(unsigned int);
      if(dhi->state == DHCP_ST_SELECTING)
         memcpy(opt, &dhi->d_offer_ip_addr, sizeof(unsigned int));
      else
         memcpy(opt, &dhi->d_curr_ip_addr, sizeof(unsigned int));
      opt += sizeof(unsigned int);
   }

  //-- Some DHCP-daemons require this tag in a REQUEST.

   *opt++ = DHCP_OPT_CLIENT_ID;
   *opt++ = DHCP_HLEN_ETHERNET + 1; //-- hw_len +1
   *opt++ = 1;                      //-- hw_type;
   memcpy(opt, ni->hw_addr, DHCP_HLEN_ETHERNET);
   opt += DHCP_HLEN_ETHERNET;

   if(dhi->lease_time && dhi->lease_time != DHCP_INFINITE_LEASE)
   {
      *opt++ = DHCP_OPT_IP_ADDR_LEASE_TIME;
      *opt++ = sizeof(unsigned int);
      tmp_ui = htonl(dhi->lease_time);
      memcpy(opt, (void*)&tmp_ui, sizeof(unsigned int));
      opt += sizeof(unsigned int);
   }

   *opt++ = DHCP_OPT_END;

   //   Send       DHCPST_RENEWING
   //   Broadcast  DHCPST_REQUESTING  DHCPST_REBOOTING DHCPST_REBINDING

   s_a.sin_len     = sizeof(struct sockaddr__in);
   s_a.sin_family  = AF_INET;
   s_a.sin_port    = htons((unsigned short)DHCP_SERVER_PORT);

   if(dhi->state == DHCP_ST_RENEWING)
      s_a.sin_addr.s__addr = dhi->dhcp_server_addr;
   else
      s_a.sin_addr.s__addr = _INADDR_BROADCAST;     //-- 0xFFFFFFFF

   return s_sendto(dhi->s,
                  (unsigned char*)dhi->dhcp_dh,
                   opt - (unsigned char*)dhi->dhcp_dh, // len
                   0,
                  (struct _sockaddr *) &s_a,
                   s_a.sin_len);
}

//----------------------------------------------------------------------------
static int dhcp_discover(TN_NET * tnet, TN_NETIF * ni)
{
   unsigned char * opt;
   unsigned char * start;
   volatile int size;
   unsigned short tmp_us;
   struct sockaddr__in s_a;

   DHCPINFO * dhi;
   struct dhcp * dh;

   if(ni == NULL)
      return -EINVAL;
   dhi = ni->dhi;
   if(dhi == NULL)
      return -EINVAL;
   dh = dhi->dhcp_dh;
   if(dh == NULL)
      return -EINVAL;

   dhi->d_xid = tn_rand(&tnet->rnd_gen);    // New random exchange ID

   start = dhcp_fill_header(ni);
   opt   = start;
   if(opt == NULL)
      return -EINVAL;

   *opt++ = DHCP_OPT_MSG_TYPE;
   *opt++ = 1;
   *opt++ = DHCP_DISCOVER;

   *opt++ = DHCP_OPT_CLIENT_ID;
   *opt++ = DHCP_HLEN_ETHERNET + 1; //-- hw_len +1
   *opt++ = 1;                      //-- hw_type;
   memcpy(opt, ni->hw_addr, DHCP_HLEN_ETHERNET);
   opt += DHCP_HLEN_ETHERNET;

   *opt++ = DHCP_OPT_MAX_MSG_SIZE;   // Maximum DHCP message size
   *opt++ = 2;
   tmp_us = htons(sizeof(struct dhcp));
   memcpy(opt, &tmp_us, sizeof(unsigned short));
   opt += 2;

   if(dhi->d_lease_time != 0)
   {
      *opt++ = DHCP_OPT_IP_ADDR_LEASE_TIME;
      *opt++ = sizeof(unsigned int);
      size = htonl(dhi->d_lease_time);
      memcpy(opt, (void*)&size, sizeof(unsigned int));
      opt += sizeof(unsigned int);
   }

   //--- Put request list

   size = _min(sizeof(g_default_request_list),
                       sizeof(dh->dh_opt) - (opt - start) - 1);
                                   // room for DHCP_OPT_END ^
   if(size > 0)
   {
      size   = _min(size, 255);
      *opt++ = DHCP_OPT_PARAM_REQUEST;
      *opt++ = size;
      memcpy(opt, &g_default_request_list[0], size);
      opt += size;
   }

   *opt++ = DHCP_OPT_END;

   //  Here - Broadcast

   s_a.sin_len          = sizeof(struct sockaddr__in);
   s_a.sin_family       = AF_INET;
   s_a.sin_port         = htons((unsigned short)DHCP_SERVER_PORT);
   s_a.sin_addr.s__addr = _INADDR_BROADCAST;     //-- 0xFFFFFFFF

   return s_sendto(dhi->s,
                  (unsigned char*)dhi->dhcp_dh,
                   opt - (unsigned char*)dhi->dhcp_dh, // len
                   0,
                  (struct _sockaddr *) &s_a,
                   s_a.sin_len);
}

//----------------------------------------------------------------------------
// msg_type - DHCP_RELEASE or DHCP_DECLINE.
// msg      - An optional message describing the cause.

static int dhcp_release_decline(TN_NET * tnet, TN_NETIF * ni, int msg_type, const char * msg)
{
   unsigned char * opt;
   unsigned char * ptr_end;
   int len;
   DHCPINFO * dhi;
   struct dhcp * dh;
   struct sockaddr__in s_a;

   if(ni == NULL)
      return -EINVAL;
   dhi = ni->dhi;
   if(dhi == NULL)
      return -EINVAL;
   dh = dhi->dhcp_dh;
   if(dh == NULL)
      return -EINVAL;

   dhi->d_xid = tn_rand(&tnet->rnd_gen);    // New random exchange ID

   opt = dhcp_fill_header(ni);
   if(opt == NULL)
      return -EINVAL;

   if(msg_type == DHCP_RELEASE) //-- Add-on for the func dhcp_fill_header()
      memcpy((char*)&dh->dh_ciaddr, (char*)&ni->ip_addr.s__addr, 4);

   *opt++ = DHCP_OPT_MSG_TYPE;
   *opt++ = 1;
   *opt++ = (unsigned char)msg_type;

   *opt++ = DHCP_OPT_SRV_IDENTIFIER;
   *opt++ = sizeof(unsigned int);
   memcpy(opt, &dhi->dhcp_server_addr, sizeof(unsigned int));
   opt += sizeof(unsigned int);

   if(msg_type == DHCP_DECLINE)
   {
      *opt++ = DHCP_OPT_REQUESTED_IP_ADDR;
      *opt++ = sizeof(unsigned int);
      memcpy(opt, &dhi->d_offer_ip_addr, sizeof(unsigned int));
      opt += sizeof(unsigned int);
   }

   if(msg)
   {
      ptr_end = &dh->dh_opt[DHCP_OPTIONS_SIZE - 1];
      len = strlen(msg);

      len = _min(len, 255);
      len = _min(len, (int)(ptr_end - opt - 3));
      *opt++ = DHCP_OPT_MSG;
      *opt++ = len;
      memcpy(opt, msg, len);
      opt += len;
   }
   *opt++ = DHCP_OPT_END;

   //   Send       DHCP_RELEASE
   //   Broadcast  DHCP_DECLINE

   s_a.sin_len          = sizeof(struct sockaddr__in);
   s_a.sin_family       = AF_INET;
   s_a.sin_port         = htons((unsigned short)DHCP_SERVER_PORT);

   if(msg_type == DHCP_RELEASE)
      s_a.sin_addr.s__addr = dhi->dhcp_server_addr;
   else
      s_a.sin_addr.s__addr = _INADDR_BROADCAST;     //-- 0xFFFFFFFF

   return s_sendto(dhi->s,
                  (unsigned char*)dhi->dhcp_dh,
                   opt - (unsigned char*)dhi->dhcp_dh, // len
                   0,
                  (struct _sockaddr *) &s_a,
                   s_a.sin_len);
}

//----------------------------------------------------------------------------
static int dhcp_parse_offer(TN_NETIF * ni)
{
   volatile unsigned int tmp_ui;
   unsigned char * opt;
   DHCPINFO * dhi;
   struct dhcp * dh;
   int got_offer;
   int len;

   if(ni == NULL)
      return FALSE;
   dhi = ni->dhi;
   if(dhi == NULL)
      return FALSE;
   dh = dhi->dhcp_dh;
   if(dh == NULL)
      return FALSE;

   got_offer = FALSE;

   opt = (unsigned char *)&dh->dh_opt[4];

   while(opt < dh->dh_opt + sizeof(dh->dh_opt))
   {
      switch (*opt)
      {
         case DHCP_OPT_PAD:

            opt++;
            continue;

         case DHCP_OPT_SUBNET_MASK:

            memcpy(&dhi->d_subnet_mask, opt + 2, 4); //-- In the network byte order
            break;

         case DHCP_OPT_ROUTERS_ON_SNET:

            memcpy(&dhi->d_router_addr, opt + 2, 4);
            break;

         case DHCP_OPT_SRV_IDENTIFIER:

            memcpy(&dhi->d_dhcp_server_addr, opt + 2, 4);
            break;

         case DHCP_OPT_DNS_SRV:

            dhi->d_dns_count = 0;
            for(len = 0; len < *(opt+1); len += sizeof(unsigned int))
            {
               memcpy((void*)&tmp_ui, opt + 2 + len, 4);
               if(dhi->d_dns_count < DHCP_MAX_DNS)
               {
                  dhi->d_dns_addr[dhi->d_dns_count] = tmp_ui;
                  dhi->d_dns_count++;
               }
            }

            break;

         case DHCP_OPT_IP_ADDR_LEASE_TIME:

            memcpy((void*)&tmp_ui, opt + 2, 4);
            len = tmp_ui;                 //-- just to make IAR compiler happy
            dhi->d_lease_time = ntohl(len);
            break;

         case DHCP_OPT_T1_VALUE:

            memcpy((void*)&tmp_ui, opt + 2, 4);
            len = tmp_ui;                 //-- just to make IAR compiler happy
            dhi->d_renewal_time = ntohl(len);
            break;

         case DHCP_OPT_T2_VALUE:

            memcpy((void*)&tmp_ui, opt + 2, 4);
            len = tmp_ui;                 //-- just to make IAR compiler happy
            dhi->d_rebind_time = ntohl(len);
            break;

         case DHCP_OPT_BROADCAST_ADDR:

            memcpy((void*)&tmp_ui, opt + 2, 4);
            dhi->d_broadcast_addr = tmp_ui;
            break;

         case DHCP_OPT_MSG_TYPE:

            if(opt[2] == DHCP_OFFER)
            {
               got_offer = TRUE;

               memcpy(&dhi->d_offer_ip_addr, (char*)&dh->dh_yiaddr, 4);
            }
            break;

         case DHCP_OPT_END:

            return got_offer;

         default:

            break;
      }
      opt += *(opt+1) + 2;
   }

   return got_offer;
}

//----------------------------------------------------------------------------
static void dhcp_set_sockets_ip_addr(TN_NETIF * ni,
                                     struct inpcb * head,
                                     unsigned int new_ip_addr)
{
   TN_INTSAVE_DATA
   struct inpcb * inp;

   if(ni->ip_addr.s__addr == new_ip_addr && new_ip_addr != 0)
      return;

   tn_disable_interrupt();

   for(inp = head->inp_next; inp != head; inp = inp->inp_next)
   {
      if(inp->inp_laddr.s__addr != _INADDR_ANY)
         inp->inp_laddr.s__addr = new_ip_addr;
   }

   tn_enable_interrupt();
}

//----------------------------------------------------------------------------
static void dhcp_debug_dump_lease(TN_NETIF * ni, DHCPINFO * dhi)
{
   char sbuf[20];
   int i;

   strcpy(dbg_buf, "\r\n--- DHCP results --- \r\n\r\n");
   strcat(dbg_buf, "ip_addr: ");
   ip4_num_to_str(sbuf, ntohl(ni->ip_addr.s__addr));
   strcat(dbg_buf, sbuf);
   strcat(dbg_buf, "\r\n");

   dbg_send(dbg_buf);

   strcpy(dbg_buf, "net_mask: ");
   ip4_num_to_str(sbuf, ntohl(ni->netmask.s__addr));
   strcat(dbg_buf, sbuf);
   strcat(dbg_buf, "\r\n");

   dbg_send(dbg_buf);

   strcpy(dbg_buf, "gateway: ");
   ip4_num_to_str(sbuf, ntohl(ni->ip_gateway.s__addr));
   strcat(dbg_buf, sbuf);
   strcat(dbg_buf, "\r\n");

   dbg_send(dbg_buf);

   strcpy(dbg_buf, "broadaddr: ");
   ip4_num_to_str(sbuf, ntohl(ni->if_broadaddr.s__addr));
   strcat(dbg_buf, sbuf);
   strcat(dbg_buf, "\r\n");

   dbg_send(dbg_buf);

   strcpy(dbg_buf, "dhcp_server: ");
   ip4_num_to_str(sbuf, ntohl(dhi->dhcp_server_addr));
   strcat(dbg_buf, sbuf);
   strcat(dbg_buf, "\r\n");

   dbg_send(dbg_buf);

   tn_snprintf(dbg_buf, 95, "time: lease = %d renewal = %d rebind = %d\r\n",
                            dhi->lease_time,
                            dhi->renewal_time,
                            dhi->rebind_time);
   dbg_send(dbg_buf);

   tn_snprintf(dbg_buf, 95, "dns_count = %d\r\n",
                             dhi->dns_count);
   dbg_send(dbg_buf);

   for(i=0; i < dhi->dns_count; i++)
   {
      ip4_num_to_str(sbuf, ntohl(dhi->dns_addr[i]));
      tn_snprintf(dbg_buf, 95, "DNS %d: %s\r\n",
                                i, sbuf);
      dbg_send(dbg_buf);
   }
   dbg_send("\r\n");
}

//----------------------------------------------------------------------------
int dhcp_record_lease(TN_NET * tnet, TN_NETIF * ni)
{
   TN_INTSAVE_DATA
   unsigned int new_ip_addr = 0;
   int i;
   DHCPINFO * dhi;
   struct dhcp * dh;

   if(ni == NULL)
      return FALSE;
   dhi = ni->dhi;
   if(dhi == NULL)
      return FALSE;
   dh = dhi->dhcp_dh;
   if(dh == NULL)
      return FALSE;

  //--- Set/change iface addr - atomic operation

   if(dhi->d_broadcast_addr == 0)
      dhi->d_broadcast_addr = dhi->d_router_addr | ~dhi->d_subnet_mask;

   tn_disable_interrupt();

   ni->ip_addr.s__addr      = dhi->d_offer_ip_addr;   //-- Device(interface) IP4 addr
   ni->netmask.s__addr      = dhi->d_subnet_mask;    //-- Subnet Mask
   ni->ip_gateway.s__addr   = dhi->d_router_addr;    //-- Gateway
   ni->if_broadaddr.s__addr = dhi->d_broadcast_addr; //-- LAN Broadcast

   tn_enable_interrupt();

   //-- Change (if needs) the own IP address in the all active sockets - atomic operation(s)

    //--- UDP sockets
   dhcp_set_sockets_ip_addr(ni,
                             &tnet->udb,
                             new_ip_addr);
#ifdef TN_TCP

    //--- TCP sockets
   dhcp_set_sockets_ip_addr(ni,
                            &tnet->tcb,
                            new_ip_addr);
#endif

   DHCP_LOCK

   if(dhi->d_lease_time == DHCP_INFINITE_LEASE)
   {
      dhi->t1_timeout = 0;
      dhi->t2_timeout = 0;
   }
   else
   {
      if(dhi->d_renewal_time == 0)
         dhi->d_renewal_time = dhi->d_lease_time >> 1;     // default T1 time
      if(dhi->d_rebind_time == 0)
         dhi->d_rebind_time  = (dhi->d_lease_time * 7)>>3;  // default T2 time

      //-- ToDo - add a checking for the 32-bit overflow when * 10

      dhi->t1_timeout = dhi->d_renewal_time * 10; //-- Each internal timer tick -100 ms
      dhi->t2_timeout = dhi->d_rebind_time  * 10;
   }

   DHCP_UNLOCK

//----------------------

   dhi->dhcp_server_addr = dhi->d_dhcp_server_addr;
   dhi->lease_time       = dhi->d_lease_time;
   dhi->renewal_time     = dhi->d_renewal_time;
   dhi->rebind_time      = dhi->d_rebind_time;
   dhi->dns_count        = dhi->d_dns_count;
   for(i=0; i < dhi->dns_count; i++)
      dhi->dns_addr[i] = dhi->d_dns_addr[i];

  //--- Here a right place to store the DCHP results in the non-volatile memory

   //-- For example

  /*
   eeprom_ni_ip_addr        = ni->ip_addr.s__addr;      //-- Device (interface) IP4 addr
   eeprom_ni_netmask        = ni->netmask.s__addr;      //-- Subnet Mask
   eeprom_ni_ip_gateway     = ni->ip_gateway.s__addr;   //-- Gateway(router)
   eeprom_ni_if_broadaddr   = ni->if_broadaddr.s__addr; //-- LAN Broadcast
   eeprom_dhcp_server_addr  = dhi->dhcp_server_addr;
   eeprom_lease_time        = dhi->lease_time;
   eeprom_renewal_time      = dhi->renewal_time;
   eeprom_rebind_time       = dhi->rebind_time;
   eeprom_dns_count         = dhi->dns_count;
   for(i=0; i < dhi->dns_count; i++)
       eeprom_dns_addr[i] = dhi->dns_addr[i];
  */

  //-- Debug / demo only

   dhcp_debug_dump_lease(ni, dhi);

   return TRUE;
}

//----------------------------------------------------------------------------
static int dhcp_set_init_timeout(TN_NET * tnet, TN_NETIF * ni)
{
   DHCPINFO * dhi;
   int i;
   static const unsigned char
           dhcp_retries_coeff[DHCP_MAX_INIT_RETRIES] = {1, 2, 4, 8};

   if(ni == NULL)
      return -EINVAL;
   dhi = ni->dhi;
   if(dhi == NULL)
      return -EINVAL;

   i = _min(dhi->init_num_retries - 1, DHCP_MAX_INIT_RETRIES - 1);
   if(i < 0)
      i = 0;

   return (DHCP_INIT_PERIOD * dhcp_retries_coeff[i]) +
             (tn_rand(&tnet->rnd_gen)%1000);
}

//----------------------------------------------------------------------------
static int dhcp_chk_dup_IP(TN_NET * tnet, TN_NETIF * ni)
{
   TN_INTSAVE_DATA
   int i;
   unsigned int save_ni_ip_addr;
   int rc;

   rc = TRUE;

   save_ni_ip_addr = ni->ip_addr.s__addr;

//----------------------------
   tn_disable_interrupt();

   ni->ip_addr.s__addr = ni->dhi->d_offer_ip_addr;

   tn_enable_interrupt();
//---------------------------

  //--- Gratuitous ARP
   for(i = 0; i < 2; i++)
   {
      arp_request(tnet,
                  ni,
                  ni->ip_addr.s__addr);
      tn_task_sleep(50);
   }
   tn_task_sleep(100);

//----------------------------
   tn_disable_interrupt();

   rc = ni->dup_own_ip_addr;
   ni->dup_own_ip_addr = FALSE;
   ni->ip_addr.s__addr = save_ni_ip_addr;

   tn_enable_interrupt();
//---------------------------

   return rc;
}

//----------------------------------------------------------------------------
int dhcp_ready(TN_NETIF * ni)
{
   if(ni == NULL)
      return FALSE;

   if(ni->dhi)
   {
      if(ni->dhi->state == DHCP_ST_BOUND)
         return TRUE;
   }

   return FALSE;
}

//----------------------------------------------------------------------------
void dhcp_timer_func(TN_NETINFO * tneti, int cnt)
{
   TN_NET * tnet = NULL;
   struct tn_netif * ni;
   struct udp_socket * so_udp;
   struct socket * so;
   volatile unsigned int tmp_ui;

   tnet = tneti->tnet;
   if(tnet == NULL)
       return;

   for(ni = tnet->netif_list; ni != NULL; ni = ni->next)
   {
      if(ni->dhi)
      {
         if(ni->dhi->s)
         {
            so = (struct socket *)ni->dhi->s;
            so_udp = (struct udp_socket *)so->so_tdata;
            if(so_udp)
            {
               DHCP_LOCK
              //---
               if(ni->dhi->t1_timeout > 0)
               {
                  ni->dhi->t1_timeout--;
                  if(ni->dhi->t1_timeout == 0) //-- DHCP_T1_EXPIRED
                  {
               //--- Send to the DHCP socket rx queue
                     tmp_ui = DHCP_T1_EXPIRED_EVENT;
                     tn_queue_send_polling(&so_udp->queueRx, (void*)tmp_ui);
                  }
               }
             //----
               if(ni->dhi->t2_timeout > 0)
               {
                  ni->dhi->t2_timeout--;
                  if(ni->dhi->t2_timeout == 0) //-- DHCP_T2_EXPIRED
                  {
               //--- Send to the DHCP socket rx queue
                     tmp_ui = DHCP_T2_EXPIRED_EVENT;
                     tn_queue_send_polling(&so_udp->queueRx, (void*)tmp_ui);
                  }
               }
               DHCP_UNLOCK
            }
         }
      }
   }
}

//----------------------------------------------------------------------------
int dhcp_init(TN_NET * tnet,
              TN_NETIF * ni,
              DHCPPAR * fparam,
              struct dhcp * dhcp_dh)
{
   DHCPINFO * dhi;

   ni->dhi = (struct _DHCPINFO *)tn_net_alloc_mid1(tnet, MB_NOWAIT);
   if(ni->dhi == NULL)
      return -ENOMEM;

   dhi = ni->dhi;

   memset(dhi, 0, sizeof(DHCPINFO));

   dhi->dhcp_dh = dhcp_dh;

/*  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

   //-- if you store a DHCP data in the non-volatile memory, read it and
   //-- fill the interface and the 'dhi' structures fields

      //--   For example:

   ni->ip_addr.s__addr      = eeprom_ni_ip_addr;        //-- Device (interface) IP4 addr
   ni->netmask.s__addr      = eeprom_ni_netmask;        //-- Subnet Mask
   ni->ip_gateway.s__addr   = eeprom_ni_ip_gateway;     //-- Gateway(router)
   ni->if_broadaddr.s__addr = eeprom_ni_if_broadaddr;   //-- LAN Broadcast

   dhi->dhcp_server_addr    = eeprom_dhcp_server_addr;
   dhi->lease_time          = eeprom_lease_time;
   dhi->renewal_time        = eeprom_renewal_time;
   dhi->rebind_time         = eeprom_rebind_time;
   dhi->dns_count           = eeprom_dns_count;
   for(i=0; i < dhi->dns_count; i++)
       eeprom_dns_addr[i] = dhi->dns_addr[i];

   dhi->d_curr_ip_addr = ni->ip_addr.s__addr;

   //-- If you want re-use the same IP addr, set DHCP state as DHCP_ST_INITREBOOT

   dhi->state = DHCP_ST_INITREBOOT;


 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

   Otherwise use a DHCP_ST_INIT and source code below

 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/

   dhi->state = DHCP_ST_INIT;

  //-- If you want to suggest a lease time (RFC2131: 0xFFFFFFFF - infinite)
  //-- set it here

     //--   For example:
   // dhi->lease_time  = DHCP_INFINITE_LEASE;

   dhi->d_lease_time = dhi->lease_time;

   //-- Create DHCP task (DHCP socket - inside task func)

   fparam->tnet = tnet;
   fparam->ni   = ni;

   tn_sem_create(&ni->dhcp_sem, 1, 1);

   tn_task_create(&ni->dhcp_task_rx,             //-- task TCB
                 dhcp_task_func,                 //-- task function
                 DHCP_TASK_PRIORITY,             //-- task priority
                 &(ni->dhcp_task_rx_stack        //-- task stack first addr in memory
                     [DHCP_TASK_STACK_SIZE-1]),
                 DHCP_TASK_STACK_SIZE,           //-- task stack size (in int,not bytes)
                 (void*)fparam,                  //-- task function parameter
                 TN_TASK_START_ON_CREATION);     //-- Creation option

   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
static void dhcp_clear_iface_ip_addr(TN_NETIF * ni)
{
   TN_INTSAVE_DATA

   tn_disable_interrupt();

   ni->ip_addr.s__addr = 0;

   tn_enable_interrupt();
}

//----------------------------------------------------------------------------
static void dhcp_prepare_init_state(TN_NETIF * ni, DHCPINFO * dhi)
{
//--- Each time before set DHCP state as DHCP_ST_INIT

   dhi->d_curr_ip_addr   = 0;
   dhi->d_offer_ip_addr  = 0;
   dhi->d_lease_time     = dhi->lease_time;

   dhi->rx_timeout       = 2;
   dhi->init_num_retries = 0;

   DHCP_LOCK

   dhi->t1_timeout   = 0;
   dhi->t2_timeout   = 0;

   DHCP_UNLOCK
}

//----------------------------------------------------------------------------
void dhcp_task_func(void * par)
{
   TN_NETIF * ni;
   TN_NET * tnet;
   DHCPINFO * dhi = NULL;
   struct dhcp * dh = NULL;
   int rc;
   unsigned char * ptr;
   int m_type;
   struct sockaddr__in s_a;

   ni   = ((DHCPPAR*)par)->ni;
   tnet = ((DHCPPAR*)par)->tnet;

   for(;;) //--Single iteration loop
   {
      if(ni == NULL)
         break;
      dhi = ni->dhi;
      if(dhi == NULL)
         break;
      dh = dhi->dhcp_dh;
      break; //-- Leave the loop any case
   }

   if(dh == NULL)
   {
      for(;;)
         tn_task_sleep(1000);
   }

   //-- A DHCP client

   for(;;)
   {
      if(dhi->s == 0) //-- Start
      {
         dhi->s = s_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
         if(dhi->s != -1 && dhi->s != 0)
         {
           //--- Call s_bind() to set a local port for the UDP socket.

            s_a.sin_len          = sizeof(struct sockaddr__in);
            s_a.sin_family       = AF_INET;
            s_a.sin_port         = htons((unsigned short)DHCP_CLIENT_PORT);
            s_a.sin_addr.s__addr = _INADDR_ANY;

            rc = s_bind(dhi->s, (struct _sockaddr *)&s_a, s_a.sin_len);
            if(rc != TERR_NO_ERR)
            {
               s_close(dhi->s);
               dhi->s = 0;
               tn_task_sleep(500);
               continue;
            }

            dhi->rx_timeout = 500;//1500;
         }
      }
      else //-- Regular operating
      {
         //--- Set rx timeout

         s_setsockopt(dhi->s,
                      0,                          //-- level,
                      SO_RCVTIMEO,                //-- name,
                      (unsigned char *)&dhi->rx_timeout,
                      sizeof(int));               //-- size

         rc = s_recvfrom(dhi->s,
                         (unsigned char*)dhi->dhcp_dh,
                         sizeof(struct dhcp), //-- An actual rx len will be always <= this value
                         0,                   //-- flags
                         NULL,
                         NULL);

         if(rc > 0) //-- Rx some data
         {
            if(rc >= DHCP_MIN_SIZE &&                   // packet large enough
                dh->dh_op  == BOOTP_REPLY &&            // got a BOOTP reply
                  ntohl(dh->dh_xid) == dhi->d_xid &&    // got our exchange ID
                       memcmp(dh->dh_chaddr,
                           ni->hw_addr, DHCP_HLEN_ETHERNET)== 0)   // correct hardware addr
            {
               ptr = &dh->dh_opt[4];
               m_type = (ptr[0] == DHCP_OPT_MSG_TYPE && ptr[1] == 1);

               if(m_type && ptr[2] == DHCP_ACK)      // ACK
               {
                  if(dhi->state == DHCP_ST_REBOOTING ||
                       dhi->state == DHCP_ST_REQUESTING ||
                          dhi->state == DHCP_ST_RENEWING ||
                             dhi->state == DHCP_ST_REBINDING)
                  {
                     if(dhcp_chk_dup_IP(tnet, ni) == FALSE)
                     {
                        dhcp_record_lease(tnet, ni);
                        dhi->state = DHCP_ST_BOUND;
                     }
                     else  //-- decline
                     {
                        dhcp_clear_iface_ip_addr(ni);

                        dhcp_release_decline(tnet, ni, DHCP_DECLINE, "IP is not free");

                        //--RFC 2131 Timeout SHOLD be at least 10 s
                        dhi->rx_timeout = dhcp_set_init_timeout(tnet, ni);

                        dhcp_prepare_init_state(ni, dhi);
                        dhi->state = DHCP_ST_INIT;
                     }
                  }
               }
               else if(m_type && ptr[2] == DHCP_NAK) // NAK
               {
                  if(dhi->state == DHCP_ST_REBOOTING ||
                     (dhi->state == DHCP_ST_REQUESTING &&
                     //-- This is a selected server answer
                      memcmp(&dhi->d_dhcp_server_addr, ptr + 1, 4) == 0) ||
                         dhi->state == DHCP_ST_RENEWING ||
                            dhi->state == DHCP_ST_REBINDING)
                  {
                     dhi->rx_timeout = dhcp_set_init_timeout(tnet, ni);
                     dhcp_prepare_init_state(ni, dhi);
                     dhi->state = DHCP_ST_INIT;
                  }
               }
               else  //-- treat it as OFFER
               {
                  if(dhi->state == DHCP_ST_SELECTING)
                  {
                     //-- RFC2131 - "Collect replies, Select offer"
                     //-- A very simple approach is used here - take a first valid responce

                     if(dhcp_parse_offer(ni))
                     {
                        dhi->rx_timeout = 200;
                        dhcp_request(tnet, ni);  // DHCPREQUEST
                        dhi->state = DHCP_ST_REQUESTING;
                     }
                     else
                     {
                        dhi->rx_timeout = 200;
                        continue;
                     }
                  }
               }
            }
         }
         else
         {
            if(rc == -ETIMEOUT) //-- This is a rx timeout
            {
               if(dhi->state == DHCP_ST_INITREBOOT)
               {
                  dhi->rx_timeout = 500;
                  dhcp_request(tnet, ni);
                  dhi->state = DHCP_ST_REBOOTING;
               }
               else if(dhi->state == DHCP_ST_INIT)
               {
                  dhi->init_num_retries++;
                  if(dhi->init_num_retries > DHCP_MAX_INIT_RETRIES)
                  {
                     //-- DHCP failed

                     //-- Implement the actions according to your project
                  }
                  dhi->rx_timeout = 1000;
                  dhcp_discover(tnet, ni);
                  dhi->state = DHCP_ST_SELECTING;
               }
               else if(dhi->state == DHCP_ST_REBOOTING ||
                          dhi->state == DHCP_ST_SELECTING ||
                             dhi->state == DHCP_ST_REQUESTING ||
                                dhi->state == DHCP_ST_RENEWING)
               {
                  dhi->rx_timeout = dhcp_set_init_timeout(tnet, ni);
                  dhcp_prepare_init_state(ni, dhi);
                  dhi->state = DHCP_ST_INIT;
               }
            }
            else if(rc == -EDHCPT1)  //-- DHCP_T1_EXPIRED
            {
               if(dhi->renewal_time != DHCP_INFINITE_LEASE)
               {
                  if(dhi->state == DHCP_ST_BOUND)
                  {
                     dhi->rx_timeout = 500;
                     dhcp_request(tnet, ni);  // DHCPREQUEST
                     dhi->state = DHCP_ST_RENEWING;
                  }
               }
            }
            else if(rc == -EDHCPT2)  //-- DHCP_T2_EXPIRED
            {
               if(dhi->renewal_time != DHCP_INFINITE_LEASE)
               {
                  if(dhi->state == DHCP_ST_RENEWING)
                  {
                     dhi->rx_timeout = 500;
                     dhcp_request(tnet, ni);
                     dhi->state = DHCP_ST_REBINDING;
                  }
                  else if(dhi->state == DHCP_ST_REBINDING)
                  {
                     dhi->rx_timeout = dhcp_set_init_timeout(tnet, ni);
                     dhcp_prepare_init_state(ni, dhi);
                     dhi->state = DHCP_ST_INIT;
                  }
               }
            }
         }
     //--- s != 0
      }
   }
}

#endif //-- #ifdef TN_DHCP

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------





