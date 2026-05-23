#ifndef __SANO_RTOS_TASK_INTERNAL_H
#define __SANO_RTOS_TASK_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "sanoRTOS/task.h"
#include "sanoRTOS/taskQueue.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void taskCleanupExited(void);
    void taskProcessExpiredTimeouts(uint32_t currentTick);
    void taskCheckStackOverflow(void);

    static inline __attribute__((always_inline)) bool taskCanPreemptCurrentCore(taskHandleType *pTask)
    {
        taskHandleType *currentTask = taskPool.currentTask[PORT_CORE_ID()];

        if ((pTask == NULL) || (currentTask == NULL))
        {
            return false;
        }
#if CONFIG_SMP
        if ((pTask->coreAffinity != AFFINITY_CORE_ANY) &&
            (pTask->coreAffinity != PORT_CORE_ID()))
        {
            return false;
        }
#endif

        return (pTask->priority <= currentTask->priority);
    }

    int taskBlockOnWaitQueue(taskQueueType *pWaitQueue,
                             blockedReasonType blockedReason,
                             uint32_t ticks,
                             bool objectIrqState);

    int taskSetReady(taskHandleType *pTask,
                     wakeupReasonType wakeupReason);

#ifdef __cplusplus
}
#endif

#endif
