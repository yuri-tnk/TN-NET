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
#include "tdtp.h"

#pragma warning(disable: 4996)

//----------------------------------------------------------------------------

extern FILE * g_hFile_rw_fc;
extern FILE * g_hFile_rw_tc;

extern unsigned int g_nbytes_rx_fc;
extern unsigned int g_nbytes_rx_tc;

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
int srv_tc_prc_func_N1(TDTPSRVTC * psrv, int op_mode)
{
   int len;

   if(op_mode == TDTP_OP_RDOPEN)      //-- rc:  < 0 - err; >=0 - tsize
   {
      if(g_hFile_rw_tc == NULL)
      {
         psrv->last_err = ERR_ILUSE;
         return ERR_ILUSE;
      }
      else
      {
         g_nbytes_rx_tc = 0;

         fseek(g_hFile_rw_tc, 0, SEEK_END);
         len = ftell(g_hFile_rw_tc);
         fseek(g_hFile_rw_tc, 0, SEEK_SET);
         return len;
      }
   }
   else if(op_mode == TDTP_OP_READ)   //-- rc:  < 0 - err; 0 - EOF; >0 - actually read
   {
      len = fread(psrv->tc_blk_data, 1, psrv->tc_blk_size, g_hFile_rw_tc);
      if(len == psrv->tc_blk_size)
      {
        // g_nbytes_rx_tc += len;
         return ERR_OK;
      }
      else //-- We not expect EOF here
      {
         psrv->last_err = ERR_FREAD;
         return ERR_SERR;
      }
   }
   else if(op_mode == TDTP_OP_CLOSE)  //-- rc:  < 0 - err; 0 - OK
   {
      if(g_hFile_rw_tc == NULL)
      {
         psrv->last_err = ERR_ILUSE;
         return ERR_ILUSE;
      }
      else
      {
         fclose(g_hFile_rw_tc);
         g_hFile_rw_tc = NULL;
         return ERR_OK;
      }
   }
   psrv->last_err = ERR_WPARAM;
   return ERR_WPARAM;
}

//----------------------------------------------------------------------------
// Read/Write example -  read file from client
//----------------------------------------------------------------------------
int srv_fc_prc_func_N1(TDTPSRVFC * psrv, int op_mode)
{
   int rc;

   if(op_mode == TDTP_OP_WROPEN)
   {
      if(g_hFile_rw_fc == NULL)
         return ERR_FOPEN;

      g_nbytes_rx_fc = 0;

      return ERR_OK;
   }
   else if(op_mode == TDTP_OP_WRITE)
   {
      if(g_hFile_rw_fc)
      {
         rc = fwrite(psrv->fc_blk_data, 1, psrv->fc_blk_size, g_hFile_rw_fc);
         if(rc == psrv->fc_blk_size)
         {
            psrv->stat.blk_wr++; //--Statistics

            g_nbytes_rx_fc += rc;

            return ERR_OK;
         }
         else
            return ERR_FWRITE;
      }
      else
         return ERR_ILUSE;
   }
   else if(op_mode == TDTP_OP_CLOSE)
   {
      if(g_hFile_rw_fc)
      {
         fclose(g_hFile_rw_fc);
         g_hFile_rw_fc = NULL;
         return ERR_OK;
      }
   }
   return  ERR_ILUSE;
}

//----------------------------------------------------------------------------
// DAQ example -  receive single block data from client
//----------------------------------------------------------------------------
int srv_fc_daq_func_N1(TDTPSRVFC * psrv, int op_mode)
{
   if(op_mode == TDTP_OP_WROPEN)
   {
      return ERR_OK;
   }
   else if(op_mode == TDTP_OP_WRITE)
   {
      return ERR_OK;
   }
   else if(op_mode == TDTP_OP_CLOSE)
   {
      return ERR_OK;
   }
   return  ERR_ILUSE;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------






