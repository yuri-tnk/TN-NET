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


#ifndef _SLIPM_H_
#define _SLIPM_H_

#define FEND   0xC0    //-- Frame End (but we use it to mark "frame start")
#define FESC   0xDB    //-- Frame Escape
#define TFEND  0xDC    //-- Transposed Frame End
#define TFESC  0xDD    //-- Transposed Frame Escape


#define  SLIPM_ERR_ESC_SEQ    (-1)    //-- Esc sequence error
#define  SLIPM_ERR_UNEXP      (-2)    //-- Never should be here
#define  SLIPM_ERR_BAD_CRC    (-3)
#define  SLIPM_ESC_SEQ        (-4)

#define  SLIPM_CRC_LEN          4

#define  SLIPM_ST_START         0
#define  SLIPM_ST_LEN1          1
#define  SLIPM_ST_LEN2          2
#define  SLIPM_ST_DATA          3
#define  SLIPM_ST_CRC           4


typedef void (*slipm_tx_func)(unsigned char val);

typedef struct _SLIPMINFO
{
      //-- Rx

   int rx_stage;
   int esc_stage;
   unsigned int curr_crc_rx;
   int rx_data_ind;
   int rx_crc_ind;
   unsigned char * rx_buf;

   union
   {
     unsigned char b[4];
     unsigned int l;
   }crc_rx;

   union
   {
     unsigned char b[2];
     unsigned short len;
   }addr_rx;

      //-- Tx

   unsigned int curr_crc_tx;
   slipm_tx_func tx_func;

}SLIPMINFO;


//--------------------

void slipm_tx(SLIPMINFO * si, unsigned char * buf, int len);
int slipm_rx(SLIPMINFO * si, unsigned char sym);

#endif

