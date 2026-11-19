/*
 * Lab 4 - Question 3.7: passing objects, WITH semaphores.
 *
 * Producer / consumer with 3 semaphores:
 *   - empty_slots : how many free places are in the buffer (starts at BUF_SIZE)
 *   - full_slots  : how many values are waiting in the buffer (starts at 0)
 *   - mutex       : a semaphore at 1, used as a lock around the buffer indexes
 *
 * Producer:  wait(empty_slots)  -> lock -> write -> unlock -> post(full_slots)
 * Consumer:  wait(full_slots)   -> lock -> read  -> unlock -> post(empty_slots)
 *
 * The counting semaphores do the "wait if full / wait if empty" job for us.
 *
 * IMPORTANT ORDER: always wait on empty_slots/full_slots BEFORE taking mutex.
 * If you take mutex first and then block on empty_slots, the consumer can
 * never get mutex to free a slot -> DEADLOCK.
 *
 * Question 3.8: with only 1 producer and 1 consumer the mutex is not strictly
 * needed (each thread only touches its own index). With SEVERAL producers or
 * consumers the mutex IS needed: two producers could otherwise write to the
 * same slot "in" at the same time. With the mutex this version stays correct.
 *
 * Build: gcc -Wall -pthread q3_7_passing_objects_semaphore.c -o q3_7
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define LOOPS 20
#define BUF_SIZE 4

#define NB_PRODUCERS 1   /* change to test question 3.8 (keep the two equal) */
#define NB_CONSUMERS 1

int buffer[BUF_SIZE];
int in = 0, out = 0;

sem_t empty_slots;
sem_t full_slots;
sem_t mutex;

void pushing(int val)
{
    sem_wait(&empty_slots);      /* wait for a free slot */
    sem_wait(&mutex);

    buffer[in] = val;
    in = (in + 1) % BUF_SIZE;
    printf("pushing value %d\n", val);   /* inside the lock: correct order */

    sem_post(&mutex);
    sem_post(&full_slots);       /* one more value available */
}

int fetching(void)
{
    int val = 0;

    sem_wait(&full_slots);       /* wait for a value */
    sem_wait(&mutex);

    val = buffer[out];
    out = (out + 1) % BUF_SIZE;
    printf("\t fetched value %d\n", val);

    sem_post(&mutex);
    sem_post(&empty_slots);      /* one more free slot */

    return val;
}

void* thread_generating(void* arg)
{
    (void)arg;
    for (int i = 0; i < LOOPS; i++)
        pushing(rand() % 10);
    return NULL;
}

void* thread_using(void* arg)
{
    (void)arg;
    for (int i = 0; i < LOOPS; i++)
        fetching();
    return NULL;
}

int main(void)
{
    pthread_t prod[NB_PRODUCERS], cons[NB_CONSUMERS];

    struct timespec tt;
    clock_gettime(CLOCK_MONOTONIC, &tt);
    srand(tt.tv_sec);

    sem_init(&empty_slots, 0, BUF_SIZE);
    sem_init(&full_slots,  0, 0);
    sem_init(&mutex,       0, 1);

    for (int i = 0; i < NB_CONSUMERS; i++)
        if (pthread_create(&cons[i], NULL, thread_using, NULL) != 0) {
            fprintf(stderr, "Failed to create a using thread\n");
            return EXIT_FAILURE;
        }
    for (int i = 0; i < NB_PRODUCERS; i++)
        if (pthread_create(&prod[i], NULL, thread_generating, NULL) != 0) {
            fprintf(stderr, "Failed to create a generating thread\n");
            return EXIT_FAILURE;
        }

    for (int i = 0; i < NB_CONSUMERS; i++) pthread_join(cons[i], NULL);
    for (int i = 0; i < NB_PRODUCERS; i++) pthread_join(prod[i], NULL);

    sem_destroy(&empty_slots);
    sem_destroy(&full_slots);
    sem_destroy(&mutex);
    return EXIT_SUCCESS;
}
