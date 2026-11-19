# Lab 3 – POSIX threads (solutions)

Build: `make`. Text answers for section 2 and 3 are in `../lab3_answers.pdf`.

| Question | File | Short answer |
|---|---|---|
| 4 (matchp) | `q4_matchp.c` | Pass a **pointer to a struct** {lyrics, times}. `./q4_matchp 2 3 2 5` |
| 5.12 | `q5_counting_fixed.c` | No: with ≥2 threads result < N×1000000. |
| 5.13 | comment in `q5_counting_fixed.c` | **Race condition**: `result++` = load, add, store; increments are lost. Fix with a mutex. |
| 6.14 | `q6_14_dependencies.c` | `done[]` flags + mutex + cond var; each thread waits for its deps. Graph drawn in the comment. |
| 7.15 | `q7_15_sum_sequential.c` | `mmap` the file, loop sum. |
| 7.16 | `q7_16_sum_threads_return.c` | Each thread returns a malloc'd int; main gets it with `pthread_join(t, &ptr)`. |
| 7.17 | comment in 7.18 | Check that **all threads finished** (pthread_join all) before summing. |
| 7.18 | `q7_18_sum_threads_shared_array.c` | global `partial[k]`, one cell per thread → no lock needed. |
| 7.19 | comment in 7.16 | Faster up to ≈ number of cores, then no gain (no more cores, memory bandwidth, thread cost). |
| 8.20 | `q8_20_collaborate_count.c` | shared `turn` variable, busy-wait, only reads/writes. |

Test the sums:
```
gcc ../lab3/src/generate_random_file.c -o gen && ./gen 16
./q7_15_sum_sequential 16 ; ./q7_16_sum_threads_return 16 4 ; ./q7_18_sum_threads_shared_array 16 4
```
