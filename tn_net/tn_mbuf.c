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

/*
 * Copyright (c) 1982, 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)uipc_mbuf.c	8.2 (Berkeley) 1/4/94
 */

#include "../tnkernel/tn.h"

#include "tn_net_cfg.h"
#include "tn_net_types.h"
#include "tn_net_pcb.h"
#include "tn_net_mem.h"
#include "tn_ip.h"
#include "tn_net.h"
#include "errno.h"
#include "tn_mbuf.h"
#include "tn_netif.h"
#include "tn_net_utils.h"
#include "tn_net_mem_func.h"

#if defined (__ICCARM__)   // IAR ARM
#pragma diag_suppress=Pa039
#endif

//----------------------------------------------------------------------------
int m_ifreem(TN_NET * tnet, TN_MBUF * mb)
{
   while(mb != NULL)
   {
      mb = m_ifree(tnet, mb);
      if(mb == INV_MEM)
         return -1;
   }
   return 0; //-- OK
}

//----------------------------------------------------------------------------
int m_freem(TN_NET * tnet, TN_MBUF * mb)
{
   while(mb != NULL)
   {
      mb = m_free(tnet, mb);  //-- Not from interrupt
      if(mb == INV_MEM)
         return -1;
   }
   return 0; //-- OK
}

//----------------------------------------------------------------------------
TN_MBUF * m_free(TN_NET * tnet, TN_MBUF * mb)
{
   TN_INTSAVE_DATA
   int rc;
   TN_MBUF * rv;
   TN_MBUF * p_pri;
   int fReleasePrimary;

   if(mb == NULL)
      return NULL;

   rv = mb->m_next;
   fReleasePrimary = FALSE;

   if(mb->m_flags & M_ETHHDR) //-- Ethernet header has not data buf
   {
      rc = tn_net_free_min(tnet, (void *)mb);
      if(rc != 0)
         rv = INV_MEM;
      return rv;
   }
   //-- Free data buf(if any)

   if(mb->m_flags & M_DBHDR) //-- Primary ?
      p_pri = mb;            //-- self
   else
   {
      p_pri = mb->m_pri;
      if(p_pri == NULL)  //-- sanity check
         return NULL;
   }

   tn_disable_interrupt();

   if(p_pri->m_refcnt > 0)
   {
      p_pri->m_refcnt--;
      if(p_pri->m_refcnt == 0)
         fReleasePrimary = TRUE;
   }

   tn_enable_interrupt();

   if(fReleasePrimary)
   {
      if((p_pri->m_flags & M_NOALLOC) == 0 && p_pri->m_dbuf != NULL)
      {
         rc = tn_net_free_pool(tnet, p_pri->m_dbtype, (void*)p_pri->m_dbuf);
         if(rc != 0)
            rv = INV_MEM;

      }
      //-- Always free here primary header

      rc = tn_net_free_min(tnet, (void *)p_pri);
      if(rc != 0)
         rv = INV_MEM;
   }

   if(((mb->m_flags & M_DBHDR) == 0) && (p_pri != mb))  //-- If not primary, free descriptor
   {
      rc = tn_net_free_min(tnet, (void *)mb);
      if(rc != 0)
         rv = INV_MEM;
   }
   return rv;
}

//----------------------------------------------------------------------------
TN_MBUF * m_ifree(TN_NET * tnet, TN_MBUF * mb)
{
   TN_INTSAVE_DATA_INT

   TN_MBUF * rv;
   TN_MBUF * p_pri;
   int fReleasePrimary;
   int rc;

   if(mb == NULL)
      return NULL;

   rv = mb->m_next;
   fReleasePrimary = FALSE;

   if(mb->m_flags & M_ETHHDR) //-- Ethernet header has not data buf
   {
      rc =tn_net_ifree_min(tnet, (void *)mb);
      if(rc != 0)
         rv = INV_MEM;
      return rv;
   }
   //-- Free data buf(if any)

   if(mb->m_flags & M_DBHDR)      //-- Primary ?
      p_pri = mb;        //-- self
   else
   {
      p_pri = mb->m_pri;
      if(p_pri == NULL)  //-- sanity check
         return NULL;
   }

   tn_idisable_interrupt();

   if(p_pri->m_refcnt > 0)
   {
      p_pri->m_refcnt--;
      if(p_pri->m_refcnt == 0)
         fReleasePrimary = TRUE;
   }

   tn_ienable_interrupt();

   if(fReleasePrimary)
   {
      if((p_pri->m_flags & M_NOALLOC) == 0 && p_pri->m_dbuf != NULL)
      {
         rc = tn_net_ifree_pool(tnet, p_pri->m_dbtype, (void*)p_pri->m_dbuf);
         if(rc != 0)
            rv = INV_MEM;
      }
      //-- Always free here primary header

      rc = tn_net_ifree_min(tnet, (void *)p_pri);
      if(rc != 0)
         rv = INV_MEM;
   }

   if(((mb->m_flags & M_DBHDR) == 0) && (p_pri != mb))  //-- If not primary, free descriptor
   {
      rc = tn_net_ifree_min(tnet, (void *)mb);
      if(rc != 0)
         rv = INV_MEM;
   }
   return rv;
}

