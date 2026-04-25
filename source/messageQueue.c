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

#include <string.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/memHeap.h"
#include "sanoRTOS/messageQueue.h"
#include "sanoRTOS/mutex.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"
#include "objectHelpers.h"

MEM_SLAB_DEFINE(dynamicMsgQueueObjectSlab,
                sizeof(msgQueueHandleType),
                CONFIG_DYNAMIC_MSG_QUEUE_SLAB_BLOCKS);

static int msgQueueObjectAlloc(msgQueueHandleType **ppQueueHandle)
{
    return objectAllocFromSlab(&dynamicMsgQueueObjectSlab,
                               (void **)ppQueueHandle,
                               sizeof(msgQueueHandleType));
}

static void msgQueueObjectFree(msgQueueHandleType *pQueueHandle)
{
    (void)objectFreeToSlab(&dynamicMsgQueueObjectSlab, pQueueHandle);
}

static int msgQueueSetup(msgQueueHandleType *pQueueHandle,
                         uint8_t *pBuffer, uint32_t length, uint32_t itemSize, uint8_t flags)
{
    if ((pQueueHandle == NULL) || (pBuffer == NULL) || (length == 0U) || (itemSize == 0U) ||
        (length > (UINT32_MAX / itemSize)))
    {
        return RET_INVAL;
    }

    memset(pQueueHandle, 0, sizeof(msgQueueHandleType));
    pQueueHandle->flags = flags;
    pQueueHandle->buffer = pBuffer;
    pQueueHandle->queueLength = length;
    pQueueHandle->itemSize = itemSize;

    return RET_SUCCESS;
}

static int msgQueueWakeWaitingTasks(taskQueueType *pWaitQueue,
                                    blockedReasonType blockedReason,
                                    wakeupReasonType wakeupReason,
                                    bool *pContextSwitchRequired,
                                    bool wakeAll)
{
    if ((pWaitQueue == NULL) || (pContextSwitchRequired == NULL))
    {
        return RET_INVAL;
    }

    taskHandleType *pTask = NULL;

getNextWaitingTask:
    pTask = waitQueuePop(pWaitQueue);
    if (pTask != NULL)
    {
        if ((pTask->state != TASK_STATE_BLOCKED) ||
            (pTask->blockedReason != blockedReason))
        {
            goto getNextWaitingTask;
        }

        int retCode = taskSetReady(pTask, wakeupReason);
        if (retCode != RET_SUCCESS)
        {
            return retCode;
        }

        if (taskCanPreemptCurrentCore(pTask))
        {
            *pContextSwitchRequired = true;
        }

        if (wakeAll)
        {
            goto getNextWaitingTask;
        }
    }

    return RET_SUCCESS;
}

