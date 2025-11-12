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

#include <stdio.h>
#include <string.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/config.h"
#include "sanoRTOS/spinLock.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/timer.h"
#include "sanoRTOS/mem.h"
#include "sanoRTOS/log.h"

LOG_MODULE_DEFINE(task);

taskPoolType taskPool = {0};

/*Currently scheduled task*/
taskHandleType *currentTask[PORT_CORE_COUNT];

/*Next task to be scheduled*/
taskHandleType *nextTask[PORT_CORE_COUNT];

static atomic_t lock;

void taskExitFunction()
{

    while (1)
        ;
}

static void taskInitStack(uint32_t *stack, uint32_t stackSize, taskFunctionType taskEntryFunction, void *taskParams)
{
    uint32_t stackWords = stackSize / sizeof(uint32_t);

    memset(stack, 0, stackSize);
    PORT_TASK_STACK_INIT(stack, stackWords, taskEntryFunction, taskExitFunction, taskParams);
}

static inline void taskDestroyDynamicResources(taskHandleType *pTask)
{
    if ((pTask->flags & TASK_FLAG_OWN_NAME) && (pTask->name != NULL))
    {
        memFree((void *)pTask->name);
    }

    if ((pTask->flags & TASK_FLAG_OWN_STACK) && (pTask->stack != NULL))
    {
        memFree(pTask->stack);
    }

    memFree(pTask);
}

static int taskSetReadyLocked(taskHandleType *pTask, wakeupReasonType wakeupReason)
{
    int retCode = RET_SUCCESS;

    if (pTask->status == TASK_STATUS_BLOCKED)
    {
        taskQueueType *pBlockedQueue = getBlockedQueue();

        /* Remove  task from the queue of blocked tasks*/
        retCode = taskQueueRemove(pBlockedQueue, pTask);
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            return retCode;
        }
    }

    pTask->status = TASK_STATUS_READY;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = wakeupReason;
    pTask->remainingSleepTicks = 0;

    taskQueueType *pReadyQueue = getReadyQueue();

    /* Add task to queue of ready tasks*/
    retCode = taskQueueAdd(pReadyQueue, pTask);

    return retCode;
}

static int taskBlockLocked(taskHandleType *pTask, blockedReasonType blockedReason, uint32_t ticks)
{
    pTask->remainingSleepTicks = ticks;
    pTask->status = TASK_STATUS_BLOCKED;
    pTask->blockedReason = blockedReason;
    pTask->wakeupReason = WAKEUP_REASON_NONE;

    taskQueueType *pBlockedQueue = getBlockedQueue();

    // Add task to queue of blocked tasks. We dont need to sort tasks in blockedQueue
    return taskQueueAddToFront(pBlockedQueue, pTask);
}

int taskSetReady(taskHandleType *pTask, wakeupReasonType wakeupReason)
{
    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&lock);
    int retCode = taskSetReadyLocked(pTask, wakeupReason);
    spinUnlock(&lock, irqState);

    return retCode;
}

int taskBlock(taskHandleType *pTask, blockedReasonType blockedReason, uint32_t ticks)
{
    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&lock);
    int retCode = taskBlockLocked(pTask, blockedReason, ticks);
    spinUnlock(&lock, irqState);

    if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    // Give CPU to other tasks
    taskYield();

    return RET_SUCCESS;
}

int taskSuspend(taskHandleType *pTask)
{
    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    int retCode = RET_SUCCESS;
    bool irqState = spinLock(&lock);

    /* If task status is ready, remove it from the readyQueue*/
    if (pTask->status == TASK_STATUS_READY)
    {
        taskQueueType *pReadyQueue = getReadyQueue();
        retCode = taskQueueRemove(pReadyQueue, pTask);
    }
    /*If task status is blocked, remove it from the blockedQueue*/
    else if (pTask->status == TASK_STATUS_BLOCKED)
    {
        taskQueueType *pBlockedQueue = getBlockedQueue();
        retCode = taskQueueRemove(pBlockedQueue, pTask);
    }

    if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
    {
        spinUnlock(&lock, irqState);
        return retCode;
    }

    pTask->remainingSleepTicks = 0;
    pTask->status = TASK_STATUS_SUSPENDED;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = WAKEUP_REASON_NONE;

    spinUnlock(&lock, irqState);

    /*If self suspended, give CPU to other tasks*/
    if (pTask == taskGetCurrent())
    {
        taskYield();
    }

    return RET_SUCCESS;
}

