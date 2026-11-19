/*
 * Lab 3 - Question 7.16: PARALLEL sum, each thread RETURNS its partial sum.
 *
 * Usage: ./q7_16_sum_threads_return nb_MB nb_threads
 *        (vector size must be divisible by nb_threads, e.g. 1, 2, 4, 8)
 *
 * Idea:
 *   - cut the vector into nb_threads equal parts
 *   - thread k sums part k with the sequential sum() function
 *   - the thread returns its result; main gets it with pthread_join
 *     (2nd argument of pthread_join = where to store the returned void *)
 *   - main adds the partial results
 *
 * How to return a number from a thread: we malloc an int, put the result
 * in it, and return its address. main reads it and frees it.
 * (Never return the address of a LOCAL variable: it disappears when the
 *  thread ends.)
 *
 * Q7.19 (performance, try "time" with 1, 2, 4, 8 threads on a big file):
 *   - With more threads the time first goes DOWN (several cores work in
 *     parallel), up to about the number of cores ("nproc").
 *   - After that it does not improve any more (or gets worse): there are no
 *     more free cores, creating threads costs time, and the work is limited
 *     by memory speed / reading the file, not by the CPU.
 *   - The first run may be slower: the file must be read from disk; later
 *     runs use the file already in memory (page cache).
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

/* what each thread needs to know: where its part starts and how long it is */
struct part {
    int *start;
    int  size;
};

void *thread_sum(void *arg)
{
    struct part *p = arg;
    int *res = malloc(sizeof(int));      /* result must outlive the thread */
    *res = sum(p->start, p->size);
    return res;                          /* same as pthread_exit(res) */
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
    int *vect = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (vect == MAP_FAILED) { perror("mmap"); return EXIT_FAILURE; }

    if (size % nb_threads != 0) {
        fprintf(stderr, "vector size %d is not divisible by %d\n", size, nb_threads);
        return EXIT_FAILURE;
    }

    pthread_t   *tids  = malloc(nb_threads * sizeof(pthread_t));
    struct part *parts = malloc(nb_threads * sizeof(struct part));  /* one struct per thread! */
    int chunk = size / nb_threads;

    for (int k = 0; k < nb_threads; k++) {
        parts[k].start = vect + k * chunk;
        parts[k].size  = chunk;
        pthread_create(&tids[k], NULL, thread_sum, &parts[k]);
    }

    int total = 0;
    for (int k = 0; k < nb_threads; k++) {
        int *partial;
        pthread_join(tids[k], (void **) &partial);   /* get the returned pointer */
        total += *partial;
        free(partial);
    }

    printf("sum = %d (with %d threads)\n", total, nb_threads);

    free(tids);
    free(parts);
    munmap(vect, st.st_size);
    close(fd);
    return EXIT_SUCCESS;
}
