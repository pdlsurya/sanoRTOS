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

typedef struct
{
    waitQueueType waitQueue;
    mutexHandleType *pMutex;
} condVarHandleType;

void condVarInit(condVarHandleType *pCondVar, mutexHandleType *pMutex);

bool condVarWait(condVarHandleType *pCondVar, uint32_t waitTicks);

bool condVarSignal(condVarHandleType *pCondVar);

bool condVarBroadcast(condVarHandleType *pCondVar);

#endif