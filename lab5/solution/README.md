# Lab 5 – Synchronization, spinlocks, gdb (machine part, solutions)

Build: `make` (in this folder). The paper part is answered in `../lab5_onpaper_answers.pdf`.

| Question | File | Short answer |
|---|---|---|
| 2.1 | `q2_2_prod_cons_fixed.c` (top comment) | The program **deadlocks** (hangs). |
| 2.2 | `q2_2_prod_cons_fixed.c` | Producers woke `cond1` (other producers) and consumers woke `cond2`. Fix: producer → `cond2`, consumer → `cond1`. gdb steps in the comment. |
| 3.3 | `q3_4_spinlock_tas.c` | Hardware must offer **atomic read-modify-write** (TAS, CAS, FAA). |
| 3.4 | `q3_4_spinlock_tas.c` | `while (__sync_lock_test_and_set(&lock,1)) ;` / `__sync_lock_release(&lock)` |
| 3.5 | `q3_5_spinlock_cas.c` | `while (!__sync_bool_compare_and_swap(&lock,0,1)) ;` |
| 3.6 | `q3_6_ticket_lock.c` | ticket lock: `my=__sync_fetch_and_add(&next,1); while(serving!=my);` unlock: `serving++` |
| 3.7 | `q3_7_fairness_test.c` | Record who gets the lock; measure "runs". `./q3_7_fairness_test tas 2` vs `ticket 2` |
| 3.8 | – | When **threads > CPU cores** (`nproc`). |
| 3.9 | comment in 3.6 | OS does not know the lock: if the thread holding the *next ticket* is descheduled, everyone spins for nothing. |

Measured on a 2-core machine: ticket lock 2 threads = 0.23 s, 3 threads = more than 20 s.
Fairness test (2 threads): TAS longest run 50000, ticket average run 3.6.
