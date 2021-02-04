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

#define  SRV_MAX_TX                   3
#define  SRV_MAX_RD_TIMEOUTS          5
#define  SRV_RD_MAX_BAD_BLK          20
#define  MAX_NUM_UNEXP_BLK            5

extern unsigned int g_nbytes_rx_tc;

//----------------------------------------------------------------------------
int srv_tc_resp_proc(TDTPSRVTC * psrv, unsigned char * rx_buf)
{
   TDTPPKTINFO tpi;
   int cmd;
   int rc;
   int ack_msg;

   rc = ERR_CONTINUE;

   cmd = *rx_buf;
   if(cmd == TDTP_CMD_ACK_WR)
   {
      rc = chk_pkt_ACK(&tpi, rx_buf, &ack_msg);
      if(rc == ERR_OK)
      {
         rc = ERR_CONTINUE;

         if(tpi.sid == psrv->tc_sid  &&  tpi.blk_num == psrv->tc_blk_num)
         {
            switch(ack_msg)
            {
               case TDTP_ACK_WRNEXT:

                  psrv->stat.ack_wrnext++; //--Statistics
                  rc = ERR_OK;
                  break;

               case TDTP_ACK_FCLOSE:

                  psrv->stat.ack_fclose++; //--Statistics
                  rc = ERR_OK;
                  break;

               case TDTP_ACK_ERR_ILUSE:

                  psrv->stat.ack_iluse++; //--Statistics
                  psrv->last_err = ERR_ACK_ILUSE;
                  rc = ERR_SERR;
                  break;

               case TDTP_ACK_ERR_FWRITE:

                  psrv->stat.ack_err_fwrite++; //--Statistics
                  psrv->last_err = ERR_ACK_FWRITE;
                  rc = ERR_SERR;
                  break;

               case TDTP_ACK_ERR_UNDEF:

                  psrv->stat.ack_err_undef++; //--Statistics
                  psrv->last_err = ERR_ACK_UNDEF;
                  rc = ERR_SERR;
                  break;

               case TDTP_ACK_ERR_OVERFLOW:

                  psrv->stat.ack_err_overflow++; //--Statistics
                  psrv->last_err = ERR_ACK_OVERFLOW;
                  rc = ERR_SERR;
                  break;

               case TDTP_ACK_ERR_BADADDR:

                  psrv->stat.ack_err_badaddr++; //--Statistics
                  psrv->last_err = ERR_ACK_BADADDR;
                  rc = ERR_SERR;
                  break;

               default: //-- Unknown ACK code - terminate session

                  psrv->stat.ack_err_unknown++; //--Statistics
                  psrv->last_err = ERR_ACK_UNKNOWN;
                  rc = ERR_SERR;
            }
         }
      }
      else
         psrv->stat.bad_ack++; //--Statistics
   }

   return rc;
}

//----------------------------------------------------------------------------
int srv_tc_exch(TDTPSRVTC * psrv, int tx_len)
{
   int i;
   int rc;
   int rx_len;

   for(i = 0; i < SRV_MAX_TX; i++)
   {
      rc = tdtp_srv_send(psrv->s_tx, psrv->tx_buf, tx_len, psrv->remoteAddr);
      if(rc != ERR_OK)
      {
         psrv->last_err = ERR_SEND_FAILED;
         return ERR_SERR;
      }
      psrv->stat.blk_send++; //--Statistics

      rc = tdtp_srv_recv(psrv->s_rx, psrv->rx_buf, &rx_len);
      if(rc == ERR_OK)
      {
         rc = srv_tc_resp_proc(psrv, psrv->rx_buf); //-- expected ACK_WR here
         if(rc == ERR_OK)
         {
//---------- This application only ------------------------------
            g_nbytes_rx_tc += psrv->tc_blk_size;
//---------------------------------------------------------------

            return ERR_OK;
         }
         else if(rc == ERR_SERR)
            return ERR_SERR;
         //-- All other errorcodes - continue
      }
      else if(rc == ERR_TIMEOUT)
      {
         psrv->stat.rx_timeouts++; //--Statistics
      }
      else //-- reception error
      {
         psrv->stat.recv_errors++; //--Statistics
      }
   }

  //-- if here - data exchange failed

   psrv->last_err = ERR_TIMEOUT;
   return ERR_SERR;
}

