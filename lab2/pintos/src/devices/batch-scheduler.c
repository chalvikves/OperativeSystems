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

void oneTask(task_t task);      /*Task requires to use the bus and executes methods below*/
void getSlot(task_t task);      /* task tries to use slot on the bus */
void transferData(task_t task); /* task processes data on the bus either sending or receiving based on the direction*/
void leaveSlot(task_t task);    /* task release the slot */

struct semaphore busSpace;     /* To ensure that there are at most BUS_CAPACITY threads using the bus */
struct semaphore sendPrioDone; /* Indicates if there are no more HIGH priority senders tasks */
struct semaphore recPrioDone;  /* Indicates if there are no more HIGH priority recievers tasks */

struct lock sendLock; /* Lock used to update number of HIGH priority sender tasks */
struct lock recLock;  /* Lock used to update number of HIGH priorities recieve tasks */

unsigned int numHighPrioSend; /* Number of HIGH priority sender tasks */
unsigned int numHighPrioRec;  /* Number of HIGH priority recieve tasks */
int direction;                /* The direction of the bus */

/* initializes semaphores */
void init_bus(void)
{
    random_init((unsigned int)123456789);

    // Init semaphores
    sema_init(&busSpace, BUS_CAPACITY);
    sema_init(&sendPrioDone, 1);
    sema_init(&recPrioDone, 1);

    // Init locks
    lock_init(&sendLock);
    lock_init(&recLock);

    // Variable init
    numHighPrioSend = 0;
    numHighPrioRec = 0;
    direction = 0;
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

    numHighPrioSend = num_priority_send;
    numHighPrioRec = num_priority_receive;

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
        if (task.priority == HIGH)
        {
            // Aquire a space on the bus
            sema_down(&busSpace);

            // Since task is done, remove from list
            lock_acquire(&sendLock);
            numHighPrioSend--;
            lock_release(&sendLock);

            // If there are no tasks left, tell normal priority that we are done
            if (numHighPrioSend == 0)
            {
                sema_up(&sendPrioDone);
            }
        }
        else
        {
            // Wait until priority is done, then aquire spot on bus
            sema_down(&sendPrioDone);
            sema_down(&busSpace);
            sema_up(&sendPrioDone);
        }
        // If the direction isn't right for the specific task
        if (task.direction != direction)
        {
            lock_acquire(&sendLock);
            direction = SENDER;
            lock_release(&sendLock);
        }
    }
    else
    {
        if (task.priority == HIGH)
        {
            // Aquire a space on the bus
            sema_down(&busSpace);

            // Since task is done, remove from list
            lock_acquire(&recLock);
            numHighPrioRec--;
            lock_release(&recLock);

            // If there are no tasks left, tell normal priority that we are done
            if (numHighPrioRec == 0)
            {
                sema_up(&sendPrioDone);
            }
        }
        else
        {
            // Wait until priority is done, then aquire spot on bus
            sema_down(&recPrioDone);
            sema_down(&busSpace);
            sema_up(&recPrioDone);
        }

        // If the direction isn't right for the specific task
        if (task.direction != direction)
        {
            lock_acquire(&recLock);
            direction = RECEIVER;
            lock_release(&recLock);
        }
    }
}

/* task processes data on the bus send/receive */
void transferData(task_t task)
{
    // Using sleep function implemented in lab2
    timer_sleep(random_ulong() % 10);
}

/* task releases the slot */
void leaveSlot(task_t task)
{
    // Simply say that the task is done, and giving space on the bus
    sema_up(&busSpace);

    // If bus is empty, change direction
    if (busSpace.value == BUS_CAPACITY)
    {
        direction = !direction;
    }
}