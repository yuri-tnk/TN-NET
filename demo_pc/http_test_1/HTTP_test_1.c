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

  HTTP_Test_1 - a simple embedded WEB server

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
#include "../tn_net/tn_tcp.h"
#include "../tn_net/tn_sockets_tcp.h"

#ifdef TN_DHCP
#include "../tn_net/tn_dhcp.h"
#endif

#include "../tn_net/tn_socket_func.h"
#include "../tn_net/dbg_func.h"

#include "types.h"
#include "tn_httpd.h"

//----------------------------------------------------------------------------
//     RTOS related items
//----------------------------------------------------------------------------

    //--- Tasks -----

#define  TASK_IO_PRIORITY           14
#define  TASK_BASE_PRIORITY         15
#define  TASK_SRV_PROC_PRIORITY     16

#define  TASK_IO_STK_SIZE          128
#define  TASK_BASE_STK_SIZE        256
#define  TASK_SRV_PROC_STK_SIZE    256

unsigned int task_io_stack[TASK_IO_STK_SIZE];
unsigned int task_base_stack[TASK_BASE_STK_SIZE];
unsigned int task_srv_0_stack[TASK_SRV_PROC_STK_SIZE];
unsigned int task_srv_1_stack[TASK_SRV_PROC_STK_SIZE];
unsigned int task_srv_2_stack[TASK_SRV_PROC_STK_SIZE];
unsigned int task_srv_3_stack[TASK_SRV_PROC_STK_SIZE];

TN_TCB  task_io;
TN_TCB  task_base;
TN_TCB  task_srv_0;
TN_TCB  task_srv_1;
TN_TCB  task_srv_2;
TN_TCB  task_srv_3;

void task_io_func(void * par);
void task_base_func(void * par);
void task_srv_proc_func(void * par);  //-- The same for the all srv tasks

    //--- Semaphores -----

TN_SEM  semTxUart;
TN_SEM  semSrvResLock;

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

#define  TCP_SRV_PORT    80

//--- Host(server) IP address

//const char g_srv_ip_addr[] = "10.101.0.132";
//const char g_srv_ip_addr[] = "192.168.2.100";

//--- UART buffers

char dbg_buf[96];
unsigned char uart_rx_buf[UART_RX_BUF_SIZE];

HTTPDINFO g_httpd_0;
HTTPDINFO g_httpd_1;
HTTPDINFO g_httpd_2;
HTTPDINFO g_httpd_3;

//--- Server's tasks data -------------------------------

//unsigned int peer_ip4_addr;
struct sockaddr__in  g_srv_addr;
struct sockaddr__in  g_peer_addr;

typedef struct _SERVERTSKDATA
{
   HTTPDINFO * hti;
   int socket;
}SERVERTSKDATA;

#define NUM_AVAL_SERVER_TASKS  4

SERVERTSKDATA g_srv_tasks_pool[NUM_AVAL_SERVER_TASKS];

//--- Local functions prototypes ------------------------

int server_find_free_task(int curr_socket);
void init_httpd_info(HTTPDINFO * hti);

//---- Debug ----------


