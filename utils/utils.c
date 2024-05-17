/**
 * @file utils.c
 * @author Surya Poudel
 * @brief Utility functions.
 * @version 0.1
 * @date 2024-05-08
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include "osConfig.h"
#include "task/task.h"
#include "utils.h"

/**
 * @brief Compare function for qsort function to sort the task queue/list in ascending order of the task's priority.
 * @param arg1
 * @param arg1
 * @return Compare result
 */
int sortCompareFunction(const void *arg1, const void *arg2)
{
    const taskHandleType *pTask1 = *(taskHandleType **)arg1;
    const taskHandleType *pTask2 = *(taskHandleType **)arg2;
    if (!pTask1 && !pTask2)
        return 0;
    if (!pTask1)
        return 1;
    if (!pTask2)
        return -1;

    return pTask1->priority - pTask2->priority;
}

/**
 * @brief Get the index of a task in the list of tasks. This performs a binary search in the
 * list of tasks sorted according to their taskId.
 * @param array List of READY tasks
 * @param len Length of the list
 * @param pTask Pointer to the taskHandle struct
 * @return Index of the task or -1 if not found.
 */
int getTaskIndex(taskHandleType **array, uint8_t len, volatile taskHandleType *pTask)
{
    int low = 0, high = len - 1, mid;

    while (low <= high)
    {
        mid = low + (high - low) / 2;
        if (array[mid]->taskId == pTask->taskId)
            return mid;
        if (pTask->taskId < array[mid]->taskId)
            high = mid - 1;

        else if (pTask->taskId > array[mid]->taskId)
            low = mid + 1;
    }
    return -1;
}

/**
 * @brief Initialize wait queue
 *
 * @param pWaitQueue
 */
void waitQueueInit(waitQueueType *pWaitQueue)
{
    pWaitQueue->count = 0;
    memset(pWaitQueue->waitingTasks, 0, WAIT_QUEUE_MAX_SIZE * sizeof(taskHandleType *));
}

/**
 * @brief Put waiting task to waitQueue
 *
 * @param pWaitQueue
 * @param pTask
 * @return true on Success
 * @return false on Fail
 */
bool waitQueuePut(waitQueueType *pWaitQueue, taskHandleType *pTask)
{
    if (pWaitQueue->count != WAIT_QUEUE_MAX_SIZE)
    {
        pWaitQueue->waitingTasks[pWaitQueue->count++] = pTask;
        return true;
    }
    return false;
}

/**
 * @brief Get the highest priority task from the waitQueue
 *
 * @param pWaitQueue Pointer to waitQueue struct
 * @return Next highest priority task if Queue is not empty, or NULL otherwise.
 */
taskHandleType *waitQueueGet(waitQueueType *pWaitQueue)
{
    if (pWaitQueue->count > 0)
    {
        if (pWaitQueue->count > 1)
        {
            // Sort waiting tasks in ascending order of their priority
            qsort(pWaitQueue->waitingTasks, pWaitQueue->count, sizeof(taskHandleType *), sortCompareFunction);
        }
        taskHandleType *pTask = pWaitQueue->waitingTasks[0];

        pWaitQueue->waitingTasks[0] = NULL;

        pWaitQueue->count--;

        if (pWaitQueue->count > 0)
        {
            // Move elements to the beginning of the list
            memmove(pWaitQueue->waitingTasks, &pWaitQueue->waitingTasks[1], pWaitQueue->count * sizeof(taskHandleType *));
        }

        return pTask;
    }

    return NULL;
}