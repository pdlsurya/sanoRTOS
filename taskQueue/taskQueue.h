/**
 * @file taskQueue.h
 * @author Surya Poudel
 * @brief Task queue implementation
 * @version 0.1
 * @date 2024-05-08
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef __SANO_RTOS_TASK_QUEUE_H
#define __SANO_RTOS_TASK_QUEUE_H

#include "osConfig.h"

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