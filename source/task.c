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
#include "sanoRTOS/memHeap.h"
#include "sanoRTOS/memSlab.h"
#include "sanoRTOS/log.h"
#include "taskInternal.h"

LOG_MODULE_DEFINE(task);

taskPoolType taskPool = {0};

/*Currently scheduled task*/
taskHandleType *currentTask[PORT_CORE_COUNT];

/*Next task to be scheduled*/
taskHandleType *nextTask[PORT_CORE_COUNT];

/* Shared by task.c and scheduler.c to serialize task state and scheduler queues. */
atomicType taskStateLock;
static taskHandleType *exitedTask[PORT_CORE_COUNT];

MEM_SLAB_DEFINE(dynamicTaskTcbSlab, sizeof(taskHandleType), CONFIG_DYNAMIC_TASK_TCB_SLAB_BLOCKS);

static void taskInitStack(uint32_t *stack, uint32_t stackSize, taskFunctionType taskEntryFunction, void *taskParams)
{
    uint32_t stackWords = stackSize / sizeof(uint32_t);

    memset(stack, 0, stackSize);
    PORT_TASK_STACK_INIT(stack, stackWords, taskEntryFunction, taskExit, taskParams);
}

static inline void taskDestroyDynamicResources(taskHandleType *pTask)
{
    if (pTask == NULL)
    {
        return;
    }

    if ((pTask->flags & TASK_FLAG_OWN_NAME) && (pTask->name != NULL))
    {
        memHeapFree((void *)pTask->name);
    }

    if ((pTask->flags & TASK_FLAG_OWN_STACK) && (pTask->stack != NULL))
    {
        memHeapFree(pTask->stack);
    }

    (void)memSlabFree(&dynamicTaskTcbSlab, pTask);
}

static inline void taskResetInactiveState(taskHandleType *pTask)
{
    if (pTask == NULL)
    {
        return;
    }

    pTask->deadlineTick = 0;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = WAKEUP_REASON_NONE;
    memset(&pTask->eventState, 0, sizeof(pTask->eventState));
    memset(&pTask->mailboxState, 0, sizeof(pTask->mailboxState));
    pTask->notification.value = 0U;
    pTask->notification.state = TASK_NOTIFY_STATE_NOT_WAITING;
    memset(&pTask->stateQueueLink, 0, sizeof(pTask->stateQueueLink));
    memset(&pTask->waitQueueLink, 0, sizeof(pTask->waitQueueLink));
    memset(&pTask->timeoutQueueLink, 0, sizeof(pTask->timeoutQueueLink));
}

static inline bool taskDeadlineReached(uint32_t currentTick, uint32_t deadlineTick)
{
    return ((int32_t)(currentTick - deadlineTick) >= 0);
}

void taskCleanupExited()
{
    taskHandleType *pTaskToDestroy = NULL;
    uint8_t coreIndex = PORT_CORE_ID();

    bool irqState = spinLock(&taskStateLock);

    taskHandleType *pTask = exitedTask[coreIndex];
    if ((pTask != NULL) && (pTask != taskPool.currentTask[coreIndex]))
    {
        exitedTask[coreIndex] = NULL;
        pTaskToDestroy = pTask;
    }

    spinUnlock(&taskStateLock, irqState);

    if (pTaskToDestroy != NULL)
    {
        taskDestroyDynamicResources(pTaskToDestroy);
    }
}

static void taskStateLocksRelease(taskQueueType *pWaitQueue,
                                  bool taskIrqState,
                                  bool objectIrqState)
{
    spinUnlock(&taskStateLock, taskIrqState);

    if ((pWaitQueue != NULL) && (pWaitQueue->pLock != NULL))
    {
        spinUnlock(pWaitQueue->pLock, objectIrqState);
    }
}

