/*
* MIT License
* 
* Copyright (c) 2025 Surya Poudel
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "soc/soc.h"
#include "soc/interrupt_reg.h"
#include "riscv/rvruntime-frames.h"
#include "soc/soc_caps.h"


.equ SAVE_GP_REGS, 32
.equ SAVE_FPU_REGS, 33
.equ GP_RES_CONTEXT_SIZE, (SAVE_GP_REGS*4)
.equ FPU_RES_CONTEXT_SIZE, (SAVE_FPU_REGS*4)
.equ MSIP_REG, 0x20000000

/* Macro which first allocates space on the stack to save general
 * purpose registers, and then save them. GP register is excluded.
 * The default size allocated on the stack is GP_RES_CONTEXT_SIZE, but it
 * can be overridden. */
.macro save_general_regs cxt_size=GP_RES_CONTEXT_SIZE
    addi sp, sp, -\cxt_size
    sw   ra, RV_STK_RA(sp)
    sw   tp, RV_STK_TP(sp)
    sw   t0, RV_STK_T0(sp)
    sw   t1, RV_STK_T1(sp)
    sw   t2, RV_STK_T2(sp)
    sw   s0, RV_STK_S0(sp)
    sw   s1, RV_STK_S1(sp)
    sw   a0, RV_STK_A0(sp)
    sw   a1, RV_STK_A1(sp)
    sw   a2, RV_STK_A2(sp)
    sw   a3, RV_STK_A3(sp)
    sw   a4, RV_STK_A4(sp)
    sw   a5, RV_STK_A5(sp)
    sw   a6, RV_STK_A6(sp)
    sw   a7, RV_STK_A7(sp)
    sw   s2, RV_STK_S2(sp)
    sw   s3, RV_STK_S3(sp)
    sw   s4, RV_STK_S4(sp)
    sw   s5, RV_STK_S5(sp)
    sw   s6, RV_STK_S6(sp)
    sw   s7, RV_STK_S7(sp)
    sw   s8, RV_STK_S8(sp)
    sw   s9, RV_STK_S9(sp)
    sw   s10, RV_STK_S10(sp)
    sw   s11, RV_STK_S11(sp)
    sw   t3, RV_STK_T3(sp)
    sw   t4, RV_STK_T4(sp)
    sw   t5, RV_STK_T5(sp)
    sw   t6, RV_STK_T6(sp)
.endm

.macro save_mepc
    csrr t0, mepc
    sw   t0, RV_STK_MEPC(sp)
.endm

.macro save_fpu_regs cxt_size=FPU_RES_CONTEXT_SIZE
    addi sp, sp, -\cxt_size
    /* -------- Save ft registers -------- */
    fsw     ft0,  RV_FPU_FT0(sp)
    fsw     ft1,  RV_FPU_FT1(sp)
    fsw     ft2,  RV_FPU_FT2(sp)
    fsw     ft3,  RV_FPU_FT3(sp)
    fsw     ft4,  RV_FPU_FT4(sp)
    fsw     ft5,  RV_FPU_FT5(sp)
    fsw     ft6,  RV_FPU_FT6(sp)
    fsw     ft7,  RV_FPU_FT7(sp)
    fsw     ft8,  RV_FPU_FT8(sp)
    fsw     ft9,  RV_FPU_FT9(sp)
    fsw     ft10, RV_FPU_FT10(sp)
    fsw     ft11, RV_FPU_FT11(sp)

    /* -------- Save fa registers -------- */
    fsw     fa0,  RV_FPU_FA0(sp)
    fsw     fa1,  RV_FPU_FA1(sp)
    fsw     fa2,  RV_FPU_FA2(sp)
    fsw     fa3,  RV_FPU_FA3(sp)
    fsw     fa4,  RV_FPU_FA4(sp)
    fsw     fa5,  RV_FPU_FA5(sp)
    fsw     fa6,  RV_FPU_FA6(sp)
    fsw     fa7,  RV_FPU_FA7(sp)

    /* -------- Save fs registers -------- */
    fsw     fs0,  RV_FPU_FS0(sp)
    fsw     fs1,  RV_FPU_FS1(sp)
    fsw     fs2,  RV_FPU_FS2(sp)
    fsw     fs3,  RV_FPU_FS3(sp)
    fsw     fs4,  RV_FPU_FS4(sp)
    fsw     fs5,  RV_FPU_FS5(sp)
    fsw     fs6,  RV_FPU_FS6(sp)
    fsw     fs7,  RV_FPU_FS7(sp)
    fsw     fs8,  RV_FPU_FS8(sp)
    fsw     fs9,  RV_FPU_FS9(sp)
    fsw     fs10, RV_FPU_FS10(sp)
    fsw     fs11, RV_FPU_FS11(sp)

    /* -------- Save FCSR -------- */
    csrr    t3, fcsr
    sw      t3, RV_FPU_FCSR(sp)     
.endm 

