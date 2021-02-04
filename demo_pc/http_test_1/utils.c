
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

#include <stdlib.h>

#include "../tn_net/cpu_lpc23xx/LPC236X.h"
#include "../tnkernel/tn.h"

#include "../tn_net/tn_net_cfg.h"
#include "../tn_net/tn_net_types.h"
#include "../tn_net/tn_net_pcb.h"
#include "../tn_net/tn_net_mem.h"
#include "../tn_net/tn_ip.h"
#include "../tn_net/tn_net.h"
#include "../tn_net/tn_mbuf.h"
#include "../tn_net/tn_netif.h"

#include "types.h"
#include "globals.h"

/*----------- Project specific pinout description - --------------------------

Port        Pin desc              Pin N    Using as   I/O dir set     I/O dir
------     -------------         -----     --------   -----------    ---------
 P0.0    RD1/TXD3/SDA1             46       I/O
 P0.1    TD1/RXD3/SCL1             47       I/O
 P0.2    TXD0                      98       TXD0
 P0.3    RXD0                      99       RXD0
 P0.4    I2SRX_CLK/RD2/CAP2.0      81       I/O(CAN RX)                In
 P0.5    I2SRX_WS/TD2/CAP2.1       80       I/O(CAN TX)                Out
 P0.6    I2SRX_SDA/SSEL1/MAT2.0    79       I/O
 P0.7    I2STX_CLK/SCK1/MAT2.1     78       I/O
 P0.8    I2STX_WS/MISO1/MAT2.2     77       I/O
 P0.9    I2STX_SDA/MOSI1/MAT2.3    76       I/O
 P0.10   TXD2/SDA2/MAT3.0          48       I/O
 P0.11   RXD2/SCL2/MAT3.1          49       I/O
 P0.15   TXD1/SCK0/SCK             62       I/O
 P0.16   RXD1/SSEL0/SSEL           63       I/O
 P0.17   CTS1/MISO0/MISO           61       I/O
 P0.18   DCD1/MOSI0/MOSI           60       I/O
 P0.19   DSR1/MCICLK/SDA1          59       I/O
 P0.20   DTR1/MCICMD/SCL1          58       I/O
 P0.21   RI1/MCIPWR/RD1            57       I/O
 P0.22   RTS1/MCIDAT0/TD1          56       I/O
 P0.23   AD0.0/I2SRX_CLK/CAP3.0     9       I/O
 P0.24   AD0.1/I2SRX_WS/CAP3.1      8       I/O
 P0.25   AD0.2/I2SRX_SDA/TXD3       7       I/O
 P0.26   AD0.3/AOUT/RXD3            6       I/O
 P0.27   SDA0                      25       I/O
 P0.28   SCL0                      24       I/O
 P0.29   U1D+                      29    USB-U1D+
 P0.30   U1D-                      30    USB-U1D-

 P1.0    ENET_TXD0                 95     ENET_TXD0
 P1.1    ENET_TXD1                 94     ENET_TXD1
 P1.4    ENET_TX_EN                93     ENET_TX_EN
 P1.8    ENET_CRS                  92     ENET_CRS
 P1.9    ENET_RXD0                 91     ENET_RXD0
 P1.10   ENET_RXD1                 90     ENET_RXD1
 P1.14   ENET_RX_ER                89     ENET_RX_ER
 P1.15   ENET_REF_CLK              88     ENET_REF_CLK
 P1.16   ENET_MDC                  87     ENET_MDC
 P1.17   ENET_MDIO                 86     ENET_MDIO
 P1.18   U1UP_LED/PWM1.1/CAP1.0    32       I/O
 P1.19   CAP1.1                    33       I/O
 P1.20   PWM1.2/SCK0               34       I/O
 P1.21   PWM1.3/SSEL0              35       I/O
 P1.22   MAT1.0                    36       I/O
 P1.23   PWM1.4/MISO0              37       I/O
 P1.24   PWM1.5/MOSI0              38       I/O
 P1.25   MAT1.1                    39       I/O
 P1.26   PWM1.6/CAP0.0             40       I/O
 P1.27   CAP0.1                    43       I/O
 P1.28   PCAP1.0/MAT0.0            44       I/O
 P1.29   PCAP1.1/MAT0.1            45       I/O
 P1.30   VBUS/AD0.4                21     USB-VBUS
 P1.31   SCK1/AD0.5                20       I/O

 P2.0    PWM1.1/TXD1/TRACECLK      75       I/O
 P2.1    PWM1.2/RXD1/PIPESTAT0     74       I/O
 P2.2    PWM1.3/CTS1/PIPESTAT1     73       I/O
 P2.3    PWM1.4/DCD1/PIPESTAT2     70       I/O
 P2.4    PWM1.5/DSR1/TRACESYNC     69       I/O
 P2.5    PWM1.6/DTR1/TRACEPKT0     68       I/O
 P2.6    PCAP1.0/RI1/TRACEPKT1     67       I/O
 P2.7    RD2/RTS1/TRACEPKT2        66       I/O
 P2.8    TD2/TXD2/TRACEPKT3        65       I/O
 P2.9    U1CONNECT/RXD2/EXTIN0     64    USB-U1CONNECT
 P2.10   EINT0                     53       I/O(RTS)                   in
 P2.11   EINT1/MCIDAT1/I2STX_CLK   52       I/O
 P2.12   EINT2/MCIDAT2/I2STX_WS    51       I/O
 P2.13   EINT3/MCIDAT3/I2STX_SDA   50       I/O

 P3.25   MAT0.0/PWM1.2]            27       I/O
 P3.26   MAT0.1/PWM1.3]            26       I/O

 P4.28   MAT2.0/TXD3               82       I/O
 P4.29   MAT2.1/RXD3               85     ETHER- E_PD                   out(???)
-----------------------------------------------------------------------------*/


