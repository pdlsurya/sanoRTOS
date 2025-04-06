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

#if  defined(__ARM_ARCH_6M__)
    .arch armv6-m
#elif defined(__ARM_ARCH_7M__)
    .syntax unified
    .arch armv7-m
#elif defined(__ARM_ARCH_7EM__)
    .syntax unified
    .arch armv7e-m
#elif defined(__ARM_ARCH_8M_MAIN__)
     .syntax unified
     .arch armv8-m.main
#endif

.thumb
.text
.globl PendSV_Handler
.type PendSV_Handler, %function

PendSV_Handler:
    
    cpsid i // Disable interrupts: 

    mrs r0, psp
    
#ifdef __ARM_FP
    /*Check if current task uses fpu instructions.If so, push registers s16-s31 to stack.
    *This will also trigger lazy fp context preservation of S0-S15
    */
    tst lr, #0x10
    it eq
    vstmdbeq r0!, {s16-s31}
#endif


#ifdef __ARM_ARCH_6M__
    sub r0, #4

    mov r2,lr //Store EXC_RETURN
    str r2, [r0]

 /*The STMDB instruction can only save low registers (R0-R7);
 Hence, we need  to copy registers R8-R11 into R4-R7 and call STMDB twice. */
    stmdb r0!,{r4-r7}

    mov	r4, r8
	mov	r5, r9
	mov	r6, r10
	mov	r7, r11
    stmdb r0!,{r4-r7}
#else

    stmdb r0!, {r4-r11,lr} //push register r4-r11 and EXC_RETURN to current task's stack

#endif

     bl switchContext
 
#ifdef __ARM_ARCH_6M__
/*The LDMIA instruction can only use low registers (R0-R7);
 Hence, we need  to first load contents of R8-R11 to R4-R7 registers 
 and later move contents to R8-R11*/
    ldmia r0!,{r4-r7}

    mov	r8, r4
    mov	r9, r5
    mov	r10, r6 
    mov	r11, r7

    ldmia r0!,{r4-r7}

    ldr r2, [r0] //Load EXC_RETURN
    mov lr, r2

    add r0, #4

#else

    ldmia r0!,{r4-r11,lr} //pop registers r4-r11 and EXC_RETURN from next task's stack
    
#endif

#ifdef __ARM_FP
    /*Check if next task uses fpu instructions.If so, pop registers s16-s31 from stack.*/
    tst lr, #0x10
    it eq
    vldmiaeq r0!, {s16-s31}
#endif

    msr psp,r0 //load psp with next task's stack pointer

	cpsie	i // Enable interrupts

	bx	lr //return with specified EXC_RETURN

.size PendSV_Handler, .-PendSV_Handler