# sanoRTOS API Reference

This guide covers the public application-facing APIs for the main sanoRTOS kernel objects, along with small usage examples for each one.

## Conventions

- Lower numeric task priority means higher scheduling priority.
- `TASK_NO_WAIT` means "do not block".
- `TASK_MAX_WAIT` means "wait indefinitely".
- APIs documented as thread-only must not be called from ISR context.
- APIs that allow ISR use require `waitTicks == TASK_NO_WAIT` when called from ISR context.

## Basic Setup

Most applications include at least:

```c
#include "sanoRTOS/task.h"
#include "sanoRTOS/scheduler.h"
```

Minimal startup:

```c
void appTask(void *args);

TASK_DEFINE(appTaskHandle, 1024, appTask, NULL, 1, AFFINITY_CORE_ANY);

void appTask(void *args)
{
    while (1)
    {
        taskSleepMS(1000);
    }
}

int main(void)
{
    taskStart(&appTaskHandle);
    schedulerStart();
    while (1)
    {
    }
}
```

## Time Conversion Helpers

Defined in [`scheduler.h`](include/sanoRTOS/scheduler.h):

- `US_TO_TIMER_TICKS(us)`
- `US_TO_RTOS_TICKS(us)`
- `MS_TO_RTOS_TICKS(ms)`
- `schedulerStart()`
- `taskYield()`

Example:

```c
uint32_t delay = MS_TO_RTOS_TICKS(250);
taskSleep(delay);
```

## Tasks

Defined in [`task.h`](include/sanoRTOS/task.h).

### Static task definition

- `TASK_DEFINE(name, stackSize, taskEntryFunction, taskParams, taskPriority, affinity)`
- `taskStart(taskHandleType *pTask)`

Example:

```c
void workerTask(void *args);

TASK_DEFINE(workerTaskHandle, 1024, workerTask, NULL, 2, AFFINITY_CORE_ANY);

void workerTask(void *args)
{
    while (1)
    {
        taskSleepMS(500);
    }
}

int main(void)
{
    taskStart(&workerTaskHandle);
    schedulerStart();
}
```

### Dynamic task creation

- `taskCreate(taskHandleType **ppTask, const char *name, uint32_t stackSize, taskFunctionType taskEntryFunction, void *taskParams, uint8_t taskPriority, coreAffinityType affinity)`
- `taskDelete(taskHandleType *pTask)`

Example:

```c
static taskHandleType *dynamicTask;

void dynamicTaskEntry(void *args)
{
    while (1)
    {
        taskSleepMS(100);
    }
}

void startDynamicTask(void)
{
    if (taskCreate(&dynamicTask, "dynamicTask", 1024, dynamicTaskEntry, NULL, 3, AFFINITY_CORE_ANY) == RET_SUCCESS)
    {
        /* Task is created and started. */
    }
}

void stopDynamicTask(void)
{
    if (dynamicTask != NULL)
    {
        (void)taskDelete(dynamicTask);
        dynamicTask = NULL;
    }
}
```

### Sleep and yield

- `taskSleep(ticks)`
- `taskSleepMS(ms)`
- `taskSleepUS(us)`
- `taskYield()`
- `taskSuspend(taskHandleType *pTask)`
- `taskResume(taskHandleType *pTask)`
- `taskGetCurrent()`
- `taskGetName()`

Example:

```c
void pacedTask(void *args)
{
    while (1)
    {
        /* Do work. */
        taskSleepMS(10);
    }
}
```

Suspend and resume example:

```c
void pauseWorker(taskHandleType *worker)
{
    (void)taskSuspend(worker);
}

void resumeWorker(taskHandleType *worker)
{
    (void)taskResume(worker);
}
```

### Task notifications

Direct task notifications are a lightweight per-task signaling mechanism.

- `taskNotify(taskHandleType *pTask, uint32_t value, taskNotifyActionType action)`
- `taskNotifyWait(uint32_t clearMaskOnEntry, uint32_t clearMaskOnExit, uint32_t *pValue, uint32_t waitTicks)`
- `taskNotifyTake(bool clearCountOnExit, uint32_t *pPreviousValue, uint32_t waitTicks)`

Useful actions:

