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

#include "task/task.h"
#include "mutex/mutex.h"
#include "taskQueue/taskQueue.h"

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
#define CONDVAR_DEFINE(name, p_mutex) \
    condVarHandleType name = {        \
        .waitQueue = {0},             \
        .pMutex = p_mutex}

    typedef struct
    {
        taskQueueType waitQueue;
        mutexHandleType *pMutex;
    } condVarHandleType;

    int condVarWait(condVarHandleType *pCondVar, uint32_t waitTicks);

    int condVarSignal(condVarHandleType *pCondVar);

    int condVarBroadcast(condVarHandleType *pCondVar);

#ifdef __cplusplus
}
#endif

#endif