//----------------------------------------------------------------------------
struct mbuf * m_copym (TN_NET * tnet,
                       struct mbuf * mb,
                       int offset,
                       int len,
                       int wait)
{
   TN_INTSAVE_DATA

   TN_MBUF * p_pri;
   TN_MBUF * mb_first;
   TN_MBUF * mb_prev;
   TN_MBUF * t_mb;
   int org_len;
   int total;

   org_len = len;

   if(offset < 0 || len <= 0)
      return NULL;

   //-- Skip over leading mbufs that are smaller than the offset
   while(mb != NULL && mb->m_len <= offset)
   {
      offset -= mb->m_len;
      mb = mb->m_next;
   }
   if(mb == NULL)
      return NULL;       // Offset was too big

//----------------------------------------------------------------------------
   mb_first = NULL;
   mb_prev = NULL;
   total = 0;
   for(;;)
   {
      //--- mb to copy

      t_mb = m_alloc_hdr(tnet, wait);  //-- bzero() all - inside
      if(t_mb == NULL)
      {
         if(mb_first)
         {
            total = m_freem(tnet, mb_first);  // fatal err - free all alloc resources
            if(total == INV_MEM_VAL)
               tn_net_panic(INV_MEM_VAL_34);
         }
         return NULL; 
      }

      if(mb_prev)
         mb_prev->m_next = t_mb;

      if(mb->m_flags & M_DBHDR) //-- Primary ?
         p_pri = mb;            //-- self
      else
         p_pri = mb->m_pri;

      tn_disable_interrupt();
      p_pri->m_refcnt++;   //---- Increment a reference cnt
      tn_enable_interrupt();

      if(mb->m_flags & M_PKTHDR)
         t_mb->m_flags = M_NOALLOC | M_PKTHDR;
      else
         t_mb->m_flags = M_NOALLOC;

      t_mb->m_pri = p_pri;


      if(mb_first == NULL) //-- First mb to copy - header
      {
         t_mb->m_tlen  = len;
         t_mb->m_data = mb->m_data + offset;

         t_mb->m_len = _min(len, mb->m_len - offset);

         mb_first = t_mb;
      }
      else //-- Not first
      {
         t_mb->m_data = mb->m_data;

         t_mb->m_len = _min(len, mb->m_len);
      }

      total += t_mb->m_len;
      len   -= t_mb->m_len;

      if(len <= 0) //-- Finished
         break;

    //--- Now - next mb to copy

      mb = mb->m_next;
      if(mb == NULL) //-- Something wrong
      {
         if(mb_first)
         {
            total = m_freem(tnet, mb_first);  // fatal err - free all alloc resources
            if(total == INV_MEM_VAL)
               tn_net_panic(INV_MEM_VAL_35);
         }
         return NULL;
      }

      mb_prev = t_mb;
   }

   //--- If here - copy should be O.K

   if(total != org_len)
   {
      if(mb_first)
      {
         total = m_freem(tnet, mb_first);  // fatal err - free all alloc resources
         if(total == INV_MEM_VAL)
            tn_net_panic(INV_MEM_VAL_36);
      }
      return NULL;
   }

   return mb_first; //-- OK
}

