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


extern CDLL_QUEUE tn_locked_mutexes_list;
extern CDLL_QUEUE tn_blocked_tasks_list;

//L. Sha, R. Rajkumar, J. Lehoczky, Priority Inheritance Protocols: An Approach
//to Real-Time Synchronization, IEEE Transactions on Computers, Vol.39, No.9, 1990

//----------------------------------------------------------------------------
//--Structure's Field mutex->id_mutex have to be set to 0
int tn_mutex_create(TN_MUTEX * mutex,
                    int attribute,
                    int ceil_priority)
{
   if(mutex == NULL || mutex->id_mutex != 0) //-- no recreation
      return TERR_WRONG_PARAM;
   if(attribute != TN_MUTEX_ATTR_CEILING && attribute != TN_MUTEX_ATTR_INHERIT)
      return TERR_WRONG_PARAM;
   if(attribute == TN_MUTEX_ATTR_CEILING &&
         (ceil_priority < 1 || ceil_priority > TN_NUM_PRIORITY-2))
      return TERR_WRONG_PARAM;

 //  TN_CHECK_NON_INT_CONTEXT

   queue_reset(&(mutex->wait_queue));
   queue_reset(&(mutex->mutex_queue));
   queue_reset(&(mutex->lock_mutex_queue));

   mutex->attr = attribute;

   mutex->holder = NULL;
   mutex->ceil_priority = ceil_priority;
   mutex->cnt = 0;
   mutex->id_mutex = TN_ID_MUTEX;

   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
int tn_mutex_delete(TN_MUTEX * mutex)
{
   TN_INTSAVE_DATA
   CDLL_QUEUE * que;
   TN_TCB * task;
   if(mutex == NULL)
      return TERR_WRONG_PARAM;
   if(mutex->id_mutex != TN_ID_MUTEX)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   //-- Remove all tasks(if any) from mutex's wait queue
   while(!is_queue_empty(&(mutex->wait_queue)))
   {
      if(tn_chk_irq_disabled() == 0) // int enable
         tn_disable_interrupt();
      que = queue_remove_head(&(mutex->wait_queue));
      task = get_task_by_tsk_queue(que);
    //-- If task in system's blocked list,remove
      remove_task_from_blocked_list(task);

      if(task_wait_complete(task,FALSE))
      {
         task->task_wait_rc = TERR_DLT;
         tn_enable_interrupt();
         tn_switch_context();
      }
   }

   //--

   if(tn_chk_irq_disabled() == 0) // int enable
      tn_disable_interrupt();

   //-- If mutex is locked,remove mutex task's locked mutexes queue
   if(mutex->holder != NULL)
   {
      queue_remove_entry(&(mutex->mutex_queue));
      queue_remove_entry(&(mutex->lock_mutex_queue));
   }
   mutex->id_mutex = 0; // Mutex not exists now

   tn_enable_interrupt();

   return TERR_NO_ERR;
}

//----------------------------------------------------------------------------
int tn_mutex_lock(TN_MUTEX * mutex,unsigned int timeout)
{
   TN_INTSAVE_DATA

   TN_TCB * blk_task;
   int rc;

   if(mutex == NULL || timeout == 0)
      return TERR_WRONG_PARAM;
   if(mutex->id_mutex != TN_ID_MUTEX)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   rc = TERR_NO_ERR;
   for(;;) //-- Single iteration loop
   {
      if(tn_curr_run_task == mutex->holder) //-- Recursive locking not enabled
      {
         rc = TERR_ILUSE;
         break;
      }
      if(mutex->attr == TN_MUTEX_ATTR_CEILING &&
         tn_curr_run_task->base_priority < mutex->ceil_priority) //-- base pri of task higher
      {
         rc = TERR_ILUSE;
         break;
      }

      if(mutex->attr == TN_MUTEX_ATTR_CEILING)
      {
         if(mutex->holder == NULL) //-- mutex not locked
         {
            if(enable_lock_mutex(tn_curr_run_task,&blk_task))
            {
               mutex->holder = tn_curr_run_task;
               tn_curr_run_task->blk_task = NULL;
               //-- Add mutex to task's locked mutexes queue
               queue_add_tail(&(tn_curr_run_task->mutex_queue),&(mutex->mutex_queue));
               //-- Add mutex to system's locked_mutexes_list
               queue_add_tail(&tn_locked_mutexes_list,&(mutex->lock_mutex_queue));
            }
            else  //-- Could not lock - blocking
            {
               //-- Base priority inheritance protocol ( if run_task's curr
                 // priority is higher task's priority that blocks run_task
                 // (mutex->holder) that mutex_holder_task inherits run_task
                 // priority
               if(blk_task != NULL)
               {
                  if(tn_curr_run_task->priority < blk_task->priority)
                     set_current_priority(blk_task,tn_curr_run_task->priority);
               }
               //-- Add task to system's blocked_tasks_list
               queue_add_tail(&tn_blocked_tasks_list,
                                       &(tn_curr_run_task->block_queue));

               tn_curr_run_task->blk_task = blk_task; //-- Store blocker

               task_curr_to_wait_action(&(mutex->wait_queue),
                                          TSK_WAIT_REASON_MUTEX_C_BLK,timeout);
               tn_enable_interrupt();
               tn_switch_context();
               return tn_curr_run_task->task_wait_rc;
            }
         }
         else //-- mutex  already locked
         {
               //-- Base priority inheritance protocol
            if(tn_curr_run_task->priority < mutex->holder->priority)
               set_current_priority(mutex->holder,tn_curr_run_task->priority);

                     //--- Task -> to the mutex wait queue
            task_curr_to_wait_action(&(mutex->wait_queue),
                                             TSK_WAIT_REASON_MUTEX_C,timeout);
            tn_enable_interrupt();
            tn_switch_context();
            return tn_curr_run_task->task_wait_rc;
         }
      }
      else if(mutex->attr == TN_MUTEX_ATTR_INHERIT)
      {
         if(mutex->holder == NULL) //-- mutex not locked
         {
            mutex->holder = tn_curr_run_task;
            queue_add_tail(&(tn_curr_run_task->mutex_queue),&(mutex->mutex_queue));
//--- v.2.3
            queue_add_tail(&tn_locked_mutexes_list,&(mutex->lock_mutex_queue));
//---
            rc = TERR_NO_ERR;
         }
         else //-- mutex already locked
         {
            //-- Base priority inheritance protocol
            //-- if run_task curr priority higher holder's curr priority
            if(tn_curr_run_task->priority < mutex->holder->priority)
               set_current_priority(mutex->holder,tn_curr_run_task->priority);

            task_curr_to_wait_action(&(mutex->wait_queue),
                                             TSK_WAIT_REASON_MUTEX_I,timeout);
            tn_enable_interrupt();
            tn_switch_context();
            return tn_curr_run_task->task_wait_rc;
         }
      }
      break;
   }
   tn_enable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
//-- Try to lock mutex
int tn_mutex_lock_polling(TN_MUTEX * mutex)
{
   TN_INTSAVE_DATA
   int rc;
   if(mutex == NULL)
      return TERR_WRONG_PARAM;
   if(mutex->id_mutex != TN_ID_MUTEX)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   rc = TERR_NO_ERR;
   for(;;) //-- Single iteration loop
   {
      if(tn_curr_run_task == mutex->holder) //-- Recursive locking not enabled
      {
         rc = TERR_ILUSE;
         break;
      }
      if(mutex->attr == TN_MUTEX_ATTR_CEILING && //-- base pri of task higher
         tn_curr_run_task->base_priority < mutex->ceil_priority)
      {
         rc = TERR_ILUSE;
         break;
      }
      if(mutex->holder == NULL) //-- mutex not locked
      {
         if(mutex->attr == TN_MUTEX_ATTR_CEILING)
         {
            if(enable_lock_mutex(tn_curr_run_task,NULL))
            {
               mutex->holder = tn_curr_run_task;
               tn_curr_run_task->blk_task = NULL;
                  //-- Add mutex to task's locked mutexes queue
               queue_add_tail(&(tn_curr_run_task->mutex_queue),&(mutex->mutex_queue));
                  //-- Add mutex to system's locked_mutexes_list
               queue_add_tail(&tn_locked_mutexes_list,&(mutex->lock_mutex_queue));
            }
            else
               rc = TERR_TIMEOUT;
         }
         else if(mutex->attr == TN_MUTEX_ATTR_INHERIT)
         {
            mutex->holder = tn_curr_run_task;
            queue_add_tail(&(tn_curr_run_task->mutex_queue),&(mutex->mutex_queue));
//--- v.2.3
            queue_add_tail(&tn_locked_mutexes_list,&(mutex->lock_mutex_queue));
//---
         }
      }
      else //-- mutex already locked
      {
         rc = TERR_TIMEOUT;
      }
      break;
   }
   tn_enable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
int tn_mutex_unlock(TN_MUTEX * mutex)
{
   TN_INTSAVE_DATA

   int rc;
   int need_switch_context;

   if(mutex == NULL)
      return TERR_WRONG_PARAM;
   if(mutex->id_mutex != TN_ID_MUTEX)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   need_switch_context = FALSE;
   rc = TERR_NO_ERR;
   for(;;) //-- Single iteration loop
   {
      //-- Unlocking is enabled only for owner and already locked mutex
      if(tn_curr_run_task != mutex->holder || mutex->holder == NULL)
      {
         rc = TERR_ILUSE;
         break;
      }
      need_switch_context = do_unlock_mutex(mutex);
     //-----
      if(need_switch_context)
      {
         tn_enable_interrupt();
         tn_switch_context();
         return TERR_NO_ERR;
      }
      break;
   }

   tn_enable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
//   Routines
//----------------------------------------------------------------------------
int do_unlock_mutex(TN_MUTEX * mutex)
{
   CDLL_QUEUE * curr_que;
   TN_MUTEX * tmp_mutex;
   TN_TCB * task;
   TN_TCB * hi_pri_task = NULL;
   TN_TCB * blk_task;
   TN_TCB * tmp_task;
   int priority;
   int pr;
   int need_switch_context;

   need_switch_context = FALSE;

   //-- Delete curr mutex from task's locked mutexes queue and
   //-- list of all locked mutexes
   queue_remove_entry(&(mutex->mutex_queue));
   queue_remove_entry(&(mutex->lock_mutex_queue));
   //---- No more mutexes,locked by our task
   if(is_queue_empty(&(tn_curr_run_task->mutex_queue)))
   {
      need_switch_context = set_current_priority(tn_curr_run_task,
                               tn_curr_run_task->base_priority);
   }
   else //-- there are another mutex(es) that curr running task locked
   {
      if(mutex->attr == TN_MUTEX_ATTR_INHERIT  ||
                           mutex->attr == TN_MUTEX_ATTR_CEILING)
      {
        //-- Find max priority among blocked tasks
           // (in mutexes's wait_queue that task still locks)
         pr = tn_curr_run_task->base_priority; //-- Start search value
         curr_que = tn_curr_run_task->mutex_queue.next;
         for(;;) //-- for each mutex that curr running task locks
         {
            tmp_mutex = get_mutex_by_mutex_queque(curr_que);
            pr = find_max_blocked_priority(tmp_mutex,pr);
            if(curr_que->next == &(tn_curr_run_task->mutex_queue)) //-- last
               break;
            else
               curr_que = curr_que->next;
         }
         //---
         if(pr != tn_curr_run_task->priority)
            need_switch_context = set_current_priority(tn_curr_run_task,pr);
      }
   }

   //--- Now try to lock this mutex
   if(is_queue_empty(&(mutex->wait_queue))) //-- No tasks that want lock mutex
   {
      mutex->holder = NULL;
      hi_pri_task = tn_curr_run_task;  //-- For another tasks unlocking
                                       //-- attempt only (TN_MUTEX_ATTR_CEILING)
   }
   else
   {
      if(mutex->attr == TN_MUTEX_ATTR_CEILING)
      {
        //-- Find task with max priority in mutex wait queue -----

         priority = TN_NUM_PRIORITY-1; //-- Minimal possible; may lock
         curr_que = mutex->wait_queue.next;
         for(;;) //-- for each task that is waiting for mutex
         {
            task = get_task_by_tsk_queue(curr_que);
            if(task->priority < priority) //  task priority is higher
            {
               hi_pri_task = task;
               priority = task->priority;
            }
            if(curr_que->next == &(mutex->wait_queue)) //-- last
                break;
            else
               curr_que = curr_que->next;
         }

         if(enable_lock_mutex(hi_pri_task,&blk_task))
         {
            //-- Remove from mutex wait queue
            queue_remove_entry(&(hi_pri_task->task_queue));
            //-- If was in locked tasks queue - remove
               //delete from blocked_tasks list
            remove_task_from_blocked_list(hi_pri_task);

            //--- Lock procedure ---

            //-- Make runnable (Inheritance Protocol - inside)
            if(task_wait_complete(hi_pri_task,FALSE))
               need_switch_context = TRUE;

            mutex->holder = hi_pri_task;
            hi_pri_task->blk_task = NULL;

            //-- Add mutex to task's locked mutexes queue
            queue_add_tail(&(hi_pri_task->mutex_queue),&(mutex->mutex_queue));
            //-- Add mutex to system's locked_mutexes_list
            queue_add_tail(&tn_locked_mutexes_list,&(mutex->lock_mutex_queue));
         }
         else  //-- Could not lock - hi_pri_task remainds blocked
         {
            //-- If task not in global lock queue
               //-- put it to this queue
               //-- change wait reason to TSK_WAIT_REASON_MUTEX_C_BLK
            if(!queue_contains_entry(&tn_blocked_tasks_list,
                                        &(hi_pri_task->block_queue)))
            {
               queue_add_tail(&tn_blocked_tasks_list,&(hi_pri_task->block_queue));
               hi_pri_task->task_wait_reason = TSK_WAIT_REASON_MUTEX_C_BLK;
               hi_pri_task->blk_task = blk_task;
            }
         }
    //---------------------------------------------------------------------
      }
      else if(mutex->attr == TN_MUTEX_ATTR_INHERIT)
      {
       //--- delete from mutex wait queue
         curr_que = queue_remove_head(&(mutex->wait_queue));
         task = get_task_by_tsk_queue(curr_que);
            //-- Make runnable (Inheritance Protocol - inside)
         if(task_wait_complete(task,FALSE))
            need_switch_context = TRUE;

         //-- Lock mutex by task that has been released from waiting
         mutex->holder = task;
         //-- Add mutex to task's locked mutexes queue
         queue_add_tail(&(task->mutex_queue),&(mutex->mutex_queue));
//--- v.2.3
         queue_add_tail(&tn_locked_mutexes_list,&(mutex->lock_mutex_queue));
      }
   }

   if(mutex->holder == NULL && mutex->attr == TN_MUTEX_ATTR_CEILING)
   {
      //-- mutex now unlocked so lock condition may changed,
      //-- try to lock another mutexes for tasks,that are now in block_queue

      curr_que = tn_blocked_tasks_list.next;
      for(;;) //-- for each task in list
      {
         tmp_task = get_task_by_block_queque(curr_que);
         if(tmp_task != hi_pri_task)
         {
            if(try_lock_mutex(tmp_task))
               need_switch_context = TRUE;
         }
         if(curr_que->next == &tn_blocked_tasks_list) //-- last
            break;
         else
            curr_que = curr_que->next;
      }
   }

   return need_switch_context;
}

//----------------------------------------------------------------------------
int find_max_blocked_priority(TN_MUTEX * mutex,int ref_priority)
{
   int priority;
   CDLL_QUEUE * curr_que;
   TN_TCB * task;

   priority = ref_priority;
   if(mutex->attr == TN_MUTEX_ATTR_INHERIT ||
        mutex->attr == TN_MUTEX_ATTR_CEILING)
   {
      if(!is_queue_empty(&(mutex->wait_queue)))
      {
         curr_que = mutex->wait_queue.next;
         for(;;) //-- for each task that is waiting for mutex
         {
            task = get_task_by_tsk_queue(curr_que);
            if(task->priority < priority) //  task priority is higher
               priority = task->priority;
            if(curr_que->next == &(mutex->wait_queue)) //-- last
               break;
            else
               curr_que = curr_que->next;
         }
      }
   }
   return priority;
}


//----------------------------------------------------------------------------
BOOL enable_lock_mutex(TN_TCB * curr_task,TN_TCB ** blk_task)
{
   TN_TCB * res_t;
   TN_TCB * tmp_task;
   TN_MUTEX * tmp_mutex;
   CDLL_QUEUE * curr_que;
   BOOL result;
   int priority;

   if(is_queue_empty(&tn_locked_mutexes_list))
   {
      if(blk_task != NULL)
         *blk_task = NULL;
      return TRUE;
   }

   result = TRUE;
   curr_que = tn_locked_mutexes_list.next;
   priority = 0; //-- Max possible
   res_t = NULL;
   for(;;) //-- for each locked mutex in system
   {
      tmp_mutex = get_mutex_by_lock_mutex_queque(curr_que);
      if(tmp_mutex->attr == TN_MUTEX_ATTR_CEILING)
      {
         tmp_task = tmp_mutex->holder;
         if(tmp_task != curr_task &&  // task has not strictly higher priority
            tmp_mutex->ceil_priority < curr_task->priority)
         {
            result = FALSE;
            if(tmp_mutex->ceil_priority > priority)// mutex has less priority
            {
               priority = tmp_mutex->ceil_priority;
               res_t = tmp_mutex->holder;
            }
         }
      }
      //--- Iteration
      if(curr_que->next == &tn_locked_mutexes_list) //-- last
         break;
      else
         curr_que = curr_que->next;
   }
 //------
   if(blk_task != NULL)
      *blk_task = res_t;
   return result;
}

//----------------------------------------------------------------------------
int try_lock_mutex(TN_TCB * task)
{
   TN_MUTEX * mutex;
   int need_switch_context;

   need_switch_context = FALSE;
   mutex = get_mutex_by_wait_queque(task->pwait_queue);
   if(mutex->holder == NULL)
   {
      if(enable_lock_mutex(task,NULL))
      {
        //--- Remove from waiting
              //-- delete from mutex wait queue
         queue_remove_entry(&(task->task_queue));
              //-- delete from blocked_tasks list (task must be there)
         queue_remove_entry(&(task->block_queue));

         if(task_wait_complete(task,FALSE)) //-- Inheritance Protocol - inside
            need_switch_context = TRUE;

         mutex->holder = task;
         task->blk_task = NULL;
         //-- Include in queues
         queue_add_tail(&(task->mutex_queue),&(mutex->mutex_queue));
         queue_add_tail(&tn_locked_mutexes_list,&(mutex->lock_mutex_queue));
      }
   }
   return need_switch_context;
}
//----------------------------------------------------------------------------
void remove_task_from_blocked_list(TN_TCB * task)
{
   CDLL_QUEUE * curr_que;
   CDLL_QUEUE * task_block_que;

   task_block_que = &(task->block_queue);
   curr_que = tn_blocked_tasks_list.next;
   for(;;) //-- for each task in list
   {
      if(curr_que == task_block_que)
      {
         queue_remove_entry(task_block_que);
         break;
      }
     //-- Iteration
      if(curr_que->next == &tn_blocked_tasks_list) //-- last
         break;
      else
         curr_que = curr_que->next;
   }
}
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
