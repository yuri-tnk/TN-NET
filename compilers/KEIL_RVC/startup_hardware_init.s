;------------------------------------------------------------------------------
;
;    TNKernel startup hardware init for LPC2XXX processors
;    (for ARM(c) compiler)
;
;    ARM(c) assembler
;
; Copyright © 2004,2008 Yuri Tiomkin
; All rights reserved.
;
;Permission to use, copy, modify, and distribute this software in source
;and binary forms and its documentation for any purpose and without fee
;is hereby granted, provided that the above copyright notice appear
;in all copies and that both that copyright notice and this permission
;notice appear in supporting documentation.
;
;THIS SOFTWARE IS PROVIDED BY THE YURI TIOMKIN AND CONTRIBUTORS ``AS IS'' AND
;ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
;ARE DISCLAIMED. IN NO EVENT SHALL YURI TIOMKIN OR CONTRIBUTORS BE LIABLE
;FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
;OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
;HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
;LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
;OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
;SUCH DAMAGE.

;  ver 2.0


  ;-- Public functions declared in this file

     EXPORT tn_startup_hardware_init

  ;-- Extern reference

     IMPORT reset

   ;-- Constants


MC_FMR    EQU  0xFFFFFF60
WDT_MR    EQU  0xFFFFFD44
CKGR_MOR  EQU  0xFFFFFC20
PMC_SR    EQU  0xFFFFFC68
CKGR_PLLR EQU  0xFFFFFC2C
PMC_MCKR  EQU  0xFFFFFC30
MC_RCR    EQU  0xFFFFFF00

NOINT     EQU  0xc0

;----------------------------------------------------------------------------

           AREA    HardwareInit, CODE, READONLY

           CODE32

rMAMTIM         DCD  0xE01FC004
rMAMCR          DCD  0xE01FC000

rPLLCON         DCD  0xE01FC080
rPLLSTAT        DCD  0xE01FC088
rPLLFEED        DCD  0xE01FC08C
r_SCS           DCD  0xE01FC1A0


;----------------------------------------------------------------------------
;  This routine is called immediately after reset to setup hardware that is
; vital for processor's functionality (for instance,SDRAM controller setup,
; PLL setup,etc.)
;  It is assumed that other hardware's init routine(s) will be invoked later
; by C-language function call.
;----------------------------------------------------------------------------

tn_startup_hardware_init


    ;-- For LPC23XX

     ; Flash speed
     ; rMAMTIM = 3

;      ldr   r0, rMAMTIM
;      mov   r1, #3
;      strb  r1, [r0]

     ; rMAMCR  = 2

;       ldr   r0, rMAMCR
;       mov   r1, #2
;       strb  r1, [r0]



;     if(rPLLSTAT & (1 << 25))  //-- Bit 25 - PLLC status
;     {
;        rPLLCON  = 1;      //-- PLLE(bit 0)= 1, PLLC(bit1) = 0  // Enable PLL, disconnected
;        rPLLFEED = 0xAA;
;        rPLLFEED = 0x55;
;     }

        ldr  r0, rPLLSTAT
        ldr  r1, [r0]
        tst  r1, #(1<<25) ; 0x02000000
        bne  label_1

        ldr   r0, rPLLCON
        mov   r1, #1
        str   r1, [r0]
        ldr   r0, rPLLFEED
        mov   r1, #0xAA
        str   r1, [r0]
        mov   r1, #0x55
        str   r1, [r0]

label_1

;   rPLLCON  = 0;                    // Disable PLL, disconnected

        ldr   r0, rPLLCON
        mov   r1, #0
        str   r1, [r0]


;       rPLLFEED = 0xaa;
;       rPLLFEED = 0x55;

      ldr  r0, rPLLFEED
      mov  r1, #0xAA
      str  r1, [r0]
      mov  r1, #0x55
      str  r1, [r0]

;   r_SCS |= 0x20;                   // Enable main OSC

      ldr  r0, r_SCS
      ldr  r1, [r0]
      orr  r1, r1, #0x20
      str  r1, [r0]

;   while(!(rSCS & 0x40));        // Wait until main OSC is usable

label_2

      ldr  r0, r_SCS
      ldr  r1, [r0]
      tst  r1, #0x40
      beq  label_2


      bx   lr

;----------------------------------------------------------------------------
      EXPORT  tn_arm_disable_interrupts

tn_arm_disable_interrupts

     mrs  r0, cpsr
     orr  r0, r0, #NOINT
     msr  cpsr_c, r0
     bx   lr


     EXPORT  tn_arm_enable_interrupts


tn_arm_enable_interrupts

     mrs  r0, cpsr
     bic  r0, r0, #NOINT
     msr  cpsr_c, r0
     bx   lr


     END

;----------------------------------------------------------------------------
;----------------------------------------------------------------------------
;----------------------------------------------------------------------------
;----------------------------------------------------------------------------
;----------------------------------------------------------------------------