static void InitUSB(void);
static void InitEthernet(void);
static void InitUART0(void);
static void InitVIC(void);
int install_irq(int IntNumber, unsigned int HandlerAddr, int Priority);

//----------------------------------------------------------------------------
void InitHardware(void)
{
   unsigned int MValue;
   unsigned int NValue;

//--- Pins - to output & set to initial values ------------------------------

  rSCS |= 0x1;  // GPIOM(bit 0) = 1  High speed GPIO is enabled on ports 0 and 1

  rIODIR0 = 0;  //-- input
  rIODIR1 = 0;  //-- input
  rIOSET0 = 0;  //-- ???
  rIOSET1 = 0;  //-- ???


#define  MASK_FIO_P0  (1) | (1<<1) | (1<<6) | (1<<7) | (1<<8) |  \
                     (1<<9) | (1<<10) | (1<<11) | (1<<15) | (1<<7) | \
                     (1<<8) | (1<<9) | (1<<10) | (1<<11) | (1<<15) | \
                     (1<<16) | (1<<17) | (1<<18) | (1<<19) | (1<<20) | \
                     (1<<21) | (1<<22) | (1<<23) | (1<<24) | (1<<25) | \
                     (1<<26) | (1<<27) | (1<<28)


   rFIO0DIR |= MASK_FIO_P0;  //-- output
   rFIO0SET |= MASK_FIO_P0;  //-- Set  to 1

// ????? P1.30   VBUS/AD0.4                21     USB-VBUS
#define  MASK_FIO_P1  (1<<18) | (1<<19) | (1<<20) | (1<<21) | (1<<22) | \
                     (1<<23) | (1<<24) | (1<<25) | (1<<26) | (1<<27) | \
                     (1<<28) | (1<<29) | (1u<<31)

   rFIO1DIR |= MASK_FIO_P1;  //-- output
   rFIO1SET |= MASK_FIO_P1;  //-- Set  to 1


#define  MASK_FIO_P2  (1<<0) | (1<<1) | (1<<2) | (1<<3) | (1<<4) | (1<<5) | \
                     (1<<6) | (1<<7) | (1<<8) | (1<<11) | (1<<12) | (1<<13)

   rFIO2DIR |= MASK_FIO_P2;  //-- output
   rFIO2SET |= MASK_FIO_P2;  //-- Set  to 1

#define  MASK_FIO_P3  (1<<25) | (1<<26)

   rFIO3DIR |= MASK_FIO_P3;  //-- output
   rFIO3SET |= MASK_FIO_P3;  //-- Set  to 1

#define  MASK_FIO_P4  (1<<28)

// P4.29   MAT2.1/RXD3               85     ETHER- E_PD             out(???)
//        (PHY Power Down. 1 = Normal operation, 0 = Power-down (def - pull-up))

   rFIO4DIR |= MASK_FIO_P4;  //-- output
   rFIO4SET |= MASK_FIO_P4;  //-- Set  to 1


//-------------------- PLL ------------------------------------------

    //-- F_cck = 57.6Mhz, F_pll = 288Mhz, and USB 48Mhz

#define PLL_MVALUE          11
#define PLL_NVALUE           0
#define CCLK_DIV_VALUE       4
#define USBCLK_DIV_VALUE     5

   //--  F_pll = (2 * M * F_in)/N
   //--  F_out = ((2 * (11+1) * 12000000)/(0+1))/(4+1)  = 288/5 = 57 600 000 Hz


   rCLKSRCSEL = 0x1;   //-- select main OSC, 12MHz, as the PLL clock source

   rPLLCFG  = PLL_MVALUE | (PLL_NVALUE << 16);
   rPLLFEED = 0xAA;
   rPLLFEED = 0x55;

   rPLLCON  = 1;                  //-- Enable PLL, disconnected
   rPLLFEED = 0xAA;
   rPLLFEED = 0x55;

   rCCLKCFG = CCLK_DIV_VALUE;    //-- Set clock divider

   while(((rPLLSTAT & (1<<26)) == 0));       // Check lock bit status

   MValue = rPLLSTAT & 0x00007FFF;
   NValue = (rPLLSTAT & 0x00FF0000) >> 16;
   while((MValue != PLL_MVALUE) && ( NValue != PLL_NVALUE));

   rPLLCON  = 3;                 //-- enable and connect
   rPLLFEED = 0xAA;
   rPLLFEED = 0x55;
   while(((rPLLSTAT & (1<<25)) == 0));  // Check connect bit status

//-------------------- Flash speed ------------------------------------

   rMAMTIM = 3;
   rMAMCR  = 1;

//-------------------- Power ------------------------------------------

   rPCONP |= (1u<<30); //-- bit 30 - PCENET Ethernet block power/clock control bit.

//-------------------- Interrupt --------------------------------------

   InitVIC();

//-------------------- Timer 0  - interrupt 1 ms ----------------------

   rPCONP |= (1<<1);         //-- PCTIM0(bit 1) =1 Timer/Counter 0 enabled

   rPCLKSEL0 |= (0x01 << 2); //-- bit3:2 =01-> Clock div = 1 for Timer 0

   rT0PR = 0;           //-- Prscaler = 0
   rT0PC = 0;

   rT0MR0 = 57600;// * 10;
   rT0MCR = 3;          //-- bit 0=1 -int on MR0 , bit 1=1 - Reset on MR0

   rT0TCR = 1;          //-- Timer 0 - run

   //--  Set interrupt for Timer 0 --

   install_irq(4,                             //-- IntNumber for Timer 0
               (unsigned int)int_func_timer0, //-- Handler
               1);                            //-- Priority

//----------------- UART 0 ------------------------------------

   InitUART0();
   InitUSB();
   InitEthernet();
}

