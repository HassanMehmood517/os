/*
 * Lab 3 - Question 8.20 (bonus): 3 threads count from 0 to 100 TOGETHER,
 * taking turns: T0 prints 0, T1 prints 1, T2 prints 2, T0 prints 3, ...
 *
 * Rule of the exercise: synchronize ONLY with reads and writes of shared
 * memory (no mutex, no semaphore, no condition variable).
 *
 * Idea: a shared variable "turn" says whose turn it is.
 *   - thread i waits (busy loop) until turn == i
 *   - it prints the next number, increases the number
 *   - it gives the turn to the next thread: turn = (i + 1) % 3
 * Only the thread whose turn it is writes the shared variables, so there is
 * never a conflict.
 *
 * Why __atomic_load_n / __atomic_store_n? They are just a READ and a WRITE,
 * but they tell the compiler (and CPU) not to cache or reorder them.
 * With a normal variable the compiler could read "turn" only once and loop
 * forever. ("volatile" alone would also work on x86.)
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NB_THREADS 3
#define MAX 100

int turn = 0;        /* whose turn it is: 0, 1 or 2 */
int number = 0;      /* next number to print */

void *counter(void *arg)
{
    int id = (int)(long) arg;

    while (1) {
        /* wait for my turn (busy waiting) */
        while (__atomic_load_n(&turn, __ATOMIC_SEQ_CST) != id)
            ;

        if (number > MAX) {
            /* finished: pass the turn so the others can also see it and stop */
            __atomic_store_n(&turn, (id + 1) % NB_THREADS, __ATOMIC_SEQ_CST);
            return NULL;
        }

        printf("T%d: %d\n", id, number);
        number++;

        __atomic_store_n(&turn, (id + 1) % NB_THREADS, __ATOMIC_SEQ_CST);
    }
}

int main(void)
{
    pthread_t tids[NB_THREADS];

    for (long i = 0; i < NB_THREADS; i++)
        pthread_create(&tids[i], NULL, counter, (void *) i);
    for (int i = 0; i < NB_THREADS; i++)
        pthread_join(tids[i], NULL);

    return EXIT_SUCCESS;
}
