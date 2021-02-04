#ifndef _TN_S_UDP_H_
#define _TN_S_UDP_H_

  //--- tn_sockets_udp.c

int udp_s_socket(TN_NET * tnet, int domain, int type, int protocol);
int udp_s_close(TN_NET * tnet, struct socket * so);
int udp_s_connect(TN_NET * tnet,
                  struct socket * so,
                  struct _sockaddr * name,
                  int namelen);
int udp_s_bind(TN_NET * tnet,
               struct socket * so,
               const struct _sockaddr * name,
               int namelen);
int udp_s_recvfrom(TN_NET * tnet,
                   struct socket * so,
                   unsigned char *  buf,
                   int  len,
                   int  flags,
                   struct _sockaddr * from, // unsigned char *  from,
                   int * fromlenaddr);
int udp_s_sendto(TN_NET * tnet,
                 struct socket * so,
                 unsigned char * buf,
                 int len,
                 int flags,
                 struct _sockaddr * to,
                 int tolen);
int udp_s_ioctl(TN_NET * tnet,
                struct socket * so,
                int cmd,
                void * data);
#endif



