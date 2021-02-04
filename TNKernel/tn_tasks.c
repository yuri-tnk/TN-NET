/*
TNKernel real-time kernel

Copyright © 2004, 2008 Yuri Tiomkin
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

/* ver 2.5 */

#include "tn.h"
#include "tn_port.h"
#include "tn_utils.h"

#define USE_ASM_FFS  1

//#define USE_MUTEXES  1

extern CDLL_QUEUE tn_blocked_tasks_list;

//-----------------------------------------------------------------------------
int tn_task_create(TN_TCB * task,           //-- task TCB
                 void (*task_func)(void *param),  //-- task function
                 int priority,                    //-- task priority
                 unsigned int * task_stack_start, //-- task stack first addr in memory (bottom)
                 int task_stack_size,             //-- task stack size (in sizeof(void*),not bytes)
                 void * param,                    //-- task function parameter
                 int option                       //-- Creation option
                 )
{
   TN_INTSAVE_DATA
   int rc; //-- return code

   unsigned int * ptr_stack;
   int i;

   //-- Light weight checking of system tasks recreation
   if((priority == 0 && ((option & TN_TASK_TIMER) == 0)) ||
       (priority == TN_NUM_PRIORITY-1 && (option & TN_TASK_IDLE) == 0))
      return TERR_WRONG_PARAM;

   if((priority < 0 || priority > TN_NUM_PRIORITY-1) ||
        task_stack_size < TN_MIN_STACK_SIZE ||
        task_func == NULL || task == NULL || task_stack_start == NULL ||
        task->id_task != 0)  //-- recreation
      return TERR_WRONG_PARAM;

   rc = TERR_NO_ERR;

   TN_CHECK_NON_INT_CONTEXT

   if(tn_system_state == TN_ST_STATE_RUNNING)
      tn_disable_interrupt();

   //--- Init task TCB

   task->task_func_addr = (void*)task_func;
   task->task_func_param = param;
   task->stk_start = (unsigned int*)task_stack_start;  //-- Base address of task stack space
   task->stk_size  = task_stack_size;         //-- Task stack size (in bytes)
   task->base_priority   = priority;          //-- Task base priority
   task->activate_count  = 0;                 //-- Activation request count
   task->id_task = TN_ID_TASK;

      //-- Fill all task stack space by 0xFFFFFFFF - only inside create_task
   for(ptr_stack = task->stk_start,i = 0;i < task->stk_size; i++)
      *ptr_stack-- = TN_FILL_STACK_VAL;

   task_set_dormant_state(task);

      //--- Init task stack
   ptr_stack = tn_stack_init(task->task_func_addr,
                             task->stk_start,
                             task->task_func_param);
   task->task_stk = ptr_stack;                //-- Pointer to task top of stack,when not running


   //-- Add task to created task queue
   queue_add_tail(&tn_create_queue,&(task->create_queue));
   tn_created_tasks_qty++;

   if((option & TN_TASK_START_ON_CREATION) != 0)
      task_to_runnable(task);

   if(tn_system_state == TN_ST_STATE_RUNNING)
      tn_enable_interrupt();

   return rc;
}

