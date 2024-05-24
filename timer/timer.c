/**
 * @file timer.c
 * @author Surya Poudel
 * @brief RTOS Software Timer implementation
 * @version 0.1
 * @date 2024-04-26
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "retCodes.h"
#include "scheduler/scheduler.h"
#include "task/task.h"
#include "timer.h"

#define TIMER_TASK_PRIORITY TASK_HIGHEST_PRIORITY // timer task has the highest possible priority [lower the value, higher the priority]

static timerListType timerList = {0}; // List of running timers

static timeoutHandlerQueueType timeoutHandlerQueue = {0}; // Queue of timeout handlers to be executed

/*Define timer task with highest possible priority*/
TASK_DEFINE(timerTask, 256, timerTaskFunction, NULL, TIMER_TASK_PRIORITY);

/**
 * @brief Insert timeoutHandler node at the end of the Queue
 *
 * @param pTimeoutHandlerQueue Pointer to the timeoutHandlerQueue struct
 * @param timeoutHandler timeoutHandler function
 */
static void timeoutHandlerQueuePush(timeoutHandlerQueueType *pTimeoutHandlerQueue, timeoutHandlerType timeoutHandler)
{
    timeoutHandlerNodeType *newNode = (timeoutHandlerNodeType *)malloc(sizeof(timeoutHandlerNodeType));
    newNode->timeoutHandler = timeoutHandler;
    newNode->nextNode = NULL;

    if (pTimeoutHandlerQueue->head == NULL)
    {
        pTimeoutHandlerQueue->head = newNode;
        pTimeoutHandlerQueue->tail = pTimeoutHandlerQueue->head;
    }
    else
    {
        pTimeoutHandlerQueue->tail->nextNode = newNode;
        pTimeoutHandlerQueue->tail = newNode;
    }
}

/**
 * @brief Get timeoutHandler node from the front of the Queue
 *
 * @param pTimeoutHandlerQueue
 * @retval timeoutHandler function
 */
static timeoutHandlerType timeoutHandlerQueuePop(timeoutHandlerQueueType *pTimeoutHandlerQueue)
{

    timeoutHandlerNodeType *temp = pTimeoutHandlerQueue->head->nextNode;

    timeoutHandlerType timeoutHandler = pTimeoutHandlerQueue->head->timeoutHandler;

    free(pTimeoutHandlerQueue->head);

    pTimeoutHandlerQueue->head = temp;

    return timeoutHandler;
}

/**
 * @brief Add a timer node to the list of running timers
 *
 * @param pTimerList  Pointer to the timer list struct
 * @param pTimerNode  Pointer to the timerNode struct
 * @retval SUCCESS if timerNode added to the list successfully
 * @retval -EINVAL if invalid arguments passed
 */
static int timerListNodeAdd(timerListType *pTimerList, timerNodeType *pTimerNode)
{
    if (pTimerList != NULL && pTimerNode != NULL)
    {
        pTimerNode->nextNode = pTimerList->head;

        pTimerList->head = pTimerNode;

        return SUCCESS;
    }

    return -EINVAL;
}

/**
 * @brief Delete the timerNode from the beginning of the list
 *
 * @param pTimerList Pointer to the timerNode struct
 */
static inline void timerListDeleteFirstNode(timerListType *pTimerList)
{
    timerNodeType *tempNode = pTimerList->head->nextNode;

    pTimerList->head->nextNode = NULL;

    pTimerList->head = tempNode;
}

/**
 * @brief Delete a specified timerNode from the list of running timers.
 *
 * @param pTimerList Pointer to the timerList struct.
 * @param pTimerNode Pointer to the timerNode struct.
 * @retval SUCCESS if timerNode deleted successfully
 * @retval -EEMPTY if timerList is empty
 * @retval -EINVAL if invalid argument passed
 */
static int timerListNodeDelete(timerListType *pTimerList, timerNodeType *pTimerNode)
{
    if (pTimerList != NULL && pTimerNode != NULL)
    {

        if (pTimerList->head != NULL)
        {
            timerNodeType *currentNode = pTimerList->head;

            /* If the timer correponds to the head node in the list, remove the timer node and reassign head node.*/
            if (pTimerNode == pTimerList->head)
                timerListDeleteFirstNode(pTimerList);

            else
            {
                while (currentNode->nextNode != pTimerNode)
                    currentNode = currentNode->nextNode;

                currentNode->nextNode = pTimerNode->nextNode;
                pTimerNode->nextNode = NULL;
            }
            return SUCCESS;
        }
        return -EEMPTY;
    }
    return -EINVAL;
}

