/*
 * Lab 5 - Question 3.3 / 3.4: spinlock with TEST_AND_SET.
 *
 * Q3.3 - What does the hardware need to provide?
 *   ATOMIC read-modify-write instructions: "read a value AND write a new
 *   one" in a single step that no other CPU can cut in the middle.
 *   Examples: Test_And_Set, Compare_And_Swap, Fetch_And_Add.
 *   (Normal separate load + store is not enough: two threads could both
 *    read "free" and both enter.)
 *
 * What is a spinlock? A lock where a waiting thread does not sleep, it
 * "spins": it loops again and again until the lock becomes free.
 *
 * Test_And_Set(&x, 1): writes 1 into x and returns the OLD value, atomically.
 *   - old value 0 -> the lock was free and now it is mine
 *   - old value 1 -> someone else has it, try again
 * gcc built-ins (legacy __sync API, see the gcc docs "__sync Builtins"):
 *   __sync_lock_test_and_set(&x, 1)  -> test and set
 *   __sync_lock_release(&x)          -> writes 0 (with a memory barrier)
 *
 * Usage: ./q3_4_spinlock_tas nb_threads
 * Correct result = nb_threads * 1000000.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define LOOPS 1000000

int count = 0;

volatile int lock = 0;   /* 0 = free, 1 = taken. volatile: always re-read from memory */

void spinlock_lock(void)
{
    /* keep trying until the OLD value was 0 (= we took a free lock) */
    while (__sync_lock_test_and_set(&lock, 1) == 1)
        ;   /* spin */
}

void spinlock_unlock(void)
{
    __sync_lock_release(&lock);   /* lock = 0 */
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
