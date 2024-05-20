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
#define TIMER_DEFINE(name, timeout_handler, timer_mode) \
    void timeout_handler(void);                         \
    timerNodeType name = {                              \
        .isRunning = false,                             \
        .mode = timer_mode,                             \
        .timeoutHandler = timeout_handler,              \
        .ticksToExpire = 0,                             \
        .intervalTicks = 0,                             \
        .nextNode = NULL}

typedef void (*timeoutHandlerType)(void); // Timeout handler function

typedef struct timerNode
{
    timeoutHandlerType timeoutHandler;
    uint32_t intervalTicks;
    uint32_t ticksToExpire;
    struct timerNode *nextNode;
    timerModeType mode;
    bool isRunning;

} timerNodeType;

typedef struct timeoutHandlerNode
{
    timeoutHandlerType timeoutHandler;
    struct timeoutHandlerNode *nextNode;

} timeoutHandlerNodeType;

typedef struct
{
    timeoutHandlerNodeType *head;
    timeoutHandlerNodeType *tail;
} timeoutHandlerQueueType;

typedef struct
{
    timerNodeType *head;
} timerListType;

void timerStart(timerNodeType *pTimerNode, uint32_t interval);

void timerStop(timerNodeType *pTimerNode);

void processTimers();

void timerTaskStart();

#endif