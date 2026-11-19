# Lab 6 – Babble (answers + where the code is)

The final server (Stage 1 + 2.c + 2.d + Stage 3) is in this folder. Build: `make`.
Run: `./babble_server.run -r` then e.g. `./stress_test.run -n 10 -k 20 -s` or `./follow_test.run`.
Original sequential code: `../lab6/stage_0`.

## Stage 0 – questions
1. **Socket opened by:** `server_connection_init()` (babble_server_implem.c): `socket()` → `setsockopt(SO_REUSEADDR)` → `bind()` → `listen()`.
2. **New connections:** `server_connection_accept()` (calls `accept()`), called in the main loop of `main()` in babble_server.c.
3. **Steps for a client message:** `network_recv()` → `parse_command()` (builds a `command_t`) → `process_command()` (calls `run_xxx_command()`, builds an `answer_t`) → `send_answer_to_client()` → free.
4. **Why LOGIN is special:** the client has no key yet. LOGIN computes the key (hash of the name), creates the `client_bundle_t`, stores the **socket** of the client and registers it. It must be the first message; all other commands use the key.
5. **registration_table:** array of all connected clients (`client_bundle_t*`) to find a client from its key.
6. **Keys:** key = `hash(client name)`. Each command carries the key of its sender; the server uses `registration_lookup(key)` to find the client (its socket, timeline, followers). Also FOLLOW hashes the name of the followed client.
7. **Answers:** an `answer_t` = key of the target client + linked list of messages (`add_msg_to_answer`). `send_answer_to_client()` first sends the number of items, then each item, with `write_to_client()` → `network_send()` on the client socket.
8. **FOLLOW:** lookup sender, hash the name to follow, lookup that client; if the sender is not already in its `followers[]` array, add it and `nb_followers++`; answer "follow X" (if expected).
9. **PUBLISH:** for every follower of the publisher (it follows itself), `timeline_insert()` the message into the follower's timeline (circular buffer of 4). Disconnected followers are removed from the list.
10. **TIMELINE:** `timeline_generate_summary()`: answer = number of new publications since last TIMELINE, then the (at most 4) latest messages; then `count_recent_adds = 0`.
11. **Network functions** (babble_communication.c): `network_send(fd, size, buf)` sends the size then the data, returns `size` or -1; `network_recv(fd, &buf)` receives the size, mallocs the buffer, receives the data, returns the payload size (≤0 on error / connection closed).

## Stage 1 (code: babble_server.c, babble_registration.c)
- Main thread: accept, then 1 **communication thread** per client (detached).
- **Command buffer**: producer/consumer (mutex + condition variables `not_full` / `not_empty`).
- **registration_table**: readers/writer problem → `pthread_rwlock` inside babble_registration.c (lookup = read lock, insert/remove = write lock).
- `fastRandomSetSeed()` is called at the start of each executor thread.

## Step 2.a – concurrency issues with N executors
| Command | Actions | Problem |
|---|---|---|
| FOLLOW | read+write `f_client->followers[]`, `nb_followers++` | two FOLLOWs on the same client → lost update / same slot; PUBLISH reading the list while it changes |
| PUBLISH | read `followers[]`, may remove disconnected ones; `timeline_insert` in each follower's timeline | two publishers write the same timeline (`youngest`, `count_recent_adds`) → lost / corrupted messages; list modified during FOLLOW |
| TIMELINE | read the timeline and reset `count_recent_adds = 0` | a PUBLISH inserting at the same time → wrong count, lost messages |
| FOLLOW_COUNT | read `nb_followers` | read while a FOLLOW changes it |
| all | answers to the same client from 2 executors | messages of 2 answers mixed on the socket |

**Solution (2.c):** per client `followers_lock` and `timeline_lock` (lock order followers → timeline, so no deadlock) and `send_lock` around a whole answer.

## Step 2.b – new problem with RDV
RDV must be acknowledged only **after all previous commands of the client are done**. With several executors, an earlier PUBLISH of this client can still be running (or waiting in the buffer) in another thread when RDV is processed → the ack arrives too early and the test sees missing publications. (More generally, commands of one client are no longer executed in order.)

**Solution (2.d):** per client counter `pending` (+1 when the communication thread puts a command in the buffer, −1 when an executor finishes it). On RDV, the communication thread waits (condition variable) until `pending == 0`, then sends the ack. The client also waits for `pending == 0` before UNREGISTER.

## Stage 3 – PUBLISH with high priority
`BABBLE_PRODCONS_NB = 2` buffers: buffer 0 only PUBLISH, buffer 1 the rest. `buffer_get()` always takes from buffer 0 if it is not empty.

## Tested
follow_test: SUCCESS. stress_test `-n 10 -k 20`, normal and streaming (`-s`), server with random delays (`-r`): SUCCESS.
