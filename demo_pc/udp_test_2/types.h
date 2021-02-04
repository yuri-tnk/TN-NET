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

typedef void (*int_func)(void);

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

   //--- tn_user.c

void int_func_uart0(void);
void int_func_timer0(void);

   //--- tn_sprintf.c


#endif


