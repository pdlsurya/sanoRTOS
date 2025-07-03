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
#include "sanoRTOS/port.h"
#include "sanoRTOS/retCodes.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Retrieve the next highest-priority task from the ready queue.
 *
 * This macro fetches the task for immediate scheduling, so affinity checking is enabled.
 *
 * @param pReadyQueue Pointer to the ready task queue.
 * @return Task control block (TCB) pointer of the next task to run.
 */
#define TASK_GET_FROM_READY_QUEUE(pReadyQueue) taskQueueGet(pReadyQueue, true)

/**
 * @brief Retrieve the next highest-priority task from the wait queue.
 *
 * Affinity checking is disabled as this queue is not used for direct scheduling.
 *
 * @param pWaitQueue Pointer to the wait task queue.
 * @return Task control block (TCB) pointer of the next waiting task.
 */
#define TASK_GET_FROM_WAIT_QUEUE(pWaitQueue) taskQueueGet(pWaitQueue, false)

/**
 * @brief Peek at the next highest-priority task in the ready queue without removing it.
 *
 * Useful for inspecting which task is scheduled to run next, with affinity checking enabled.
 *
 * @param pReadyQueue Pointer to the ready task queue.
 * @return Task control block (TCB) pointer of the next task to run.
 */
#define TASK_PEEK_FROM_READY_QUEUE(pReadyQueue) taskQueuePeek(pReadyQueue, true)

/**
 * @brief Peek at the next highest-priority task in the wait queue without removing it.
 *
 * Affinity check is disabled. This is typically used for monitoring or diagnostics.
 *
 * @param pWaitQueue Pointer to the wait task queue.
 * @return Task control block (TCB) pointer of the next waiting task.
 */
#define TASK_PEEK_FROM_WAIT_QUEUE(pWaitQueue) taskQueuePeek(pWaitQueue, false)

    /*Forward declaration of taskHandleType*/
    typedef struct taskHandle taskHandleType;

    /**
     * @brief Node structure for linked list of tasks in a task queue.
     */
    typedef struct taskNode
    {
        taskHandleType *pTask;         ///< Pointer to the task represented by this node.
        struct taskNode *nextTaskNode; ///< Pointer to the next node in the task queue.
    } taskNodeType;

    /**
     * @brief Task queue structure used to manage a list of tasks (e.g., for ready, waiting, or blocked states).
     */
    typedef struct
    {
        taskNodeType *head; ///< Pointer to the head of the task queue linked list.
    } taskQueueType;

    taskHandleType *taskQueueGet(taskQueueType *pTaskQueue, bool affinityCheck);

    taskHandleType *taskQueuePeek(taskQueueType *pTaskQueue, bool affinityCheck);

    void taskQueueAdd(taskQueueType *pTaskQueue, taskHandleType *pTask);

    void taskQueueAddToFront(taskQueueType *pTaskQueue, taskHandleType *pTask);

    void taskQueueRemove(taskQueueType *pTaskQueue, taskHandleType *pTask);

    /**
     * @brief Check if taskQueue is empty
     *
     * @param pTaskQueue
     * @retval `TRUE` if taskQueue is empty
     * @retval `FALSE`, otherwise
     */
    static inline __attribute__((always_inline)) bool taskQueueEmpty(taskQueueType *pTaskQueue)
    {
        return (pTaskQueue->head == NULL);
    }

#ifdef __cplusplus
}
#endif

#endif
