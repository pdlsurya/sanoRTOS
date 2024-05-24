#ifndef __SANO_RTOS_RET_CODES_H
#define __SANO_RTOS_RET_CODES_H

#ifdef __cplusplus
extern "C"
{
#endif

#define SUCCESS 0  /*Success*/
#define EINVAL 1   /*Invalid argument or Invalid Operation*/
#define ETIMEOUT 2 /*Wait Timeout*/
#define ENODATA 3  /*No data available*/
#define ENOSPACE 4 /*No space available*/
#define ENOTASK 5  /*No task available*/
#define EBUSY 6    /*Resource busy*/
#define ENOTOWNER 7 /*Not owner*/

#ifdef __cplusplus
}
#endif

#endif
