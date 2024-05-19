/**
 * @file conditionVariable.h
 * @author Surya Poudel
 * @brief
 * @version 0.1
 * @date 2024-05-10
 *
 * @copyright Copyright (c) 2024
 *
 */
#ifndef __SANO_RTOS_CONDITION_VARIABLE
#define __SANO_RTOS_CONDITION_VARIABLE

#include "task/task.h"
#include "mutex/mutex.h"
#include "utils/utils.h"

#define CONDVAR_DEFINE(condVarHandle, p_mutex) \
    condVarHandleType condVarHandle = {        \
        .waitQueue = {0},                      \
        .pMutex = p_mutex}

typedef struct
{
    taskQueueType waitQueue;
    mutexHandleType *pMutex;
} condVarHandleType;

bool condVarWait(condVarHandleType *pCondVar, uint32_t waitTicks);

bool condVarSignal(condVarHandleType *pCondVar);

bool condVarBroadcast(condVarHandleType *pCondVar);

#endif