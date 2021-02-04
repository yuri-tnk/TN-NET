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
#include "tdtp_err.h"
#include "tdtp.h"

#define CLI_MAX_RD_TIMEOUTS   5 //10
#define CLI_MAX_UNEXP_BLOCKS  3 //10

volatile  int g_tst1 = 0;
volatile  int g_tst2 = 0;

//----------------------------------------------------------------------------
int cli_wr_err_to_ack(int errcode)
{
   int rc;
   switch(errcode)
   {
      case ERR_OK:
      case ERR_FOPEN:
         rc = TDTP_ACK_WRNEXT;
         break;
      case ERR_EVENT_FCLOSE:
         rc = TDTP_ACK_FCLOSE;
         break;
      case ERR_ILUSE:
         rc = TDTP_ACK_ERR_ILUSE;
         break;
      case ERR_FWRITE:
         rc = TDTP_ACK_ERR_FWRITE;
         break;
       default:
          rc = TDTP_ACK_ERR_UNDEF;
    }
    return rc;
}

//----------------------------------------------------------------------------
int cli_do_write(TDTPCLI * cli)
{
   int rc;

   rc = cli->wr_prc_func(cli, TDTP_OP_WRITE);
   if(rc == ERR_OK)
   {
      cli->wr_nbytes -= cli->wr_blk_size;
      if(cli->wr_nbytes <= 0) //-- End of writting
      {
         rc = cli->wr_prc_func(cli, TDTP_OP_CLOSE);
         if(rc == ERR_OK && cli->wr_nbytes == 0) //-- Transfer is finished successfully.
         {
            rc = ERR_EVENT_FCLOSE;
            //-- Final actions
            //-- Statistics
         }
      }
   }
   return rc;
}

//----------------------------------------------------------------------------
void cli_wr_send_ack(TDTPCLI * cli, int ack_msg)
{
   TDTPPKTINFO tpi;

   tpi.sid      = cli->wr_sid;
   tpi.blk_num  = cli->wr_blk_num;
   tpi.addr_len = TDTP_CMD_ACK_WR; //-- Here as  tmp 'Cmd' storage

   build_pkt_ACK(&tpi, cli->tx_buf, ack_msg);
   tdtp_cli_send(cli->s_tx, cli->tx_buf, TDTP_ACK_PACKET_LEN);
}

//----------------------------------------------------------------------------
void cli_do_read(TDTPCLI * cli)
{
   int rc;

   cli->rd_blk_num++;

   if(cli->rd_nbytes < TDTP_MAX_DATA_SIZE)
      cli->rd_blk_size = cli->rd_nbytes;
   else
      cli->rd_blk_size = TDTP_MAX_DATA_SIZE;

   rc = cli->rd_prc_func(cli, TDTP_OP_READ); //-- open, set active,Check addr - inside
   if(rc == ERR_OK)
   {
      cli->rd_nbytes -= cli->rd_blk_size;

      if(cli->rd_nbytes <= 0)
      {
         rc = cli->rd_prc_func(cli, TDTP_OP_CLOSE);
         cli->rd_prc_func = NULL;

         if(rc == ERR_OK && cli->rd_nbytes == 0) //-- Transfer is finished successfully.
         {
            //-- Final actions
            //-- Statistics
         }
      }
   }
   else //-- Failed to read - close session
   {
      cli->rd_prc_func = NULL;
      cli->rd_blk_num  = -1;
      cli->rd_tsize    = TDTP_ACK_ERR_FREAD; //-- tsize here as NACK storage
   }
}

