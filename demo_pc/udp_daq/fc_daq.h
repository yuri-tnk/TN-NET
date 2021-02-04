#pragma once

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


#include "tdtp1.h"

#define RX_BUF_SIZE   4096
#define TX_BUF_SIZE   4096

class CDaq
{
   public:

      CDaq(void);
      ~CDaq(void);

      BOOL Start(void);
      void Stop(void);
      BOOL do_bind(int localPort);
      BOOL set_udp_remote_host(CString & remoteHost, int remotePort);

      SOCKET m_s;

      TDTPSRVFC * m_srv_fc;
      int m_flag_to_stop;

   protected:

      static DWORD WINAPI CmdThreadProc(LPVOID lpV);

      SOCKADDR_IN  m_remoteAddress;
      SOCKADDR_IN  m_localAddress;

      HANDLE m_CmdThread;

   //-- Data

      int m_errorCode;

      BOOL m_Connect;
      BOOL m_Bind;
      int * m_flag_to_stop_addr;
  //-----

};




