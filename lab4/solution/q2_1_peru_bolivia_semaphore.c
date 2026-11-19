/*
 * Lab 4 - Question 2.1: Peru / Bolivia trains, safe version using a SEMAPHORE.
 *
 * Idea in simple words:
 *   The shared track is like a room where only ONE train may be at a time.
 *   A semaphore initialised to 1 is a "box with one ticket":
 *     - sem_wait()  = take the ticket (if the box is empty, wait until it is back)
 *     - sem_post()  = put the ticket back (wakes up one waiting train)
 *   So only one train can be inside the shared section at the same time.
 *
 * Check: every printed line must contain only P's or only B's, never both.
 *
 * Build: gcc -Wall -pthread q2_1_peru_bolivia_semaphore.c -o q2_1
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define LOOPS 3

sem_t track;   /* 1 = track is free, 0 = a train is on it */

void enter_shared_track(void)
{
    sem_wait(&track);          /* wait until the track is free, then take it */
}

void exit_shared_track(void)
{
    sem_post(&track);          /* free the track for the other train */
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
    }

    return NULL;
}

int main(void)
{
    pthread_t tids[2];
    int i=0;

    struct timespec tt;
    clock_gettime(CLOCK_MONOTONIC, &tt);
    srand(tt.tv_sec);           /* seed for the random number generator */

    /* 0 = shared between threads of this process (not between processes)
       1 = initial value: the track starts free */
    sem_init(&track, 0, 1);

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

    sem_destroy(&track);
    return EXIT_SUCCESS;
}
