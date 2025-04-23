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

#ifndef __SANO_RTOS_RET_CODES_H
#define __SANO_RTOS_RET_CODES_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Return codes for various operations and error states.
 */
#define RET_SUCCESS 0 ///< Success: The operation completed successfully.

#define RET_INVAL -1 ///< Invalid argument or operation: The provided argument is invalid or the operation is unsupported.

#define RET_TIMEOUT -2 ///< Wait Timeout: The operation timed out while waiting for a resource or event.

#define RET_EMPTY -3 ///< Empty List/Queue: The operation failed because the list or queue is empty.

#define RET_FULL -4 ///< Full List/Queue: The operation failed because the list or queue is full.

#define RET_NOTASK -5 ///< No task available: No task was found or available to perform the operation.

#define RET_BUSY -6 ///< Resource busy: The resource is currently in use and cannot be accessed.

#define RET_NOTOWNER -7 ///< Not owner: The task or thread is not the owner of the resource or mutex.

#define RET_NOTACTIVE -8 ///< Timer/Task not running: The timer or task is not active or has been stopped.

#define RET_ALREADYACTIVE -9 ///< Timer/Task already running: The timer or task is already active and cannot be restarted.

#define RET_NOTSUSPENDED -10 ///< Task is not suspended: The task is not currently in a suspended state.

#define RET_NOSEM -11 ///< No semaphore available to signal: No semaphore available to perform the signal operation.

#define RET_NOTLOCKED -12 ///< Mutex not locked: The mutex was not previously locked by the calling task.

#define RET_NOMEM -13 ///< Memory could not be allocated: The requested memory allocation failed due to insufficient memory.

#ifdef __cplusplus
}
#endif

#endif
