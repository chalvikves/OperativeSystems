/* Tests cetegorical mutual exclusion with different numbers of threads.
 * Automatic checks only catch severe problems like crashes.
 */
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"
#include "lib/random.h" //generate random numbers

#define BUS_CAPACITY 3
#define SENDER 0
#define RECEIVER 1
#define NORMAL 0
#define HIGH 1

/*
 *	initialize task with direction and priority
 *	call o
 * */
typedef struct
{
    int direction;
    int priority;
} task_t;

void batchScheduler(unsigned int num_tasks_send, unsigned int num_task_receive,
                    unsigned int num_priority_send, unsigned int num_priority_receive);

void senderTask(void *);
void receiverTask(void *);
void senderPriorityTask(void *);
void receiverPriorityTask(void *);

struct semaphore sendWaitingToTransfer;     /* The sending normal tasks waiting for the bus */
struct semaphore sendPrioWaitingToTransfer; /* The sending prioritized tasks waiting for the bus */
struct semaphore recWaitingToTransfer;      /* The recieving normal tasks waiting for the bus */
struct semaphore recPrioWaitingToTransfer;  /* The recieving prioritized tasks waiting for the bus */
struct semaphore busSem;                    /* If the bus is full we need to wait */
struct lock lock;                           /* The normal lock */
struct lock prioLock;                       /* The priority lock */
int waiters[2];                             /* Number of waiters on each side */
int prioWaiters[2];                         /* Number of prioritized waiters on each side */
int runningTasks;                           /* Number of tasks in the bus */
int currentDirection;                       /* The direction of the bus SENDER/RECIEVER */

void oneTask(task_t task);      /*Task requires to use the bus and executes methods below*/
void getSlot(task_t task);      /* task tries to use slot on the bus */
void transferData(task_t task); /* task processes data on the bus either sending or receiving based on the direction*/
void leaveSlot(task_t task);    /* task release the slot */

/* initializes semaphores */
void init_bus(void)
{

    random_init((unsigned int)123456789);

    // Init semaphores
    sema_init(&sendWaitingToTransfer, 1);
    sema_init(&sendPrioWaitingToTransfer, 1);
    sema_init(&recWaitingToTransfer, 1);
    sema_init(&recPrioWaitingToTransfer, 1);
    sema_init(&busSem, BUS_CAPACITY);

    // Init locks
    lock_init(&lock);
    lock_init(&prioLock);

    // Initvariables
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
    for (i = 0; i < num_tasks_send; i++)
        thread_create("sender_task", 1, senderTask, NULL);

    /* create receiver threads */
    for (i = 0; i < num_task_receive; i++)
        thread_create("receiver_task", 1, receiverTask, NULL);

    /* create high priority sender threads */
    for (i = 0; i < num_priority_send; i++)
        thread_create("prio_sender_task", 1, senderPriorityTask, NULL);

    /* create high priority receiver threads */
    for (i = 0; i < num_priority_receive; i++)
        thread_create("prio_receiver_task", 1, receiverPriorityTask, NULL);
}

/* Normal task,  sending data to the accelerator */
void senderTask(void *aux UNUSED)
{
    task_t task = {SENDER, NORMAL};
    oneTask(task);
}

/* High priority task, sending data to the accelerator */
void senderPriorityTask(void *aux UNUSED)
{
    task_t task = {SENDER, HIGH};
    oneTask(task);
}

/* Normal task, reading data from the accelerator */
void receiverTask(void *aux UNUSED)
{
    task_t task = {RECEIVER, NORMAL};
    oneTask(task);
}

/* High priority task, reading data from the accelerator */
void receiverPriorityTask(void *aux UNUSED)
{
    task_t task = {RECEIVER, HIGH};
    oneTask(task);
}

/* abstract task execution*/
void oneTask(task_t task)
{
    getSlot(task);
    transferData(task);
    leaveSlot(task);
}

/* task tries to get slot on the bus subsystem */
void getSlot(task_t task)
{

    if (task.direction == SENDER)
    {
        lock_acquire(&lock);
        // Priority tasks
        if (task.priority == HIGH)
        {

            while ((runningTasks == BUS_CAPACITY) || (runningTasks > 0 && currentDirection != task.direction))
            {

                prioWaiters[task.direction]++;
                sema_down(&sendPrioWaitingToTransfer);
                sema_down(&busSem);
                prioWaiters[task.direction]--;
            }
        }
        // non-priority tasks
        else
        {
            lock_acquire(&lock);
            lock_acquire(&prioLock);
            while ((runningTasks == BUS_CAPACITY) || (runningTasks > 0 && currentDirection != task.direction))
            {
                waiters[task.direction]++;
                sema_down(&sendWaitingToTransfer);
                sema_down(&busSem);
                waiters[task.direction]--;
            }
            lock_release(&prioLock);
        }
    }
    else
    {
        lock_acquire(&lock);
        // Priority tasks
        if (task.priority == HIGH)
        {

            while ((runningTasks == BUS_CAPACITY) || (runningTasks > 0 && currentDirection != task.direction))
            {

                prioWaiters[task.direction]++;
                sema_down(&recPrioWaitingToTransfer);
                sema_down(&busSem);
                prioWaiters[task.direction]--;
            }
        }
        // non-priority tasks
        else
        {
            lock_acquire(&lock);
            lock_acquire(&prioLock);
            while ((runningTasks == BUS_CAPACITY) || (runningTasks > 0 && currentDirection != task.direction))
            {
                waiters[task.direction]++;
                sema_down(&recWaitingToTransfer);
                sema_down(&busSem);
                waiters[task.direction]--;
            }
            lock_release(&prioLock);
        }
    }
    runningTasks++;
    currentDirection = task.direction;
    lock_release(&lock);
}

/* task processes data on the bus send/receive */
void transferData(task_t task)
{
    timer_sleep(random_ulong() % 10);
}

/* task releases the slot */
void leaveSlot(task_t task)
{
    lock_acquire(&lock);
    runningTasks--;

    if (task.direction == SENDER)
    {
        if (task.priority)
        {

            if (prioWaiters[currentDirection] > 0)
            {
                sema_up(&sendPrioWaitingToTransfer);
            }
            else if (prioWaiters[1 - currentDirection] > 0)
            {
                if (runningTasks == 0)
                {
                    sema_up(&sendPrioWaitingToTransfer);
                }
            }
        }
        else
        {
            lock_acquire(&prioLock);
            if (waiters[currentDirection] > 0)
            {
                sema_up(&sendWaitingToTransfer);
            }
            else if (runningTasks == 0)
            {
                sema_up(&sendWaitingToTransfer);
            }
            lock_release(&prioLock);
        }
    }
    else
    {
        if (task.priority)
        {

            if (prioWaiters[currentDirection] > 0)
            {
                sema_up(&recPrioWaitingToTransfer);
            }
            else if (prioWaiters[1 - currentDirection] > 0)
            {
                if (runningTasks == 0)
                {
                    sema_up(&recPrioWaitingToTransfer);
                }
            }
        }
        else
        {
            lock_acquire(&prioLock);
            if (waiters[currentDirection] > 0)
            {
                sema_up(&recWaitingToTransfer);
            }
            else if (runningTasks == 0)
            {
                sema_up(&recWaitingToTransfer);
            }
            lock_release(&prioLock);
        }
    }

    lock_release(&lock);
}