int taskResume(taskHandleType *pTask)
{
    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    if (pTask->status == TASK_STATUS_SUSPENDED)
    {
        return taskSetReady(pTask, RESUME);
    }

    return RET_NOTSUSPENDED;
}

int taskNotify(taskHandleType *pTask, uint32_t value, taskNotifyActionType action)
{
    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    int retCode = RET_SUCCESS;
    bool contextSwitchRequired = false;
    bool irqState = spinLock(&lock);

    /* Reject no-overwrite updates when a previous notification is still pending. */
    if ((action == TASK_NOTIFY_SET_VALUE_WITHOUT_OVERWRITE) &&
        (pTask->notification.state == TASK_NOTIFY_STATE_RECEIVED))
    {
        retCode = RET_BUSY;
    }
    else
    {
        switch (action)
        {
        case TASK_NOTIFY_NO_ACTION:
            break;

        case TASK_NOTIFY_SET_BITS:
            pTask->notification.value |= value;
            break;

        case TASK_NOTIFY_INCREMENT:
            pTask->notification.value++;
            break;

        case TASK_NOTIFY_SET_VALUE_WITH_OVERWRITE:
        case TASK_NOTIFY_SET_VALUE_WITHOUT_OVERWRITE:
            pTask->notification.value = value;
            break;

        default:
            retCode = RET_INVAL;
            break;
        }

        if (retCode == RET_SUCCESS)
        {
            pTask->notification.state = TASK_NOTIFY_STATE_RECEIVED;

            /* Wake the task immediately if it is blocked in a notification wait API. */
            if ((pTask->status == TASK_STATUS_BLOCKED) &&
                (pTask->blockedReason == WAIT_FOR_NOTIFICATION))
            {
                retCode = taskSetReadyLocked(pTask, TASK_NOTIFIED);
                if ((retCode == RET_SUCCESS) && taskCanPreemptCurrentCore(pTask))
                {
                    contextSwitchRequired = true;
                }
            }
        }
    }

    spinUnlock(&lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

int taskNotifyWait(uint32_t clearMaskOnEntry, uint32_t clearMaskOnExit,
                   uint32_t *pValue, uint32_t waitTicks)
{
    if (pValue == NULL)
    {
        return RET_INVAL;
    }

    if (portIsInISRContext())
    {
        return RET_INVAL;
    }

    taskHandleType *currentTask = taskGetCurrent();

retry:
    bool irqState = spinLock(&lock);
    int retCode;

    /* Clear requested bits before checking whether a notification is pending. */
    currentTask->notification.value &= ~clearMaskOnEntry;

    if (currentTask->notification.state == TASK_NOTIFY_STATE_RECEIVED)
    {
        *pValue = currentTask->notification.value;
        currentTask->notification.value &= ~clearMaskOnExit;
        currentTask->notification.state = TASK_NOTIFY_STATE_NOT_WAITING;
        retCode = RET_SUCCESS;
    }
    else if (waitTicks == TASK_NO_WAIT)
    {
        retCode = RET_BUSY;
    }
    else
    {
        /* Mark the task as waiting and block it while still holding the task lock. */
        currentTask->notification.state = TASK_NOTIFY_STATE_WAITING;

        retCode = taskBlockLocked(currentTask, WAIT_FOR_NOTIFICATION, waitTicks);
        if (retCode != RET_SUCCESS)
        {
            currentTask->notification.state = TASK_NOTIFY_STATE_NOT_WAITING;
            spinUnlock(&lock, irqState);
            return retCode;
        }

        spinUnlock(&lock, irqState);

        taskYield();

        irqState = spinLock(&lock);

        if (currentTask->notification.state == TASK_NOTIFY_STATE_RECEIVED)
        {
            *pValue = currentTask->notification.value;
            currentTask->notification.value &= ~clearMaskOnExit;
            currentTask->notification.state = TASK_NOTIFY_STATE_NOT_WAITING;
            retCode = RET_SUCCESS;
        }
        else if (currentTask->wakeupReason == WAIT_TIMEOUT)
        {
            currentTask->notification.state = TASK_NOTIFY_STATE_NOT_WAITING;
            retCode = RET_TIMEOUT;
        }
        else
        {
            currentTask->notification.state = TASK_NOTIFY_STATE_NOT_WAITING;
            spinUnlock(&lock, irqState);

            /* The task may have been resumed while waiting. Retry the notification wait. */
            goto retry;
        }
    }

    spinUnlock(&lock, irqState);

    return retCode;
}

int taskNotifyTake(bool clearCountOnExit, uint32_t *pPreviousValue, uint32_t waitTicks)
{
    if (portIsInISRContext())
    {
        return RET_INVAL;
    }

    taskHandleType *currentTask = taskGetCurrent();

retry:
    bool irqState = spinLock(&lock);
    int retCode;

    if (currentTask->notification.value != 0U)
    {
        if (pPreviousValue != NULL)
        {
            *pPreviousValue = currentTask->notification.value;
        }

        if (clearCountOnExit)
        {
            currentTask->notification.value = 0U;
            currentTask->notification.state = TASK_NOTIFY_STATE_NOT_WAITING;
        }
        else
        {
            currentTask->notification.value--;
            currentTask->notification.state = (currentTask->notification.value != 0U) ?
                                                 TASK_NOTIFY_STATE_RECEIVED :
                                                 TASK_NOTIFY_STATE_NOT_WAITING;
        }
        retCode = RET_SUCCESS;
    }
    else if (waitTicks == TASK_NO_WAIT)
    {
        retCode = RET_BUSY;
    }
    else
    {
        /* Block until any notification becomes pending for the current task. */
        currentTask->notification.state = TASK_NOTIFY_STATE_WAITING;

        retCode = taskBlockLocked(currentTask, WAIT_FOR_NOTIFICATION, waitTicks);
        if (retCode != RET_SUCCESS)
        {
            currentTask->notification.state = TASK_NOTIFY_STATE_NOT_WAITING;
            spinUnlock(&lock, irqState);
            return retCode;
        }

        spinUnlock(&lock, irqState);

        taskYield();

        irqState = spinLock(&lock);

        if (currentTask->notification.value != 0U)
        {
            if (pPreviousValue != NULL)
            {
                *pPreviousValue = currentTask->notification.value;
            }

            if (clearCountOnExit)
            {
                currentTask->notification.value = 0U;
                currentTask->notification.state = TASK_NOTIFY_STATE_NOT_WAITING;
            }
            else
            {
                currentTask->notification.value--;
                currentTask->notification.state = (currentTask->notification.value != 0U) ?
                                                     TASK_NOTIFY_STATE_RECEIVED :
                                                     TASK_NOTIFY_STATE_NOT_WAITING;
            }
            retCode = RET_SUCCESS;
        }
        else if (currentTask->wakeupReason == WAIT_TIMEOUT)
        {
            currentTask->notification.state = TASK_NOTIFY_STATE_NOT_WAITING;
            retCode = RET_TIMEOUT;
        }
        else
        {
            currentTask->notification.state = TASK_NOTIFY_STATE_NOT_WAITING;
            spinUnlock(&lock, irqState);

            /* The task may have been resumed while waiting. Retry the notification take. */
            goto retry;
        }
    }

    spinUnlock(&lock, irqState);

    return retCode;
}

int taskStart(taskHandleType *pTask)
{
    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    int retCode;
    bool irqState = spinLock(&lock);

    pTask->coreAffinity = taskNormalizeCoreAffinity(pTask->coreAffinity);

    taskQueueType *pReadyQueue = getReadyQueue();

    retCode = taskQueueAdd(pReadyQueue, pTask);

    spinUnlock(&lock, irqState);

    return retCode;
}

int taskCreate(taskHandleType **ppTask, const char *name, uint32_t stackSize,
               taskFunctionType taskEntryFunction, void *taskParams,
               uint8_t taskPriority, coreAffinityType affinity)
{
    taskHandleType *pTask = NULL;
    uint32_t *stack = NULL;
    char *taskName = NULL;

    if ((ppTask == NULL) || (taskEntryFunction == NULL) ||
        (stackSize < (PORT_INITIAL_TASK_STACK_OFFSET * sizeof(uint32_t))) ||
        ((stackSize % sizeof(uint32_t)) != 0))
    {
        return RET_INVAL;
    }

    pTask = (taskHandleType *)memAlloc(sizeof(taskHandleType));
    if (pTask == NULL)
    {
        return RET_NOMEM;
    }

    stack = (uint32_t *)memAlloc(stackSize);
    if (stack == NULL)
    {
        memFree(pTask);
        return RET_NOMEM;
    }

    if (name != NULL)
    {
        size_t nameLen = strlen(name) + 1U;
        taskName = (char *)memAlloc(nameLen);
        if (taskName == NULL)
        {
            memFree(stack);
            memFree(pTask);
            return RET_NOMEM;
        }
        memcpy(taskName, name, nameLen);
    }

    taskInitStack(stack, stackSize, taskEntryFunction, taskParams);

    memset(pTask, 0, sizeof(taskHandleType));
    pTask->stackPointer = (uint32_t)(stack + (stackSize / sizeof(uint32_t)) - PORT_INITIAL_TASK_STACK_OFFSET);
    pTask->stack = stack;
    pTask->name = (taskName != NULL) ? taskName : "dynamicTask";
    pTask->params = taskParams;
    pTask->entry = taskEntryFunction;
    pTask->remainingSleepTicks = 0;
    pTask->status = TASK_STATUS_READY;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = WAKEUP_REASON_NONE;
    pTask->coreAffinity = taskNormalizeCoreAffinity(affinity);
    pTask->priority = taskPriority;
    pTask->flags = TASK_FLAG_DYNAMIC | TASK_FLAG_OWN_STACK;

    if (taskName != NULL)
    {
        pTask->flags |= TASK_FLAG_OWN_NAME;
    }

    int retCode = taskStart(pTask);
    if (retCode != RET_SUCCESS)
    {
        taskDestroyDynamicResources(pTask);
        return retCode;
    }
    *ppTask = pTask;

    return RET_SUCCESS;
}

int taskDelete(taskHandleType *pTask)
{
    bool isDynamicTask;

    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&lock);

    if (pTask->status == TASK_STATUS_RUNNING)
    {
        spinUnlock(&lock, irqState);
        return RET_BUSY;
    }

    if (pTask->status == TASK_STATUS_READY)
    {
        int retCode = taskQueueRemove(getReadyQueue(), pTask);
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            spinUnlock(&lock, irqState);
            return retCode;
        }
    }
    else if (pTask->status == TASK_STATUS_BLOCKED)
    {
        int retCode = taskQueueRemove(getBlockedQueue(), pTask);
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            spinUnlock(&lock, irqState);
            return retCode;
        }
    }

    pTask->remainingSleepTicks = 0;
    pTask->status = TASK_STATUS_SUSPENDED;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = WAKEUP_REASON_NONE;

    isDynamicTask = ((pTask->flags & TASK_FLAG_DYNAMIC) != 0U);

    spinUnlock(&lock, irqState);

    if (isDynamicTask)
    {
        taskDestroyDynamicResources(pTask);
    }

    return RET_SUCCESS;
}

void taskCheckStackOverflow(void)
{
    taskHandleType *pCurrentTask = taskGetCurrent();

    if (pCurrentTask->stackPointer <= (uint32_t)(pCurrentTask->stack + STACK_GUARD_WORDS))
    {
        LOG_ERROR("%s stack overflow at address: %p", pCurrentTask->name, (void *)pCurrentTask->stackPointer);

        while (1)
            ;
    }
}
