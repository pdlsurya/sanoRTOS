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
#include "streamBufferInternal.h"

static inline uint32_t msgBufferRequiredBytes(uint32_t length)
{
    return (uint32_t)sizeof(uint32_t) + length;
}

static int msgBufferPeekLengthLocked(msgBufferHandleType *pMsgBuffer, uint32_t *pLength)
{
    if ((pMsgBuffer == NULL) || (pLength == NULL))
    {
        return RET_INVAL;
    }

    uint32_t bytesPeeked = 0U;
    int retCode = streamBufferPeekLocked(&pMsgBuffer->streamBuffer, pLength, sizeof(uint32_t), &bytesPeeked);
    if (retCode != RET_SUCCESS)
    {
        return retCode;
    }

    return (bytesPeeked == sizeof(uint32_t)) ? RET_SUCCESS : RET_INVAL;
}

static int msgBufferWrite(msgBufferHandleType *pMsgBuffer, const void *pData, uint32_t length)
{
    if (pMsgBuffer == NULL)
    {
        return RET_INVAL;
    }
    if ((length > 0U) && (pData == NULL))
    {
        return RET_INVAL;
    }

    streamBufferHandleType *pStreamBuffer = &pMsgBuffer->streamBuffer;
    uint32_t requiredBytes = msgBufferRequiredBytes(length);
    if (requiredBytes > pStreamBuffer->bufferSize)
    {
        return RET_INVAL;
    }

    int retCode;
    bool irqState = spinLock(&pStreamBuffer->lock);
    bool contextSwitchRequired = false;

    if (streamBufferBytesFree(pStreamBuffer) >= requiredBytes)
    {
        retCode = streamBufferWriteLocked(pStreamBuffer, &length, sizeof(uint32_t));
        if (retCode == RET_SUCCESS)
        {
            retCode = streamBufferWriteLocked(pStreamBuffer, pData, length);
        }
        if (retCode == RET_SUCCESS)
        {
            retCode = streamBufferWakeDataAvailableLocked(pStreamBuffer, &contextSwitchRequired);
        }
    }
    else
    {
        retCode = RET_FULL;
    }

    spinUnlock(&pStreamBuffer->lock, irqState);

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

    streamBufferHandleType *pStreamBuffer = &pMsgBuffer->streamBuffer;
    int retCode;
    bool irqState = spinLock(&pStreamBuffer->lock);
    bool contextSwitchRequired = false;
    uint32_t messageLength = 0U;

    retCode = msgBufferPeekLengthLocked(pMsgBuffer, &messageLength);
    if (retCode == RET_SUCCESS)
    {
        if (messageLength > bufferCapacity)
        {
            *pLength = messageLength;
            retCode = RET_INVAL;
        }
        else
        {
            uint32_t bytesRead = 0U;

            retCode = streamBufferReadLocked(pStreamBuffer,
                                             &messageLength,
                                             sizeof(uint32_t),
                                             &bytesRead);
            if ((retCode == RET_SUCCESS) && (bytesRead == sizeof(uint32_t)))
            {
                retCode = streamBufferReadLocked(pStreamBuffer,
                                                 pData,
                                                 messageLength,
                                                 &bytesRead);
                if ((retCode == RET_SUCCESS) && (bytesRead == messageLength))
                {
                    *pLength = messageLength;
                    retCode = streamBufferWakeSpaceAvailableLocked(pStreamBuffer, &contextSwitchRequired);
                }
                else if (retCode == RET_SUCCESS)
                {
                    retCode = RET_INVAL;
                }
            }
            else if (retCode == RET_SUCCESS)
            {
                retCode = RET_INVAL;
            }
        }
    }

    spinUnlock(&pStreamBuffer->lock, irqState);

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
    streamBufferHandleType *pStreamBuffer = &pMsgBuffer->streamBuffer;

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
            bool irqState = spinLock(&pStreamBuffer->lock);

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
    streamBufferHandleType *pStreamBuffer = &pMsgBuffer->streamBuffer;

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
            bool irqState = spinLock(&pStreamBuffer->lock);

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

int msgBufferNextLength(msgBufferHandleType *pMsgBuffer, uint32_t *pLength)
{
    if ((pMsgBuffer == NULL) || (pLength == NULL))
    {
        return RET_INVAL;
    }

    bool irqState = spinLock(&pMsgBuffer->streamBuffer.lock);
    int retCode = msgBufferPeekLengthLocked(pMsgBuffer, pLength);
    spinUnlock(&pMsgBuffer->streamBuffer.lock, irqState);

    return retCode;
}
