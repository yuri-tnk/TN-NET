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
 * Copyright (c) 1982, 1986, 1988, 1993
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

#include <string.h>  //-- memset

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
#include "tn_net_utils.h"
#include "tn_eth.h"
#include "tn_net_func.h"
#include "tn_net_mem_func.h"

#include "dbg_func.h"


#if defined (__ICCARM__)   // IAR ARM
#pragma diag_suppress=Pa039
#endif

const u_char etherbroadcastaddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


//--- Local functions prototypes

static TN_ARPENTRY * arp_lookup(TN_NET * tnet,
                         TN_NETIF *ni,
                         unsigned long ip_to_find,
                         int create_new_entry);
//-----------------

#define ARP_LOCK       tn_sem_acquire(ni->arp_sem, TN_WAIT_INFINITE);
#define ARP_UNLOCK     tn_sem_signal(ni->arp_sem);


//----------------------------------------------------------------------------
//-- The function should create an Ethernet header and then write to drv
//-- Creating an Eth header, filling it, combine mbufs
//----------------------------------------------------------------------------
TN_MBUF * add_eth_header(TN_NET * tnet,
                         TN_MBUF * mb,             //-- mbuf to send
                         unsigned char * eth_dst,  //-- Net order
                         unsigned char * eth_src,  //-- Net order
                         int eth_type)             //-- Host order
{
   u_short type;
   TN_MBUF * mb0;
   struct ether_header * eh = NULL;


   mb0 = (TN_MBUF *)tn_net_alloc_tx_min(tnet, MB_NOWAIT);
   if(mb0 == NULL)
   {
      m_freem(tnet, mb);
      return NULL;
   }
   //-- 'mb_hdr' is always aligned to 'int'
   bzero_dw((unsigned int*)mb0, sizeof(TN_MBUF)>>2);

   //--- Only an Ethernet mbuf descriptor does NOT has a data buffer

   mb0->m_data = (unsigned char*)mb0 + 2 +  //-- short m_flags;
                                       2 +  //-- short m_len;
                   sizeof(struct mbuf *) +  //-- struct mbuf * m_next;
                   sizeof(unsigned char *); //-- unsigned char * m_data;

   eh = (struct ether_header *)(mb0->m_data);

   type = htons((u_short)eth_type);
   bcopy((unsigned char *)&type,(unsigned char *)&eh->ether_type, sizeof(eh->ether_type));
   bcopy((unsigned char *)eth_dst, (unsigned char *)eh->ether_dhost, MAC_SIZE);
   bcopy((unsigned char *)eth_src, (unsigned char *)eh->ether_shost, MAC_SIZE);

   mb->m_flags &= ~M_PKTHDR; //-- 'm_tlen' in 'mb' still valid

   mb0->m_next = mb;
   mb0->m_len  = sizeof(struct ether_header);
   mb0->m_flags = M_ETHHDR | //-- it means that it has't a data buf
                  M_PKTHDR;

   return mb0;
}

//----------------------------------------------------------------------------
void arp_init(TN_NETIF * ni,
              TN_ARPENTRY * arp_arr,
              int nelem,
              TN_SEM * p_arp_sem)
{

   if(ni == NULL || arp_arr == NULL || nelem < 2 || p_arp_sem == NULL)
   {
      ni->arp_arr = NULL;
      ni->arp_arr_capacity = 0;
      ni->arp_sem = NULL;
      return;
   }

   s_memset(arp_arr, 0, sizeof(TN_ARPENTRY) * nelem);
   ni->arp_arr = arp_arr;
   ni->arp_arr_capacity = nelem;

   ni->dup_own_ip_addr = 0;

  //--- Create ARP lock/unlock semaphore

   p_arp_sem->id_sem = 0;
   tn_sem_create(p_arp_sem,
                         1,   //-- Start value
                         1);  //-- Max value

   ni->arp_sem = p_arp_sem;
}