//----------------------------------------------------------------------------
int main()
 {
   tn_arm_disable_interrupts();

   InitHardware();

   g_tneti.tnet  = &g_tnet;
   g_tneti.ni[0] = &g_iface1;

 //---------------------------------------------------------------

   g_srv_tasks_pool[0].hti    = &g_httpd_0;
   g_srv_tasks_pool[0].socket = 0;

   g_srv_tasks_pool[1].hti    = &g_httpd_1;
   g_srv_tasks_pool[1].socket = 0;

   g_srv_tasks_pool[2].hti    = &g_httpd_2;
   g_srv_tasks_pool[2].socket = 0;

   g_srv_tasks_pool[3].hti    = &g_httpd_3;
   g_srv_tasks_pool[3].socket = 0;

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

   //--- TCP server tasks

   task_srv_0.id_task = 0;
   tn_task_create(&task_srv_0,                    //-- task TCB
                 task_srv_proc_func,              //-- task function
                 TASK_SRV_PROC_PRIORITY,          //-- task priority
                 &(task_srv_0_stack               //-- task stack first addr in memory
                    [TASK_SRV_PROC_STK_SIZE-1]),
                 TASK_SRV_PROC_STK_SIZE,          //-- task stack size (in int,not bytes)
                 (void*)&g_srv_tasks_pool[0],     //-- task function parameter
                 TN_TASK_START_ON_CREATION);      //-- Creation option

   task_srv_1.id_task = 0;
   tn_task_create(&task_srv_1,                    //-- task TCB
                 task_srv_proc_func,              //-- task function
                 TASK_SRV_PROC_PRIORITY,          //-- task priority
                 &(task_srv_1_stack               //-- task stack first addr in memory
                    [TASK_SRV_PROC_STK_SIZE-1]),
                 TASK_SRV_PROC_STK_SIZE,          //-- task stack size (in int,not bytes)
                 (void*)&g_srv_tasks_pool[1],     //-- task function parameter
                 TN_TASK_START_ON_CREATION);      //-- Creation option

   task_srv_2.id_task = 0;
   tn_task_create(&task_srv_2,                    //-- task TCB
                 task_srv_proc_func,              //-- task function
                 TASK_SRV_PROC_PRIORITY,          //-- task priority
                 &(task_srv_2_stack               //-- task stack first addr in memory
                    [TASK_SRV_PROC_STK_SIZE-1]),
                 TASK_SRV_PROC_STK_SIZE,          //-- task stack size (in int,not bytes)
                 (void*)&g_srv_tasks_pool[2],     //-- task function parameter
                 TN_TASK_START_ON_CREATION);      //-- Creation option

   task_srv_3.id_task = 0;
   tn_task_create(&task_srv_3,                    //-- task TCB
                 task_srv_proc_func,              //-- task function
                 TASK_SRV_PROC_PRIORITY,          //-- task priority
                 &(task_srv_3_stack               //-- task stack first addr in memory
                    [TASK_SRV_PROC_STK_SIZE-1]),
                 TASK_SRV_PROC_STK_SIZE,          //-- task stack size (in int,not bytes)
                 (void*)&g_srv_tasks_pool[3],     //-- task function parameter
                 TN_TASK_START_ON_CREATION);      //-- Creation option

   //--- Semaphores

   semTxUart.id_sem = 0;
   tn_sem_create(&semTxUart, 1, 1);

   semSrvResLock.id_sem = 0;
   tn_sem_create(&semSrvResLock, 1, 1);

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
   //struct _linger lg;

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
            g_srv_addr.sin_len          = sizeof(struct sockaddr__in);
            g_srv_addr.sin_family       = AF_INET;
            g_srv_addr.sin_port         = htons((unsigned short)TCP_SRV_PORT); //-- Port
            g_srv_addr.sin_addr.s__addr = _INADDR_ANY;   //--

            rc = s_bind(s_srv, (struct _sockaddr *)&g_srv_addr, g_srv_addr.sin_len);
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
            dbg_send("Listen OK\r\n");
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
         peer_addr_len = sizeof(struct sockaddr__in);
         s_curr = s_accept(s_srv, (struct _sockaddr *) &g_peer_addr, &peer_addr_len);

         if(s_curr < 0) //-- could not accept
         {
               tn_snprintf(dbg_buf,90,"accept() failed. Err: %d.\r\n", s_curr);
               dbg_send(dbg_buf);
               tn_task_sleep(500);
               continue;
         }
         else //-- Accept is o.k.
         {
/*
            lg.l_onoff  = 1;   //-- SO_LINGER - on
            lg.l_linger = 0;  //-- if linger time = 0 - force RST instead FIN
            s_setsockopt(s_curr,
                         0,           //--level
                         SO_LINGER,  //-- name,
                         (unsigned char *)&lg,
                         sizeof(struct _linger));
*/
        //    s_close(s_curr);
        //    continue;

            rc = server_find_free_task(s_curr);
            if(rc != TERR_NO_ERR)
            {
               dbg_send("No free tasks in pool.\r\n");
/*
               lg.l_onoff  = 1;   //-- SO_LINGER - on
               lg.l_linger = 0;  //-- if linger time = 0 - force RST instead FIN
               s_setsockopt(s_curr,
                            0,           //--level
                            SO_LINGER,  //-- name,
                            (unsigned char *)&lg,
                            sizeof(struct _linger));
*/
               s_close(s_curr);
               continue;
            }

         }
      }
   }
}

//----------------------------------------------------------------------------
void task_srv_proc_func(void * par)
{
   SERVERTSKDATA * sd;
   int flag;

   sd = (SERVERTSKDATA *)par;

   for(;;)
   {
      tn_sem_acquire(&semSrvResLock, TN_WAIT_INFINITE);
      flag = (sd->socket != 0);
      tn_sem_signal(&semSrvResLock);
      if(flag)
      {
         init_httpd_info(sd->hti);
         sd->hti->s = sd->socket;

         httpd_main_func(sd->hti);

         tn_sem_acquire(&semSrvResLock, TN_WAIT_INFINITE);
         sd->socket = 0;
         tn_sem_signal(&semSrvResLock);
      }
      else
         tn_task_sleep(5);
   }
}

//----------------------------------------------------------------------------
int server_find_free_task(int curr_socket)
{
   int i;

   tn_sem_acquire(&semSrvResLock, TN_WAIT_INFINITE);

   for(i=0; i < NUM_AVAL_SERVER_TASKS; i++)
   {
      if(g_srv_tasks_pool[i].socket == 0)
      {
         g_srv_tasks_pool[i].socket = curr_socket;
         tn_sem_signal(&semSrvResLock);
         return 0;
      }
   }
   tn_sem_signal(&semSrvResLock);

   return  -1; //-- Err
}

//----------------------------------------------------------------------------
void init_httpd_info(HTTPDINFO * hti)
{
   httpd_init(hti);

   hti->port = TCP_SRV_PORT;
   hti->ip_addr[0] =  g_srv_addr.sin_addr.s__addr & 0xFF;
   hti->ip_addr[1] = (g_srv_addr.sin_addr.s__addr>>8) & 0xFF;
   hti->ip_addr[2] = (g_srv_addr.sin_addr.s__addr>>16) & 0xFF;
   hti->ip_addr[3] = (g_srv_addr.sin_addr.s__addr>>24) & 0xFF;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



