/*
 * Lab 4 - Question 3.6: passing objects, WITHOUT semaphores
 *                        (mutex + condition variables).
 *
 * Question 3.5: this is the PRODUCER / CONSUMER problem
 *   (also called "bounded buffer"). One thread produces values, another
 *   consumes them, through a buffer of fixed size BUF_SIZE.
 *
 * The buffer is used as a circular queue:
 *   - in   = index where the next value is written
 *   - out  = index where the next value is read
 *   - count = how many values are in the buffer now
 *   Indexes wrap around with "% BUF_SIZE" (after the last slot comes slot 0).
 *
 * Two rules:
 *   - the producer must WAIT when the buffer is FULL  (count == BUF_SIZE)
 *   - the consumer must WAIT when the buffer is EMPTY (count == 0)
 *
 * Question 3.8: is it still correct with many producers / consumers?
 *   YES for this version, because:
 *     - the whole buffer is protected by one mutex, and
 *     - we test the conditions with WHILE (not IF). When a thread wakes up,
 *       another thread may already have taken the free slot / the value,
 *       so it must check again before continuing.
 *   Using two condition variables (not_full / not_empty) means a producer
 *   only wakes consumers and a consumer only wakes producers.
 *   To test: set NB_PRODUCERS / NB_CONSUMERS below to more than 1.
 *
 * Build: gcc -Wall -pthread q3_6_passing_objects_condvar.c -o q3_6
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define LOOPS 20
#define BUF_SIZE 4

#define NB_PRODUCERS 1   /* change to test question 3.8 (keep the two equal) */
#define NB_CONSUMERS 1

int buffer[BUF_SIZE];
int in = 0, out = 0, count = 0;

pthread_mutex_t mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  not_full  = PTHREAD_COND_INITIALIZER;  /* signalled when a slot is freed  */
pthread_cond_t  not_empty = PTHREAD_COND_INITIALIZER;  /* signalled when a value is added */

void pushing(int val)
{
    pthread_mutex_lock(&mutex);

    while (count == BUF_SIZE)                 /* buffer full -> wait for a free slot */
        pthread_cond_wait(&not_full, &mutex);

    buffer[in] = val;
    in = (in + 1) % BUF_SIZE;
    count++;

    /* printf INSIDE the lock: the order on the screen is the real order */
    printf("pushing value %d\n", val);

    pthread_cond_signal(&not_empty);          /* tell a consumer there is data */
    pthread_mutex_unlock(&mutex);
}

int fetching(void)
{
    int val = 0;

    pthread_mutex_lock(&mutex);

    while (count == 0)                        /* buffer empty -> wait for a value */
        pthread_cond_wait(&not_empty, &mutex);

    val = buffer[out];
    out = (out + 1) % BUF_SIZE;
    count--;

    printf("\t fetched value %d\n", val);

    pthread_cond_signal(&not_full);           /* tell a producer there is space */
    pthread_mutex_unlock(&mutex);

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

    return EXIT_SUCCESS;
}
