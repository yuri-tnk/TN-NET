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


#include <tnkernel/tn.h>

#include "tn_net_cfg.h"
#include "tn_net_types.h"
#include "tn_net_pcb.h"
#include "tn_net_mem.h"
#include "tn_ip.h"
#include "tn_net.h"
#include "errno.h"
#include "tn_mbuf.h"
#include "tn_netif.h"

//----------------------------------------------------------------------------
static void * do_alloc(TN_FMP * mpool, int wait)
{
   volatile void * ret_val = NULL;
   int rc;

   if(wait == 0) //-- don't wait
   {
      rc = tn_fmem_get_polling(mpool, (void**)&ret_val);
      if(rc != TERR_NO_ERR)
         return NULL;
   }
   else //-- wait
   {
      rc = tn_fmem_get(mpool, (void**)&ret_val, TN_WAIT_INFINITE);
      if(rc != TERR_NO_ERR)
         return NULL;
   }

   return (void*)ret_val;
}

//----------------------------------------------------------------------------
static void * do_ialloc(TN_FMP * mpool)
{
   volatile void * ret_val = NULL;
   int rc;

   rc = tn_fmem_get_ipolling(mpool, (void**)&ret_val);
   if(rc != TERR_NO_ERR)
      return NULL;

   return (void*)ret_val;
}


//----------------------------------------------------------------------------
static int do_free(TN_FMP * mpool, void * ptr)
{
   return tn_fmem_release(mpool, ptr);
}

//----------------------------------------------------------------------------
static int do_ifree(TN_FMP * mpool, void * ptr)
{
   return tn_fmem_irelease(mpool, ptr);
}

