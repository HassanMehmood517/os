/*
 * Lab 4 - Question 5.11 / 5.12 (bonus): a REUSABLE barrier.
 *
 * Question 5.11: what is the problem when we use the same barrier many times?
 *   After the last thread arrives we must reset "arrived" to 0 for the next
 *   round. But a fast thread may leave the barrier and call barrier() again
 *   (next round) BEFORE the slow threads have woken up. Then:
 *     - the fast thread changes the counter of the new round, and
 *     - the slow threads re-check "arrived < N", see it is small again,
 *       and go back to sleep forever (or threads mix two rounds).
 *   So threads must be able to tell "which round am I waiting for?".
 *
 * Solution (5.12): a ROUND number (also called "generation").
 *   - each thread remembers the round it arrived in (my_round)
 *   - the last thread resets the counter AND increases the round number
 *   - waiting threads sleep while "round == my_round"; as soon as the round
 *     number changes they know their barrier is finished, even if new
 *     threads already started using the counter for the next round.
 *
 * The test below passes the barrier ROUNDS times. In each round all
 * "round r: at barrier" lines must come before the "round r: passed" lines.
 *
 * Build: gcc -Wall -pthread q5_12_reusable_barrier.c -o q5_12
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define NB_THREADS 16
#define ROUNDS     3

pthread_mutex_t bar_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  bar_cond = PTHREAD_COND_INITIALIZER;
int arrived = 0;            /* threads arrived in the current round */
unsigned long round_nb = 0; /* number of the current round */

void barrier(void)
{
    pthread_mutex_lock(&bar_lock);

    unsigned long my_round = round_nb;   /* the round I am taking part in */
    arrived++;

    if (arrived == NB_THREADS) {
        /* last thread of this round: reset for the next round and wake all */
        arrived = 0;
        round_nb++;
        pthread_cond_broadcast(&bar_cond);
    } else {
        /* wait until the round number changes = my round is finished */
        while (my_round == round_nb)
            pthread_cond_wait(&bar_cond, &bar_lock);
    }

    pthread_mutex_unlock(&bar_lock);
}

void* thread_routine(void* arg)
{
    unsigned long id = (unsigned long)arg;

    for (int r = 0; r < ROUNDS; r++) {
        printf("round %d: thread %lu at the barrier\n", r, id);
        barrier();
        printf("\t round %d: thread %lu passed\n", r, id);
    }
    return NULL;
}

int main(void)
{
    pthread_t tids[NB_THREADS];

    for (unsigned long i = 0; i < NB_THREADS; i++)
        if (pthread_create(&tids[i], NULL, thread_routine, (void*)i) != 0) {
            fprintf(stderr, "Failed to create thread %lu\n", i);
            return EXIT_FAILURE;
        }

    for (unsigned long i = 0; i < NB_THREADS; i++)
        pthread_join(tids[i], NULL);

    return EXIT_SUCCESS;
}
