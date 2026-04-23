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

#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"
#include "sanoRTOS/event.h"

static inline uint32_t eventMatchedMask(uint32_t activeEvents, uint32_t waitMask)
{
    return activeEvents & waitMask;
}

static inline bool eventConditionMatched(uint32_t activeEvents, uint32_t waitMask, bool waitAll)
{
    uint32_t matchedEvents = eventMatchedMask(activeEvents, waitMask);

    if (waitAll)
    {
        return matchedEvents == waitMask;
    }

    return matchedEvents != 0U;
}

static inline void eventTaskWaitReset(taskHandleType *pTask)
{
    if (pTask == NULL)
    {
        return;
    }

    pTask->eventState.waitMask = 0U;
    pTask->eventState.matchedEvents = 0U;
    pTask->eventState.waitAll = 0U;
    pTask->eventState.clearOnExit = 0U;
}

static inline void eventTaskWaitSetup(taskHandleType *pTask, uint32_t waitMask, bool waitAll, bool clearOnExit)
{
    if (pTask == NULL)
    {
        return;
    }

    pTask->eventState.waitMask = waitMask;
    pTask->eventState.matchedEvents = 0U;
    pTask->eventState.waitAll = waitAll ? 1U : 0U;
    pTask->eventState.clearOnExit = clearOnExit ? 1U : 0U;
}

static int eventWakeMatchingTasks(eventHandleType *pEvent, bool *pContextSwitchRequired)
{
    if (pEvent == NULL)
    {
        return RET_INVAL;
    }

    uint32_t clearMask = 0U;
    taskNodeType *currentTaskNode = pEvent->waitQueue.head;

    /* Check each task in the wait queue for a matching event condition */
    while (currentTaskNode != NULL)
    {
        taskNodeType *nextTaskNode = currentTaskNode->nextTaskNode;
        taskHandleType *pTask = currentTaskNode->pTask;
        uint32_t matchedEvents = 0U;

        /* Clear any stale match state before re-evaluating the waiter */
        pTask->eventState.matchedEvents = 0U;

        if ((pTask->status == TASK_STATUS_BLOCKED) &&
            (pTask->blockedReason == WAIT_FOR_EVENT) &&
            eventConditionMatched(pEvent->events, pTask->eventState.waitMask, (pTask->eventState.waitAll != 0U)))
        {
            matchedEvents = eventMatchedMask(pEvent->events, pTask->eventState.waitMask);
            pTask->eventState.matchedEvents = matchedEvents;

            /* Accumulate matched bits to clear after all waiters have been checked */
            if (pTask->eventState.clearOnExit != 0U)
            {
                clearMask |= matchedEvents;
            }

            /* Remove matched task from the event wait queue before making it READY */
            int retCode = taskQueueRemove(&pEvent->waitQueue, pTask);
            if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
            {
                return retCode;
            }

            retCode = taskSetReady(pTask, EVENT_MATCHED);
            if (retCode != RET_SUCCESS)
            {
                return retCode;
            }

            if (taskCanPreemptCurrentCore(pTask))
            {
                *pContextSwitchRequired = true;
            }
        }

        currentTaskNode = nextTaskNode;
    }

    /* Clear all matched event bits requested with clearOnExit */
    pEvent->events &= ~clearMask;

    return RET_SUCCESS;
}

