/*

Copyright © 2004, 2009 Yuri Tiomkin
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
#include "../bsd_socket.h"
#include "../tn_socket.h"
#include "../tn_net_utils.h"

#include "lpc23xx_net_emac.h"
#include "lpc23xx_mac_drv.h"



//----------------------------------------------------------------------------
void drv_lpc23xx_net_int(TN_NET * tnet, TN_NETIF * ni, unsigned int status)
{
   int head_cnt;
   int tail_cnt;
   int rc;
   int last_tx_ind;
   unsigned int descr_status;
   LPC23XXNETINFO * nhwi;
   TN_MBUF * mb;

   nhwi = (LPC23XXNETINFO *)ni->drv_info;

 //--------- Reception --------------------------------------------------------

   if(status & ((1<<3) | //-- Bit 3 - RxDoneInt
                (1<<2) | //-- Bit 2 - RxFinishedIntEn
                (1<<1))) //-- Bit 1 - RxErrorInt
   {
      //-- Clear int source

      rMAC_INTCLEAR = (1<<3) | //-- Bit 3 - RxDoneInt
                      (1<<2) | //-- Bit 2 - RxFinishedIntEn
                      (1<<1);  //-- Bit 1 - RxErrorInt

      //-- ToDo - Add Statistic here

      head_cnt  = rMAC_RXPRODUCEINDEX;
      tail_cnt  = rMAC_RXCONSUMEINDEX;

      while(tail_cnt != head_cnt)  //-- Processing all received frames
      {
         rc = drv_lpc23xx_net_rx_data(tnet,
                                  ni,
                                  tail_cnt,  // ind,
                                  &mb); //-- [OUT]
         if(rc == TERR_NO_ERR) //-- Rx frame without error
         {
            rc = tn_queue_isend_polling(&ni->queueIfaceRx, (void*)mb);
            if(rc != TERR_NO_ERR) //-- No free entries in rx mbuf(s) queue
            {
               if(m_ifreem(tnet, mb) == INV_MEM_VAL)   //-- drop it
                  tn_net_panic_int(INV_MEM_VAL_I_2);
            }
         }
         else if(rc == TERR_OUT_OF_MEM)
         {
            tail_cnt = head_cnt;
            break;  //-- No memory - no reason to continue rx results processing
         }

         tail_cnt++;
         if(tail_cnt >= nhwi->num_rx_descr)  //-- Descr index overflow
            tail_cnt = 0;
      }
   //-- If some rxed buffers were not processed(No memory for new buffers)
   //-- data in Receive Descriptor Fields(Packet & Control) still valid for
   //-- the new receptions

      rMAC_RXCONSUMEINDEX = tail_cnt; //-- New rx beginning
   }

   //-- err - rx overrun

   if(status & MAC_MASK_INT_RX_OVERRUN)
   {
      rMAC_INTCLEAR = MAC_MASK_INT_RX_OVERRUN; //-- Clear int source

     //-- ToDo - Add Statistic here

      rMAC_COMMAND |= (1<<5);    //-- Bit 5 - RxReset
      rMAC_COMMAND |=  1;        //-- Bit 0 - RxEnable
      rMAC_MAC1    |=  1;        //-- Bit 0 - RECEIVE ENABLE
   }

//--------- Transmission -----------------------------------------------------

   if(status & ((1<<6) |         //-- Bit 6 - TxFinishedInt
                (1<<5)))         //-- Bit 5 - TxErrorInt
   {
      rMAC_INTCLEAR = (1<<7) |   //-- Bit 7 - TxDoneInt - clear any case
                      (1<<6) |   //-- Bit 6 - TxFinishedInt
                      (1<<5);    //-- Bit 5 - TxErrorInt

      head_cnt  = rMAC_TXPRODUCEINDEX;
      tail_cnt  = rMAC_TXCONSUMEINDEX;

      //--- We transmit only a single frame per session

      if(head_cnt != tail_cnt)   //-- Err - not all descr was transmitted
      {
         //-- Tx Err - reset MAC Tx

         rMAC_COMMAND |= (1<<4); //-- Bit 4 TxReset;
         rMAC_COMMAND |= (1<<1); //-- Bit 1 TxEnable;
      }

      //--- Calc last txed descriptor (index)

      if(tail_cnt == 0)
         last_tx_ind = nhwi->num_tx_descr - 1;
      else
         last_tx_ind = tail_cnt - 1;

      descr_status = nhwi->tx_status[last_tx_ind].StatusInfo;

      //-- Processing errors at the last txed fragment

         //-- errors ToDo - Add Statistic here

      if(descr_status & (1<<28))  //-- Bit - 28 LateCollision
      {
           //-- The same tx mb  - tx again

         if(nhwi->mbuf_to_tx != NULL)
         {
            mb = nhwi->mbuf_to_tx;
            rc = TERR_NO_ERR;
         }
         else
            rc = TERR_ILUSE;  //-- To stop tx
      }
      else
      {
         //-- Free 'already done tx' memory

         if(nhwi->mbuf_to_tx != NULL)
         {
            if(m_ifreem(tnet, nhwi->mbuf_to_tx) == INV_MEM_VAL)
               tn_net_panic_int(INV_MEM_VAL_I_3);

            nhwi->mbuf_to_tx = NULL;
         }
         //-- Get data for new tx

         rc = tn_queue_ireceive(&ni->queueIfaceTx, (void **)&mb);
        // rc = -1;
      }

     //-- Prepare new tx - a very rare case(only for the very fast CPU)

      if(rc == TERR_NO_ERR)
      {
         drv_lpc23xx_net_tx_data(tnet, ni, mb, TRUE); //-- from_interrupt
      }
      else //-- stop TX (99.9999...% we are here)
      {
         nhwi->tx_int_en = FALSE;

         //-- Bit 7 - TxDoneIntEn - not enabled here

         rMAC_INTENABLE |= ~((1<<4) | //-- Bit 4 - TxUnderrunIntEn
                             (1<<5) | //-- Bit 5 - TxErrorIntEn
                             (1<<6)); //-- Bit 6 - TxFinishedIntEn
      }
   }

  //-- Err - tx underrun

   if(status & MAC_MASK_INT_TX_UNDERRUN)
   {
      rMAC_INTCLEAR = MAC_MASK_INT_TX_UNDERRUN;

      //-- ToDo - Add Statistic here

      rMAC_COMMAND |= (1<<4); //-- Bit 4 TxReset;
      rMAC_COMMAND |= (1<<1); //-- Bit 1 TxEnable;
   }
}

