/**
 * @file timer.c
 * @author Surya Poudel
 * @brief Software timer implementation
 * @version 0.1
 * @date 2024-04-26
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "cmsis_gcc.h"
#include "math.h"
#include "scheduler/scheduler.h"
#include "task/task.h"
#include "timer.h"

#define TIMER_TASK_PRIORITY TASK_HIGHEST_PRIORITY // timer task has the highest possible priority [lower the value, higher the priority]

static uint8_t timersCount; // Number of running timers in the list

static timerNodeType *headNode = NULL; // head node in the list

static timeoutHandlersQueueType timeoutHandlersQueue;

/*Define timer task with highest possible priority*/
TASK_DEFINE(timerTask, 256, timerTaskFunction, NULL, TIMER_TASK_PRIORITY);

void timerTaskFunction(void *args)
{
    (void)args;

    while (1)
    {
        if (timeoutHandlersQueue.handlersCount > 0)
        {
            timeoutHandlerType timeoutHandler = timeoutHandlersQueue.handlersBuffer[timeoutHandlersQueue.readIndex];
            timeoutHandlersQueue.readIndex = (timeoutHandlersQueue.readIndex + 1) % MAX_HANDLERS_QUEUE_SIZE;
            timeoutHandlersQueue.handlersCount--;
            timeoutHandler();
        }
        else
        {
            // Block timer task and give cpu to other tasks while waiting for timeout
            taskBlock(&timerTask, WAIT_FOR_TIMER_TIMEOUT, 0);
        }
    }
}

/*Function to add an timer node instance in the queue of running timers.
 *This forms a linked list of running timers
 */
static void timerListNodeAdd(timerNodeType *pTimerNode)
{
    // if no timers in the list, assign the head node with first timer node.
    if (headNode == NULL)
    {
        headNode = pTimerNode;
        headNode->nextNode = NULL;
        timersCount++;
        return;
    }

    timerNodeType *currentNode = headNode;
    // traverse over the list until the last node
    while (currentNode->nextNode != NULL)
    {
        currentNode = currentNode->nextNode;
    }
    currentNode->nextNode = pTimerNode;
    currentNode->nextNode->nextNode = NULL;
    timersCount++;
}

/*Function to delete the timer node from the beginning of the list.
 * This removes the head node and reassigns head node with the next timer in the list.
 */
static inline void timerListDeleteFirstNode(timerNodeType **headNode)
{
    timerNodeType *temp_node = (*headNode)->nextNode;
    (*headNode)->nextNode = NULL;
    *headNode = temp_node;
}

/*Function to delete the timer from the list
 */
static inline void timerListNodeDelete(timerNodeType *pTimerNode)
{
    timerNodeType *currentNode = headNode;

    // if the timer correponds to the head node in the list, remove the timer node and reassign head node.
    if (pTimerNode == headNode && headNode->nextNode != NULL)
        timerListDeleteFirstNode(&headNode);

    // if there is only one timer node in the list, unlink the node by making head node NULL.
    else if (pTimerNode == headNode && headNode->nextNode == NULL)
        headNode = NULL;

    else
    {
        while (currentNode->nextNode != pTimerNode)
            currentNode = currentNode->nextNode;

        currentNode->nextNode = pTimerNode->nextNode;
        pTimerNode->nextNode = NULL;
    }
    timersCount--;
}

/*Function to create a timer by initizing the timer pTimerNode with corresponding timer parameters.
 * This does not start the timer. timerStart function should be called to start
 * the corresponding timer.
 */
void timerCreate(timerNodeType *pTimerNode, timeoutHandlerType timeoutHandler, timerModeType mode)
{
    pTimerNode->mode = mode;
    pTimerNode->isRunning = false;
    pTimerNode->timeoutHandler = timeoutHandler;
    pTimerNode->nextNode = NULL;
}

/**
 * @brief  Function to start the timer pTimerNodes. This stores the timer pTimerNodes in a running timer queue and loads CC register with next minimum RTC ticks.
 *
 * @param pTimerNode Pointer to the timer pTimerNode
 * @param intervalTicks Timer intervalTicks
 */
void timerStart(timerNodeType *pTimerNode, uint32_t intervalTicks)
{
    // check if the timer is already in running state. If so, abort re-starting the timer.
    if (pTimerNode->isRunning)
        return;

    // Check for maximum number of allowed pTimerNode to run and generate an error accordingly.
    if (timersCount == MAX_TIMERS_CNT)
        return;

    // Set isRunning flag for the started timer pTimerNode.
    pTimerNode->isRunning = true;

    pTimerNode->ticksToExpire = pTimerNode->intervalTicks = intervalTicks;

    // add the timer in the queue of running timers
    timerListNodeAdd(pTimerNode);
}

/* Function to stop the corresponding timer pTimerNode.
 * This sets isRunning flag to false to prevent subsequent events from this timer.
 */
void timerStop(timerNodeType *pTimerNode)
{
    if (pTimerNode->isRunning)
    {
        pTimerNode->isRunning = false;
        timerListNodeDelete(pTimerNode);
    }
}

/*Function to check for timers timeout and trigger correspondig timeoutHandler
 */
void processTimers()
{
    if (timersCount > 0)
    {
        timerNodeType *currentNode = headNode;
        while (currentNode != NULL)
        {
            // Save next node to avoid losing track of linked list after a time node is deleted while stopping the timer
            timerNodeType *nextNode = currentNode->nextNode;

            // Decrement ticks to expire
            if (currentNode->ticksToExpire > 0)
                currentNode->ticksToExpire--;

            // Check if timer has expired.If set, call the timeoutHandler() and update the ticksToExpire for next event.
            if (currentNode->ticksToExpire == 0)
            {

                // add timeout handler to the timeoutHandlersQueue
                if (timeoutHandlersQueue.handlersCount != MAX_HANDLERS_QUEUE_SIZE)
                {
                    timeoutHandlersQueue.handlersBuffer[timeoutHandlersQueue.writeIndex] = currentNode->timeoutHandler;
                    timeoutHandlersQueue.writeIndex = (timeoutHandlersQueue.writeIndex + 1) % MAX_HANDLERS_QUEUE_SIZE;
                    timeoutHandlersQueue.handlersCount++;

                    // Check if timer task is suspended. If so, change status to ready to allow execution.
                    if (timerTask.status == TASK_STATUS_BLOCKED)
                        taskSetReady(&timerTask, TIMER_TIMEOUT);
                }
                currentNode->ticksToExpire = currentNode->intervalTicks;

                // Check if the timer mode is SINGLE_SHOT. If true, stop the correponding timer
                if (currentNode->mode == TIMER_MODE_SINGLE_SHOT)
                    timerStop(currentNode);
            }
            currentNode = nextNode;
        }
    }
}

/**
 * @brief Function to Start timerTask. This function will be called when starting the scheduler.
 *
 */
void timerTaskStart()
{

    taskStart(&timerTask);
}
