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
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

 /*
 *  Random number generator - based on George Marsaglia "A very fast and very
 *                                              good random number generator"
 *  http://groups.google.com/group/sci.math/msg/fa034083b193adf0?hl=en&lr&ie=UTF-8&pli=1
 */

#include <string.h>  //--
#include <stdlib.h>

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
#include "tn_net_func.h"

#include "dbg_func.h"



//-- sizeof(word) MUST BE A POWER OF TWO SO THAT wmask BELOW IS ALL ONES

typedef long word;                // "word" used for optimal copy speed


#define        wsize        sizeof(word)
#define        wmask        (wsize - 1)

//----------------------------------------------------------------------------
void bzero(void * b, int length)
{
   unsigned char * p;

   for(p = (unsigned char *)b; length--;)
      *p++ = 0;
}

//----------------------------------------------------------------------------
void bzero_dw(unsigned int * ptr, int len)
{
   unsigned int * p;

   for(p = ptr; len--;)
      *p++ = 0;
}

//----------------------------------------------------------------------------
// Copy a block of memory, handling overlap.
// This is the routine that actually implements
// (the portable versions of) bcopy, memcpy, and memmove.
//----------------------------------------------------------------------------
void bcopy(const void * src0, void * dst0, int length)
{
   char * dst = (char * )dst0;
   const char * src = (const char * )src0;
   int t;

   if(length == 0 || dst == src)                /* nothing to do */
      goto done;

      //-- Macros: loop-t-times; and loop-t-times, t>0

#define TLOOP(s)   if (t) TLOOP1(s)
#define  TLOOP1(s) do { s; } while (--t)

   if((unsigned long)dst < (unsigned long)src)
   {

      //-- Copy forward.

      t = (long)src;                  // only need low bits
      if((t | (long)dst) & wmask)
      {

         //-- Try to align operands.  This cannot be done
         //-- unless the low bits match.

         if((t ^ (long)dst) & wmask || length < wsize)
            t = length;
         else
            t = wsize - (t & wmask);
         length -= t;
         TLOOP1(*dst++ = *src++);
      }

      //-- Copy whole words, then mop up any trailing bytes.

      t = length / wsize;
      TLOOP(*(word *)dst = *(word *)src; src += wsize; dst += wsize);
      t = length & wmask;
      TLOOP(*dst++ = *src++);
   }
   else
   {
      //-- Copy backwards.  Otherwise essentially the same.
      //-- Alignment works as before, except that it takes
      //-- (t&wmask) bytes to align, not wsize-(t&wmask).

      src += length;
      dst += length;
      t = (long)src;
      if((t | (long)dst) & wmask)
      {
         if((t ^ (long)dst) & wmask || length <= wsize)
            t = length;
         else
            t &= wmask;
         length -= t;
         TLOOP1(*--dst = *--src);
      }
      t = length / wsize;
      TLOOP(src -= wsize; dst -= wsize; *(word *)dst = *(word *)src);
      t = length & wmask;
      TLOOP(*--dst = *--src);
   }

done:

   return;
}

//----------------------------------------------------------------------------
// Insert 'entry' after 'ins_after_entry'
//----------------------------------------------------------------------------
void tn_np_que_insert_entry(struct tn_np_que * entry,
                            struct tn_np_que * ins_after_entry)
{
   struct tn_np_que * next_entry;

   next_entry = ins_after_entry->next;
   ins_after_entry->next = entry;
   next_entry->prev = entry;

   entry->next = next_entry;
   entry->prev = ins_after_entry;
}

//----------------------------------------------------------------------------
void tn_np_que_remove_entry(struct tn_np_que * entry)
{
   entry->prev->next = entry->next;
   entry->next->prev = entry->prev;
}

//----------------------------------------------------------------------------
int tn_socket_wait(TN_SEM * t_sem)// so->wait_sem);
{
   tn_sem_acquire(t_sem, TN_WAIT_INFINITE);
   return 0;
}

//----------------------------------------------------------------------------
void tn_net_panic(int event_code)
{
   char buf[16];

   tn_snprintf(buf, 15,"%d\r\n", event_code);
   dbg_send(buf);
   for(;;)
   {
   }
}

//----------------------------------------------------------------------------
void tn_net_panic_int(int event_code)
{
   char buf[16];

   buf[0] = 0;
   switch(event_code)
   {
      case INV_MEM_VAL_I_1:
         strcpy(buf,"--1--\r\n");
         break;
      case INV_MEM_VAL_I_2:
         strcpy(buf,"--2--\r\n");
         break;
      case INV_MEM_VAL_I_3:
         strcpy(buf,"--3--\r\n");
         break;
   }

   dbg_send_int(buf);
   for(;;)
   {
   }
}

//----------------------------------------------------------------------------
void tn_net_wakeup(TN_SEM * sem)
{
   tn_sem_signal(sem);
}

//----------------------------------------------------------------------------
int tn_net_wait(TN_SEM * sem, unsigned int timeout)
{
  return tn_sem_acquire(sem, timeout ? timeout : TN_WAIT_INFINITE);
}

