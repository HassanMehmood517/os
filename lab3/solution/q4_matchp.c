/*
 * Lab 3 - Section 4: "matchp" = match with PARAMETERS.
 *
 * Problem: pthread_create() lets us pass only ONE argument (a void *).
 * We need TWO values per thread: the song lyrics and how many times to sing.
 * Solution: put both values in a STRUCT and pass a POINTER to the struct.
 *
 * Usage: ./q4_matchp nb_french nb_times_french nb_english nb_times_english
 * Example: ./q4_matchp 2 3 2 5
 *
 * Important: each struct must stay alive until the thread has finished
 * reading it. Here the 2 structs live in main(), and main() waits with
 * pthread_join, so this is safe. (Never pass the address of a local variable
 * that changes in a loop before the thread reads it.)
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

/* the two parameters for one supporter thread */
struct song {
    char *lyrics;
    int   times;
};

void *supporter(void *arg)
{
    struct song *s = (struct song *) arg;   /* get our struct back */
    pthread_t tid = pthread_self();

    for (int i = 0; i < s->times; i++)
        printf("Process %d Thread %lx : %s\n", getpid(), (unsigned long) tid, s->lyrics);

    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage : %s nb_french times_french nb_english times_english\n", argv[0]);
        exit(-1);
    }

    int team1 = atoi(argv[1]);
    struct song french  = { "Allons enfants de la patrie", atoi(argv[2]) };
    int team2 = atoi(argv[3]);
    struct song english = { "Swing low, sweet chariot",    atoi(argv[4]) };

    int nb_threads = team1 + team2;
    pthread_t *tids = malloc(nb_threads * sizeof(pthread_t));

    int i;
    for (i = 0; i < team1; i++)                     /* French supporters */
        pthread_create(&tids[i], NULL, supporter, &french);
    for (; i < nb_threads; i++)                     /* English supporters */
        pthread_create(&tids[i], NULL, supporter, &english);

    for (i = 0; i < nb_threads; i++)                /* wait for everybody */
        pthread_join(tids[i], NULL);

    free(tids);
    return EXIT_SUCCESS;
}
