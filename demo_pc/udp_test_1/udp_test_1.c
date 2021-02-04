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

/*============================================================================
  TN NET TCP/IP Stack - UDP_Test_1

  This is a UDP data acquisition example.
  The device sends a packet to the host, host sends back an ACK response.

   This example works together with a PC-based "UDP_daq" application (in
 examples)

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
#include "types.h"

//----------------------------------------------------------------------------
//     RTOS related items
//----------------------------------------------------------------------------

    //--- Tasks -----

#define  TASK_DAQ_PRIORITY          11
#define  TASK_IO_PRIORITY           14

#define  TASK_IO_STK_SIZE          128
#define  TASK_DAQ_STK_SIZE         256

unsigned int task_io_stack[TASK_IO_STK_SIZE];
unsigned int task_daq_stack[TASK_DAQ_STK_SIZE];

TN_TCB  task_io;
TN_TCB  task_daq;

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

//-------- Non - OS  globals -------------------------------------------------

#define  UDP_DAQ_PORT_LOCAL    11094
#define  UDP_DAQ_PORT_REMOTE   50000

//--- Host(server) IP address

const char g_srv_ip_addr[] = "10.101.0.132";
//const char g_srv_ip_addr[] = "192.168.2.100";

//--- UART buffers

char dbg_buf[96];
unsigned char uart_rx_buf[UART_RX_BUF_SIZE];


//----------------------------------------------------------------------------
//   Data acquisition transferring (uses 'udp_socket_daq')
//----------------------------------------------------------------------------

   //-- Socket

volatile int udp_socket_daq = -1;

   //-- Structures & buffers

DAQPROTINFO g_daq;               //-- Data acquisition base structure

unsigned char g_daq_tx_buf[1024];
unsigned char g_daq_rx_buf[1024];

DAQDATASIM g_daq_data;          //-- Data acquisition buffer (simulation)

   //-- A type(address) of the data acquisition

const char g_daq_addr[] = "daq_mode_N1";

//----------------------------------------------------------------------------
int main()
{
   tn_arm_disable_interrupts();

   InitHardware();

   g_tneti.tnet  = &g_tnet;
   g_tneti.ni[0] = &g_iface1;

//--- Data acquisition transferring (uses 'udp_socket_daq')

   memset(&g_daq, 0, sizeof(DAQPROTINFO));

   g_daq.tx_buf = g_daq_tx_buf;
   g_daq.rx_buf = g_daq_rx_buf;

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

     //--- UART sends to the host a transferr statistics

      if(cnt == 3) //-- ~1 s
      {
         cnt = 0;
         tn_snprintf(dbg_buf, 95,
               "tx_ok=%d tx_repeats=%d rx_timeouts=%d err_sock_rx=%d err_timeout=%d\r\n",
               g_daq.tx_ok,
               g_daq.tx_repeats,
               g_daq.rx_timeouts,
               g_daq.err_sock_rx,
               g_daq.err_timeout);

         dbg_send(dbg_buf);
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
   int tx_len = 0;
   int cnt = 0;

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

            name_len = s_a.sin_len;

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
               tn_task_sleep(1000); //-- Repeat the attempt(re-open etc.)after 1 s
               continue;
            }

          //--- Set rx timeout
            tmp = 200;  //-- Timeout value in ms - here timeout is 200 ms
            s_setsockopt(udp_socket_daq,
                         0,                          //-- level,
                         SO_RCVTIMEO,                //-- name,
                         (unsigned char *)&tmp,
                         sizeof(int));               //-- size

           //-- Also valid
           // s_ioctl(udp_socket_daq, _FIORXTIMEOUT, &tmp);

//---- DAQ preparing -----------------------------------------

            g_daq.s_tx = udp_socket_daq;
            g_daq.s_rx = g_daq.s_tx;

            daq_example_prepare_data();

            g_daq.addr_ptr = (unsigned char *)&g_daq_addr[0];
            g_daq.addr_len = strlen(g_daq_addr);

            cnt = 0;
            daq_example_new_data_to_send(&g_daq, cnt);

            tx_len = build_pkt_WR_daq(&g_daq);
//--------------------------------------------------------
         }
         else
         {
            tn_task_sleep(1000); //-- Repeat attempt to open after 1 s
         }
      }
      else //-- Regular operations
      {
         rc = daq_send(&g_daq, tx_len);
         if(rc == ERR_OK)
         {
         //---- Sending a new DAQ data packet ----------------
            cnt++;
            daq_example_new_data_to_send(&g_daq, cnt);
            tx_len = build_pkt_WR_daq(&g_daq);
         //-------------------------------------------------
         }
      }
   }
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



