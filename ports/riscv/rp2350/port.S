/*
* MIT License
* 
* Copyright (c) 2024 Surya Poudel
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

#include "hardware/regs/sio.h"
#include "hardware/regs/addressmap.h"

.equ SAVE_REGS, 32
.equ CONTEXT_SIZE, (SAVE_REGS * 4)

.equ RV_STK_MEPC,  0
.equ RV_STK_RA,    4
.equ RV_STK_TP,    8
.equ RV_STK_T0,    12
.equ RV_STK_T1,    16
.equ RV_STK_T2,    20
.equ RV_STK_S0,    24
.equ RV_STK_S1,    28
.equ RV_STK_A0,    32
.equ RV_STK_A1,    36
.equ RV_STK_A2,    40
.equ RV_STK_A3,    44
.equ RV_STK_A4,    48
.equ RV_STK_A5,    52
.equ RV_STK_A6,    56
.equ RV_STK_A7,    60
.equ RV_STK_S2,    64
.equ RV_STK_S3,    68
.equ RV_STK_S4,    72
.equ RV_STK_S5,    76
.equ RV_STK_S6,    80
.equ RV_STK_S7,    84
.equ RV_STK_S8,    88
.equ RV_STK_S9,    92
.equ RV_STK_S10,   96
.equ RV_STK_S11,   100
.equ RV_STK_T3,    104
.equ RV_STK_T4,    108
.equ RV_STK_T5,    112
.equ RV_STK_T6,    116


/* Macro which first allocates space on the stack to save general
 * purpose registers, and then save them. GP register is excluded.
 * The default size allocated on the stack is CONTEXT_SIZE, but it
 * can be overridden. */
.macro save_general_regs cxt_size=CONTEXT_SIZE
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

/* Restore the general purpose registers (excluding gp) from the context on
 * the stack. The context is then deallocated. The default size is CONTEXT_SIZE
 * but it can be overridden. */
.macro restore_general_regs cxt_size=CONTEXT_SIZE
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

    csrr t0, mhartid             // Get current core number into a0
    li   t1, 1
    addi t0, t0, 8               // Shift by 8 for core 0, 9 for core 1
    sll  t1, t1, t0              // t1 = 1 << (mhartid + 8)

    li t2, (SIO_BASE+SIO_RISCV_SOFTIRQ_OFFSET)

    sw   t1, 0(t2)                // Write to soft IRQ register
.endm

.section .time_critical
.global isr_riscv_machine_soft_irq
.type isr_riscv_machine_soft_irq, @function

isr_riscv_machine_soft_irq:

    fence 
    fence.i

/* Start by saving the general purpose registers and the PC value before
 * the interrupt happened. */
    save_general_regs
    save_mepc
    
    /*Clear msip */
    clear_msip

    /* Read hart ID (CPU ID) into t2 */
    csrr t2, mhartid       // t2 = hart ID (0 or 1)

    /* Save current task's stack pointer */
    la t0, currentTask     // t0 = &currentTask
    slli t3, t2, 2         // t3 = t2 * 4 (assuming 32-bit pointers)
    add t0, t0, t3         // t0 = &currentTask[hartid]
    lw t1, 0(t0)           // t1 = currentTask[hartid]
    sw sp, 0(t1)           // Save SP to currentTask[hartid]->sp
    fence 

    /* Load next task's stack pointer */
    la t0, nextTask        // t0 = &nextTask
    add t0, t0, t3         // t0 = &nextTask[hartid]
    lw t1, 0(t0)           // t1 = nextTask[hartid]
    lw sp, 0(t1)           // Load SP from nextTask[hartid]->sp

    restore_mepc
    restore_general_regs

    fence
    fence.i

    mret

.size isr_riscv_machine_soft_irq, .-isr_riscv_machine_soft_irq