//----------------------------------------------------------------------------
void m_adj(struct mbuf * mp, int req_len)
{
   int len = req_len;
   struct mbuf * m;
   int count;

   m = mp;
   if(m == NULL)
      return;

   if(len >= 0)  //-- Trim from head.
   {
      while (m != NULL && len > 0)
      {
         if(m->m_len <= len)
         {
            len -= m->m_len;
            m->m_len = 0;
            m = m->m_next;
         }
         else
         {
            m->m_len -= len;
            m->m_data += len;
            len = 0;
         }
      }
      m = mp;
      if(mp->m_flags & M_PKTHDR)
         m->m_tlen -= (req_len - len);
   }
   else
   {
      //-- Trim from tail.  Scan the mbuf chain,
      //-- calculating its length and finding the last mbuf.
      //-- If the adjustment only affects this mbuf, then just
      //-- adjust and return.  Otherwise, rescan and truncate
      //-- after the remaining size.

      len = -len;
      count = 0;
      for(;;)
      {
         count += m->m_len;
         if(m->m_next == NULL)
            break;
         m = m->m_next;
      }

      if(m->m_len >= len)
      {
         m->m_len -= len;
         if(mp->m_flags & M_PKTHDR)
            mp->m_tlen -= len;
         return;
      }
      count -= len;
      if(count < 0)
         count = 0;

      //-- Correct length for chain is "count".
      //-- Find the mbuf with last data, adjust its length,
      //-- and toss data from remaining mbufs on chain.

      m = mp;
      if(m->m_flags & M_PKTHDR)
         m->m_tlen = count;
      for(; m != NULL; m = m->m_next)
      {
         if(m->m_len >= count)
         {
            m->m_len = count;
            break;
         }
         count -= m->m_len;
      }

      for(m = m->m_next; m != NULL; m = m->m_next)
         m->m_len = 0;
   }
}

//----------------------------------------------------------------------------
void m_adj_free(TN_NET * tnet, struct mbuf * mp, int req_len)
{
   int len = req_len;
   struct mbuf * m;
   struct mbuf * mb_tmp;
   int count;
   int rc;

   m = mp;
   if(m == NULL)
      return;

   if(len >= 0)  //-- Trim from head.
   {
      while (m != NULL && len > 0)
      {
         if(m->m_len <= len)
         {
            len -= m->m_len;
            m->m_len = 0;
            m = m_free(tnet, m);
            if(m == INV_MEM)
               tn_net_panic(INV_MEM_VAL_27);
         }
         else
         {
            m->m_len -= len;
            m->m_data += len;
            len = 0;
         }
      }
      m = mp;
      if(mp->m_flags & M_PKTHDR)
         m->m_tlen -= (req_len - len);
   }
   else
   {
      //-- Trim from tail.  Scan the mbuf chain,
      //-- calculating its length and finding the last mbuf.
      //-- If the adjustment only affects this mbuf, then just
      //-- adjust and return.  Otherwise, rescan and truncate
      //-- after the remaining size.

      len = -len;
      count = 0;
      for(;;)
      {
         count += m->m_len;
         if(m->m_next == NULL)
            break;
         m = m->m_next;
      }

      if(m->m_len >= len)
      {
         m->m_len -= len;
         if(mp->m_flags & M_PKTHDR)
            mp->m_tlen -= len;
         return;
      }
      count -= len;
      if(count < 0)
         count = 0;

      //-- Correct length for chain is "count".
      //-- Find the mbuf with last data, adjust its length,
      //-- and toss data from remaining mbufs on chain.

      m = mp;
      if(m->m_flags & M_PKTHDR)
         m->m_tlen = count;
      for(; m != NULL; m = m->m_next)
      {
         if(m->m_len >= count)
         {
            m->m_len = count;
            break;
         }
         count -= m->m_len;
      }

      mb_tmp = m;
      m = m->m_next;
      if(m)
      {
         rc = m_freem(tnet, m);
         if(rc == INV_MEM_VAL)
            tn_net_panic(INV_MEM_VAL_37);

      }
      mb_tmp->m_next = NULL;
   }
}

//----------------------------------------------------------------------------
int tn_get_dbuf_size(struct mbuf * mb)
{
   struct mbuf * p_pri;

   if(mb == NULL)
      return -1;

   if(mb->m_flags & M_DBHDR)    //-- A data buf header ?
      p_pri = mb;        //-- self
   else
      p_pri = mb->m_pri;

   if(p_pri == NULL)   //-- sanity check
      return -1;

   return m_get_dbuf_size(p_pri->m_dbtype);
}


//----------------------------------------------------------------------------
int m_get_dbuf_size(int dtype)
{
   int rc = 0;

   switch(dtype)
   {
      case TNNET_SMALL_BUF:

         rc = TNNET_SMALL_BUF_SIZE;
         break;

      case TNNET_MID1_BUF:

         rc = TNNET_MID1_BUF_SIZE;
         break;

      case TNNET_HI_BUF:

         rc = TNNET_HI_BUF_SIZE;
         break;
   }
   return rc;
}

