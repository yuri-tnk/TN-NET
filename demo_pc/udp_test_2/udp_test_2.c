/*
Copyright © 2004,2009 Yuri Tiomkin
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

/*============================================================================

    TN NET TCP/IP Stack - UDP_Test_2

    This is a UDP & TDTP protocol in action!

    This example works together with a PC-based "UDP_server" application (in
  examples)

    There are 2 network related user tasks in the project - TASK_PROCESSING and
  TASK_DAQ. Each task has an own UDP socket to transfer data.
    Both network related user tasks have an equal priority and performs a UDP data
  exchange simultaneously.


    A TASK_PROCESSING task sends a on-the-fly generated file (~4Mbytes) to the "UDP_server"
  application, when a "Receive to file" radio button is selected in the "UDP_server" dialog
  window.
    When "Send file" radio button is selected in the "UDP_server" dialog window,
  a file from the PC(an user should choose the file name) is transferred to the device.
    Both transfers are performed with TDTP protocol.


    A TASK_DAQ task simulates a data acquisition process by the "UDP_server"
  application request. After a request from the "UDP_server" application, the
  task sends a "data" to the "UDP_server".
    The transfer is performed with TDTP protocol.

*===========================================================================*/

#include <stdio.h>
#include <string.h>
#include "../tn_net/cpu_lpc23xx/LPC236X.h"

#include "../tnkernel/tn.h"

#include "../tn_net/tn_net_cfg.h"
#include "../tn_net/tn_net_types.h"
#include "../tn_net/tn_net_pcb.h"
#include "../tn_net/tn_net_mem.h"
#include "../tn_net/tn_ip.h"
#include "../tn_net/tn_net.h"
#include "../tn_net/errno.h"
#include "../tn_net/tn_mbuf.h"
#include "../tn_net/tn_netif.h"
#include "../tn_net/tn_net_utils.h"
#include "../tn_net/bsd_socket.h"
#include "../tn_net/tn_socket.h"

#ifdef TN_DHCP
#include "../tn_net/tn_dhcp.h"
#endif

#include "../tn_net/tn_socket_func.h"
#include "../tn_net/dbg_func.h"

#include "tdtp_err.h"
#include "tdtp.h"
#include "types.h"

//----------------------------------------------------------------------------
//     RTOS related items
//----------------------------------------------------------------------------

    //--- Tasks -----

#define  TASK_PROCESSING_PRIORITY   11
#define  TASK_IO_PRIORITY           14
#define  TASK_DAQ_PRIORITY          11

#define  TASK_PROCESSING_STK_SIZE  192
#define  TASK_IO_STK_SIZE          128
#define  TASK_DAQ_STK_SIZE         192

unsigned int task_processing_stack[TASK_PROCESSING_STK_SIZE];
unsigned int task_io_stack[TASK_IO_STK_SIZE];
unsigned int task_daq_stack[TASK_DAQ_STK_SIZE];

TN_TCB  task_processing;
TN_TCB  task_io;
TN_TCB  task_daq;

void task_processing_func(void * par);
void task_io_func(void * par);
void task_daq_func(void * par);

    //--- Semaphores -----

TN_SEM  semTxUart;

//----------------------------------------------------------------------------
//     Network
//----------------------------------------------------------------------------

    //--- Network base data structure - single per device

TN_NET   g_tnet;
TN_NET * g_tnet_addr = &g_tnet;
TN_NETINFO  g_tneti;

    //--- Interface N1 (iface1) - LPC23xx MAC

extern TN_NETIF g_iface1;

//--- External functions prototypes

int net_iface1_init(TN_NETINFO * tnet);
int net_init_network(TN_NETINFO * tneti);
void daq_cli(TDTPCLI * cli);
void daq_prepare_example_data(void);

//--- UART buffers

char g_udp_tst_buf[32];
char dbg_buf[64];

//-------- UDP addresses -------------------------------------------------

#define  UDP_RW_PORT_LOCAL     11093
#define  UDP_RW_PORT_REMOTE    50001

#define  UDP_DAQ_PORT_LOCAL    11094
#define  UDP_DAQ_PORT_REMOTE   50000

//--- Host(server) IP address

const char g_srv_ip_addr[] = "10.101.0.132";

//--- TDTP & additional data for the file transferring (uses 'udp_socket_ft')

    //-- Socket

volatile int udp_socket_ft = -1;

    //-- TDTP  struct & buffers

TDTPCLI  g_tdtp_cli_ft;                //-- File transferring by server request
unsigned char g_cli_ft_tx_buf[1024];
unsigned char g_cli_ft_rx_buf[1024];

    //-- File buf (simulation)

unsigned char g_ft_file_buf[1024];

    //-- Flags

volatile int g_hFlashOpN1 = 0;
volatile int g_hFlashOpN2 = 0;

//--- TDTP & additional data for the data acquisition transferring (uses 'udp_socket_daq')

   //-- Socket

