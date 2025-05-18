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
#include "sanoRTOS/retCodes.h"
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
#define SEMAPHORE_DEFINE(_name, initialCount, maxCnt) \
    semaphoreHandleType _name = {                     \
        .name = #_name,                               \
        .lock = 0,                                    \
        .waitQueue = {0},                             \
        .count = initialCount,                        \
        .maxCount = maxCnt}

    /**
     * @brief Structure representing a counting semaphore for task synchronization.
     */
    typedef struct
    {
        const char *name;        ///< Name of the semaphore.
        atomic_t lock;           ///< Spinlock variable used to ensure atomic access to the semaphore.
        taskQueueType waitQueue; ///< Queue of tasks waiting to acquire the semaphore.
        uint8_t count;           ///< Current count of the semaphore (number of available resources).
        uint8_t maxCount;        ///< Maximum count the semaphore can reach (resource capacity limit).
    } semaphoreHandleType;

    int semaphoreTake(semaphoreHandleType *pSem, uint32_t waitTicks);

    int semaphoreGive(semaphoreHandleType *pSem);

#ifdef __cplusplus
}
#endif

#endif