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

#ifndef __SANO_RTOS_CONFIG_H
#define __SANO_RTOS_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif
    /**
     * @brief Configuration macros for system settings and features.
     */

#define CONFIG_LOG 1 ///< Configure logging: Enables or disables logging in the system.

#if CONFIG_LOG
#define CONFIG_LOG_LEVEL 4 ///< Configure log level: Sets the level of logging to be enabled (0 = None, 1 = Error, 2 = Warning, 3 = Info, 4 = Debug).
#define CONFIG_LOG_COLOR 1 ///< Configure log color: Enables or disables colored output in logs when logging is enabled.
#endif

#define CONFIG_SMP 1///< Configure Symmetric Multiprocessing (SMP): Enables or disables SMP support (0 = Disabled, 1 = Enabled).

#define CONFIG_MUTEX_PRIORITY_INHERITANCE 1 ///< Configure mutex priority inheritance: Enables priority inheritance for mutexes to avoid priority inversion.

#define CONFIG_STACK_OVERFLOW_CHECK 0 ///< Configure stack overflow check

#define CONFIG_TICK_INTERVAL_US 1000 ///< Configure Tick interval to generate interrupt every TICK_INTERVAL_US (in microseconds): Sets the interrupt frequency for the system tick.

#define CONFIG_READY_QUEUE_PRIORITY_MULTIQ 1 ///< Select ready-queue backend: 1 = bitmap-backed per-priority queues, 0 = single priority-sorted queue.

#define CONFIG_TASK_PRIORITY_LEVELS 16 ///< Number of scheduler priority levels used by the bitmap-backed ready queue.

#define CONFIG_TIMER_TIMEOUT_NODE_SLAB_BLOCKS 32 ///< Number of slab blocks for timeout-handler nodes.

#define CONFIG_DYNAMIC_TASK_TCB_SLAB_BLOCKS 16 ///< Number of slab blocks for dynamic task control blocks.

#define CONFIG_DYNAMIC_SEMAPHORE_SLAB_BLOCKS 8 ///< Number of slab blocks for dynamic semaphores.

#define CONFIG_DYNAMIC_MUTEX_SLAB_BLOCKS 8 ///< Number of slab blocks for dynamic mutexes.

#define CONFIG_DYNAMIC_EVENT_SLAB_BLOCKS 8 ///< Number of slab blocks for dynamic events.

#define CONFIG_DYNAMIC_COND_VAR_SLAB_BLOCKS 8 ///< Number of slab blocks for dynamic condition variables.

#define CONFIG_DYNAMIC_MAILBOX_SLAB_BLOCKS 8 ///< Number of slab blocks for dynamic mailboxes.

#define CONFIG_DYNAMIC_MSG_QUEUE_SLAB_BLOCKS 8 ///< Number of slab blocks for dynamic message queues.

#define CONFIG_DYNAMIC_STREAM_BUFFER_SLAB_BLOCKS 8 ///< Number of slab blocks for dynamic stream buffers.

#define CONFIG_DYNAMIC_MSG_BUFFER_SLAB_BLOCKS 8 ///< Number of slab blocks for dynamic message buffers.

#define CONFIG_DYNAMIC_TIMER_SLAB_BLOCKS 8 ///< Number of slab blocks for dynamic timers.

#if (CONFIG_TASK_PRIORITY_LEVELS == 0) || (CONFIG_TASK_PRIORITY_LEVELS > 32)
#error "CONFIG_TASK_PRIORITY_LEVELS must be between 1 and 32"
#endif

#ifdef __cplusplus
}
#endif

#endif