- `TASK_NOTIFY_NO_ACTION`
- `TASK_NOTIFY_SET_BITS`
- `TASK_NOTIFY_INCREMENT`
- `TASK_NOTIFY_SET_VALUE_WITH_OVERWRITE`
- `TASK_NOTIFY_SET_VALUE_WITHOUT_OVERWRITE`

Bit/value style example:

```c
static taskHandleType *consumerTask;

void consumerEntry(void *args)
{
    uint32_t value;

    consumerTask = taskGetCurrent();

    while (1)
    {
        if (taskNotifyWait(0, 0xFFFFFFFFu, &value, TASK_MAX_WAIT) == RET_SUCCESS)
        {
            /* Process notification bits/value. */
        }
    }
}

void producerSendFlag(void)
{
    (void)taskNotify(consumerTask, 1u << 0, TASK_NOTIFY_SET_BITS);
}
```

Semaphore-like example:

```c
void workerEntry(void *args)
{
    while (1)
    {
        (void)taskNotifyTake(true, NULL, TASK_MAX_WAIT);
        /* Consume one unit of work. */
    }
}

void submitWork(taskHandleType *worker)
{
    (void)taskNotify(worker, 0, TASK_NOTIFY_INCREMENT);
}
```

## Spin Lock

Defined in [`spinLock.h`](include/sanoRTOS/spinLock.h). This is a low-level primitive mainly for kernel or driver code.

- `spinLock(atomic_t *pLock)` returns the previous IRQ state
- `spinUnlock(atomic_t *pLock, bool irqState)`

Example:

```c
static atomic_t myLock;
static uint32_t sharedValue;

void updateSharedValue(uint32_t value)
{
    bool irqState = spinLock(&myLock);
    sharedValue = value;
    spinUnlock(&myLock, irqState);
}
```

## Semaphore

Defined in [`semaphore.h`](include/sanoRTOS/semaphore.h).

- `SEMAPHORE_DEFINE(name, initialCount, maxCount)`
- `semaphoreTake(semaphoreHandleType *pSem, uint32_t waitTicks)`
- `semaphoreGive(semaphoreHandleType *pSem)`

Example:

```c
SEMAPHORE_DEFINE(rxSemaphore, 0, 1);

void producerTask(void *args)
{
    while (1)
    {
        /* Produce data. */
        (void)semaphoreGive(&rxSemaphore);
    }
}

void consumerTask(void *args)
{
    while (1)
    {
        if (semaphoreTake(&rxSemaphore, TASK_MAX_WAIT) == RET_SUCCESS)
        {
            /* Consume data. */
        }
    }
}
```

## Mutex

Defined in [`mutex.h`](include/sanoRTOS/mutex.h).

- `MUTEX_DEFINE(name)`
- `mutexLock(mutexHandleType *pMutex, uint32_t waitTicks)`
- `mutexUnlock(mutexHandleType *pMutex)`

This is a thread-only object.

Example:

```c
MUTEX_DEFINE(logMutex);

void safeLog(const char *text)
{
    if (mutexLock(&logMutex, TASK_MAX_WAIT) == RET_SUCCESS)
    {
        /* Write to UART/log sink here. */
        (void)mutexUnlock(&logMutex);
    }
}
```

## Condition Variable

Defined in [`conditionVariable.h`](include/sanoRTOS/conditionVariable.h).

- `CONDVAR_DEFINE(name, pMutex)`
- `condVarWait(condVarHandleType *pCondVar, uint32_t waitTicks)`
- `condVarSignal(condVarHandleType *pCondVar)`
- `condVarBroadcast(condVarHandleType *pCondVar)`

This is a thread-only object and uses an associated mutex.

Example:

```c
MUTEX_DEFINE(queueMutex);
CONDVAR_DEFINE(queueCondVar, &queueMutex);

static bool dataReady;

void consumerTask(void *args)
{
    while (1)
    {
        (void)mutexLock(&queueMutex, TASK_MAX_WAIT);
        while (!dataReady)
        {
            (void)condVarWait(&queueCondVar, TASK_MAX_WAIT);
        }
        dataReady = false;
        (void)mutexUnlock(&queueMutex);
    }
}

void producerTask(void *args)
{
    while (1)
    {
        (void)mutexLock(&queueMutex, TASK_MAX_WAIT);
        dataReady = true;
        (void)condVarSignal(&queueCondVar);
        (void)mutexUnlock(&queueMutex);
        taskSleepMS(100);
    }
}
```

