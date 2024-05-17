/**
 * @file mutex.h
 * @author Surya Poudel
 * @brief
 * @version 0.1
 * @date 2024-04-09
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef __SANO_RTOS_MUTEX_H
#define __SANO_RTOS_MUTEX_H

#include <stdint.h>
#include <stdbool.h>
#include "task/task.h"
#include "utils/utils.h"
#include "osConfig.h"

#define MAX_MUTEXES 16

typedef struct
{
    waitQueueType waitQueue;
    volatile taskHandleType *ownerTask;
    int16_t ownerDefaultPriority;
    bool locked;

} mutexHandleType;

bool mutexInit(mutexHandleType *pMutex);

bool mutexLock(mutexHandleType *pMutex, uint32_t waitTicks);

bool mutexUnlock(mutexHandleType *pMutex);

#endif