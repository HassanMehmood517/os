/*
 * Lab 3 - Section 5: the "disappointing" counting program, explained + fixed.
 *
 * Q5.12 - Do we get the expected result?
 *   NO. With 1 thread we get 1000000, but with 2 or more threads the result
 *   is usually SMALLER than nb_threads * 1000000 (and changes at each run).
 *
 * Q5.13 - Why?
 *   "result = result + 1" is NOT one single step. The CPU does 3 steps:
 *        1. load result from memory into a register
 *        2. add 1 to the register
 *        3. store the register back into memory
 *   Two threads can interleave like this (result is 5):
 *        T1 loads 5            T2 loads 5
 *        T1 adds -> 6          T2 adds -> 6
 *        T1 stores 6           T2 stores 6      => result = 6, not 7!
 *   One increment is LOST. This is called a RACE CONDITION. The code that
 *   touches the shared variable is a CRITICAL SECTION and must be protected.
 *
 * FIX (this file): protect the increment with a mutex.
 *   Run: ./q5_counting_fixed 4   -> always 4000000.
 *   To see the bug, run the original ../lab3/src/counting.c.
 *
 * Even better for performance: each thread counts in a LOCAL variable and
 * adds it to result only once at the end (1 lock per thread, not 1 million).
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define N_ITER 1000000

int result;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *count(void *arg)
{
    (void)arg;
    for (int i = 0; i < N_ITER; i++) {
        pthread_mutex_lock(&lock);      /* only one thread at a time here */
        result = result + 1;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage : %s nb_threads\n", argv[0]);
        exit(-1);
    }

    int nb_threads = atoi(argv[1]);
    pthread_t *tids = malloc(nb_threads * sizeof(pthread_t));
    result = 0;

    for (int i = 0; i < nb_threads; i++)
        if (pthread_create(&tids[i], NULL, count, NULL) != 0)
            fprintf(stderr, "Failed to create thread number %d\n", i);

    for (int i = 0; i < nb_threads; i++)
        pthread_join(tids[i], NULL);

    free(tids);
    printf("the result of the computation is %d (expected %d)\n", result, nb_threads * N_ITER);
    return EXIT_SUCCESS;
}
