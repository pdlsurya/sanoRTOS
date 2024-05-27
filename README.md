# sanoRTOS
sanoRTOS is a minimal Real-Time Operating System (RTOS) designed for ARM Cortex-M microcontrollers. This implementation provides a simple yet effective API for task management, synchronization, and communication, enabling efficient and predictable multitasking in embedded systems.

# Features

- Priority based preemptive scheduling
- Optional Priority Inheritance to avoid Priority inversion problem
- Task synchronization
- Task communication
- Lightweight and minimalistic design

# API Functions


## Task Management

- TASK_DEFINE: Statically define and initialize a task.
- taskStart : Start the task.
- taskYield: Yield the processor to allow other tasks to run.
- taskSleepMS: Delay a task for a specified number of milliseconds.
- taskSleepUS: Delay a task for a specified number of microseconds.
- schedulerStart: Start the RTOS scheduler.


## Mutex

- MUTEX_DEFINE: Statically define and initialize a mutex.
- mutexLock: Acquire a mutex, blocking if necessary.
- mutexUnlock: Release a mutex.

## Semaphore

- SEMAPHORE_DEFINE: Statically define and initialize a semaphore.
- semaphoreTake: Take the semaphore.
- semaphoreGive: Release a semaphore.

## Message Queue

- MSG_QUEUE_DEFINE: Statically define and initialize a message queue.
- msgQueueSend: Send a message to a queue.
- msgQueueReceive: Receive a message from a queue.

## Condition Variable

- CONDVAR_DEFINE: Statically define and initialize a conditon variable.
- condVarWait: Wait on a condition variable.
- condVarSignal: Signal a condition variable, waking one waiting task.
- condVarBroadcast: Broadcast a condition variable, waking all waiting tasks.

## Software Timer

- TIMER_DEFINE: Statically define and initialize a timer
- timerStart: Start a timer with a specified timeout.
- timerStop: Stop a running timer.

# License
This project is licensed under the MIT License.


