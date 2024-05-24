#ifndef __SANO_RTOS_RET_CODES_H
#define __SANO_RTOS_RET_CODES_H

#ifdef __cplusplus
extern "C"
{
#endif

#define SUCCESS 0        /*Success*/
#define EINVAL 1         /*Invalid argument or Invalid Operation*/
#define ETIMEOUT 2       /*Wait Timeout*/
#define ENODATA 3        /*No data available*/
#define ENOSPACE 4       /*No space available*/
#define ENOTASK 5        /*No task available*/
#define EBUSY 6          /*Resource busy*/
#define ENOTOWNER 7      /*Not owner*/
#define ENOTACTIVE 8     /*Timer/Task not running*/
#define EALREADYACTIVE 9 /*Timer/Task already running*/
#define EEMPTY 10        /*Empty list/queue*/

#ifdef __cplusplus
}
#endif

#endif
