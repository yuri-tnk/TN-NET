/*

Copyright © 2004,2009 Yuri Tiomkin
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


#include <string.h>

#include "../tn_net/cpu_lpc23xx/LPC236X.h"
#include "../tnkernel/tn.h"

#include "../tn_net/tn_net_cfg.h"
#include "../tn_net/tn_net_types.h"
#include "../tn_net/tn_net_pcb.h"
#include "../tn_net/tn_net_mem.h"
#include "../tn_net/tn_ip.h"
#include "../tn_net/tn_net.h"
#include "../tn_net/tn_mbuf.h"
#include "../tn_net/errno.h"
#include "../tn_net/tn_netif.h"
#include "../tn_net/tn_net_utils.h"
#include "../tn_net/bsd_socket.h"
#include "../tn_net/tn_socket.h"
#include "../tn_net/tn_socket_func.h"

#include "../tn_net/tn_net_func.h"
#include "../tn_net/dbg_func.h"

#include "types.h"
#include "tdtp_err.h"
#include "tdtp.h"

extern volatile int g_hFlashOpN1;
extern volatile int g_hFlashOpN2;
extern unsigned char g_ft_file_buf[];
extern unsigned char g_udp_tst_buf[];
extern DAQDATASIM g_daq_data;
extern volatile int g_hDAQ_N1;

//--- DAQ simulation

const char g_parameter1_name[] = "Ch1 frequency, Hz:";
const char g_parameter2_name[] = "Ch2 voltage, mV:";
const char g_parameter3_name[] = "Ch3 current, uA:";

//---  External functions prototypes

void daq_prepare_example_data(void);

//---  Local functions prototypes

static int cli_wr_data_prc_func_N1(TDTPCLI * cli, int op_mode);
static int cli_to_srv_data_prc_func_N1(TDTPCLI * cli, int op_mode);
static int cli_to_srv_daq_func_N1(TDTPCLI * cli, int op_mode);

//----------------------------------------------------------------------------
int tdtp_cli_send(void * s, unsigned char * tx_buf, int tx_len)
{
   s_sendto((int)s,
                   tx_buf,
                   tx_len, // name_len,
                   0,      //-- flags,
                   NULL,   //-- NULL - case s_connect() was before; //struct _sockaddr * to,
                   0);     //-- tolen
   return ERR_OK;
}

//----------------------------------------------------------------------------
int tdtp_cli_recv(void * s, unsigned char * rx_buf, int * rx_len)
{
   int rc;
   rc =  s_recvfrom((int)s,
                      rx_buf,
                      TNNET_HI_BUF_SIZE, //-- Real rx len will be always <= this value
                      0,                 //-- flags
                      NULL,
                      NULL);
   if(rc > 0)
   {
      *rx_len = rc;
      return ERR_OK;
   }
   else
   {
     if(rc == -ETIMEOUT)
        return ERR_TIMEOUT;
   }
   return ERR_ILUSE;
}

//----------------------------------------------------------------------------
//  This is a place, where according to the received TDTP 'address' field
//  the actions handler is selected
//----------------------------------------------------------------------------
void cli_wr_proc_addr(TDTPCLI * cli, TDTPPKTINFO * pi)
{
   //-- Set 'data_prc_func' according to the address value
   //-- Use appropriate 'cli' structure fields to store addr, if it is necessary

      // For this example only

   if(memcmp(pi->addr_ptr, "some_flash_data_N2", pi->addr_len) == 0)
      cli->wr_prc_func = cli_wr_data_prc_func_N1;
   else
      cli->wr_prc_func = NULL;

}

//----------------------------------------------------------------------------
//  This is a place, where according to the received TDTP 'address' field
//  the actions handler is selected
//----------------------------------------------------------------------------
void cli_rd_proc_addr(TDTPCLI * cli, TDTPPKTINFO * pi)
{
   //-- Set 'data_prc_func' according to the address value

   if(memcmp(pi->addr_ptr, "some_flash_data_N1", pi->addr_len) == 0)
      cli->rd_prc_func = cli_to_srv_data_prc_func_N1;
   else if(memcmp(pi->addr_ptr, "daq_mode_N1", pi->addr_len) == 0)
      cli->rd_prc_func = cli_to_srv_daq_func_N1;
   else
      cli->rd_prc_func = NULL;

  //-- use 'rcli' fields to store addr, if it is necessary

}


//----------------------------------------------------------------------------
// This function is valid only for current example (TFTP WR address is "some_flash_data_N2"
//----------------------------------------------------------------------------
static int cli_wr_data_prc_func_N1(TDTPCLI * cli, int op_mode)
{
   //int rc;
   //char * ptr;

   if(op_mode == TDTP_OP_WROPEN)
   {
      //--
      if(g_hFlashOpN1 == 0)
      {
         // ptr = &g_FileNameBuf;
         // memcpy(ptr, cli->wr_addr_ptr, cli->wr_addr_len);
         // ptr[cli->wr_addr_len] = 0;
         // g_hCliWrFile = fopen(ptr, "wb+");
         // if(g_hCliWrFile == NULL)
         //   rc = ERR_FOPEN;
         // else
         //   rc = ERR_OK;
         // return rc;

         //-- Do preparing for writting here

         g_hFlashOpN1 = 1;
         return ERR_OK;
      }
      else
         return ERR_ILUSE;
   }
   else if(op_mode == TDTP_OP_WRITE)
   {
      if(g_hFlashOpN1)
      {
       //  rc = fwrite(cli->wr_blk_data, 1, cli->wr_blk_size, g_hFlashOpN1);
       //  if(rc == cli->wr_blk_size)
       //     return ERR_OK;
       //  else
       //     return ERR_FWRITE;

         //-- Do your writting here

          return ERR_OK;
      }
      else
         return ERR_ILUSE;
   }
   else if(op_mode == TDTP_OP_CLOSE)
   {
      if(g_hFlashOpN1)
      {
         //fclose();
         g_hFlashOpN1 = 0;
         return ERR_OK;
      }
   }
   return  ERR_ILUSE;
}

//----------------------------------------------------------------------------
static int cli_to_srv_data_prc_func_N1(TDTPCLI * cli, int op_mode)
{
   int rc;
   unsigned char * ptr;
   int name_len;
   int i;
   static int cnt = 0;

   if(op_mode == TDTP_OP_RDOPEN)
   {
      if(g_hFlashOpN2 == 0)
      {
//--------- This is the example only data preparation to send

         ptr = &g_ft_file_buf[0];
         *ptr = 0;
         name_len = 0;
         for(i=0; i<45; i++) //-- 44*11(lines 1..44) + 28(line 0) = 512)
         {
            if(i == 0)
            {
               strcpy((char*)g_udp_tst_buf,"---- Line: 000000000 -----\r\n");
            }
            else
            {
               tn_snprintf((char*)g_udp_tst_buf, 15, "%09u\r\n", i);
            }
            rc = strlen((char*)g_udp_tst_buf);
            strcat((char*)ptr, (char*)g_udp_tst_buf);
            ptr += rc;
            name_len += rc;
         }
//-----------------------------------------------------------------------
         cnt = 0;
         g_hFlashOpN2 = TRUE;
         return name_len * 8192;
      }
      else
         return ERR_ILUSE;
   }
   else if(op_mode == TDTP_OP_READ)
   {
      if(g_hFlashOpN2)
      {
//--------- This is the example only data preparation to send
         cnt++;
         tn_snprintf((char*)g_udp_tst_buf, 15, "%09u", cnt);
         memcpy(g_ft_file_buf + 11, g_udp_tst_buf, 9);

         cli->rd_blk_data = &g_ft_file_buf[0];
//-----------------------------------------------------------------------
         return ERR_OK;
      }
      else
         return ERR_ILUSE;
   }
   else if(op_mode == TDTP_OP_CLOSE)
   {
      if(g_hFlashOpN2)
      {
         //fclose(g_hCliRdFile);
         g_hFlashOpN2 = 0;
         return ERR_OK;
      }
   }
   return  ERR_ILUSE;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
static int cli_to_srv_daq_func_N1(TDTPCLI * cli, int op_mode)
{
   char buf[16];

   static int cnt = 0;

   if(op_mode == TDTP_OP_RDOPEN)
   {
      if(g_hDAQ_N1 == 0)
      {
//--------- This is the example only data preparation to send

         daq_prepare_example_data();

//-----------------------------------------------------------------------
         cnt = 0;
         g_hDAQ_N1 = TRUE;
         return sizeof(DAQDATASIM);
      }
      else
         return ERR_ILUSE;
   }
   else if(op_mode == TDTP_OP_READ)
   {
      if(g_hDAQ_N1)
      {
//--------- This is the example only data preparation to send
         cnt++;
         tn_snprintf(buf, 15, "%09u", cnt);

         strcpy(g_daq_data.parameter1_value, buf);
         strcpy(g_daq_data.parameter2_value, buf);
         strcpy(g_daq_data.parameter3_value, buf);

         cli->rd_blk_data = (unsigned char*)&g_daq_data;
//-----------------------------------------------------------------------
         return ERR_OK;
      }
      else
         return ERR_ILUSE;
   }
   else if(op_mode == TDTP_OP_CLOSE)
   {
      if(g_hDAQ_N1)
      {
         //fclose(g_hCliRdFile);
         g_hDAQ_N1 = 0;
         return ERR_OK;
      }
   }
   return  ERR_ILUSE;
}

//----------------------------------------------------------------------------
void daq_prepare_example_data(void)
{
   int i;

   memset(&g_daq_data, 0x20, sizeof(DAQDATASIM));

   i = strlen(g_parameter1_name);        //-- "Ch1 frequency, Hz:";
   memcpy(g_daq_data.parameter1_name, g_parameter1_name, i);
   i = strlen(g_parameter2_name);        //-- "Ch2 voltage, V:"
   memcpy(g_daq_data.parameter2_name, g_parameter2_name, i);
   i = strlen(g_parameter3_name);        //-- "Ch3 current, uA:"
   memcpy(g_daq_data.parameter3_name, g_parameter3_name, i);

}

//----------------------------------------------------------------------------
//   This function is not a part of the TDTP.
//
//  The function just uses a TDTP data structure and a few TDTP routines
//
//----------------------------------------------------------------------------
void daq_cli(TDTPCLI * cli)
{
   TDTPPKTINFO tpi;
   int rx_len;
   int cmd;
   int rc;
   static int cnt =0;
   char buf[16];

   rc = tdtp_cli_recv(cli->s_rx, cli->rx_buf, &rx_len);
   if(rc == ERR_OK) //-- OK
   {
      cli->rd_timeouts = 0; //-- Reset timeout counter

      cmd = *cli->rx_buf;
      if(cmd == TDTP_CMD_RD)
      {
         rc = chk_pkt_RD(&tpi, cli->rx_buf);
         if(rc == ERR_OK)
         {
            cli->rd_sid     = tpi.sid;
            cli->rd_blk_num = 0;

         //-- We send an answer here - DAQ simulation ------

            cnt++;
            tn_snprintf(buf, 15, "%09u", cnt);

            strcpy(g_daq_data.parameter1_value, buf);
            strcpy(g_daq_data.parameter2_value, buf);
            strcpy(g_daq_data.parameter3_value, buf);

            cli->rd_blk_data = (unsigned char*)&g_daq_data;
            cli->rd_blk_size = sizeof(DAQDATASIM);

         //-------------------------------------------------

            cli->tx_len = build_pkt_RD_DATA(cli);
            rc = tdtp_cli_send(cli->s_tx, cli->tx_buf, cli->tx_len);
            // if(rc != ERR_OK) {} -  Add if you need it
         }
      }
   }
   else if(rc == ERR_TIMEOUT)
   {
      //-- Do nothing here
   }
   else //-- Statistics
   {
   }
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



