#ifndef  _TDTP_H_
#define  _TDTP_H_

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

#ifndef BOOL
#define BOOL  int
#endif


#ifndef FALSE
#define FALSE  0
#endif

#ifndef TRUE
#define TRUE   1
#endif

//---------- TDTP server - write to client (to_client - 'tc' prefix) ---------

   //--- Server tc statistics

typedef struct _TDTPSRVTCSTAT
{
   int tsize;
   int blk_num;
   int blk_send;
   int rx_timeouts;
   int recv_errors;

   int ack_wrnext;
   int ack_fclose;
   int ack_iluse;
   int ack_err_fwrite;
   int ack_err_undef;
   int ack_err_overflow;
   int ack_err_badaddr;
   int ack_err_unknown;
   int bad_ack;

}TDTPSRVTCSTAT;

//----------------------------------
struct _TDTPSRVTC;

typedef int (* srv_tc_data_func)(struct _TDTPSRVTC * psrv, int op_mode);

struct _TDTPSRVTC
{
   int tc_addr_len;
   unsigned char * tc_addr_ptr;
   unsigned char * tx_buf;
   unsigned char * rx_buf;

   unsigned int tc_sid;
   int tc_blk_num;
   int tc_tsize;
   int tc_nbytes;

   unsigned char * tc_blk_data;
   int tc_blk_size;

   srv_tc_data_func tc_data_func;

   int last_err;
   void * s_rx; //-- connection ( socket or similar)
   void * s_tx; //-- connection ( socket or similar)
   void * remoteAddr; //-- Should be (void*)&SOCKETADDR_IN

   TDTPSRVTCSTAT stat;
};
typedef struct _TDTPSRVTC TDTPSRVTC;


//---------- TDTP server - read from client (from_client - 'fc' prefix) ------

   //--- Server read statistics

typedef struct _TDTPSRVFCSTAT
{
   int tsize;
   int blk_wr;

   int ack_rdnext;
   int ack_err_fwrite;
   int ack_event_fclose;
   int blk_send;
   int blk_recv;
   int nack_blk;
   int dup_blk;
   int expected_blk;
   int bad_blk;
   int unknown_blk;
   int timeouts;
   int send_dup_ack;
   int err_timeouts;
   int err_recv;
   int err_bad_blk;
}TDTPSRVFCSTAT;

//----------------------------------

struct _TDTPSRVFC;
typedef int (* srv_fc_data_func)(struct _TDTPSRVFC * psrv, int op_mode);

struct _TDTPSRVFC
{
   int fc_addr_len;
   unsigned char * fc_addr_ptr;
   unsigned char * tx_buf;
   unsigned char * rx_buf;

   unsigned int fc_sid;
   int fc_blk_num;
   int fc_tsize;
   int fc_nbytes;

   unsigned char * fc_blk_data;
   int fc_blk_size;

   srv_fc_data_func fc_data_func;

   int fc_timeouts;
   int fc_bad_blk;
   int fc_unexp_blk;

   int last_err;
   void * s_rx; //-- connection ( socket or similar)
   void * s_tx; //-- connection ( socket or similar)
   void * remoteAddr; //-- Should be (void*)&SOCKETADDR_IN

   TDTPSRVFCSTAT stat;
};
typedef struct _TDTPSRVFC TDTPSRVFC;

//------------ TDTP client - recv from server /send to server (both)

struct _TDTPCLI;
typedef int (*tdtp_cli_prc_pfunc)(struct _TDTPCLI * cli, int op_mode);

struct _TDTPCLI
{
   int wr_addr_len;
   unsigned char * wr_addr_ptr;
   int rd_addr_len;
   unsigned char * rd_addr_ptr;

   int tx_len;
   int rd_timeouts;

   unsigned int wr_sid;
   int wr_blk_num;
   int wr_tsize;
   int wr_nbytes;

   unsigned int rd_sid;
   int rd_blk_num;
   int rd_tsize;
   int rd_nbytes;

   unsigned char * wr_blk_data;
   int wr_blk_size;

