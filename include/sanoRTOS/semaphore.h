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

#ifndef __SANO_RTOS_SEMAPHORE_H
#define __SANO_RTOS_SEMAPHORE_H

#include <stdint.h>
#include <stdbool.h>
#include "sanoRTOS/config.h"
#include "sanoRTOS/port.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Statically define and initialize a semaphore.
 * @param name Name of the semaphore.
 * @param initialCount Initial semaphore count.
 * @param maxCnt Maximum semaphore count.
 */
#define SEMAPHORE_DEFINE(name, initialCount, maxCnt) \
    semaphoreHandleType name = {                     \
        .lock = 0,                                   \
        .waitQueue = {0},                            \
        .count = initialCount,                       \
        .maxCount = maxCnt}

    typedef struct
    {
        atomic_t lock;
        taskQueueType waitQueue;
        uint8_t count;
        uint8_t maxCount;
    } semaphoreHandleType;

    int semaphoreTake(semaphoreHandleType *pSem, uint32_t waitTicks);

    int semaphoreGive(semaphoreHandleType *pSem);

#ifdef __cplusplus
}
#endif

#endif