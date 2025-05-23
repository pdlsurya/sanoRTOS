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

#ifndef __SANO_RTOS_MUTEX_H
#define __SANO_RTOS_MUTEX_H

#include <stdint.h>
#include <stdbool.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/port.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/config.h"
#include "sanoRTOS/spinLock.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Statically define and initialize a mutex.
 * @param name Name of the mutex.
 */
#define MUTEX_DEFINE(_name)         \
    mutexHandleType _name = {       \
        .name = #_name,             \
        .lock = 0,                  \
        .waitQueue = {0},           \
        .ownerTask = NULL,          \
        .ownerDefaultPriority = -1, \
        .locked = false}

    /**
     * @brief Structure representing a mutex for task synchronization.
     */
    typedef struct
    {
        const char *name;             ///< Name of the mutex.
        atomic_t lock;                ///< Spinlock variable used for atomic access to the mutex (for low-level synchronization).
        taskQueueType waitQueue;      ///< Queue of tasks waiting to acquire the mutex.
        taskHandleType *ownerTask;    ///< Pointer to the task currently holding the mutex.
        int16_t ownerDefaultPriority; ///< Original priority of the owner task before priority inheritance (if used).
        bool locked;                  ///< Indicates whether the mutex is currently locked.
    } mutexHandleType;

    int mutexLock(mutexHandleType *pMutex, uint32_t waitTicks);

    int mutexUnlock(mutexHandleType *pMutex);

#ifdef __cplusplus
}
#endif

#endif