/*
 * Lab 5 - Question 3.7: a simple test to show which lock is FAIR.
 *
 * Idea:
 *   Each thread takes the lock many times. Every time a thread gets the
 *   lock, it writes its id into a shared "history" array. At the end we
 *   look at the history:
 *     - With a FAIR lock (ticket lock) threads that keep asking get the lock
 *       in turn: 0 1 2 3 0 1 2 3 ... -> the same thread almost never gets
 *       the lock twice in a row while others wait.
 *     - With an UNFAIR lock (TAS / CAS) the thread that just unlocked is
 *       already on the CPU and in the cache, so it often re-takes the lock
 *       immediately: 0 0 0 0 0 0 1 1 1 1 ... -> long "runs" of the same id.
 *   We print the LONGEST RUN (most times in a row for one thread) and the
 *   average run. Small = fair, big = unfair.
 *
 * Usage: ./q3_7_fairness_test tas|cas|ticket [nb_threads]
 *   Use nb_threads <= number of cores ("nproc") for a meaningful result.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define LOOPS 100000   /* lock acquisitions per thread */

/* ---- the three locks (same code as q3_4, q3_5, q3_6) ---- */
volatile int flag = 0;
volatile unsigned int next_ticket = 0, now_serving = 0;
int lock_type = 0;   /* 0 = tas, 1 = cas, 2 = ticket */

void lock(void)
{
    if (lock_type == 0) {
        while (__sync_lock_test_and_set(&flag, 1) == 1) ;
    } else if (lock_type == 1) {
        while (!__sync_bool_compare_and_swap(&flag, 0, 1)) ;
    } else {
        unsigned int my = __sync_fetch_and_add(&next_ticket, 1);
        while (now_serving != my) ;
    }
}

void unlock(void)
{
    if (lock_type == 2) __sync_fetch_and_add(&now_serving, 1);
    else                __sync_lock_release(&flag);
}

/* ---- the test ---- */
int *history;        /* history[k] = id of the thread that got the lock k-th */
long pos = 0;        /* next free place in history (protected by the lock) */

void *thread_routine(void *arg)
{
    int id = (int)(long)arg;
    for (int i = 0; i < LOOPS; i++) {
        lock();
        history[pos++] = id;
        unlock();
    }
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s tas|cas|ticket [nb_threads]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if      (strcmp(argv[1], "tas") == 0)    lock_type = 0;
    else if (strcmp(argv[1], "cas") == 0)    lock_type = 1;
    else if (strcmp(argv[1], "ticket") == 0) lock_type = 2;
    else { fprintf(stderr, "unknown lock %s\n", argv[1]); return EXIT_FAILURE; }

    int n = (argc > 2) ? atoi(argv[2]) : 4;
    long total = (long)n * LOOPS;
    history = malloc(total * sizeof(int));
    pthread_t *tids = malloc(n * sizeof(pthread_t));

    for (int i = 0; i < n; i++)
        pthread_create(&tids[i], NULL, thread_routine, (void *)(long)i);
    for (int i = 0; i < n; i++)
        pthread_join(tids[i], NULL);

    /* Measure the runs, but only in the middle part of the history where
       all threads were still asking for the lock (start and end are
       special: some threads had not started yet / were already finished). */
    long from = total / 4, to = total * 3 / 4;
    long longest = 1, cur = 1, runs = 1;
    for (long k = from + 1; k < to; k++) {
        if (history[k] == history[k - 1]) {
            cur++;
        } else {
            runs++;
            cur = 1;
        }
        if (cur > longest) longest = cur;
    }

    printf("%-6s threads=%d  longest run=%ld  average run=%.1f  (1 = perfectly fair)\n",
           argv[1], n, longest, (double)(to - from) / runs);

    free(history);
    free(tids);
    return EXIT_SUCCESS;
}
