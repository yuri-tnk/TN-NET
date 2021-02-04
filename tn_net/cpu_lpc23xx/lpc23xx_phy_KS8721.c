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

#include "LPC236x.h"
#include "emac.h"
#include "lpc23xx_net_emac.h"

#include "lpc23xx_phy_KS8721.h"


//-----  This is global var for the PHY addr enumeration

static volatile unsigned int g_iface1_phy_addr = 0;

//----------------------------------------------------------------------------
int ks8721_phy_read(int phy_reg,  unsigned int * res) //-- [OUT]
{
   int i;
   volatile int delay;

    //-- bits 12..8 -PHY addr, bits 4..0 - PHY reg addr

   rMAC_MADR = (g_iface1_phy_addr << 8) | (phy_reg & 0x1F);
   rMAC_MCMD = 0x0001;                    //-- read command

   for(i= 0; i < PHY_RD_MAX_ITERATIONS; i++)
   {
      if((rMAC_MIND & (MIND_BUSY | MIND_NOT_VAL))== 0)
         break;
      for(delay = 0; delay < 1000; delay++);
   }
   rMAC_MCMD = 0;

   if(i >= PHY_RD_MAX_ITERATIONS) //-- timeout
      return FALSE;

   *res =  rMAC_MRDD;
   return TRUE;
}

//----------------------------------------------------------------------------
int ks8721_phy_write(int phy_reg, unsigned int data)
{
   int i;

   //-- bits 12..8 -PHY addr, bits 4..0 - PHY reg addr

   rMAC_MADR = (g_iface1_phy_addr<<8) | (phy_reg & 0x1F);

   rMAC_MWTD = data;

   for(i= 0; i < MII_WR_TOUT; i++)
   {
      if((rMAC_MIND & 1) == 0)              //-- Bit 0 -BUSY
         break;
   }

   if(i >= MII_WR_TOUT) //-- timeout
      return FALSE;

   return TRUE;
}

//----------------------------------------------------------------------------
void ks8721_phy_update_link_state(void)
{
   volatile unsigned int op_mode;
   int rc;

   rc = phy_read(PHY_REG_KS8721_100BASETX, (unsigned int*)&op_mode);
   if(rc == FALSE)
      return;

   op_mode = (op_mode >> 2) & 0x07;  //-- Get bits 4:2

   if(op_mode == 2)       //-- [010] = 100BASE-TX half-duplex.
   {
      rMAC_MAC2    &=    ~0x1;
      rMAC_COMMAND &=  ~(1<<10);
      rMAC_IPGT     =   0x0012;   //-- IPGT_HALF_DUP

      rMAC_SUPP    |=   (1<<8);   //-- RMII - for 100Mb
   }
   else if(op_mode == 5)  //-- [101] = 10BASE-T full-duplex.
   {
      rMAC_MAC2    |=     0x1;    //-- MAC2_FULL_DUP
      rMAC_COMMAND |=  (1<<10);   //-- CR_FULL_DUP
      rMAC_IPGT     =  0x0015;    //-- IPGT_FULL_DUP

      rMAC_SUPP    &=  ~(1<<8);
   }
   else if(op_mode == 6)  //-- [110] = 100BASE-TX full-duplex.
   {
      rMAC_MAC2    |=    0x1;   //-- MAC2_FULL_DUP
      rMAC_COMMAND |=  (1<<10); //-- CR_FULL_DUP
      rMAC_IPGT     =  0x0015;  //-- IPGT_FULL_DUP

      rMAC_SUPP    |=  (1<<8);
   }
   else //-- [001] = 10BASE-T half-duplex or unknown(default)
   {
      rMAC_MAC2    &=  ~0x1;
      rMAC_COMMAND &=  ~(1<<10);
      rMAC_IPGT     =  0x0012;   //-- IPGT_HALF_DUP

      rMAC_SUPP    &=  ~(1<<8);
   }
}

//----------------------------------------------------------------------------
int ks8721_phy_is_link_up(void)
{
   int rc;
   volatile unsigned int status;

   rc = phy_read(PHY_REG_BMSR, (unsigned int*) &status);
   if(rc == TRUE) //-- O.K.
   {
      if(status & (1<<2))   //-- PHY_BMSR_LINK_STATUS)
         return TRUE;
      else
         return FALSE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------
int ks8721_phy_init(LPC23XXNETINFO * nhwi)
{
   volatile unsigned int read_res;
   volatile unsigned int delay;
   unsigned int rhy_id;
   int rc;

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
         rc = phy_read(PHY_REG_BMCR, (unsigned int*)&read_res);
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

   g_iface1_phy_addr = 1;  //-- According to the Board hardware

   rc = phy_write(PHY_REG_BMCR, 0x8000);
   if(rc == FALSE)
      return ERR_WR_PHY;

   for(rhy_id = 0; rhy_id < 10; rhy_id++)
   {
      rc = phy_read(PHY_REG_BMCR, (unsigned int*)&read_res);
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

   rc = phy_read(PHY_REG_IDR1, (unsigned int*)&read_res);
   if(rc == FALSE)
      return ERR_RD_PHY;
   rhy_id = read_res<<16;

   rc = phy_read(PHY_REG_IDR2, (unsigned int*)&read_res);
   if(rc == FALSE)
      return ERR_RD_PHY;
   rhy_id |= read_res & 0x0000FFF0; //-- we ignore  4 bits LSB (Revision Number)
                                           //--    for the chip recognition
   if(rhy_id != RMII_KS8721_ID)
      return ERR_PHY_ID;

 //-- Configure the PHY device

   //-- Use autonegotiation about the link speed.

   rc = phy_write(PHY_REG_BMCR, PHY_AUTO_NEG);
   if(rc == FALSE)
      return ERR_WR_PHY;

   //-- Wait to complete Auto_Negotiation.

   for(rhy_id = 0; rhy_id < 100; rhy_id++)
   {
      rc = phy_read(PHY_REG_BMSR, (unsigned int *)&read_res);
      if(rc == FALSE)
         return ERR_RD_PHY;
      if(read_res & 0x0020)  //-- Autonegotiation Complete.
         break;
      for(delay = 0; delay < 200000; delay++);
   }

   nhwi->link_up = phy_is_link_up();

   phy_update_link_state();

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


