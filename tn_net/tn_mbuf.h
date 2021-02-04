#ifndef _TN_MBUF_H_
#define _TN_MBUF_H_

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


#define  MSG_ZEROCOPY  0x0010       /* if zero copy interface enabled */

//-- TN NET

#define  M_DBHDR     0x0001    //-- This descr is primary and it is "header"
                                                       //-- of a data buffer
#define  M_PKTHDR    0x0002    //-- start of record
#define  M_NOALLOC   0x0004    //-- The data buffer is foreign - do not free it

#define  M_ETHHDR    0x0008

/*
                     0x0010
                     0x0020
                     0x0040
                     0x0080
*/

#define  M_BCAST     0x0100     /* send/received as link-level broadcast */
#define  M_MCAST     0x0200     /* send/received as link-level multicast */


// Checksumming flags

#define   M_IPV4_CSUM_OUT       0x0001       // IPv4 checksum needed
#define   M_TCPV4_CSUM_OUT      0x0002       // TCP checksum needed
#define   M_UDPV4_CSUM_OUT      0x0004       // UDP checksum needed
#define   M_IPV4_CSUM_IN_OK     0x0008       // IPv4 checksum verified
#define   M_IPV4_CSUM_IN_BAD    0x0010       // IPv4 checksum bad
#define   M_TCP_CSUM_IN_OK      0x0020       // TCP/IPv4 checksum verified
#define   M_TCP_CSUM_IN_BAD     0x0040       // TCP/IPv4 checksum bad
#define   M_UDP_CSUM_IN_OK      0x0080       // UDP/IPv4 checksum verified
#define   M_UDP_CSUM_IN_BAD     0x0100       // UDP/IPv4 checksum bad

// mbuf types

#define   MT_FREE                    0       // should be on free list
#define   MT_DATA                    1       // dynamic (data) allocation
#define   MT_HEADER                  2       // packet header
#define   MT_SONAME                  3       // socket name
#define   MT_SOOPTS                  4       // socket options
#define   MT_FTABLE                  5       // fragment reassembly header
#define   MT_CONTROL                 6       // extra-data protocol message
#define   MT_OOBDATA                 7       // expedited data

//-- Exists only as "descr" part of the mbuf
//-- Size - 32 bytes; packed

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(push, 1)
#endif

//-- !!! Do not move the structure data members !!!

struct mbuf
{
   __pkt_field short m_flags;
   __pkt_field short m_len;
   struct mbuf * m_next;
   unsigned char * m_data;  //-- Ptr to data in m_dbuf;
   unsigned char * m_dbuf;   //-- Allocated data buf start addr (only for primary (M_DBHDR is set))
   struct mbuf * m_nextpkt;
   struct mbuf * m_pri;     //-- Ptr to the primary data buffer's descriptor
 //----------
   unsigned int m_tlen : 20,  //-- as pkthdr.len
    //-- valid only for primary data buffer's descriptor (M_DBHDR is set)
                m_dbtype : 4, //-- data buffer type(to obtain dbuf size)
                m_refcnt : 8;
 //----------
   void * r1;
}__pkt_struct_ex;

typedef struct mbuf TN_MBUF;

#if defined (__ICCARM__)   // IAR ARM
#pragma pack(pop)
#endif

#define   m_get_dbaddr(m)  (unsigned char *)(m)->m_descr;
#define   mtod(m,type) ((type)((m)->m_data))

#define INV_MEM        ((TN_MBUF *)0xFFFFFFFF)
#define INV_MEM_VAL    (-1)

#define INV_MEM_VAL_I_1    701
#define INV_MEM_VAL_I_2    702
#define INV_MEM_VAL_I_3    703

#define INV_MEM_1          801
#define INV_MEM_2          802
#define INV_MEM_3          803
#define INV_MEM_4          804

#define INV_MEM_VAL_1     1001
#define INV_MEM_VAL_2     1002
#define INV_MEM_VAL_3     1003
#define INV_MEM_VAL_4     1004
#define INV_MEM_VAL_5     1005
#define INV_MEM_VAL_6     1006
#define INV_MEM_VAL_7     1007
#define INV_MEM_VAL_8     1008
#define INV_MEM_VAL_9     1009
#define INV_MEM_VAL_10    1010
#define INV_MEM_VAL_11    1011
#define INV_MEM_VAL_12    1012
#define INV_MEM_VAL_13    1013
#define INV_MEM_VAL_14    1014
#define INV_MEM_VAL_15    1015
#define INV_MEM_VAL_16    1016
#define INV_MEM_VAL_17    1017
#define INV_MEM_VAL_18    1018
#define INV_MEM_VAL_19    1019
#define INV_MEM_VAL_20    1020
#define INV_MEM_VAL_21    1021
#define INV_MEM_VAL_22    1022
#define INV_MEM_VAL_23    1023
#define INV_MEM_VAL_24    1024
#define INV_MEM_VAL_25    1025
#define INV_MEM_VAL_26    1026
#define INV_MEM_VAL_27    1027
#define INV_MEM_VAL_28    1028
#define INV_MEM_VAL_29    1029
#define INV_MEM_VAL_30    1030
#define INV_MEM_VAL_31    1031
#define INV_MEM_VAL_32    1032
#define INV_MEM_VAL_33    1033
#define INV_MEM_VAL_34    1034
#define INV_MEM_VAL_35    1035
#define INV_MEM_VAL_36    1036
#define INV_MEM_VAL_37    1037
#define INV_MEM_VAL_38    1038
#define INV_MEM_VAL_39    1039
#define INV_MEM_VAL_40    1040
#define INV_MEM_VAL_41    1041
#define INV_MEM_VAL_42    1042


#define TNP_ACCEPT        1100
#define TNP_IN4_CHKSUM_1  1101
#define TNP_IN4_CHKSUM_2  1102
#define TNP_SOCLOSE_1     1103
#define TNP_SOACCEPT_1    1104
#define TNP_SOFREE_DQ     1105
#define TNP_SBGLUSH_1     1106
#define TNP_SBDROP_1      1107


#define   m_copy(tnet, m, o, l)    m_copym((tnet),(m), (o), (l), TNNET_MEM_DONTWAIT)

TN_MBUF * mb_get(TN_NET * tnet, int mbuf_type, int wait, int use_tx_pool);
TN_MBUF * mb_iget(TN_NET * tnet, int mbuf_type, int use_tx_pool);


TN_MBUF * m_freem_pkt(TN_NET * tnet, TN_MBUF * mb);

int  m_freem(TN_NET * tnet, TN_MBUF *);
int  m_ifreem(TN_NET * tnet, TN_MBUF *);
TN_MBUF * m_free(TN_NET * tnet, TN_MBUF *);
TN_MBUF * m_ifree(TN_NET * tnet, TN_MBUF *);

TN_MBUF * m_getclr(int wait);
TN_MBUF * mbuf_alloc_hdr(void);
struct mbuf * m_copym (TN_NET * tnet, struct mbuf *, int , int , int );
struct mbuf * m_get(int wait);
int  tn_get_dbuf_size(struct mbuf * mb);
int  m_get_dbuf_size(int dtype);
void m_adj(struct mbuf *, int);

TN_MBUF * m_alloc_hdr(TN_NET * tnet, int wait);
TN_MBUF * m_ialloc_hdr(TN_NET * tnet);

int m_read(TN_NET * tnet, TN_MBUF * mb, unsigned char * dst_ptr);

void m_cat(TN_NET * tnet, struct mbuf * m, struct mbuf * n);
TN_MBUF * buf_to_mbuf(TN_NET * tnet, unsigned char * buf, int len, int use_drv_pool);



#endif


