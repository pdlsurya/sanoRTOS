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

#ifndef __SANO_RTOS_CONDITION_VARIABLE
#define __SANO_RTOS_CONDITION_VARIABLE

#include <stdint.h>
#include <stdbool.h>
#include "sanoRTOS/retCodes.h"
#include "sanoRTOS/task.h"
#include "sanoRTOS/mutex.h"
#include "sanoRTOS/taskQueue.h"
#include "sanoRTOS/spinLock.h"
#include "sanoRTOS/port.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Statically define and initialize a condition variable. Condition variable internally
 * uses a mutex to serialize the access.
 * @param name Name of the condition variable
 * @param p_mutex Pointer to a mutex that condition variable uses internally.
 */
#define CONDVAR_DEFINE(_name, p_mutex) \
    condVarHandleType _name = {        \
        .flags = 0U,                   \
        .waitQueue = {                 \
            .head = NULL,              \
            .tail = NULL,              \
            .pLock = &_name.lock},    \
        .pMutex = p_mutex,             \
        .lock = 0}

    /**
     * @brief Condition variable structure for blocking and waking tasks based on specific conditions.
     */
    typedef struct
    {
        uint8_t flags;           ///< Dynamic ownership flags.
        taskQueueType waitQueue; ///< Queue of tasks waiting on the condition variable.
        mutexHandleType *pMutex; ///< Pointer to the associated mutex (used to avoid race conditions).
        atomicType lock;         ///< Spinlock to ensure atomic access to the condition variable's internal state.
    } condVarHandleType;

    int condVarCreate(condVarHandleType **ppCondVar, mutexHandleType *pMutex);
    int condVarDelete(condVarHandleType *pCondVar);

    /**
     * @brief Wait on a condition variable.
     *
     * The current task blocks until the condition variable is signaled,
     * broadcast, or the timeout expires. This API is intended for task
     * context only. It cannot be called from ISR context.
     *
     * @param pCondVar Pointer to condition variable handle.
     * @param waitTicks Maximum ticks to wait (`TASK_FOREVER_WAIT` for infinite wait).
     * @return `RET_SUCCESS` if signaled, `RET_TIMEOUT` on timeout, `RET_INVAL`
     *         if arguments are invalid, error code otherwise.
     */
    int condVarWait(condVarHandleType *pCondVar, uint32_t waitTicks);

     /**
     * @brief Signal a condition variable and wake one waiting task.
     *
     * Condition variables are paired with task-owned mutex state. This API is
     * intended for task context only. It cannot be called from ISR context.
     *
     * @param pCondVar Pointer to condition variable handle.
     * @return `RET_SUCCESS` on success, `RET_INVAL` if arguments are invalid,
     *         error code otherwise.
     */
    int condVarSignal(condVarHandleType *pCondVar);

     /**
     * @brief Broadcast a condition variable and wake all waiting tasks.
     *
     * Condition variables are paired with task-owned mutex state. This API is
     * intended for task context only. It cannot be called from ISR context.
     *
     * @param pCondVar Pointer to condition variable handle.
     * @return `RET_SUCCESS` on success, `RET_INVAL` if arguments are invalid,
     *         error code otherwise.
     */
    int condVarBroadcast(condVarHandleType *pCondVar);

#ifdef __cplusplus
}
#endif

#endif
