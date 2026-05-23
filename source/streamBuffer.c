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
#include "sanoRTOS/memHeap.h"
#include "sanoRTOS/streamBuffer.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"
#include "streamBufferInternal.h"
#include "taskInternal.h"
#include "objectHelpers.h"

MEM_SLAB_DEFINE(dynamicStreamBufferObjectSlab,
                sizeof(streamBufferHandleType),
                CONFIG_DYNAMIC_STREAM_BUFFER_SLAB_BLOCKS);

static int streamBufferObjectAlloc(streamBufferHandleType **ppStreamBuffer)
{
    return objectAllocFromSlab(&dynamicStreamBufferObjectSlab,
                               (void **)ppStreamBuffer,
                               sizeof(streamBufferHandleType));
}

static void streamBufferObjectFree(streamBufferHandleType *pStreamBuffer)
{
    (void)objectFreeToSlab(&dynamicStreamBufferObjectSlab, pStreamBuffer);
}

static int streamBufferSetup(streamBufferHandleType *pStreamBuffer,
                             uint8_t *pBuffer, uint32_t bufferSize, uint8_t flags)
{
    if ((pStreamBuffer == NULL) || (pBuffer == NULL) || (bufferSize == 0U))
    {
        return RET_INVAL;
    }

    memset(pStreamBuffer, 0, sizeof(streamBufferHandleType));
    pStreamBuffer->flags = flags;
    pStreamBuffer->producerWaitQueue.pLock = &pStreamBuffer->lock;
    pStreamBuffer->consumerWaitQueue.pLock = &pStreamBuffer->lock;
    pStreamBuffer->buffer = pBuffer;
    pStreamBuffer->bufferSize = bufferSize;

    return RET_SUCCESS;
}

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

static int streamBufferWakeWaitingTaskLocked(taskQueueType *pWaitQueue,
                                             wakeupReasonType wakeupReason,
                                             bool *pContextSwitchRequired,
                                             bool wakeAll)
{
    if ((pWaitQueue == NULL) || (pContextSwitchRequired == NULL))
    {
        return RET_INVAL;
    }

    taskHandleType *pTask;

    while ((pTask = waitQueuePop(pWaitQueue)) != NULL)
    {
        int retCode = taskSetReady(pTask, wakeupReason);
        if (retCode != RET_SUCCESS)
        {
            return retCode;
        }

        if (taskCanPreemptCurrentCore(pTask))
        {
            *pContextSwitchRequired = true;
        }

        if (!wakeAll)
        {
            break;
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

    return streamBufferWakeWaitingTaskLocked(&pStreamBuffer->consumerWaitQueue,
                                             STREAM_BUFFER_DATA_AVAILABLE,
                                             pContextSwitchRequired,
                                             false);
}

int streamBufferWakeSpaceAvailableLocked(streamBufferHandleType *pStreamBuffer,
                                         bool *pContextSwitchRequired)
{
    if ((pStreamBuffer == NULL) || (pContextSwitchRequired == NULL))
    {
        return RET_INVAL;
    }

    return streamBufferWakeWaitingTaskLocked(&pStreamBuffer->producerWaitQueue,
                                             STREAM_BUFFER_SPACE_AVAILABLE,
                                             pContextSwitchRequired,
                                             false);
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
            retCode = taskBlockOnWaitQueue(&pStreamBuffer->producerWaitQueue,
                                           WAIT_FOR_STREAM_BUFFER_SPACE,
                                           waitTicks,
                                           irqState);
            if (retCode != RET_SUCCESS)
            {
                return retCode;
            }

            if (currentTask->wakeupReason == STREAM_BUFFER_SPACE_AVAILABLE)
            {
                goto retry;
            }
            else if (currentTask->wakeupReason == WAIT_TIMEOUT)
            {
                retCode = RET_TIMEOUT;
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
            retCode = taskBlockOnWaitQueue(&pStreamBuffer->consumerWaitQueue,
                                           WAIT_FOR_STREAM_BUFFER_DATA,
                                           waitTicks,
                                           irqState);
            if (retCode != RET_SUCCESS)
            {
                return retCode;
            }

            if (currentTask->wakeupReason == STREAM_BUFFER_DATA_AVAILABLE)
            {
                goto retry;
            }
            else if (currentTask->wakeupReason == WAIT_TIMEOUT)
            {
                retCode = RET_TIMEOUT;
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

int streamBufferReset(streamBufferHandleType *pStreamBuffer)
{
    if (pStreamBuffer == NULL)
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&pStreamBuffer->lock);
    bool contextSwitchRequired = false;

    pStreamBuffer->usedBytes = 0U;
    pStreamBuffer->readIndex = 0U;
    pStreamBuffer->writeIndex = 0U;

    int retCode = streamBufferWakeWaitingTaskLocked(&pStreamBuffer->producerWaitQueue,
                                                    STREAM_BUFFER_SPACE_AVAILABLE,
                                                    &contextSwitchRequired,
                                                    true);

    spinUnlock(&pStreamBuffer->lock, irqState);

    if (contextSwitchRequired)
    {
        taskYield();
    }

    return retCode;
}

int streamBufferCreate(streamBufferHandleType **ppStreamBuffer, uint32_t bufferSize)
{
    streamBufferHandleType *pStreamBuffer = NULL;
    uint8_t *pBuffer = NULL;
    int retCode;

    if ((ppStreamBuffer == NULL) || (bufferSize == 0U))
    {
        return RET_INVAL;
    }

    retCode = streamBufferObjectAlloc(&pStreamBuffer);
    if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    pBuffer = (uint8_t *)memHeapAlloc(bufferSize);
    if (pBuffer == NULL)
    {
        streamBufferObjectFree(pStreamBuffer);
        return RET_NOMEM;
    }

    retCode = streamBufferSetup(pStreamBuffer, pBuffer, bufferSize,
                                OBJECT_FLAG_DYNAMIC | OBJECT_FLAG_OWN_BUFFER);
    if (retCode != RET_SUCCESS)
    {
        memHeapFree(pBuffer);
        streamBufferObjectFree(pStreamBuffer);
        return retCode;
    }

    *ppStreamBuffer = pStreamBuffer;

    return RET_SUCCESS;
}

int streamBufferDelete(streamBufferHandleType *pStreamBuffer)
{
    bool irqState;
    uint8_t flags;
    uint8_t *pBuffer;

    if ((pStreamBuffer == NULL) || !objectIsDynamic(pStreamBuffer->flags))
    {
        return RET_INVAL;
    }

    irqState = spinLock(&pStreamBuffer->lock);
    if (objectWaitQueueHasWaiters(&pStreamBuffer->producerWaitQueue) ||
        objectWaitQueueHasWaiters(&pStreamBuffer->consumerWaitQueue))
    {
        spinUnlock(&pStreamBuffer->lock, irqState);
        return RET_BUSY;
    }

    flags = pStreamBuffer->flags;
    pBuffer = pStreamBuffer->buffer;
    spinUnlock(&pStreamBuffer->lock, irqState);
    if ((flags & OBJECT_FLAG_OWN_BUFFER) != 0U)
    {
        memHeapFree(pBuffer);
    }
    streamBufferObjectFree(pStreamBuffer);

    return RET_SUCCESS;
}
