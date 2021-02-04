#ifndef _TDTP_ERR_H_
#define _TDTP_ERR_H_

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



#define   ERR_OK                         0
#define   ERR_CONTINUE             (-20000)
#define   ERR_TIMEOUT              (-20001)
#define   ERR_OUT_OF_MEMORY        (-20002)
#define   ERR_CREATE_EVENT         (-20003)
#define   ERR_WPARAM               (-20004)
#define   ERR_UNEXP                (-20005)
#define   ERR_ILUSE                (-20006)
#define   ERR_FOPEN                (-20007)
#define   ERR_FREAD                (-20008)
#define   ERR_FWRITE               (-20009)
#define   ERR_SEND_FAILED          (-20010)
#define   ERR_CRC                  (-20011)
#define   ERR_SERR                 (-20012)
#define   ERR_BADBLOCK             (-20013)
#define   ERR_UNEXP_BLK            (-20014)

#define   ERR_ACK_ILUSE            (-20070)
#define   ERR_BADFLEN              (-20071)
#define   ERR_ACK_FWRITE           (-20072)
#define   ERR_ACK_UNDEF            (-20073)
#define   ERR_ACK_OVERFLOW         (-20074)
#define   ERR_ACK_BADADDR          (-20075)
#define   ERR_ACK_UNKNOWN          (-20076)


#define   ERR_EVENT_FCLOSE         (-20100)

#define   ERR_RX_1                 (-20200)
#define   ERR_RX_2                 (-20201)
#define   ERR_RX_3                 (-20202)
#define   ERR_RX_4                 (-20203)


#endif


