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

#include <string.h>
#include "emac.h"
#include "LPC236x.h"

#include "../../tnkernel/tn.h"
#include "../../tnkernel/tn_port.h"
#include "../../tnkernel/tn_utils.h"

#include "../tn_net_cfg.h"
#include "../tn_net_types.h"
#include "../tn_net_pcb.h"
#include "../tn_net_mem.h"
#include "../tn_ip.h"
#include "../tn_net.h"
#include "../errno.h"
#include "../tn_mbuf.h"
#include "../tn_netif.h"
#include "../tn_net_utils.h"
#include "../tn_eth.h"
#include "../tn_net_func.h"
#include "../tn_net_mem_func.h"

#ifdef TN_DHCP
#include "../tn_dhcp.h"
#endif

#include "lpc23xx_net_emac.h"
#include "lpc23xx_phy_KS8721.h"

#include "../dbg_func.h"

extern const u_char etherbroadcastaddr[6];


//----------------------------------------------------------------------------
//  This is a place to set all network addr info for the Interface N1 (LPC23XX MAC)
//----------------------------------------------------------------------------

#ifdef TN_DHCP

const char * g_iface1_addr[] =
{
   "\x10\x0C\xCC\x22\x33\x44",  //-- Own MAC addr - as hex char string
   "0.0.0.0",                   //-- Own IP addr
   "255.255.255.0",             //-- IP addr mask
   "0.0.0.0",                   //-- Gateway
   "255.255.255.255"            //-- LAN Broadcast addr/mask
};

#else

const char * g_iface1_addr[] =
{
   "\x10\x0C\xCC\x22\x33\x44",   //-- Own MAC addr - as hex char string
   "10.101.0.125",               //-- Own IP addr
   "255.255.255.0",              //-- IP addr mask
   "10.101.0.1",                 //-- Gateway
   "10.101.0.255"                //-- LAN Broadcast addr/mask
};

/*
const char * g_iface1_addr[] =
{
   "\x10\x0C\xCC\x22\x33\x44",   //-- Own MAC addr - as hex char string
   "192.168.2.14",               //-- Own IP addr
   "255.255.255.0",              //-- IP addr mask
   "192.168.2.1",                //-- Gateway addr
   "192.168.2.255"               //-- LAN Broadcast addr/mask
};
*/
#endif

void tcp_timer_func(TN_NETINFO * tneti, int cnt);

//----------------------------------------------------------------------------
//   Network timer functions
//----------------------------------------------------------------------------

const net_proto_timer_func g_net_proto_timer_functions[] =
{
  // iface1_link_timer_func,   //-- Ethernet Link status checking - period 1 sec

   arp_timer_func,           //-- ARP timer

   ip_slowtimo,              //-- IP4 reassembly timeout timer

#ifdef TN_TCP
   tcp_timer_func,           //-- TCP timer tick 100 ms - for TCP timers
#endif

#ifdef TN_DHCP
   dhcp_timer_func,          //-- DHCP timer tick 100 ms - for DHCP timers
#endif

   NULL
};

//----------------------------------------------------------------------------
//   Interface N1  - LPC23XX MAC
//----------------------------------------------------------------------------

    //--- Interface N1 (iface1)

TN_NETIF g_iface1;

   //--- Interface N1 data structure

LPC23XXNETINFO g_iface1_lpc23xx_info;

    //--- Interface ARP

TN_ARPENTRY g_iface1_arp_arr[IFACE1_ARP_QUEUE_SIZE];
TN_SEM      g_iface1_arp_sem;

    //--- Interface Rx Tack stack

unsigned int g_iface1_rx_task_stack[IFACE1_TASK_RX_STK_SIZE];

    //--- Interface Rx & Tx Queues memory

void * g_iface1_queueRxMem[IFACE1_RX_QUEUE_SIZE];
void * g_iface1_queueTxMem[IFACE1_TX_QUEUE_SIZE];


#ifdef TN_DHCP

   //--- Interface DHCP data

unsigned int g_iface1_dhcp_task_stack[DHCP_TASK_STACK_SIZE];
DHCPPAR      g_iface1_dhcp_func_par;
struct dhcp  g_iface1_dhcp_dh;

#endif
//--- Extern functions prototypes

int drv_lpc23xx_net_wr(TN_NET * tnet, TN_NETIF * ni, TN_MBUF * mb);
int drv_lpc23xx_net_ioctl(TN_NET * tnet, TN_NETIF * ni,
                                         int req_type, void * par);
int init_mac(TN_NETINFO * tneti);

//--- Local functions prototypes

static void net_task_timer_func(void * par);
static int net_netif1_out_func(TN_NET * tnet, TN_NETIF * ni, TN_MBUF * mb);
static void net_iface1_rx_task_func(void * par);

