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

#define RET_SUCCESS 0        /*Success*/
#define RET_INVAL -1         /*Invalid argument or Invalid Operation*/
#define RET_TIMEOUT -2       /*Wait Timeout*/
#define RET_EMPTY -3         /*Empty List/Queue*/
#define RET_FULL -4          /*FUll List/Queuee*/
#define RET_NOTASK -5        /*No task available*/
#define RET_BUSY -6          /*Resource busy*/
#define RET_NOTOWNER -7      /*Not owner*/
#define RET_NOTACTIVE -8     /*Timer/Task not running*/
#define RET_ALREADYACTIVE -9 /*Timer/Task already running*/
#define RET_NOTSUSPENDED -10 /*Task is not suspended*/
#define RET_NOSEM -11        /*No semaphore available to signal*/
#define RET_NOTLOCKED -12    /*Mutex not locked*/
#define RET_NOMEM -13        /*Memory could not be allocated*/

#ifdef __cplusplus
}
#endif

#endif