//----------------------------------------------------------------------------
int tn_net_init_mem_pools(TN_NET * tnet)
{
   int rc;

   tnet->lobuf_mpool.id_fmp = 0;
   rc = tn_fmem_create(&tnet->lobuf_mpool,
                 (void *)&tnet->mem_lobuf_mpool[0], // start_addr
                 lobuf_mpool_item_size,
                 NUM_SMALL_BUF);
   if(rc != TERR_NO_ERR)
      return rc;

   tnet->m1buf_mpool.id_fmp = 0;
   rc = tn_fmem_create(&tnet->m1buf_mpool,
                 (void *)&tnet->mem_m1buf_mpool[0], // start_addr
                 m1buf_mpool_item_size,
                 NUM_MID1_BUF);
   if(rc != TERR_NO_ERR)
      return rc;

//-- Rx buf

       //-- Hi

   tnet->hibuf_mpool.id_fmp = 0;
   rc = tn_fmem_create(&tnet->hibuf_mpool,
                 (void *)&tnet->mem_hibuf_mpool[0], // start_addr
                 hibuf_mpool_item_size,
                 NUM_HI_BUF);
   if(rc != TERR_NO_ERR)
      return rc;

//-- Drv buf

      //-- Small

   tnet->drv_lobuf_mpool.id_fmp = 0;
   rc = tn_fmem_create(&tnet->drv_lobuf_mpool,
                 (void *)&tnet->mem_drv_lobuf_mpool[0], // start_addr
                 drv_lobuf_mpool_item_size,
                 NUM_DRV_SMALL_BUF);
   if(rc != TERR_NO_ERR)
      return rc;

      //-- Mid 1

   tnet->drv_m1buf_mpool.id_fmp = 0;
   rc = tn_fmem_create(&tnet->drv_m1buf_mpool,
                 (void *)&tnet->mem_drv_m1buf_mpool[0], // start_addr
                 drv_m1buf_mpool_item_size,
                 NUM_DRV_MID1_BUF);

   if(rc != TERR_NO_ERR)
      return rc;

   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
void * tn_net_alloc_min(TN_NET * tnet, int wait)
{
   return do_alloc(&tnet->lobuf_mpool, wait);
}

//----------------------------------------------------------------------------
void * tn_net_ialloc_min(TN_NET * tnet)
{
   return do_ialloc(&tnet->lobuf_mpool);
}

//----------------------------------------------------------------------------
int tn_net_free_min(TN_NET * tnet, void * ptr)
{
   if((unsigned int)ptr < MEM_DRV_START_ADDR)
      return do_free(&tnet->lobuf_mpool, ptr);
   else
      return do_free(&tnet->drv_lobuf_mpool, ptr);
}

//----------------------------------------------------------------------------
int tn_net_ifree_min(TN_NET * tnet, void * ptr)
{
   if((unsigned int)ptr < MEM_DRV_START_ADDR)
      return do_ifree(&tnet->lobuf_mpool, ptr);
   else
      return do_ifree(&tnet->drv_lobuf_mpool, ptr);
}

//----------------------------------------------------------------------------
void * tn_net_alloc_tx_min(TN_NET * tnet, int wait)
{
   return do_alloc(&tnet->drv_lobuf_mpool, wait);
}

//----------------------------------------------------------------------------
void * tn_net_ialloc_tx_min(TN_NET * tnet)
{
   return do_ialloc(&tnet->drv_lobuf_mpool);
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void * tn_net_alloc_mid1(TN_NET * tnet, int wait)
{
   return do_alloc(&tnet->m1buf_mpool, wait);
}

//----------------------------------------------------------------------------
void * tn_net_ialloc_mid1(TN_NET * tnet)
{
   return do_ialloc(&tnet->m1buf_mpool);
}
//----------------------------------------------------------------------------
void * tn_net_alloc_tx_mid1(TN_NET * tnet, int wait)
{
   return do_alloc(&tnet->drv_m1buf_mpool, wait);
}

//----------------------------------------------------------------------------
void * tn_net_ialloc_tx_mid1(TN_NET * tnet)
{
   return do_ialloc(&tnet->drv_m1buf_mpool);
}

//----------------------------------------------------------------------------
int tn_net_free_mid1(TN_NET * tnet, void * ptr)
{
   if((unsigned int)ptr < MEM_DRV_START_ADDR)
      return do_free(&tnet->m1buf_mpool, ptr);
   else
      return do_free(&tnet->drv_m1buf_mpool, ptr);
}

//----------------------------------------------------------------------------
void tn_net_ifree_mid1(TN_NET * tnet, void * ptr)
{
   if((unsigned int)ptr < MEM_DRV_START_ADDR)
      do_ifree(&tnet->m1buf_mpool, ptr);
   else
      do_ifree(&tnet->drv_m1buf_mpool, ptr);
}

//----------------------------------------------------------------------------
void * tn_net_alloc_hi(TN_NET * tnet, int wait)
{
   return do_alloc(&tnet->hibuf_mpool, wait);
}

//----------------------------------------------------------------------------
void * tn_net_ialloc_hi(TN_NET * tnet)
{
   return do_ialloc(&tnet->hibuf_mpool);
}

//----------------------------------------------------------------------------
int tn_net_free_hi(TN_NET * tnet, void * ptr)
{
   return do_free(&tnet->hibuf_mpool, ptr);
}

//----------------------------------------------------------------------------
void tn_net_ifree_hi(TN_NET * tnet, void * ptr)
{
   do_ifree(&tnet->hibuf_mpool, ptr);
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void * tn_net_alloc(TN_NET * tnet, int size, int wait)
{
   if(size <= TNNET_SMALL_BUF_SIZE)
      return do_alloc(&tnet->lobuf_mpool, wait);
   else if(size <= TNNET_MID1_BUF_SIZE)
      return do_alloc(&tnet->m1buf_mpool, wait);
   else if(size <= TNNET_HI_BUF_SIZE)
      return do_alloc(&tnet->hibuf_mpool, wait);
   else
      return NULL;
}

//----------------------------------------------------------------------------
int tn_net_free_pool(TN_NET * tnet, int pool, void * ptr)
{
   int rc = -1;
   switch(pool)
   {
      case TNNET_SMALL_BUF:

         if((unsigned int)ptr < MEM_DRV_START_ADDR)
            rc = do_free(&tnet->lobuf_mpool, ptr);
         else
            rc = do_free(&tnet->drv_lobuf_mpool, ptr);
         break;

      case TNNET_MID1_BUF:

         if((unsigned int)ptr < MEM_DRV_START_ADDR)
            rc = do_free(&tnet->m1buf_mpool, ptr);
         else
            rc = do_free(&tnet->drv_m1buf_mpool, ptr);
         break;

      case TNNET_HI_BUF:

         rc = do_free(&tnet->hibuf_mpool, ptr);
         break;
   }
   return rc;
}

//----------------------------------------------------------------------------
int tn_net_ifree_pool(TN_NET * tnet, int pool, void * ptr)
{
   int rc = -1;
   switch(pool)
   {
      case TNNET_SMALL_BUF:

         if((unsigned int)ptr < MEM_DRV_START_ADDR)
            rc = do_ifree(&tnet->lobuf_mpool, ptr);
         else
            rc = do_ifree(&tnet->drv_lobuf_mpool, ptr);
         break;

      case TNNET_MID1_BUF:

         if((unsigned int)ptr < MEM_DRV_START_ADDR)
            rc = do_ifree(&tnet->m1buf_mpool, ptr);
         else
            rc = do_ifree(&tnet->drv_m1buf_mpool, ptr);
         break;

      case TNNET_HI_BUF:

         rc = do_ifree(&tnet->hibuf_mpool, ptr);
         break;
   }
   return rc;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------