//----------------------------------------------------------------------------
int tdtp_srv_send_file(TDTPSRVTC * psrv, int * flag_to_stop_addr)
{
   int rc;
   int tx_len;

   psrv->last_err = ERR_OK;

   tx_len = psrv->tc_data_func(psrv, TDTP_OP_RDOPEN);
   if(tx_len < 0)
   {
      psrv->last_err = ERR_BADFLEN;
      return ERR_SERR;
   }

   psrv->tc_tsize  = tx_len;
   psrv->tc_nbytes = tx_len;

   psrv->tc_sid++;
   if(psrv->tc_tsize <= TDTP_MAX_DATA_SIZE)
   {
      psrv->tc_blk_num  = 0; //-- Single block transfer
      psrv->tc_blk_size = psrv->tc_tsize;
   }
   else
   {
      psrv->tc_blk_num = 1; //-- First block of the multi-block transfer
      psrv->tc_blk_size = TDTP_MAX_DATA_SIZE;
   }

   psrv->stat.tsize = psrv->tc_tsize; //--Statistics

   if(psrv->tc_tsize > 0)
   {
      rc = psrv->tc_data_func(psrv, TDTP_OP_READ);
      if(rc != ERR_OK)  //-- Error (EOF here - also  error)
      {
         psrv->tc_data_func(psrv, TDTP_OP_CLOSE);
         return ERR_SERR;
      }
   }

   tx_len = build_pkt_WR(psrv);

   rc = srv_tc_exch(psrv, tx_len);
   if(rc != ERR_OK)
   {
      psrv->tc_data_func(psrv, TDTP_OP_CLOSE);
      return ERR_SERR;
   }

   //--- body(if any)

   for(;;)
   {
      psrv->tc_nbytes -= psrv->tc_blk_size;
      if(psrv->tc_nbytes <= 0)
      {
         psrv->tc_data_func(psrv, TDTP_OP_CLOSE);
         return ERR_OK; //-- Successful session ending
      }

      psrv->tc_blk_num++;
      if(psrv->tc_nbytes < TDTP_MAX_DATA_SIZE)
         psrv->tc_blk_size = psrv->tc_nbytes;
      else
         psrv->tc_blk_size = TDTP_MAX_DATA_SIZE;

      psrv->stat.blk_num++; //--Statistics
//----------------
      if(*flag_to_stop_addr == TRUE) //-- User abort
      {
         psrv->tc_data_func(psrv, TDTP_OP_CLOSE);
         return ERR_OK;
      }
//-----------------
      rc = psrv->tc_data_func(psrv, TDTP_OP_READ);
      if(rc != ERR_OK)  //-- Error (EOF here - also  error)
      {
         psrv->tc_data_func(psrv, TDTP_OP_CLOSE);
         return ERR_SERR;
      }

      tx_len = build_pkt_WR_DATA(psrv);
      rc = srv_tc_exch(psrv, tx_len);
      if(rc != ERR_OK)
      {
         psrv->tc_data_func(psrv, TDTP_OP_CLOSE);
         return ERR_SERR;
      }
   }
   return ERR_SERR; //-- Never reached
}

