/**
 * @file task.h
 * @author Surya Poudel
 * @brief
 * @version 0.1
 * @date 2024-04-09
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef __SANO_RTOS_TASK_H
#define __SANO_RTOS_TASK_H

#include "osConfig.h"
#include "taskQueue/taskQueue.h"

#define TASK_LOWEST_PRIORITY 0xff
#define TASK_HIGHEST_PRIORITY 0

#define TASK_DEFINE(taskHandle, stackDepth, taskEntryFunction, taskParams, taskPriority) \
    void taskEntryFunction(void *);                                                      \
    uint32_t taskHandle##Stack[stackDepth] = {0};                                        \
    taskHandleType taskHandle = {                                                        \
        .stackPointer = (uint32_t)(taskHandle##Stack + stackDepth - 17),                 \
        .priority = taskPriority,                                                        \
        .taskEntry = taskEntryFunction,                                                  \
        .params = taskParams,                                                            \
        .remainingSleepTicks = 0,                                                        \
        .status = TASK_STATUS_READY,                                                     \
        .blockedReason = BLOCK_REASON_NONE,                                              \
        .wakeupReason = WAKEUP_REASON_NONE}

typedef void (*taskFunctionType)(void *params);

typedef enum
{
    TASK_STATUS_READY,
    TASK_STATUS_RUNNING,
    TASK_STATUS_BLOCKED,
    TASK_STATUS_SUSPENDED
} taskStatusType;

typedef enum
{
    BLOCK_REASON_NONE,
    SLEEP,
    WAIT_FOR_SEMAPHORE,
    WAIT_FOR_MUTEX,
    WAIT_FOR_MSG_QUEUE_DATA,
    WAIT_FOR_MSG_QUEUE_SPACE,
    WAIT_FOR_COND_VAR,
    WAIT_FOR_TIMER_TIMEOUT,

} blockedReasonType;

typedef enum
{
    WAKEUP_REASON_NONE,
    WAIT_TIMEOUT,
    SLEEP_TIME_TIMEOUT,
    SEMAPHORE_AVAILABLE,
    MUTEX_AVAILABLE,
    MSG_QUEUE_DATA_AVAILABLE,
    MSG_QUEUE_SPACE_AVAILABE,
    COND_VAR_SIGNALLED,
    TIMER_TIMEOUT,
    RESUME

} wakeupReasonType;

typedef struct taskHandle
{
    uint32_t stackPointer;
    taskFunctionType taskEntry;
    void *params;
    uint32_t remainingSleepTicks;
    taskStatusType status;
    blockedReasonType blockedReason;
    wakeupReasonType wakeupReason;
    uint8_t priority;

} taskHandleType;

typedef struct
{
    taskQueueType readyQueue;
    taskQueueType blockedQueue;
    taskHandleType *currentTask;

} taskPoolType;

extern taskHandleType *currentTask;
extern taskHandleType *nextTask;
extern taskPoolType taskPool;

bool taskStart(taskHandleType *pTaskHandle);

void taskSleepMS(uint32_t sleepTimeMS);

void taskSleepUS(uint32_t sleepTimeUS);

void taskSetReady(taskHandleType *pTask, wakeupReasonType wakeupReason);

void taskBlock(taskHandleType *pTask, blockedReasonType blockedReason, uint32_t ticks);

void taskSuspend(taskHandleType *pTask);

bool taskResume(taskHandleType *pTask);

#endif