## Event Object

Defined in [`event.h`](include/sanoRTOS/event.h).

- `EVENT_DEFINE(name)`
- `eventSet(eventHandleType *pEvent, uint32_t events)`
- `eventClear(eventHandleType *pEvent, uint32_t events)`
- `eventGet(eventHandleType *pEvent, uint32_t *pEvents)`
- `eventWaitAny(...)`
- `eventWaitAll(...)`
- `eventSync(...)`

Example:

```c
#define EVENT_RX_READY   (1u << 0)
#define EVENT_TX_DONE    (1u << 1)

EVENT_DEFINE(ioEvents);

void ioTask(void *args)
{
    uint32_t matched;

    while (1)
    {
        if (eventWaitAny(&ioEvents, EVENT_RX_READY | EVENT_TX_DONE, true, &matched, TASK_MAX_WAIT) == RET_SUCCESS)
        {
            if (matched & EVENT_RX_READY)
            {
                /* Handle RX. */
            }
            if (matched & EVENT_TX_DONE)
            {
                /* Handle TX completion. */
            }
        }
    }
}
```

## Message Queue

Defined in [`messageQueue.h`](include/sanoRTOS/messageQueue.h).

- `MSG_QUEUE_DEFINE(name, length, itemSize)`
- `msgQueueSend(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks)`
- `msgQueueReceive(msgQueueHandleType *pQueueHandle, void *pItem, uint32_t waitTicks)`
- `msgQueueFull(...)`
- `msgQueueEmpty(...)`

This is a fixed-size FIFO queue.

Example:

```c
typedef struct
{
    uint32_t id;
    uint32_t value;
} sampleMsgType;

MSG_QUEUE_DEFINE(sampleQueue, 8, sizeof(sampleMsgType));

void senderTask(void *args)
{
    sampleMsgType msg = {.id = 1, .value = 1234};
    (void)msgQueueSend(&sampleQueue, &msg, TASK_MAX_WAIT);
}

void receiverTask(void *args)
{
    sampleMsgType msg;

    while (1)
    {
        if (msgQueueReceive(&sampleQueue, &msg, TASK_MAX_WAIT) == RET_SUCCESS)
        {
            /* Process msg. */
        }
    }
}
```

## Stream Buffer

Defined in [`streamBuffer.h`](include/sanoRTOS/streamBuffer.h).

- `STREAM_BUFFER_DEFINE(name, bufferSize)`
- `streamBufferSend(streamBufferHandleType *pStreamBuffer, const void *pData, uint32_t length, uint32_t waitTicks)`
- `streamBufferReceive(streamBufferHandleType *pStreamBuffer, void *pData, uint32_t *pLength, uint32_t waitTicks)`
- `streamBufferPeek(streamBufferHandleType *pStreamBuffer, void *pData, uint32_t *pLength)`
- `streamBufferBytesUsed(...)`
- `streamBufferBytesFree(...)`
- `streamBufferEmpty(...)`

This is a byte stream. Message boundaries are not preserved.

Example:

```c
STREAM_BUFFER_DEFINE(uartStream, 64);

void producerTask(void *args)
{
    static const char text[] = "ABCDE12345";
    (void)streamBufferSend(&uartStream, text, sizeof(text) - 1, TASK_MAX_WAIT);
}

void consumerTask(void *args)
{
    uint8_t buffer[8];
    uint32_t length = sizeof(buffer);

    while (1)
    {
        length = sizeof(buffer);
        if (streamBufferReceive(&uartStream, buffer, &length, TASK_MAX_WAIT) == RET_SUCCESS)
        {
            /* length may be any value from 1 to sizeof(buffer). */
        }
    }
}
```

## Message Buffer

Defined in [`messageBuffer.h`](include/sanoRTOS/messageBuffer.h).

