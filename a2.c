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

#define MAX_THREADS 50

sem_t sem_count, sem1, sem2; // Anonymous semaphores
sem_t *sem3, *sem4;          // Named semaphores

pthread_mutex_t lock;
pthread_cond_t condition;
int counter = 0;

// Round-robin control variables
sem_t rr_semaphores[MAX_THREADS];
int rr_current = 0;
pthread_mutex_t rr_lock = PTHREAD_MUTEX_INITIALIZER;

// Struct to hold thread IDs
typedef struct {
    pid_t tid;
} TH_STRUCT;

// Thread function for process 5 with round-robin
void *process_5(void *arg) {
    TH_STRUCT *s = (TH_STRUCT *)arg;
    int thread_id = s->tid - 1;

    while (1) {
        printf("Thread %d waiting for its turn\n", s->tid);
        sem_wait(&rr_semaphores[thread_id]); // Wait for the thread's turn
        printf("Thread %d got semaphore\n", s->tid);

        pthread_mutex_lock(&rr_lock);
        printf("Thread %d starting\n", s->tid);
        info(BEGIN, 5, s->tid); // Log thread start
        pthread_mutex_unlock(&rr_lock);

        sleep(1); // Simulate thread work

        pthread_mutex_lock(&rr_lock);
        printf("Thread %d ending\n", s->tid);
        info(END, 5, s->tid); // Log thread end

        rr_current = (rr_current + 1) % MAX_THREADS; // Move to the next thread
        printf("Thread %d signaling thread %d\n", s->tid, rr_current + 1);
        sem_post(&rr_semaphores[rr_current]); // Signal the next thread
        pthread_mutex_unlock(&rr_lock);

        break; // Remove if threads should cycle indefinitely
    }
    return NULL;
}

int main() {
    init();
    info(BEGIN, 1, 0);
    printf("Initialization complete\n");

    sem_init(&sem_count, 0, 4);
    sem_init(&sem1, 0, 0);
    sem_init(&sem2, 0, 0);
    sem3 = sem_open("/semaphore3", O_CREAT, 0644, 0);
    sem4 = sem_open("/semaphore4", O_CREAT, 0644, 0);
    printf("Semaphores initialized\n");

    pid_t pid5;

    pid5 = fork();
    if (pid5 == 0) {
        printf("Process 5 started\n");
        info(BEGIN, 5, 0);

        TH_STRUCT thread_param[MAX_THREADS];
        pthread_t tids[MAX_THREADS];

        for (int i = 0; i < MAX_THREADS; i++) {
            sem_init(&rr_semaphores[i], 0, (i == 0) ? 1 : 0);
        }
        printf("Round-robin semaphores initialized\n");

        for (int i = 0; i < MAX_THREADS; i++) {
            thread_param[i].tid = i + 1;
            pthread_create(&tids[i], NULL, process_5, &thread_param[i]);
            printf("Thread %d created\n", i + 1);
        }

        for (int i = 0; i < MAX_THREADS; i++) {
            pthread_join(tids[i], NULL);
            printf("Thread %d joined\n", i + 1);
        }

        for (int i = 0; i < MAX_THREADS; i++) {
            sem_destroy(&rr_semaphores[i]);
        }
        printf("Round-robin semaphores destroyed\n");

        info(END, 5, 0);
        exit(0);
    }

    waitpid(pid5, NULL, 0);
    printf("Process 5 finished\n");
    info(END, 1, 0);

    sem_destroy(&sem_count);
    sem_destroy(&sem1);
    sem_destroy(&sem2);
    sem_unlink("/semaphore3");
    sem_unlink("/semaphore4");
    printf("Cleanup complete\n");

    pthread_mutex_destroy(&rr_lock);
    return 0;
}
