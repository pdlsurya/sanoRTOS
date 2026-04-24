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

#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/config.h"
#include "sanoRTOS/port.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/mailbox.h"
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

#define TASK_FLAG_FPU_USED (1U << 0)
#define TASK_FLAG_DYNAMIC (1U << 8)
#define TASK_FLAG_OWN_STACK (1U << 9)
#define TASK_FLAG_OWN_NAME (1U << 10)

    /**
     * @brief Internal function executed if a task entry returns.
     */
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
#define TASK_DEFINE(_name, stackSize, taskEntryFunction, taskParams, taskPriority, affinity)                      \
    _Static_assert(((stackSize) % sizeof(uint32_t)) == 0U, "Task stack size must be word aligned.");             \
    _Static_assert((stackSize) >= ((PORT_INITIAL_TASK_STACK_OFFSET + STACK_GUARD_WORDS) * sizeof(uint32_t)),     \
                   "Task stack too small for initial frame and stack guard.");                                    \
    void taskEntryFunction(void *);                                                                               \
    PORT_TASK_STACK_DEFINE(_name, stackSize, taskEntryFunction, taskExitFunction, taskParams);                    \
    static taskHandleType _name = {                                                                               \
        .name = #_name,                                                                                           \
        .stackPointer = (uint32_t)(_name##Stack + stackSize / sizeof(uint32_t) - PORT_INITIAL_TASK_STACK_OFFSET), \
        .flags = 0,                                                                                               \
        .stack = _name##Stack,                                                                                    \
        .priority = taskPriority,                                                                                 \
        .coreAffinity = affinity,                                                                                 \
        .entry = taskEntryFunction,                                                                               \
        .params = taskParams,                                                                                     \
        .remainingSleepTicks = 0,                                                                                 \
        .status = TASK_STATUS_READY,                                                                              \
        .blockedReason = BLOCK_REASON_NONE,                                                                       \
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
        WAIT_FOR_MSG_BUFFER_DATA,
        WAIT_FOR_MSG_BUFFER_SPACE,
        WAIT_FOR_COND_VAR,
        WAIT_FOR_EVENT,
        WAIT_FOR_MAILBOX_SEND,
        WAIT_FOR_MAILBOX_RECEIVE,
        WAIT_FOR_NOTIFICATION,
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
        MSG_BUFFER_DATA_AVAILABLE,
        MSG_BUFFER_SPACE_AVAILABLE,
        COND_VAR_SIGNALLED,
        EVENT_MATCHED,
        MAILBOX_TRANSFER_DONE,
        TASK_NOTIFIED,
        TIMER_TIMEOUT,
        RESUME

    } wakeupReasonType;

    /**
     * @brief Notify action applied by taskNotify() to the target task's notification value.
     */
    typedef enum
    {
        TASK_NOTIFY_NO_ACTION,
        TASK_NOTIFY_SET_BITS,
        TASK_NOTIFY_INCREMENT,
        TASK_NOTIFY_SET_VALUE_WITH_OVERWRITE,
        TASK_NOTIFY_SET_VALUE_WITHOUT_OVERWRITE
    } taskNotifyActionType;

    /**
     * @brief Internal state used to track whether a task is waiting for or has received a notification.
     */
    typedef enum
    {
        TASK_NOTIFY_STATE_NOT_WAITING,
        TASK_NOTIFY_STATE_WAITING,
        TASK_NOTIFY_STATE_RECEIVED
    } taskNotifyStateType;

    /**
     * @brief Per-task mailbox exchange state used while waiting on a mailbox object.
     */
    typedef struct
    {
        mailboxMsgType msg; ///< Mailbox message descriptor saved while the task is waiting.
        void *pRxBuffer;    ///< Receiver buffer used by mailboxReceive().
    } taskMailboxStateType;

    /**
     * @brief Per-task state used by the event object wait APIs.
     */
    typedef struct
    {
        uint32_t waitMask;       ///< Event bits the task is currently waiting for.
        uint32_t matchedEvents;  ///< Event bits that matched when the task was woken.
        uint8_t waitAll;         ///< Non-zero if all bits in waitMask must be present.
        uint8_t clearOnExit;     ///< Non-zero if matched event bits should be cleared before wake-up.
    } taskEventStateType;

    /**
     * @brief Per-task state used by direct task notification APIs.
     */
    typedef struct
    {
        uint32_t value; ///< Notification value updated by taskNotify().
        uint8_t state;  ///< taskNotifyStateType stored as an 8-bit value.
    } taskNotificationType;

    /**
     * @brief Task Control Block (TCB) structure used to manage a task in the system.
     */
    typedef struct taskHandle
    {
        uint32_t stackPointer;           ///< Current value of the task's stack pointer (used during context switches).
        uint32_t flags;                  ///< Task flags (e.g. FPU usage, dynamic-resource ownership).
        uint32_t *stack;                 ///< Pointer to the base of the task's stack memory.
        const char *name;                ///< Human-readable name of the task (for debugging or logging).
        void *params;                    ///< Pointer to parameters passed to the task function.
        taskFunctionType entry;          ///< Function pointer to the task's entry function.
        uint32_t remainingSleepTicks;    ///< Number of ticks remaining for which the task is sleeping or being blocked.
        taskStatusType status;           ///< Current status of the task (e.g., running, ready, blocked).
        blockedReasonType blockedReason; ///< Reason the task is blocked (e.g., waiting for mutex/semaphore,sleeping).
        wakeupReasonType wakeupReason;   ///< Reason the task was woken up (e.g., timeout, signal).
        coreAffinityType coreAffinity;   ///< Core affinity for SMP systems (which core the task prefers or is pinned to).
        uint8_t priority;                ///< Priority level of the task (lower value indicate higher priority).
        taskEventStateType eventState;   ///< Event wait state used by the event kernel object.
        taskMailboxStateType mailboxState; ///< Mailbox wait state used by the mailbox kernel object.
        taskNotificationType notification; ///< Direct task notification state stored in the task itself.
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

    /**
     * @brief Set task state to READY and enqueue it in ready queue.
     *
     * @param pTask Pointer to task handle.
     * @param wakeupReason Reason for waking the task.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int taskSetReady(taskHandleType *pTask, wakeupReasonType wakeupReason);

    /**
     * @brief Block a task with a reason and timeout.
     *
     * @param pTask Pointer to task handle.
     * @param blockedReason Reason why task is blocked.
     * @param ticks Ticks to remain blocked.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int taskBlock(taskHandleType *pTask, blockedReasonType blockedReason, uint32_t ticks);

    /**
     * @brief Suspend a task.
     *
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int taskSuspend(taskHandleType *pTask);

    /**
     * @brief Resume a suspended task.
     *
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, `RET_NOTSUSPENDED` otherwise.
     */
    int taskResume(taskHandleType *pTask);

    /**
     * @brief Enqueue a task to ready queue.
     *
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int taskStart(taskHandleType *pTask);

    /**
     * @brief Send a direct notification to a task.
     *
     * The notification updates the target task's per-task notification value and,
     * if the task is blocked in a notification wait API, wakes it up.
     *
     * @param pTask Pointer to task handle to notify.
     * @param value Value used by the selected notify action.
     * @param action Notify action controlling how `value` is applied.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int taskNotify(taskHandleType *pTask, uint32_t value, taskNotifyActionType action);

    /**
     * @brief Wait for a direct task notification.
     *
     * This API is intended for value/bit based notification use-cases.
     *
     * @param clearMaskOnEntry Bits to clear from the notification value before checking for a pending notification.
     * @param clearMaskOnExit Bits to clear from the notification value before returning to the caller.
     * @param pValue Output pointer receiving the notification value observed by the wait.
     * @param waitTicks Number of RTOS ticks to wait.
     * @return `RET_SUCCESS` on success, `RET_BUSY` if no notification is pending and `waitTicks` is zero,
     *         `RET_TIMEOUT` on timeout, or another error code otherwise.
     */
    int taskNotifyWait(uint32_t clearMaskOnEntry, uint32_t clearMaskOnExit,
                       uint32_t *pValue, uint32_t waitTicks);

    /**
     * @brief Wait for a direct task notification and consume its count/value.
     *
     * This API is intended for semaphore-like notification flows, especially
     * when paired with `TASK_NOTIFY_INCREMENT`.
     *
     * @param clearCountOnExit If true, clear the notification value to zero before returning.
     *                         Otherwise decrement it by one when it is non-zero.
     * @param pPreviousValue Optional output pointer receiving the notification value observed by the take operation.
     * @param waitTicks Number of RTOS ticks to wait.
     * @return `RET_SUCCESS` on success, `RET_BUSY` if no notification is pending and `waitTicks` is zero,
     *         `RET_TIMEOUT` on timeout, or another error code otherwise.
     */
    int taskNotifyTake(bool clearCountOnExit, uint32_t *pPreviousValue, uint32_t waitTicks);

    /**
     * @brief Dynamically create and start a task.
     *
     * @param ppTask Output pointer receiving created task handle.
     * @param name Task name (optional).
     * @param stackSize Stack size in bytes.
     * @param taskEntryFunction Task entry function.
     * @param taskParams Entry function argument.
     * @param taskPriority Task priority.
     * @param affinity Core affinity.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int taskCreate(taskHandleType **ppTask, const char *name, uint32_t stackSize,
                   taskFunctionType taskEntryFunction, void *taskParams,
                   uint8_t taskPriority, coreAffinityType affinity);

    /**
     * @brief Delete a task.
     *
     * Dynamically-created task resources are freed by this API.
     *
     * @param pTask Pointer to task handle.
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    int taskDelete(taskHandleType *pTask);

    /**
     * @brief Check current task stack-overflow guard using the lowest known saved/live stack pointer.
     */
    void taskCheckStackOverflow();

    /**
     * @brief Block the current task for a specified number of RTOS ticks.
     *
     * @param sleepTicks Number of RTOS ticks to sleep
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    static inline __attribute__((always_inline)) int taskSleep(uint32_t sleepTicks)
    {
        return taskBlock(taskPool.currentTask[PORT_CORE_ID()], SLEEP, sleepTicks);
    }

    /**
     * @brief Block the current task for a specified number of milliseconds.
     *
     * @param sleepTimeMS Sleep duration in milliseconds
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    static inline __attribute__((always_inline)) int taskSleepMS(uint32_t sleepTimeMS)
    {
        return taskSleep(MS_TO_RTOS_TICKS(sleepTimeMS));
    }

    /**
     * @brief Block the current task for a specified number of microseconds.
     *
     * @param sleepTimeUS Sleep duration in microseconds
     * @return `RET_SUCCESS` on success, error code otherwise.
     */
    static inline __attribute__((always_inline)) int taskSleepUS(uint32_t sleepTimeUS)
    {
        return taskSleep(US_TO_RTOS_TICKS(sleepTimeUS));
    }

    /**
     * @brief Get the name of the currently running task.
     *
     * @return Pointer to the task name string
     */
    static inline __attribute__((always_inline)) const char *taskGetName()
    {
        return taskPool.currentTask[PORT_CORE_ID()]->name;
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
     * @brief Normalize task core affinity for the current build configuration.
     *
     * On single-core builds, any fixed-core affinity collapses to `AFFINITY_CORE_0`
     * so the task remains schedulable.
     *
     * @param affinity Requested task affinity.
     * @return Normalized affinity for this build.
     */
    static inline __attribute__((always_inline)) coreAffinityType taskNormalizeCoreAffinity(coreAffinityType affinity)
    {
#if (PORT_CORE_COUNT == 1)
        if (affinity != AFFINITY_CORE_ANY)
        {
            return AFFINITY_CORE_0;
        }
#endif

        return affinity;
    }

    /**
     * @brief Check whether a task can preempt the current core immediately.
     *
     * A task can preempt the current core only when it is eligible to run on this
     * core and has an equal or higher priority than the currently running task.
     *
     * @param pTask Pointer to the task being evaluated.
     * @retval `true` if a local yield can switch to this task on the current core.
     * @retval `false` otherwise.
     */
    static inline __attribute__((always_inline)) bool taskCanPreemptCurrentCore(taskHandleType *pTask)
    {
        taskHandleType *currentTask = taskPool.currentTask[PORT_CORE_ID()];

        if ((pTask == NULL) || (currentTask == NULL))
        {
            return false;
        }
#if CONFIG_SMP
        if ((pTask->coreAffinity != AFFINITY_CORE_ANY) &&
            (pTask->coreAffinity != PORT_CORE_ID()))
        {
            return false;
        }
#endif

        return (pTask->priority <= currentTask->priority);
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
        pTask->coreAffinity = taskNormalizeCoreAffinity(affinity);
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
