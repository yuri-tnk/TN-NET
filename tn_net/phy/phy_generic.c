/*

Copyright � 2004, 2009 Yuri Tiomkin
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

#include <tnkernel/tn.h>

#include "../tn_net_cfg.h"
#include "../tn_net_types.h"
#include "../tn_net_pcb.h"
#include "../tn_net_mem.h"
#include "../tn_net.h"
#include "../tn_mbuf.h"
#include "../tn_netif.h"

#include "phy.h"
#include "phy_irc.h"

//----------------------------------------------------------------------------
int phy_is_link_up(TN_NETIF * ni, unsigned short * op_mode)
{
   int rc;
   volatile unsigned short status;

   rc = ni->mdio_rd(ni->phy->addr, PHY_REG_BMSR, (unsigned short*)&status);
   if(rc == TRUE) //-- O.K.
   {
      ni->mdio_rd(ni->phy->addr, PHY_REG_100BASETX, op_mode);
      *op_mode = (*op_mode >> 2) & 0x07;
      if(status & (1<<2))   //-- PHY_BMSR_LINK_STATUS)
         return TRUE;
      else
         return FALSE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------
int phy_init(TN_NETIF * ni)
{
   volatile unsigned short read_res;
   volatile unsigned int delay;
   unsigned int rhy_id;
   int rc;
   //LPC23XXNETINFO * nhwi = (LPC23XXNETINFO *)ni->drv_info;

/*
   g_iface1_phy_addr = 0;

   for(;;)
   {
      //-- Put the PHY into the reset mode

      rc = phy_write(PHY_REG_BMCR, 0x8000);
      if(rc == FALSE)
         return ERR_WR_PHY;
      for(delay = 0; delay < 10000; delay++);

      //- Waiting for the end of the hardware reset

      for(delay = 0; delay < 0x1000; delay++)
      {
         rc = ni->mdio_rd(ni->phy->addr, PHY_REG_BMCR, (unsigned short*)&read_res);
         if(rc == FALSE)
            return ERR_RD_PHY;
         if((read_res & 0x8000) == 0) // Reset complete
            break;
      }

      if(delay < 0x1000)
         break;

      g_iface1_phy_addr++;
      g_iface1_phy_addr &= 0x1f;
   }
*/

 //  g_iface1_phy_addr = 1;  //-- According to the Board hardware

   rc = ni->mdio_wr(ni->phy->addr, PHY_REG_BMCR, 0x8000);
   if(rc == FALSE)
      return ERR_WR_PHY;
   for(rhy_id = 0; rhy_id < 10; rhy_id++)
   {
      rc = ni->mdio_rd(ni->phy->addr, PHY_REG_BMCR, (unsigned short*)&read_res);
      if(rc == FALSE)
         return ERR_RD_PHY;
      if((read_res  & (1<<15)) == 0)
         break;

      for(delay = 0; delay < 20000; delay++);
   }
 //-- Read RHY ID

   //-- Register 2h - PHY Identifer 1

        //-- bits 15:0  - PHY ID Number;  for Micrel KS8721BL =  0x0022

   //-- Register 3h - PHY Identifer 2

       //-- bits 15:10 - PHY ID Number;  for Micrel KS8721BL  - 000101
       //-- bits 15:10 - Model Number;   for Micrel KS8721BL  - 100001
       //-- bits  3:0  - Revision Number for Micrel KS8721BL  -  >= 1001

   rc = ni->mdio_rd(ni->phy->addr, PHY_REG_IDR1, (unsigned short*)&read_res);
   if(rc == FALSE)
      return ERR_RD_PHY;
   rhy_id = (unsigned int)(read_res)<<16;

   rc = ni->mdio_rd(ni->phy->addr, PHY_REG_IDR2, (unsigned short*)&read_res);
   if(rc == FALSE)
      return ERR_RD_PHY;
   rhy_id |= read_res & 0x0000FFF0; //-- we ignore  4 bits LSB (Revision Number)
                                           //--    for the chip recognition
   ni->phy_id = rhy_id;
   //if(rhy_id != RMII_KS8721_ID && rhy_id != RMII_LAN8720_ID && rhy_id != RMII_LAN9303_ID)
   //   return ERR_PHY_ID;

 //-- Configure the PHY device

   //-- Use autonegotiation about the link speed.
   
   rc = ni->mdio_wr(ni->phy->addr, PHY_REG_BMCR, ni->phy->mode);
   if(rc == FALSE)
      return ERR_WR_PHY;

   //-- Wait to complete Auto_Negotiation.
//#ifdef PHY_WAIT_ANEG_AFTER_INIT
//   for(rhy_id = 0; rhy_id < 100; rhy_id++)
//   {
//      rc = ni->mdio_rd(ni->phy->addr, PHY_REG_BMSR, (unsigned int *)&read_res);
//      if(rc == FALSE)
//         return ERR_RD_PHY;
//      if(read_res & 0x0020)  //-- Autonegotiation Complete.
//         break;
//      for(delay = 0; delay < 200000; delay++);
//   }
//   if(read_res & (1<<2))
//     nhwi->link_up = TRUE;//-- PHY_BMSR_LINK_STATUS)
//   rc = ni->mdio_rd(ni->phy->addr, PHY_100BASETX, (unsigned short*)&read_res);
//   if (rc == TRUE)
//     ni->drv_ioctl(ni, IOCTL_MAC_SET_MODE, (void *)read_res);
//#endif
   return 0;  //TERR_NO_ERR;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


