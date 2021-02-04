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

#ifndef  _TN_PORT_H_
#define  _TN_PORT_H_

//#include "tn.h"

#define  TN_TIMER_STACK_SIZE   80//64
#define  TN_IDLE_STACK_SIZE    48
#define  TN_MIN_STACK_SIZE     40        //--  +20 for exit func when ver GCC > 4

#define  TN_BITS_IN_INT        32

#define  TN_ALIG                4
#define  MAKE_ALIG(a)  (((sizeof(a) + (TN_ALIG-1)) / TN_ALIG) * TN_ALIG)

#define  TN_PORT_STACK_EXPAND_AT_EXIT  16

#endif



