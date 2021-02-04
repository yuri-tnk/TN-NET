#ifndef _TN_NET_FUNC_H_
#define _TN_NET_FUNC_H_

//----------------------------------------------------------------------------
void icmp_input(TN_NET * tnet, TN_NETIF * ni, struct mbuf * mb);
 int ip_output(TN_NET * tnet, TN_NETIF * ni,  TN_MBUF * m0);
void tcp_input(TN_NET * tnet, TN_NETIF * ni, struct mbuf *m);
void ip_init(TN_NET * tnet);
void tcp_init(TN_NET * tnet);
void udp_init(TN_NET * tnet);
void ipv4_input(TN_NET * tnet, TN_NETIF * ni, struct mbuf * m);


//----------------------------------------------------------------------------

unsigned int tn_time_sec(void);
int tn_net_random_id(void);

int  sockargs(TN_NET * tnet,
              struct mbuf ** mp,
              unsigned char * buf,
              int buflen,
              int type);

#endif

