/*
    TNKernel startup file for Rowley Associates Ltd.(R) CrossStudio(C) IDE
    and any other IDE uses GCC compiler

    GCC ARM assembler

Copyright © 2004,2005 Yuri Tiomkin
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

/* Mode, correspords to bits 0-5 in CPSR */

   .equ  MODE_BITS,   0x1F               /* Bit mask for mode bits in CPSR */
   .equ  USR_MODE,    0x10               /* User mode */
   .equ  FIQ_MODE,    0x11               /* Fast Interrupt Request mode */
   .equ  IRQ_MODE,    0x12               /* Interrupt Request mode */
   .equ  SVC_MODE,    0x13               /* Supervisor mode */
   .equ  ABT_MODE,    0x17               /* Abort mode */
   .equ  UND_MODE,    0x1B               /* Undefined Instruction mode */
   .equ  SYS_MODE,    0x1F               /* System mode */


  .section .vectors, "ax"
  .code 32
  .align 0

  .extern tn_cpu_irq_isr
  .extern tn_cpu_fiq_isr
  .global _vectors

/*****************************************************************************
 * Exception Vectors                                                         *
 *****************************************************************************/
_vectors:
              ldr  pc, reset_handler_address
              ldr  pc, undef_handler_address
              ldr  pc, swi_handler_address
              ldr  pc, pabort_handler_address
              ldr  pc, dabort_handler_address
              .word   0xB8A06F58          /* 0 - (sum of other vectors instructions) */
              ldr  pc, irq_handler_address
              ldr  pc, fiq_handler_address

reset_handler_address:  .word  reset_handler
undef_handler_address:  .word  undef_handler
swi_handler_address:    .word  swi_handler
pabort_handler_address: .word  pabort_handler
dabort_handler_address: .word  dabort_handler
                        .word  0x00
irq_handler_address:    .word  tn_cpu_irq_isr
fiq_handler_address:    .word  tn_cpu_fiq_isr

  .text
  .code 32
  .align 0

  .global __start
  .global __gccmain
  .extern main
  .extern exit
  .extern tn_startup_hardware_init


/*  exception handlers  */

reset_handler:   b  _start
undef_handler:   b  undef_handler
swi_handler:     b  swi_handler
pabort_handler:  b  pabort_handler
dabort_handler:  b  dabort_handler


/*****************************************************************************
 * Function    : _start                                                      *
 * Description : Main entry point and startup code for C system.             *
 *****************************************************************************/
_start:
__start:
              ldr   r0,=tn_startup_hardware_init      //-- vital hardware init
              mov   lr,pc
              bx    r0

     /* init stacks */
              mrs   r0,cpsr                           /* Original PSR value */
              bic   r0,r0,#MODE_BITS                  /* Clear the mode bits */
              orr   r0,r0,#IRQ_MODE                   /* Set IRQ mode bits */
              msr   cpsr_c,r0                         /* Change the mode */
              ldr   sp, =__stack_irq_end__            /* End of IRQ_STACK */

              bic   r0,r0,#MODE_BITS                  /* Clear the mode bits */
              orr   r0,r0,#FIQ_MODE                    /* Set FIQ mode bits */
              msr   cpsr_c,r0                          /* Change the mode */
              ldr   sp, =__stack_fiq_end__            /* End of FIQ_STACK */

              bic   r0,r0,#MODE_BITS                  /* Clear the mode bits */
              orr   r0,r0,#SVC_MODE                   /* Set Supervisor mode bits */
              msr   cpsr_c,r0                         /* Change the mode */
              ldr   sp, =__stack_end__                /* End of stack */


  /* Copy from initialised data section to data section (if necessary). */
  ldr r0, =__data_load_start__
  ldr r1, =__data_start__
  cmp r0, r1
  beq copy_data_end

  ldr r2, =__data_end__
  subs r2, r2, r1
  beq copy_data_end

copy_data_loop:
  ldrb r3, [r0], #+1
  strb r3, [r1], #+1
  subs r2, r2, #1
  bne copy_data_loop
copy_data_end:

  /* Copy from initialised fast_text section to fast_text section (if necessary). */
  ldr r0, =__fast_load_start__
  ldr r1, =__fast_start__
  cmp r0, r1
  beq copy_fast_end

  ldr r2, =__fast_end__
  subs r2, r2, r1
  beq copy_fast_end

copy_fast_loop:
  ldrb r3, [r0], #+1
  strb r3, [r1], #+1
  subs r2, r2, #1
  bne copy_fast_loop
copy_fast_end:

  /* Zero the bss. */
  ldr r0, =__bss_start__
  ldr r1, =__bss_end__
  mov r2, #0
zero_bss_loop:
  cmp r0, r1
  beq zero_bss_end
  strb r2, [r0], #+1
  b zero_bss_loop
zero_bss_end:

  /* Initialise the heap */
  ldr r0, = __heap_start__
  ldr r1, = __heap_end__
  sub r1, r1, r0     /* r1 = r1-r0 */
  mov r2, #0
  str r2, [r0], #+4 /* *r0++ = 0 */
  str r1, [r0]      /* *r0 = __heap_end__ - __heap_start__ */

  /* Call constructors */
  ldr r0, =__ctors_start__
  ldr r1, =__ctors_end__
ctor_loop:
  cmp r0, r1
  beq ctor_end
  ldr r2, [r0], #+4
  stmfd sp!, {r0-r1}
  mov lr, pc
  mov pc, r2
  ldmfd sp!, {r0-r1}
  b ctor_loop
ctor_end:

  /* Jump to main entry point */
  mov r0, #0
  mov r1, #0
  ldr r2, =main
  mov     lr, pc
  bx      r2

  /* Call destructors */
  ldr r0, =__dtors_start__
  ldr r1, =__dtors_end__
dtor_loop:
  cmp r0, r1
  beq dtor_end
  ldr r2, [r0], #+4
  stmfd sp!, {r0-r1}
  mov lr, pc
  mov pc, r2
  ldmfd sp!, {r0-r1}
  b dtor_loop
dtor_end:

  /* Return from main, loop forever. */
exit_loop: b exit_loop


