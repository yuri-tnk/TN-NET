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


      //-- Interface N1 - Ethernet LPC2368

#define  IFACE1_ARP_QUEUE_SIZE          8

#define  IFACE1_TASK_RX_PRIORITY        4
#define  IFACE1_TASK_RX_STK_SIZE      512 //256

#define  IFACE1_RX_QUEUE_SIZE          16
#define  IFACE1_TX_QUEUE_SIZE          16

//--- Protocols

     //-- ARP

//-- In some UDP applications(SNMP for example) if we have more than one 
//-- outgoing packets at a time and arp does not have an active entry for 
//-- our peer we lose second packet if arp entry still was not renewed at 
//-- second packet arrival time. Having additional holding entry allows us 
//-- to handle two packets while arp entry renewal is in progress. 
//#define TN_ARP_EXTRA_LAHOLD
     
     //-- DHCP

#define  TN_DHCP  1

      //-- UDP

#define  UDP_MAX_TX_SIZE             4096   //-- It's project depends
#define  USE_UDP_CHKSUM                 1

      //-- TCP

#define TN_TCP 1                     //-- Use TCP

//-- Allows to send RST if we can't handle new TCP connection due to backlog 
//-- queue overflow. A standard behavior is to drop the connection silently. 
//-- But it may be important to change behavior in some applications.
//#define TN_TCP_RESET_ON_SONEWCONN_FAIL

//-- Going in TCPS_FIN_WAIT_2 state with minimal idle timeout.
//-- This allows us to faster close the connection and release resources if 
//-- the peer gets stuck. This is nonstandard, but changing behavior in some 
//-- applications may be important.
//#define TN_TCP_SUPRESS_FW2_MAX_IDLE

//-- Don't use TIME_WAIT state. Closing the connection and release resources 
//-- immediately after passing FIN_WAIT_2 state. This is very speeds up 
//-- freeing resources and improving performance on memory constraint devices. 
//-- Turned off by default because this is nonstandard behavior. 
//#define TN_TCP_SUPRESS_TIME_WAIT

#define TCP_MIN_FREE_BUF_FOR_NEWCONN   20 
//----------------------------------------------------------------------------

#endif



