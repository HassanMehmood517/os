/*
 * Lab 4 - Question 2.2: Peru / Bolivia trains, safe version WITHOUT semaphores.
 *
 * Idea in simple words:
 *   A mutex ("mutual exclusion" lock) is a lock with one key.
 *     - pthread_mutex_lock()   = take the key (wait if someone else has it)
 *     - pthread_mutex_unlock() = give the key back
 *   Only the thread holding the key can be on the shared track.
 *
 * Rule: a mutex must be unlocked by the SAME thread that locked it
 * (this is true here: the train that enters is the one that exits).
 *
 * Question 2.3 (the main problem of 2.1 and 2.2):
 *   Both solutions are SAFE but NOT FAIR. Nothing forces the trains to take
 *   turns. When a train exits and immediately calls enter() again, it often
 *   gets the lock back before the other train even wakes up. So one train can
 *   pass many times in a row while the other waits (it can even "starve").
 *   See q2_4_peru_bolivia_fair.c for a fair version.
 *
 * Build: gcc -Wall -pthread q2_2_peru_bolivia_mutex.c -o q2_2
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define LOOPS 3

/* A mutex can be initialised statically with this macro (no init call needed). */
pthread_mutex_t track = PTHREAD_MUTEX_INITIALIZER;

void enter_shared_track(void)
{
    pthread_mutex_lock(&track);
}

void exit_shared_track(void)
{
    pthread_mutex_unlock(&track);
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
