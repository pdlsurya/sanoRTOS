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

#include "sanoRTOS/workQueue.h"
#include "sanoRTOS/task.h"

static void workQueuePushLocked(workQueueHandleType *pWorkQueue, workItemType *pWork)
{
    if ((pWorkQueue == NULL) || (pWork == NULL))
    {
        return;
    }

    pWork->pNextWork = NULL;
    pWork->pending = true;

    if (pWorkQueue->tail == NULL)
    {
        pWorkQueue->head = pWork;
        pWorkQueue->tail = pWork;
    }
    else
    {
        pWorkQueue->tail->pNextWork = pWork;
        pWorkQueue->tail = pWork;
    }
}

static workItemType *workQueuePop(workQueueHandleType *pWorkQueue)
{
    if (pWorkQueue == NULL)
    {
        return NULL;
    }

    workItemType *pWork = NULL;
    bool irqState = spinLock(&pWorkQueue->lock);

    if (pWorkQueue->head != NULL)
    {
        pWork = pWorkQueue->head;
        pWorkQueue->head = pWork->pNextWork;

        if (pWorkQueue->head == NULL)
        {
            pWorkQueue->tail = NULL;
        }

        pWork->pNextWork = NULL;
        pWork->pending = false;
    }

    spinUnlock(&pWorkQueue->lock, irqState);

    return pWork;
}

static bool delayedWorkIsQueued(delayedWorkType *pDelayedWork)
{
    if ((pDelayedWork == NULL) || (pDelayedWork->pWorkQueue == NULL))
    {
        return false;
    }

    bool pending;
    bool irqState = spinLock(&pDelayedWork->pWorkQueue->lock);
    pending = pDelayedWork->work.pending;
    spinUnlock(&pDelayedWork->pWorkQueue->lock, irqState);

    return pending;
}

static int workQueueSubmitInternal(workQueueHandleType *pWorkQueue, workItemType *pWork)
{
    if ((pWorkQueue == NULL) || (pWorkQueue->pWorkerTask == NULL) ||
        (pWork == NULL) || (pWork->handler == NULL))
    {
        return RET_INVAL;
    }

    int retCode = RET_SUCCESS;
    bool irqState = spinLock(&pWorkQueue->lock);

    /* Reject duplicate submits while the work item is already queued. */
    if (pWork->pending)
    {
        retCode = RET_BUSY;
    }
    else
    {
        workQueuePushLocked(pWorkQueue, pWork);
    }

    spinUnlock(&pWorkQueue->lock, irqState);

    return retCode;
}

int workQueueStart(workQueueHandleType *pWorkQueue)
{
    if ((pWorkQueue == NULL) || (pWorkQueue->pWorkerTask == NULL))
    {
        return RET_INVAL;
    }

    return taskStart(pWorkQueue->pWorkerTask);
}

int workSubmit(workQueueHandleType *pWorkQueue, workItemType *pWork)
{
    int retCode = workQueueSubmitInternal(pWorkQueue, pWork);

    if (retCode == RET_SUCCESS)
    {
        retCode = taskNotify(pWorkQueue->pWorkerTask, 0U, TASK_NOTIFY_INCREMENT);
    }

    return retCode;
}

int delayedWorkSchedule(workQueueHandleType *pWorkQueue, delayedWorkType *pDelayedWork,
                        uint32_t delayTicks)
{
    if ((pWorkQueue == NULL) || (pDelayedWork == NULL) || (pDelayedWork->work.handler == NULL))
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&pDelayedWork->lock);

    if (pDelayedWork->scheduled || delayedWorkIsQueued(pDelayedWork))
    {
        spinUnlock(&pDelayedWork->lock, irqState);
        return RET_BUSY;
    }

    pDelayedWork->pWorkQueue = pWorkQueue;

    if (delayTicks == 0U)
    {
        int retCode = workQueueSubmitInternal(pWorkQueue, &pDelayedWork->work);
        spinUnlock(&pDelayedWork->lock, irqState);

        if (retCode == RET_SUCCESS)
        {
            retCode = taskNotify(pWorkQueue->pWorkerTask, 0U, TASK_NOTIFY_INCREMENT);
        }

        return retCode;
    }

    pDelayedWork->scheduled = true;
    int retCode = timerStart(&pDelayedWork->timer, delayTicks);
    if (retCode != RET_SUCCESS)
    {
        pDelayedWork->scheduled = false;
    }

    spinUnlock(&pDelayedWork->lock, irqState);

    return retCode;
}

int delayedWorkCancel(delayedWorkType *pDelayedWork)
{
    if (pDelayedWork == NULL)
    {
        return RET_INVAL;
    }

    int retCode;
    bool irqState = spinLock(&pDelayedWork->lock);

    if (pDelayedWork->scheduled)
    {
        retCode = timerStop(&pDelayedWork->timer);
        if (retCode == RET_SUCCESS)
        {
            pDelayedWork->scheduled = false;
        }
        else if (retCode == RET_NOTACTIVE)
        {
            retCode = RET_BUSY;
        }
    }
    else if (delayedWorkIsQueued(pDelayedWork))
    {
        retCode = RET_BUSY;
    }
    else
    {
        retCode = RET_NOTACTIVE;
    }

    spinUnlock(&pDelayedWork->lock, irqState);

    return retCode;
}

void delayedWorkTimeoutHandler(void *pArg)
{
    delayedWorkType *pDelayedWork = (delayedWorkType *)pArg;
    workQueueHandleType *pWorkQueue;
    int retCode;

    if (pDelayedWork == NULL)
    {
        return;
    }

    bool irqState = spinLock(&pDelayedWork->lock);

    if (!pDelayedWork->scheduled || (pDelayedWork->pWorkQueue == NULL))
    {
        spinUnlock(&pDelayedWork->lock, irqState);
        return;
    }

    pWorkQueue = pDelayedWork->pWorkQueue;
    retCode = workQueueSubmitInternal(pWorkQueue, &pDelayedWork->work);
    pDelayedWork->scheduled = false;
    spinUnlock(&pDelayedWork->lock, irqState);

    if (retCode == RET_SUCCESS)
    {
        (void)taskNotify(pWorkQueue->pWorkerTask, 0U, TASK_NOTIFY_INCREMENT);
    }
}

bool delayedWorkPending(delayedWorkType *pDelayedWork)
{
    if (pDelayedWork == NULL)
    {
        return false;
    }

    bool pending;
    bool irqState = spinLock(&pDelayedWork->lock);
    pending = pDelayedWork->scheduled || delayedWorkIsQueued(pDelayedWork);
    spinUnlock(&pDelayedWork->lock, irqState);

    return pending;
}

void workQueueTask(void *pArgs)
{
    workQueueHandleType *pWorkQueue = (workQueueHandleType *)pArgs;

    while (1)
    {
        workItemType *pWork = workQueuePop(pWorkQueue);
        if (pWork != NULL)
        {
            pWork->handler(pWork->pArg);
        }
        else
        {
            /* Block until new work is submitted to this queue. */
            (void)taskNotifyTake(true, NULL, TASK_FOREVER_WAIT);
        }
    }
}
