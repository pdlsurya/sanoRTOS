/**
 * @file conditionVariable.h
 * @author Surya Poudel
 * @brief Condition variable APIs
 * @version 0.1
 * @date 2024-05-10
 *
 * @copyright Copyright (c) 2024 Surya Poudel
 *
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