//----------------------------------------------------------------------------
// mb->m_data points to the Ethernet header
// Rx mbuf is big enough to fit 'struct ether_arp'
//----------------------------------------------------------------------------
void arp_input(TN_NET * tnet, TN_NETIF * ni, struct mbuf * mb)
{
   struct ether_header * eh;
   struct ether_arp * ea;
   TN_ARPENTRY * arpe;
   TN_MBUF * mb0;
   struct in__addr isaddr;
   struct in__addr itaddr;
   struct in__addr myaddr;
   int op;

   mb0 = NULL;
   if(mb->m_tlen < sizeof(struct ether_arp))   // drop short packets
   {
       m_freem(tnet, mb);
       return;
   }

   eh = (struct ether_header *)(mb->m_data);
   ea = (struct ether_arp *)(mb->m_data + sizeof(struct ether_header));
   op = ntohs(ea->arp_op);

   bcopy((unsigned char *)ea->arp_spa, (unsigned char *)&isaddr, sizeof (struct in__addr));
   bcopy((unsigned char *)ea->arp_tpa, (unsigned char *)&itaddr, sizeof (struct in__addr));

 //-------------------------------------------------------------------
   if(s_memcmp((unsigned char *)ea->arp_sha, (unsigned char *)etherbroadcastaddr, MAC_SIZE) == 0)
   {
     //-- Err - ether address is broadcast for IP address

      m_freem(tnet, mb);
      return;
   }

   myaddr.s__addr = ni->ip_addr.s__addr; //--- My addr from curr 'ni' - may be 0 (not yet assigned)

   if(isaddr.s__addr == myaddr.s__addr)
   {
      //-- Err - duplicate IP address - but we accept it
      bcopy((unsigned char *)&myaddr, (unsigned char *)&itaddr, sizeof(itaddr));

      ni->dup_own_ip_addr = TRUE;
   }
   else
   {
      ARP_LOCK

   //-- If the packet is for this iface - enable creating a new entry

      arpe = arp_lookup(tnet, ni, isaddr.s__addr, itaddr.s__addr == myaddr.s__addr);
      if(arpe != NULL)
      {
    //-- Any case copy hw addr to the entry

         bcopy((unsigned char *)ea->arp_sha, arpe->mac_addr, MAC_SIZE);

        //-- Even we created an entry in advance, it now in the ARP_SOLVED state
        //-- If entry is new, 'la_hold' == NULL

         arpe->flags &= ~ARP_STATE_MASK;
         arpe->flags |= ARP_SOLVED;

    //--------------------------
         arpe->rt_expire = tn_time_sec() + ARP_TIME_KEEP;
         arpe->la_asked = 0;

         if(arpe->la_hold) //-- Have data to send
         {
            mb0 = add_eth_header(tnet,
                                 arpe->la_hold,   //-- mbuf to send
                                 arpe->mac_addr,  //-- Ethernet dst - Net order
                                 ni->hw_addr,     //-- Ethernet src - Net order
                                 ETHERTYPE_IP);   //-- Ethernet type- Host order
            //-- Data to send - to Ethernet driver
            if(mb0 != NULL)
            {
               arpe->la_hold = NULL;
               ni->drv_wr(tnet, ni, mb0);
            }
         }
#ifdef TN_ARP_EXTRA_LAHOLD         
         if(arpe->la_hold1) //-- Have data to send
         {
            mb0 = add_eth_header(tnet,
                                 arpe->la_hold1,   //-- mbuf to send
                                 arpe->mac_addr,  //-- Ethernet dst - Net order
                                 ni->hw_addr,     //-- Ethernet src - Net order
                                 ETHERTYPE_IP);   //-- Ethernet type- Host order
            //-- Data to send - to Ethernet driver
            if(mb0 != NULL)
            {
               arpe->la_hold1 = NULL;
               ni->drv_wr(tnet, ni, mb0);
            }
         }
#endif         
      }

      ARP_UNLOCK
   }

   //--- If here - replay(if any)

   if(op == ARPOP_REQUEST)
   {
     //-- Send responce (op == ARPOP_REQUEST)

      if(itaddr.s__addr == myaddr.s__addr)
      {
                   // I am the target
         bcopy((unsigned char *)ea->arp_sha, (unsigned char *)ea->arp_tha, MAC_SIZE);
         bcopy((unsigned char *)ni->hw_addr, (unsigned char *)ea->arp_sha, MAC_SIZE);
      }
      else //-- No proxy ARP support. Also here if  myaddr.s__addr == 0
      {
         m_freem(tnet, mb);
         return;
      }

      bcopy((unsigned char *)ea->arp_spa, (unsigned char *)ea->arp_tpa, sizeof(ea->arp_spa));
      bcopy((unsigned char *)&itaddr,     (unsigned char *)ea->arp_spa, sizeof(ea->arp_spa));

      ea->arp_op   = htons(ARPOP_REPLY);
      ea->arp_pro  = htons(ETHERTYPE_IP); // BSD - let's be sure!

      //-- Update Ethernet header ----

      bcopy((unsigned char *)ea->arp_tha, (unsigned char *)eh->ether_dhost, sizeof(eh->ether_dhost));
      bcopy((unsigned char *)ni->hw_addr, (unsigned char *)eh->ether_shost, sizeof(eh->ether_shost));
      eh->ether_type = htons(ETHERTYPE_ARP);

      //------------------------------
      mb->m_len = sizeof(struct ether_header) + sizeof(struct ether_arp);
      ni->drv_wr(tnet, ni, mb);
   }
   else
      m_freem(tnet, mb);
}