static void taskStateLocksAcquire(taskHandleType *pTask,
                                  taskQueueType **ppWaitQueue,
                                  bool *pTaskIrqState,
                                  bool *pObjectIrqState)
{
    taskQueueType *pWaitQueue;

    if ((ppWaitQueue == NULL) || (pTaskIrqState == NULL) || (pObjectIrqState == NULL))
    {
        return;
    }

    while (1)
    {
        /* Another core may change the task's state or wait-queue ownership after this snapshot. */
        pWaitQueue = pTask->waitQueueLink.pOwnerQueue;
        if ((pTask->state == TASK_STATE_BLOCKED) &&
            (pWaitQueue != NULL) &&
            (pWaitQueue->pLock != NULL))
        {
            *pObjectIrqState = spinLock(pWaitQueue->pLock);
            *pTaskIrqState = spinLock(&taskStateLock);
            if ((pTask->state == TASK_STATE_BLOCKED) &&
                (pTask->waitQueueLink.pOwnerQueue == pWaitQueue))
            {
                *ppWaitQueue = pWaitQueue;
                return;
            }

            /* Retry until the task is still blocked on the same wait queue whose lock we hold. */
            taskStateLocksRelease(pWaitQueue, *pTaskIrqState, *pObjectIrqState);
            continue;
        }

        *pTaskIrqState = spinLock(&taskStateLock);
        pWaitQueue = pTask->waitQueueLink.pOwnerQueue;

        if ((pTask->state == TASK_STATE_BLOCKED) &&
            (pWaitQueue != NULL) &&
            (pWaitQueue->pLock != NULL))
        {
            /* The task moved onto an object wait queue before we got the task lock; retry with that lock too. */
            spinUnlock(&taskStateLock, *pTaskIrqState);
            continue;
        }

        *ppWaitQueue = NULL;
        *pObjectIrqState = false;
        return;
    }
}

static int taskSetReadyLocked(taskHandleType *pTask, wakeupReasonType wakeupReason)
{
    int retCode = RET_SUCCESS;

    if ((pTask == NULL) || (pTask->state == TASK_STATE_TERMINATED))
    {
        return RET_INVAL;
    }

    if ((pTask->state == TASK_STATE_READY) || (pTask->state == TASK_STATE_RUNNING))
    {
        return RET_SUCCESS;
    }

    if (pTask->state == TASK_STATE_BLOCKED)
    {
        retCode = blockedQueueRemove(pTask);
        /* RET_NOTASK is benign here: a wake path may have already detached this link. */
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            return retCode;
        }

        retCode = waitQueueRemove(pTask);
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            return retCode;
        }

        retCode = timeoutQueueRemove(pTask);
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            return retCode;
        }
    }

    pTask->state = TASK_STATE_READY;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = wakeupReason;
    pTask->deadlineTick = 0;

    /* Add task to queue of ready tasks*/
    retCode = readyQueueAdd(pTask);

    return retCode;
}

static int taskBlockLocked(taskHandleType *pTask, blockedReasonType blockedReason, uint32_t ticks)
{
    taskStateType previousState;
    blockedReasonType previousBlockedReason;
    wakeupReasonType previousWakeupReason;
    uint32_t previousDeadlineTick;
    int retCode;

    if ((pTask == NULL) || (pTask->state == TASK_STATE_TERMINATED))
    {
        return RET_INVAL;
    }

    previousState = pTask->state;
    previousBlockedReason = pTask->blockedReason;
    previousWakeupReason = pTask->wakeupReason;
    previousDeadlineTick = pTask->deadlineTick;

    pTask->deadlineTick = (ticks == TASK_FOREVER_WAIT) ? 0U : (schedulerGetTickCount() + ticks);
    pTask->state = TASK_STATE_BLOCKED;
    pTask->blockedReason = blockedReason;
    pTask->wakeupReason = WAKEUP_REASON_NONE;

    /* Add task to queue of blocked tasks. We dont need to sort tasks in blockedQueue. */
    retCode = blockedQueueAdd(pTask);
    if (retCode != RET_SUCCESS)
    {
        pTask->deadlineTick = previousDeadlineTick;
        pTask->state = previousState;
        pTask->blockedReason = previousBlockedReason;
        pTask->wakeupReason = previousWakeupReason;
        return retCode;
    }

    if (ticks != TASK_FOREVER_WAIT)
    {
        retCode = timeoutQueueAdd(pTask);
        if (retCode != RET_SUCCESS)
        {
            (void)blockedQueueRemove(pTask);
            pTask->deadlineTick = previousDeadlineTick;
            pTask->state = previousState;
            pTask->blockedReason = previousBlockedReason;
            pTask->wakeupReason = previousWakeupReason;
            return retCode;
        }
    }

    return RET_SUCCESS;
}

