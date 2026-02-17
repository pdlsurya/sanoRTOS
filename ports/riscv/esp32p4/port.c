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
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sanoRTOS/port.h"
#include "sanoRTOS/config.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/spinLock.h"
#include "interrupts.h"
#include "startup.h"
#include "usb_serial.h"

/*RTOS tick handler function*/
extern void tickHandler(void);

static atomic_t lock;

#if (CONFIG_SMP)

TASK_DEFINE(idleTask1, 512, idleTaskHandler1, NULL, TASK_LOWEST_PRIORITY, AFFINITY_CORE_1);

void idleTaskHandler1(void *params)
{
    (void)params;
    while (1)
    {
        taskCleanupExited();
        // PORT_ENTER_SLEEP_MODE();
    }
}
#endif

static inline void portConfig()
{
    /*Replace this with vendor specific machine software interrupt enable api and
    machine timer interrupt callback api */

    /*Enable machine software interrupt*/
    msi_enable();

    /*initialize mtimer to generate interrupt every 1 ms*/
    mtimer_cb_init_t mtimer_cb_init = {0};
    mtimer_cb_init.period_ticks = TIMER_TICKS_PER_RTOS_TICK;
    mtimer_cb_init.cb_function = tickHandler;
    mtimer_callback_init(&mtimer_cb_init);
}

void portRunFirstTask()
{
    bool irqState = spinLock(&lock);

    taskQueueType *pReadyQueue = getReadyQueue();
    uint8_t coreId = PORT_CORE_ID();

    /*Get the highest priority ready task from ready Queue*/
    currentTask[coreId] = TASK_GET_FROM_READY_QUEUE(pReadyQueue);

    taskSetCurrent(currentTask[coreId]);

    /*Change status to RUNNING*/
    currentTask[coreId]->status = TASK_STATUS_RUNNING;

    portConfig();

    spinUnlock(&lock, irqState);

#if USE_ISR_STACK
    /*Save main sp to mscratch so that exception/interrupt handler can retrieve it during an
     exception/interrupt and use its own stack*/
    __asm__ volatile("csrw mscratch, sp");

#endif

    taskHandleType *pCurrentTask = currentTask[coreId];

    __asm__ volatile(" mv sp, %0" ::"r"(pCurrentTask->stackPointer));
    /*Jump directly to the first task*/
    pCurrentTask->entry(pCurrentTask->params);
}

void core1_main(void)
{
    portRunFirstTask();
}

void portSchedulerStart()
{
#if (CONFIG_SMP)
    (void)taskStart(&idleTask1);

    core1_start(core1_main);
#endif

    portRunFirstTask();
}