//----------------------------------------------------------------------------
void tdtp_cli(TDTPCLI * cli)
{
   TDTPPKTINFO tpi;
   int rx_len;
   int cmd;
   int rc;
   int ack_msg;

   rc = tdtp_cli_recv(cli->s_rx, cli->rx_buf, &rx_len);
   if(rc == ERR_OK) //-- OK
   {
      cli->rd_timeouts = 0; //-- Reset timeout counter

      cmd = *cli->rx_buf;
      if(cmd == TDTP_CMD_WR)
      {
         rc = chk_pkt_WR(&tpi, cli->rx_buf, rx_len);  //-- client side routine
         if(rc == ERR_OK)
         {
            if(tpi.blk_num == 0) //-- Single-block session expected
            {
               if(tpi.data_len != tpi.tsize)
                  return;
            }

            if(cli->wr_prc_func) //-- Session running
            {
               if(cli->wr_sid == tpi.sid) //-- Same session
                  return;

               cli->wr_prc_func(cli, TDTP_OP_CLOSE); //-- Stop WR session(if any)
               cli->wr_prc_func = NULL;
            }

            //-- Processing address

            cli_wr_proc_addr(cli, &tpi);  //-- Set 'data_prc_func' according to the address value
            if(cli->wr_prc_func) //-- The address is known and valid
            {
              //-- Start new session

               cli->wr_sid     = tpi.sid;
               cli->wr_blk_num = tpi.blk_num;
               cli->wr_tsize   = tpi.tsize;
               cli->wr_nbytes  = tpi.tsize;

               cli->wr_addr_ptr = tpi.addr_ptr; //-- for open()
               cli->wr_addr_len = tpi.addr_len;

               rc = cli->wr_prc_func(cli, TDTP_OP_WROPEN); //-- open, set active,Check addr - inside
               if(rc == ERR_OK)
               {
                  cli->wr_blk_data = tpi.data_ptr;
                  cli->wr_blk_size = tpi.data_len;
                  if(cli->wr_tsize > 0)
                  {
                     rc = cli_do_write(cli);
                  }
                  else
                  {
                     rc = cli->wr_prc_func(cli, TDTP_OP_CLOSE);
                     cli->wr_prc_func = NULL;   //-- Cli Session ending
                     if(rc == ERR_OK) //-- Transfer is finished successfully.
                     {
                        //-- Final actions
                        //-- Statistics
                     }
                  }

                  cli_wr_send_ack(cli, cli_wr_err_to_ack(rc));
               }
               else  //-- Terminate session activity
                  cli->wr_prc_func = NULL;
            }
            else //-- Invalid/unknown addr - for this error we send ACK as NACK
            {
               cli_wr_send_ack(cli, TDTP_ACK_ERR_BADADDR);
            }
         }
      }
      else if(cmd == TDTP_CMD_WR_DATA)
      {
         rc = chk_pkt_WR_DATA(&tpi, cli->rx_buf, rx_len);
         if(rc == ERR_OK)
         {
            if(cli->wr_sid == tpi.sid) //-- Same session
            {
               if(cli->wr_blk_num == tpi.blk_num)
               {
                  //-- Server didn't got ACK ('wr_active' - don't care here) or
                  //-- network packet delay/duplicating

                  cli_wr_send_ack(cli, TDTP_ACK_WRNEXT);
               }
               else if(tpi.blk_num == cli->wr_blk_num + 1) //-- New block arrived
               {
                  if(cli->wr_prc_func) //-- Session running
                  {
                     cli->wr_blk_num = tpi.blk_num;

                     cli->wr_blk_data = tpi.data_ptr;
                     cli->wr_blk_size = tpi.data_len;

                     rc = cli_do_write(cli);

                     cli_wr_send_ack(cli, cli_wr_err_to_ack(rc));
                  }
               }
            }
         }
      }
      else if(cmd == TDTP_CMD_RD)
      {
         rc = chk_pkt_RD(&tpi, cli->rx_buf);
         if(rc == ERR_OK)
         {
            if(cli->rd_prc_func) //-- Session running
            {
               if(cli->rd_sid == tpi.sid) //-- Same session
                  return;  //-- Ignore this packet

               cli->rd_prc_func(cli, TDTP_OP_CLOSE); //-- Stop RD session(if any)
               cli->rd_prc_func = NULL;
            }

            //-- Start new session

            cli->rd_sid       = tpi.sid;
            cli->rd_blk_num   = 0;
            cli->rd_unexp_blk = 0;

            //-- Processing address

            cli_rd_proc_addr(cli, &tpi);  //-- Set 'data_prc_func' according to the address value
            if(cli->rd_prc_func) //-- The address is known and valid
            {
               cli->rd_addr_ptr = tpi.addr_ptr; //-- for open()
               cli->rd_addr_len = tpi.addr_len;

               rx_len = cli->rd_prc_func(cli, TDTP_OP_RDOPEN); //-- open, set active, Check addr - inside
               if(rx_len < 0)
               {
                  cli->rd_prc_func = NULL;
                  cli->rd_blk_num = -1;
                  cli->rd_tsize   = TDTP_ACK_ERR_FOPEN; //-- tsize here as NACK storage
               }
               else
               {
                  cli->rd_tsize  = rx_len;
                  cli->rd_nbytes = rx_len;

                  cli_do_read(cli);
               }
            }
            else  //-- Error
            {
               cli->rd_blk_num = -1;
               cli->rd_tsize   = TDTP_ACK_ERR_BADADDR; //-- tsize here as NACK storage
            }

            //-- We send an answer here - any case (RD_DATA packet)

            if(cli->rd_blk_num == -1) //-- Error, client session was terminated
            {
               //-- Message errorcode already in 'rcli->rd_tsize'
               cli->rd_blk_size = 0; //-- No data here
            }

            cli->tx_len = build_pkt_RD_DATA(cli);
            rc = tdtp_cli_send(cli->s_tx, cli->tx_buf, cli->tx_len);
            // if(rc != ERR_OK) - Add your error handler
         }
      }
      else if(cmd == TDTP_CMD_ACK_RD)
      {
         rc = chk_pkt_ACK(&tpi, cli->rx_buf, &ack_msg);
         if(rc == ERR_OK)
         {
            if(cli->rd_sid == tpi.sid)
            {
               cli->rd_unexp_blk = 0;

               if(cli->rd_blk_num == tpi.blk_num) //-- Next block needs
               {
                  if(cli->rd_nbytes > 0)
                  {
                     if(cli->rd_prc_func)
                     {
                        cli_do_read(cli);
                        cli->tx_len = build_pkt_RD_DATA(cli);
                        rc = tdtp_cli_send(cli->s_tx, cli->tx_buf, cli->tx_len);
                        // if(rc != ERR_OK) - Add your error handler
                     }
                  }
                  else //-- ACK_RD for the last block
                  {
                     if(cli->rd_prc_func)
                     {
                    //-- It was Regular Last Block ACK  - send nothing && terminate session
                        cli->rd_prc_func = NULL;
                     }
                     else
                     {
                       //-- Probably, last in session RD_DATA block was lost and
                       //-- server send ACK_RD again to obtain it
                       //-- Just re-send last RD_DATA block
                        rc = tdtp_cli_send(cli->s_tx, cli->tx_buf, cli->tx_len);
                       // if(rc != ERR_OK) - Add your error handler
                     }
                  }
               }
               else if(cli->rd_blk_num == tpi.blk_num - 1) //-- Sended block was lost, repeat it
               {
                  rc = tdtp_cli_send(cli->s_tx, cli->tx_buf, cli->tx_len);
               }
            }
            else //--Unexpected session's block
            {
               cli->rd_unexp_blk++;
               if(cli->rd_unexp_blk > CLI_MAX_UNEXP_BLOCKS)
               {
                  if(cli->rd_prc_func)
                  {
                     cli->rd_prc_func(cli, TDTP_OP_CLOSE); //-- Stop RD session(if any)
                     cli->rd_prc_func = NULL;
                  }
               }
            }
         }
      }
   }
   else if(rc == ERR_TIMEOUT)
   {
      if(cli->rd_prc_func) //-- The rd session is active
      {
         cli->rd_timeouts++;
         if(cli->rd_timeouts > CLI_MAX_RD_TIMEOUTS)
         {
            cli->rd_prc_func(cli, TDTP_OP_CLOSE);
            cli->rd_prc_func = NULL;
         }
         else
         {
            //-- Probably, RD_DATA block was lost - just
            //-- re-send last RD_DATA block

            rc = tdtp_cli_send(cli->s_tx, cli->tx_buf, cli->tx_len);
            // if(rc != ERR_OK) - Add your error handler
         }
      }
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
//----------------------------------------------------------------------------