volatile int udp_socket_daq = -1;

   //-- TDTP  struct & buffers

TDTPCLI  g_tdtp_cli_daq;               //-- Data sending by server request
unsigned char g_cli_daq_tx_buf[1024];
unsigned char g_cli_daq_rx_buf[1024];

   //-- Data acquisition buf (simulation)

DAQDATASIM g_daq_data;

   //-- Flags

volatile int g_hDAQ_N1 = 0;

//----------------------------------------------------------------------------
int main()
{
   tn_arm_disable_interrupts();

   InitHardware();

   g_tneti.tnet  = &g_tnet;
   g_tneti.ni[0] = &g_iface1;

//---- TDTP for the file transferring (uses 'udp_socket_ft') ----

   memset(&g_tdtp_cli_ft, 0, sizeof(TDTPCLI));

   g_tdtp_cli_ft.tx_buf = g_cli_ft_tx_buf;
   g_tdtp_cli_ft.rx_buf = g_cli_ft_rx_buf;

//--- TDTP for the data acquisition transferring (uses 'udp_socket_daq')

   memset(&g_tdtp_cli_daq, 0, sizeof(TDTPCLI));

   g_tdtp_cli_daq.tx_buf = g_cli_daq_tx_buf;
   g_tdtp_cli_daq.rx_buf = g_cli_daq_rx_buf;

 //---------------------------------------------------------------

   tn_start_system(); //-- Never returns

   return 1;
}

//----------------------------------------------------------------------------
void  tn_app_init()
{
  //--- TN_NET

   net_init_network(&g_tneti);

   g_tnet.netif_default = &g_iface1;
   g_tnet.netif_list    = &g_iface1; //-- Root of the iface list

  //-- Interfaces

   net_iface1_init(&g_tneti);

   //--- Task processing

   task_processing.id_task = 0;
   tn_task_create(&task_processing,              //-- task TCB
                 task_processing_func,           //-- task function
                 TASK_PROCESSING_PRIORITY,       //-- task priority
                 &(task_processing_stack         //-- task stack first addr in memory
                    [TASK_PROCESSING_STK_SIZE-1]),
                 TASK_PROCESSING_STK_SIZE,       //-- task stack size (in int,not bytes)
                 NULL,                           //-- task function parameter
                 TN_TASK_START_ON_CREATION);     //-- Creation option

   //--- Task IO

   task_io.id_task = 0;
   tn_task_create(&task_io,                      //-- task TCB
                 task_io_func,                   //-- task function
                 TASK_IO_PRIORITY,               //-- task priority
                 &(task_io_stack                 //-- task stack first addr in memory
                    [TASK_IO_STK_SIZE-1]),
                 TASK_IO_STK_SIZE,               //-- task stack size (in int,not bytes)
                 NULL,                           //-- task function parameter
                 TN_TASK_START_ON_CREATION);     //-- Creation option

   //--- Task DAQ

   task_daq.id_task = 0;
   tn_task_create(&task_daq,                     //-- task TCB
                 task_daq_func,                  //-- task function
                 TASK_DAQ_PRIORITY,              //-- task priority
                 &(task_daq_stack                //-- task stack first addr in memory
                    [TASK_DAQ_STK_SIZE-1]),
                 TASK_DAQ_STK_SIZE,              //-- task stack size (in int,not bytes)
                 NULL,                           //-- task function parameter
                 TN_TASK_START_ON_CREATION);     //-- Creation option

//------------------------------------

   //--- Semaphores

   semTxUart.id_sem = 0;
   tn_sem_create(&semTxUart,1,1);
}

