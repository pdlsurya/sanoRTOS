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

#include <stdlib.h>
#include <sanoRTOS/port.h>
#include "sanoRTOS/config.h"
#include "sanoRTOS/task.h"

/*RTOS tick handler function*/
extern void tickHandler(void);

static inline void portConfig()
{
    /* Assign lowest priority to PendSV*/
    NVIC_SetPriority(PendSV_IRQn, PENDSV_PRIORITY);
}

void portSchedulerStart()
{

    taskQueueType *pReadyQueue = getReadyQueue();

    /*Get the highest priority ready task from ready Queue*/
    currentTask[PORT_CORE_ID()] = taskQueueGet(pReadyQueue, true);

    taskSetCurrent(currentTask[PORT_CORE_ID()]);

    /*Change status to RUNNING*/
    currentTask[PORT_CORE_ID()]->status = TASK_STATUS_RUNNING;

    portConfig();

    __set_PSP(currentTask[PORT_CORE_ID()]->stackPointer); /* Set PSP to the top of task's stack */

    uint32_t control = __get_CONTROL(); // Read CONTROL register

    __set_CONTROL(control | 0x2); // Select PSP for thread mode.

    __ISB(); // Instruction Synchronization Barrier

    /*Jump to first task*/
    currentTask[PORT_CORE_ID()]->entry(currentTask[PORT_CORE_ID()]->params);
}

void osSysTick_Handler()
{
    tickHandler();
}

__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile(
        "cpsid i\n" // Disable interrupts

        "mrs r0, psp\n" // Get current process stack pointer

        "isb\n"

        // If task uses FPU, save s16–s31
        "tst lr, #0x10\n"
        "it eq\n"
        "vstmdbeq r0!, {s16-s31}\n"

        // Save r4-r11 and lr to current task's stack
        "stmdb r0!, {r4-r11, lr}\n"

               // Save current stack pointer to *currentTask
        "str r0, [%[current]]\n"

        // Load next stack pointer from *nextTask
        "ldr r0, [%[next]]\n"

        // Restore r4-r11 and lr
        "ldmia r0!, {r4-r11, lr}\n"

        // If task uses FPU, restore s16–s31
        "tst lr, #0x10\n"
        "it eq\n"
        "vldmiaeq r0!, {s16-s31}\n"

        "msr psp, r0\n" // Set PSP

        "isb\n" // Flush instruction pipeline before return

        "cpsie i\n" // Enable interrupts

        "bx lr\n" // Return to Thread mode
        :
        : [current] "r"(&currentTask[PORT_CORE_ID()]->stackPointer),
          [next] "r"(&nextTask[PORT_CORE_ID()]->stackPointer)
        : "r0", "memory");
}
