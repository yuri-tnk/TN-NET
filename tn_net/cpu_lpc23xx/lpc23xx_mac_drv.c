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
#include "../tn_net_utils.h"
#include "../tn_eth.h"
#include "../tn_net_mem_func.h"

#include "lpc23xx_net_emac.h"
#include "lpc23xx_mac_drv.h"
#include "../phy/phy.h"

#include "../dbg_func.h"

//--- Local macros

#define MAC_TX_LOCK      if(from_interrupt) { tn_idisable_interrupt(); } \
                         else               { tn_disable_interrupt();  }

#define MAC_TX_UNLOCK    if(from_interrupt) { tn_ienable_interrupt(); } \
                         else               { tn_enable_interrupt();  }


//--- Local functions prototypes

int drv_lpc23xx_net_tx_data(TN_NET * tnet,
                            TN_NETIF * ni,
                            TN_MBUF * mb_to_tx,
                            int from_interrupt);

int drv_lpc23xx_mdio_read(int phy_addr, int phy_reg, unsigned short * value)
{
  int i;
  volatile int delay;
  
  //-- bits 12..8 -PHY addr, bits 4..0 - PHY reg addr
  
  rMAC_MADR = (phy_addr << 8) | (phy_reg & 0x1F);
  rMAC_MCMD = 0x0001;                    //-- read command
  
  for(i= 0; i < PHY_RD_MAX_ITERATIONS; i++)
  {
    if((rMAC_MIND & (MIND_BUSY | MIND_NOT_VAL))== 0)
      break;
    for(delay = 0; delay < 1000; delay++);
  }
  rMAC_MCMD = 0;
  
  if(i >= PHY_RD_MAX_ITERATIONS) //-- timeout
    return FALSE;
  
  *value =  rMAC_MRDD;
  return TRUE;
}

int drv_lpc23xx_mdio_write(int phy_addr, int phy_reg, unsigned short value)
{
  int i;
  
  //-- bits 12..8 -PHY addr, bits 4..0 - PHY reg addr
  
  rMAC_MADR = (phy_addr<<8) | (phy_reg & 0x1F);
  
  rMAC_MWTD = value;
  
  for(i= 0; i < MII_WR_TOUT; i++)
  {
    if((rMAC_MIND & 1) == 0)              //-- Bit 0 -BUSY
      break;
  }
  
  if(i >= MII_WR_TOUT) //-- timeout
    return FALSE;
  
  return TRUE;
}

//----------------------------------------------------------------------------
int  drv_lpc23xx_net_ioctl(TN_NETIF * ni, int req_type, void * par)
{
   TN_INTSAVE_DATA
   LPC23XXNETINFO * nhwi;
   int rc = TERR_ILUSE;

   nhwi = (LPC23XXNETINFO *)ni->drv_info;
   if(nhwi)
   {
      if(req_type == IOCTL_ETH_IS_TX_DISABLED)
      {
         tn_disable_interrupt();
         rc = nhwi->tx_int_en;
         tn_enable_interrupt();

         rc = rc ? FALSE : TRUE;
      }
      else if(req_type == IOCTL_ETH_IS_LINK_UP)
      {
         tn_disable_interrupt();
         rc = nhwi->link_up;
         tn_enable_interrupt();

         rc = rc ? TRUE : FALSE;
      }
      else if(req_type == IOCTL_MAC_SET_MODE)
      {
        rc = TRUE;
        if ((int)par == PHY_LINK_100_HD)
        {
          rMAC_MAC2    &=    ~0x1;
          rMAC_COMMAND &=  ~(1<<10);
          rMAC_IPGT     =   0x0012;   //-- IPGT_HALF_DUP
          
          rMAC_SUPP    |=   (1<<8);   //-- RMII - for 100Mb
        }
        else if ((int)par == PHY_LINK_10_FD)
        {
          rMAC_MAC2    |=     0x1;    //-- MAC2_FULL_DUP
          rMAC_COMMAND |=  (1<<10);   //-- CR_FULL_DUP
          rMAC_IPGT     =  0x0015;    //-- IPGT_FULL_DUP
          
          rMAC_SUPP    &=  ~(1<<8);
        }
        else if ((int)par == PHY_LINK_100_FD)
        {
          rMAC_MAC2    |=    0x1;   //-- MAC2_FULL_DUP
          rMAC_COMMAND |=  (1<<10); //-- CR_FULL_DUP
          rMAC_IPGT     =  0x0015;  //-- IPGT_FULL_DUP
          
          rMAC_SUPP    |=  (1<<8);
        }
        else
        {
          rMAC_MAC2    &=  ~0x1;
          rMAC_COMMAND &=  ~(1<<10);
          rMAC_IPGT     =  0x0012;   //-- IPGT_HALF_DUP
          
          rMAC_SUPP    &=  ~(1<<8);
        }
      }
   }
   return rc;
}

