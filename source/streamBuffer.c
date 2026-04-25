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
#include "sanoRTOS/streamBuffer.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"
#include "streamBufferInternal.h"

static void streamBufferRingWrite(streamBufferHandleType *pStreamBuffer, uint32_t index, const void *pData, uint32_t length)
{
    if (length == 0U)
    {
        return;
    }

    const uint8_t *pBytes = (const uint8_t *)pData;
    uint32_t firstChunk = pStreamBuffer->bufferSize - index;
    if (firstChunk > length)
    {
        firstChunk = length;
    }

    memcpy(&pStreamBuffer->buffer[index], pBytes, firstChunk);

    if (length > firstChunk)
    {
        memcpy(pStreamBuffer->buffer, &pBytes[firstChunk], length - firstChunk);
    }
}

static void streamBufferRingRead(streamBufferHandleType *pStreamBuffer, uint32_t index, void *pData, uint32_t length)
{
    if (length == 0U)
    {
        return;
    }

    uint8_t *pBytes = (uint8_t *)pData;
    uint32_t firstChunk = pStreamBuffer->bufferSize - index;
    if (firstChunk > length)
    {
        firstChunk = length;
    }

    memcpy(pBytes, &pStreamBuffer->buffer[index], firstChunk);

    if (length > firstChunk)
    {
        memcpy(&pBytes[firstChunk], pStreamBuffer->buffer, length - firstChunk);
    }
}

