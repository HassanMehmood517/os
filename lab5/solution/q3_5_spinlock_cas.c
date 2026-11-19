/*
 * Lab 5 - Question 3.5: spinlock with COMPARE_AND_SWAP.
 *
 * Compare_And_Swap(&x, expected, new):
 *   atomically: "if x == expected then x = new". It tells us if it worked.
 *   gcc built-in: __sync_bool_compare_and_swap(&x, expected, new)
 *                 returns true if the swap was done.
 *
 * Lock:   try to change lock from 0 (free) to 1 (taken). If it fails,
 *         somebody else has it -> spin and try again.
 * Unlock: put 0 back.
 *
 * Usage: ./q3_5_spinlock_cas nb_threads
 * Correct result = nb_threads * 1000000.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define LOOPS 1000000

int count = 0;

volatile int lock = 0;   /* 0 = free, 1 = taken */

void spinlock_lock(void)
{
    while (!__sync_bool_compare_and_swap(&lock, 0, 1))
        ;   /* spin: the lock was not 0, try again */
}

void spinlock_unlock(void)
{
    /* also done atomically; __sync_lock_release(&lock) would work too */
    __sync_bool_compare_and_swap(&lock, 1, 0);
}


void* thread_routine(void* arg)
{
    (void)arg;
    int i =0;

    for(i=0; i<LOOPS; i++){
        spinlock_lock();

        count++;

        spinlock_unlock();
    }

    return NULL;
}

int main(int argc, char **argv)
{
    int i ;
    int nb_threads = 0 ;
    pthread_t *tids;

    if (argc != 2){
        fprintf(stderr, "usage : %s nb_threads\n", argv[0]) ;
        exit (-1) ;
    }

    nb_threads = atoi (argv[1]) ;
    tids = malloc (nb_threads*sizeof(pthread_t)) ;

    for (i = 0 ; i < nb_threads; i++){
        if(pthread_create (&tids[i], NULL, thread_routine, NULL) != 0){
            fprintf(stderr,"Failed to create thread number %d\n",i);
        }
    }

    for (i = 0; i < nb_threads; i++){
        pthread_join (tids[i], NULL) ;
    }

    free (tids) ;

    printf("the result of the computation is %d\n",count);

    return EXIT_SUCCESS;
}
