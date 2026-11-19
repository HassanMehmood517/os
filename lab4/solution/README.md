# Lab 4 – Synchronizing threads (solutions)

Build everything: `make`  — clean: `make clean`

| Question | File | Short answer |
|---|---|---|
| 2.1 | `q2_1_peru_bolivia_semaphore.c` | One semaphore at 1 around the shared track. |
| 2.2 | `q2_2_peru_bolivia_mutex.c` | Same with a mutex. |
| 2.3 | (comment in 2.2) | Safe but **not fair**: a train can re-take the lock many times in a row, the other may starve. |
| 2.4 | `q2_4_peru_bolivia_fair.c` | Ticket system (mutex + cond var): trains go in arrival order. |
| 3.5 | – | **Producer / consumer** (bounded buffer). |
| 3.6 | `q3_6_passing_objects_condvar.c` | mutex + 2 condition variables (`not_full`, `not_empty`), `while` loops. |
| 3.7 | `q3_7_passing_objects_semaphore.c` | semaphores `empty_slots`(=BUF_SIZE), `full_slots`(=0), `mutex`(=1). |
| 3.8 | (comments in 3.6/3.7) | Still correct with many threads **because** of the mutex and the `while` re-checks. Change `NB_PRODUCERS`/`NB_CONSUMERS` to test. |
| 4 | – | see below |
| 5.9 | `q5_9_barrier.c` | counter + cond var; the last thread does `broadcast`. |
| 5.10 | (comment in 5.9) | POSIX barrier: N chosen at init, reusable, returns `PTHREAD_BARRIER_SERIAL_THREAD` to one thread. |
| 5.11/5.12 | `q5_12_reusable_barrier.c` | Problem: a fast thread starts the next round before slow ones wake up. Fix: a **round number**. |

## Section 4 – malloc and threads
If two threads run `malloc` at the same time on a heap that is a linked list
of free blocks, both may read the same free block and **both get the same
memory**, or the list pointers get mixed and the free list is **broken**.
`man 3 malloc` (section ATTRIBUTES) says `malloc`/`free` are **MT-Safe**:
glibc protects the heap with internal locks, so the programmer does not need
to add a lock around `malloc`/`free`.
