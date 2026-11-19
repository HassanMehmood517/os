/*
 * Lab 3 - Question 6.14: run threads T0..T9 following the dependency graph.
 *
 * The graph of Figure 1 (an arrow A -> B means "B starts after A ends"):
 *
 *            T0
 *           /  \
 *         T1    T2 ---------.
 *         |    /  \          \
 *         |  T3    T4        T5
 *         |   |     |         |
 *         |   |     |        T8
 *         |   |     |       /
 *         |   |     T7 <---'
 *          \  |   /
 *            T6          (T6 waits for T1, T3 and T7)
 *            |
 *            T9
 *
 *   T0: -           T1: T0        T2: T0        T3: T2       T4: T2
 *   T5: T2          T6: T1,T3,T7  T7: T4,T8     T8: T5       T9: T6
 *
 * Idea (works for ANY graph):
 *   - We create all 10 threads at the beginning.
 *   - Each thread first WAITS until all the threads it depends on are done,
 *     then does its work (prints, sleeps a random time, prints),
 *     then marks itself as done and wakes up the others.
 *   - "done[i]" flags are protected by a mutex; a condition variable is used
 *     to sleep until something changes (broadcast = wake everybody, each
 *     thread re-checks its own dependencies in a while loop).
 *
 * Why not pthread_join? Several threads depend on T2 (T3, T4, T5), but a
 * thread may be joined only ONCE. Flags + condition variable avoid that.
 *
 * Threads with no dependency between them run at the same time
 * (e.g. T1 and T2, or T3, T4 and T5) -> they appear mixed in the output.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define N 10
#define MAX_DEPS 3

/* deps[i] = list of threads that must finish before Ti starts (-1 = end) */
int deps[N][MAX_DEPS + 1] = {
    /* T0 */ {-1},
    /* T1 */ {0, -1},
    /* T2 */ {0, -1},
    /* T3 */ {2, -1},
    /* T4 */ {2, -1},
    /* T5 */ {2, -1},
    /* T6 */ {1, 3, 7, -1},
    /* T7 */ {4, 8, -1},
    /* T8 */ {5, -1},
    /* T9 */ {6, -1},
};

int done[N];                                   /* done[i] = 1 when Ti finished */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  changed = PTHREAD_COND_INITIALIZER;

/* returns 1 if all dependencies of thread i are finished (call with lock held) */
int ready(int i)
{
    for (int k = 0; deps[i][k] != -1; k++)
        if (!done[deps[i][k]])
            return 0;
    return 1;
}

void *task(void *arg)
{
    int i = (int)(long) arg;

    /* 1. wait for my dependencies */
    pthread_mutex_lock(&lock);
    while (!ready(i))
        pthread_cond_wait(&changed, &lock);
    pthread_mutex_unlock(&lock);

    /* 2. do the work */
    printf("T%d starts\n", i);
    sleep(rand() % 3 + 1);              /* 1 to 3 seconds */
    printf("\tT%d ends\n", i);

    /* 3. say I am done and wake up the waiting threads */
    pthread_mutex_lock(&lock);
    done[i] = 1;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&lock);

    return NULL;
}

int main(void)
{
    pthread_t tids[N];
    srand(time(NULL));

    for (long i = 0; i < N; i++)
        pthread_create(&tids[i], NULL, task, (void *) i);

    for (int i = 0; i < N; i++)
        pthread_join(tids[i], NULL);

    return EXIT_SUCCESS;
}
