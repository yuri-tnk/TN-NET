#ifndef _TN_NET_MEM_H_
#define _TN_NET_MEM_H_

#define  TNNET_SMALL_BUF           0
#define  TNNET_MID1_BUF            1
#define  TNNET_HI_BUF              2

#define  TNNET_SMALL_BUF_SIZE     32
#define  TNNET_MID1_BUF_SIZE     128
#define  TNNET_HI_BUF_SIZE      1536

#define  TNNET_MEM_DONTWAIT        0
#define  TNNET_MEM_WAIT            1

#define  MB_NOWAIT                 TNNET_MEM_DONTWAIT
#define  MB_WAIT                   TNNET_MEM_WAIT

#define  MB_MIN                    TNNET_SMALL_BUF
#define  MB_MID1                   TNNET_MID1_BUF
#define  MB_HI                     TNNET_HI_BUF


#endif