static int eventWaitCommon(eventHandleType *pEvent, uint32_t waitEvents,
                           bool waitAll, bool clearOnExit,
                           uint32_t setEvents, uint32_t *pMatchedEvents,
                           uint32_t waitTicks)
{
    if ((pEvent == NULL) || (waitEvents == 0U) || (pMatchedEvents == NULL))
    {
        return RET_INVAL;
    }

    if (portIsInISRContext() && (waitTicks != TASK_NO_WAIT))
    {
        return RET_INVAL;
    }

    *pMatchedEvents = 0U;

    bool contextSwitchRequired;
    bool irqState;
    int retCode;

retry:
    contextSwitchRequired = false;
    irqState = spinLock(&pEvent->lock);

    /* Optionally set event bits before testing the wait condition */
    if (setEvents != 0U)
    {
        pEvent->events |= setEvents;

        retCode = eventWakeMatchingTasks(pEvent, &contextSwitchRequired);
        if (retCode != RET_SUCCESS)
        {
            spinUnlock(&pEvent->lock, irqState);
            return retCode;
        }
    }

    /* If the requested condition is already satisfied, return immediately */
    if (eventConditionMatched(pEvent->events, waitEvents, waitAll))
    {
        *pMatchedEvents = eventMatchedMask(pEvent->events, waitEvents);

        if (clearOnExit)
        {
            pEvent->events &= ~(*pMatchedEvents);
        }
        retCode = RET_SUCCESS;
    }
    else if (waitTicks == TASK_NO_WAIT)
    {
        /*Wait condition not met and caller requested no-wait */
        retCode = RET_BUSY;
    }
    else
    {
        taskHandleType *currentTask = taskGetCurrent();

        /* Save current task's event wait criteria before blocking */
        eventTaskWaitSetup(currentTask, waitEvents, waitAll, clearOnExit);

        /* Add current task to the event object's wait queue */
        retCode = taskQueueAdd(&pEvent->waitQueue, currentTask);
        if (retCode != RET_SUCCESS)
        {
            eventTaskWaitReset(currentTask);
            spinUnlock(&pEvent->lock, irqState);
            return retCode;
        }

        spinUnlock(&pEvent->lock, irqState);

        /* Block current task and give CPU to other tasks while waiting for events */
        retCode = taskBlock(currentTask, WAIT_FOR_EVENT, waitTicks);
        if (retCode != RET_SUCCESS)
        {
            irqState = spinLock(&pEvent->lock);
            (void)taskQueueRemove(&pEvent->waitQueue, currentTask);
            eventTaskWaitReset(currentTask);
            spinUnlock(&pEvent->lock, irqState);
            return retCode;
        }

        irqState = spinLock(&pEvent->lock);

        /*Task has been woken up either due to event match or wait timeout */
        if (currentTask->wakeupReason == EVENT_MATCHED)
        {
            *pMatchedEvents = currentTask->eventState.matchedEvents;
            eventTaskWaitReset(currentTask);
            retCode = RET_SUCCESS;
        }
        else if (currentTask->wakeupReason == WAIT_TIMEOUT)
        {
            /*Wait timed out, remove task from the waitQueue */
            retCode = taskQueueRemove(&pEvent->waitQueue, currentTask);
            eventTaskWaitReset(currentTask);

            if ((retCode == RET_SUCCESS) || (retCode == RET_NOTASK))
            {
                retCode = RET_TIMEOUT;
            }
        }
        else
        {
            /*Task might have been suspended while waiting for events and later resumed.
              In this case, retry waiting on the event object again */
            retCode = taskQueueRemove(&pEvent->waitQueue, currentTask);
            eventTaskWaitReset(currentTask);
            spinUnlock(&pEvent->lock, irqState);

            if ((retCode != RET_SUCCESS) && (retCode != RET_NOTASK))
            {
                return retCode;
            }

            goto retry;
        }
    }

    spinUnlock(&pEvent->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

int eventSet(eventHandleType *pEvent, uint32_t events)
{
    if (pEvent == NULL)
    {
        return RET_INVAL;
    }

    if (events == 0U)
    {
        return RET_SUCCESS;
    }

    bool contextSwitchRequired = false;
    bool irqState = spinLock(&pEvent->lock);

    pEvent->events |= events;

    int retCode = eventWakeMatchingTasks(pEvent, &contextSwitchRequired);

    spinUnlock(&pEvent->lock, irqState);

    if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return RET_SUCCESS;
}

int eventClear(eventHandleType *pEvent, uint32_t events)
{
    if (pEvent == NULL)
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&pEvent->lock);

    pEvent->events &= ~events;

    spinUnlock(&pEvent->lock, irqState);

    return RET_SUCCESS;
}

int eventGet(eventHandleType *pEvent, uint32_t *pEvents)
{
    if ((pEvent == NULL) || (pEvents == NULL))
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&pEvent->lock);

    *pEvents = pEvent->events;

    spinUnlock(&pEvent->lock, irqState);

    return RET_SUCCESS;
}

int eventWaitAny(eventHandleType *pEvent, uint32_t events,
                 bool clearOnExit, uint32_t *pMatchedEvents, uint32_t waitTicks)
{
    return eventWaitCommon(pEvent, events, false, clearOnExit, 0U, pMatchedEvents, waitTicks);
}

int eventWaitAll(eventHandleType *pEvent, uint32_t events,
                 bool clearOnExit, uint32_t *pMatchedEvents, uint32_t waitTicks)
{
    return eventWaitCommon(pEvent, events, true, clearOnExit, 0U, pMatchedEvents, waitTicks);
}

int eventSync(eventHandleType *pEvent, uint32_t setEvents,
              uint32_t waitEvents, uint32_t *pMatchedEvents, uint32_t waitTicks)
{
    return eventWaitCommon(pEvent, waitEvents, true, true, setEvents, pMatchedEvents, waitTicks);
}
