/*
TNKernel real-time kernel

Copyright © 2004,2007 Yuri Tiomkin
All rights reserved.

Permission to use, copy, modify, and distribute this software in source
and binary forms and its documentation for any purpose and without fee
is hereby granted, provided that the above copyright notice appear
in all copies and that both that copyright notice and this permission
notice appear in supporting documentation.

THIS SOFTWARE IS PROVIDED BY THE YURI TIOMKIN AND CONTRIBUTORS "AS IS" AND
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

/* ver 2.4  */

#include "tn.h"
#include "tn_utils.h"
#include "tn_port.h"

//-- Local function prototypes --

static void * fm_get(TN_FMP * fmp);
static int fm_put(TN_FMP * fmp, void * mem);

//----------------------------------------------------------------------------
// Structure's field fmp->id_id_fmp have to be set to 0
int tn_fmem_create(TN_FMP * fmp,
                     void * start_addr,
                     unsigned int block_size,
                     int num_blocks
                    )
{
   void ** p_tmp;
   unsigned char * p_block;
   unsigned int i,j;

   if(fmp == NULL || fmp->id_fmp == TN_ID_FSMEMORYPOOL)
      return TERR_WRONG_PARAM;

   if(start_addr == NULL || num_blocks < 2 || block_size < sizeof(int))
   {
      fmp->fblkcnt = 0;
      fmp->num_blocks = 0;
      fmp->id_fmp = 0;
      fmp->free_list = NULL;
      return TERR_WRONG_PARAM;
   }

   queue_reset(&(fmp->wait_queue));

  //-- Prepare addr/block aligment
   i = (((unsigned int)start_addr + (TN_ALIG -1))/TN_ALIG) * TN_ALIG;
   fmp->start_addr  = (void*)i;
   fmp->block_size = ((block_size + (TN_ALIG -1))/TN_ALIG) * TN_ALIG;

   i = (unsigned int)start_addr + block_size * num_blocks;
   j = (unsigned int)fmp->start_addr + fmp->block_size * num_blocks;
   fmp->num_blocks = num_blocks;
   while(j > i)  //-- Get actual num_blocks
   {
      j -= fmp->block_size;
      fmp->num_blocks--;
   }
   if(fmp->num_blocks < 2)
   {
      fmp->fblkcnt = 0;
      fmp->num_blocks = 0;
      fmp->free_list = NULL;
      return TERR_WRONG_PARAM;
   }

  //-- Set blocks ptrs for allocation -------
   p_tmp = (void **)fmp->start_addr;
   p_block  = (unsigned char *)fmp->start_addr + fmp->block_size;
   for(i = 0; i < (fmp->num_blocks - 1); i++)
   {
      *p_tmp  = (void *)p_block;  //-- contents of cell = addr of next block
      p_tmp   = (void **)p_block;
      p_block += fmp->block_size;
   }
   *p_tmp = NULL;          //-- Last memory block first cell contents -  NULL

   fmp->free_list = fmp->start_addr;
   fmp->fblkcnt   = fmp->num_blocks;

   fmp->id_fmp = TN_ID_FSMEMORYPOOL;
  //-----------------------------------------

   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
int tn_fmem_delete(TN_FMP * fmp)
{
   TN_INTSAVE_DATA
   CDLL_QUEUE * que;
   TN_TCB * task;
   if(fmp == NULL)
      return TERR_WRONG_PARAM;
   if(fmp->id_fmp != TN_ID_FSMEMORYPOOL)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   while(!is_queue_empty(&(fmp->wait_queue)))
   {
      if(tn_chk_irq_disabled() == 0) // int enable
         tn_disable_interrupt();

     //--- delete from sem wait queue
      que = queue_remove_head(&(fmp->wait_queue));
      task = get_task_by_tsk_queue(que);
      if(task_wait_complete(task,FALSE))
      {
         task->task_wait_rc = TERR_DLT;
         tn_enable_interrupt();
         tn_switch_context();
      }
   }

   if(tn_chk_irq_disabled() == 0) // int enable
      tn_disable_interrupt();

   fmp->id_fmp = 0; // Fixed-size memory pool not exists now

   tn_enable_interrupt();

   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
int tn_fmem_get(TN_FMP * fmp,void ** p_data,unsigned int timeout)
{
   TN_INTSAVE_DATA
   int rc; //-- return code
   void * ptr;

   if(fmp == NULL || p_data == NULL || timeout == 0)
      return  TERR_WRONG_PARAM;
   if(fmp->id_fmp != TN_ID_FSMEMORYPOOL)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   ptr = fm_get(fmp);
   if(ptr != NULL) //-- Get memory
   {
      *p_data = ptr;
      rc = TERR_NO_ERR;
   }
   else
   {
      task_curr_to_wait_action(&(fmp->wait_queue),
                                     TSK_WAIT_REASON_WFIXMEM,timeout);
      tn_enable_interrupt();
      tn_switch_context();
         //-- When return to this point,in data_elem have to be  valid value
      *p_data = tn_curr_run_task->data_elem; //-- Return to caller
      return tn_curr_run_task->task_wait_rc;
     // return  TERR_NO_ERR;
   }

   tn_enable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
int tn_fmem_get_polling(TN_FMP * fmp,void ** p_data)
{
   TN_INTSAVE_DATA
   int rc; //-- return code
   void * ptr;

   if(fmp == NULL || p_data == NULL)
      return  TERR_WRONG_PARAM;
   if(fmp->id_fmp != TN_ID_FSMEMORYPOOL)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   ptr = fm_get(fmp);
   if(ptr != NULL) //-- Get memory
   {
      *p_data = ptr;
      rc = TERR_NO_ERR;
   }
   else
      rc = TERR_TIMEOUT;

   tn_enable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
int tn_fmem_get_ipolling(TN_FMP * fmp,void ** p_data)
{
   TN_INTSAVE_DATA_INT
   int rc; //-- return code
   void * ptr;

   if(fmp == NULL || p_data == NULL)
      return  TERR_WRONG_PARAM;
   if(fmp->id_fmp != TN_ID_FSMEMORYPOOL)
      return TERR_NOEXS;

   TN_CHECK_INT_CONTEXT

   tn_idisable_interrupt();

   ptr = fm_get(fmp);
   if(ptr != NULL) //-- Get memory
   {
      *p_data = ptr;
      rc = TERR_NO_ERR;
   }
   else
      rc = TERR_TIMEOUT;

   tn_ienable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
int tn_fmem_release(TN_FMP * fmp,void * p_data)
{
   TN_INTSAVE_DATA

   CDLL_QUEUE * que;
   TN_TCB * task;

   if(fmp == NULL || p_data == NULL)
      return  TERR_WRONG_PARAM;
   if(fmp->id_fmp != TN_ID_FSMEMORYPOOL)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   if(!is_queue_empty(&(fmp->wait_queue)))
   {
      que = queue_remove_head(&(fmp->wait_queue));
      task = get_task_by_tsk_queue(que);

      task->data_elem = p_data;

      if(task_wait_complete(task,FALSE))
      {
         tn_enable_interrupt();
         tn_switch_context();
         return TERR_NO_ERR;
      }
   }
   else
      fm_put(fmp,p_data);

   tn_enable_interrupt();
   return  TERR_NO_ERR;
}

//----------------------------------------------------------------------------
int tn_fmem_irelease(TN_FMP * fmp,void * p_data)
{
   TN_INTSAVE_DATA_INT

   CDLL_QUEUE * que;
   TN_TCB * task;

   if(fmp == NULL || p_data == NULL)
      return  TERR_WRONG_PARAM;
   if(fmp->id_fmp != TN_ID_FSMEMORYPOOL)
      return TERR_NOEXS;

   TN_CHECK_INT_CONTEXT

   tn_idisable_interrupt();

   if(!is_queue_empty(&(fmp->wait_queue)))
   {
      que = queue_remove_head(&(fmp->wait_queue));
      task = get_task_by_tsk_queue(que);

      task->data_elem = p_data;

      if(task_wait_complete(task,FALSE))
      {
         tn_context_switch_request = TRUE;
         tn_ienable_interrupt();
         return TERR_NO_ERR;
      }
   }
   else
      fm_put(fmp,p_data);

   tn_ienable_interrupt();
   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
static void * fm_get(TN_FMP * fmp)
{
   void * p_tmp;
   if(fmp->fblkcnt > 0)
   {
      p_tmp = fmp->free_list;
      fmp->free_list = *(void **)fmp->free_list;   //-- ptr - to new free list
      fmp->fblkcnt--;
      return p_tmp;
   }
   return NULL;
}
//----------------------------------------------------------------------------
static int fm_put(TN_FMP * fmp, void * mem)
{
   if(fmp->fblkcnt < fmp->num_blocks)
   {
      *(void **)mem  = fmp->free_list;   //-- insert block into free block list
      fmp->free_list = mem;
      fmp->fblkcnt++;
      return TERR_NO_ERR;
   }
   return TERR_OVERFLOW;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


