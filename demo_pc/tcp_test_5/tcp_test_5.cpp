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

  Use it together with TN NET TCP/IP stack 'TCP_Test_5' program

*===========================================================================*/

#include "stdafx.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>

#include "rnd.h"
#include "slipm.h"

#pragma warning(disable: 4996)

#define NUM_RX_TX_REPEATS  100

#define TCP_SRV_PORT     50001 

#define RX_BUF_SIZE  1024
unsigned char g_rx_buf[RX_BUF_SIZE];


SLIPMINFO g_slipm;

WSADATA wsaData;
SOCKET s_l = 0;

#define EXCH_BUF_SIZE  150000
#define NUM_TX_BYTES   70000

unsigned char * g_exch_buf = NULL;
unsigned char * g_tx_buf = NULL;
int g_tx_ind = 0;

int data_exch(void);
BOOL CtrlHandler(DWORD fdwCtrlType);


//----------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
   if(!SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlHandler, TRUE))
   {
      printf( "\nERROR: Could not set control handler");
      while(!_kbhit());
      return -1;
   }

   g_tx_buf = (unsigned char *)malloc(NUM_TX_BYTES);
   g_exch_buf = (unsigned char *)malloc(EXCH_BUF_SIZE);
   if(g_exch_buf == NULL || g_tx_buf == NULL)
   {
      printf( "\nERROR: Out of memory\n");
      while(!_kbhit());
      return -1;
   }

   memset(&g_slipm, 0, sizeof(SLIPMINFO));

   data_exch();

   if(s_l != 0)
   {
      shutdown(s_l, SD_BOTH);
      closesocket(s_l);
   }

   WSACleanup();

   free(g_tx_buf);
   free(g_exch_buf);

  // while(!_kbhit());


   return 0;
}

//----------------------------------------------------------------------------
int data_exch(void)
{
   struct sockaddr_in srv_address;
   int rc;
   int len;
   int i;
   int cycle_num = 0;

 //-- Initialize Winsock

   rc = WSAStartup(MAKEWORD(2,2), &wsaData);
   if(rc != NO_ERROR)
   {
      printf("\nWSAStartup() failed.\n");
      return -1; //error
   }

 //-- Open socket

   s_l = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   if(s_l == INVALID_SOCKET)
   {
      printf("socket() failed: %ld.\n", WSAGetLastError());
      return -1; //error
   }

   //--- fill the address structure & connect

   memset((char *)&srv_address, 0,  sizeof(struct sockaddr_in));
   srv_address.sin_family      = AF_INET;
   srv_address.sin_addr.s_addr = inet_addr("10.101.0.14");//("127.0.01");
   srv_address.sin_port        = htons((unsigned short)TCP_SRV_PORT);

   rc = connect(s_l, (struct sockaddr *) &srv_address,sizeof(struct sockaddr_in)) ;
   if(rc == SOCKET_ERROR)
   {
      printf("connect() failed: %d.\n", WSAGetLastError());
      return -1; //-- Err
   }
   printf("Connected...\n");

   for(;;)
   {

  //---- Receive

      rc = recv(s_l, (char*)g_rx_buf, 512, 0);
      if(rc == SOCKET_ERROR)
      {
         printf("recv() failed. Err: %ld.\n", WSAGetLastError());
         return -1; //-- Err
      }

      len = rc;
      for(i = 0; i < len; i++)
      {
         rc = slipm_rx(&g_slipm, g_rx_buf[i]);
         if(rc < 0)
         {
            printf("Rx error: %d\n", rc);
         }
         else if(rc == 1)
         {
            printf("Rx OK. Len - %d bytes\n", g_slipm.addr_rx.len);

            cycle_num++;
            printf("Start cycle N %d\n", cycle_num);
         }
      }
      if(_kbhit())
         break;
   }

   return 0;
}

//----------------------------------------------------------------------------
BOOL CtrlHandler(DWORD fdwCtrlType)
{
   if(fdwCtrlType == CTRL_CLOSE_EVENT)      // CTRL-CLOSE: confirm that the user wants to exit.
   {
      if(s_l)
      {
         shutdown(s_l, SD_BOTH);
         closesocket(s_l);
         s_l = 0;
      }
      WSACleanup();
      return TRUE;
   }

   return FALSE;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

