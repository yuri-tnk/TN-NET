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

#include "StdAfx.h"
#include <Mmsystem.h>

#include "tdtp_err.h"
#include "tdtp_fc_daq.h"

#pragma warning (disable: 4996)

extern HWND g_hMsgWnd;

extern unsigned int g_packets_rx_daq;
extern unsigned int g_packets_daq_repeats;
extern unsigned int g_packets_daq_failed;

int daq_fc(TDTPSRVFC * psrv, int tx_len);

//----------------------------------------------------------------------------
CTDTP_fc_srv_daq::CTDTP_fc_srv_daq(void)
{
   m_s = INVALID_SOCKET;

   m_Connect  = FALSE;
   m_Bind     = FALSE;

   m_srv_fc = (TDTPSRVFC *)malloc(sizeof(TDTPSRVFC));
   memset(m_srv_fc, 0, sizeof(TDTPSRVFC));

   m_srv_fc->rx_buf = (unsigned char *) malloc(RX_BUF_SIZE);
   m_srv_fc->tx_buf = (unsigned char *) malloc(TX_BUF_SIZE);

   m_srv_fc->remoteAddr = (void*)&m_remoteAddress;

   m_flag_to_stop = TRUE;
   m_cli_addr = NULL;
}

//----------------------------------------------------------------------------
CTDTP_fc_srv_daq::~CTDTP_fc_srv_daq(void)
{
   free(m_srv_fc->rx_buf);
   free(m_srv_fc->tx_buf);
   free(m_srv_fc);

   if(m_cli_addr)
      free(m_cli_addr);
}

//----------------------------------------------------------------------------
BOOL CTDTP_fc_srv_daq::set_udp_remote_host(CString & remoteHost, int remotePort)
{
   DWORD hostByIP;
   HOSTENT *hostByName;

   if(m_s == INVALID_SOCKET)
      return FALSE;

   memset(&m_remoteAddress, 0, sizeof(SOCKADDR_IN));
   m_remoteAddress.sin_family = AF_INET;
   m_remoteAddress.sin_port   = htons(remotePort);

   hostByIP = inet_addr(remoteHost);

   if(hostByIP == INADDR_NONE)
   {
      hostByName = gethostbyname(remoteHost);
      if(hostByName == NULL)
      {
         m_errorCode = WSAGetLastError();
         return FALSE;
      }
      m_remoteAddress.sin_addr = *((IN_ADDR*)hostByName->h_addr_list[0]);
   }
   else
      m_remoteAddress.sin_addr.s_addr = hostByIP;

  // if(connect(m_s, (SOCKADDR*) &m_remoteAddress, sizeof(SOCKADDR_IN)) != 0)
  // {
  //    m_errorCode = WSAGetLastError();
  //    return FALSE;
  // }

   m_Connect = TRUE;
   return TRUE;
}

