#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <signal.h>
#include <sys/wait.h>
#include "a2_helper.h"

// for many of the implementations, I looked up the lab manuals and work, to guide me
#define MAX_COUNTER 10000 // it can be any value bigger or egual to 5 since we need 4 threads (including itself) to run in T5.13

sem_t sem_count, sem1, sem2; // anonymus semaphores for threads in the same process
sem_t *sem3, *sem4;          // named semaphores for threads in different processes
pthread_mutex_t lock;
pthread_cond_t condition;
int counter = 0; // we need to ensure that T5.13 can only end while 4 threads (including itself) are running

// a struct that contains the thread's id
typedef struct
{
    pid_t tid;
} TH_STRUCT;

// The thread T8.5 must terminate before T6.2 starts, but T8.1 must start only after T6.2 terminates.
void *process_8(void *arg)
{
    TH_STRUCT *s = (TH_STRUCT *)arg;
    if (s->tid == 5)
    {
        info(BEGIN, 8, s->tid);
        info(END, 8, s->tid);
        sem_post(sem3);
    }
    else if (s->tid == 1)
    {
        sem_wait(sem4);
        info(BEGIN, 8, s->tid);
        info(END, 8, s->tid);
    }
    else
    {
        info(BEGIN, 8, s->tid);
        info(END, 8, s->tid);
    }
    return NULL;
}

// Thread T6.4 must start before T6.1 starts and terminate after T6.1 terminates
void *process_6(void *arg)
{
    TH_STRUCT *s = (TH_STRUCT *)arg;
    if (s->tid == 4)
    {
        info(BEGIN, 6, s->tid);
        sem_post(&sem1); // signal T6.1 to start
        sem_wait(&sem2); // wait for T6.1 to terminate
        info(END, 6, s->tid);
    }
    else if (s->tid == 1)
    {
        sem_wait(&sem1); // wait for T6.4 to start
        info(BEGIN, 6, s->tid);
        info(END, 6, s->tid);
        sem_post(&sem2); // signal that T6.1 has terminated
    }
    else if (s->tid == 2)
    {
        sem_wait(sem3);
        info(BEGIN, 6, s->tid);
        info(END, 6, s->tid);
        sem_post(sem4);
    }
    else
    {
        info(BEGIN, 6, s->tid);
        info(END, 6, s->tid);
    }
    return NULL;
}

// At any time, at most 4 threads of process P5 could be running simultaneously, not counting the main thread
// Thread T5.13 can only end while 4 threads (including itself) are running
void *process_5(void *arg)
{
    TH_STRUCT *s = (TH_STRUCT *)arg;
    sem_wait(&sem_count);
    info(BEGIN, 5, s->tid);
    pthread_mutex_lock(&lock);
    counter++;
    while (counter < 4)
    {
        pthread_cond_wait(&condition, &lock);
    }
    counter--;
    if (s->tid == 13 && counter == 3) // thread 13 can only end while 4 threads (including itself) are running
    {
        counter = MAX_COUNTER;
        pthread_cond_broadcast(&condition);
    }
    pthread_mutex_unlock(&lock);
    info(END, 5, s->tid);
    sem_post(&sem_count);
    return NULL;
}

int main()
{
    // creating the first process P1
    init();
    info(BEGIN, 1, 0);

    // initializing the semaphores
    sem_init(&sem_count, 0, 4);
    sem_init(&sem1, 0, 0);
    sem_init(&sem2, 0, 0);
    sem3 = sem_open("/semaphore3", O_CREAT, 0644);
    sem4 = sem_open("/semaphore4", O_CREAT, 0644);

    pid_t pid2, pid3, pid4, pid5, pid6, pid7, pid8;
    // initializing the processes' ids with -1 at first
    pid2 = pid3 = pid4 = pid5 = pid6 = pid7 = pid8 = -1;

    // creating process P2
    pid2 = fork();
    if (pid2 == 0)
    {
        info(BEGIN, 2, 0);
        // creating process P6
        pid6 = fork();
        if (pid6 == 0)
        {
            info(BEGIN, 6, 0);
            TH_STRUCT thread_param[4];
            pthread_t tids[4];
            // creating the threads and initializing the id
            for (int i = 0; i < 4; i++)
            {
                thread_param[i].tid = (i + 1);
            }
            for (int i = 0; i < 4; i++)
            {
                pthread_create(&tids[i], NULL, process_6, &thread_param[i]);
            }
            // waiting for the threads to terminate
            for (int i = 0; i < 4; i++)
            {
                pthread_join(tids[i], NULL);
            }
            info(END, 6, 0);
        }
        else
        {
            wait(NULL);
            info(END, 2, 0);
        }
    }
    else
    {
        // creating process P3
        pid3 = fork();
        if (pid3 == 0)
        {
            info(BEGIN, 3, 0);
            // creating process P4
            pid4 = fork();
            if (pid4 == 0)
            {
                info(BEGIN, 4, 0);
                // creating process P8
                pid8 = fork();
                if (pid8 == 0)
                {
                    info(BEGIN, 8, 0);
                    // creating the threads and initializing the id
                    TH_STRUCT thread_param[5];
                    pthread_t tids[5];

                    for (int i = 0; i < 5; i++)
                    {
                        thread_param[i].tid = (i + 1);
                    }
                    for (int i = 0; i < 5; i++)
                    {
                        pthread_create(&tids[i], NULL, process_8, &thread_param[i]);
                    }
                    // waiting for the threads to terminate
                    for (int i = 0; i < 5; i++)
                    {
                        pthread_join(tids[i], NULL);
                    }

                    info(END, 8, 0);
                }
                else
                {
                    wait(NULL);
                    info(END, 4, 0);
                }
            }
            else
            {
                // creating process P5
                pid5 = fork();
                if (pid5 == 0)
                {
                    info(BEGIN, 5, 0);
                    TH_STRUCT thread_param[50];
                    pthread_t tids[50];
                    // creating the threads and initializing the id
                    for (int i = 0; i < 50; i++)
                    {
                        thread_param[i].tid = (i + 1);
                    }

                    for (int i = 0; i < 50; i++)
                    {
                        pthread_create(&tids[i], NULL, process_5, &thread_param[i]);
                    }
                    // waiting for the threads to terminate
                    for (int i = 0; i < 50; i++)
                    {
                        pthread_join(tids[i], NULL);
                    }

                    // creating process P7
                    pid7 = fork();
                    if (pid7 == 0)
                    {
                        info(BEGIN, 7, 0);
                        info(END, 7, 0);
                    }
                    else
                    {
                        wait(NULL);
                        info(END, 5, 0);
                    }
                }
                else
                {
                    waitpid(pid4, NULL, 0);
                    waitpid(pid5, NULL, 0);
                    info(END, 3, 0);
                }
            }
        }
        else
        {
            waitpid(pid2, NULL, 0);
            waitpid(pid3, NULL, 0);
            info(END, 1, 0);
        }
    }
    // destroying the semaphores, the mutex and the condition
    sem_destroy(&sem_count);
    sem_destroy(&sem1);
    sem_destroy(&sem2);
    sem_unlink("/semaphore3");
    sem_unlink("/semaphore4");
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&condition);
    return 0;
}

