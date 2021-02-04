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

#include "stdafx.h"

#include "tdtp_err.h"
#include "tdtp1.h"

//----------------------------------------------------------------------------
int tdtp_srv_send(void * s, unsigned char * tx_buf, int tx_len, void * remoteAddr)
{
   int rc;
   rc = sendto((SOCKET)s,
               (char*)tx_buf,
                tx_len, // len,
                0,
               (SOCKADDR*)remoteAddr,  //(SOCKADDR*) &up->m_remoteAddress,
                sizeof(SOCKADDR)); // sizeof(up->m_remoteAddress));

   if(rc == SOCKET_ERROR)
   {
      CString str;
      str.Format("%d", WSAGetLastError());
      return ERR_SERR;
   }
   return ERR_OK;
}

//----------------------------------------------------------------------------
int tdtp_srv_recv(void * s, unsigned char * rx_buf, int * rx_len, void * remoteAddr)
{
   int rc;
   int remAddressLen;
   struct timeval Timeout;
   fd_set readfds;

   readfds.fd_count    = 1;
   readfds.fd_array[0] = (SOCKET)s;
   Timeout.tv_sec  = 0;
   Timeout.tv_usec = 100000; //-- 100 ms

   rc = select(1, &readfds, NULL, NULL, &Timeout);
   if(rc == SOCKET_ERROR)
   {
      AfxMessageBox("select() error!");
   }
   else if(rc == 0) //-- Timeout
   {
      return ERR_TIMEOUT;
   }
   else
   {
      remAddressLen = sizeof(SOCKADDR);
      rc = recvfrom((SOCKET)s,
                    (char*)rx_buf,
                    1536, // bufSize,
                    0,    // flags
                    (SOCKADDR*)remoteAddr,
                    &remAddressLen);
      if(rc == SOCKET_ERROR)
      {
         CString s;
         s.Format("%d", WSAGetLastError());
         AfxMessageBox(s);
         return ERR_UNEXP;
      }
      else
      {
         *rx_len = rc;
         return ERR_OK;
      }
   }

   return ERR_UNEXP;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



