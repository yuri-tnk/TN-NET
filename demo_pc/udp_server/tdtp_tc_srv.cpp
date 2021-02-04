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

#include "tdtp_err.h"
#include "tdtp_tc_srv.h"

unsigned int tdtp_crc32(unsigned char * buf, int nbytes);
extern HWND g_hMsgWnd;
//----------------------------------------------------------------------------
CTDTP_tc_srv::CTDTP_tc_srv(void)
{
   m_s = INVALID_SOCKET;

   m_Connect  = FALSE;
   m_Bind     = FALSE;

   m_srv_tc = (TDTPSRVTC *)malloc(sizeof(TDTPSRVTC));
   memset(m_srv_tc, 0, sizeof(TDTPSRVTC));

   m_srv_tc->tc_blk_data = (unsigned char *) malloc(RX_BUF_SIZE);
   m_srv_tc->rx_buf      = (unsigned char *) malloc(RX_BUF_SIZE);
   m_srv_tc->tx_buf      = (unsigned char *) malloc(TX_BUF_SIZE);

   m_srv_tc->remoteAddr  = (void*)&m_remoteAddress;

   m_flag_to_stop = TRUE;
   m_cli_addr = NULL;
}

//----------------------------------------------------------------------------
CTDTP_tc_srv::~CTDTP_tc_srv(void)
{
   free(m_srv_tc->tc_blk_data);
   free(m_srv_tc->rx_buf);
   free(m_srv_tc->tx_buf);
   free(m_srv_tc);
   if(m_cli_addr)
      free(m_cli_addr);
}

//----------------------------------------------------------------------------
BOOL CTDTP_tc_srv::set_udp_remote_host(CString & remoteHost, int remotePort)
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
BOOL CTDTP_tc_srv::do_bind(int localPort)
{
   if(m_s == INVALID_SOCKET)
      return FALSE;

   memset(&m_localAddress, 0, sizeof(SOCKADDR_IN));
   m_localAddress.sin_family      = AF_INET;
   m_localAddress.sin_port        = htons(localPort);
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
void CTDTP_tc_srv::Stop(void)
{
   if(m_flag_to_stop == TRUE)
      return;

   m_flag_to_stop = TRUE; //-- Signal to terminate thread

   ::WaitForSingleObject(m_CmdThread, INFINITE);
   ::CloseHandle(m_CmdThread);

   shutdown(m_s, SD_BOTH);
   closesocket(m_s);
   m_s = INVALID_SOCKET;

   m_Connect   = FALSE;
   m_Bind      = FALSE;
   m_errorCode = 0;
   m_flag_to_stop_addr = NULL;

   if(m_cli_addr)
   {
      free(m_cli_addr);
      m_cli_addr = NULL;
   }
}

//----------------------------------------------------------------------------
BOOL CTDTP_tc_srv::Start(srv_tc_data_func srv_func,
                         unsigned char * cli_addr,
                         int cli_addr_len)
{
   DWORD dwThreadId;

   if(m_Connect == FALSE || m_Bind == FALSE)
      return FALSE;

   m_srv_tc->tc_data_func  = srv_func;

   m_cli_addr     = (unsigned char*)malloc(cli_addr_len);
   m_cli_addr_len = cli_addr_len;
   memcpy(m_cli_addr, cli_addr, m_cli_addr_len);

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
DWORD WINAPI CTDTP_tc_srv::CmdThreadProc(LPVOID lpV)
{
   int rc;
   CTDTP_tc_srv * tc_srv  = (CTDTP_tc_srv *)lpV;

   tc_srv->m_srv_tc->s_tx = (void*)tc_srv->m_s;
   tc_srv->m_srv_tc->s_rx = (void*)tc_srv->m_s;

   tc_srv->m_srv_tc->tc_addr_ptr = tc_srv->m_cli_addr;
   tc_srv->m_srv_tc->tc_addr_len = tc_srv->m_cli_addr_len;

   unsigned int tval;
   tval = (unsigned)time(NULL);
   srand(tval);
   tc_srv->m_srv_tc->tc_sid = rand();

   rc = tdtp_srv_send_file(tc_srv->m_srv_tc, &tc_srv->m_flag_to_stop);
   if(tc_srv->m_flag_to_stop == FALSE)
   {
      if(rc == ERR_SERR)
      {
         CString s;
         if(tc_srv->m_srv_tc->last_err == ERR_TIMEOUT)
            s = _T("Communication failed - timeout.");
         else
            s.Format("Communication failed - Error %d",tc_srv->m_srv_tc->last_err);
         AfxMessageBox(s);
      }
      else if(rc == ERR_OK)
      {
         AfxMessageBox("Transfer finished successfully.", MB_ICONINFORMATION);
      }
      ::PostMessage(g_hMsgWnd, CM_THREAD_EXIT, 0, 1);

      while(tc_srv->m_flag_to_stop == FALSE)
      {
         ::Sleep(20);
      }
   }
   return 0;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



