#ifndef _TYPES_H_
#define _TYPES_H_


  //--- COMMON

#define LED_MASK     (1<<27)
#define IRQ_TST_MASK (1<<29)

  //--- UART

#define  LCR_DISABLE_LATCH_ACCESS    0x00000000
#define  LCR_ENABLE_LATCH_ACCESS     0x00000080

#define UART_RX_BUF_SIZE        64


//--- VIC access

#define VIC_SIZE                32
#define VECT_ADDR_INDEX      0x100
#define VECT_CNTL_INDEX      0x200


//----------------------------------------------------------------------------

#define  TDTP_CMD_RD                   1
#define  TDTP_CMD_RD_DATA              2
#define  TDTP_CMD_WR                   3
#define  TDTP_CMD_WR_DATA              4
#define  TDTP_CMD_ACK_RD               5
#define  TDTP_CMD_ACK_WR               6

#define  TDTP_ACK_WRNEXT               1
#define  TDTP_ACK_RDNEXT               2
#define  TDTP_ACK_FCLOSE               3
#define  TDTP_ACK_RD                   4

//----------------------------------------------------------------------------

typedef void (*int_func)(void);

//----------------------------------------------------------------------------
typedef struct _DAQPROTINFO
{
   unsigned char * tx_buf;
   unsigned char * rx_buf;

   unsigned int sid;
   int blk_num;
   int tsize;

   int addr_len;
   unsigned char * addr_ptr;

   unsigned char * blk_data;
   int blk_size;

   int s_rx;
   int s_tx;

   //---- Statistics

   int tx_ok;
   int tx_repeats;
   int rx_timeouts;
   int err_sock_rx;
   int err_timeout;

}DAQPROTINFO;

//---- tmp data packet info struct

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

//----------------------------------------------------------------------------
// This data structure is used to simulate DAQ data source
//----------------------------------------------------------------------------

#define DAQ_PARAM_NAME_SIZE    24
#define DAQ_PARAM_VALUE_SIZE   12

typedef struct _DAQDATASIM
{
   char parameter1_name[DAQ_PARAM_NAME_SIZE];
   char parameter1_value[DAQ_PARAM_VALUE_SIZE];
   char parameter2_name[DAQ_PARAM_NAME_SIZE];
   char parameter2_value[DAQ_PARAM_VALUE_SIZE];
   char parameter3_name[DAQ_PARAM_NAME_SIZE];
   char parameter3_value[DAQ_PARAM_VALUE_SIZE];
}DAQDATASIM;

   //--- utils.c

void InitHardware(void);
void exs_send_to_uart(unsigned char * data);
int  uart_rx_drv(unsigned char * buf, unsigned char in_byte);

   //--- tn_user.c

void int_func_uart0(void);
void int_func_timer0(void);

   //--- daq_test.c

void daq_example_prepare_data(void);
void daq_example_new_data_to_send(DAQPROTINFO * pdaq, int cnt);
int daq_send(DAQPROTINFO * pdaq, int tx_len);
int build_pkt_WR_daq(DAQPROTINFO * pdaq);
int chk_pkt_ACK(TDTPPKTINFO * pi, unsigned char * buf, int * ack_msg);


#endif