//----------------------------------------------------------------------------
static void InitUSB(void)
{
}

//----------------------------------------------------------------------------
static void InitEthernet(void)
{
 //  install_irq(6,                             //-- IntNumber for UART0
 //              (unsigned int)int_func_uart0,  //-- Handler
 //              2);                            //-- Priority
}

//----------------------------------------------------------------------------
static void InitUART0(void)
{
   // F_in = 57 600 000 Hz

//-- U0DLM =0, U0DLL =3, DIVADDVAL =3, MULVAL =10 UART0baudrate=923076.923077
//-- err = 0.160256 %

//-- U0DLM =0, U0DLL =25, DIVADDVAL =1, MULVAL =4, UART0baudrate=115200.000000
//-- err = 0.000000 %

   //-- Pins
   rPINSEL0 |= (0x01<<4) |  //-- P0.2  TXD0
               (0x01<<6);   //-- P0.3  RXD0

   rPCLKSEL0 |= (0x01 << 6); //-- bit 7:6 =01-> Clock div = 1 for UART0


   //-- enable access to divisor latch regs
   rU0LCR = LCR_ENABLE_LATCH_ACCESS;

   //-- set divisor for desired baud
   rU0FDR =  (0x04<<4)  //-- MULVAL
                  | 1;  //-- DIVADDVAL
   rU0DLM = 0;
   rU0DLL = 25;//32; //-- 32 - 115200  4 - 921600   // (14746000*4)/(16*115200*8)  = 4

   //-- disable access to divisor latch regs (enable access to xmit/rcv fifos
   //-- and int enable regs)
   rU0LCR = LCR_DISABLE_LATCH_ACCESS;

   //-- Enable UART0 rx interrupts
  // rU0IER = 3;  //-- Enable RDA(Receive Data Available) int & TX Empty int

   rU0IER = 0;  //-- Disable UART int

   rU0FCR = (0x3<<6) | 1;  //-- Int Trigger - 4 bytes, Enable FIFO,Reset Tx FIFO & Rx FIFO

   //-- setup line control reg - disable break transmittion, even parity,
   //-- 1 stop bit, 8 bit chars
   rU0LCR = 0x13;//-- 0b00010011

   //--- Interrupts

 //  install_irq(6,                             //-- IntNumber for UART0
 //              (unsigned int)int_func_uart0,  //-- Handler
 //              2);                            //-- Priority
}

