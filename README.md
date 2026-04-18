# sanoRTOS
sanoRTOS is a minimal Real-Time Operating System (RTOS) designed for ARM Cortex-M and RISC-V microcontrollers. This implementation provides a simple yet effective API for task management, synchronization, and communication, enabling efficient and predictable multitasking in embedded systems.

## 🚀 Features

- **Priority-Based Preemptive Scheduling**  
  Efficient task management with support for preemptive scheduling based on task priority levels.

- **Optional Privileged and User-Level Tasks**
  Supports the differentiation between privileged (kernel) tasks and user-level tasks. Privileged tasks have access to critical system resources and can execute sensitive operations, 
  while user-level tasks operate with restricted permissions to enhance system security and stability. This separation allows for better isolation and protection between tasks of 
  different trust levels, ensuring that critical system operations are secure from user-level task interference.

- **Optional Priority Inheritance**  
  Prevents priority inversion during mutex acquisition by temporarily elevating the priority of lower-priority tasks.

- **Symmetric Multiprocessing (SMP) Support**  
  Fully supports multi-core systems with per-task **core affinity** configuration for optimal load balancing.

- **Dynamic Stack Overflow Detection**  
  Runtime monitoring of task stacks to catch and handle stack overflows proactively.

- **Configurable Tick Rate**  
  Easily adjustable tick frequency to match application-specific timing and power requirements.

- **Task Synchronization Primitives**  
  Includes mutexes, semaphores, condition variables, and event objects for safe and efficient coordination between tasks.

- **Inter-Task Communication**  
  Enables message passing between tasks using message queues and mailboxes.

- **Minimalistic and Lightweight Design**  
  Designed for embedded systems with limited resources — small footprint, fast context switches, and no unnecessary bloat.


# API Functions

## Task Management

- `TASK_DEFINE(name, stackSize, taskEntryFunction, taskParams, taskPriority, affinity)`: Macro to statically define and initialize a task.
- `taskCreate(ppTask, name, stackSize, taskEntryFunction, taskParams, taskPriority, affinity)`: Dynamically create and start a task.
- `taskDelete(pTask)`: Delete a task. Dynamically created task resources are freed.
- `taskStart(pTask)`: Start a task that has already been defined.
- `taskYield()`: Yield the processor to allow other ready tasks to run.
- `taskSleepMS(sleepTimeMS)`: Delay the current task for a specified number of milliseconds.
- `taskSleepUS(sleepTimeUS)`: Delay the current task for a specified number of microseconds.
- `schedulerStart()`: Start the RTOS scheduler.

## Spin Lock

- `spinLock(pLock)`: Acquire a spin lock and return the previous interrupt state.
- `spinUnlock(pLock, irqState)`: Release a spin lock and restore the saved interrupt state.

## Mutex

- `MUTEX_DEFINE(name)`: Macro to statically define and initialize a mutex.
- `mutexLock(pMutex, waitTicks)`: Acquire a mutex, blocking up to `waitTicks` if necessary. This API must not be called from ISR context.
- `mutexUnlock(pMutex)`: Release a mutex. This API must not be called from ISR context.

## Semaphore

- `SEMAPHORE_DEFINE(name, initialCount, maxCount)`: Macro to statically define and initialize a semaphore.
- `semaphoreTake(pSem, waitTicks)`: Take the semaphore, optionally waiting up to `waitTicks` if unavailable.
- `semaphoreGive(pSem)`: Release a semaphore.

## Message Queue

- `MSG_QUEUE_DEFINE(name, length, itemSize)`: Macro to statically define and initialize a message queue.
- `msgQueueSend(pQueueHandle, pItem, waitTicks)`: Send a message to a queue, optionally waiting up to `waitTicks` for free space.
- `msgQueueReceive(pQueueHandle, pItem, waitTicks)`: Receive a message from a queue, optionally waiting up to `waitTicks` for data.

## Mailbox

