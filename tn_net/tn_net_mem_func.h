#ifndef _TN_NET_MEM_FUNC_H_
#define _TN_NET_MEM_FUNC_H_


   int tn_net_init_mem_pools(TN_NET * tnet);
void * tn_net_alloc_min(TN_NET * tnet, int wait);
void * tn_net_ialloc_min(TN_NET * tnet);
   int tn_net_free_min(TN_NET * tnet, void * ptr);
   int tn_net_ifree_min(TN_NET * tnet, void * ptr);
void * tn_net_alloc_tx_min(TN_NET * tnet, int wait);
void * tn_net_ialloc_tx_min(TN_NET * tnet);
void * tn_net_alloc_mid1(TN_NET * tnet, int wait);
void * tn_net_ialloc_mid1(TN_NET * tnet);
void * tn_net_alloc_tx_mid1(TN_NET * tnet, int wait);
void * tn_net_ialloc_tx_mid1(TN_NET * tnet);
   int tn_net_free_mid1(TN_NET * tnet, void * ptr);
  void tn_net_ifree_mid1(TN_NET * tnet, void * ptr);
void * tn_net_alloc_hi(TN_NET * tnet, int wait);
void * tn_net_ialloc_hi(TN_NET * tnet);
   int tn_net_free_hi(TN_NET * tnet, void * ptr);
  void tn_net_ifree_hi(TN_NET * tnet, void * ptr);
void * tn_net_alloc(TN_NET * tnet, int size, int wait);
   int tn_net_free_pool(TN_NET * tnet, int pool, void * ptr);
   int tn_net_ifree_pool(TN_NET * tnet, int pool, void * ptr);

TN_MBUF * m_hi_to_mid1(TN_NET * tnet, TN_MBUF * mb_hi);
void m_adj_free(TN_NET * tnet, struct mbuf * mp, int req_len);

#endif