static int msgQueueBufferWrite(msgQueueHandleType *pQueueHandle, void *pItem)
{
    if ((pQueueHandle == NULL) || (pItem == NULL))
    {
        return RET_INVAL;
    }

    int retCode = RET_SUCCESS;
    bool irqState = spinLock(&pQueueHandle->lock);
    bool contextSwitchRequired = false;

    if (!msgQueueFull(pQueueHandle))
    {
        memcpy(&pQueueHandle->buffer[pQueueHandle->writeIndex], pItem, pQueueHandle->itemSize);
        pQueueHandle->writeIndex = (pQueueHandle->writeIndex + pQueueHandle->itemSize) % (pQueueHandle->queueLength * pQueueHandle->itemSize);
        pQueueHandle->itemCount++;

        retCode = msgQueueWakeWaitingTasks(&pQueueHandle->consumerWaitQueue,
                                           WAIT_FOR_MSG_QUEUE_DATA,
                                           MSG_QUEUE_DATA_AVAILABLE,
                                           &contextSwitchRequired,
                                           false);
    }
    else
    {
        retCode = RET_FULL;
    }

    spinUnlock(&pQueueHandle->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

static int msgQueueBufferRead(msgQueueHandleType *pQueueHandle, void *pItem)
{
    if ((pQueueHandle == NULL) || (pItem == NULL))
    {
        return RET_INVAL;
    }

    int retCode = RET_SUCCESS;
    bool irqState = spinLock(&pQueueHandle->lock);
    bool contextSwitchRequired = false;

    if (!msgQueueEmpty(pQueueHandle))
    {
        memcpy(pItem, &pQueueHandle->buffer[pQueueHandle->readIndex], pQueueHandle->itemSize);
        pQueueHandle->readIndex = (pQueueHandle->readIndex + pQueueHandle->itemSize) % (pQueueHandle->queueLength * pQueueHandle->itemSize);
        pQueueHandle->itemCount--;

        retCode = msgQueueWakeWaitingTasks(&pQueueHandle->producerWaitQueue,
                                           WAIT_FOR_MSG_QUEUE_SPACE,
                                           MSG_QUEUE_SPACE_AVAILABE,
                                           &contextSwitchRequired,
                                           false);
    }
    else
    {
        retCode = RET_EMPTY;
    }

    spinUnlock(&pQueueHandle->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

int msgQueueReset(msgQueueHandleType *pQueueHandle)
{
    if (pQueueHandle == NULL)
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&pQueueHandle->lock);
    bool contextSwitchRequired = false;

    pQueueHandle->itemCount = 0U;
    pQueueHandle->readIndex = 0U;
    pQueueHandle->writeIndex = 0U;

    int retCode = msgQueueWakeWaitingTasks(&pQueueHandle->producerWaitQueue,
                                           WAIT_FOR_MSG_QUEUE_SPACE,
                                           MSG_QUEUE_SPACE_AVAILABE,
                                           &contextSwitchRequired,
                                           true);

    spinUnlock(&pQueueHandle->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

int msgQueueSend(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks)
{
    if ((pQueueHandle == NULL) || (pItem == NULL))
    {
        return RET_INVAL;
    }
    if (portIsInISRContext() && (waitTicks != TASK_NO_WAIT))
    {
        return RET_INVAL;
    }

    int retCode;

    /*Write to msgQueue buffer if messageQueue is not full*/
retry:
    retCode = msgQueueBufferWrite(pQueueHandle, pItem);

    if (retCode == RET_FULL)
    {
        if (waitTicks == TASK_NO_WAIT)
        {
            retCode = RET_FULL;
        }
        else
        {
            taskHandleType *currentTask = taskGetCurrent();

            bool irqState = spinLock(&pQueueHandle->lock);

            retCode = waitQueueAdd(&pQueueHandle->producerWaitQueue, currentTask);
            if (retCode != RET_SUCCESS)
            {
                spinUnlock(&pQueueHandle->lock, irqState);
                return retCode;
            }

            spinUnlock(&pQueueHandle->lock, irqState);

            /* Block current task and  give CPU to other tasks while waiting for space to be available */
            retCode = taskBlock(WAIT_FOR_MSG_QUEUE_SPACE, waitTicks);
            if (retCode != RET_SUCCESS)
            {
                irqState = spinLock(&pQueueHandle->lock);
                (void)waitQueueRemove(currentTask);
                spinUnlock(&pQueueHandle->lock, irqState);
                return retCode;
            }

            if (currentTask->wakeupReason == MSG_QUEUE_SPACE_AVAILABE)
            {
                retCode = msgQueueBufferWrite(pQueueHandle, pItem);
            }
            else if (currentTask->wakeupReason == WAIT_TIMEOUT)
            {
                retCode = RET_TIMEOUT;
            }
            /*Task might have been suspended while waiting for space to be available and later resumed.
              In this case, retry sending to the msgQueue again */
            else
            {
                goto retry;
            }
        }
    }
    else if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    return retCode;
}

int msgQueueReceive(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks)
{
    if ((pQueueHandle == NULL) || (pItem == NULL))
    {
        return RET_INVAL;
    }
    if (portIsInISRContext() && (waitTicks != TASK_NO_WAIT))
    {
        return RET_INVAL;
    }

    int retCode;

retry:
    retCode = msgQueueBufferRead(pQueueHandle, pItem);

    if (retCode == RET_EMPTY)
    {
        if (waitTicks == TASK_NO_WAIT)
        {
            retCode = RET_EMPTY;
        }
        else
        {
            taskHandleType *currentTask = taskGetCurrent();

            bool irqState = spinLock(&pQueueHandle->lock);

            retCode = waitQueueAdd(&pQueueHandle->consumerWaitQueue, currentTask);
            if (retCode != RET_SUCCESS)
            {
                spinUnlock(&pQueueHandle->lock, irqState);
                return retCode;
            }

            spinUnlock(&pQueueHandle->lock, irqState);

            /* Block current task and give CPU to other tasks while waiting for data to be available */
            retCode = taskBlock(WAIT_FOR_MSG_QUEUE_DATA, waitTicks);
            if (retCode != RET_SUCCESS)
            {
                irqState = spinLock(&pQueueHandle->lock);
                (void)waitQueueRemove(currentTask);
                spinUnlock(&pQueueHandle->lock, irqState);
                return retCode;
            }

            if (currentTask->wakeupReason == MSG_QUEUE_DATA_AVAILABLE)
            {
                retCode = msgQueueBufferRead(pQueueHandle, pItem);
            }
            else if (currentTask->wakeupReason == WAIT_TIMEOUT)
            {
                retCode = RET_TIMEOUT;
            }
            /*Task might have been suspended while waiting for data to be available and later resumed.
            In this case, retry receiving from the msgQueue again */
            else
            {
                goto retry;
            }
        }
    }
    else if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    return retCode;
}

int msgQueueCreate(msgQueueHandleType **ppQueueHandle, uint32_t length, uint32_t itemSize)
{
    msgQueueHandleType *pQueueHandle = NULL;
    uint8_t *pBuffer = NULL;
    int retCode;

    if ((ppQueueHandle == NULL) || (length == 0U) || (itemSize == 0U) ||
        (length > (UINT32_MAX / itemSize)))
    {
        return RET_INVAL;
    }

    retCode = msgQueueObjectAlloc(&pQueueHandle);
    if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    pBuffer = (uint8_t *)memHeapAlloc(length * itemSize);
    if (pBuffer == NULL)
    {
        msgQueueObjectFree(pQueueHandle);
        return RET_NOMEM;
    }

    retCode = msgQueueSetup(pQueueHandle, pBuffer, length, itemSize,
                            OBJECT_FLAG_DYNAMIC | OBJECT_FLAG_OWN_BUFFER);
    if (retCode != RET_SUCCESS)
    {
        memHeapFree(pBuffer);
        msgQueueObjectFree(pQueueHandle);
        return retCode;
    }

    *ppQueueHandle = pQueueHandle;

    return RET_SUCCESS;
}

int msgQueueDelete(msgQueueHandleType *pQueueHandle)
{
    bool irqState;
    uint8_t flags;
    uint8_t *pBuffer;

    if ((pQueueHandle == NULL) || !objectIsDynamic(pQueueHandle->flags))
    {
        return RET_INVAL;
    }

    irqState = spinLock(&pQueueHandle->lock);
    if (objectWaitQueueHasWaiters(&pQueueHandle->producerWaitQueue) ||
        objectWaitQueueHasWaiters(&pQueueHandle->consumerWaitQueue))
    {
        spinUnlock(&pQueueHandle->lock, irqState);
        return RET_BUSY;
    }

    flags = pQueueHandle->flags;
    pBuffer = pQueueHandle->buffer;
    spinUnlock(&pQueueHandle->lock, irqState);
    if ((flags & OBJECT_FLAG_OWN_BUFFER) != 0U)
    {
        memHeapFree(pBuffer);
    }
    msgQueueObjectFree(pQueueHandle);

    return RET_SUCCESS;
}