//----------------------------------------------------------------------------
int drv_lpc23xx_net_wr(TN_NET * tnet, TN_NETIF * ni, TN_MBUF * mb)
{
   TN_MBUF * mb0;
   int rc = 0; //-- OK

   if(ni->drv_ioctl(ni, IOCTL_ETH_IS_LINK_UP, NULL))
   {
      rc = tn_queue_send_polling(&ni->queueIfaceTx, (void*)mb);
      if(rc == TERR_NO_ERR)
      {
         //-- if tx is disabled

         if(ni->drv_ioctl(ni, IOCTL_ETH_IS_TX_DISABLED, NULL))
         {
            //-- To Fill descr after stop tx

            rc = tn_queue_receive_polling(&ni->queueIfaceTx, (void**)&mb0);
            if(rc == TERR_NO_ERR)
            {
               return drv_lpc23xx_net_tx_data(tnet, ni,
                                              mb0,
                                              FALSE); //-- not from_interrupt
            }
            else
               return ENOBUFS;
         }
      }
      else
      {
         if(m_freem(tnet, mb) == INV_MEM_VAL)
            tn_net_panic(INV_MEM_VAL_23);

         rc = ENOBUFS;
      }
   }
   else
   {
      if(m_freem(tnet, mb) == INV_MEM_VAL)
         tn_net_panic(INV_MEM_VAL_24);

      rc = ENETDOWN;
   }
   return rc;
}

