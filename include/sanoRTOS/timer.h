/*
 * MIT License
 *
 * Copyright (c) 2024 Surya Poudel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __SANO_RTOS_TIMER_H
#define __SANO_RTOS_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "sanoRTOS/config.h"
#include "sanoRTOS/scheduler.h"

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