static int taskSetReadyInternal(taskHandleType *pTask, wakeupReasonType wakeupReason)
{
    taskQueueType *pWaitQueue = NULL;

    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    bool taskIrqState;
    bool objectIrqState;

    taskStateLocksAcquire(pTask, &pWaitQueue, &taskIrqState, &objectIrqState);
    int retCode = taskSetReadyLocked(pTask, wakeupReason);
    taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);

    return retCode;
}

static int taskBlock(blockedReasonType blockedReason, uint32_t ticks)
{
    bool irqState = spinLock(&taskStateLock);
    taskHandleType *pTask = taskGetCurrent();

    if (pTask == NULL)
    {
        spinUnlock(&taskStateLock, irqState);
        return RET_INVAL;
    }

    int retCode = taskBlockLocked(pTask, blockedReason, ticks);
    spinUnlock(&taskStateLock, irqState);

    if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    /* Give CPU to other tasks */
    taskYield();

    return RET_SUCCESS;
}

int taskSleep(uint32_t sleepTicks)
{
    return taskBlock(SLEEP, sleepTicks);
}

int taskBlockOnWaitQueue(taskQueueType *pWaitQueue,
                         blockedReasonType blockedReason,
                         uint32_t ticks,
                         bool objectIrqState)
{
    taskHandleType *pTask;
    bool taskIrqState;
    int retCode;

    if ((pWaitQueue == NULL) || (pWaitQueue->pLock == NULL))
    {
        return RET_INVAL;
    }

    pTask = taskGetCurrent();
    if (pTask == NULL)
    {
        spinUnlock(pWaitQueue->pLock, objectIrqState);
        return RET_INVAL;
    }

    taskIrqState = spinLock(&taskStateLock);

    retCode = waitQueueAdd(pWaitQueue, pTask);
    if (retCode == RET_SUCCESS)
    {
        retCode = taskBlockLocked(pTask, blockedReason, ticks);
    }

    if (retCode != RET_SUCCESS)
    {
        (void)waitQueueRemove(pTask);
        taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);
        return retCode;
    }

    taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);

    taskYield();

    return RET_SUCCESS;
}

int taskSetReady(taskHandleType *pTask,
                 wakeupReasonType wakeupReason)
{
    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&taskStateLock);
    int retCode = taskSetReadyLocked(pTask, wakeupReason);
    spinUnlock(&taskStateLock, irqState);

    return retCode;
}

