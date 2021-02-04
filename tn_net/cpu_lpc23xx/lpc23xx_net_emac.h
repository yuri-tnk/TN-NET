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


#ifndef _LPC23XX_NET_EMAC_H_
#define _LPC23XX_NET_EMAC_H_



  //-- Interrupt Status/Enable/Clear/Set Registers

#define  MAC_MASK_INT_RX_OVERRUN     0x00000001     //-- Overrun Error in RX Queue
#define  MAC_MASK_INT_RX_ERR         0x00000002     //-- Receive Error
#define  MAC_MASK_INT_RX_FIN         0x00000004     //-- RX Finished Process Descriptors
#define  MAC_MASK_INT_RX_DONE        0x00000008     //-- Receive Done
#define  MAC_MASK_INT_TX_UNDERRUN    0x00000010     //-- Transmit Underrun
#define  MAC_MASK_INT_TX_ERR         0x00000020     //-- Transmit Error
#define  MAC_MASK_INT_TX_FIN         0x00000040     //-- TX Finished Process Descriptors
#define  MAC_MASK_INT_TX_DONE        0x00000080     //-- Transmit Done
#define  MAC_MASK_INT_SOFT_INT       0x00001000     //-- Software Triggered Interrupt
#define  MAC_MASK_INT_WAKEUP         0x00002000     //-- Wakeup Event Interrupt

      //-- Errors

#define  ERR_RX_ERR                       -2001
#define  ERR_WR_PHY                       -2002
#define  ERR_RD_PHY                       -2003
#define  ERR_RES_PHY                      -2004
#define  ERR_PHY_ID                       -2005

 //----- MAC descriptors --------------------------------------

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(push, 1)
#endif

struct rx_descr_fields
{
   void * Packet;
   unsigned int Control;
}__pkt_struct;
typedef struct rx_descr_fields TN_MAC_RX_DESCR;

struct rx_status_fields
{
   unsigned int StatusInfo;
   unsigned int StatusHashCRC;
}__pkt_struct;
typedef struct rx_status_fields TN_MAC_RX_STATUS;

struct tx_descr_fields
{
   void * Packet;
   unsigned int Control;
}__pkt_struct;
typedef struct tx_descr_fields TN_MAC_TX_DESCR;

struct tx_status_fields
{
   unsigned int StatusInfo;
}__pkt_struct;
typedef struct tx_status_fields TN_MAC_TX_STATUS;

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(pop)
#endif

   //---  Global MAC info

struct mbuf;

struct _LPC23XXNETINFO
{
   int num_tx_descr;
   int num_rx_descr;

   TN_MAC_RX_DESCR  * rx_descr;
   TN_MAC_RX_STATUS * rx_status;
   TN_MAC_TX_DESCR  * tx_descr;
   TN_MAC_TX_STATUS * tx_status;

   struct mbuf * mbuf_to_tx;
   int link_up;
   int tx_int_en;
};
typedef struct _LPC23XXNETINFO LPC23XXNETINFO;


    //--- MAC descriptors & RX descriptors memory

       //--  0x7FE00000 - 0x7FE03FFF Ethernet RAM (16 kB)

#define  MEM_BASE_ADDR_ETHERNET       0x7FE00000u
#define  LPC2368_MAX_ETH_ADDR         0x7FE03FFFu

#define  MAC_NUM_RX_DESCR                     3  //-- 3 ??
#define  MAC_NUM_TX_DESCR                    24  // 20 16

#define  MAC_RX_BUF_SIZE              TNNET_HI_BUF_SIZE   //-- Here - 1536

    //--- Memory map for the Ethernet RAM

#define  MAC_MEM_RX_DESCR_BASE        (MEM_BASE_ADDR_ETHERNET)
#define  MAC_MEM_RX_STATUS_BASE      ((MAC_MEM_RX_DESCR_BASE)  + (sizeof(TN_MAC_RX_DESCR)  * MAC_NUM_RX_DESCR))
#define  MAC_MEM_TX_DESCR_BASE       ((MAC_MEM_RX_STATUS_BASE) + (sizeof(TN_MAC_RX_STATUS) * MAC_NUM_RX_DESCR))
#define  MAC_MEM_TX_STATUS_BASE      ((MAC_MEM_TX_DESCR_BASE)  + (sizeof(TN_MAC_TX_DESCR)  * MAC_NUM_TX_DESCR))
#define  MAC_MEM_RX_BUF_BASE         ((MAC_MEM_TX_STATUS_BASE) + (sizeof(TN_MAC_TX_STATUS) * MAC_NUM_TX_DESCR))
#define  MAC_MEM_DRV_SMALL_BUF_BASE  ((MAC_MEM_RX_BUF_BASE)        + (TNNET_HI_BUF_SIZE    * NUM_HI_BUF))
#define  MAC_MEM_DRV_MID1_BUF_BASE   ((MAC_MEM_DRV_SMALL_BUF_BASE) + (TNNET_SMALL_BUF_SIZE * NUM_DRV_SMALL_BUF))
#define  MAC_MEM_MAX_USE_ADDR        ((MAC_MEM_DRV_MID1_BUF_BASE)  + (TNNET_MID1_BUF_SIZE  * NUM_DRV_MID1_BUF))




//--- According to the 'tn_net_cfg.h' select PHY

#ifdef USE_PHY_KS8721  //-- define it inside 'tn_net_cfg.h'

#define   phy_read(a, b)            ks8721_phy_read(a, b)
#define   phy_write(a, b)           ks8721_phy_write(a, b)
#define   phy_init(a)               ks8721_phy_init(a)
#define   phy_update_link_state()   ks8721_phy_update_link_state()
#define   phy_is_link_up()          ks8721_phy_is_link_up()

// #elif defined - another PHY etc

#else

#error "PHY not defined"

#endif

#endif
