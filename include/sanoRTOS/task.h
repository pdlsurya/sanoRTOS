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
#include "sanoRTOS/config.h"
#include "sanoRTOS/port.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/scheduler.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TASK_LOWEST_PRIORITY 0xff
#define TASK_HIGHEST_PRIORITY 0

#define TASK_NO_WAIT 0
#define TASK_MAX_WAIT 0xffffffffUL

#define STACK_GUARD_WORDS 8

    void taskExitFunction();

    typedef enum
    {
        AFFINITY_CORE_ANY = -1,
        AFFINITY_CORE_0,
        AFFINITY_CORE_1,
        AFFINITY_CORE_2,
        AFFINITY_CORE_3
    } coreAffinityType;

/**
 * @brief Statically define and initialize a task and its default stack contents.
 * @param name Name of the task.
 * @param stackSize Size of task stack in bytes.
 * @param taskEntryFunction Task  entry  function.
 * @param taskParams Entry point parameter.
 * @param taskPriority Task priority.
 */
#define TASK_DEFINE(name, stackSize, taskEntryFunction, taskParams, taskPriority, affinity)                 \
    void taskEntryFunction(void *);                                                                         \
    PORT_TASK_STACK_DEFINE(name, stackSize, taskEntryFunction, taskExitFunction, taskParams);               \
    static taskHandleType name = {                                                                          \
        .taskName = #name,                                                                                  \
        .stackPointer = (uint32_t)(name##Stack + stackSize / sizeof(uint32_t) - INITIAL_TASK_STACK_OFFSET), \
        .stack = name##Stack,                                                                               \
        .priority = taskPriority,                                                                           \
        .coreAffinity = affinity,                                                                           \
        .taskEntry = taskEntryFunction,                                                                     \
        .params = taskParams,                                                                               \
        .remainingSleepTicks = 0,                                                                           \
        .status = TASK_STATUS_READY,                                                                        \
        .blockedReason = BLOCK_REASON_NONE,                                                                 \
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
        uint32_t *stack;
        taskFunctionType taskEntry;
        void *params;
        const char *taskName;
        uint32_t remainingSleepTicks;
        taskStatusType status;
        blockedReasonType blockedReason;
        wakeupReasonType wakeupReason;
        uint8_t priority;
        coreAffinityType coreAffinity;

    } taskHandleType;

    typedef struct
    {
        /*Queue of tasks in ready state*/
        taskQueueType readyQueue;

        /*Queue of tasks in blocked state*/
        taskQueueType blockedQueue;

        /*Currently running task*/
        taskHandleType *currentTask[PORT_CORE_COUNT];

    } taskPoolType;

    extern taskHandleType *currentTask[PORT_CORE_COUNT];
    extern taskHandleType *nextTask[PORT_CORE_COUNT];
    extern taskPoolType taskPool;

    void taskSetReady(taskHandleType *pTask, wakeupReasonType wakeupReason);

    void taskBlock(taskHandleType *pTask, blockedReasonType blockedReason, uint32_t ticks);

    void taskSuspend(taskHandleType *pTask);

    int taskResume(taskHandleType *pTask);

    void taskStart(taskHandleType *pTask);

    void taskCheckStackOverflow();

    /**
     * @brief Block the current task for a specified number of RTOS ticks.
     *
     * @param sleepTicks Number of RTOS ticks to sleep
     */
    static inline __attribute__((always_inline)) void taskSleep(uint32_t sleepTicks)
    {
        taskBlock(taskPool.currentTask[PORT_CORE_ID()], SLEEP, sleepTicks);
    }

    /**
     * @brief Block the current task for a specified number of milliseconds.
     *
     * @param sleepTimeMS Sleep duration in milliseconds
     */
    static inline __attribute__((always_inline)) void taskSleepMS(uint32_t sleepTimeMS)
    {
        taskSleep(MS_TO_RTOS_TICKS(sleepTimeMS));
    }

    /**
     * @brief Block the current task for a specified number of microseconds.
     *
     * @param sleepTimeUS Sleep duration in microseconds
     */
    static inline __attribute__((always_inline)) void taskSleepUS(uint32_t sleepTimeUS)
    {
        taskSleep(US_TO_RTOS_TICKS(sleepTimeUS));
    }

    /**
     * @brief Get the name of the currently running task.
     *
     * @return Pointer to the task name string
     */
    static inline __attribute__((always_inline)) const char *taskGetName()
    {
        return taskPool.currentTask[PORT_CORE_ID()]->taskName;
    }

    /**
     * @brief Get the handle of the currently running task.
     *
     * @return Pointer to the current task's handle (taskHandleType)
     */
    static inline __attribute__((always_inline)) taskHandleType *taskGetCurrent()
    {
        return taskPool.currentTask[PORT_CORE_ID()];
    }

    /**
     * @brief Set the current task for the core.
     *
     * @param pTask Pointer to the task handle to set as current
     */
    static inline __attribute__((always_inline)) void taskSetCurrent(taskHandleType *pTask)
    {
        taskPool.currentTask[PORT_CORE_ID()] = pTask;
    }

    /**
     * @brief Assign a core affinity to the specified task.
     *
     * @param pTask Pointer to the task
     * @param affinity Core affinity to assign
     */
    static inline __attribute__((always_inline)) void taskSetCoreAffinity(taskHandleType *pTask, coreAffinityType affinity)
    {
        pTask->coreAffinity = affinity;
    }

    /**
     * @brief Retrieve the core affinity of the specified task.
     *
     * @param pTask Pointer to the task
     * @return Core affinity of the task
     */
    static inline __attribute__((always_inline)) coreAffinityType taskGetCoreAffinity(taskHandleType *pTask)
    {
        return pTask->coreAffinity;
    }

    /**
     * @brief Set the priority of the specified task.
     *
     * @param pTask Pointer to the task
     * @param priority Priority value to set
     */
    static inline __attribute__((always_inline)) void taskSetPriority(taskHandleType *pTask, uint8_t priority)
    {
        pTask->priority = priority;
    }

    /**
     * @brief Get the priority of the specified task.
     *
     * @param pTask Pointer to the task
     * @return Task priority
     */
    static inline __attribute__((always_inline)) uint8_t taskGetPriority(taskHandleType *pTask)
    {
        return pTask->priority;
    }

    /**
     * @brief Get a pointer to the ready queue.
     *
     * This function returns a pointer to the ready queue in the task pool,
     * which contains tasks that are ready to run.
     *
     * @return Pointer to the ready queue (taskQueueType *)
     */
    static inline __attribute__((always_inline)) taskQueueType *getReadyQueue()
    {
        return &taskPool.readyQueue;
    }

    /**
     * @brief Get a pointer to the blocked queue.
     *
     * This function returns a pointer to the blocked queue in the task pool,
     * which contains tasks that are currently waiting (e.g., sleeping or blocked on a resource).
     *
     * @return Pointer to the blocked queue (taskQueueType *)
     */
    static inline __attribute__((always_inline)) taskQueueType *getBlockedQueue()
    {
        return &taskPool.blockedQueue;
    }

#ifdef __cplusplus
}
#endif

#endif
