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

#ifndef __SANO_RTOS_MSG_QUEUE_H
#define __SANO_RTOS_MSG_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "sanoRTOS/mutex.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Statically define and initialize a message queue. The message queue internally uses a ring buffer,
 * which is (length * item_size) bytes long, to store message items. The ring buffer's access is serialized using a mutex locak.
 * The message queue operates in First In First Out(FIFO) manner.
 * @param name Name of the message queue.
 * @param length Maximum number of message items the message queue can hold.
 * @param item_size Size of a message item in bytes.
 */
#define MSG_QUEUE_DEFINE(name, length, item_size) \
    uint8_t name##Buffer[length * item_size];     \
    msgQueueHandleType name = {                   \
        .producerWaitQueue = {0},                 \
        .consumerWaitQueue = {0},                 \
        .buffer = name##Buffer,                   \
        .queueLength = length,                    \
        .itemSize = item_size,                    \
        .itemCount = 0,                           \
        .readIndex = 0,                           \
        .writeIndex = 0,                          \
        .lock = 0}

    typedef struct
    {
        taskQueueType producerWaitQueue;
        taskQueueType consumerWaitQueue;
        uint8_t *buffer;
        uint32_t queueLength;
        uint32_t itemSize;
        uint32_t itemCount;
        uint32_t readIndex;
        uint32_t writeIndex;
        atomic_t lock;
    } msgQueueHandleType;

    /**
     * @brief Check if message queue is full
     *
     * @param pQueueHandle
     * @retval true if msgQueue is full
     * @retval false otherwise
     */
    static inline __attribute__((always_inline)) bool msgQueueFull(msgQueueHandleType *pQueueHandle)
    {
        return pQueueHandle->itemCount == pQueueHandle->queueLength;
    }

    /**
     * @brief Check if message queue is empty
     *
     * @param pQueueHandle
     * @retval true if msgQueue is empty
     * @return false otherwise
     */
    static inline __attribute__((always_inline)) bool msgQueueEmpty(msgQueueHandleType *pQueueHandle)
    {
        return (pQueueHandle->itemCount == 0);
    }

    int msgQueueSend(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks);

    int msgQueueReceive(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks);

#ifdef __cplusplus
}
#endif

#endif