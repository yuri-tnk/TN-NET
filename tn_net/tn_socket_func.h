#ifndef _TN_SOCKET_FUNC_H_
#define _TN_SOCKET_FUNC_H_

    //--- tn_sockets.c

int s_socket(int domain, int type, int protocol);
int s_close(int s);
int s_bind(int s, const struct _sockaddr * name, int namelen);
int s_connect(int s, struct _sockaddr * name, int namelen);
int s_accept(int s,
             struct _sockaddr * addr,
             int * addrlen);
int s_listen(int s,
             int backlog);
int s_recv(int s,
           unsigned char * buf,
           int nbytes,
           int flags);
int s_recvfrom(int  s,
               unsigned char *  buf,
               int  len,
               int  flags,
               struct _sockaddr * from,
               int * fromlenaddr);
int s_send(int  s,
           unsigned char * buf,
           int  nbytes,
           int  flags);
int s_sendto(int s,
             unsigned char * buf,
             int len,
             int flags,
             struct _sockaddr * dst_addr,
             int dat_addr_len);
int s_ioctl(int s,
            int cmd,
            void * data);
int s_shutdown(int s, int how);
int  s_getpeername( int s,
                    struct _sockaddr * name,
                    int * namelen);
int s_getsockopt(int  s,
                 int level,
                 int name,
                 unsigned char * val,
                 int * avalsize);
int s_setsockopt(int s,
                 int level,
                 int name,
                 unsigned char * val,
                 int avalsize);
#endif

