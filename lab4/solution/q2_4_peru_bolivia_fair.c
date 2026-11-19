/*
 * Lab 4 - Question 2.4 (bonus): Peru / Bolivia trains, SAFE and FAIR.
 *
 * Problem with 2.1 / 2.2: a train that just left can grab the track again
 * before the waiting train wakes up, so one train may wait forever.
 *
 * Idea in simple words: a "ticket" system, like at the bakery.
 *   - Each train that arrives takes the next ticket number (next_ticket++).
 *   - A display shows which ticket may go now (now_serving).
 *   - A train waits until now_serving == its ticket.
 *   - When it leaves, it increases now_serving and wakes everybody up.
 *   Trains are served in the order they arrived (First In, First Out),
 *   so if both trains keep coming, they will take turns.
 *
 * Tools used:
 *   - a mutex to protect the two counters
 *   - a condition variable to sleep while it is not our turn
 *     (pthread_cond_wait releases the mutex while sleeping and takes it
 *      back when woken up; we always re-check the condition in a WHILE loop)
 *
 * Build: gcc -Wall -pthread q2_4_peru_bolivia_fair.c -o q2_4
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define LOOPS 3

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  turn = PTHREAD_COND_INITIALIZER;
unsigned long next_ticket = 0;   /* next number to give to an arriving train */
unsigned long now_serving = 0;   /* number allowed to use the track now     */

void enter_shared_track(void)
{
    pthread_mutex_lock(&lock);
    unsigned long my_ticket = next_ticket++;     /* take a ticket */
    while (now_serving != my_ticket)             /* not my turn yet -> sleep */
        pthread_cond_wait(&turn, &lock);
    pthread_mutex_unlock(&lock);
    /* now I am the only train on the track */
}

void exit_shared_track(void)
{
    pthread_mutex_lock(&lock);
    now_serving++;                      /* call the next ticket */
    pthread_cond_broadcast(&turn);      /* wake all waiters; only the right one goes */
    pthread_mutex_unlock(&lock);
}

void using_shared_track_section(char* name)
{
    int trip_duration = rand() % 6 +1;

    enter_shared_track();

    for(; trip_duration>0; trip_duration--){
        printf("%c ",name[0]);
        fflush(stdout);
        usleep(1);
    }

    printf("\n");

    exit_shared_track();
}

void* train(void* arg)
{
    char *country=(char*) arg;
    int i=0;

    for(i=0; i<LOOPS; i++){
        using_shared_track_section(country);
        usleep(100);   /* time to "drive around" before coming back to the border */
    }

    return NULL;
}

int main(void)
{
    pthread_t tids[2];
    int i=0;

    struct timespec tt;
    clock_gettime(CLOCK_MONOTONIC, &tt);
    srand(tt.tv_sec);

    if(pthread_create (&tids[0], NULL, train, (void*)"PERU") != 0){
        fprintf(stderr,"Failed to create the peruvian thread\n");
        return EXIT_FAILURE;
    }
    if(pthread_create (&tids[1], NULL, train, (void*)"BOLIVIA") != 0){
        fprintf(stderr,"Failed to create the bolivian thread\n");
        return EXIT_FAILURE;
    }

    for (i = 0; i < 2; i++){
        pthread_join (tids[i], NULL) ;
    }

    return EXIT_SUCCESS;
}
