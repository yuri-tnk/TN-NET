#ifndef _TN_IN_PCB_FUNC_H_
#define _TN_IN_PCB_FUNC_H_

int in_pcballoc(TN_NET * tnet, struct socket *so, struct inpcb * head);
int in_pcbbind(TN_NET * tnet, struct inpcb *inp, struct mbuf *nam);
int in_pcbconnect(TN_NET * tnet, struct inpcb *inp, struct mbuf *nam);
void in_pcbdisconnect(TN_NET * tnet, struct inpcb *inp);
void in_pcbdetach(TN_NET * tnet, struct inpcb *inp);
struct inpcb * in_pcblookup(TN_NET * tnet,
                            struct inpcb * head,
                            unsigned int faddr,
                            unsigned int fport_arg,
                            unsigned int laddr,
                            unsigned int lport_arg,
                            int flags);

void in_setsockaddr(struct inpcb *inp, struct mbuf *nam);
void in_setpeeraddr(struct inpcb *inp, struct mbuf * nam);
int in_broadcast(struct in__addr in, TN_NETIF * ni);

#endif
