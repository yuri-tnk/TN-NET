/*

Copyright © 2004, 2009 Yuri Tiomkin
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
#include "../tn_net/cpu_lpc23xx/emac.h"
#include "../tn_net/cpu_lpc23xx/LPC236x.h"
#include "../tnkernel/tn.h"

#include "../tn_net/tn_net_cfg.h"
#include "../tn_net/tn_net_types.h"
#include "../tn_net/tn_net_pcb.h"
#include "../tn_net/tn_net_mem.h"
#include "../tn_net/tn_net.h"
#include "../tn_net/errno.h"
#include "../tn_net/tn_mbuf.h"
#include "../tn_net/tn_netif.h"
#include "../tn_net/tn_socket.h"

#include "types.h"
#include "globals.h"

  //--- Extern functions prototypes

void drv_lpc23xx_net_int(TN_NET * tnet, TN_NETIF * ni, unsigned int status);

  //--- File global vars

static volatile unsigned int tn_sec_cnt = 0;
//static volatile int g_rx_packets = 0;

//----------------------------------------------------------------------------
//   User routine to processing IRQ (for LPC2XX)
//----------------------------------------------------------------------------
void tn_cpu_irq_handler(void)
{
   int_func pf;
   pf = (int_func)rVICVectAddr;
   if(pf != NULL)
      (*pf)();
}

//----------------------------------------------------------------------------
void int_func_timer0(void)
{
   TN_INTSAVE_DATA_INT

   static unsigned int tst = 0;

   rT0IR = 0xFF;                //-- clear int source

   tst++;
/*
   if(tst & 1)
      rFIO1SET |= IRQ_TST_MASK;
   else
      rFIO1CLR |= IRQ_TST_MASK;
*/
   tn_tick_int_processing();    //-- OS timer ticks

   if(tst % 1000 == 0) //-- 1 sec
   {
      tn_sec_cnt++;
   }

   rVICVectAddr = 0xFF;
}


//----------------------------------------------------------------------------
void int_func_ethernet(void)
{
   register unsigned int status;

   status = rMAC_INTSTATUS;

   drv_lpc23xx_net_int(&g_tnet, &g_iface1, status);

   rVICVectAddr = 0xFF;
}

//----------------------------------------------------------------------------
void tn_cpu_int_enable(void)
{
   tn_arm_enable_interrupts();

//--- Gratuitous ARP
//      rc = drv_lpc23xx_net_rx_data(&g_tnet,
//                                  &g_iface1,
//                                  tail_cnt,  // ind,
//                                  &mb); //-- [OUT]
// arp_request(&g_tnet,
//             &g_iface1,
//             g_iface1.ip_addr.s__addr); //--- Target IP addr (grt hw to it)

}

//----------------------------------------------------------------------------
unsigned int tn_time_sec(void)
{
   TN_INTSAVE_DATA
   unsigned int rc;

   tn_disable_interrupt();
   rc = tn_sec_cnt;
   tn_enable_interrupt();

   return rc;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------