//----------------------------------------------------------------------------
int arp_resolve(TN_NET * tnet,
                TN_NETIF * ni,
                TN_MBUF * mb,
                struct in__addr arp_ip_addr,
                unsigned char * dst) //-- [OUT]
{

   TN_ARPENTRY * arpe;
   int do_ARP_req = FALSE;

   if(mb->m_flags & M_BCAST)         // broadcast
   {
      bcopy((unsigned char *)etherbroadcastaddr, (unsigned char *)dst, MAC_SIZE);
      return TRUE;
   }

   if(mb->m_flags & M_MCAST)   // multicast
   {
     //--  ETHER_MAP_IP_MULTICAST macro

      unsigned char * ptr;
      ptr = (unsigned char *)&arp_ip_addr.s__addr;

      dst[0] = 0x01;
      dst[1] = 0x00;
      dst[2] = 0x5e;
      dst[3] = ptr[1] & 0x7f;
      dst[4] = ptr[2];
      dst[5] = ptr[3];

      return TRUE;
   }

   ARP_LOCK

   arpe = arp_lookup(tnet, ni, arp_ip_addr.s__addr, TRUE); //-- enable create new entry
   if(arpe == NULL)
   {
      ARP_UNLOCK
      m_freem(tnet, mb);
      return FALSE;
   }
   // Check that the address family and length are valid, the address
   // is resolved; otherwise, try to resolve.

   if(((arpe->flags & ARP_PERMANENT) || arpe->rt_expire > tn_time_sec()) &&
       ((arpe->flags & ARP_STATE_MASK) == ARP_SOLVED))
   {
      if(dst)
         bcopy(arpe->mac_addr, dst, MAC_SIZE);
      ARP_UNLOCK
      return TRUE;
   }

   //--- If we are here - we didn't solved the entry (includes new entry)
                                               //-- with status ARP_PENDING

   //-- This is an arptab entry, but there is no Ethernet address
   //-- response (yet).  Replace the held 'mbuf' with this latest one.

   if(arpe->la_hold)
#ifdef TN_ARP_EXTRA_LAHOLD   
   {
     if(arpe->la_hold1)
       m_freem(tnet, arpe->la_hold1);
     arpe->la_hold1 = arpe->la_hold;   
   }
#else
   m_freem(tnet, arpe->la_hold);
#endif   
   arpe->la_hold = mb;

   if(arpe->la_asked == 0 || arpe->rt_expire != tn_time_sec())
   {
      arpe->rt_expire = tn_time_sec();
      if(arpe->la_asked < ARP_MAX_TRIES)
      {
         arpe->la_asked++;
         do_ARP_req = TRUE;
      }
      else
      {
         arpe->rt_expire += ARP_TIME_DOWN;
         arpe->la_asked = 0;
      }
   }

   ARP_UNLOCK

   if(do_ARP_req)
      arp_request(tnet, ni, arp_ip_addr.s__addr);  //--- Target IP addr

   return FALSE;
}

//----------------------------------------------------------------------------
// Broadcast an ARP request. Caller specifies:
//        - arp header source ip address
//        - arp header target ip address
//        - arp header source ethernet address
//----------------------------------------------------------------------------
void arp_request(TN_NET * tnet,
                 TN_NETIF * ni,
                 unsigned long tip)   //--- Target IP addr (grt hw to it)
{
   struct mbuf * mb;
   struct ether_header *eh;
   struct ether_arp * ea;

   mb = mb_get(tnet, MB_MID1, MB_NOWAIT, TRUE); //-- from Tx pool
   if(mb == NULL)
      return;

   mb->m_len  = sizeof(struct ether_header) + sizeof(struct ether_arp);
   mb->m_tlen = mb->m_len;

   eh = mtod(mb, struct ether_header *);
   ea = (struct ether_arp *)(mb->m_data + sizeof(struct ether_header));

   bzero((unsigned char *)ea, sizeof (*ea));
   bcopy((unsigned char *)etherbroadcastaddr, (unsigned char *)eh->ether_dhost, MAC_SIZE);
   bcopy((unsigned char *)&ni->hw_addr, (unsigned char *)eh->ether_shost, MAC_SIZE);

   eh->ether_type = htons(ETHERTYPE_ARP);
   ea->arp_hrd = htons(ARPHRD_ETHER);
   ea->arp_pro = htons(ETHERTYPE_IP);
   ea->arp_hln = sizeof(ea->arp_sha);     // hardware address length
   ea->arp_pln = sizeof(ea->arp_spa);     // protocol address length
   ea->arp_op  = htons(ARPOP_REQUEST);

   bcopy((unsigned char *)&ni->hw_addr, (unsigned char *)&ea->arp_sha, sizeof(ea->arp_sha));
   bcopy((unsigned char *)&ni->ip_addr, (unsigned char *)&ea->arp_spa, sizeof(ea->arp_spa));
   bcopy((unsigned char *)&tip,         (unsigned char *)ea->arp_tpa,  sizeof(ea->arp_tpa));

   //-- Wr to Ethernet driver

   ni->drv_wr(tnet, ni, mb);
}