//----------------------------------------------------------------------------
static void InitVIC(void)
{
   int i;
   unsigned int * vect_addr, *vect_cntl;

   rVICIntSelect = 0;            //-- All int - IRQ
   rVICIntEnClr  = 0xffffffff;   //-- Disable all int
   rVICVectAddr  = 0;
   rVICIntSelect = 0;

   //-- set all the vector and vector control register to

   for(i = 0;i < VIC_SIZE; i++)
   {
      vect_addr = (unsigned int *)(VIC_BASE_ADDR + VECT_ADDR_INDEX + i*4);
      vect_cntl = (unsigned int *)(VIC_BASE_ADDR + VECT_CNTL_INDEX + i*4);
      *vect_addr = 0x0;
      *vect_cntl = 0x0F;
   }
}

//----------------------------------------------------------------------------
int install_irq(int IntNumber, unsigned int HandlerAddr, int Priority)
{
   unsigned int * vect_addr;
   unsigned int * vect_cntl;

   rVICIntEnClr = 1 << IntNumber;        //-- Disable Interrupt
   if(IntNumber >= VIC_SIZE)
      return 0;
   else
   {
      vect_addr  = (unsigned int *)(VIC_BASE_ADDR + VECT_ADDR_INDEX + IntNumber*4);
      vect_cntl  = (unsigned int *)(VIC_BASE_ADDR + VECT_CNTL_INDEX + IntNumber*4);
      *vect_addr = HandlerAddr;                //-- set interrupt vector
      *vect_cntl = Priority;
      rVICIntEnable = 1 << IntNumber;        //-- Enable Interrupt
   }
   return 1;
}

//----------------------------------------------------------------------------
int uart_rx_drv(unsigned char * buf, unsigned char in_byte)
{
   static int stage = 0;
   static int pos;

   switch(stage)
   {
      case 0:

         if(in_byte == '&' || in_byte == '#')
         {
            pos   = 0;
            stage = 1;
         }
         break;

      case 1:

         if(in_byte == '\r') //-- For Enter
         {
            stage = 0;
            return pos;
         }
         else if(in_byte == '\n')   //-- As EOL
         {
            //stage = 0;
            //return pos;
         }
         else
         {
            buf[pos] = in_byte;
            pos ++;
            if(pos >= UART_RX_BUF_SIZE) //-- Too much symbols
               stage = 0;
         }
         break;
      default:
         stage = 0;
   }
   return 0;
}

//----------------------------------------------------------------------------
void dbg_send(char * buf)
{
   int ind;
   volatile int delay;

   if(buf == NULL || buf[0] == 0)
      return;

   tn_sem_acquire(&semTxUart, TN_WAIT_INFINITE);

   for(ind = 0;;)
   {
      if(buf[ind] == 0)
         break;
      rU0THR = buf[ind++];
      for(delay = 0; delay < 500; delay++);
   }

   tn_sem_signal(&semTxUart);
}

//----------------------------------------------------------------------------
void dbg_send_int(char * buf)
{
   int ind;
   volatile int delay;
   if(buf == NULL || buf[0] == 0)
      return;

   for(ind = 0;;)
   {
      if(buf[ind] == 0)
         break;
      rU0THR = buf[ind++];
      for(delay = 0; delay < 500; delay++);
   }
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