- `MSG_BUFFER_DEFINE(name, bufferSize)`
- `msgBufferSend(msgBufferHandleType *pMsgBuffer, const void *pData, uint32_t length, uint32_t waitTicks)`
- `msgBufferReceive(msgBufferHandleType *pMsgBuffer, void *pData, uint32_t *pLength, uint32_t waitTicks)`
- `msgBufferNextLength(msgBufferHandleType *pMsgBuffer, uint32_t *pLength)`
- `msgBufferBytesUsed(...)`
- `msgBufferBytesFree(...)`
- `msgBufferEmpty(...)`

This is a variable-length message FIFO built on top of a stream buffer. Message boundaries are preserved.

Example:

```c
MSG_BUFFER_DEFINE(logBuffer, 128);

void senderTask(void *args)
{
    static const char hello[] = "HELLO";
    static const char world[] = "ABCDEFGHIJ";

    (void)msgBufferSend(&logBuffer, hello, sizeof(hello) - 1, TASK_MAX_WAIT);
    (void)msgBufferSend(&logBuffer, world, sizeof(world) - 1, TASK_MAX_WAIT);
}

void receiverTask(void *args)
{
    uint8_t smallBuf[8];
    uint32_t length;

    while (1)
    {
        length = sizeof(smallBuf);
        if (msgBufferReceive(&logBuffer, smallBuf, &length, TASK_MAX_WAIT) == RET_SUCCESS)
        {
            /* Received one full message. length is the actual message length. */
        }
        else if (length > sizeof(smallBuf))
        {
            /* Next queued message is larger than smallBuf. */
        }
    }
}
```

## Mailbox

Defined in [`mailbox.h`](include/sanoRTOS/mailbox.h).

- `MAILBOX_DEFINE(name)`
- `MAILBOX_ANY_TASK`
- `mailboxSend(mailboxHandleType *pMailbox, mailboxMsgType *pMsg, uint32_t waitTicks)`
- `mailboxReceive(mailboxHandleType *pMailbox, mailboxMsgType *pMsg, void *pBuffer, uint32_t waitTicks)`

Mailboxes match senders and receivers directly. They are thread-only objects.

Example:

```c
MAILBOX_DEFINE(commandMailbox);

void senderTask(void *args)
{
    static const uint8_t payload[] = {1, 2, 3, 4};
    mailboxMsgType msg = {
        .info = 0x10,
        .size = sizeof(payload),
        .pTxData = payload,
        .pTargetTask = MAILBOX_ANY_TASK,
        .pSourceTask = taskGetCurrent(),
    };

    (void)mailboxSend(&commandMailbox, &msg, TASK_MAX_WAIT);
}

void receiverTask(void *args)
{
    uint8_t buffer[16];
    mailboxMsgType msg = {
        .size = sizeof(buffer),
        .pSourceTask = MAILBOX_ANY_TASK,
    };

    if (mailboxReceive(&commandMailbox, &msg, buffer, TASK_MAX_WAIT) == RET_SUCCESS)
    {
        /* msg.info and msg.size describe the transferred payload. */
    }
}
```

## Software Timer

Defined in [`timer.h`](include/sanoRTOS/timer.h).

- `TIMER_DEFINE(name, timeoutHandler, timerMode)`
- `timerStart(timerNodeType *pTimerNode, uint32_t interval)`
- `timerStop(timerNodeType *pTimerNode)`

Modes:

- `TIMER_MODE_SINGLE_SHOT`
- `TIMER_MODE_PERIODIC`

Notes:

- `schedulerStart()` starts the internal timer service task automatically.
- Applications usually do not need to call `timerTaskStart()` directly.

Example:

```c
void heartbeatTimeout(void *arg);

TIMER_DEFINE(heartbeatTimer, heartbeatTimeout, TIMER_MODE_PERIODIC);

void heartbeatTimeout(void *arg)
{
    /* Timer callback work. Keep it short. */
}

void startHeartbeat(void)
{
    (void)timerStart(&heartbeatTimer, MS_TO_RTOS_TICKS(1000));
}
```

## Work Queue

Defined in [`workQueue.h`](include/sanoRTOS/workQueue.h).

