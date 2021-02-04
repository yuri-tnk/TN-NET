#ifndef _TN_NET_UTILS_H_
#define _TN_NET_UTILS_H_


#define   _min(a,b)                   (((a)<(b))?(a):(b))
#define   _max(a,b)                   (((a)>(b))?(a):(b))

//#define   copyout(from, to, len)      bcopy(from, to, len)
//#define   copyin(from, to, len)       bcopy(from, to, len)

//-------  insque/remque -------------

struct tn_np_que
{
   struct tn_np_que * next;
   struct tn_np_que * prev;
};

#define  insque(inp, head) \
    tn_np_que_insert_entry((struct tn_np_que *) (inp), \
                                      (struct tn_np_que *) (head))

#define  remque(inp) \
     tn_np_que_remove_entry((struct tn_np_que *) inp)

void tn_np_que_insert_entry(struct tn_np_que * entry,
                        struct tn_np_que * ins_after_entry);
void tn_np_que_remove_entry(struct tn_np_que * entry);

//------------------------------------

void bzero(void * b, int length);
void bzero_dw(unsigned int * ptr, int len);

void bcopy(const void *src0, void *dst0, int length);

//------------------------------------
int  splnet(TN_NET * tnet);
int  splimp(TN_NET * tnet);
void splx(TN_NET * tnet, int sm);

//------------------------------------
 int tn_socket_wait(TN_SEM * t_sem);
void tn_net_wakeup(TN_SEM * sem);
void tn_net_wait(TN_SEM * sem);
//------------------------------------
void tn_srand(RNDMWC * mwc, unsigned char * hw_addr);
unsigned int tn_rand(RNDMWC * mwc);
//------------------------------------
void tn_net_panic(int event_code);
void tn_net_panic_int(int event_code);
 int ip4_str_to_num(char * str , unsigned int * ip4_addr);
void ip4_num_to_str(char * str , unsigned int ip4_addr);

int in4_cksum(struct mbuf *m, unsigned char nxt, int off, int len);

#define in_cksum(m, len)        in4_cksum(m, 0, 0, len)



#endif
