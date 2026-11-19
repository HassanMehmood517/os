/*
 * Lab 3 - Question 7.15: SEQUENTIAL sum of a vector read with mmap.
 *
 * Steps to test:
 *   gcc ../lab3/src/generate_random_file.c -o gen
 *   ./gen 100                       -> creates /tmp/random_int_100MB.data
 *   ./q7_15_sum_sequential 100      -> prints the sum
 *   time ./q7_15_sum_sequential 100 -> measure time (question 7.19)
 *
 * mmap in simple words: it puts the content of the file directly in our
 * memory, so we can use it like a normal int array (no read() needed).
 *   mmap(NULL,           let the OS choose the address
 *        file_size,      how many bytes to map
 *        PROT_READ,      we only read
 *        MAP_PRIVATE,    our own view of the file (we do not write it back)
 *        fd, 0)          which file, starting at offset 0
 *
 * Note: the file contains "N MB" * 1024 * 1024 ints (each int = 4 bytes).
 * Values are 0..9, so the sum fits in an int for files up to ~400 "MB".
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

int sum(int *vect, int size)
{
    int s = 0;
    for (int i = 0; i < size; i++)
        s += vect[i];
    return s;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s nb_MB   (file /tmp/random_int_<nb_MB>MB.data)\n", argv[0]);
        return EXIT_FAILURE;
    }

    char filename[256];
    snprintf(filename, sizeof filename, "/tmp/random_int_%sMB.data", argv[1]);

    int fd = open(filename, O_RDONLY);
    if (fd == -1) { perror("open"); return EXIT_FAILURE; }

    struct stat st;
    fstat(fd, &st);                         /* get the file size in bytes */
    int size = st.st_size / sizeof(int);    /* number of ints in the file */

    int *vect = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (vect == MAP_FAILED) { perror("mmap"); return EXIT_FAILURE; }

    printf("sum = %d\n", sum(vect, size));

    munmap(vect, st.st_size);
    close(fd);
    return EXIT_SUCCESS;
}
