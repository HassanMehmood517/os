/*
 * Lab 4 - Question 5.9: a ONE-TIME barrier with condition variables.
 *
 * What a barrier does: every thread that calls barrier() stops and waits
 * until ALL NB_THREADS threads have called barrier(). Then all continue.
 * Check: all "at the barrier" lines must be printed BEFORE any "after" line.
 *
 * Idea:
 *   - a counter "arrived" counts how many threads reached the barrier
 *   - each thread increases it; if it is not the last one, it sleeps
 *   - the LAST thread (arrived == NB_THREADS) wakes everybody up (broadcast)
 *   - we sleep in a WHILE loop because a thread can wake up "by accident"
 *     (spurious wakeup): it checks again that everybody arrived.
 *
 * Question 5.10: difference with the POSIX barrier (pthread_barrier_wait):
 *   - The POSIX barrier is initialised with pthread_barrier_init(&b, NULL, N)
 *     so the number of threads N is chosen at run time, and you can have
 *     many barrier objects. Ours is hard-coded to NB_THREADS and global.
 *   - The POSIX barrier is REUSABLE (it resets itself after each round);
 *     ours only works once.
 *   - pthread_barrier_wait returns PTHREAD_BARRIER_SERIAL_THREAD to exactly
 *     one thread (useful to let one thread do some extra work).
 *
 * Build: gcc -Wall -pthread q5_9_barrier.c -o q5_9
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define NB_THREADS 16

pthread_mutex_t bar_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  bar_cond = PTHREAD_COND_INITIALIZER;
int arrived = 0;          /* how many threads reached the barrier */

void barrier(void)
{
    pthread_mutex_lock(&bar_lock);

    arrived++;
    if (arrived == NB_THREADS) {
        /* I am the last one: wake up everybody who is waiting */
        pthread_cond_broadcast(&bar_cond);
    } else {
        /* not everybody is here yet: sleep until they are */
        while (arrived < NB_THREADS)
            pthread_cond_wait(&bar_cond, &bar_lock);
    }

    pthread_mutex_unlock(&bar_lock);
}

void* thread_routine(void* arg)
{
    unsigned long id = (unsigned long)arg;

    printf("thread %lu at the barrier\n", id);

    barrier();

    printf("\t thread %lu after the barrier\n", id);

    return NULL;
}

int main(void)
{
    pthread_t tids[NB_THREADS];
    unsigned long i=0;

    for(i=0; i<NB_THREADS; i++){
        if(pthread_create (&tids[i], NULL, thread_routine, (void*)i) != 0){
            fprintf(stderr,"Failed to create thread %lu\n", i);
            return EXIT_FAILURE;
        }
    }

    for (i = 0; i < NB_THREADS; i++){
        pthread_join (tids[i], NULL) ;
    }

    return EXIT_SUCCESS;
}
