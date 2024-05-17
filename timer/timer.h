/**
 * @file timer.h
 * @author Surya Poudel
 * @brief Software timer implementation
 * @version 0.1
 * @date 2024-04-26
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include "osConfig.h"

#ifndef __SANO_RTOS_TIMER_H
#define __SANO_RTOS_TIMER_H

typedef enum
{
    TIMER_MODE_SINGLE_SHOT,
    TIMER_MODE_PERIODIC
} timerModeType;

/* Macro to define  timer node struct */
#define TIMER_DEF(name) timerNodeType name

#define MAX_TIMERS_CNT 16 // Maximum number of timer nodes allowed

#define MAX_HANDLERS_QUEUE_SIZE 16

typedef void (*timeoutHandlerType)(void); // Timeout handler function

typedef struct timerNode
{
    bool isRunning;
    timerModeType mode;
    timeoutHandlerType timeoutHandler;
    uint32_t intervalTicks;
    uint32_t ticksToExpire;
    struct timerNode *nextNode;
} timerNodeType; // Timer node structure

typedef struct
{
    timeoutHandlerType handlersBuffer[MAX_HANDLERS_QUEUE_SIZE];
    uint8_t handlersCount;
    uint8_t readIndex;
    uint8_t writeIndex;
} timeoutHandlersQueueType;

// Function to create a timer by initializing the timer node instance.
void timerCreate(timerNodeType *pTimerNode,
                 timeoutHandlerType timeout_handler, timerModeType mode);

// Function to start the timer  by adding it to the list of running timers.
void timerStart(timerNodeType *pTimerNode, uint32_t interval);

// Function to stop the timer
void timerStop(timerNodeType *pTimerNode);

void processTimers();

void timerTaskStart();

#endif