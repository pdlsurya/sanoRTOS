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

#ifndef __SANO_RTOS_TASK_H
#define __SANO_RTOS_TASK_H

#include <assert.h>
#include "osConfig.h"
#include "taskQueue/taskQueue.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TASK_LOWEST_PRIORITY 0xff
#define TASK_HIGHEST_PRIORITY 0

#define TASK_NO_WAIT 0
#define TASK_MAX_WAIT 0xffffffffUL

    extern void taskExitFunction();

    /**********--Task's default stack contents--****************************************
          ____ <-- tackBase = stack + stackSize / sizeof(uint32_t)
         |____|xPSR  --> stackBase - 1
         |____|Return address(PC) <-Task Entry --> stackBase - 2
         |____|LR --> stackBase - 3
         |____|R12
         |____|R3
         |____|R2
         |____|R1
         |____|R0 <-Task params --> stackBase - 8
         |____|EXC_RETURN --> stackBase - 9

        <--Cortex-M3/M4/M7-->               <--Cortex-MO/M0+--->
         |____|R11                                 |____|R7
         |____|R10                                 |____|R6
         |____|R9                                  |____|R5
         |____|R8                                  |____|R4
         |____|R7                                  |____|R11
         |____|R6                                  |____|R10
         |____|R5                                  |____|R9
         |____|R4 <--stackPointer=(stackBase - 17) |____|R8 <--stackPointer=(stackBase - 17)
            |                                         |
            |                                         |
         |____|                                    |____|
         |____|                                    |____|
       <-32bits->                                 <-32bits->
      *************************************************************************************/
/**
 * @brief Statically define and initialize a task and its default stack contents.
 * @param name Name of the task.
 * @param stackSize Size of task stack in bytes.
 * @param taskEntryFunction Task  entry  function.
 * @param taskParams Entry point parameter.
 * @param taskPriority Task priority.
 */
#define TASK_DEFINE(name, stackSize, taskEntryFunction, taskParams, taskPriority)    \
    void taskEntryFunction(void *);                                                  \
    uint32_t name##Stack[stackSize / sizeof(uint32_t)] = {                           \
        [stackSize / sizeof(uint32_t) - 1] = 0x01000000,                             \
        [stackSize / sizeof(uint32_t) - 2] = (uint32_t)taskEntryFunction,            \
        [stackSize / sizeof(uint32_t) - 3] = (uint32_t)taskExitFunction,             \
        [stackSize / sizeof(uint32_t) - 8] = (uint32_t)taskParams,                   \
        [stackSize / sizeof(uint32_t) - 9] = EXC_RETURN_THREAD_PSP};                 \
    taskHandleType name = {                                                          \
        .stackPointer = (uint32_t)(name##Stack + stackSize / sizeof(uint32_t) - 17), \
        .priority = taskPriority,                                                    \
        .taskEntry = taskEntryFunction,                                              \
        .params = taskParams,                                                        \
        .remainingSleepTicks = 0,                                                    \
        .status = TASK_STATUS_READY,                                                 \
        .blockedReason = BLOCK_REASON_NONE,                                          \
        .wakeupReason = WAKEUP_REASON_NONE}

    typedef void (*taskFunctionType)(void *params);

    typedef enum
    {
        TASK_STATUS_READY,
        TASK_STATUS_RUNNING,
        TASK_STATUS_BLOCKED,
        TASK_STATUS_SUSPENDED
    } taskStatusType;

    typedef enum
    {
        BLOCK_REASON_NONE,
        SLEEP,
        WAIT_FOR_SEMAPHORE,
        WAIT_FOR_MUTEX,
        WAIT_FOR_MSG_QUEUE_DATA,
        WAIT_FOR_MSG_QUEUE_SPACE,
        WAIT_FOR_COND_VAR,
        WAIT_FOR_TIMER_TIMEOUT,

    } blockedReasonType;

    typedef enum
    {
        WAKEUP_REASON_NONE,
        WAIT_TIMEOUT,
        SLEEP_TIME_TIMEOUT,
        SEMAPHORE_TAKEN,
        MUTEX_LOCKED,
        MSG_QUEUE_DATA_AVAILABLE,
        MSG_QUEUE_SPACE_AVAILABE,
        COND_VAR_SIGNALLED,
        TIMER_TIMEOUT,
        RESUME

    } wakeupReasonType;

    /*Task control block struct*/
    typedef struct taskHandle
    {
        uint32_t stackPointer;
        taskFunctionType taskEntry;
        void *params;
        uint32_t remainingSleepTicks;
        taskStatusType status;
        blockedReasonType blockedReason;
        wakeupReasonType wakeupReason;
        uint8_t priority;

    } taskHandleType;

    typedef struct
    {
        taskQueueType readyQueue;
        taskQueueType blockedQueue;
        taskHandleType *currentTask;

    } taskPoolType;

    extern taskHandleType *currentTask;
    extern taskHandleType *nextTask;
    extern taskPoolType taskPool;

    /**
     * @brief Store pointer to the taskHandle struct to the queue of ready tasks. Calling this
     * function from main does not start execution of the task if Scheduler is not started.To start executeion of task, osStartScheduler must be
     * called from  main after calling taskStart. If this function is called from other running tasks, execution happens based on priority of the task.
     *
     * @param pTask Pointer to taskHandle struct
     */
    static inline void taskStart(taskHandleType *pTask)
    {
        assert(pTask != NULL);

        taskQueueAdd(&taskPool.readyQueue, pTask);
    }

    extern void taskBlock(taskHandleType *pTask, blockedReasonType blockedReason, uint32_t ticks);

    /**
     * @brief Block task for specified number of RTOS Ticks
     *
     * @param sleepTicks
     */
    static inline void taskSleep(uint32_t sleepTicks)
    {
        taskBlock(taskPool.currentTask, SLEEP, sleepTicks);
    }

    /**
     * @brief Block task for specified number of milliseconds
     *
     * @param sleepTimeMS
     */
    static inline void taskSleepMS(uint32_t sleepTimeMS)
    {
        taskSleep(MS_TO_OS_TICKS(sleepTimeMS));
    }

    /**
     * @brief Block task for specified number of microseconds
     *
     * @param sleepTimeUS
     */
    static inline void taskSleepUS(uint32_t sleepTimeUS)
    {
        taskSleep(US_TO_OS_TICKS(sleepTimeUS));
    }

    void taskSetReady(taskHandleType *pTask, wakeupReasonType wakeupReason);

    void taskBlock(taskHandleType *pTask, blockedReasonType blockedReason, uint32_t ticks);

    void taskSuspend(taskHandleType *pTask);

    int taskResume(taskHandleType *pTask);

#ifdef __cplusplus
}
#endif

#endif