//----------------------------------------------------------------------------
BOOL CTDTP_fc_srv_daq::do_bind(int localPort)
{
   if(m_s == INVALID_SOCKET)
      return FALSE;

   memset(&m_localAddress, 0, sizeof(SOCKADDR_IN));
   m_localAddress.sin_family = AF_INET;
   m_localAddress.sin_port = htons(localPort);
   m_localAddress.sin_addr.s_addr = htonl(INADDR_ANY);

   if(bind(m_s, (SOCKADDR*) &m_localAddress, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
   {
      m_errorCode = WSAGetLastError();
      return FALSE;
   }
   m_Bind = TRUE;
   return TRUE;
}

//----------------------------------------------------------------------------
void CTDTP_fc_srv_daq::Stop(void)
{
   if(m_flag_to_stop == TRUE)
      return;

   m_flag_to_stop = TRUE; //-- Signal to terminate thread

   ::WaitForSingleObject(m_CmdThread, INFINITE);
   ::CloseHandle(m_CmdThread);

   shutdown(m_s, SD_BOTH);
   closesocket(m_s);
   m_s = INVALID_SOCKET;

   m_Connect = FALSE;
   m_Bind = FALSE;
   m_errorCode = 0;
   m_flag_to_stop_addr = NULL;
   if(m_cli_addr)
   {
      free(m_cli_addr);
      m_cli_addr = NULL;
   }
}

//----------------------------------------------------------------------------
BOOL CTDTP_fc_srv_daq::Start(srv_fc_data_func srv_func,
                         unsigned char * cli_addr,
                         int cli_addr_len)
{
   DWORD dwThreadId;

   if(m_Connect == FALSE || m_Bind == FALSE ||
            cli_addr == NULL || cli_addr_len <= 0)
      return FALSE;

   m_cli_addr = (unsigned char*)malloc(cli_addr_len);
   m_cli_addr_len = cli_addr_len;
   memcpy(m_cli_addr, cli_addr, m_cli_addr_len);

   m_srv_fc->fc_data_func  = srv_func;

   m_flag_to_stop = FALSE;

   m_CmdThread = ::CreateThread(NULL,           //-- lpThreadAttributes
                                0,              //-- dwStackSize,
                                (LPTHREAD_START_ROUTINE) CmdThreadProc,
                                (LPVOID)this,   //-- lpParameter,
                                0,              //-- dwCreationFlags,
                                &dwThreadId);   //-- lpThreadId
   if(m_CmdThread == NULL)
   {
      m_flag_to_stop = TRUE;
      closesocket(m_s);
      return FALSE;
   }

   if(!::SetThreadPriority(m_CmdThread,THREAD_PRIORITY_HIGHEST))
                                                 // THREAD_PRIORITY_TIME_CRITICAL))
   {
      m_flag_to_stop = TRUE;
      ::WaitForSingleObject(m_CmdThread, INFINITE);
      ::CloseHandle(m_CmdThread);
      closesocket(m_s);
      return FALSE;
   }

   return TRUE;
}

//----------------------------------------------------------------------------
DWORD WINAPI CTDTP_fc_srv_daq::CmdThreadProc(LPVOID lpV)
{
   int rc;
   int curr_sid;
   int tx_len;

   CTDTP_fc_srv_daq * fc_srv = (CTDTP_fc_srv_daq *)lpV;

   fc_srv->m_srv_fc->s_tx = (void*)fc_srv->m_s;
   fc_srv->m_srv_fc->s_rx = (void*)fc_srv->m_s;

   fc_srv->m_srv_fc->fc_addr_ptr = fc_srv->m_cli_addr;
   fc_srv->m_srv_fc->fc_addr_len = fc_srv->m_cli_addr_len;

   unsigned int tval;
   tval = (unsigned)time(NULL);
   srand(tval);
   curr_sid = rand();
   fc_srv->m_srv_fc->fc_sid = curr_sid;

   g_packets_rx_daq      = 0;
   g_packets_daq_repeats = 0;
   g_packets_daq_failed  = 0;

   timeBeginPeriod(1);

   for(;;)
   {
   //   ::Sleep(1);

      if(fc_srv->m_flag_to_stop == TRUE) //-- User abort
         return ERR_EVENT_FCLOSE;

      fc_srv->m_srv_fc->fc_sid++;

      tx_len = build_pkt_RD(fc_srv->m_srv_fc);
      rc = daq_fc(fc_srv->m_srv_fc, tx_len);
      if(rc == ERR_OK) //-- OK
      {
//------- Processing payload here ----------------------

         g_packets_rx_daq++; //-- This application only

//------------------------------------------------------
      }
      else if(rc == ERR_SERR)
      {
/*
         if(fc_srv->m_srv_fc->last_err == ERR_TIMEOUT)
         {

            CString s;
            s = _T("Communication failed - timeout.");
            AfxMessageBox(s);

            ::PostMessage(g_hMsgWnd, CM_THREAD_EXIT, 0, 2);

            while(fc_srv->m_flag_to_stop == FALSE)
            {
               ::Sleep(20);
            }
            break;

         }
         else
         {
            // Add your error handler here
         }
*/
      }
   }

   timeEndPeriod(1);
   return 0;
}

//----------------------------------------------------------------------------
// This function is not a part of TDFP protocol
// It just uses TDFP protocol data structures and a few routines
//----------------------------------------------------------------------------
int daq_fc(TDTPSRVFC * psrv, int tx_len)
{
   TDTPPKTINFO tpi;
   int i;
   int rc;
   int rx_len;
   unsigned char cmd;

#define MAX_TX 5

   for(i = 0; i < MAX_TX; i++)
   {
      rc = tdtp_srv_send(psrv->s_tx, psrv->tx_buf, tx_len, psrv->remoteAddr);
      if(rc != ERR_OK)
         return ERR_SERR;

      rc = tdtp_srv_recv(psrv->s_rx, psrv->rx_buf, &rx_len);
      if(rc == ERR_OK)
      {
         cmd = *psrv->rx_buf;
         if(cmd == TDTP_CMD_RD_DATA)
         {
            rc = chk_pkt_RD_DATA(&tpi, psrv->rx_buf, rx_len);
            if(rc == ERR_OK)
            {
               psrv->fc_bad_blk  = 0; //-- Reset bad block counter

               if(psrv->fc_sid == tpi.sid) //-- Same session - block is valid
               {
                  if(i > 0) //-- Was repeat
                     g_packets_daq_repeats++; //-- This application only

                  return ERR_OK;
               }
               else
               {
                  g_packets_daq_failed++; //-- This application only
                  psrv->last_err = ERR_RX_4;
                  return ERR_SERR;
               }
            }
            else
            {
               g_packets_daq_failed++; //-- This application only
               psrv->last_err = ERR_RX_3;
               return ERR_SERR;
            }
         }
         else
         {
            psrv->last_err = ERR_RX_2;
            g_packets_daq_failed++; //-- This application only
            return ERR_SERR;
         }
      }
      else if(rc == ERR_TIMEOUT)
      {
         psrv->last_err = ERR_TIMEOUT;
      }
      else //-- reception error
      {
         psrv->last_err = ERR_RX_1;
         break;
      }
   }
  //-- if here - data exchange failed
   g_packets_daq_failed++; //-- This application only
   return ERR_SERR;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