int taskSuspend(taskHandleType *pTask)
{
    taskQueueType *pWaitQueue = NULL;

    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    if (pTask->state == TASK_STATE_TERMINATED)
    {
        return RET_INVAL;
    }

    int retCode = RET_SUCCESS;
    bool taskIrqState;
    bool objectIrqState;

    taskStateLocksAcquire(pTask, &pWaitQueue, &taskIrqState, &objectIrqState);

    if (pTask->state == TASK_STATE_READY)
    {
        retCode = readyQueueRemove(pTask);
    }
    else if (pTask->state == TASK_STATE_BLOCKED)
    {
        retCode = blockedQueueRemove(pTask);
        /* RET_NOTASK is benign here: a wake path may have already detached this link. */
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);
            return retCode;
        }

        retCode = waitQueueRemove(pTask);
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);
            return retCode;
        }

        retCode = timeoutQueueRemove(pTask);
    }
    if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
    {
        taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);
        return retCode;
    }

    pTask->deadlineTick = 0;
    pTask->state = TASK_STATE_SUSPENDED;
    pTask->blockedReason = BLOCK_REASON_NONE;
    pTask->wakeupReason = WAKEUP_REASON_NONE;

    taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);

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

    if (pTask->state == TASK_STATE_TERMINATED)
    {
        return RET_INVAL;
    }

    if (pTask->state == TASK_STATE_SUSPENDED)
    {
        return taskSetReadyInternal(pTask, RESUME);
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
    bool irqState = spinLock(&taskStateLock);

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
            if ((pTask->state == TASK_STATE_BLOCKED) &&
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

    spinUnlock(&taskStateLock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

int taskNotifyWait(uint32_t clearMaskOnEntry, uint32_t clearMaskOnExit,
                   uint32_t *pValue, uint32_t waitTicks)
{
    taskHandleType *currentTask;
    bool irqState;
    int retCode;

    if (pValue == NULL)
    {
        return RET_INVAL;
    }

    currentTask = taskGetCurrent();

retry:
    irqState = spinLock(&taskStateLock);

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
            spinUnlock(&taskStateLock, irqState);
            return retCode;
        }

        spinUnlock(&taskStateLock, irqState);

        taskYield();

        irqState = spinLock(&taskStateLock);

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
            spinUnlock(&taskStateLock, irqState);

            /* The task may have been resumed while waiting. Retry the notification wait. */
            goto retry;
        }
    }

    spinUnlock(&taskStateLock, irqState);

    return retCode;
}

int taskNotifyTake(bool clearCountOnExit, uint32_t *pPreviousValue, uint32_t waitTicks)
{
    taskHandleType *currentTask;
    bool irqState;
    int retCode;

    currentTask = taskGetCurrent();

retry:
    irqState = spinLock(&taskStateLock);

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
            currentTask->notification.state = (currentTask->notification.value != 0U) ? TASK_NOTIFY_STATE_RECEIVED : TASK_NOTIFY_STATE_NOT_WAITING;
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
            spinUnlock(&taskStateLock, irqState);
            return retCode;
        }

        spinUnlock(&taskStateLock, irqState);

        taskYield();

        irqState = spinLock(&taskStateLock);

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
                currentTask->notification.state = (currentTask->notification.value != 0U) ? TASK_NOTIFY_STATE_RECEIVED : TASK_NOTIFY_STATE_NOT_WAITING;
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
            spinUnlock(&taskStateLock, irqState);

            /* The task may have been resumed while waiting. Retry the notification take. */
            goto retry;
        }
    }

    spinUnlock(&taskStateLock, irqState);

    return retCode;
}

int taskStart(taskHandleType *pTask)
{
    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    if ((pTask->state == TASK_STATE_TERMINATED) || (pTask->priority > TASK_LOWEST_PRIORITY))
    {
        return RET_INVAL;
    }

    int retCode;
    bool irqState = spinLock(&taskStateLock);

    pTask->coreAffinity = taskNormalizeCoreAffinity(pTask->coreAffinity);

    retCode = readyQueueAdd(pTask);

    spinUnlock(&taskStateLock, irqState);

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
        (stackSize < ((PORT_INITIAL_TASK_STACK_OFFSET + STACK_GUARD_WORDS) * sizeof(uint32_t))) ||
        ((stackSize % sizeof(uint32_t)) != 0) ||
        (taskPriority > TASK_LOWEST_PRIORITY))
    {
        return RET_INVAL;
    }

    if (memSlabAlloc(&dynamicTaskTcbSlab, (void **)&pTask, TASK_NO_WAIT) != RET_SUCCESS)
    {
        return RET_NOMEM;
    }

    stack = (uint32_t *)memHeapAlloc(stackSize);
    if (stack == NULL)
    {
        (void)memSlabFree(&dynamicTaskTcbSlab, pTask);
        return RET_NOMEM;
    }

    if (name != NULL)
    {
        size_t nameLen = strlen(name) + 1U;
        taskName = (char *)memHeapAlloc(nameLen);
        if (taskName == NULL)
        {
            memHeapFree(stack);
            (void)memSlabFree(&dynamicTaskTcbSlab, pTask);
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
    pTask->deadlineTick = 0;
    pTask->state = TASK_STATE_READY;
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
    taskQueueType *pWaitQueue = NULL;

    if (pTask == NULL)
    {
        return RET_INVAL;
    }

    bool taskIrqState;
    bool objectIrqState;

    taskStateLocksAcquire(pTask, &pWaitQueue, &taskIrqState, &objectIrqState);

    if (pTask->state == TASK_STATE_RUNNING)
    {
        taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);
        return RET_BUSY;
    }

    if ((pTask->flags & TASK_FLAG_EXIT_PENDING) != 0U)
    {
        taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);
        return RET_BUSY;
    }

    if (pTask->state == TASK_STATE_TERMINATED)
    {
        taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);
        return RET_SUCCESS;
    }

    if (pTask->state == TASK_STATE_READY)
    {
        int retCode = readyQueueRemove(pTask);
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);
            return retCode;
        }
    }
    else if (pTask->state == TASK_STATE_BLOCKED)
    {
        int retCode = blockedQueueRemove(pTask);
        /* RET_NOTASK is benign here: a wake path may have already detached this link. */
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);
            return retCode;
        }

        retCode = waitQueueRemove(pTask);
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);
            return retCode;
        }

        retCode = timeoutQueueRemove(pTask);
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);
            return retCode;
        }
    }

    taskResetInactiveState(pTask);
    pTask->state = TASK_STATE_TERMINATED;

    isDynamicTask = ((pTask->flags & TASK_FLAG_DYNAMIC) != 0U);

    taskStateLocksRelease(pWaitQueue, taskIrqState, objectIrqState);

    if (isDynamicTask)
    {
        taskDestroyDynamicResources(pTask);
    }

    return RET_SUCCESS;
}

