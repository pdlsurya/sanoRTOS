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

#ifndef __SANO_RTOS_STREAM_BUFFER_H
#define __SANO_RTOS_STREAM_BUFFER_H

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
 * @brief Statically define and initialize a stream buffer.
 *
 * The stream buffer stores raw bytes in a ring buffer without preserving any
 * message boundaries.
 *
 * @param _name Name of the stream buffer.
 * @param _bufferSize Total size of the underlying byte ring buffer.
 */
#define STREAM_BUFFER_DEFINE(_name, _bufferSize) \
    uint8_t _name##Buffer[_bufferSize];          \
    streamBufferHandleType _name = {             \
        .name = #_name,                          \
        .producerWaitQueue = {0},                \
        .consumerWaitQueue = {0},                \
        .buffer = _name##Buffer,                 \
        .bufferSize = _bufferSize,               \
        .usedBytes = 0,                          \
        .readIndex = 0,                          \
        .writeIndex = 0,                         \
        .lock = 0}

    /**
     * @brief Stream buffer object used for byte-stream communication.
     */
    typedef struct
    {
        const char *name;                ///< Name of the stream buffer.
        taskQueueType producerWaitQueue; ///< Tasks waiting for free space.
        taskQueueType consumerWaitQueue; ///< Tasks waiting for available data.
        uint8_t *buffer;                 ///< Underlying byte ring buffer.
        uint32_t bufferSize;             ///< Total size of the ring buffer in bytes.
        uint32_t usedBytes;              ///< Number of bytes currently stored.
        uint32_t readIndex;              ///< Ring index of the next byte to read.
        uint32_t writeIndex;             ///< Ring index at which the next byte will be written.
        atomic_t lock;                   ///< Spinlock protecting the stream buffer state.
    } streamBufferHandleType;

    /**
     * @brief Check whether the stream buffer is empty.
     *
     * @param pStreamBuffer Pointer to stream buffer handle.
     * @retval `true` if the buffer contains no bytes.
     * @retval `false` otherwise.
     */
    static inline __attribute__((always_inline)) bool streamBufferEmpty(streamBufferHandleType *pStreamBuffer)
    {
        return (pStreamBuffer == NULL) || (pStreamBuffer->usedBytes == 0U);
    }

    /**
     * @brief Get the number of bytes currently stored in the stream buffer.
     *
     * @param pStreamBuffer Pointer to stream buffer handle.
     * @return Number of used bytes, or `0` for `NULL`.
     */
    static inline __attribute__((always_inline)) uint32_t streamBufferBytesUsed(streamBufferHandleType *pStreamBuffer)
    {
        return (pStreamBuffer == NULL) ? 0U : pStreamBuffer->usedBytes;
    }

    /**
     * @brief Get the number of free bytes remaining in the stream buffer.
     *
     * @param pStreamBuffer Pointer to stream buffer handle.
     * @return Number of free bytes, or `0` for `NULL`.
     */
    static inline __attribute__((always_inline)) uint32_t streamBufferBytesFree(streamBufferHandleType *pStreamBuffer)
    {
        return (pStreamBuffer == NULL) ? 0U : (pStreamBuffer->bufferSize - pStreamBuffer->usedBytes);
    }

    /**
     * @brief Send raw bytes to a stream buffer.
     *
     * The full `length` must fit before data is written.
     *
     * @param pStreamBuffer Pointer to stream buffer handle.
     * @param pData Pointer to source bytes. May be `NULL` only when `length` is zero.
     * @param length Number of bytes to send.
     * @param waitTicks Maximum ticks to wait if insufficient free space is available.
     * @return `RET_SUCCESS` on success, `RET_FULL` if the buffer is full and no wait was requested,
     *         `RET_TIMEOUT` on timeout, `RET_INVAL` for invalid arguments, or another error code.
     */
    int streamBufferSend(streamBufferHandleType *pStreamBuffer, const void *pData, uint32_t length, uint32_t waitTicks);

    /**
     * @brief Receive raw bytes from a stream buffer.
     *
     * `*pLength` provides the maximum number of bytes to read on entry and is
     * updated with the actual number of bytes read on success.
     *
     * @param pStreamBuffer Pointer to stream buffer handle.
     * @param pData Pointer to destination buffer. May be `NULL` only when `*pLength` is zero.
     * @param pLength Pointer to requested length on entry and received length on exit.
     * @param waitTicks Maximum ticks to wait if the buffer is empty.
     * @return `RET_SUCCESS` on success, `RET_EMPTY` if the buffer is empty and no wait was requested,
     *         `RET_TIMEOUT` on timeout, `RET_INVAL` for invalid arguments, or another error code.
     */
    int streamBufferReceive(streamBufferHandleType *pStreamBuffer, void *pData, uint32_t *pLength, uint32_t waitTicks);

    /**
     * @brief Peek raw bytes from a stream buffer without consuming them.
     *
     * `*pLength` provides the maximum number of bytes to peek on entry and is
     * updated with the actual number of bytes copied on success.
     *
     * @param pStreamBuffer Pointer to stream buffer handle.
     * @param pData Pointer to destination buffer. May be `NULL` only when `*pLength` is zero.
     * @param pLength Pointer to requested length on entry and peeked length on exit.
     * @return `RET_SUCCESS` on success, `RET_EMPTY` if the buffer is empty, `RET_INVAL` for invalid arguments.
     */
    int streamBufferPeek(streamBufferHandleType *pStreamBuffer, void *pData, uint32_t *pLength);

    /**
     * @brief Reset a stream buffer to the empty state.
     *
     * Stored bytes are discarded. Producers waiting only for free space are
     * woken so they can retry immediately.
     *
     * @param pStreamBuffer Pointer to stream buffer handle.
     * @return `RET_SUCCESS` on success, `RET_INVAL` for invalid arguments, or another error code.
     */
    int streamBufferReset(streamBufferHandleType *pStreamBuffer);

#ifdef __cplusplus
}
#endif

#endif