//----------------------------------------------------------------------------
void net_iface1_set_addresses(TN_NETIF * ni)
{
   unsigned int tmp;

   //-- MAC addr

   memcpy(&ni->hw_addr[0], g_iface1_addr[0], MAC_SIZE);

   //-- Device IP4 addr

   ip4_str_to_num((char*)g_iface1_addr[1], &tmp);
   ni->ip_addr.s__addr = htonl(tmp);

   //-- Mask

   ip4_str_to_num((char*)g_iface1_addr[2], &tmp);
   ni->netmask.s__addr = htonl(tmp);

   //-- Gateway

   ip4_str_to_num((char*)g_iface1_addr[3], &tmp);
   ni->ip_gateway.s__addr = htonl(tmp);

   //-- LAN Broadcast

   ip4_str_to_num((char*)g_iface1_addr[4], &tmp);
   ni->if_broadaddr.s__addr = htonl(tmp); //-- it is  IFF_BROADCAST interface
}

//----------------------------------------------------------------------------
int net_iface1_init(TN_NETINFO * tneti)
{
   TN_NETIF * ni;
   int rc;

   ni = tneti->ni[0];

   s_memset(ni, 0, sizeof(struct tn_netif));

   net_iface1_set_addresses(ni);

   ni->if_flags   |= IFF_BROADCAST;
   ni->if_mtu      = 1500;
   ni->output_func = net_netif1_out_func;

   s_memset(&g_iface1_lpc23xx_info, 0, sizeof(LPC23XXNETINFO));
   ni->drv_info  = (void *)&g_iface1_lpc23xx_info;

   ni->drv_wr    = drv_lpc23xx_net_wr;
   ni->drv_ioctl = drv_lpc23xx_net_ioctl;

  //-- Interface rx queue

   ni->queueIfaceRxMem = (unsigned int*)&g_iface1_queueRxMem[0];

   tn_queue_create(&ni->queueIfaceRx,       //-- Ptr to already existing TN_DQUE
                   (void**)&ni->queueIfaceRxMem[0], //-- Ptr to already existing array of void * to store data queue entries.Can be NULL
                   IFACE1_RX_QUEUE_SIZE);     //-- Capacity of data queue(num entries).
  //-- Interface tx queue

   ni->queueIfaceTxMem = (unsigned int*)&g_iface1_queueTxMem[0];

   tn_queue_create(&ni->queueIfaceTx,
                   (void**)&ni->queueIfaceTxMem[0], //-- Ptr to already existing array of void * to store data queue entries.Can be NULL
                   IFACE1_TX_QUEUE_SIZE);     //-- Capacity of data queue(num entries).

  //-- rx task

   ni->task_rx_stack = &g_iface1_rx_task_stack[0];            //-- task stack first addr in memory

   tn_task_create(&ni->task_rx,                      //-- task TCB
                 net_iface1_rx_task_func,            //-- task function
                 IFACE1_TASK_RX_PRIORITY,            //-- task priority
                 &(ni->task_rx_stack                 //-- task stack first addr in memory
                     [IFACE1_TASK_RX_STK_SIZE-1]),
                 IFACE1_TASK_RX_STK_SIZE,            //-- task stack size (in int,not bytes)
                 (void*)tneti,                //-- task function parameter
                 TN_TASK_START_ON_CREATION);            //-- Creation option

  //-- ARP

   arp_init(ni, &g_iface1_arp_arr[0], IFACE1_ARP_QUEUE_SIZE, &g_iface1_arp_sem);

 //-- MAC/PHY

   rc = init_mac(tneti);
   if(rc != 0)
      return rc;

   ni->if_flags |= IFF_UP; //-- Last action

//--- Gratuitous ARP (2 times)

 //  arp_request(tneti->tnet,
 //              ni,
 //             ni->ip_addr.s__addr); //unsigned long tip);  //--- Target IP addr (grt hw to it)

 //  arp_request(tneti->tnet,
 //              ni,
 //             ni->ip_addr.s__addr); //unsigned long tip);  //--- Target IP addr (grt hw to it)

#ifdef TN_DHCP

   ni->dhcp_task_rx_stack = &g_iface1_dhcp_task_stack[0];

   dhcp_init(tneti->tnet, 
             ni,  
             &g_iface1_dhcp_func_par, 
             &g_iface1_dhcp_dh);
#endif
   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
int net_init_network(TN_NETINFO * tneti)
{
   int rc;
   volatile unsigned int tmp;
   TN_NET * tnet;

   if(tneti == NULL)
      return -1; //-- ????

   tnet = tneti->tnet;

 //-- TN Net Semaphore(s)

   tnet->spl_holder_task = NULL;

   tnet->ip_sem.id_sem = 0;
   tn_sem_create(&tnet->ip_sem,
                              1,   //-- Start value
                              1);  //-- Max value

   tnet->spl_sem.id_sem = 0;
   tn_sem_create(&tnet->spl_sem,
                              1,   //-- Start value
                              1);  //-- Max value

 //---- Hi Size Buffers Memory

 //--- LPC23XX - Rx buffers(1536 bytes each) are placed in the Ethernet memory

   tnet->mem_hibuf_mpool     = (unsigned int *) MAC_MEM_RX_BUF_BASE;
   tnet->mem_drv_lobuf_mpool = (unsigned int *) MAC_MEM_DRV_SMALL_BUF_BASE;
   tnet->mem_drv_m1buf_mpool = (unsigned int *) MAC_MEM_DRV_MID1_BUF_BASE;

   //--- Checking max Eth RAM addr
   tmp = (unsigned int)MAC_MEM_MAX_USE_ADDR;

   if((unsigned int)MAC_MEM_MAX_USE_ADDR > LPC2368_MAX_ETH_ADDR)
   {
      tn_task_sleep(TN_WAIT_INFINITE);
   }

 //-------------------------------------------------------------

   rc = tn_net_init_mem_pools(tnet);
   if(rc != TERR_NO_ERR)
      return rc;

  //-- Random generator init
   
   tn_srand(&tnet->rnd_gen, (unsigned char*)g_iface1_addr[0]);

  //---- IP

   ip_init(tnet);    //-- Add to proto list - inside

  //---- UDP

   udp_init(tnet);   //-- Add to proto list - inside

  //---- TCP

   tcp_init(tnet);   //-- Add to proto list - inside

  //-- TN Net Task(s) - now all timer_func()for all attached protocols are ready

      //-- Net timer task

   tnet->task_net_timer.id_task = 0;
   tn_task_create(&tnet->task_net_timer,        //-- task TCB
                 net_task_timer_func,                  //-- task function
                 TNNET_NETTIMERTASK_PRIORITY,          //-- task priority
                 &(tnet->task_net_timer_stack   //-- task stack first addr in memory
                     [TNNET_NETTIMERTASK_STK_SIZE-1]),
                 TNNET_NETTIMERTASK_STK_SIZE,          //-- task stack size (in int,not bytes)
                 (void*)tneti,                         //-- task function parameter
                 TN_TASK_START_ON_CREATION);           //-- Creation option

  tnet->tcb.inp_next = tnet->tcb.inp_prev = &tnet->tcb; // head of queue of active tcpcb's

  //--------------------------

   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
// spl in TN NET is a mutex-like object.
//----------------------------------------------------------------------------
int splnet(TN_NET * tnet)
{
   TN_INTSAVE_DATA
   TN_SEM * sem;

   sem = &tnet->spl_sem;

   tn_disable_interrupt();

  //-- Check re-entering

   if(tn_curr_run_task == tnet->spl_holder_task)
   {
      tn_enable_interrupt();
      return 1;
   }

  //-----------------

   if(tnet->spl_holder_task == NULL)
   {
      tnet->spl_holder_task = tn_curr_run_task;
   }
   else
   {
      task_curr_to_wait_action(&(sem->wait_queue),
                                TSK_WAIT_REASON_SEM, TN_WAIT_INFINITE);
      tn_enable_interrupt();
      tn_switch_context();

      return 0;
   }

   tn_enable_interrupt();

   return 0;
}

//----------------------------------------------------------------------------
void splx(TN_NET * tnet, int sm)
{
   TN_INTSAVE_DATA

   CDLL_QUEUE * que;
   TN_TCB * task;
   TN_SEM * sem;

   if(sm != 0)
      return;

   sem = &tnet->spl_sem;

   tn_disable_interrupt();

   if(!(is_queue_empty(&(sem->wait_queue))))
   {
      //--- delete from sem wait queue

      que = queue_remove_head(&(sem->wait_queue));
      task = get_task_by_tsk_queue(que);

      tnet->spl_holder_task = task;

      if(task_wait_complete(task,FALSE))
      {
         tn_enable_interrupt();
         tn_switch_context();
         return;
      }
   }
   else
   {
      tnet->spl_holder_task = NULL;
   }
   tn_enable_interrupt();
}
//----------------------------------------------------------------------------
int tn_net_random_id(void)
{
   static unsigned short cnt = 121;
   cnt++;
   return cnt;
}

//----------------------------------------------------------------------------
static void net_task_timer_func(void * par)
{
   static TN_NETINFO * hwni = NULL;
   static unsigned int cnt = 0;
   net_proto_timer_func  timer_func;
   int i;

   hwni = (TN_NETINFO *)par;

   for(;;)
   {
      cnt++;

      for(i=0; ;i++)
      {
         timer_func = g_net_proto_timer_functions[i];

         if(timer_func)
            timer_func(hwni, cnt);
         else            //--  NULL - Function's array last element
            break;
      }
      tn_task_sleep(100);  //-- 100 mS delay
   }
}

//----------------------------------------------------------------------------
static void net_iface1_rx_task_func(void * par)
{
   TN_NETINFO * tneti;
   TN_NET * tnet;
   TN_NETIF * ni;
   TN_MBUF * mb;
   struct ether_header * eh = NULL;
   int rc;
   int in_packet_cnt = 0;

   tneti = (TN_NETINFO *)par;
   if(tneti == NULL)
      tn_task_sleep(TN_WAIT_INFINITE);

   tnet = tneti->tnet;
   ni   = tneti->ni[0];  //-- iface N1

   if(tnet == NULL || ni == NULL)
      tn_task_sleep(TN_WAIT_INFINITE);

   for(;;)
   {
      tnet = tneti->tnet;
      ni   = tneti->ni[0];  //-- iface N1

      rc = tn_queue_receive(&ni->queueIfaceRx, (void**)&mb, TN_WAIT_INFINITE);
      if(rc == TERR_NO_ERR)
      {
         if((ni->if_flags & IFF_UP) == 0)  //-- if iface not up yet
         {
            if(m_freem(tnet, mb) == INV_MEM_VAL)
               tn_net_panic(INV_MEM_VAL_40);
            continue;
         }

         eh = (struct ether_header *)(mb->m_data);
         switch(ntohs(eh->ether_type))
         {
            case ETHERTYPE_IP:

               if(memcmp((unsigned char *)etherbroadcastaddr,
                                           (unsigned char *)eh->ether_dhost, MAC_SIZE) == 0)
                  mb->m_flags |= M_BCAST;
               else if(eh->ether_dhost[0] & 1)
                  mb->m_flags |= M_MCAST;

               mb->m_data += sizeof(struct ether_header); //-- Remove Eth header
               mb->m_len  -= sizeof(struct ether_header) + sizeof(int); //-- CRC32 size
               mb->m_tlen  = mb->m_len;

              //-- mb->m_data points to the IP header
               in_packet_cnt++;

//dbg_send("ip\r\n");

               ipv4_input(tnet, ni, mb);

               break;

            case ETHERTYPE_ARP:

              //-- mb->m_data points to the Ethernet header

               in_packet_cnt++;
               arp_input(tnet, ni, mb);

               break;

            default:  //-- unused/unknown

               if(m_freem(tnet, mb) == INV_MEM_VAL)
                  tn_net_panic(INV_MEM_VAL_41);

               break;
         }
      }
   }
}

//----------------------------------------------------------------------------
static int net_netif1_out_func(TN_NET * tnet, TN_NETIF * ni, TN_MBUF * mb)
{
   TN_IPHDR * iphdr;
   int rc;
   TN_MBUF * mb0;
   struct in__addr fr_host_ip;
   unsigned char dst_mac[MAC_SIZE];

   iphdr = (TN_IPHDR *)mb->m_data;

 //--  get IP for ARP, put it to 'the fr_host_ip'

   if(((iphdr->ip_dst.s__addr & ni->netmask.s__addr) ==
          (ni->ip_addr.s__addr & ni->netmask.s__addr)) ||
      ((iphdr->ip_dst.s__addr & ni->netmask.s__addr) ==
           ni->netmask.s__addr))
   {
      fr_host_ip.s__addr = iphdr->ip_dst.s__addr;
   }
   else
   {
      if(ni->ip_gateway.s__addr == 0)
      {
         if(m_freem(tnet, mb) == INV_MEM_VAL)
            tn_net_panic(INV_MEM_VAL_42);

         return TERR_WSTATE;
      }
      else
         fr_host_ip.s__addr = ni->ip_gateway.s__addr;
   }
   rc = arp_resolve(tnet, ni, mb, fr_host_ip, &dst_mac[0]); //-- [OUT]
   if(rc) //-- OK
   {
      mb0 = add_eth_header(tnet,
                            mb,  //-- mbuf to send
                            &dst_mac[0],    //-- Ethernet dst - Net order
                            ni->hw_addr,    //-- Ethernet src - Net order
                            ETHERTYPE_IP);  //-- Ethernet type- Host order
            //-- Data to send - to Ethernet driver
      if(mb0 != NULL)
         ni->drv_wr(tnet, ni, mb0);
      else
      {
       // Statistics
      }
   }
   else
   {
      // Statistics
   }
   return 0;
}

//----------------------------------------------------------------------------
void dbg_pin_on(int pin_mask)
{
   rFIO1SET |= pin_mask;
}
//----------------------------------------------------------------------------
void dbg_pin_off(int pin_mask)
{
   rFIO1CLR |= pin_mask;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


