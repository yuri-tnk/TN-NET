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

#ifndef _LPC23XX_PHY_KS8721_H_
#define _LPC23XX_PHY_KS8721_H_


    //--- KS8721BL PHY

#define  RMII_KS8721_ID              0x00221610
#define  PHY_REG_KS8721_100BASETX          0x1F

#define  PHY_RD_MAX_ITERATIONS               10

#define  MIND_BUSY                   0x00000001  // MII is Busy
#define  MIND_NOT_VAL                0x00000004  // MII Read Data not valid

#define  MII_WR_TOUT                 0x00050000  // MII Write timeout count


 int ks8721_phy_read(int phy_reg,  unsigned int * res); //-- [OUT]
 int ks8721_phy_write(int phy_reg, unsigned int data);
void ks8721_phy_update_link_state(void);
 int ks8721_phy_is_link_up(void);
 int ks8721_phy_init(LPC23XXNETINFO * nhwi);


#endif
