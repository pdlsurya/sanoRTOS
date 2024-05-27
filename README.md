# sanoRTOS
sanoRTOS is a minimal Real-Time Operating System (RTOS) designed for ARM Cortex-M microcontrollers. This implementation provides a simple yet effective API for task management, synchronization, and communication, enabling efficient and predictable multitasking in embedded systems.

# Features

- Priority based preemptive scheduling
- Optional priority inheritance to avoid priority inversion problem while using mutexes.
- Task synchronization
- Task communication
- Lightweight and minimalistic design

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
   - In the **Source Location** tab, click on **Link folder** and add the path to **sanoRTOS** directory by selecting **Link to folder in the filesystem**.
     
5. Specify STM32 platform:
   - Open **osConfig.h** file and define the macro **PLATFORM_STM32**

6. Edit **stm32xxxx_it.c** file:
   - STM32 initializes the SysTick timer during its clock initialization process and defines the `SysTick_Handler` ISR function for the implementation of the delay function in the 
   **Core > Src > stm32xxxx_it.c** file. Hence, the `SysTick_Handler` ISR function cannot be redefined inside the sanoRTOS. Instead, we need to call the function `osSysTick_Handler` from the `SysTick_Handler` ISR function.
    
   - Moreover, **sanoRTOS** includes definition for `PendSV_Handler` and `SVC_Handler` ISRs used for task scheduling and context switching. STM32 also 
   defines these ISRs in **stm32xxxx_it.c** file; Hence, we need to remove the definition of these ISRs from the **stm32xxxx_it.c** file to avoid multiple definition error. 

 
7. Example Code:
    ```c
   #include "main.h"
   #include "osConfig.h"
   #include "scheduler/scheduler.h"
   #include "task/task.h"

   TASK_DEFINE(task1, 128, firstTask, NULL, 1);
   TASK_DEFINE(task2, 128, secondTask, NULL, 1);

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