   unsigned char * rd_blk_data;
   int rd_blk_size;

   tdtp_cli_prc_pfunc  wr_prc_func;
   tdtp_cli_prc_pfunc  rd_prc_func;

   unsigned char * tx_buf;
   unsigned char * rx_buf;

   void * s_rx; //-- connection ( socket or similar)
   void * s_tx; //-- connection ( socket or similar)
};
typedef struct _TDTPCLI TDTPCLI;

//---------- Client tmp data packet info struct

struct _TDTPPKTINFO
{
   unsigned int sid;
   int blk_num;
   int tsize;
   int addr_len;
   unsigned char * addr_ptr;
   int data_len;
   unsigned char * data_ptr;
};
typedef struct _TDTPPKTINFO TDTPPKTINFO;

//---------------------------------------------------------------

#define bcopy(src, dst, len)  memcpy(dst, src, len)

//---------------------------------------------------------------

#define  TDTP_DEF_RX_BLK_SIZE          512
#define  TDTP_MAX_DATA_SIZE            512

//---------------------------------------------------------------

#define  TDTP_ACK_PACKET_LEN          17

#define  TDTP_CMD_RD                   1
#define  TDTP_CMD_RD_DATA              2
#define  TDTP_CMD_WR                   3
#define  TDTP_CMD_WR_DATA              4
#define  TDTP_CMD_ACK_RD               5
#define  TDTP_CMD_ACK_WR               6

#define  TDTP_OP_RDOPEN                8
#define  TDTP_OP_READ                  9
#define  TDTP_OP_CLOSE                10
#define  TDTP_OP_WROPEN               11
#define  TDTP_OP_WRITE                12

#define  TDTP_ACK_WRNEXT               1
#define  TDTP_ACK_RDNEXT               2
#define  TDTP_ACK_FCLOSE               3
#define  TDTP_ACK_RD                   4

#define  TDTP_ACK_ERR_ILUSE          100
#define  TDTP_ACK_ERR_FWRITE         101
#define  TDTP_ACK_ERR_UNDEF          102
#define  TDTP_ACK_ERR_OVERFLOW       103
#define  TDTP_ACK_ERR_BADADDR        104
#define  TDTP_ACK_ERR_FREAD          105
#define  TDTP_ACK_ERR_FOPEN          106

//----------------------------------------------------------------------------

     //-- tdtp_srv.c

int tdtp_srv_send_file(TDTPSRVTC * psrv, int * flag_to_stop_addr);
int tdtp_srv_recv_file(TDTPSRVFC * psrv, int * flag_to_stop_addr);

void dump_srv_tc_statistics(TDTPSRVTC * psrv);
void dump_srv_fc_statistics(TDTPSRVFC * psrv);

     //-- tdtp_cli.c

void tdtp_cli(TDTPCLI * cli);

     //-- tdtp_utils.c

int chk_pkt_WR_DATA(TDTPPKTINFO * pi, unsigned char * rx_buf, int rx_len);
int chk_pkt_WR(TDTPPKTINFO * pi, unsigned char * rx_buf, int rx_len);
int chk_pkt_RD(TDTPPKTINFO * pi, unsigned char * buf);
int chk_pkt_RD_DATA(TDTPPKTINFO * pi, unsigned char * buf, int rx_len);
int chk_pkt_ACK(TDTPPKTINFO * pi, unsigned char * buf, int * ack_msg);

int build_pkt_RD(TDTPSRVFC * psrv);
int build_pkt_RD_DATA(TDTPCLI * cli);
int build_pkt_WR(TDTPSRVTC * psrv);
int build_pkt_WR_DATA(TDTPSRVTC * psrv);
int build_pkt_ACK(TDTPPKTINFO * pi, unsigned char * tx_buf, int ack_msg);

     //-- tdtp_port.c

int tdtp_srv_send(void * s, unsigned char * tx_buf, int tx_len, void * remoteAddr);
int tdtp_srv_recv(void * s, unsigned char * rx_buf, int * rx_len, void * remoteAddr);



#endif
