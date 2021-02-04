#ifndef  _TDTP_H_
#define  _TDTP_H_

#ifndef BOOL
#define BOOL  int
#endif


#ifndef FALSE
#define FALSE  0
#endif

#ifndef TRUE
#define TRUE   1
#endif

//---------- TDTP server - write to client ---------------------

   //--- Server write statistics

typedef struct _TDTPSRVWRSTAT
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

}TDTPSRVWRSTAT;

//----------------------------------
struct _TDTPSRVWR; 

typedef int (*srv_rd_data_func)(struct _TDTPSRVWR * psrv, int op_mode);

struct _TDTPSRVWR
{ 
   int wr_addr_len;
   unsigned char * wr_addr_ptr;
   unsigned char * tx_buf;
   unsigned char * rx_buf;

   unsigned int wr_sid;
   int wr_blk_num;
   int wr_tsize;
   int wr_nbytes;

   unsigned char * wr_blk_data;
   int wr_blk_size;

   srv_rd_data_func rdata_func;

   int last_err;
   void * s_rx; //-- connection ( socket or similar)
   void * s_tx; //-- connection ( socket or similar)

   TDTPSRVWRSTAT stat;
};
typedef struct _TDTPSRVWR TDTPSRVWR; 


//---------- TDTP server - read from client ---------------------

   //--- Server read statistics

typedef struct _TDTPSRVRDSTAT
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
}TDTPSRVRDSTAT;

//----------------------------------

struct _TDTPSRVRD; 
typedef int (*srv_wr_data_func)(struct _TDTPSRVRD * psrv, int op_mode);

struct _TDTPSRVRD
{ 
   int rd_addr_len;
   unsigned char * rd_addr_ptr;
   unsigned char * tx_buf;
   unsigned char * rx_buf;

   unsigned int rd_sid;
   int rd_blk_num;
   int rd_tsize;
   int rd_nbytes;

   unsigned char * rd_blk_data;
   int rd_blk_size;

   srv_wr_data_func data_prc_func;

   int rd_timeouts;
   int rd_bad_blk;

   int last_err;
   void * s_rx; //-- connection ( socket or similar)
   void * s_tx; //-- connection ( socket or similar)

   TDTPSRVRDSTAT stat;
};
typedef struct _TDTPSRVRD TDTPSRVRD; 


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

   int rd_unexp_blk;

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

//#define bcopy(src, dst, len)  memcpy(dst, src, len) 

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

int tdtp_srv_send_file(TDTPSRVWR * psrv, unsigned char * addr_ptr, int addr_len);
int tdtp_srv_recv_file(TDTPSRVRD * psrv, unsigned char * addr_ptr, int addr_len);

void dump_srv_wr_statistics(TDTPSRVWR * psrv);
void dump_srv_rd_statistics(TDTPSRVRD * psrv);

     //-- tdtp_cli.c 

void tdtp_cli(TDTPCLI * cli); 

     //-- tdtp_utils.c 

int chk_pkt_WR_DATA(TDTPPKTINFO * pi, unsigned char * rx_buf, int rx_len);
int chk_pkt_WR(TDTPPKTINFO * pi, unsigned char * rx_buf, int rx_len); 
int chk_pkt_RD(TDTPPKTINFO * pi, unsigned char * buf);
int chk_pkt_RD_DATA(TDTPPKTINFO * pi, unsigned char * buf, int rx_len);  
int chk_pkt_ACK(TDTPPKTINFO * pi, unsigned char * buf, int * ack_msg);

int build_pkt_RD(TDTPSRVRD * psrv);
int build_pkt_RD_DATA(TDTPCLI * cli);
int build_pkt_WR(TDTPSRVWR * psrv);
int build_pkt_WR_DATA(TDTPSRVWR * psrv);
int build_pkt_ACK(TDTPPKTINFO * pi, unsigned char * tx_buf, int ack_msg);

     //-- tdtp_port.c 

int tdtp_cli_send(void * s, unsigned char * tx_buf, int tx_len);
int tdtp_cli_recv(void * s, unsigned char * rx_buf, int * rx_len);
int tdtp_srv_send(void * s, unsigned char * tx_buf, int tx_len);
int tdtp_srv_recv(void * s, unsigned char * rx_buf, int * rx_len);

void cli_wr_proc_addr(TDTPCLI * cli, TDTPPKTINFO * pi);
void cli_rd_proc_addr(TDTPCLI * cli, TDTPPKTINFO * pi);


//int cli_wr_data_prc_func(TDTPCLI * cli, int op_mode);
//int cli_rd_data_prc_func(TDTPCLI * cli, int op_mode);

//int srv_wr_prc_func(TDTPSRVWR * psrv, int op_mode);
//int srv_rd_prc_func(TDTPSRVRD * psrv, int op_mode);

//int srv_wr_init(TDTPSRVWR * psrv);
//int srv_rd_init(TDTPSRVRD * psrv);
//int cli_tdtp_init(TDTPCLI * cli);


#endif
