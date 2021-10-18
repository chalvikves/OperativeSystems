/* Tests cetegorical mutual exclusion with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "lib/random.h" //generate random numbers
#include "devices/timer.h"

#define BUS_CAPACITY 3
#define SENDER 0
#define RECEIVER 1
#define NORMAL 0
#define HIGH 1

/*
 *	initialize task with direction and priority
 *	call o
 * */
typedef struct {
	int direction;
	int priority;
} task_t;

void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive);

void senderTask(void *);
void receiverTask(void *);
void senderPriorityTask(void *);
void receiverPriorityTask(void *);

struct condition waitingToTransfer[2];      /* Waiters in each direction */
struct condition prioWaitingToTransfer[2];  /* Priority waiters in each direction */
struct lock lock;                           /* The lock */
int waiters[2];                             /* Number of waiters in each direction currently */
int prioWaiters[2];                         /* Number of priority waiters in each direction currently */
int runningTasks;                           /* Number of running tasks in the bus */
int currentDirection;                       /* Current direction of the bus */


void oneTask(task_t task);/*Task requires to use the bus and executes methods below*/
void getSlot(task_t task); /* task tries to use slot on the bus */
void transferData(task_t task); /* task processes data on the bus either sending or receiving based on the direction*/
void leaveSlot(task_t task); /* task release the slot */



/* initializes semaphores */ 
void init_bus(void){ 
 
    random_init((unsigned int)123456789); 
    
    // Condition initialization
    cond_init(&waitingToTransfer[0]);
    cond_init(&waitingToTransfer[1]);
    cond_init(&prioWaitingToTransfer[0]);
    cond_init(&prioWaitingToTransfer[1]);

    // Lock initialization
    lock_init(&lock);

    // Init variables
    waiters[0] = 0;
    waiters[1] = 0;
    prioWaiters[0] = 0;
    prioWaiters[1] = 0;
    runningTasks = 0;
    currentDirection = 0;
}

/*
 *  Creates a memory bus sub-system  with num_tasks_send + num_priority_send
 *  sending data to the accelerator and num_task_receive + num_priority_receive tasks
 *  reading data/results from the accelerator.
 *
 *  Every task is represented by its own thread. 
 *  Task requires and gets slot on bus system (1)
 *  process data and the bus (2)
 *  Leave the bus (3).
 */

void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
        unsigned int num_priority_send, unsigned int num_priority_receive)
{
    unsigned int i;
    /* create sender threads */
    for(i = 0; i < num_tasks_send; i++)
        thread_create("sender_task", 1, senderTask, NULL);

    /* create receiver threads */
    for(i = 0; i < num_task_receive; i++)
        thread_create("receiver_task", 1, receiverTask, NULL);

    /* create high priority sender threads */
    for(i = 0; i < num_priority_send; i++)
       thread_create("prio_sender_task", 1, senderPriorityTask, NULL);

    /* create high priority receiver threads */
    for(i = 0; i < num_priority_receive; i++)
       thread_create("prio_receiver_task", 1, receiverPriorityTask, NULL);
}

/* Normal task,  sending data to the accelerator */
void senderTask(void *aux UNUSED){
        task_t task = {SENDER, NORMAL};
        oneTask(task);
}

/* High priority task, sending data to the accelerator */
void senderPriorityTask(void *aux UNUSED){
        task_t task = {SENDER, HIGH};
        oneTask(task);
}

/* Normal task, reading data from the accelerator */
void receiverTask(void *aux UNUSED){
        task_t task = {RECEIVER, NORMAL};
        oneTask(task);
}

/* High priority task, reading data from the accelerator */
void receiverPriorityTask(void *aux UNUSED){
        task_t task = {RECEIVER, HIGH};
        oneTask(task);
}

/* abstract task execution*/
void oneTask(task_t task) {
  getSlot(task);
  transferData(task);
  leaveSlot(task);
}

/* task tries to get slot on the bus subsystem */
void getSlot(task_t task) 
{   
    lock_acquire(&lock);

    // Priority tasks
    if(task.priority == HIGH){
        while((runningTasks == BUS_CAPACITY) || (runningTasks > 0 && currentDirection != task.direction )){
            prioWaiters[task.direction]++;
            cond_wait(&prioWaitingToTransfer[task.direction], &lock);
            prioWaiters[task.direction]--;
        }
    }

    // non-priority tasks
    else {
        while((runningTasks == BUS_CAPACITY) || (runningTasks > 0 && currentDirection != task.direction )){
            waiters[task.direction]++;
            cond_wait(&waitingToTransfer[task.direction], &lock);
            waiters[task.direction]--;
        }
    }
    
    runningTasks++;
    currentDirection = task.direction;
    lock_release(&lock);
       
}

/* task processes data on the bus send/receive */
void transferData(task_t task) 
{
    
    // Using sleep function implemented in lab2
    timer_sleep(random_ulong() % 100);
}

/* task releases the slot */
void leaveSlot(task_t task) 
{
    
    lock_acquire(&lock);
    runningTasks--;

    // If there still are priority tasks in the current direction, signal to them first
    if (prioWaiters[currentDirection] > 0){
        cond_signal(&prioWaitingToTransfer[currentDirection], &lock);
    }
    // If there still are priority tasks in the opposite direction, 
    // do nothing unless we are the last task to leave the bus.
    // If so, broadcast to the priority tasks in the opposite direction.
    else if (prioWaiters[!currentDirection] > 0){
        if (runningTasks == 0){
            cond_broadcast(&prioWaitingToTransfer[!currentDirection], &lock);
        }
    }
    // If no priority waiters and we have waiters in the current direction,
    // signal them to go
    else if (waiters[currentDirection] > 0){
        cond_signal(&waitingToTransfer[currentDirection], &lock);
    }
    // If the bus is empty and no tasks are running broadcast to other direction to go.
    else if (runningTasks == 0){
        cond_broadcast(&waitingToTransfer[!currentDirection], &lock);
    }

    lock_release(&lock);
}