- `MAILBOX_DEFINE(name)`: Macro to statically define and initialize a mailbox.
- `mailboxSend(pMailbox, pMsg, waitTicks)`: Send a mailbox message descriptor and wait for a compatible receiver.
- `mailboxReceive(pMailbox, pMsg, pBuffer, waitTicks)`: Receive a mailbox message into `pBuffer`, optionally waiting for a compatible sender.

## Condition Variable

- `CONDVAR_DEFINE(name, pMutex)`: Macro to statically define and initialize a condition variable.
- `condVarWait(pCondVar, waitTicks)`: Wait on a condition variable for up to `waitTicks`. This API must not be called from ISR context.
- `condVarSignal(pCondVar)`: Signal a condition variable and wake one waiting task. This API must not be called from ISR context.
- `condVarBroadcast(pCondVar)`: Broadcast a condition variable and wake all waiting tasks. This API must not be called from ISR context.

## Event Object

- `EVENT_DEFINE(name)`: Macro to statically define and initialize an event object.
- `eventSet(pEvent, events)`: Set one or more event bits and wake matching waiters.
- `eventClear(pEvent, events)`: Clear one or more event bits.
- `eventGet(pEvent, pEvents)`: Read the current event bits.
- `eventWaitAny(pEvent, events, clearOnExit, pMatchedEvents, waitTicks)`: Wait until any bit in `events` becomes set.
- `eventWaitAll(pEvent, events, clearOnExit, pMatchedEvents, waitTicks)`: Wait until all bits in `events` become set.
- `eventSync(pEvent, setEvents, waitEvents, pMatchedEvents, waitTicks)`: Atomically set `setEvents`, then wait until all bits in `waitEvents` are set.

## Software Timer

- `TIMER_DEFINE(name, timeoutHandler, timerMode)`: Macro to statically define and initialize a timer.
- `timerStart(pTimerNode, interval)`: Start a timer with a specified timeout in RTOS ticks.
- `timerStop(pTimerNode)`: Stop a running timer.

# Building and Running
## Example for STM32Cube IDE

1. Clone the Repository:
   - Open a terminal and clone the sanoRTOS repository
     `git clone https://github.com/pdlsurya/sanoRTOS.git`
2. Open STM32Cube IDE:

   - Launch STM32Cube IDE and create a new STM32 project or open an existing one.

3. Include sanoRTOS in Project:
   - Right-click on your project and select **Properties**.
   - Go to **C/C++ Build > Settings**.
   - Under **Tool Settings**, go to **MCU GCC Compiler > Include paths** and add the path to **sanoRTOS/include** and **sanoRTOS/ports/arm/stm32/include** directory.

4. Add Source Files:
   - Navigate to **C/C++ General > Paths and Symbols**.
   - In the **Source Location** tab, click on **Link folder** and add the path to **sanoRTOS/source** and **sanoRTOS/ports/arm/stm32** directory by selecting **Link to folder in the filesystem**.
     
6. Edit **stm32xxxx_it.c** file:
   - STM32 initializes the SysTick timer during its clock initialization process and defines the `SysTick_Handler` ISR function for the implementation of the delay function in the 
   **Core > Src > stm32xxxx_it.c** file. Hence, the `SysTick_Handler` ISR function cannot be redefined inside the sanoRTOS. Instead, we need to call the function `osSysTick_Handler` from the `SysTick_Handler` ISR function.
    
   - Moreover, **sanoRTOS** includes definition for `PendSV_Handler` and `SVC_Handler` ISRs used for task scheduling and context switching. STM32 also 
   defines these ISRs in **stm32xxxx_it.c** file; Hence, we need to remove the definition of these ISRs from the **stm32xxxx_it.c** file to avoid multiple definition error. 

## Example for RP2350(Raspberry Pi pico 2) using pico-sdk
1. Clone the Repository:
   - Open a terminal and clone the sanoRTOS repository
     `git clone https://github.com/pdlsurya/sanoRTOS.git`

2. Set Up Your Pico Project:
  - You can either create a new CMake-based Pico SDK project or add sanoRTOS to an existing one. Add sanoRTOS folder to your project folder
  - sanoRTOS supports both ARM and RISC-V cores of the RP2350.