//----------------------------------------------------------------------------
void dump_srv_tc_statistics(TDTPSRVTC * psrv)
{
   printf(" Server to client statistics \n");
   printf("-------------------------\n\n");
   printf("tsize: %d\n", psrv->stat.tsize);
   printf("blk_num: %d\n", psrv->stat.blk_num);
   printf("blk_send: %d\n", psrv->stat.blk_send);
   printf("rx_timeouts: %d\n", psrv->stat.rx_timeouts);
   printf("recv_errors: %d\n", psrv->stat.recv_errors);
   printf("ack_wrnext: %d\n", psrv->stat.ack_wrnext);
   printf("ack_fclose: %d\n", psrv->stat.ack_fclose);
   printf("ack_iluse: %d\n", psrv->stat.ack_iluse);
   printf("ack_err_fwrite: %d\n", psrv->stat.ack_err_fwrite);
   printf("ack_err_undef: %d\n", psrv->stat.ack_err_undef);
   printf("ack_err_overflow: %d\n", psrv->stat.ack_err_overflow);
   printf("ack_err_badaddr: %d\n", psrv->stat.ack_err_badaddr);
   printf("ack_err_unknown: %d\n", psrv->stat.ack_err_unknown);
   printf("bad_ack: %d\n", psrv->stat.bad_ack);
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
int srv_fc_send_ack(TDTPSRVFC * psrv, int ack_msg)
{
   TDTPPKTINFO tpi;

   tpi.sid      = psrv->fc_sid;
   tpi.blk_num  = psrv->fc_blk_num;
   tpi.addr_len = TDTP_CMD_ACK_RD; //-- Here as  tmp 'Cmd' storage

   build_pkt_ACK(&tpi, psrv->tx_buf, ack_msg);
   tdtp_srv_send(psrv->s_tx, psrv->tx_buf, TDTP_ACK_PACKET_LEN, psrv->remoteAddr);

   return ERR_OK;
}
//----------------------------------------------------------------------------
int srv_fc_err_to_ack(TDTPSRVFC * psrv, int errorcode)
{
   int rc = TDTP_ACK_ERR_UNDEF;

   switch(errorcode)
   {
      case ERR_OK:

         psrv->stat.ack_rdnext++; //--Statistics
         rc = TDTP_ACK_RDNEXT;
         break;

      case ERR_FWRITE:

         psrv->stat.ack_err_fwrite++; //--Statistics
         rc = TDTP_ACK_ERR_FWRITE;
         break;

      case ERR_EVENT_FCLOSE:

         psrv->stat.ack_event_fclose++; //--Statistics
         rc = TDTP_ACK_FCLOSE;
         break;
   }
   return rc;
}

//----------------------------------------------------------------------------
// Prepare addr/addr_len - before func calling
//----------------------------------------------------------------------------
int tdtp_srv_recv_file(TDTPSRVFC * psrv, int * flag_to_stop_addr)
{
   TDTPPKTINFO tpi;
   int rc;
   int tx_len;
   int rx_len;
   int cmd;

   psrv->last_err = ERR_OK;

   rc = psrv->fc_data_func(psrv, TDTP_OP_WROPEN);
   if(rc != ERR_OK)
   {
      psrv->last_err = ERR_FOPEN;
      return ERR_SERR;
   }

   psrv->fc_sid++;
   psrv->fc_timeouts = 0;
   psrv->fc_bad_blk  = 0;
   psrv->fc_blk_num  = 0;
   psrv->fc_unexp_blk = 0;

   tx_len = build_pkt_RD(psrv);
   rc = tdtp_srv_send(psrv->s_tx, psrv->tx_buf, tx_len, psrv->remoteAddr);
   psrv->stat.blk_send++; //--Statistics
   if(rc != ERR_OK)
   {
      psrv->fc_data_func(psrv, TDTP_OP_CLOSE);
      psrv->last_err = ERR_SEND_FAILED;
      return ERR_SERR;
   }

   //-- Receive a file(if any)
   for(;;)
   {
      rc = tdtp_srv_recv(psrv->s_rx, psrv->rx_buf, &rx_len);
      if(rc == ERR_OK) //-- OK
      {
         psrv->stat.blk_recv++; //--Statistics

         if(*flag_to_stop_addr == TRUE) //-- User abort
         {
            psrv->fc_data_func(psrv, TDTP_OP_CLOSE);
            return ERR_EVENT_FCLOSE;
         }

         psrv->fc_timeouts = 0; //-- Reset timeouts counter

         cmd = *psrv->rx_buf;
         if(cmd == TDTP_CMD_RD_DATA)
         {
            rc = chk_pkt_RD_DATA(&tpi, psrv->rx_buf, rx_len);
            if(rc == ERR_OK)
            {
               psrv->fc_bad_blk  = 0; //-- Reset bad block counter

               if(psrv->fc_sid == tpi.sid) //-- Same session
               {
                  if(tpi.blk_num == -1)   //-- it means that this block is NACK
                  {                       //-- and session should be terminated.

                     psrv->stat.nack_blk++; //--Statistics

                     psrv->fc_data_func(psrv, TDTP_OP_CLOSE);
                     psrv->last_err = tpi.tsize; //-- In this case a 'tsize' value is errorcode
                     return ERR_SERR;
                  }

                  if(psrv->fc_blk_num == tpi.blk_num)
                  {
                     //-- Client didn't got ACK or network packet delay/duplicating
                     psrv->stat.dup_blk++; //--Statistics
                     srv_fc_send_ack(psrv, TDTP_ACK_RDNEXT);
                  }
                  else if(tpi.blk_num == psrv->fc_blk_num + 1) //-- New block arrived
                  {
                     psrv->stat.expected_blk++; //--Statistics

                     psrv->fc_blk_num = tpi.blk_num;
                     if(tpi.blk_num == 1) //first in session blk_num
                     {
                        psrv->stat.tsize = tpi.tsize; //--Statistics

                        psrv->fc_tsize  = tpi.tsize;
                        psrv->fc_nbytes = tpi.tsize;
                     }

                     psrv->fc_blk_data = tpi.data_ptr;
                     psrv->fc_blk_size = tpi.data_len;

                     rc = psrv->fc_data_func(psrv, TDTP_OP_WRITE);
                     if(rc == ERR_OK)
                     {
                        psrv->fc_nbytes -= psrv->fc_blk_size;
                        if(psrv->fc_nbytes <= 0) //-- End of writting
                        {
                           psrv->fc_data_func(psrv, TDTP_OP_CLOSE);
                           rc = ERR_EVENT_FCLOSE;
                        }
                     }
                     if(rc == ERR_EVENT_FCLOSE) //-- No regular ACK for the last packet
                        return ERR_OK;
                     else
                        srv_fc_send_ack(psrv, srv_fc_err_to_ack(psrv, rc));
                     //if(rc == ERR_EVENT_FCLOSE)
                  }
               }
               else //-- Block from another session
               {
                  psrv->fc_unexp_blk++;
                  if(psrv->fc_unexp_blk > MAX_NUM_UNEXP_BLK)
                  {
                     psrv->fc_data_func(psrv, TDTP_OP_CLOSE);
                     psrv->last_err = ERR_UNEXP_BLK;
                     return ERR_SERR;
                  }
               }
            }
            else
            {
               psrv->stat.bad_blk++; //--Statistics
               psrv->fc_bad_blk++;
               if(psrv->fc_bad_blk > SRV_RD_MAX_BAD_BLK)
               {
                  psrv->stat.err_bad_blk++; //--Statistics

                  psrv->fc_data_func(psrv, TDTP_OP_CLOSE);
                  psrv->last_err = ERR_BADBLOCK;
                  break;
               }
               else
               {
                  srv_fc_send_ack(psrv, TDTP_ACK_RDNEXT);
               }
            }
         }
         else //-- Unknown/unsupported cmd
         {
            psrv->stat.unknown_blk++; //--Statistics
         }
      }
      else if(rc == ERR_TIMEOUT)
      {
         psrv->stat.timeouts++; //--Statistics
   //--------------------------------------------------------------
         if(*flag_to_stop_addr == TRUE) //-- User abort
         {
            psrv->fc_data_func(psrv, TDTP_OP_CLOSE);
            return ERR_EVENT_FCLOSE;
         }
   //--------------------------------------------------------------

         psrv->fc_timeouts++;
         if(psrv->fc_timeouts > SRV_MAX_RD_TIMEOUTS)
         {
            psrv->stat.err_timeouts++; //--Statistics

            psrv->fc_data_func(psrv, TDTP_OP_CLOSE);
            psrv->last_err = ERR_TIMEOUT;
            break;
         }

         psrv->stat.send_dup_ack++; //--Statistics

         srv_fc_send_ack(psrv, TDTP_ACK_RDNEXT);
      }
      else //-- Statistics
      {
         psrv->stat.err_recv++; //--Statistics

         if(*flag_to_stop_addr == TRUE) //-- User abort
         {
            psrv->fc_data_func(psrv, TDTP_OP_CLOSE);
            return ERR_EVENT_FCLOSE;
         }
      }
   }
   return ERR_SERR;
}

//----------------------------------------------------------------------------
void dump_srv_fc_statistics(TDTPSRVFC * psrv)
{
   printf(" Server from client statistics \n");
   printf("-------------------------\n\n");
   printf("tsize: %d\n", psrv->stat.tsize);
   printf("blk_wr: %d\n", psrv->stat.blk_wr);
   printf("ack_rdnext: %d\n", psrv->stat.ack_rdnext);
   printf("ack_err_fwrite: %d\n", psrv->stat.ack_err_fwrite);
   printf("ack_event_fclose: %d\n", psrv->stat.ack_event_fclose);
   printf("blk_send: %d\n", psrv->stat.blk_send);
   printf("blk_recv: %d\n", psrv->stat.blk_recv);
   printf("nack_blk: %d\n", psrv->stat.nack_blk);
   printf("dup_blk: %d\n", psrv->stat.dup_blk);
   printf("expected_blk: %d\n", psrv->stat.expected_blk);
   printf("bad_blk: %d\n", psrv->stat.bad_blk);
   printf("unknown_blk: %d\n", psrv->stat.unknown_blk);
   printf("timeouts: %d\n", psrv->stat.timeouts);
   printf("send_dup_ack: %d\n", psrv->stat.send_dup_ack);
   printf("err_timeouts: %d\n", psrv->stat.err_timeouts);
   printf("err_recv: %d\n", psrv->stat.err_recv);
   printf("err_bad_blk: %d\n", psrv->stat.err_bad_blk);
}
//-------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



