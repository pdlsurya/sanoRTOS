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
#include "sanoRTOS/messageBuffer.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"

static inline uint32_t msgBufferRequiredBytes(uint32_t length)
{
    return (uint32_t)sizeof(uint32_t) + length;
}

static void msgBufferRingWrite(msgBufferHandleType *pMsgBuffer, uint32_t index, const void *pData, uint32_t length)
{
    if (length == 0U)
    {
        return;
    }

    const uint8_t *pBytes = (const uint8_t *)pData;
    uint32_t firstChunk = pMsgBuffer->bufferSize - index;
    if (firstChunk > length)
    {
        firstChunk = length;
    }

    memcpy(&pMsgBuffer->buffer[index], pBytes, firstChunk);

    if (length > firstChunk)
    {
        memcpy(pMsgBuffer->buffer, &pBytes[firstChunk], length - firstChunk);
    }
}

static void msgBufferRingRead(msgBufferHandleType *pMsgBuffer, uint32_t index, void *pData, uint32_t length)
{
    if (length == 0U)
    {
        return;
    }

    uint8_t *pBytes = (uint8_t *)pData;
    uint32_t firstChunk = pMsgBuffer->bufferSize - index;
    if (firstChunk > length)
    {
        firstChunk = length;
    }

    memcpy(pBytes, &pMsgBuffer->buffer[index], firstChunk);

    if (length > firstChunk)
    {
        memcpy(&pBytes[firstChunk], pMsgBuffer->buffer, length - firstChunk);
    }
}

static int msgBufferPeekLength(msgBufferHandleType *pMsgBuffer, uint32_t *pLength)
{
    if ((pMsgBuffer == NULL) || (pLength == NULL))
    {
        return RET_INVAL;
    }

    if (msgBufferEmpty(pMsgBuffer))
    {
        return RET_EMPTY;
    }

    msgBufferRingRead(pMsgBuffer, pMsgBuffer->readIndex, pLength, sizeof(uint32_t));
    return RET_SUCCESS;
}