//----------------------------------------------------------------------------
// Alloc mbuf
//----------------------------------------------------------------------------
TN_MBUF * mb_get(TN_NET * tnet, int mbuf_type, int wait, int use_tx_pool)
{
   TN_MBUF * res;

   res = m_alloc_hdr(tnet, wait);
   if(res == NULL)
      return NULL;

   switch(mbuf_type)
   {
      case MB_MIN:
         if(use_tx_pool)
            res->m_dbuf = (unsigned char *)tn_net_alloc_tx_min(tnet, wait);
         else
            res->m_dbuf = (unsigned char *)tn_net_alloc_min(tnet, wait);
         break;
      case MB_MID1:
         if(use_tx_pool)
            res->m_dbuf = (unsigned char *)tn_net_alloc_tx_mid1(tnet, wait);
         else
            res->m_dbuf = (unsigned char *)tn_net_alloc_mid1(tnet, wait);
         break;
      case MB_HI: //-- always from tx pool
            res->m_dbuf = (unsigned char *)tn_net_alloc_hi(tnet, wait);
         break;
   }

   if(res->m_dbuf == NULL)
   {
      tn_net_free_min(tnet, (void*)res);
      return NULL;
   }

   res->m_pri    = res;          //-- self - case primary
   res->m_dbtype = mbuf_type;    //-- data buffer type(to obtain dbuf size)
   res->m_data   = res->m_dbuf;
   res->m_refcnt = 1;
   res->m_flags  = M_DBHDR;

   return res;
}

//----------------------------------------------------------------------------
//  We call this function in the Rx Drv to copy small Rx buf
//  -> descr & data buf - from regular memory
//----------------------------------------------------------------------------
TN_MBUF * mb_iget(TN_NET * tnet, int mbuf_type, int use_tx_pool)
{
   TN_MBUF * res;

   res = m_ialloc_hdr(tnet);
   if(res == NULL)
      return NULL;

   switch(mbuf_type)
   {
      case MB_MIN:
         if(use_tx_pool)
            res->m_dbuf = (unsigned char *)tn_net_ialloc_tx_min(tnet);
         else
            res->m_dbuf = (unsigned char *)tn_net_ialloc_min(tnet);
         break;
      case MB_MID1:
         if(use_tx_pool)
            res->m_dbuf = (unsigned char *)tn_net_ialloc_tx_mid1(tnet);
         else
            res->m_dbuf = (unsigned char *)tn_net_ialloc_mid1(tnet);
         break;
      case MB_HI:
         res->m_dbuf = (unsigned char *)tn_net_ialloc_hi(tnet);
         break;
   }

   if(res->m_dbuf == NULL)
   {
      tn_net_ifree_min(tnet, (void*)res);
      return NULL;
   }

   res->m_pri    = res;          //-- self - case primary
   res->m_dbtype = mbuf_type;    //-- data buffer type(to obtain dbuf size)
   res->m_data   = res->m_dbuf;
   res->m_refcnt = 1;
   res->m_flags  = M_DBHDR;

   return res;
}

//----------------------------------------------------------------------------
TN_MBUF * m_alloc_hdr(TN_NET * tnet, int wait)
{
   TN_MBUF * res;

   res = (TN_MBUF *)tn_net_alloc_min(tnet, wait);
   if(res == NULL)
      return NULL;

   //-- 'res' is always aligned to int

   bzero_dw((unsigned int*)res, sizeof(TN_MBUF)>>2);

   return res;
}

//----------------------------------------------------------------------------
// Header - from regular memory
//----------------------------------------------------------------------------
TN_MBUF * m_ialloc_hdr(TN_NET * tnet)
{
   TN_MBUF * res;

   res = (TN_MBUF *)tn_net_ialloc_min(tnet);
   if(res == NULL)
      return NULL;

  //-- 'res' is always aligned to int

   bzero_dw((unsigned int*)res, sizeof(TN_MBUF)>>2);

   return res;
}

//----------------------------------------------------------------------------
int m_read(TN_NET * tnet, TN_MBUF * mb, unsigned char * dst_ptr)
{
   int len;
   int total;
   TN_MBUF * next_buf;

   if(dst_ptr == NULL)
      return 0;

   total = 0;
   while(mb)
   {
      len = mb->m_len;
      bcopy((unsigned char *)mb->m_data, dst_ptr, len);
      dst_ptr += len;
      total   += len;

      //-- the overall mbuf was copied - free it now

      next_buf = mb->m_next;
      mb = m_free(tnet, mb);   //-- m now - next mbuf(if not exists - NULL)
      if(mb == INV_MEM)
          tn_net_panic(INV_MEM_VAL_28);
      if(mb)
         mb->m_next = next_buf;
   }
   return total;
}

