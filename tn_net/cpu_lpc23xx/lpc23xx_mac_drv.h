#ifndef _LPC23XX_MAC_DRV_H_
#define _LPC23XX_MAC_DRV_H_

int drv_lpc23xx_net_rx_data(TN_NET * tnet,
                            TN_NETIF * ni,
                            int ind,
                            TN_MBUF ** mb_out);
int drv_lpc23xx_net_tx_data(TN_NET * tnet,
                            TN_NETIF * ni,
                            TN_MBUF * mb_to_tx,
                            int from_interrupt);
int drv_lpc23xx_mdio_write(int phy_addr, 
                           int phy_reg, 
                           unsigned short value);
int drv_lpc23xx_mdio_read(int phy_addr, 
                          int phy_reg, 
                          unsigned short * value);
#endif

