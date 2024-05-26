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
#define RET_NOSEM -11 /*No semaphore available to signal*/
#define RET_NOTLOCKED -12 /*Mutex not locked*/

#ifdef __cplusplus
}
#endif

#endif