void taskProcessExpiredTimeouts(uint32_t currentTick)
{
    while (true)
    {
        bool irqState = spinLock(&taskStateLock);
        taskHandleType *pTask = timeoutQueuePeek();
        wakeupReasonType wakeupReason;
        int retCode;

        if ((pTask == NULL) || !taskDeadlineReached(currentTick, pTask->deadlineTick))
        {
            spinUnlock(&taskStateLock, irqState);
            break;
        }

        wakeupReason = (pTask->blockedReason == SLEEP) ? SLEEP_TIME_TIMEOUT : WAIT_TIMEOUT;
        spinUnlock(&taskStateLock, irqState);

        retCode = taskSetReadyInternal(pTask, wakeupReason);
        if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
        {
            break;
        }
    }
}

void taskExit(void)
{
    taskCleanupExited();

    taskHandleType *pCurrentTask;

    bool irqState = spinLock(&taskStateLock);

    pCurrentTask = taskGetCurrent();
    if (pCurrentTask == NULL)
    {
        spinUnlock(&taskStateLock, irqState);
        while (1)
        {
            PORT_NOP();
        }
    }

    taskResetInactiveState(pCurrentTask);
    pCurrentTask->state = TASK_STATE_TERMINATED;

    if ((pCurrentTask->flags & TASK_FLAG_DYNAMIC) != 0U)
    {
        pCurrentTask->flags |= TASK_FLAG_EXIT_PENDING;
        exitedTask[PORT_CORE_ID()] = pCurrentTask;
    }

    spinUnlock(&taskStateLock, irqState);

    taskYield();

    while (1)
    {
        PORT_NOP();
    }
}

void taskCheckStackOverflow(void)
{
    taskHandleType *pCurrentTask = taskGetCurrent();
    uint32_t stackPointer;
    uint32_t liveStackPointer;

    if ((pCurrentTask == NULL) || (pCurrentTask->stack == NULL))
    {
        return;
    }

    stackPointer = pCurrentTask->stackPointer;
    liveStackPointer = portGetCurrentStackPointer();

    if (liveStackPointer < stackPointer)
    {
        stackPointer = liveStackPointer;
    }

    if (stackPointer < (uint32_t)(pCurrentTask->stack + STACK_GUARD_WORDS))
    {
        LOG_ERROR("%s stack overflow at address: %p", pCurrentTask->name, (void *)stackPointer);

        while (1)
            ;
    }
}