//----------------------------------------------------------------------------
//  If the task is runnable, it is moved to the SUSPENDED state. If the task
//  is in the WAITING state, it is moved to the WAITING­SUSPENDED state.
//----------------------------------------------------------------------------
int tn_task_suspend(TN_TCB * task)
{
   TN_INTSAVE_DATA
   int rc; //-- return code

   //-- ToDo: validation for input parameter etc.
   if(task == NULL)
      return  TERR_WRONG_PARAM;
   if(task->id_task != TN_ID_TASK)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   if(task->task_state & TSK_STATE_SUSPEND)
      rc = TERR_OVERFLOW;
   else
   {
      if(task->task_state == TSK_STATE_DORMANT)
         rc = TERR_WSTATE;
      else
      {
         if(task == tn_curr_run_task && tn_enable_switch_context == 0)
            rc =  TERR_WCONTEXT;
         else
         {
            if(task->task_state == TSK_STATE_RUNNABLE)
            {
               task->task_state = TSK_STATE_SUSPEND;
               task_to_non_runnable(task);
               tn_enable_interrupt();
               if(tn_next_task_to_run != tn_curr_run_task &&
                                                 tn_enable_switch_context != 0)
                  tn_switch_context();
               return  TERR_NO_ERR;
            }
            else
            {
               task->task_state |= TSK_STATE_SUSPEND;
               rc = TERR_NO_ERR;
            }
         }
      }
   }
   tn_enable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
int tn_task_resume(TN_TCB * task)
{
   TN_INTSAVE_DATA
   int rc; //-- return code

   if(task == NULL)
      return  TERR_WRONG_PARAM;
   if(task->id_task != TN_ID_TASK)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   if(!(task->task_state & TSK_STATE_SUSPEND))
      rc = TERR_WSTATE;
   else
   {
      if(!(task->task_state & TSK_STATE_WAIT)) // Task not in the WAIT-SUSPEND state
      {
         task_to_runnable(task);
         tn_enable_interrupt();
         if(tn_next_task_to_run != tn_curr_run_task &&
                                                 tn_enable_switch_context != 0)
            tn_switch_context();
         return TERR_NO_ERR;
      }
      else  //-- Just remove TSK_STATE_SUSPEND from  task state
      {
         task->task_state &= ~TSK_STATE_SUSPEND;
         rc = TERR_NO_ERR;
      }
   }
   tn_enable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
//-- Put current running task to sleep mode
//           (TN_WAIT_INFINITE) -
//                the function's time-out interval never elapses.
//  if wakeup_count > 0, wakeup_count--  and running task continues execution
//----------------------------------------------------------------------------
int tn_task_sleep(unsigned int timeout)
{
   TN_INTSAVE_DATA
   int rc; //-- return code

   if(timeout == 0)
      return  TERR_WRONG_PARAM;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   if(tn_curr_run_task->wakeup_count > 0)
   {
      tn_curr_run_task->wakeup_count--;
      rc = TERR_NO_ERR;
   }
   else
   {
      task_curr_to_wait_action(NULL,TSK_WAIT_REASON_SLEEP,timeout);
      tn_enable_interrupt();
      tn_switch_context();
      return  TERR_NO_ERR;
   }

   tn_enable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
//-- Wake-up task
int tn_task_wakeup(TN_TCB * task)
{
   TN_INTSAVE_DATA
   int rc; //-- return code

   if(task == NULL)
      return  TERR_WRONG_PARAM;
   if(task->id_task != TN_ID_TASK)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   if(task->task_state == TSK_STATE_DORMANT)
   {
      rc = TERR_WCONTEXT;
   }
   else
   {
      if((task->task_state & TSK_STATE_WAIT) &&
                 task->task_wait_reason == TSK_WAIT_REASON_SLEEP)
      {
         if(task_wait_complete(task,FALSE))
         {
            tn_enable_interrupt();
            tn_switch_context();
            return TERR_NO_ERR;
         }
         rc = TERR_NO_ERR;
      }
      else
      {
         if(tn_curr_run_task->wakeup_count == 0) //-- if here - task in not in
         {                                              //-- the SLEEP mode
            tn_curr_run_task->wakeup_count++;
            rc = TERR_NO_ERR;
         }
         else
            rc = TERR_OVERFLOW;
      }
   }
   tn_enable_interrupt();
   return rc;
}
//----------------------------------------------------------------------------
//-- Wake-up task from interrupts
int tn_task_iwakeup(TN_TCB * task)
{
   TN_INTSAVE_DATA_INT
   int rc; //-- return code

   if(task == NULL)
      return TERR_WRONG_PARAM;
   if(task->id_task != TN_ID_TASK)
      return TERR_NOEXS;

   TN_CHECK_INT_CONTEXT

   tn_idisable_interrupt();

   if(task->task_state == TSK_STATE_DORMANT)
   {
      rc = TERR_WCONTEXT;
   }
   else
   {
      if((task->task_state & TSK_STATE_WAIT) &&
                 task->task_wait_reason == TSK_WAIT_REASON_SLEEP)
      {
         if(task_wait_complete(task,FALSE))
         {
            tn_context_switch_request = TRUE;
            tn_ienable_interrupt();
            return TERR_NO_ERR;
         }
         rc = TERR_NO_ERR;
      }
      else
      {
         if(tn_curr_run_task->wakeup_count == 0) //-- if here - task in not in
         {                                              //-- the SLEEP mode
            tn_curr_run_task->wakeup_count++;
            rc = TERR_NO_ERR;
         }
         else
            rc = TERR_OVERFLOW;
      }
   }
   tn_ienable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
int tn_task_activate(TN_TCB * task)
{
   TN_INTSAVE_DATA
   int rc; //-- return code

   if(task == NULL)
      return  TERR_WRONG_PARAM;
   if(task->id_task != TN_ID_TASK)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   if(task->task_state == TSK_STATE_DORMANT)
   {
      task_to_runnable(task);
      tn_enable_interrupt();
      if(tn_next_task_to_run != tn_curr_run_task &&
                                               tn_enable_switch_context != 0)
            tn_switch_context();
      return TERR_NO_ERR;
   }
   else
   {
      if(task->activate_count == 0)
      {
         task->activate_count++;
         rc = TERR_NO_ERR;
      }
      else
         rc = TERR_OVERFLOW;
   }
   tn_enable_interrupt();
   return rc;
}
//----------------------------------------------------------------------------
int tn_task_iactivate(TN_TCB * task)
{
   TN_INTSAVE_DATA_INT
   int rc; //-- return code

   if(task == NULL)
      return  TERR_WRONG_PARAM;
   if(task->id_task != TN_ID_TASK)
      return TERR_NOEXS;

   TN_CHECK_INT_CONTEXT

   tn_idisable_interrupt();

   if(task->task_state == TSK_STATE_DORMANT)
   {
      task_to_runnable(task);
      if(tn_next_task_to_run != tn_curr_run_task &&
                                                tn_enable_switch_context != 0)
         tn_context_switch_request = TRUE;
      tn_ienable_interrupt();
      return TERR_NO_ERR;
   }
   else
   {
      if(task->activate_count == 0)
      {
         task->activate_count++;
         rc = TERR_NO_ERR;
      }
      else
         rc = TERR_OVERFLOW;
   }
   tn_ienable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
//-- Release task from waiting
int tn_task_release_wait(TN_TCB * task)
{
   TN_INTSAVE_DATA
   int rc; //-- return code

   //-- ToDo: validation for input parameter etc.
   if(task == NULL)
      return  TERR_WRONG_PARAM;
   if(task->id_task != TN_ID_TASK)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   if((task->task_state & TSK_STATE_WAIT) == 0)
   {
      rc = TERR_WCONTEXT;
   }
   else
   {
      if(task_wait_complete(task,TRUE))
      {
         tn_enable_interrupt();
         tn_switch_context();
         return TERR_NO_ERR;
      }
      rc = TERR_NO_ERR;
   }
   tn_enable_interrupt();
   return rc;


}

//----------------------------------------------------------------------------
int tn_task_irelease_wait(TN_TCB * task)
{
   TN_INTSAVE_DATA_INT
   int rc; //-- return code

   //-- ToDo: validation for input parameter etc.
   if(task == NULL)
      return  TERR_WRONG_PARAM;
   if(task->id_task != TN_ID_TASK)
      return TERR_NOEXS;

   TN_CHECK_INT_CONTEXT
   tn_idisable_interrupt();

   if((task->task_state & TSK_STATE_WAIT) == 0)
      rc = TERR_WCONTEXT;
   else
   {
      if(task_wait_complete(task,TRUE))
         tn_context_switch_request = TRUE;
      rc = TERR_NO_ERR;
   }

   tn_ienable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
void tn_task_exit(int attr)
{
#ifdef USE_MUTEXES
   CDLL_QUEUE * que;
   TN_MUTEX * mutex;
#endif
   TN_TCB * task;
   unsigned int * ptr_stack;
   volatile int stack_exp[TN_PORT_STACK_EXPAND_AT_EXIT]; //-- For GCC ver > 4 only

   TN_CHECK_INT_CONTEXT_NORETVAL

   tn_cpu_save_sr();  //-- Disable interrupts without saving SPSR

   //-- To use stack_exp[] and avoid warning message
   stack_exp[0] = (int)tn_cpu_save_sr; //-- For GCC ver > 4 only
   ptr_stack =(unsigned int *)stack_exp[0];
   //--------------------------------------------------

  // Unlock all mutexes locked by the task
#ifdef USE_MUTEXES
   while(!is_queue_empty(&(tn_curr_run_task->mutex_queue)))
   {
      que = queue_remove_head(&(tn_curr_run_task->mutex_queue));
      mutex = get_mutex_by_mutex_queque(que);
      do_unlock_mutex(mutex);
   }
#endif

   task = tn_curr_run_task;
   task_to_non_runnable(tn_curr_run_task);

   task_set_dormant_state(task);
   ptr_stack = tn_stack_init(task->task_func_addr,
                             task->stk_start,
                             task->task_func_param);
   task->task_stk = ptr_stack;  //-- Pointer to task top of stack,when not running

   if(task->activate_count > 0)  //-- Cannot exit
   {
      task->activate_count--;
      task_to_runnable(task);
   }
//-- If need - delete task
   if(attr == TN_EXIT_AND_DELETE_TASK)
   {
      queue_remove_entry(&(task->create_queue));
      tn_created_tasks_qty--;
      task->id_task = 0;
   }
   //-- v.2.4
   // interrupts will be enabled inside tn_switch_context_exit()
   tn_switch_context_exit();
}

//-----------------------------------------------------------------------------
int tn_task_terminate(TN_TCB * task)
{
   TN_INTSAVE_DATA

   int rc; //-- return code
   unsigned int * ptr_stack;
#ifdef USE_MUTEXES
   CDLL_QUEUE * que;
   TN_MUTEX * mutex;
#endif
   volatile int stack_exp[TN_PORT_STACK_EXPAND_AT_EXIT]; //-- For GCC ver > 4 only

   if(task == NULL)
      return  TERR_WRONG_PARAM;
   if(task->id_task != TN_ID_TASK)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   //-- To use stack_exp[] and avoid warning message
   stack_exp[0] = (int)tn_cpu_save_sr; //-- For GCC ver > 4 only
   ptr_stack =(unsigned int *)stack_exp[0];
   //--------------------------------------------------

   rc = TERR_NO_ERR;
   if(task->task_state == TSK_STATE_DORMANT ||
        tn_curr_run_task == task) //-- Cannot terminate running task
      rc = TERR_WCONTEXT;
   else
   {
      if(task->task_state == TSK_STATE_RUNNABLE)
         task_to_non_runnable(task);
      else if(task->task_state & TSK_STATE_WAIT)
      {
         //-- Free all queues involved in waiting
         queue_remove_entry(&(task->task_queue));
         if(queue_contains_entry(&tn_blocked_tasks_list,&(task->block_queue)))
            queue_remove_entry(&(task->block_queue));
         //-----------------------------------------
         if(task->tick_count != TN_WAIT_INFINITE)
            queue_remove_entry(&(task->timer_queue));
      }

     //-- Unlock all mutexes locked by the task
#ifdef USE_MUTEXES
      while(!is_queue_empty(&(task->mutex_queue)))
      {
         que = queue_remove_head(&(task->mutex_queue));
         mutex = get_mutex_by_mutex_queque(que);
         do_unlock_mutex(mutex);
      }
#endif

      task_set_dormant_state(task);
      ptr_stack = tn_stack_init(task->task_func_addr,
                             task->stk_start,
                             task->task_func_param);
      task->task_stk = ptr_stack;  //-- Pointer to task top of stack,when not running

      if(task->activate_count > 0) //-- Cannot terminate
      {
         task->activate_count--;
         task_to_runnable(task);
         tn_enable_interrupt();
         if(tn_next_task_to_run != tn_curr_run_task &&
                                             tn_enable_switch_context != 0)
            tn_switch_context();
         return TERR_NO_ERR;
      }
   }
   tn_enable_interrupt();
   return rc;
}

//-----------------------------------------------------------------------------
int tn_task_delete(TN_TCB * task)
{
   TN_INTSAVE_DATA
   int rc; //-- return code

   if(task == NULL)
      return TERR_WRONG_PARAM;
   if(task->id_task != TN_ID_TASK)
      return TERR_NOEXS;

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   rc = TERR_NO_ERR;
   for(;;) //-- Single iteration loop
   {
      if(task->task_state != TSK_STATE_DORMANT) //-- Cannot delete not-terminated task
      {
         rc = TERR_WCONTEXT;
         break;
      }
      queue_remove_entry(&(task->create_queue));
      tn_created_tasks_qty--;
      task->id_task = 0;
      break;
   }
   tn_enable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
int tn_task_change_priority(TN_TCB * task, int new_priority)
{
   TN_INTSAVE_DATA
   int rc; //-- return code

   if(task == NULL)
      return  TERR_WRONG_PARAM;
   if(task->id_task != TN_ID_TASK)
      return TERR_NOEXS;
   if(new_priority < 0 || new_priority > TN_NUM_PRIORITY-2) //-- try to set pri
      return TERR_WRONG_PARAM;                                // reserved by sys

   TN_CHECK_NON_INT_CONTEXT

   tn_disable_interrupt();

   if(new_priority == 0)
      new_priority = task->base_priority;

   rc = TERR_NO_ERR;

   if(task->task_state == TSK_STATE_DORMANT)
      rc = TERR_WCONTEXT;
   else if(task->task_state == TSK_STATE_RUNNABLE)
   {
      if(change_running_task_priority(task,new_priority))
      {
         tn_enable_interrupt();
         tn_switch_context();
         return TERR_NO_ERR;
      }
   }
   else
   {
      task->priority = new_priority;
   }
   tn_enable_interrupt();
   return rc;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
void find_next_task_to_run(void)
{
   int tmp;
#ifndef USE_ASM_FFS
   int i;
   unsigned int mask;
#endif

#ifdef USE_ASM_FFS
   tmp = ffs_asm(tn_ready_to_run_bmp);
   tmp--;
#elif
   mask = 1;
   tmp = 0;
   for(i=0;i<TN_BITS_IN_INT;i++)  //-- for each bit in bmp
   {
      if(tn_ready_to_run_bmp & mask)
      {
         tmp = i;
         break;
      }
      mask = mask<<1;
   }
#endif
   tn_next_task_to_run = get_task_by_tsk_queue(tn_ready_list[tmp].next);
}

//-----------------------------------------------------------------------------
void task_to_non_runnable(TN_TCB * task)
{
   int priority;
   CDLL_QUEUE * que;

   priority = task->priority;
   que = &(tn_ready_list[priority]);

   //-- remove curr task from  any queue (have to be from ready queue now)
   queue_remove_entry(&(task->task_queue));

   if(is_queue_empty(que))  //-- If there are no ready tasks for curr priority
   {
      tn_ready_to_run_bmp &= ~(1<<priority); //-- remove ready from curr priority
      //-- Find highest priority ready to run
      if(tn_ready_to_run_bmp == 0) //-- no ready to run tasks
         tn_next_task_to_run = NULL;
      else //-- There are tasks ready to run
         find_next_task_to_run();
   }
   else //-- There are ready task(s) for curr priority
   {
      if(tn_next_task_to_run == task)
      {
         tn_next_task_to_run = get_task_by_tsk_queue(que->next);
      }
   }
}

//-----------------------------------------------------------------------------
void task_to_runnable(TN_TCB * task)
{
   int priority;

   priority = task->priority;
   task->task_state = TSK_STATE_RUNNABLE;       //-- v.2.3
   task->pwait_queue = NULL;
   //-- Add task to the end of ready queue for current priority
   queue_add_tail(&(tn_ready_list[priority]), &(task->task_queue));
   tn_ready_to_run_bmp |= 1 << priority;

   //-- prior 0 - max priority,so '<' operation are here
   if(tn_next_task_to_run == NULL || (tn_next_task_to_run != NULL &&
          (priority < tn_next_task_to_run->priority)))
   {
      tn_next_task_to_run = task;
   }
}

//----------------------------------------------------------------------------
int  task_wait_complete(TN_TCB * task,int tqueue_remove_enable) //-- v. 2.3
{
   int rc;

#ifdef USE_MUTEXES
   int fl_mutex;
   int curr_priority;
   TN_MUTEX * mutex;
   TN_TCB * tmp_task;
   CDLL_QUEUE * t_que;
#endif


   if(task == NULL)
      return 0;

   rc = FALSE;
#ifdef USE_MUTEXES
   t_que = NULL;
   if(task->task_wait_reason == TSK_WAIT_REASON_MUTEX_I ||
      task->task_wait_reason == TSK_WAIT_REASON_MUTEX_C ||
      task->task_wait_reason == TSK_WAIT_REASON_MUTEX_C_BLK)
   {
      fl_mutex = TRUE;
      t_que = task->pwait_queue;
   }
   else
      fl_mutex = FALSE;
#endif

   if(tqueue_remove_enable == TRUE)  //-- v. 2.3
      queue_remove_entry(&(task->task_queue));

   task->pwait_queue = NULL;
   task->task_wait_rc = TERR_NO_ERR;

   if(task->tick_count != TN_WAIT_INFINITE )
      queue_remove_entry(&(task->timer_queue));
   task->tick_count = TN_WAIT_INFINITE;

   if(!(task->task_state & TSK_STATE_SUSPEND))
   {
      task_to_runnable(task);
      rc = TRUE;
   }
   else  //-- remove WAIT state
      task->task_state = TSK_STATE_SUSPEND;


#ifdef USE_MUTEXES

   if(fl_mutex == TRUE)
   {
      mutex = get_mutex_by_wait_queque(t_que);
      if(task->task_wait_reason == TSK_WAIT_REASON_MUTEX_C_BLK)
      {
         tmp_task = task->blk_task;
         //-- v.2.5
         if(queue_contains_entry(&tn_blocked_tasks_list, &(task->block_queue)))
            queue_remove_entry(&(task->block_queue));
      }
      else
         tmp_task = mutex->holder;
      if(tmp_task != NULL)
      {
         //-- if task was blocked by another task and its pri was changed
            //  - recalc current priority
         if(tmp_task->priority != tmp_task->base_priority &&
            tmp_task->priority == task->priority)
         {
            curr_priority = find_max_blocked_priority(mutex,
                                            tmp_task->base_priority);
            if(set_current_priority(tmp_task,curr_priority))
               rc = TRUE;
         }
      }
   }
#endif

   task->task_wait_reason = 0; //-- v2.5
   return rc;
}


//----------------------------------------------------------------------------
void task_curr_to_wait_action(CDLL_QUEUE * wait_que,int wait_reason,
                                                         unsigned int timeout)
{
   task_to_non_runnable(tn_curr_run_task);
   tn_curr_run_task->task_state = TSK_STATE_WAIT;
   tn_curr_run_task->task_wait_reason = wait_reason;
   tn_curr_run_task->tick_count = timeout;
   //--- Add to  wait queue  - FIFO
   if(wait_que == NULL) //-- Thanks to Vavilov D.O.
   {
      queue_reset(&(tn_curr_run_task->task_queue));
   }
   else
   {
      queue_add_tail(wait_que,&(tn_curr_run_task->task_queue));
      tn_curr_run_task->pwait_queue = wait_que;
   }
   //--- Add to timers queue
   if(timeout != TN_WAIT_INFINITE)
      queue_add_tail(&tn_wait_timeout_list,&(tn_curr_run_task->timer_queue));
}

//----------------------------------------------------------------------------
int change_running_task_priority(TN_TCB * task, int new_priority)
{
   int rc;
   int old_priority;
   CDLL_QUEUE * que;

   rc = 0;
   old_priority = task->priority;
   que = &(tn_ready_list[old_priority]);

  //-- remove curr task from any (wait/ready) queue
   queue_remove_entry(&(task->task_queue));
   if(is_queue_empty(que))  //-- If there are no ready tasks for old priority
      tn_ready_to_run_bmp &= ~(1<<old_priority); //-- clear ready bit for old priority

   task->priority = new_priority;

     //-- Add task to the end of ready queue for current priority
   queue_add_tail(&(tn_ready_list[new_priority]), &(task->task_queue));
   tn_ready_to_run_bmp |= 1 << new_priority;
   if(tn_next_task_to_run == task)
   {
      if(new_priority >= old_priority)  //-- new less or the same as old
      {
         find_next_task_to_run();
         if(tn_next_task_to_run != task)
            rc = tn_enable_switch_context;
      }
   }
   else
   {
      if(new_priority < tn_next_task_to_run->priority) //-- new is higher
      {
         tn_next_task_to_run = task;
         rc = tn_enable_switch_context;  //-- need switch context
      }
   }
   return rc;
}

//----------------------------------------------------------------------------
int set_current_priority(TN_TCB * task, int priority)
{
   TN_TCB * curr_task;
#ifdef USE_MUTEXES
   TN_MUTEX * mutex;
#endif
   int rc;
   int old_priority;
   int new_priority;

   //-- transitive priority changing

   // if we have a task A that is blocked by the task B and we changed priority
   // of task A,priority of task B also have to be changed. I.e, if we have
   // the task A that is waiting for the mutex M1 and we changed priority
   // of this task, a task B that holds a mutex M1 now, also needs priority's
   // changing.  Then, if a task B now is waiting for the mutex M2, the same
   // procedure have to be applied to the task C that hold a mutex M2 now
   // and so on.

   rc = 0;
   curr_task = task;
   new_priority = priority;
   for(;;)
   {
      old_priority = curr_task->priority;
      if(old_priority == new_priority)
         break;
      if(curr_task->task_state == TSK_STATE_RUNNABLE)
      {
         rc = change_running_task_priority(curr_task,new_priority);
         break;
      }
      else if(curr_task->task_state & TSK_STATE_WAIT)
      {
         if(curr_task->task_wait_reason == TSK_WAIT_REASON_MUTEX_I ||
            curr_task->task_wait_reason == TSK_WAIT_REASON_MUTEX_C ||
            curr_task->task_wait_reason == TSK_WAIT_REASON_MUTEX_C_BLK)

         {

#ifdef USE_MUTEXES
            //-- Find task that blocks curr task ( holder of mutex,that
              // curr_task wait)
            mutex = get_mutex_by_wait_queque(curr_task->pwait_queue);
            if(curr_task->task_wait_reason == TSK_WAIT_REASON_MUTEX_C_BLK)
               curr_task = curr_task->blk_task;
            else
               curr_task = mutex->holder;

            if(old_priority < new_priority)  // old higher
                 new_priority = find_max_blocked_priority(mutex,
                                             curr_task->base_priority);
#endif
         }
         else
         {
            task->priority = new_priority;
            break;
         }
      }
      else
      {
         task->priority = new_priority;
         break;
      }
   }
   return rc;
}

//----------------------------------------------------------------------------
void task_set_dormant_state(TN_TCB* task)
{
   queue_reset(&(task->task_queue));
   queue_reset(&(task->timer_queue));
   queue_reset(&(task->create_queue));
   queue_reset(&(task->mutex_queue));
   queue_reset(&(task->block_queue));

   task->pwait_queue = NULL;
   task->blk_task = NULL;

   task->priority   = task->base_priority;  //-- Task curr priority
   task->task_state = TSK_STATE_DORMANT;    //-- Task state
   task->task_wait_reason = 0;              //-- Reason for waiting
   task->task_wait_rc  = TERR_NO_ERR;
   task->ewait_pattern = 0;                 //-- Event wait pattern
   task->ewait_mode    = 0;                 //-- Event wait mode:  _AND or _OR
   task->data_elem     = NULL;              //-- Store data queue entry,if data queue is full

   task->tick_count = TN_WAIT_INFINITE;     //-- Remaining time until timeout
   task->wakeup_count   = 0;                //-- Wakeup request count
   task->suspend_count  = 0;                //-- Suspension count

   task->tslice_count = 0;
}
//---