//----------------------------------------------------------------------------
// Concatenate mbuf chain n to m.
// Both chains must be of the same type (e.g. MT_DATA).
// Any m_tlen is not updated.
//----------------------------------------------------------------------------
void m_cat(TN_NET * tnet, struct mbuf * m, struct mbuf * n)
{
   struct mbuf * p_pri;
   int buf_size;

   while(m->m_next)  //-- Go to last mbuf in chain 'm'
      m = m->m_next;

   while(n)
   {
//--------------------------------------------------
      if(m->m_flags & M_DBHDR)    //-- primary ?
         p_pri = m;        //-- self
      else
         p_pri = m->m_pri;
      if(p_pri == NULL)   //-- sanity check
         return;

      buf_size = m_get_dbuf_size(p_pri->m_dbtype);
      if(buf_size == 0) //-- sanity check
         return;
//--------------------------------------------------

      if((m->m_flags & M_NOALLOC) ||
          m->m_data + m->m_len + n->m_len >= &p_pri->m_dbuf[buf_size])
      {
        // just join the two chains
         m->m_next = n;
         return;
      }
     // splat the data from one into the other
      bcopy(mtod(n, unsigned char *), mtod(m, unsigned char *) + m->m_len,
                                                     (unsigned int)n->m_len);
      m->m_len += n->m_len;
      n = m_free(tnet, n);
      if(n == INV_MEM)
         tn_net_panic(INV_MEM_VAL_29);
   }
}

//----------------------------------------------------------------------------
// mb_hi not is not free here
//----------------------------------------------------------------------------
TN_MBUF * m_hi_to_mid1(TN_NET * tnet, TN_MBUF * mb_hi)
{
   TN_MBUF * mb_prev;
   TN_MBUF * mb_first;
   TN_MBUF * mb = NULL;
   unsigned char * ptr;
   int len;
   int nbytes;

   if(mb_hi == NULL || tnet == NULL)
      return NULL;

   mb_prev = NULL;
   mb_first = NULL;
   len = mb_hi->m_len;
   ptr = mb_hi->m_data;

   while(len)
   {
      if(len > TNNET_MID1_BUF_SIZE)
         nbytes = TNNET_MID1_BUF_SIZE;
      else
         nbytes = len;

      mb = mb_get(tnet, MB_MID1, MB_NOWAIT, FALSE); //-- Not From Tx pool
      if(mb == NULL)
      {
         if(mb_first)
         {
            len = m_freem(tnet, mb_first);
            if(len == INV_MEM_VAL)
               tn_net_panic(INV_MEM_VAL_38);
         }
         return NULL;
      }

      mb->m_len = nbytes;
      bcopy(ptr, mb->m_data, nbytes);

      if(mb_prev == NULL) //-- First mbuf in the chain
         mb_first = mb;
      else                //-- != NULL
         mb_prev->m_next = mb;

      mb_prev = mb;

      ptr += nbytes;
      len -= nbytes;
   }

   //-- Now mb - last buf in the chain
   if(mb)
      mb->m_next = mb_hi->m_next;

   if(mb_first)
   {
      mb_first->m_flags = mb_hi->m_flags;
      mb_first->m_tlen  = mb_hi->m_len;
      mb_first->m_nextpkt = mb_hi->m_nextpkt;
   }
   return mb_first;
}

//----------------------------------------------------------------------------
TN_MBUF * buf_to_mbuf(TN_NET * tnet, unsigned char * buf, int len, int use_drv_pool)
{
   TN_MBUF * mb;
   TN_MBUF * mb_prev;
   TN_MBUF * mb_first;
   int tx_len;
   int nbytes;
   unsigned char * ptr;

   mb_prev = NULL;
   tx_len  = len;
   ptr     = buf;
   mb_first = NULL;
   while(tx_len)
   {
      nbytes = _min(TNNET_MID1_BUF_SIZE, tx_len);

      mb = mb_get(tnet, MB_MID1, MB_NOWAIT, use_drv_pool); //-- From Tx pool
      if(mb == NULL)
      {
         if(mb_first)
         {
            nbytes = m_freem(tnet, mb_first);
            if(nbytes == INV_MEM_VAL)
               tn_net_panic(INV_MEM_VAL_39);
         }
         return NULL;
      }

      bcopy(ptr, mb->m_data, nbytes);
      mb->m_len = nbytes;

      if(mb_prev == NULL) //-- First mbuf in the chain
         mb_first = mb;
      else                //-- != NULL
         mb_prev->m_next = mb;

      mb_prev = mb;

      ptr    += nbytes;
      tx_len -= nbytes;
   }

   mb_first->m_flags |= M_PKTHDR;
   mb_first->m_tlen = len;

   return mb_first;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------