/**
 * @brief  Function to start the timer. This stores the timerNode in the list of running timers.
 *
 * @param pTimerNode Pointer timerNode struct
 * @param intervalTicks Timer intervalTicks
 * @retval SUCCESS if timer started successfully
 * @retval -EALREADYACTIVE if timer is already running
 * @retval -EINVAL if invalid arguments passed
 */
int timerStart(timerNodeType *pTimerNode, uint32_t intervalTicks)
{
    if (pTimerNode != NULL)
    {
        /* check if the timer is already in running state. If so, abort re-starting the timer.*/
        if (pTimerNode->isRunning)
            return -EALREADYACTIVE;

        /* Set isRunning flag for the started timer pTimerNode.*/
        pTimerNode->isRunning = true;

        pTimerNode->ticksToExpire = pTimerNode->intervalTicks = intervalTicks;

        /* Add the timer in the queue of running timers*/
        return timerListNodeAdd(&timerList, pTimerNode);
    }
    return -EINVAL;
}

/**
 * @brief Function to stop the specified timerNode. This sets isRunning flag to false to prevent subsequent events from this timer
 *  and delete timer from the list of running timers
 *
 * @param pTimerNode Pointer to timerNode struct
 * @retval SUCCESS if timer stopped successfully
 * @retval -ENOTACTIVE if timer is not running
 * @retval -EINVAL if invalid argument passed
 */
int timerStop(timerNodeType *pTimerNode)
{
    if (pTimerNode)
    {
        if (pTimerNode->isRunning)
        {
            pTimerNode->isRunning = false;
            return timerListNodeDelete(&timerList, pTimerNode);
        }
        return -ENOTACTIVE;
    }
    return -EINVAL;
}

/**
 * @brief Check for timer timeout and add the corresponding timeout handler to the
 *  Queue of timeout handlers
 */
void processTimers()
{
    if (timerList.head != NULL)
    {
        timerNodeType *currentNode = timerList.head;
        while (currentNode != NULL)
        {
            /*Save next node to avoid losing track of linked list after a time node is deleted while stopping the timer*/
            timerNodeType *nextNode = currentNode->nextNode;

            /*Decrement ticks to expire*/
            if (currentNode->ticksToExpire > 0)
                currentNode->ticksToExpire--;

            /* Check if timer has expired.If set, call the timeoutHandler() and update the ticksToExpire for next event.*/
            if (currentNode->ticksToExpire == 0)
            {

                /*Add timeout handler to the timeoutHandlerQueue*/
                timeoutHandlerQueuePush(&timeoutHandlerQueue, currentNode->timeoutHandler);

                /* Check if timer task is suspended. If so, change status to ready to allow execution.*/
                if (timerTask.status == TASK_STATUS_BLOCKED)
                    taskSetReady(&timerTask, TIMER_TIMEOUT);

                currentNode->ticksToExpire = currentNode->intervalTicks;

                /* Check if the timer mode is SINGLE_SHOT. If true, stop the correponding timer*/
                if (currentNode->mode == TIMER_MODE_SINGLE_SHOT)
                    timerStop(currentNode);
            }
            currentNode = nextNode;
        }
    }
}

/**
 * @brief Function to Start timerTask. This function will be called when starting the scheduler.
 * @retval SUCCESS if timerTask started successfully
 * @retval -EINVAL if invalid argument passed
 */
int timerTaskStart()
{

    return taskStart(&timerTask);
}

/**
 * @brief RTOS timer task function
 *
 * @param args
 */
void timerTaskFunction(void *args)
{
    (void)args;

    while (1)
    {
        if (timeoutHandlerQueue.head != NULL) // Check for non-empty condition
        {
            timeoutHandlerType timeoutHandler = timeoutHandlerQueuePop(&timeoutHandlerQueue);
            timeoutHandler();
        }
        else
        {
            /* Block timer task and give cpu to other tasks while waiting for timeout*/
            taskBlock(&timerTask, WAIT_FOR_TIMER_TIMEOUT, 0);
        }
    }
}
