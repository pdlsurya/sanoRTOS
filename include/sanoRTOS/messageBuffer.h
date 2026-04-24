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

#ifndef __SANO_RTOS_MSG_BUFFER_H
#define __SANO_RTOS_MSG_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Statically define and initialize a message buffer.
 *
 * The message buffer stores variable-length messages in a byte ring buffer.
 * Each queued message is stored with a 32-bit length header followed by the
 * message payload bytes.
 *
 * @param _name Name of the message buffer.
 * @param bufferSize Total size of the underlying byte ring buffer.
 */
#define MSG_BUFFER_DEFINE(_name, _bufferSize) \
    uint8_t _name##Buffer[_bufferSize];       \
    msgBufferHandleType _name = {            \
        .name = #_name,                      \
        .producerWaitQueue = {0},            \
        .consumerWaitQueue = {0},            \
        .buffer = _name##Buffer,             \
        .bufferSize = _bufferSize,            \
        .usedBytes = 0,                      \
        .readIndex = 0,                      \
        .writeIndex = 0,                     \
        .lock = 0}

    /**
     * @brief Variable-length message buffer object.
     */
    typedef struct
    {
        const char *name;                ///< Name of the message buffer.
        taskQueueType producerWaitQueue; ///< Tasks waiting for free space.
        taskQueueType consumerWaitQueue; ///< Tasks waiting for available data.
        uint8_t *buffer;                 ///< Underlying byte ring buffer.
        uint32_t bufferSize;             ///< Total size of the ring buffer in bytes.
        uint32_t usedBytes;              ///< Number of bytes currently occupied by queued messages.
        uint32_t readIndex;              ///< Ring index of the next message header to read.
        uint32_t writeIndex;             ///< Ring index at which the next message header will be written.
        atomic_t lock;                   ///< Spinlock protecting the message buffer state.
    } msgBufferHandleType;

    static inline __attribute__((always_inline)) bool msgBufferEmpty(msgBufferHandleType *pMsgBuffer)
    {
        return (pMsgBuffer == NULL) || (pMsgBuffer->usedBytes == 0U);
    }

    static inline __attribute__((always_inline)) uint32_t msgBufferBytesUsed(msgBufferHandleType *pMsgBuffer)
    {
        return (pMsgBuffer == NULL) ? 0U : pMsgBuffer->usedBytes;
    }

    static inline __attribute__((always_inline)) uint32_t msgBufferBytesFree(msgBufferHandleType *pMsgBuffer)
    {
        return (pMsgBuffer == NULL) ? 0U : (pMsgBuffer->bufferSize - pMsgBuffer->usedBytes);
    }

    int msgBufferSend(msgBufferHandleType *pMsgBuffer, const void *pData, uint32_t length, uint32_t waitTicks);
    int msgBufferReceive(msgBufferHandleType *pMsgBuffer, void *pData, uint32_t *pLength, uint32_t waitTicks);
    int msgBufferNextLength(msgBufferHandleType *pMsgBuffer, uint32_t *pLength);

#ifdef __cplusplus
}
#endif

#endif
