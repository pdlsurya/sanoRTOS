# sanoRTOS
sanoRTOS is a minimal Real-Time Operating System (RTOS) designed for ARM Cortex-M and RISC-V microcontrollers. This implementation provides a simple yet effective API for task management, synchronization, and communication, enabling efficient and predictable multitasking in embedded systems.

## 🚀 Features

- **Priority-Based Preemptive Scheduling**  
  Efficient task management with support for preemptive scheduling based on task priority levels.

- **Optional Priority Inheritance**  
  Prevents priority inversion during mutex acquisition by temporarily elevating the priority of lower-priority tasks.

- **Symmetric Multiprocessing (SMP) Support**  
  Fully supports multi-core systems with per-task **core affinity** configuration for optimal load balancing.

- **Dynamic Stack Overflow Detection**  
  Runtime monitoring of task stacks to catch and handle stack overflows proactively.

- **Configurable Tick Rate**  
  Easily adjustable tick frequency to match application-specific timing and power requirements.

- **Task Synchronization Primitives**  
  Includes mutexes, semaphores, and condition variables for safe and efficient coordination between tasks.

- **Inter-Task Communication**  
  Enables message passing between tasks using message queue.

- **Minimalistic and Lightweight Design**  
  Designed for embedded systems with limited resources — small footprint, fast context switches, and no unnecessary bloat.


# API Functions


## Task Management

- **TASK_DEFINE**: Macro to statically define and initialize a task.
- **taskStart** : Start the task.
- **taskYield**: Yield the processor to allow other tasks to run.
- **taskSleepMS**: Delay a task for a specified number of milliseconds.
- **taskSleepUS**: Delay a task for a specified number of microseconds.
- **schedulerStart**: Start the RTOS scheduler.


## Mutex

- **MUTEX_DEFINE**: Macro to statically define and initialize a mutex.
- **mutexLock**: Acquire a mutex, blocking if necessary.
- **mutexUnlock**: Release a mutex.

## Semaphore

- **SEMAPHORE_DEFINE**: Macro to statically define and initialize a semaphore.
- **semaphoreTake**: Take the semaphore.
- **semaphoreGive**: Release a semaphore.

## Message Queue

- **MSG_QUEUE_DEFINE**: Macro to statically define and initialize a message queue.
- **msgQueueSend**: Send a message to a queue.
- **msgQueueReceive**: Receive a message from a queue.

## Condition Variable

- **CONDVAR_DEFINE**: Macro to statically define and initialize a conditon variable.
- **condVarWait**: Wait on a condition variable.
- **condVarSignal**: Signal a condition variable, waking one waiting task.
- **condVarBroadcast**: Broadcast a condition variable, waking all waiting tasks.

## Software Timer

- **TIMER_DEFINE**: Macro to statically define and initialize a timer
- **timerStart**: Start a timer with a specified timeout.
- **timerStop**: Stop a running timer.

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
   - Under **Tool Settings**, go to **MCU GCC Compiler > Include paths** and add the path to **sanoRTOS** directory.

4. Add Source Files:
   - Navigate to **C/C++ General > Paths and Symbols**.
   - In the **Source Location** tab, click on **Link folder** and add the path to **sanoRTOS/source** and **sanoRTOS/ports/arm/stm32** directory by selecting **Link to folder in the filesystem**.
     
6. Edit **stm32xxxx_it.c** file:
   - STM32 initializes the SysTick timer during its clock initialization process and defines the `SysTick_Handler` ISR function for the implementation of the delay function in the 
   **Core > Src > stm32xxxx_it.c** file. Hence, the `SysTick_Handler` ISR function cannot be redefined inside the sanoRTOS. Instead, we need to call the function `osSysTick_Handler` from the `SysTick_Handler` ISR function.
    
   - Moreover, **sanoRTOS** includes definition for `PendSV_Handler` and `SVC_Handler` ISRs used for task scheduling and context switching. STM32 also 
   defines these ISRs in **stm32xxxx_it.c** file; Hence, we need to remove the definition of these ISRs from the **stm32xxxx_it.c** file to avoid multiple definition error. 

 
7. Example Code:
    ```c
   #include "main.h"
   #include "sanoRTOS/config.h"
   #include "sanoRTOS/scheduler.h"
   #include "sanoRTOS/task.h"
 
   TASK_DEFINE(task1, 512, firstTask, NULL, 1, AFFINITY_CORE_ANY);
   TASK_DEFINE(task2, 512, secondTask, NULL, 1, AFFINITY_CORE_ANY);

    void firstTask(void *args){

        while(1){
             //Task1 code to run repeatedly

              }
    }

    void secondTask(void * args){

       while(1){
           //Task2 code to run repeatedly

              }
     
    }

    int main(){

      //STM32 Clock and peripheral initialization ...


       //Start  tasks
       taskStart(&task1);
       taskStart(&task2);

       //Start the RTOS scheduler
       schedulerStart();

       //Control should never reach here
     
       return 0;
    }
    
    ```

# License
This project is licensed under the MIT License-see the [LICENSE](LICENSE) file for details.