- `WORK_DEFINE(name, handler, arg)`
- `WORK_QUEUE_DEFINE(name, stackSize, taskPriority, affinity)`
- `workInit(...)`
- `workQueueStart(workQueueHandleType *pWorkQueue)`
- `workSubmit(workQueueHandleType *pWorkQueue, workItemType *pWork)`
- `workPending(workItemType *pWork)`

This is a FIFO of work items executed by a dedicated worker task.

Example:

```c
void blinkWorkHandler(void *arg);

WORK_QUEUE_DEFINE(systemWorkQueue, 1024, 1, AFFINITY_CORE_ANY);
WORK_DEFINE(blinkWork, blinkWorkHandler, NULL);

void blinkWorkHandler(void *arg)
{
    /* Deferred work. */
}

void appInit(void)
{
    (void)workQueueStart(&systemWorkQueue);
}

void triggerDeferredWork(void)
{
    (void)workSubmit(&systemWorkQueue, &blinkWork);
}
```

## Delayed Work

Defined in [`workQueue.h`](include/sanoRTOS/workQueue.h).

- `DELAYED_WORK_DEFINE(name, handler, arg)`
- `delayedWorkInit(...)`
- `delayedWorkSchedule(workQueueHandleType *pWorkQueue, delayedWorkType *pDelayedWork, uint32_t delayTicks)`
- `delayedWorkCancel(delayedWorkType *pDelayedWork)`
- `delayedWorkPending(delayedWorkType *pDelayedWork)`

Delayed work uses a one-shot timer and submits the underlying work item to a work queue when the delay expires.

Example:

```c
void ledWorkHandler(void *arg);

WORK_QUEUE_DEFINE(systemWorkQueue, 1024, 1, AFFINITY_CORE_ANY);
DELAYED_WORK_DEFINE(statusLedWork, ledWorkHandler, NULL);

void ledWorkHandler(void *arg)
{
    /* Deferred work after the delay. */
}

void appInit(void)
{
    (void)workQueueStart(&systemWorkQueue);
}

void scheduleLedUpdate(void)
{
    (void)delayedWorkSchedule(&systemWorkQueue, &statusLedWork, MS_TO_RTOS_TICKS(500));
}
```

## Memory Slab

Defined in [`memSlab.h`](include/sanoRTOS/memSlab.h).

- `MEM_SLAB_DEFINE(name, blockSize, numBlocks)`
- `memSlabAlloc(memSlabHandleType *pMemSlab, void **ppBlock, uint32_t waitTicks)`
- `memSlabFree(memSlabHandleType *pMemSlab, void *pBlock)`
- `memSlabFreeBlocks(...)`
- `memSlabBlockSize(...)`

This is a deterministic fixed-size allocator.

Example:

```c
MEM_SLAB_DEFINE(packetSlab, 64, 8);

void usePacketBuffer(void)
{
    void *block;

    if (memSlabAlloc(&packetSlab, &block, TASK_MAX_WAIT) == RET_SUCCESS)
    {
        /* Use the 64-byte block. */
        (void)memSlabFree(&packetSlab, block);
    }
}
```

## Heap Wrapper

Defined in [`memHeap.h`](include/sanoRTOS/memHeap.h).

- `memHeapAlloc(size_t size)`
- `memHeapFree(void *ptr)`
- `memHeapRealloc(void *ptr, size_t size)`

This is a thin wrapper around heap allocation. Prefer kernel objects or memory slabs where deterministic behavior matters.

Example:

```c
char *buffer = memHeapAlloc(128);
if (buffer != NULL)
{
    buffer = memHeapRealloc(buffer, 256);
    memHeapFree(buffer);
}
```

## Recommended Object Choices

- Use `semaphore` for counts or handoff signals.
- Use `mutex` for mutual exclusion around shared state.
- Use `condition variable` when you need "sleep until predicate becomes true" with a mutex.
- Use `event object` for bitmask-style waits.
- Use `message queue` for fixed-size FIFO items.
- Use `stream buffer` for raw byte streams.
- Use `message buffer` for variable-length discrete messages.
- Use `mailbox` for direct sender/receiver matching.
- Use `work queue` for deferred task-context work.
- Use `timer` for timeout and periodic scheduling.
- Use `memory slab` for deterministic fixed-size allocation.