static int msgBufferWakeWaitingTask(taskQueueType *pWaitQueue,
                                    blockedReasonType blockedReason,
                                    wakeupReasonType wakeupReason,
                                    bool *pContextSwitchRequired)
{
    if ((pWaitQueue == NULL) || (pContextSwitchRequired == NULL))
    {
        return RET_INVAL;
    }

    taskHandleType *pTask = NULL;

getNextWaitingTask:
    pTask = TASK_GET_FROM_WAIT_QUEUE(pWaitQueue);
    if (pTask != NULL)
    {
        /* Skip stale tasks that are no longer blocked on this message buffer wait reason. */
        if ((pTask->status != TASK_STATUS_BLOCKED) ||
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
    }

    return RET_SUCCESS;
}

static int msgBufferWrite(msgBufferHandleType *pMsgBuffer, const void *pData, uint32_t length)
{
    if ((pMsgBuffer == NULL) || ((length > 0U) && (pData == NULL)))
    {
        return RET_INVAL;
    }

    uint32_t requiredBytes = msgBufferRequiredBytes(length);
    if ((pMsgBuffer->buffer == NULL) || (pMsgBuffer->bufferSize < sizeof(uint32_t)) || (requiredBytes > pMsgBuffer->bufferSize))
    {
        return RET_INVAL;
    }

    int retCode = RET_SUCCESS;
    bool irqState = spinLock(&pMsgBuffer->lock);
    bool contextSwitchRequired = false;

    if (msgBufferBytesFree(pMsgBuffer) >= requiredBytes)
    {
        msgBufferRingWrite(pMsgBuffer, pMsgBuffer->writeIndex, &length, sizeof(uint32_t));
        msgBufferRingWrite(pMsgBuffer,
                           (pMsgBuffer->writeIndex + sizeof(uint32_t)) % pMsgBuffer->bufferSize,
                           pData,
                           length);

        pMsgBuffer->writeIndex = (pMsgBuffer->writeIndex + requiredBytes) % pMsgBuffer->bufferSize;
        pMsgBuffer->usedBytes += requiredBytes;

        retCode = msgBufferWakeWaitingTask(&pMsgBuffer->consumerWaitQueue,
                                           WAIT_FOR_MSG_BUFFER_DATA,
                                           MSG_BUFFER_DATA_AVAILABLE,
                                           &contextSwitchRequired);
    }
    else
    {
        retCode = RET_FULL;
    }

    spinUnlock(&pMsgBuffer->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

static int msgBufferRead(msgBufferHandleType *pMsgBuffer, void *pData, uint32_t *pLength)
{
    if ((pMsgBuffer == NULL) || (pLength == NULL))
    {
        return RET_INVAL;
    }

    uint32_t bufferCapacity = *pLength;
    if ((bufferCapacity > 0U) && (pData == NULL))
    {
        return RET_INVAL;
    }

    int retCode = RET_SUCCESS;
    bool irqState = spinLock(&pMsgBuffer->lock);
    bool contextSwitchRequired = false;
    uint32_t messageLength = 0U;

    retCode = msgBufferPeekLength(pMsgBuffer, &messageLength);
    if (retCode == RET_SUCCESS)
    {
        if (messageLength > bufferCapacity)
        {
            *pLength = messageLength;
            retCode = RET_INVAL;
        }
        else
        {
            msgBufferRingRead(pMsgBuffer,
                              (pMsgBuffer->readIndex + sizeof(uint32_t)) % pMsgBuffer->bufferSize,
                              pData,
                              messageLength);

            pMsgBuffer->readIndex = (pMsgBuffer->readIndex + msgBufferRequiredBytes(messageLength)) % pMsgBuffer->bufferSize;
            pMsgBuffer->usedBytes -= msgBufferRequiredBytes(messageLength);
            *pLength = messageLength;

            retCode = msgBufferWakeWaitingTask(&pMsgBuffer->producerWaitQueue,
                                               WAIT_FOR_MSG_BUFFER_SPACE,
                                               MSG_BUFFER_SPACE_AVAILABLE,
                                               &contextSwitchRequired);
        }
    }

    spinUnlock(&pMsgBuffer->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

int msgBufferSend(msgBufferHandleType *pMsgBuffer, const void *pData, uint32_t length, uint32_t waitTicks)
{
    if (pMsgBuffer == NULL)
    {
        return RET_INVAL;
    }
    if (portIsInISRContext() && (waitTicks != TASK_NO_WAIT))
    {
        return RET_INVAL;
    }
    if ((length > 0U) && (pData == NULL))
    {
        return RET_INVAL;
    }

    int retCode;

retry:
    retCode = msgBufferWrite(pMsgBuffer, pData, length);

    if (retCode == RET_FULL)
    {
        if (waitTicks == TASK_NO_WAIT)
        {
            retCode = RET_FULL;
        }
        else
        {
            taskHandleType *currentTask = taskGetCurrent();
            bool irqState = spinLock(&pMsgBuffer->lock);

            retCode = taskQueueAdd(&pMsgBuffer->producerWaitQueue, currentTask);
            if (retCode != RET_SUCCESS)
            {
                spinUnlock(&pMsgBuffer->lock, irqState);
                return retCode;
            }

            spinUnlock(&pMsgBuffer->lock, irqState);

            retCode = taskBlock(currentTask, WAIT_FOR_MSG_BUFFER_SPACE, waitTicks);
            if (retCode != RET_SUCCESS)
            {
                irqState = spinLock(&pMsgBuffer->lock);
                (void)taskQueueRemove(&pMsgBuffer->producerWaitQueue, currentTask);
                spinUnlock(&pMsgBuffer->lock, irqState);
                return retCode;
            }

            if (currentTask->wakeupReason == MSG_BUFFER_SPACE_AVAILABLE)
            {
                retCode = msgBufferWrite(pMsgBuffer, pData, length);
            }
            else if (currentTask->wakeupReason == WAIT_TIMEOUT)
            {
                irqState = spinLock(&pMsgBuffer->lock);
                retCode = taskQueueRemove(&pMsgBuffer->producerWaitQueue, currentTask);
                spinUnlock(&pMsgBuffer->lock, irqState);

                if ((retCode == RET_SUCCESS) || (retCode == RET_NOTASK))
                {
                    retCode = RET_TIMEOUT;
                }
            }
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

int msgBufferReceive(msgBufferHandleType *pMsgBuffer, void *pData, uint32_t *pLength, uint32_t waitTicks)
{
    if ((pMsgBuffer == NULL) || (pLength == NULL))
    {
        return RET_INVAL;
    }
    if (portIsInISRContext() && (waitTicks != TASK_NO_WAIT))
    {
        return RET_INVAL;
    }
    if ((*pLength > 0U) && (pData == NULL))
    {
        return RET_INVAL;
    }

    int retCode;

retry:
    retCode = msgBufferRead(pMsgBuffer, pData, pLength);

    if (retCode == RET_EMPTY)
    {
        if (waitTicks == TASK_NO_WAIT)
        {
            retCode = RET_EMPTY;
        }
        else
        {
            taskHandleType *currentTask = taskGetCurrent();
            bool irqState = spinLock(&pMsgBuffer->lock);

            retCode = taskQueueAdd(&pMsgBuffer->consumerWaitQueue, currentTask);
            if (retCode != RET_SUCCESS)
            {
                spinUnlock(&pMsgBuffer->lock, irqState);
                return retCode;
            }

            spinUnlock(&pMsgBuffer->lock, irqState);

            retCode = taskBlock(currentTask, WAIT_FOR_MSG_BUFFER_DATA, waitTicks);
            if (retCode != RET_SUCCESS)
            {
                irqState = spinLock(&pMsgBuffer->lock);
                (void)taskQueueRemove(&pMsgBuffer->consumerWaitQueue, currentTask);
                spinUnlock(&pMsgBuffer->lock, irqState);
                return retCode;
            }

            if (currentTask->wakeupReason == MSG_BUFFER_DATA_AVAILABLE)
            {
                retCode = msgBufferRead(pMsgBuffer, pData, pLength);
            }
            else if (currentTask->wakeupReason == WAIT_TIMEOUT)
            {
                irqState = spinLock(&pMsgBuffer->lock);
                retCode = taskQueueRemove(&pMsgBuffer->consumerWaitQueue, currentTask);
                spinUnlock(&pMsgBuffer->lock, irqState);

                if ((retCode == RET_SUCCESS) || (retCode == RET_NOTASK))
                {
                    retCode = RET_TIMEOUT;
                }
            }
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

int msgBufferNextLength(msgBufferHandleType *pMsgBuffer, uint32_t *pLength)
{
    if ((pMsgBuffer == NULL) || (pLength == NULL))
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&pMsgBuffer->lock);
    int retCode = msgBufferPeekLength(pMsgBuffer, pLength);
    spinUnlock(&pMsgBuffer->lock, irqState);

    return retCode;
}
