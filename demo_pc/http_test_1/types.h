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


//---- Interrupt function type

typedef void (*int_func)(void);

   //--- tn_user.c

void int_func_uart0(void);
void int_func_timer0(void);

   //--- utils.c

void InitHardware(void);

   //--- tn_sprintf.c

#endif


