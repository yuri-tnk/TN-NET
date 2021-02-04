#ifndef _TN_NET_H_
#define _TN_NET_H_

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

//#define USE_DBG_MEM_ALOCATOR  1

#define  TNNET_NETTIMERTASK_PRIORITY               8
#define  TNNET_NETTIMERTASK_STK_SIZE            (128)

struct _TN_NET;
struct tn_netif;
struct reasm_entry;
struct socket;
struct mbuf;

#define  MAX_NETIF    1

//---- Random number generator data

typedef struct _RNDMWC
{
   unsigned int a;
   unsigned int x;
   unsigned int c;
   unsigned int ah;
   unsigned int al;
}RNDMWC;

//--------------------------------------------
struct _TN_NETINFO
{
   struct _TN_NET * tnet;
   struct tn_netif * ni[MAX_NETIF];
};
typedef struct _TN_NETINFO  TN_NETINFO;

typedef void (* net_timer_func)(TN_NETINFO * , int cnt);

//------------- TCP --------------

typedef void (* protocol_in_func)(struct _TN_NET * , struct mbuf *, struct tn_netif *);

struct _TN_TIMERINFO
{
   struct _TN_TIMERINFO * next;
   struct tn_netif * ni;
   net_timer_func timer_func;
};
typedef struct _TN_TIMERINFO TN_TIMERINFO;


struct _TN_PROTINFO
{
   struct _TN_PROTINFO * next;
   int prot;
   protocol_in_func in_func;
};
typedef struct _TN_PROTINFO TN_PROTINFO;

//----------------------------------------------------------------------------


struct _TN_NET
{
   int id;
   int cnt;

  //----- BSD globals --------

       //-- IP

   struct reasm_entry * ip_reasm_list;

       //-- TCP

   unsigned long tcp_sendspace; // = 1024*8;
   unsigned long tcp_recvspace; // = 1024*8;
   int tcp_iss;

   // patchable/settable parameters for tcp

   unsigned long sb_max; // = SB_MAX;                // patchable

   int  tcp_keepidle;     //  = TCPTV_KEEP_IDLE;
   int  tcp_keepintvl;    // = TCPTV_KEEPINTVL;
   int  tcp_maxidle;

   int  tcp_mssdflt;     //= TCP_MSS;
   int  tcp_rttdflt;     //= TCPTV_SRTTDFLT / PR_SLOWHZ;
   int  tcp_do_rfc1323;  // = 0;

   struct inpcb tcb;     // head of queue of active tcpcb's

   struct tn_socketex * so_tcp_ex; //-- Head of the TCP socket list

      //-- UDP

   struct inpcb * udp_last_inpcb;
   struct inpcb udb;    // head of queue of UDP pcb's

  //--------------------------

   struct tn_netif * netif_default;
   struct tn_netif * netif_list;

  //------- TN NET memory buffers pools -------------------

#ifdef USE_DBG_MEM_ALOCATOR
   TN_FMP_DBG lobuf_mpool;
#else
   TN_FMP lobuf_mpool;
#endif

#define lobuf_mpool_item_size (TNNET_SMALL_BUF_SIZE)
   unsigned int mem_lobuf_mpool[NUM_SMALL_BUF *
                                (lobuf_mpool_item_size / sizeof(int))];
#ifdef USE_DBG_MEM_ALOCATOR
   unsigned int mem_lobuf_mpool_fl[NUM_SMALL_BUF];
#endif


#ifdef USE_DBG_MEM_ALOCATOR
   TN_FMP_DBG m1buf_mpool;
#else
   TN_FMP m1buf_mpool;
#endif

#define m1buf_mpool_item_size (TNNET_MID1_BUF_SIZE)
   unsigned int mem_m1buf_mpool[NUM_MID1_BUF *
                                (m1buf_mpool_item_size / sizeof(int))];
#ifdef USE_DBG_MEM_ALOCATOR
   unsigned int mem_m1buf_mpool_fl[NUM_MID1_BUF];
#endif

         //---- Drv(Ethernet)memory - in Ethernet RAM ----

#ifdef USE_DBG_MEM_ALOCATOR
   TN_FMP_DBG hibuf_mpool;
#else
   TN_FMP hibuf_mpool;
#endif

#define hibuf_mpool_item_size (TNNET_HI_BUF_SIZE)
   unsigned int * mem_hibuf_mpool;
#ifdef USE_DBG_MEM_ALOCATOR
   unsigned int mem_hibuf_mpool_fl[NUM_HI_BUF];
#endif

#ifdef USE_DBG_MEM_ALOCATOR
   TN_FMP_DBG drv_lobuf_mpool;
#else
   TN_FMP drv_lobuf_mpool;
#endif

#define drv_lobuf_mpool_item_size (TNNET_SMALL_BUF_SIZE)
   unsigned int * mem_drv_lobuf_mpool;
#ifdef USE_DBG_MEM_ALOCATOR
   unsigned int mem_drv_lobuf_mpool_fl[NUM_DRV_SMALL_BUF];
#endif

#ifdef USE_DBG_MEM_ALOCATOR
   TN_FMP_DBG drv_m1buf_mpool;
#else
   TN_FMP drv_m1buf_mpool;
#endif

#define drv_m1buf_mpool_item_size (TNNET_MID1_BUF_SIZE)
   unsigned int * mem_drv_m1buf_mpool;
#ifdef USE_DBG_MEM_ALOCATOR
   unsigned int mem_drv_m1buf_mpool_fl[NUM_DRV_MID1_BUF];
#endif

 //----------------------------------------------------------------
  //-- Random number generator

   RNDMWC   rnd_gen;

  //-- OS related

   TN_SEM   ip_sem;
   TN_SEM   spl_sem;
   TN_TCB * spl_holder_task;
   TN_TCB  task_net_timer;
   unsigned int task_net_timer_stack[TNNET_NETTIMERTASK_STK_SIZE];
};
typedef struct _TN_NET TN_NET;


typedef void (*net_proto_timer_func)(TN_NETINFO * tnet, int cnt);

//--- UDP socket (part of socket buf)

#define UDP_SOCK_RX_QUEUE_SIZE 4

struct udp_socket
{
   TN_DQUE queueRx;
   void * queueRxMem[UDP_SOCK_RX_QUEUE_SIZE];

};

//----------------------------------------------------------------------------

int tn_net_init(TN_NET * tnet);


  //-- Timer functions

void iface1_link_timer_func(TN_NETINFO * tnet, int cnt);
void arp_timer_func(TN_NETINFO * tnet, int cnt);
void ip_slowtimo(TN_NETINFO * tneti, int cnt);

  //-- Additional

int s_memcmp(const void * buf1, const void * buf2, int count);
void * s_memset(void * dst,  int ch, int length);

#endif



