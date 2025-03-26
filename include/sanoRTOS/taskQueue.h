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

#ifndef __SANO_RTOS_TASK_QUEUE_H
#define __SANO_RTOS_TASK_QUEUE_H

#include "sanoRTOS/config.h"

#ifdef __cplusplus
extern "C"
{
#endif
    /*Forward declaration of taskHandleType*/
    typedef struct taskHandle taskHandleType;

    typedef struct taskNode
    {
        taskHandleType *pTask;
        struct taskNode *nextTaskNode;
    } taskNodeType;

    typedef struct
    {
        taskNodeType *head;
    } taskQueueType;

    taskHandleType *taskQueueGet(taskQueueType *pTaskQueue);

    void taskQueueAdd(taskQueueType *pTaskQueue, taskHandleType *pTask);

    void taskQueueAddToFront(taskQueueType *pTaskQueue, taskHandleType *pTask);

    void taskQueueRemove(taskQueueType *pTaskQueue, taskHandleType *pTask);

    /**
     * @brief Check if taskQueue is empty
     *
     * @param pTaskQueue
     * @retval true if taskQueue is empty
     * @retval false, otherwise
     */
    static inline bool taskQueueEmpty(taskQueueType *pTaskQueue)
    {
        return pTaskQueue->head == NULL;
    }

    /**
     * @brief Get task corresponding to front node from the task queue without removing the node
     *
     * @param pTaskQueue Pointer to the taskQueue struct
     * @return Pointer to taskHandle struct corresponding to front node
     */
    static inline taskHandleType *taskQueuePeek(taskQueueType *pTaskQueue)
    {
        return pTaskQueue->head->pTask;
    }

#ifdef __cplusplus
}
#endif

#endif