//----------------------------------------------------------------------------
//  Is invoked inside interrupt
//----------------------------------------------------------------------------
int drv_lpc23xx_net_rx_data(TN_NET * tnet,
                            TN_NETIF * ni,
                            int ind,
                            TN_MBUF ** mb_out) //-- [OUT]
{
   unsigned int rx_status;
   volatile unsigned int dbg_info;
   int len;
   TN_MBUF * mb;
   unsigned char * new_buf;
   LPC23XXNETINFO * nhwi;
   struct ether_header * eh;

   nhwi = (LPC23XXNETINFO *)ni->drv_info;
   if(nhwi == NULL)
      return TERR_ILUSE;

   rx_status = nhwi->rx_status[ind].StatusInfo;
   dbg_info = rMAC_RSV;

   if(rx_status & ((1<<23) |//-- Bit 23 -- CRCError
                   (1<<24) |//-- Bit 24 SymbolError
                 //(1<<25) |//-- Bit 25 LengthError
                 //(1<<26) |//-- Bit 26 RangeError[1]
                   (1<<27) |//-- Bit 27 AlignmentError
                   (1<<28)))//-- Bit 28 Overrun
   {
              //-- Refresh control info for new rx
      nhwi->rx_descr[ind].Control = ((MAC_RX_BUF_SIZE-1) & 0x7FF) |  //-- Buf length
                                     (1u<<31);   //-- Enable RxDone Interrupt

      return ERR_RX_ERR;
   }

   len = (rx_status & 0x7FF) + 1;

   //-- If a rxed frame is not big one( <= MID1 BUF size), alloc buf and COPY it

   mb = NULL;

   if(len <= 128)
   {
      eh = (struct ether_header *)nhwi->rx_descr[ind].Packet;
      switch(ntohs(eh->ether_type))
      {
         case ETHERTYPE_ARP:
            mb = mb_iget(tnet, MB_MID1, TRUE); //-- Use Tx pool
            break;
         case ETHERTYPE_IP:
         default:
            mb = mb_iget(tnet, MB_MID1, FALSE);
            if (mb == NULL)
            {
                //drops.rx++
            }
            break;
      }

      if(mb != NULL)
      {
         bcopy((void*)nhwi->rx_descr[ind].Packet, mb->m_data, len);

         mb->m_len  = len;
         mb->m_tlen = len;
         mb->m_flags |= M_PKTHDR;
         if(rx_status & (1<<21)) //-- Bit 21 - Multicast
            mb->m_flags |= M_MCAST;
         if(rx_status & (1<<22)) //-- Bit 22 - Broadcast
            mb->m_flags |= M_BCAST;
      }
      //-- Refresh control info for new rx
      nhwi->rx_descr[ind].Control = ((MAC_RX_BUF_SIZE - 1) & 0x7FF) | //-- Buf length
                                    (1u << 31);                       //-- Enable RxDone Interrupt
      if (mb == NULL)
      {
         //rx_drops++;
         return TERR_OUT_OF_MEM;
      }
   }

   else
   {
      new_buf = (unsigned char *)tn_net_ialloc_hi(tnet);
      if(new_buf == NULL)
      {
             //-- Refresh control info for new rx
         nhwi->rx_descr[ind].Control = ((MAC_RX_BUF_SIZE-1) & 0x7FF) |  //-- Buf length
                                                            (1u<<31);   //-- Enable RxDone Interrupt
         return TERR_OUT_OF_MEM;
      }

      mb = (TN_MBUF *)m_ialloc_hdr(tnet);
      if(mb == NULL)
      {
         tn_net_ifree_hi(tnet, new_buf);
              //-- Refresh control info for new rx
         nhwi->rx_descr[ind].Control = ((MAC_RX_BUF_SIZE-1) & 0x7FF) |  //-- Buf length
                                                            (1u<<31);   //-- Enable RxDone Interrupt
         return TERR_OUT_OF_MEM;
      }

      mb->m_dbuf = (unsigned char *)nhwi->rx_descr[ind].Packet;

     //-- Refresh descr  for new rx

      nhwi->rx_descr[ind].Packet  = (void*)new_buf;
      nhwi->rx_descr[ind].Control = ((MAC_RX_BUF_SIZE-1) & 0x7FF) |  //-- Buf length
                                                         (1u<<31);   //-- Enable RxDone Interrupt

     //-- Create mbuf from new1 && rxed buf

      mb->m_pri    = mb;          //-- self - case primary
      mb->m_dbtype = MB_HI;
      mb->m_data   = mb->m_dbuf;
      mb->m_refcnt = 1;
      mb->m_flags  = M_DBHDR;

      mb->m_len  = len;
      mb->m_tlen = len;
      mb->m_flags |= M_PKTHDR;
    //  if(rx_status & (1<<21))     //-- Bit 21 - Multicast
    //     mb->m_flags |= M_MCAST;
    //  if(rx_status & (1<<22))     //-- Bit 22 - Broadcast
    //     mb->m_flags |= M_BCAST;
   }

   *mb_out = mb;
   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
int drv_lpc23xx_net_tx_data(TN_NET * tnet,
                            TN_NETIF * ni,
                            TN_MBUF * mb_to_tx,
                            int from_interrupt)
{
   TN_INTSAVE_DATA
   TN_INTSAVE_DATA_INT

   TN_MBUF * mb;
   int head_last;
   LPC23XXNETINFO * nhwi;
   int tail_cnt;
   int head_cnt;

   nhwi = (LPC23XXNETINFO *)ni->drv_info;
   if(nhwi == NULL)
      return EINVAL;

   head_cnt  = rMAC_TXPRODUCEINDEX;
   tail_cnt  = rMAC_TXCONSUMEINDEX;
   head_last = -1;

   for(mb = mb_to_tx; mb != NULL; mb = mb->m_next)
   {
       //-- If tx FIFO  is full
      if((tail_cnt == 0 && head_cnt == nhwi->num_tx_descr - 1)
                                              || head_cnt == tail_cnt-1)
      {
           //-- Free mem
          if(from_interrupt)
          {
             if(m_ifreem(tnet, mb_to_tx) == INV_MEM_VAL)
                tn_net_panic_int(INV_MEM_VAL_I_1);
          }
          else
          {
             if(m_freem(tnet, mb_to_tx) == INV_MEM_VAL)
                tn_net_panic(INV_MEM_VAL_24);
          }

          //-- Reset Tx MAC

          rMAC_COMMAND |= (1<<4); //-- Bit 4 TxReset;
          rMAC_COMMAND |= (1<<1); //-- Bit 1 TxEnable;

          MAC_TX_LOCK

          nhwi->mbuf_to_tx = NULL;  //-- Nothing to save here
          nhwi->tx_int_en = FALSE;  //-- stop TX - yes

          MAC_TX_UNLOCK

          rMAC_INTENABLE |= ~((1<<4) | //-- Bit 4 - TxUnderrunIntEn
                              (1<<5) | //-- Bit 5 - TxErrorIntEn
                              (1<<6) | //-- Bit 6 - TxFinishedIntEn
                              (1<<7)); //-- Bit 7 - TxDoneIntEn

          return ENOBUFS; //TERR_OVERFLOW;
      }
      else                                  //-- OK ( not full)
      {
         // ToDo - add Statistics here

          //-- Fill descriptor data ---

         nhwi->tx_descr[head_cnt].Packet  = (void *)mb->m_data;
         nhwi->tx_descr[head_cnt].Control = ((mb->m_len &0x7FF) - 1);
         head_last = head_cnt;

         //---------------------------

         head_cnt++;
         if(head_cnt >= nhwi->num_tx_descr)
            head_cnt = 0;
      }
   }

   //--- If here - do Tx.
   //--  Now set last descr as Last and enable Int TxDone

   if(head_last >= 0)
   {
      nhwi->tx_descr[head_last].Control |= (1 << 30) |  //-- bit 30 - Last
                                          (1u << 31);   //-- bit 31 - Interrupt
      MAC_TX_LOCK

      nhwi->mbuf_to_tx = mb_to_tx; //-- To free mbuf after tx ending
      nhwi->tx_int_en  = TRUE;

      MAC_TX_UNLOCK

      //--- restart tx

      rMAC_INTENABLE |= (1<<4) | //-- Bit 4 - TxUnderrunIntEn
                        (1<<5) | //-- Bit 5 - TxErrorIntEn
                        (1<<6); // | //-- Bit 6 - TxFinishedIntEn
                        //(1<<7);  //-- Bit 7 - TxDoneIntEn

      rMAC_TXPRODUCEINDEX = head_cnt;
   }

   return 0; //-- OK
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



