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
extern void idleTaskHandler(void *params);

TASK_DEFINE(idleTask1, 512, idleTaskHandler, NULL, TASK_LOWEST_PRIORITY, 1);
#endif

/*RTOS system call handler function*/
extern void syscallHandler(uint8_t sysCode);

/*RTOS tick handler function*/
extern void tickHandler(void);

/**
 * @brief Configure platform specific interrupts and tick timer.
 */
static inline void portConfig()
{
    /* Assign lowest priority to PendSV*/
    NVIC_SetPriority(PendSV_IRQn, PENDSV_PRIORITY);

    /* Assign lowest priority to SysTick*/
    NVIC_SetPriority(SysTick_IRQn, SYSTICK_PRIORITY);

    /* Configure SysTick to generate interrupt every OS_INTERVAL_CPU_TICKS */
    SYSTICK_CONFIG();
}

static void portRunFirstTask()
{
    spinLock(&lock);

    /*Get the highest priority ready task from ready Queue*/
    currentTask[CORE_ID()] = taskPool.currentTask[CORE_ID()] = taskQueueGet(&taskPool.readyQueue);

    /*Change status to RUNNING*/
    currentTask[CORE_ID()]->status = TASK_STATUS_RUNNING;

    portConfig();

    __set_PSP(currentTask[CORE_ID()]->stackPointer); /* Set PSP to the top of task's stack */

    uint32_t control = __get_CONTROL(); // Read CONTROL register

    __set_CONTROL(CONFIG_TASK_USER_MODE ? (control | 0x3) : control | 0x2); // Set bit 0 to 1 to switch to unprivileged mode

    __ISB(); // Instruction Synchronization Barrier

    __DSB(); // Data Synchronization Barrier

    spinUnlock(&lock);

    /*Jump to first task*/
    currentTask[CORE_ID()]->taskEntry(currentTask[CORE_ID()]->params);
}

void core1_entry(void)
{

    portRunFirstTask();
}

/**
 * @brief Setup the scheduler to start the first task.
 *
 * @param pTask
 */
void portSchedulerStart()
{
#if (CONFIG_SMP)
    taskStart(&idleTask1);
    multicore_launch_core1(core1_entry);
#endif

    portRunFirstTask();
}

/**
 * @brief SysTick Timer interrupt handler. It selects next task to run and
 * triggers PendSV to perform actual context switch.
 */
void SYSTICK_HANDLER()
{
    tickHandler();
}

/**
 * @brief SVC interrupt service routine(ISR). SVC interrupt is triggered via SYSCALL
 * with a specific SVC number. SVC number is decoded to perform corresponding action.
 *
 */
void SVC_Handler()
{

    /*Get the stack pointer of the task which triggered the SVC interrupt*/
    uint32_t *stackPointer = (uint32_t *)__get_PSP();

    /*Get PC from exception stack frame which hold address of instruction after the SVC instruction
        ____ <-- stackBase
       |____|xPSR stackPointer[7]
       |____|PC stackPointer[6]
       |____|LR stackPointer[5]
       |____|R12 stackPointer[4]
       |____|R3 stackPointer[3]
       |____|R2 stackPointer[2]
       |____|R1 stackPointer[1]
       |____|R0 stackPointer[0]*/

    uint32_t PC = stackPointer[6];

    /*Extract the SVC number from the SVC instruction, which resides just before the address in the PC.
     The SVC instruction is a 16-bit instruction, whose LSB byte is the SVC number.
     Hence, the SVC number is extracted by accessing the value at the memory location which is two bytes before the address in the PC.*/
    uint8_t svcNumber = *((uint8_t *)PC - 2);

    /*Call high level syscallHandler defined in scheduler.c*/
    syscallHandler(svcNumber);
}
