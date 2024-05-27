/**
 * @file timer.h
 * @author Surya Poudel
 * @brief RTOS Software Timer implementation
 * @version 0.1
 * @date 2024-04-26
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef __SANO_RTOS_TIMER_H
#define __SANO_RTOS_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "osConfig.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        TIMER_MODE_SINGLE_SHOT,
        TIMER_MODE_PERIODIC
    } timerModeType;

/**
 * @brief Statically define and initialize a timer.
 * @param name Name of the timer.
 * @param timeout_handler Function to execute on timer timeout.
 * @param timer_mode Timer mode[PERIODIC or SINGLE_SHOT].
 *
 */
#define TIMER_DEFINE(name, timeout_handler, timer_mode) \
    void timeout_handler(void);                         \
    timerNodeType name = {                              \
        .isRunning = false,                             \
        .mode = timer_mode,                             \
        .timeoutHandler = timeout_handler,              \
        .ticksToExpire = 0,                             \
        .intervalTicks = 0,                             \
        .nextNode = NULL}

    typedef void (*timeoutHandlerType)(void); // Timeout handler function type definition

    /*Timer node structure*/
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

    int timerStart(timerNodeType *pTimerNode, uint32_t interval);

    int timerStop(timerNodeType *pTimerNode);

    void processTimers();

    void timerTaskStart();

#ifdef __cplusplus
}
#endif

#endif