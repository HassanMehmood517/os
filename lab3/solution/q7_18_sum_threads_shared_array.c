/*
 * Lab 3 - Questions 7.17 / 7.18: PARALLEL sum with a GLOBAL SHARED ARRAY.
 *
 * Usage: ./q7_18_sum_threads_shared_array nb_MB nb_threads
 *
 * Idea: a global array "partial[]" with one cell per thread. Thread k writes
 * its partial sum into partial[k]. Each thread writes ONLY its own cell, so
 * there is no race and no lock is needed.
 *
 * Q7.17 - What must we check before computing the final sum?
 *   That ALL threads have FINISHED (= all cells are written).
 *   We do that by calling pthread_join on every thread before adding the
 *   cells. If we add too early, we read cells that are still 0 / unfinished.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>

int sum(int *vect, int size)
{
    int s = 0;
    for (int i = 0; i < size; i++)
        s += vect[i];
    return s;
}

/* shared between all threads */
int *vect;          /* the whole vector (mmap) */
int  chunk;         /* size of one part */
int *partial;       /* partial[k] = result of thread k */

void *thread_sum(void *arg)
{
    long k = (long) arg;                 /* my thread number 0..nb_threads-1 */
    partial[k] = sum(vect + k * chunk, chunk);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s nb_MB nb_threads\n", argv[0]);
        return EXIT_FAILURE;
    }

    char filename[256];
    snprintf(filename, sizeof filename, "/tmp/random_int_%sMB.data", argv[1]);
    int nb_threads = atoi(argv[2]);

    int fd = open(filename, O_RDONLY);
    if (fd == -1) { perror("open"); return EXIT_FAILURE; }
    struct stat st;
    fstat(fd, &st);
    int size = st.st_size / sizeof(int);
    vect = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (vect == MAP_FAILED) { perror("mmap"); return EXIT_FAILURE; }

    if (size % nb_threads != 0) {
        fprintf(stderr, "vector size %d is not divisible by %d\n", size, nb_threads);
        return EXIT_FAILURE;
    }

    chunk   = size / nb_threads;
    partial = calloc(nb_threads, sizeof(int));
    pthread_t *tids = malloc(nb_threads * sizeof(pthread_t));

    /* we pass the number k itself (cast to a pointer), not its address,
       so every thread gets its own value */
    for (long k = 0; k < nb_threads; k++)
        pthread_create(&tids[k], NULL, thread_sum, (void *) k);

    /* Q7.17: wait for ALL threads before reading the results */
    for (int k = 0; k < nb_threads; k++)
        pthread_join(tids[k], NULL);

    int total = 0;
    for (int k = 0; k < nb_threads; k++)
        total += partial[k];

    printf("sum = %d (with %d threads)\n", total, nb_threads);

    free(tids);
    free(partial);
    munmap(vect, st.st_size);
    close(fd);
    return EXIT_SUCCESS;
}