//----------------------------------------------------------------------------
void task_processing_func(void * par)
{
   struct sockaddr__in s_a;
   int name_len = 0;
   int rc;
   unsigned int tmp;

   for(;;)
   {
      if(udp_socket_ft == -1)
      {
#ifdef TN_DHCP
         if(dhcp_ready(&g_iface1) == FALSE)
         {
            tn_task_sleep(100);
            continue;
         }
#else
         tn_task_sleep(1500);
#endif
         udp_socket_ft = s_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
         if(udp_socket_ft != -1)
         {
           //--- Call s_bind() to set a local port for the UDP socket.

            s_a.sin_len          = sizeof(struct sockaddr__in);
            s_a.sin_family       = AF_INET;
            s_a.sin_port         = htons((unsigned short)UDP_RW_PORT_LOCAL); //-- Port
            s_a.sin_addr.s__addr = _INADDR_ANY;   //--

            name_len = s_a.sin_len;

            rc = s_bind(udp_socket_ft, (struct _sockaddr *)&s_a, name_len);
            if(rc != TERR_NO_ERR)
            {
                //-- Add your error handler here
            }

          //--- Call s_connect() to set a remote IP address & port for the UDP socket.

            s_a.sin_len          = sizeof(struct sockaddr__in);
            s_a.sin_family       = AF_INET;
            s_a.sin_port         = htons((unsigned short)UDP_RW_PORT_REMOTE); //-- Port

            ip4_str_to_num((char*)g_srv_ip_addr, &tmp); //-- dst (server) IP address
            s_a.sin_addr.s__addr = htonl(tmp);

            name_len = s_a.sin_len;

            rc = s_connect(udp_socket_ft, (struct _sockaddr *)&s_a, name_len);
            if(rc != TERR_NO_ERR)
            {
               s_close(udp_socket_ft);
               udp_socket_ft = -1;
               tn_task_sleep(1000); //-- Repeat attempt(re-open etc.)after 1 s
               continue;
            }

          //--- Set rx timeout

            tmp = 100;
            s_setsockopt(udp_socket_ft,
                         0,                          //-- level,
                         SO_RCVTIMEO,                //-- name,
                         (unsigned char *)&tmp,
                         sizeof(int));               //-- size
           //-- Also valid
           // s_ioctl(udp_socket_ft, _FIORXTIMEOUT, &tmp);

//---- TDTP info -----------------------------------------

            g_tdtp_cli_ft.s_tx   = (void*)udp_socket_ft;
            g_tdtp_cli_ft.s_rx   = g_tdtp_cli_ft.s_tx;

//--------------------------------------------------------
         }
         else
         {
            tn_task_sleep(1000); //-- Repeat attempt to open after 1 s
         }
      }
      else
      {
         tdtp_cli(&g_tdtp_cli_ft);
      }
   }
}

//----------------------------------------------------------------------------
void task_io_func(void * par)
{
   int cnt = 0;

   for(;;)
   {
      rFIO1SET |= LED_MASK;
      tn_task_sleep(166);
      rFIO1CLR |= LED_MASK;
      tn_task_sleep(167);

      cnt++;

      if(cnt == 3) //-- 1 s
      {
         cnt = 0;
         dbg_send("Timemark 1 s.\r\n");
      }

   }
}

//----------------------------------------------------------------------------
void task_daq_func(void * par)
{
   struct sockaddr__in s_a;
   int name_len = 0;
   int rc;
   unsigned int tmp;

   for(;;)
   {
      if(udp_socket_daq == -1)
      {
#ifdef TN_DHCP
         if(dhcp_ready(&g_iface1) == FALSE)
         {
            tn_task_sleep(100);
            continue;
         }
#else
         tn_task_sleep(1500);
#endif
         udp_socket_daq = s_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
         if(udp_socket_daq != -1)
         {
           //--- Call s_bind() to set a local port for the UDP socket.

            s_a.sin_len          = sizeof(struct sockaddr__in);
            s_a.sin_family       = AF_INET;
            s_a.sin_port         = htons((unsigned short)UDP_DAQ_PORT_LOCAL); //-- Port
            s_a.sin_addr.s__addr = _INADDR_ANY;   //--

            name_len = s_a.sin_len; //IP4_SIN_ADDR_SIZE;

            rc = s_bind(udp_socket_daq, (struct _sockaddr *)&s_a, name_len);
            if(rc != TERR_NO_ERR)
            {
                //-- Add your error handler here
            }

          //--- Call s_connect() to set a remote IP address & port for the UDP socket.

            s_a.sin_len          = sizeof(struct sockaddr__in);
            s_a.sin_family       = AF_INET;
            s_a.sin_port         = htons((unsigned short)UDP_DAQ_PORT_REMOTE); //-- Port

            ip4_str_to_num((char*)g_srv_ip_addr, &tmp); //-- dst (server) IP address
            s_a.sin_addr.s__addr = htonl(tmp);

            name_len = s_a.sin_len;

            rc = s_connect(udp_socket_daq, (struct _sockaddr *)&s_a, name_len);
            if(rc != TERR_NO_ERR)
            {
               s_close(udp_socket_daq);
               udp_socket_daq = -1;
               tn_task_sleep(1000); //-- Repeat attempt(re-open etc.)after 1 s
               continue;
            }

          //--- Set rx timeout

            tmp = 200;
            s_setsockopt(udp_socket_daq,
                         0,                          //-- level,
                         SO_RCVTIMEO,                //-- name,
                         (unsigned char *)&tmp,
                         sizeof(int));               //-- size

           //-- Also valid
           // s_ioctl(udp_socket_daq, _FIORXTIMEOUT, &tmp);


//---- TDTP info -----------------------------------------

            g_tdtp_cli_daq.s_tx   = (void*)udp_socket_daq;
            g_tdtp_cli_daq.s_rx   = g_tdtp_cli_daq.s_tx;

            daq_prepare_example_data(); //-- New
//--------------------------------------------------------
         }
         else
         {
            tn_task_sleep(1000); //-- Repeat attempt to open after 1 s
         }
      }
      else
      {
        //--- This function uses just TDTP data structure,
        //--- but not performs a TDTP protocol

         daq_cli(&g_tdtp_cli_daq);
      }
   }
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



