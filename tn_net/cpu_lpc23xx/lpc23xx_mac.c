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

#include <tnkernel/tn.h>

#include "../tn_net_cfg.h"
#include "../tn_net_types.h"
#include "../tn_net_pcb.h"
#include "../tn_net_mem.h"
#include "../tn_ip.h"
#include "../tn_net.h"
#include "../errno.h"
#include "../tn_mbuf.h"
#include "../tn_netif.h"
#include "../tn_eth.h"

#include "lpc23xx_net_emac.h"


#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE  1
#endif


//--- The test board has a KS8721 PHY chip & LPC2368 CPU


//-- External functions prototypes

int install_irq(int IntNumber, unsigned int HandlerAddr, int Priority);
void int_func_ethernet(void);

//----------------------------------------------------------------------------
static void mac_rx_descr_init(TN_NET * tnet, LPC23XXNETINFO * hwni)
{
   int i;
   int rc;
   volatile void * mem_ptr;

   hwni->num_rx_descr = MAC_NUM_RX_DESCR;

   //--- Assign descriptors memory (Ethernet memory block)

   hwni->rx_descr  = (TN_MAC_RX_DESCR *) MAC_MEM_RX_DESCR_BASE;
   hwni->rx_status = (TN_MAC_RX_STATUS *)MAC_MEM_RX_STATUS_BASE;

   //--- Fill descriptors - for LPC23XX Rx Descriptors, a mem block size
   //--  is 1536 byte and frame will be received to the single descriptor buf
   //-- Rx buffers are placed at the Ethernet memory block(16 KBytes)


   for(i = 0; i < MAC_NUM_RX_DESCR; i++)
   {
      //rc = tn_fmem_dbg_get_polling(&tnet->hibuf_mpool, (void **) &mem_ptr);
      rc = tn_fmem_get_polling(&tnet->hibuf_mpool, (void **) &mem_ptr);
      if(rc != TERR_NO_ERR) //-- Fatal err
      {
         for(;;);
      }

      hwni->rx_descr[i].Packet = (void *)mem_ptr;
      hwni->rx_descr[i].Control = ((MAC_RX_BUF_SIZE-1) & 0x7FF) |  //-- Buf length
                                                       (1u<<31);   //-- Enable RxDone Interrupt
      hwni->rx_status[i].StatusInfo    = 0;
      hwni->rx_status[i].StatusHashCRC = 0;
   }

  //-- Set EMAC Receive Descriptor Registers.

   rMAC_RXDESCRIPTOR     =  MAC_MEM_RX_DESCR_BASE;
   rMAC_RXSTATUS         =  MAC_MEM_RX_STATUS_BASE;
   rMAC_RXDESCRIPTORNUM  =  MAC_NUM_RX_DESCR - 1;

  //-- Rx Descriptors Point to 0

   rMAC_RXCONSUMEINDEX  = 0;
}

//----------------------------------------------------------------------------
static void mac_tx_descr_init(LPC23XXNETINFO * hwni)
{
   int i;

   hwni->num_tx_descr = MAC_NUM_TX_DESCR;

   //--- Assign descriptors memory (Ethernet memory block)

   hwni->tx_descr  = (TN_MAC_TX_DESCR *) MAC_MEM_TX_DESCR_BASE;
   hwni->tx_status = (TN_MAC_TX_STATUS *)MAC_MEM_TX_STATUS_BASE;

   //--- Fill descriptors - for LPC23XX Tx Descriptors, a mem block size
   //--  can be arbitrary size. It should be filled immediately before Tx
   //--  and here (at the init point) are undefined

   for(i = 0; i < MAC_NUM_TX_DESCR; i++)
   {
      hwni->tx_descr[i].Packet  = 0;  //-- See above
      hwni->tx_descr[i].Control = 0;

      hwni->tx_status[i].StatusInfo    = 0;
   }

  //-- Set EMAC Transmit Descriptor Registers.

   rMAC_TXDESCRIPTOR    = MAC_MEM_TX_DESCR_BASE;
   rMAC_TXSTATUS        = MAC_MEM_TX_STATUS_BASE;
   rMAC_TXDESCRIPTORNUM = MAC_NUM_TX_DESCR - 1;

  //-- Tx Descriptors Point to 0

   rMAC_TXPRODUCEINDEX  = 0;
}

