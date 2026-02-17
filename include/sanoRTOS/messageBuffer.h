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
#include "sanoRTOS/streamBuffer.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Statically define and initialize a message buffer.
 *
 * The message buffer stores variable-length messages in a stream buffer by
 * writing a 32-bit length header before each message payload.
 *
 * @param _name Name of the message buffer.
 * @param _bufferSize Total size of the underlying byte ring buffer.
 */
#define MSG_BUFFER_DEFINE(_name, _bufferSize) \
    uint8_t _name##Buffer[_bufferSize];       \
    msgBufferHandleType _name = {             \
        .streamBuffer = {                     \
            .name = #_name,                   \
            .producerWaitQueue = {0},         \
            .consumerWaitQueue = {0},         \
            .buffer = _name##Buffer,          \
            .bufferSize = _bufferSize,        \
            .usedBytes = 0,                   \
            .readIndex = 0,                   \
            .writeIndex = 0,                  \
            .lock = 0}}

    /**
     * @brief Variable-length message buffer object built on top of a stream buffer.
     */
    typedef struct
    {
        streamBufferHandleType streamBuffer; ///< Underlying stream buffer storage and wait queues.
    } msgBufferHandleType;

    /**
     * @brief Check whether the message buffer is empty.
     *
     * @param pMsgBuffer Pointer to message buffer handle.
     * @retval `true` if the buffer contains no queued messages.
     * @retval `false` otherwise.
     */
    static inline __attribute__((always_inline)) bool msgBufferEmpty(msgBufferHandleType *pMsgBuffer)
    {
        return (pMsgBuffer == NULL) || streamBufferEmpty(&pMsgBuffer->streamBuffer);
    }

    /**
     * @brief Get the number of payload and header bytes currently stored.
     *
     * @param pMsgBuffer Pointer to message buffer handle.
     * @return Number of used bytes, or `0` for `NULL`.
     */
    static inline __attribute__((always_inline)) uint32_t msgBufferBytesUsed(msgBufferHandleType *pMsgBuffer)
    {
        return (pMsgBuffer == NULL) ? 0U : streamBufferBytesUsed(&pMsgBuffer->streamBuffer);
    }

    /**
     * @brief Get the number of free bytes remaining in the underlying ring buffer.
     *
     * @param pMsgBuffer Pointer to message buffer handle.
     * @return Number of free bytes, or `0` for `NULL`.
     */
    static inline __attribute__((always_inline)) uint32_t msgBufferBytesFree(msgBufferHandleType *pMsgBuffer)
    {
        return (pMsgBuffer == NULL) ? 0U : streamBufferBytesFree(&pMsgBuffer->streamBuffer);
    }

    /**
     * @brief Send one variable-length message to a message buffer.
     *
     * @param pMsgBuffer Pointer to message buffer handle.
     * @param pData Pointer to payload bytes. May be `NULL` only when `length` is zero.
     * @param length Payload length in bytes.
     * @param waitTicks Maximum ticks to wait if the buffer does not have enough free space.
     * @return `RET_SUCCESS` on success, `RET_FULL` if no space and no wait was requested,
     *         `RET_TIMEOUT` on timeout, `RET_INVAL` for invalid arguments, or another error code.
     */
    int msgBufferSend(msgBufferHandleType *pMsgBuffer, const void *pData, uint32_t length, uint32_t waitTicks);

    /**
     * @brief Receive the next queued message from a message buffer.
     *
     * The caller provides the destination capacity through `*pLength`. On success,
     * `*pLength` is updated with the received payload length. If the next queued
     * message is larger than the destination buffer, the message stays queued,
     * `*pLength` is updated with the required length, and the function returns
     * `RET_INVAL`.
     *
     * @param pMsgBuffer Pointer to message buffer handle.
     * @param pData Pointer to destination buffer. May be `NULL` only when `*pLength` is zero.
     * @param pLength Pointer to destination capacity on entry and received length on exit.
     * @param waitTicks Maximum ticks to wait if the buffer is empty.
     * @return `RET_SUCCESS` on success, `RET_EMPTY` if no data and no wait was requested,
     *         `RET_TIMEOUT` on timeout, `RET_INVAL` for invalid arguments or too-small buffer,
     *         or another error code.
     */
    int msgBufferReceive(msgBufferHandleType *pMsgBuffer, void *pData, uint32_t *pLength, uint32_t waitTicks);

    /**
     * @brief Get the length of the next queued message without removing it.
     *
     * @param pMsgBuffer Pointer to message buffer handle.
     * @param pLength Pointer updated with the next message payload length.
     * @return `RET_SUCCESS` on success, `RET_EMPTY` if the buffer is empty, `RET_INVAL` for invalid arguments.
     */
    int msgBufferNextLength(msgBufferHandleType *pMsgBuffer, uint32_t *pLength);

    /**
     * @brief Reset a message buffer to the empty state.
     *
     * Stored messages are discarded. Producers waiting only for free space are
     * woken so they can retry immediately.
     *
     * @param pMsgBuffer Pointer to message buffer handle.
     * @return `RET_SUCCESS` on success, `RET_INVAL` for invalid arguments, or another error code.
     */
    int msgBufferReset(msgBufferHandleType *pMsgBuffer);

#ifdef __cplusplus
}
#endif

#endif