//----------------------------------------------------------------------------
static TN_ARPENTRY * arp_lookup(TN_NET * tnet,
                         TN_NETIF *ni,
                         unsigned long ip_to_find,
                         int create_new_entry)
{
   TN_ARPENTRY * arpe;
   int idx;
   int flags;
   unsigned int oldest_expire_solved  = 0xFFFFFFFF;
   unsigned int oldest_expire_pending = 0xFFFFFFFF;
   int oldest_solved_ind  = -1;
   int oldest_pending_ind = -1;
   int first_free_ind = -1;

   //--- Find entry

   for(idx = 0; idx < ni->arp_arr_capacity; idx++)
   {
      arpe = &ni->arp_arr[idx];

      if(arpe->itaddr.s__addr == ip_to_find)
         return arpe;
      else
      {
         if(create_new_entry)
         {
            flags = arpe->flags & ARP_STATE_MASK;

            if(flags == ARP_FREE) //-- 0
            {
               if(first_free_ind == -1)
                  first_free_ind = idx;
            }
            else if(flags == ARP_SOLVED)
            {
               if((arpe->flags & ARP_PERMANENT) == 0)
               {
                   //-- find oldest solved
                  if(arpe->rt_expire < oldest_expire_solved)
                  {
                     oldest_expire_solved = arpe->rt_expire;
                     oldest_solved_ind  = idx;
                  }
               }
            }
            else if(flags == ARP_PENDING)
            {
               if((arpe->flags & ARP_PERMANENT) == 0)
               {
                   //-- find oldest pending
                  if(arpe->rt_expire < oldest_expire_pending)
                  {
                     oldest_expire_pending = arpe->rt_expire;
                     oldest_pending_ind = idx;
                  }
               }
            }
         }
      }
   }
//-----
   idx = -1;
   if(create_new_entry) //-- Must create new entry
   {
      if(first_free_ind != -1)
         idx = first_free_ind;
      else if(oldest_solved_ind != -1)  //-- Lo priority to save
         idx = oldest_solved_ind;
      else if(oldest_pending_ind != -1)  //-- Hi priority to save
         idx = oldest_pending_ind;
   }

   if(idx != -1) //-- Create/replace for the new entry
   {
      arpe = &ni->arp_arr[idx];

      arpe->rt_expire = tn_time_sec();

      if(arpe->la_hold)
         m_freem(tnet, arpe->la_hold);
      arpe->la_hold = NULL;

#ifdef TN_ARP_EXTRA_LAHOLD      
      if(arpe->la_hold1)
        m_freem(tnet, arpe->la_hold1);
      arpe->la_hold1 = NULL;
#endif      
      
      arpe->itaddr.s__addr = ip_to_find;
      s_memset(arpe->mac_addr, 0, MAC_SIZE);
      arpe->flags = ARP_PENDING;
      arpe->la_asked = 0;
      return arpe;
   }

   return NULL;
}

//----------------------------------------------------------------------------
// Timeout routine.  Age arp_tab entries periodically each 5 min
//----------------------------------------------------------------------------
void arp_timer_func(TN_NETINFO * tneti, int cnt)
{
   TN_ARPENTRY * arpe;
   TN_NETIF * ni;
   volatile unsigned int time_sec;
   int i;
   int j;

   if(cnt %(10*60*5) == 0)  //-- cnt - 100 ms ticks, (10* 60* 5) = 5 min
   {
      if(tneti == NULL)
         return;

      for(j = 0; j < MAX_NETIF; j++)
      {
         ni = tneti->ni[j]; //-- netif1
         if(ni == NULL)
            continue;
         if(ni->arp_arr == NULL)
            continue;

         ARP_LOCK
         for(i = 0; i < ni->arp_arr_capacity; i++)
         {
            arpe = &ni->arp_arr[i];
            time_sec = tn_time_sec();
            if(arpe->rt_expire && arpe->rt_expire <= time_sec)
            {
              //-- Free Entry
               if(arpe->la_hold)
                  m_freem(tneti->tnet, arpe->la_hold);
#ifdef TN_ARP_EXTRA_LAHOLD                  
               if(arpe->la_hold1)
                 m_freem(tneti->tnet, arpe->la_hold1);
#endif                 
               s_memset(arpe, 0, sizeof(TN_ARPENTRY));
            }
         }
         ARP_UNLOCK
      }
   }
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