//----------------------------------------------------------------------------
int init_mac(TN_NETINFO * tneti)
{
   int rc;
   LPC23XXNETINFO * hwni;
   TN_NETIF * ni;

   ni = tneti->ni[0];
   if(ni == NULL)
      return -1;
   hwni = (LPC23XXNETINFO *)ni->drv_info;
   if(hwni == NULL)
      return -1;

   //-- Enable P1 Ethernet Pins. ;
   //   select the PHY functions for GPIO Port1[17:0] ;

   //   PINSEL2 = 0x50151105;

   //-- selects P1[0,1,4,8,9,10,14,15]
   rPINSEL2 = 0x50150105;
   rPINSEL3 = (rPINSEL3 &~0x0000000f) | 0x5;

   //-- Power Up the EMAC controller.

   rPCONP |= 0x40000000;

   //-- Reset all EMAC internal modules.

   rMAC_MAC1 = MAC1_RES_TX |
               MAC1_RES_MCS_TX |
               MAC1_RES_RX |
               MAC1_RES_MCS_RX |
               MAC1_SIM_RES |
               MAC1_SOFT_RES;

   rMAC_COMMAND = CR_REG_RES | CR_TX_RES | CR_RX_RES;

   for(rc = 0; rc <400; rc++);  //-- A short delay after reset.

   //-- Initialize MAC control registers.

   rMAC_MAC1 = MAC1_PASS_ALL;
   rMAC_MAC2 = MAC2_CRC_EN | MAC2_PAD_EN;
   rMAC_MAXF = 1536;       //-- 1536 = 0x600; //ETH_MAX_FLEN;
   rMAC_CLRT = CLRT_DEF;
   rMAC_IPGR = IPGR_DEF;

   rMAC_MCFG = MCFG_HCLK_DIV_28;

   //-- Enable Reduced MII interface.

   rMAC_COMMAND = CR_RMII | CR_PASS_RUNT_FRM;

   //-- Reset Reduced MII Logic.

   rMAC_SUPP = SUPP_RES_RMII;
   for(rc = 0; rc < 200; rc++);
   rMAC_SUPP = 0;

   rc = ni->phy->init(ni);
   if(rc != 0) //-- Err
      return rc;

  //-- Set the Ethernet MAC Address registers

   rMAC_SA0 = (ni->hw_addr[0] << 8) | ni->hw_addr[1];
   rMAC_SA1 = (ni->hw_addr[2] << 8) | ni->hw_addr[3];
   rMAC_SA2 = (ni->hw_addr[4] << 8) | ni->hw_addr[5];

  //-- Initialize Tx and Rx DMA Descriptors

   mac_rx_descr_init(tneti->tnet, hwni);
   mac_tx_descr_init(hwni);

  //-- Set RX filter

   rMAC_RXFILTERCTRL =     1 | //-- Bit 0 AcceptUnicastEn
                       (1<<1)| //-- Bit 1 AcceptBroadcastEn
                       (1<<2)| //-- Bit 2 AcceptMulticastEn       dif
                       (1<<5); //-- Bit 5 AcceptPerfectEn

  //-- Enable Rx EMAC interrupts.


   rMAC_INTENABLE =     1 | //-- Bit 0 RxOverrunIntEn
                    (1<<1)| //-- Bit 1 RxErrorIntEn
                    (1<<2)| //-- Bit 2 RxFinishedIntEn
                    (1<<3); //| //-- Bit 3 RxDoneIntEn

                   // (1<<4)| //-- Bit 4 TxUnderrunIntEn
                   // (1<<5)| //-- Bit 5 TxErrorIntEn
                   // (1<<6)| //-- Bit 6 TxFinishedIntEn
                   //(1<<7);  //-- Bit 7 TxDoneIntEn

  //-- Reset all interrupts

   rMAC_INTCLEAR = 0xFFFF;

  //-- Setup VIC Ethernet interrupt

   install_irq(21,                               //-- VIC Int Number for Ethernet
               (unsigned int)int_func_ethernet,  //-- Handler
               0);                               //-- Highest Priority

#ifdef LOOPBACK
   ni->mdio_rd(ni->phy->addr, 0, (unsigned short *)&rc);
   rc |=   (1<<14) | //-- Loopback
           (1<<13) | //-- Speed - 100 Mb/s
           (1<<8);   //-- 1 = full-duplex

   ni->mdio_wr(ni->phy->addr, 0, (unsigned short)rc);
#endif

   //-- Enable receive and transmit mode of MAC Ethernet core

   rMAC_COMMAND |= CR_RX_EN |
                   CR_TX_EN;

   rMAC_MAC1 |= MAC1_REC_EN;

#ifdef LOOPBACK
   rMAC_MAC1 |=  (1<<4); //-- Loopback
#endif

   return 0; //-- O.K
}

//----------------------------------------------------------------------------
void iface1_link_timer_func(TN_NETINFO * tneti, int cnt)
{
   TN_INTSAVE_DATA
   LPC23XXNETINFO * nhwi;
   TN_NETIF * ni;
   
   int rc;
   unsigned short op_mode;

   if(cnt % 10 == 0)  //-- each 1 sec
   {
      ni = tneti->ni[0];
      if(ni)
      {
         nhwi = (LPC23XXNETINFO *)ni->drv_info;

         if(nhwi)
         {
            rc = ni->phy->is_link_up(ni, &op_mode);
            if (nhwi->link_mode != op_mode)
            {
              ni->drv_ioctl(ni, IOCTL_MAC_SET_MODE, (void *)op_mode);
              nhwi->link_mode = op_mode;
            }
            if (nhwi->link_up != rc)
            {
              tn_disable_interrupt();
              nhwi->link_up = rc;
              tn_enable_interrupt();
              if (nhwi->link_up)
                arp_request(tneti->tnet, //-- gratious ARP on link up
                            ni,
                            ni->ip_addr.s__addr);
            }
         }
      }
   }
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------







