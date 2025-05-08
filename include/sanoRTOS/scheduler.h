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

#ifndef __SANO_RTOS_SCHEDULER_H
#define __SANO_RTOS_SCHEDULER_H

#include "sanoRTOS/config.h"
#include "sanoRTOS/port.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Convert microseconds to timer ticks.
 *
 * This macro converts a time duration in microseconds to the equivalent number of hardware timer ticks,
 * based on the timer's tick frequency.
 *
 * @param us Duration in microseconds.
 * @return Equivalent number of timer ticks.
 */
#define US_TO_TIMER_TICKS(us) ((uint32_t)((uint64_t)(us) * PORT_TIMER_TICK_FREQ / 1000000))

/**
 * @brief Number of hardware timer ticks in one RTOS tick.
 *
 * This macro defines how many low-level timer ticks correspond to one RTOS scheduler tick,
 * based on the configured RTOS tick interval in microseconds.
 */
#define TIMER_TICKS_PER_RTOS_TICK US_TO_TIMER_TICKS(CONFIG_TICK_INTERVAL_US)

/**
 * @brief Convert microseconds to RTOS ticks.
 *
 * This macro converts a time duration in microseconds to the number of RTOS ticks,
 * using the configured tick interval.
 *
 * @param us Duration in microseconds.
 * @return Equivalent number of RTOS ticks.
 */
#define US_TO_RTOS_TICKS(us) ((us) / CONFIG_TICK_INTERVAL_US)

/**
 * @brief Convert milliseconds to RTOS ticks.
 *
 * This macro converts a time duration in milliseconds to the number of RTOS ticks,
 * using the configured tick interval.
 *
 * @param ms Duration in milliseconds.
 * @return Equivalent number of RTOS ticks.
 */
#define MS_TO_RTOS_TICKS(ms) (((ms) * 1000) / CONFIG_TICK_INTERVAL_US)

    void schedulerStart();

    void taskYield();

#ifdef __cplusplus
}
#endif

#endif
