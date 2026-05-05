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
#include "sanoRTOS/port.h"
#include "sanoRTOS/config.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"

static atomic_t lock;

#if (CONFIG_SMP)
TASK_DEFINE(idleTask1, 512, idleTaskHandler1, NULL, TASK_LOWEST_PRIORITY, AFFINITY_CORE_1);

void idleTaskHandler1(void *params)
{
    (void)params;
    while (true)
    {
        taskCleanupExited();
        /* PORT_ENTER_SLEEP_MODE(); */
    }
}
#endif

/*RTOS tick handler function*/
extern void tickHandler(void);

static inline void portConfig(void)
{
    SystemCoreClockUpdate();

    /* Assign lowest priority to PendSV */
    NVIC_SetPriority(PendSV_IRQn, PENDSV_PRIORITY);

    /* Assign lowest priority to SysTick */
    NVIC_SetPriority(SysTick_IRQn, SYSTICK_PRIORITY);

    /* Configure SysTick to generate interrupt every TIMER_TICKS_PER_RTOS_TICK */
    SysTick_Config(TIMER_TICKS_PER_RTOS_TICK);
}

static void portRunFirstTask(void)
{
    bool irqState = spinLock(&lock);

    /*Get the highest priority ready task from ready Queue*/
    currentTask[PORT_CORE_ID()] = readyQueuePop();

    taskSetCurrent(currentTask[PORT_CORE_ID()]);

    /*Change state to RUNNING*/
    currentTask[PORT_CORE_ID()]->state = TASK_STATE_RUNNING;

    portConfig();

    __set_PSP(currentTask[PORT_CORE_ID()]->stackPointer); /* Set PSP to the top of task's stack */

    uint32_t control = __get_CONTROL(); /* Read CONTROL register */
    __set_CONTROL(control | 0x2U); /* Select PSP for thread mode. */
    __ISB(); /* Instruction Synchronization Barrier */
    spinUnlock(&lock, irqState);

    /*Jump to first task*/
    currentTask[PORT_CORE_ID()]->entry(currentTask[PORT_CORE_ID()]->params);
}

#if (CONFIG_SMP)
static void core1_entry(void)
{
    portRunFirstTask();
}
#endif

void portSchedulerStart(void)
{
#if (CONFIG_SMP)
    /* Reserve one SDK OS spin lock for RTOS-wide CAS serialization. */
    spin_lock_claim(PORT_ATOMIC_SPINLOCK_ID);

    (void)taskStart(&idleTask1);
    multicore_launch_core1(core1_entry);
#endif

    portRunFirstTask();
}

void SysTick_Handler(void)
{
    tickHandler();
}

__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile(
        "cpsid i\n"
        "mrs r0, psp\n"
        "isb\n"

        /* Cortex-M0+ lacks the full Thumb-2 push/pop forms for high registers. */
        "sub r0, #36\n"

        "mov r1, r8\n"
        "str r1, [r0, #0]\n"
        "mov r1, r9\n"
        "str r1, [r0, #4]\n"
        "mov r1, r10\n"
        "str r1, [r0, #8]\n"
        "mov r1, r11\n"
        "str r1, [r0, #12]\n"
        "str r4, [r0, #16]\n"
        "str r5, [r0, #20]\n"
        "str r6, [r0, #24]\n"
        "str r7, [r0, #28]\n"
        "mov r1, lr\n"
        "str r1, [r0, #32]\n"

        "str r0, [%[current]]\n"
        "ldr r0, [%[next]]\n"

        "ldr r1, [r0, #0]\n"
        "mov r8, r1\n"
        "ldr r1, [r0, #4]\n"
        "mov r9, r1\n"
        "ldr r1, [r0, #8]\n"
        "mov r10, r1\n"
        "ldr r1, [r0, #12]\n"
        "mov r11, r1\n"
        "ldr r4, [r0, #16]\n"
        "ldr r5, [r0, #20]\n"
        "ldr r6, [r0, #24]\n"
        "ldr r7, [r0, #28]\n"
        "ldr r1, [r0, #32]\n"
        "mov lr, r1\n"

        "add r0, #36\n"

        "msr psp, r0\n"
        "isb\n"
        "cpsie i\n"
        "bx lr\n"
        :
        : [current] "r"(&currentTask[PORT_CORE_ID()]->stackPointer),
          [next] "r"(&nextTask[PORT_CORE_ID()]->stackPointer)
        : "r0", "r1", "memory");
}
