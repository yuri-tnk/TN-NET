#ifndef  _GLOBALS_H_
#define  _GLOBALS_H_


extern TN_NET   g_tnet;
extern TN_NETIF g_iface1;

//extern unsigned int g_iface1_rx_task_stack[];
//extern TN_ARPENTRY g_iface1_arp_arr[];
//extern TN_SEM      g_iface1_arp_sem;

//extern void * g_iface1_queueRxMem[];
//extern void * g_iface1_queueTxMem[];

extern TN_SEM  semTxUart;
extern TN_DQUE queueTxUart;

#endif