.macro restore_fpu_res cxt_size=FPU_RES_CONTEXT_SIZE
 /* -------- Restore ft registers -------- */
    flw     ft0,  RV_FPU_FT0(sp)
    flw     ft1,  RV_FPU_FT1(sp)
    flw     ft2,  RV_FPU_FT2(sp)
    flw     ft3,  RV_FPU_FT3(sp)
    flw     ft4,  RV_FPU_FT4(sp)
    flw     ft5,  RV_FPU_FT5(sp)
    flw     ft6,  RV_FPU_FT6(sp)
    flw     ft7,  RV_FPU_FT7(sp)
    flw     ft8,  RV_FPU_FT8(sp)
    flw     ft9,  RV_FPU_FT9(sp)
    flw     ft10, RV_FPU_FT10(sp)
    flw     ft11, RV_FPU_FT11(sp)

    /* -------- Restore fa registers -------- */
    flw     fa0,  RV_FPU_FA0(sp)
    flw     fa1,  RV_FPU_FA1(sp)
    flw     fa2,  RV_FPU_FA2(sp)
    flw     fa3,  RV_FPU_FA3(sp)
    flw     fa4,  RV_FPU_FA4(sp)
    flw     fa5,  RV_FPU_FA5(sp)
    flw     fa6,  RV_FPU_FA6(sp)
    flw     fa7,  RV_FPU_FA7(sp)

    /* -------- Restore fs registers -------- */
    flw     fs0,  RV_FPU_FS0(sp)
    flw     fs1,  RV_FPU_FS1(sp)
    flw     fs2,  RV_FPU_FS2(sp)
    flw     fs3,  RV_FPU_FS3(sp)
    flw     fs4,  RV_FPU_FS4(sp)
    flw     fs5,  RV_FPU_FS5(sp)
    flw     fs6,  RV_FPU_FS6(sp)
    flw     fs7,  RV_FPU_FS7(sp)
    flw     fs8,  RV_FPU_FS8(sp)
    flw     fs9,  RV_FPU_FS9(sp)
    flw     fs10, RV_FPU_FS10(sp)
    flw     fs11, RV_FPU_FS11(sp)

    /* -------- Restore FCSR -------- */
    lw      t3, RV_FPU_FCSR(sp)
    csrw    fcsr, t3

    addi sp,sp, \cxt_size
.endm


/* Restore the general purpose registers (excluding gp) from the context on
 * the stack. The context is then deallocated. The default size is GP_RES_CONTEXT_SIZE
 * but it can be overridden. */
.macro restore_general_regs cxt_size=GP_RES_CONTEXT_SIZE
    lw   ra, RV_STK_RA(sp)
    lw   tp, RV_STK_TP(sp)
    lw   t0, RV_STK_T0(sp)
    lw   t1, RV_STK_T1(sp)
    lw   t2, RV_STK_T2(sp)
    lw   s0, RV_STK_S0(sp)
    lw   s1, RV_STK_S1(sp)
    lw   a0, RV_STK_A0(sp)
    lw   a1, RV_STK_A1(sp)
    lw   a2, RV_STK_A2(sp)
    lw   a3, RV_STK_A3(sp)
    lw   a4, RV_STK_A4(sp)
    lw   a5, RV_STK_A5(sp)
    lw   a6, RV_STK_A6(sp)
    lw   a7, RV_STK_A7(sp)
    lw   s2, RV_STK_S2(sp)
    lw   s3, RV_STK_S3(sp)
    lw   s4, RV_STK_S4(sp)
    lw   s5, RV_STK_S5(sp)
    lw   s6, RV_STK_S6(sp)
    lw   s7, RV_STK_S7(sp)
    lw   s8, RV_STK_S8(sp)
    lw   s9, RV_STK_S9(sp)
    lw   s10, RV_STK_S10(sp)
    lw   s11, RV_STK_S11(sp)
    lw   t3, RV_STK_T3(sp)
    lw   t4, RV_STK_T4(sp)
    lw   t5, RV_STK_T5(sp)
    lw   t6, RV_STK_T6(sp)
    addi sp,sp, \cxt_size
.endm

.macro restore_mepc
    lw      t0, RV_STK_MEPC(sp)
    csrw    mepc, t0
.endm

.macro clear_msip
 li t0, MSIP_REG
 sw zero, 0(t0)
.endm

.section .tcm.text
.global m_software_interrupt_handler
.type m_software_interrupt_handler, @function

m_software_interrupt_handler:

/* Start by saving the general purpose registers and the PC value before
 * the interrupt happened. */
    save_general_regs
    save_mepc

    
/* ------------------------------------------------ */
/* Lazy FPU Save (only if FS == Dirty)             */
/* ------------------------------------------------ */

    li      t4, 0x0
    csrr    t0, mstatus
    li      t2, 0x6000            /*FS dirty mask */
    and     t1, t0, t2            /* Extract FS[14:13] */
    bne     t1, t2, 1f            /* Skip if not Dirty */
    li      t4, 0x1

    save_fpu_regs

    /*Clear msip */
   1: clear_msip

    /* Read hart ID (CPU ID) into t2 */
    csrr t2, mhartid       // t2 = hart ID (0 or 1)

    /* Save current task's stack pointer */
    la t0, currentTask     // t0 = &currentTask
    slli t3, t2, 2         // t3 = t2 * 4 (assuming 32-bit pointers)
    add t0, t0, t3         // t0 = &currentTask[hartid]
    lw t1, 0(t0)           // t1 = currentTask[hartid]
    sw sp, 0(t1)           // Save SP to currentTask[hartid]->sp
    sw t4, 4(t1)           // Store fpu usage flag in userFlags of TCB

    /* Load next task's stack pointer */
    la t0, nextTask        // t0 = &nextTask
    add t0, t0, t3         // t0 = &nextTask[hartid]
    lw t1, 0(t0)           // t1 = nextTask[hartid]
    lw sp, 0(t1)           // Load SP from nextTask[hartid]->sp
    lw t4, 4(t1)           // Load fpu usage flag from userFlags of TCB

    beq x0, t4, 2f

    restore_fpu_res

  2:restore_mepc
    restore_general_regs
    
    mret

.size m_software_interrupt_handler, .-m_software_interrupt_handler