static int streamBufferWakeWaitingTask(taskQueueType *pWaitQueue,
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
        /* Skip stale tasks that are no longer blocked on this stream buffer wait reason. */
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

int streamBufferWriteLocked(streamBufferHandleType *pStreamBuffer,
                            const void *pData,
                            uint32_t length)
{
    if (pStreamBuffer == NULL)
    {
        return RET_INVAL;
    }
    if ((length > 0U) && (pData == NULL))
    {
        return RET_INVAL;
    }
    if ((pStreamBuffer->buffer == NULL) || (length > pStreamBuffer->bufferSize))
    {
        return RET_INVAL;
    }

    if (streamBufferBytesFree(pStreamBuffer) < length)
    {
        return RET_FULL;
    }

    streamBufferRingWrite(pStreamBuffer, pStreamBuffer->writeIndex, pData, length);
    pStreamBuffer->writeIndex = (pStreamBuffer->writeIndex + length) % pStreamBuffer->bufferSize;
    pStreamBuffer->usedBytes += length;

    return RET_SUCCESS;
}

int streamBufferReadLocked(streamBufferHandleType *pStreamBuffer,
                           void *pData,
                           uint32_t length,
                           uint32_t *pBytesRead)
{
    if ((pStreamBuffer == NULL) || (pBytesRead == NULL))
    {
        return RET_INVAL;
    }
    if ((length > 0U) && (pData == NULL))
    {
        return RET_INVAL;
    }

    if (streamBufferEmpty(pStreamBuffer))
    {
        return RET_EMPTY;
    }

    *pBytesRead = streamBufferBytesUsed(pStreamBuffer);
    if (*pBytesRead > length)
    {
        *pBytesRead = length;
    }

    streamBufferRingRead(pStreamBuffer, pStreamBuffer->readIndex, pData, *pBytesRead);
    pStreamBuffer->readIndex = (pStreamBuffer->readIndex + *pBytesRead) % pStreamBuffer->bufferSize;
    pStreamBuffer->usedBytes -= *pBytesRead;

    return RET_SUCCESS;
}

int streamBufferWakeDataAvailableLocked(streamBufferHandleType *pStreamBuffer,
                                        bool *pContextSwitchRequired)
{
    if ((pStreamBuffer == NULL) || (pContextSwitchRequired == NULL))
    {
        return RET_INVAL;
    }

    return streamBufferWakeWaitingTask(&pStreamBuffer->consumerWaitQueue,
                                       WAIT_FOR_STREAM_BUFFER_DATA,
                                       STREAM_BUFFER_DATA_AVAILABLE,
                                       pContextSwitchRequired);
}

int streamBufferWakeSpaceAvailableLocked(streamBufferHandleType *pStreamBuffer,
                                         bool *pContextSwitchRequired)
{
    if ((pStreamBuffer == NULL) || (pContextSwitchRequired == NULL))
    {
        return RET_INVAL;
    }

    return streamBufferWakeWaitingTask(&pStreamBuffer->producerWaitQueue,
                                       WAIT_FOR_STREAM_BUFFER_SPACE,
                                       STREAM_BUFFER_SPACE_AVAILABLE,
                                       pContextSwitchRequired);
}

int streamBufferPeekLocked(streamBufferHandleType *pStreamBuffer,
                           void *pData,
                           uint32_t length,
                           uint32_t *pBytesRead)
{
    if ((pStreamBuffer == NULL) || (pBytesRead == NULL))
    {
        return RET_INVAL;
    }
    if ((length > 0U) && (pData == NULL))
    {
        return RET_INVAL;
    }

    if (streamBufferEmpty(pStreamBuffer))
    {
        return RET_EMPTY;
    }

    *pBytesRead = streamBufferBytesUsed(pStreamBuffer);
    if (*pBytesRead > length)
    {
        *pBytesRead = length;
    }

    streamBufferRingRead(pStreamBuffer, pStreamBuffer->readIndex, pData, *pBytesRead);

    return RET_SUCCESS;
}

int streamBufferSend(streamBufferHandleType *pStreamBuffer, const void *pData, uint32_t length, uint32_t waitTicks)
{
    if (pStreamBuffer == NULL)
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

    if (length == 0U)
    {
        return RET_SUCCESS;
    }

    int retCode;
    bool irqState;
    bool contextSwitchRequired;

retry:
    irqState = spinLock(&pStreamBuffer->lock);
    contextSwitchRequired = false;
    retCode = streamBufferWriteLocked(pStreamBuffer, pData, length);
    if (retCode == RET_SUCCESS)
    {
        retCode = streamBufferWakeDataAvailableLocked(pStreamBuffer, &contextSwitchRequired);
    }
    spinUnlock(&pStreamBuffer->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    if (retCode == RET_FULL)
    {
        if (waitTicks == TASK_NO_WAIT)
        {
            retCode = RET_FULL;
        }
        else
        {
            taskHandleType *currentTask = taskGetCurrent();

            irqState = spinLock(&pStreamBuffer->lock);
            retCode = taskQueueAdd(&pStreamBuffer->producerWaitQueue, currentTask);
            if (retCode != RET_SUCCESS)
            {
                spinUnlock(&pStreamBuffer->lock, irqState);
                return retCode;
            }

            spinUnlock(&pStreamBuffer->lock, irqState);

            retCode = taskBlock(currentTask, WAIT_FOR_STREAM_BUFFER_SPACE, waitTicks);
            if (retCode != RET_SUCCESS)
            {
                irqState = spinLock(&pStreamBuffer->lock);
                (void)taskQueueRemove(&pStreamBuffer->producerWaitQueue, currentTask);
                spinUnlock(&pStreamBuffer->lock, irqState);
                return retCode;
            }

            if (currentTask->wakeupReason == STREAM_BUFFER_SPACE_AVAILABLE)
            {
                goto retry;
            }
            else if (currentTask->wakeupReason == WAIT_TIMEOUT)
            {
                irqState = spinLock(&pStreamBuffer->lock);
                retCode = taskQueueRemove(&pStreamBuffer->producerWaitQueue, currentTask);
                spinUnlock(&pStreamBuffer->lock, irqState);

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

    return retCode;
}

int streamBufferReceive(streamBufferHandleType *pStreamBuffer, void *pData, uint32_t *pLength, uint32_t waitTicks)
{
    if ((pStreamBuffer == NULL) || (pLength == NULL))
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

    if (*pLength == 0U)
    {
        return RET_SUCCESS;
    }

    int retCode;
    bool irqState;
    bool contextSwitchRequired;
    uint32_t bytesRead = 0U;

retry:
    irqState = spinLock(&pStreamBuffer->lock);
    contextSwitchRequired = false;
    retCode = streamBufferReadLocked(pStreamBuffer, pData, *pLength, &bytesRead);
    if (retCode == RET_SUCCESS)
    {
        retCode = streamBufferWakeSpaceAvailableLocked(pStreamBuffer, &contextSwitchRequired);
    }
    spinUnlock(&pStreamBuffer->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    if (retCode == RET_SUCCESS)
    {
        *pLength = bytesRead;
    }
    else if (retCode == RET_EMPTY)
    {
        if (waitTicks == TASK_NO_WAIT)
        {
            retCode = RET_EMPTY;
        }
        else
        {
            taskHandleType *currentTask = taskGetCurrent();

            irqState = spinLock(&pStreamBuffer->lock);
            retCode = taskQueueAdd(&pStreamBuffer->consumerWaitQueue, currentTask);
            if (retCode != RET_SUCCESS)
            {
                spinUnlock(&pStreamBuffer->lock, irqState);
                return retCode;
            }

            spinUnlock(&pStreamBuffer->lock, irqState);

            retCode = taskBlock(currentTask, WAIT_FOR_STREAM_BUFFER_DATA, waitTicks);
            if (retCode != RET_SUCCESS)
            {
                irqState = spinLock(&pStreamBuffer->lock);
                (void)taskQueueRemove(&pStreamBuffer->consumerWaitQueue, currentTask);
                spinUnlock(&pStreamBuffer->lock, irqState);
                return retCode;
            }

            if (currentTask->wakeupReason == STREAM_BUFFER_DATA_AVAILABLE)
            {
                goto retry;
            }
            else if (currentTask->wakeupReason == WAIT_TIMEOUT)
            {
                irqState = spinLock(&pStreamBuffer->lock);
                retCode = taskQueueRemove(&pStreamBuffer->consumerWaitQueue, currentTask);
                spinUnlock(&pStreamBuffer->lock, irqState);

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

    return retCode;
}

int streamBufferPeek(streamBufferHandleType *pStreamBuffer, void *pData, uint32_t *pLength)
{
    if ((pStreamBuffer == NULL) || (pLength == NULL))
    {
        return RET_INVAL;
    }
    if ((*pLength > 0U) && (pData == NULL))
    {
        return RET_INVAL;
    }

    if (*pLength == 0U)
    {
        return RET_SUCCESS;
    }

    bool irqState = spinLock(&pStreamBuffer->lock);
    uint32_t bytesRead = 0U;
    int retCode = streamBufferPeekLocked(pStreamBuffer, pData, *pLength, &bytesRead);
    spinUnlock(&pStreamBuffer->lock, irqState);

    if (retCode == RET_SUCCESS)
    {
        *pLength = bytesRead;
    }

    return retCode;
}