3. Include sanoRTOS in Your CMakeLists.txt:
  - Update your CMakeLists.txt to include sanoRTOS headers and sources as follows:

```cmake

file(GLOB_RECURSE SANORTOS_SRCS
    sanoRTOS/source/*.c
    sanoRTOS/ports/arm/rp2350/port.c)
#for Hazard3 RISC-V cores, add source files from risc-v port of rp2350 (sanoRTOS/ports/riscv/rp2350/port.c and  sanoRTOS/ports/riscv/rp2350/port.S)

target_include_directories(project_name PRIVATE
   sanoRTOS/include
   sanoRTOS/ports/arm/rp2350/include #for RISC-V port replace this line  with sanoRTOS/ports/risc-v/rp2350/include 
) 
target_sources(project_name PRIVATE ${SANORTOS_SRCS})

```

## Example Code:
 
 ```c

#include "sanoRTOS/config.h"
#include "sanoRTOS/scheduler.h"
#include "sanoRTOS/task.h"
//Include MCU-specific header files here

// Define two tasks with 1024-byte stacks.
// These tasks are assigned to any available core using AFFINITY_CORE_ANY.
//
// On multicore MCUs (e.g. RP2350), sanoRTOS can schedule tasks on different cores
// depending on the affinity setting. AFFINITY_CORE_ANY allows the scheduler to choose any core.
// This can help distribute load across both cores and enable true parallel execution.
//
// On single-core MCUs, this setting is safely ignored — all tasks will run on the only available core.
TASK_DEFINE(task1, 1024, firstTask, NULL, 1, AFFINITY_CORE_ANY);
TASK_DEFINE(task2, 1024, secondTask, NULL, 1, AFFINITY_CORE_ANY);
taskHandleType *dynamicTask = NULL;


// Task 1 function – user-defined logic inside this loop
void firstTask(void *args) {
    while (1) {
        // Task1 code to run repeatedly
    }
}

// Task 2 function – user-defined logic inside this loop
void secondTask(void *args) {
    while (1) {
        // Task2 code to run repeatedly
    }
}

// Dynamically created task function
void thirdTask(void *args) {
    while (1) {
        // Dynamic task code to run repeatedly
    }
}

int main() {
    // Perform MCU-specific initializations here
    // Example: initialize GPIO, UART, peripherals, etc.

    // Start tasks
    taskStart(&task1);
    taskStart(&task2);

    // Create and start a dynamic task
    int ret = taskCreate(&dynamicTask,
                         "dynamicTask",
                         1024,
                         thirdTask,
                         NULL,
                         1,
                         AFFINITY_CORE_ANY);
    if (ret != RET_SUCCESS) {
        // Handle task creation failure
        while (1) {
        }
    }

    // Start the sanoRTOS scheduler (will not return)
    schedulerStart();

    // Control should never reach here if scheduler is working correctly
    return 0;
}
```

## ESP32 Ports

This RTOS includes ports for the following Espressif RISC-V based MCUs:

- **ESP32-C6**
- **ESP32-P4**

Unlike most ESP32 projects, these ports **do not use Espressif's ESP-IDF**.  
Instead, they are built on top of custom [ESP32 RISC-V bare-metal SDK](https://github.com/pdlsurya/esp32-riscv-bare-metal-sdk) developed specifically for low-level control and minimal system overhead.

The bare-metal SDK provides:

- Startup code and linker scripts
- Register definitions and peripheral structures
- Interrupt and exception handling
- Basic hardware initialization
- Minimal HAL required for RTOS operation

This SDK allows the RTOS to run **without the ESP-IDF framework**, making the
implementation closer to traditional embedded RTOS ports.

### Notes

- The RTOS ports assume the system is initialized using the
  corresponding bare-metal SDK.
- ESP-IDF is **not required** to build or run these ports.

# License
This project is licensed under the MIT License-see the [LICENSE](LICENSE) file for details.
