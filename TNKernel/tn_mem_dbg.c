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

/* ver 2.5.3  */

#include "tn.h"
#include "tn_utils.h"
#include "tn_port.h"

//-- Local function prototypes --

static void * fm_get_dbg(TN_FMP_DBG * fmpd);
static int fm_put_dbg(TN_FMP_DBG * fmpd, void * mem);

//----------------------------------------------------------------------------
// Structure's field fmp->id_id_fmp have to be set to 0
int tn_fmem_dbg_create(TN_FMP_DBG * fmpd,
                     void * start_addr,
                     unsigned int * free_list,
                     unsigned int block_size,
                     int num_blocks
                    )
{
   unsigned int * p_tmp;
   unsigned int i,j;

   if(fmpd == NULL || fmpd->id_fmp == TN_ID_FSMEMORYPOOL)
      return TERR_WRONG_PARAM;

   if(start_addr == NULL || num_blocks < 2 || free_list == NULL
       || block_size < sizeof(int))
   {
      fmpd->fblkcnt = 0;
      fmpd->num_blocks = 0;
      fmpd->id_fmp = 0;
      fmpd->free_list = NULL;
      return TERR_WRONG_PARAM;
   }

   queue_reset(&(fmpd->wait_queue));

   for(i=0; i< num_blocks; i++)
      free_list[i] = 0;

  //-- Prepare addr/block aligment
   i = (((unsigned int)start_addr + (TN_ALIG -1))/TN_ALIG) * TN_ALIG;
   fmpd->start_addr  = (void*)i;
   fmpd->block_size = ((block_size + (TN_ALIG -1))/TN_ALIG) * TN_ALIG;

   i = (unsigned int)start_addr + block_size * num_blocks;
   j = (unsigned int)fmpd->start_addr + fmpd->block_size * num_blocks;
   fmpd->num_blocks = num_blocks;
   while(j > i)  //-- Get actual num_blocks
   {
      j -= fmpd->block_size;
      fmpd->num_blocks--;
   }
   if(fmpd->num_blocks < 2)
   {
      fmpd->fblkcnt = 0;
      fmpd->num_blocks = 0;
      fmpd->free_list = NULL;
      return TERR_WRONG_PARAM;
   }

  //-- Set blocks ptrs for allocation -------

   fmpd->free_list = free_list;

   p_tmp = (unsigned int *)fmpd->start_addr;
   for(i = 0; i < fmpd->num_blocks; i++)
   {
       fmpd->free_list[i] = (unsigned int)p_tmp;
       fmpd->end_addr = (unsigned int)p_tmp;
       p_tmp += fmpd->block_size>>2;
   }

   fmpd->fblkcnt   = fmpd->num_blocks;
   fmpd->id_fmp = TN_ID_FSMEMORYPOOL;
  //-----------------------------------------

   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
int tn_fmem_dbg_get(TN_FMP_DBG * fmpd, void ** p_data, unsigned int timeout)
{
   TN_INTSAVE_DATA
   int rc; //-- return code
   void * ptr;

   if(fmpd == NULL || p_data == NULL || timeout == 0)
      return  TERR_WRONG_PARAM;
   if(fmpd->id_fmp != TN_ID_FSMEMORYPOOL)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   ptr = fm_get_dbg(fmpd);
   if(ptr != NULL) //-- Get memory
   {
      *p_data = ptr;
      rc = TERR_NO_ERR;
   }
   else
   {
      task_curr_to_wait_action(&(fmpd->wait_queue),
                                     TSK_WAIT_REASON_WFIXMEM, timeout);
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
int tn_fmem_dbg_get_polling(TN_FMP_DBG * fmpd, void ** p_data)
{
   TN_INTSAVE_DATA
   int rc; //-- return code
   void * ptr;

   if(fmpd == NULL || p_data == NULL)
      return  TERR_WRONG_PARAM;
   if(fmpd->id_fmp != TN_ID_FSMEMORYPOOL)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   ptr = fm_get_dbg(fmpd);
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
int tn_fmem_dbg_get_ipolling(TN_FMP_DBG * fmpd, void ** p_data)
{
   TN_INTSAVE_DATA_INT
   int rc; //-- return code
   void * ptr;

   if(fmpd == NULL || p_data == NULL)
      return  TERR_WRONG_PARAM;
   if(fmpd->id_fmp != TN_ID_FSMEMORYPOOL)
      return TERR_NOEXS;

   TN_CHECK_INT_CONTEXT

   tn_idisable_interrupt();

   ptr = fm_get_dbg(fmpd);
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
int tn_fmem_dbg_release(TN_FMP_DBG * fmpd, void * p_data)
{
   TN_INTSAVE_DATA

   CDLL_QUEUE * que;
   TN_TCB * task;
   int rc = TERR_NO_ERR;

   if(fmpd == NULL || p_data == NULL)
      return  TERR_WRONG_PARAM;
   if(fmpd->id_fmp != TN_ID_FSMEMORYPOOL)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   if(!is_queue_empty(&(fmpd->wait_queue)))
   {
      que = queue_remove_head(&(fmpd->wait_queue));
      task = get_task_by_tsk_queue(que);

      task->data_elem = p_data;

      if(task_wait_complete(task, FALSE))
      {
         tn_enable_interrupt();
         tn_switch_context();
         return TERR_NO_ERR;
      }
   }
   else
   {
      rc = fm_put_dbg(fmpd, p_data);
   }
   tn_enable_interrupt();
   return  rc;
}

//----------------------------------------------------------------------------
int tn_fmem_dbg_irelease(TN_FMP_DBG * fmpd, void * p_data)
{
   TN_INTSAVE_DATA_INT

   CDLL_QUEUE * que;
   TN_TCB * task;
   int rc = TERR_NO_ERR;

   if(fmpd == NULL || p_data == NULL)
      return  TERR_WRONG_PARAM;
   if(fmpd->id_fmp != TN_ID_FSMEMORYPOOL)
      return TERR_NOEXS;

   TN_CHECK_INT_CONTEXT

   tn_idisable_interrupt();

   if(!is_queue_empty(&(fmpd->wait_queue)))
   {
      que = queue_remove_head(&(fmpd->wait_queue));
      task = get_task_by_tsk_queue(que);

      task->data_elem = p_data;

      if(task_wait_complete(task, FALSE))
      {
         tn_context_switch_request = TRUE;
         tn_ienable_interrupt();
         return TERR_NO_ERR;
      }
   }
   else
      rc = fm_put_dbg(fmpd, p_data);

   tn_ienable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
static void * fm_get_dbg(TN_FMP_DBG * fmpd)
{
   void * p_tmp;
   int i;

   if(fmpd->fblkcnt > 0)
   {
      for(i = 0; i < fmpd->num_blocks; i++)
      {
         if(fmpd->free_list[i] != 0)
         {
            p_tmp = (void*)fmpd->free_list[i];
            fmpd->free_list[i] = 0;
            fmpd->fblkcnt--;
            return p_tmp;
         }
      }
      return NULL;
   }
   return NULL;
}

//----------------------------------------------------------------------------
static int fm_put_dbg(TN_FMP_DBG * fmpd, void * mem)
{
   int i;
   int free_ind = -1;
   register unsigned int tmp;

   if(fmpd->fblkcnt < fmpd->num_blocks)
   {
      if((unsigned int)mem < (unsigned int)fmpd->start_addr ||
           (unsigned int)mem > fmpd->end_addr)
         return TERR_ILUSE; //-- Err - bad addr

      for(i = 0; i < fmpd->num_blocks; i++)
      {
         tmp = fmpd->free_list[i];
         if(tmp == (unsigned int)mem) //-- Duplicated free
            return TERR_ILUSE; //-- Err - duplicated free
         else if(free_ind == -1)
         {
            if(tmp == 0) //-- Free cell
                free_ind = i;
         }
      }
      if(free_ind == -1)
         return TERR_WSTATE;
      else
      {
         fmpd->free_list[free_ind] = (unsigned int)mem;
         fmpd->fblkcnt++;
         return TERR_NO_ERR;
      }
   }
   return TERR_OVERFLOW;
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------


