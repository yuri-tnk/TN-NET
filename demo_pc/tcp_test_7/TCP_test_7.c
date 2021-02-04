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

  TN NET TCP/IP stack

  TCP_Test_7 -  - TCP server (Reception & Transmission) with open/close sockets

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
#include "../tn_net/bsd_socket.h"
#include "../tn_net/tn_socket.h"
#include "../tn_net/tn_net_utils.h"
#include "../tn_net/tn_protosw.h"
#include "../tn_net/tn_tcp.h"
#include "../tn_net/tn_sockets_tcp.h"
#include "../tn_net/tn_net_mem_func.h"

#ifdef TN_DHCP
#include "../tn_net/tn_dhcp.h"
#endif

#include "../tn_net/tn_socket_func.h"
#include "../tn_net/dbg_func.h"

#include "types.h"

//----------------------------------------------------------------------------
//     RTOS related items
//----------------------------------------------------------------------------

    //--- Tasks -----

#define  TASK_IO_PRIORITY           14
#define  TASK_BASE_PRIORITY         15

#define  TASK_IO_STK_SIZE          128
#define  TASK_BASE_STK_SIZE        512 //  256

unsigned int task_io_stack[TASK_IO_STK_SIZE];
unsigned int task_base_stack[TASK_BASE_STK_SIZE];

TN_TCB  task_io;
TN_TCB  task_base;

void task_io_func(void * par);
void task_base_func(void * par);

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

#define  TCP_SRV_PORT    50001


//--- UART buffers

char dbg_buf[96];
unsigned char uart_rx_buf[UART_RX_BUF_SIZE];


//--- Server's tasks data -------------------------------

unsigned int peer_ip4_addr;
struct sockaddr__in  srv_addr;
struct sockaddr__in  peer_addr;


//--- Local functions prototypes ------------------------

#define RX_BUF_SIZE  512
char g_rx_buf[RX_BUF_SIZE];
char g_tx_buf[RX_BUF_SIZE];
volatile int g_s;
int snd_main_func(void);

//----------------------------------------------------------------------------
int main()
 {
   tn_arm_disable_interrupts();

   InitHardware();

   g_s = sizeof(struct socket_tcp);

   g_tneti.tnet  = &g_tnet;
   g_tneti.ni[0] = &g_iface1;

 //---------------------------------------------------------------

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
   tn_task_create(&task_io,                    //-- task TCB
                 task_io_func,                 //-- task function
                 TASK_IO_PRIORITY,             //-- task priority
                 &(task_io_stack               //-- task stack first addr in memory
                    [TASK_IO_STK_SIZE-1]),
                 TASK_IO_STK_SIZE,             //-- task stack size (in int,not bytes)
                 NULL,                         //-- task function parameter
                 TN_TASK_START_ON_CREATION);   //-- Creation option

   //--- Task Base

   task_base.id_task = 0;
   tn_task_create(&task_base,                    //-- task TCB
                 task_base_func,                 //-- task function
                 TASK_BASE_PRIORITY,             //-- task priority
                 &(task_base_stack               //-- task stack first addr in memory
                    [TASK_BASE_STK_SIZE-1]),
                 TASK_BASE_STK_SIZE,             //-- task stack size (in int,not bytes)
                 NULL,                           //-- task function parameter
                 TN_TASK_START_ON_CREATION);     //-- Creation option

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

     //--- UART sends to the host a mbuf statistics

      if(cnt == 9) //-- ~3 s
      {
         cnt = 0;
         tn_snprintf(dbg_buf, 95,
         "l=%d m=%d h=%d ld=%d md=%d \r\n",
               g_tnet.lobuf_mpool.fblkcnt,
               g_tnet.m1buf_mpool.fblkcnt,
               g_tnet.hibuf_mpool.fblkcnt,
               g_tnet.drv_lobuf_mpool.fblkcnt,
               g_tnet.drv_m1buf_mpool.fblkcnt);
         dbg_send(dbg_buf);
      }
   }
}

//----------------------------------------------------------------------------
void task_base_func(void * par)
{
   int peer_addr_len;
   int rc;
   int s_srv = 0;
   int s_curr= 0;
//   struct _linger lg;


   //-------------------------------------

   for(;;)
   {
      if(s_srv == 0) //-- There is no an open base server socket yet
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
         s_srv = s_socket(AF_INET, SOCK_STREAM, 0);
         if(s_srv != 0 && s_srv != -1)
         {
            srv_addr.sin_len          = sizeof(struct sockaddr__in);
            srv_addr.sin_family       = AF_INET;
            srv_addr.sin_port         = htons((unsigned short)TCP_SRV_PORT); //-- Port
            srv_addr.sin_addr.s__addr = _INADDR_ANY;   //--

            rc = s_bind(s_srv, (struct _sockaddr *)&srv_addr, srv_addr.sin_len);
            if(rc != TERR_NO_ERR)
            {
               s_close(s_srv);
               s_srv = 0;

               tn_snprintf(dbg_buf,90,"bind() failed. Err: %d.\r\n", rc);
               dbg_send(dbg_buf);
               continue;
            }

            rc = s_listen(s_srv, 1);
            if(rc != TERR_NO_ERR)
            {
               s_close(s_srv);
               s_srv = 0;

               tn_snprintf(dbg_buf,90,"listen() failed. Err: %d.\r\n", rc);
               dbg_send(dbg_buf);
               continue;
            }
            dbg_send("Listen...\r\n");
    //------------------------------------------------------------------------
         }
         else
         {
            tn_snprintf(dbg_buf,90,"socket() failed. Err: %d.\r\n", s_srv);
            dbg_send(dbg_buf);
            s_srv = 0;
            continue;
         }
      }
      else
      {
         for(;;)
         {
            peer_addr_len = sizeof(struct sockaddr__in);
            g_s = s_accept(s_srv, (struct _sockaddr *) &peer_addr, &peer_addr_len);
            if(g_s < 0) //-- could not accept
            {
               tn_snprintf(dbg_buf,90,"accept() failed. Err: %d.\r\n", s_curr);
               dbg_send(dbg_buf);
               tn_task_sleep(500);
               continue;
            }
            else //-- Accept is o.k. - find free processing task
            {
/*
               lg.l_onoff  = 1;   //-- SO_LINGER - on
               lg.l_linger = 0;  //-- if linger time = 0 - force RST instead FIN
               s_setsockopt(g_s,
                            0,           //--level
                            SO_LINGER,  //-- name,
                            (unsigned char *)&lg,
                            sizeof(struct _linger));
*/
dbg_send("Accept!\r\n");
               snd_main_func();
            }
         }
      }
  //--------------------------------
   }
}

//----------------------------------------------------------------------------
int snd_main_func(void)
{
   int rc;

   for(;;)
   {
      rc = s_recv(g_s, (unsigned char*)g_rx_buf, RX_BUF_SIZE, 0);
      if(rc < 0) //-- SOCKET_ERROR
      {
         dbg_send("Rx Error 1\r\n");
         s_close(g_s);
         break;
      }
      else if(rc == 0)
      {
         dbg_send("closed by peer\r\n");
         s_close(g_s);
         break;
      }
      else
      {

#define CHK_SEND 1
#ifdef CHK_SEND

         rc = s_send(g_s, (unsigned char*)g_tx_buf, 25, 0);
         if(rc <= 0) //-- SOCKET_ERROR
         {
            dbg_send("Rx Error 2\r\n");
            s_close(g_s);
            break;
         }
         s_shutdown(g_s, SHUT_WR);
#endif
      }
    //-----
   }

   return 0;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


