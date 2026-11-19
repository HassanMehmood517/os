/*
 * Lab 5 - Question 3.6 / 3.8 / 3.9: FAIR spinlock = TICKET LOCK,
 *                                   using FETCH_AND_ADD.
 *
 * Fetch_And_Add(&x, n): atomically adds n to x and returns the OLD value.
 *   gcc built-in: __sync_fetch_and_add(&x, n)
 *
 * Idea (like the ticket machine at the bakery):
 *   - next_ticket : the number the next arriving thread will get
 *   - now_serving : the number that is allowed in the critical section
 *   lock():   my = Fetch_And_Add(&next_ticket, 1);   // take a ticket
 *             while (now_serving != my) spin;        // wait for my number
 *   unlock(): now_serving++;                         // call the next number
 *   Because tickets are given in order, a thread that called lock() first
 *   gets the lock first -> FAIR (First In, First Out).
 *   Only the lock holder changes now_serving, so a simple ++ is enough,
 *   but we use an atomic add anyway to have a memory barrier.
 *
 * Q3.8 - When does the performance really drop?
 *   When the number of threads becomes LARGER than the number of CPU cores
 *   (check with the command "nproc"). Example: compare
 *       time ./q3_6_ticket_lock 4      and     time ./q3_6_ticket_lock 8
 *   on a 4-core machine.
 *
 * Q3.9 - Why?
 *   The OS does not know about our lock. With more threads than cores, some
 *   threads are not running (descheduled). If the OS stops the thread whose
 *   ticket is next, NOBODY can enter: all running threads spin and waste
 *   their whole time slice until the OS runs that one thread again.
 *   With the unfair locks (TAS/CAS) any running thread can take the lock,
 *   so this problem is much smaller.
 *
 * Measured on a 2-core machine: 2 threads = 0.23 s, 3 threads = more than 20 s!
 *
 * Usage: ./q3_6_ticket_lock nb_threads
 * Correct result = nb_threads * 1000000.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define LOOPS 1000000

int count = 0;

volatile unsigned int next_ticket = 0;
volatile unsigned int now_serving = 0;

void spinlock_lock(void)
{
    unsigned int my_ticket = __sync_fetch_and_add(&next_ticket, 1);
    while (now_serving != my_ticket)
        ;   /* spin until it is my turn */
}

void spinlock_unlock(void)
{
    __sync_fetch_and_add(&now_serving, 1);   /* next ticket please */
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
