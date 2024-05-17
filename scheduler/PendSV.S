.syntax unified
.arch armv7e-m

.thumb
.text
.globl PendSV_Handler
.type PendSV_Handler, %function

PendSV_Handler:
    
    cpsid i // Disable interrupts: 

    mrs r0, psp

    /*Check if current task uses fpu instructions.If so, push registers s16-s31 to stack.
    *This will also trigger lazy fp context preservation of S0-S15
    */
    tst lr, #0x10
    it eq
    vstmdbeq r0!, {s16-s31}

    stmdb r0!, {r4-r11,lr} //push register r4-r11 and EXC_RETURN to current task's stack

    /*save current task's stack pointer*/
    ldr r1, =currentTask
    ldr r2,[r1] 
    str r0,[r2] //first member of the taskHandleType struct is stack pointer

    /*load next task's stack pointer*/
    ldr r1, =nextTask
    ldr r2,[r1]
    ldr r0,[r2] //first member of the taskHandleType struct is stack pointer

    ldmia r0!,{r4-r11,lr} //pop registers r4-r11 and EXC_RETURN from next task's stack

    /*Check if next task uses fpu instructions.If so, pop registers s16-s31 from stack.*/
    tst lr, #0x10
    it eq
    vldmiaeq r0!, {s16-s31}
    
    msr psp,r0 //load psp with next task's stack pointer

	cpsie	i // Enable interrupts

	bx	lr //return with specified EXC_RETURN

.size PendSV_Handler, .-PendSV_Handler