//----------------------------------------------------------------------------
void * s_memset(void * dst,  int ch, int length)
{
   register unsigned char * ptr = (unsigned char *)dst - 1;

   while(length--)
      *++ptr = ch;

   return dst;
}

//----------------------------------------------------------------------------
int s_memcmp(const void * buf1, const void * buf2, int count)
{
   while( --count && *(unsigned char *)buf1 == *(unsigned char *)buf2)
   {
      buf1 = (unsigned char *)buf1 + 1;
      buf2 = (unsigned char *)buf2 + 1;
   }

   return( *((unsigned char *)buf1) - *((unsigned char *)buf2));
}

//----------------------------------------------------------------------------
//  ip4_addr - in host order
//----------------------------------------------------------------------------
int ip4_str_to_num(char * str , unsigned int * ip4_addr)
{
   char dig_arr[8];

   char ch;
   //int i;
   unsigned int val;

   char * ptr = str;
   int state = 0;
   int digit_cnt = 0;
   int dot_cnt = 0;
   int fErr = TRUE;
   unsigned int res = 0;

   for(;;)
   {
      ch = *ptr++;
      if(ch == ' ' || ch == '\t') //-- Skip whitespace
      {
        // dot_cnt   = 0;
        // digit_cnt = 0;

      }
      else if(ch >= 0x30 && ch <= 0x39)  //-- Digits
      {
         dot_cnt = 0;

         dig_arr[digit_cnt] = ch;

         digit_cnt++;
         if(digit_cnt > 3)
            break; //-- err

      }
      else if(ch == '.') //-- dot is separator only
      {
         dot_cnt++;
         if(dot_cnt > 1)
            break;      //--- Err

         dig_arr[digit_cnt] = '\0';
         digit_cnt = 0;

         val = atoi(dig_arr);
         if(val > 255)
            break; //-- Err

        // for(i=0;i< 3-state; i++)
        //    val <<= 8;
        //-- this code is faster
         switch(state)
         {
            case 0:
               val <<= 24;
               break;
            case 1:
               val <<= 16;
               break;
            case 2:
               val <<= 8;
               break;
            default:    //-- sanity chk
               return FALSE;
         }
         res |= val;

         state++;
      }
      else //-- any other character
      {
         if(ch == '\0') //-- End of string
         {
            if(digit_cnt > 0 && state == 3)
            {
               dig_arr[digit_cnt] = '\0';
               val = atoi(dig_arr);
               if(val > 255)
                  break; //-- Err
               res |= val;

               fErr = FALSE; //-- Syntax is OK
            }
         }
         break;
      }
   }
 //---------------
   if(fErr == FALSE) //-- Syntax is OK
   {
      *ip4_addr = res;
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------
//  ip4_addr - in host order

void ip4_num_to_str(char * str , unsigned int ip4_addr)
{
  tn_snprintf(str,18, "%d.%d.%d.%d",
             (ip4_addr >> 24) & 0xFF,
             (ip4_addr >> 16) & 0xFF,
             (ip4_addr >> 8)  & 0xFF,
             ip4_addr & 0xFF);
}

//----------------------------------------------------------------------------
// multiply-with-carry generator x(n) = a*x(n-1) + carry mod 2^32.
// period = (a*2^31)-1

//-- Choose a value for a from this list
 //  1791398085 1929682203 1683268614 1965537969 1675393560
 //  1967773755 1517746329 1447497129 1655692410 1606218150
 //  2051013963 1075433238 1557985959 1781943330 1893513180
 //  1631296680 2131995753 2083801278 1873196400 1554115554

//static unsigned int a=1791398085;
//static unsigned int x=30903, c=0, ah, al;


//----------------------------------------------------------------------------
void tn_srand(RNDMWC * mwc, unsigned char * hw_addr)
{
   unsigned int xi; 
   unsigned int ci;

   memcpy(&xi, &hw_addr[2], 4);
   memcpy(&ci, &hw_addr[0], 2);
   memcpy(((char*)&ci) + 2, &hw_addr[0], 2);

//----------------------------------------------
   mwc->a = 1791398085;
   mwc->x = 30903;
   mwc->c = 0;

   if(xi != 0)
     mwc->x = xi;
   mwc->c = ci;
   mwc->ah = mwc->a >> 16;
   mwc->al = mwc->a & 0xFFFF;
}

//----------------------------------------------------------------------------
unsigned int tn_rand(RNDMWC * mwc)
{
   unsigned int xh = mwc->x >> 16;
   unsigned int xl = mwc->x & 0xFFFF;

   mwc->x = mwc->x * mwc->a + mwc->c;
   mwc->c = xh * mwc->ah + ((xh * mwc->al) >> 16) + ((xl * mwc->ah) >> 16);
   if(xl * mwc->al >= ~mwc->c + 1)
      mwc->c++;  //-- thanks to Don Mitchell for this correction
   return mwc->x;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------





