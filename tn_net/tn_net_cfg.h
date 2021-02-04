#ifndef _TN_NET_CFG_H_
#define _TN_NET_CFG_H_


#define  BIG_ENDIAN             1
#define  LITTLE_ENDIAN          2


#define  CPU_BYTE_ORDER        LITTLE_ENDIAN

//------ Num memory buffers

#define  NUM_SMALL_BUF          64   //--   32 bytes each
#define  NUM_MID1_BUF           48   //--  128 bytes each

#define  NUM_HI_BUF              6   //-- Ethernet Rx only, 1536 bytes each

#define  NUM_DRV_SMALL_BUF       8   //-- Ethernet header,  32 bytes each
#define  NUM_DRV_MID1_BUF       48   //--  128 bytes each

#define  EXP_RX_BUF   1 //-- A rx buffers (1536 bytes each) are "very expensive"

    //-- LPC2368

#define  MEM_DRV_START_ADDR     0x7FE00000u

//-- Max IP fragments queue size

#define  MAX_IP_REASS_QSIZE    20

    //-- For CPU like LPC2368(Eth memory is 16Kbytes) do not change this values !

#define  IP4_REASM_PACKETS          1  //-- According to the avaliable CPU Ethernet memory
#define  IP4_MAX_REASM_FRAGMENTS    2  //-- Max input packet size is 2 MTU (3000 bytes)

//--- Interfaces

#define  USE_PHY_KS8721             1

      //-- Interface N1 - Ethernet LPC2368

#define  IFACE1_ARP_QUEUE_SIZE          8

#define  IFACE1_TASK_RX_PRIORITY        4
#define  IFACE1_TASK_RX_STK_SIZE      512 //256

#define  IFACE1_RX_QUEUE_SIZE          16
#define  IFACE1_TX_QUEUE_SIZE          16

//--- Protocols

     //-- DHCP

#define  TN_DHCP  1

      //-- UDP

#define  UDP_MAX_TX_SIZE             4096   //-- It's project depends
#define  USE_UDP_CHKSUM                 1

      //-- TCP

#define TN_TCP 1                     //-- Use TCP

#define TCP_MIN_FREE_BUF_FOR_NEWCONN   20 
//----------------------------------------------------------------------------

#endif



