# Lab 2 – Memory allocator (solution)

Code is in `lab2/src/`:
- `mem_alloc_fast_pool.c` – Step 1 (fast pools, LIFO singly linked free list). Step 1 answers in the top comment.
- `mem_alloc_standard_pool.c` – Steps 2, 4, 5 (standard pool: doubly linked free list sorted by address,
  header/footer, split, immediate coalescing, First/Best/Next fit, alignment). Step 2 answers in the top comment.
- `mem_alloc.c` – Step 3 `print_mem_state()` + safety checks (memory leak warning at exit, free of an
  address outside the pools is ignored; double free in the standard pool is ignored).

## Run the tests
```
cd lab2/src
make -B tests/alloc1.test                        # one test
for i in 1 2 3 4 5 6 7 8 9; do make -B -s tests/alloc$i.test | head -1; done
make -B STDPOOL_POLICY=BF tests/alloc5.test      # Best fit
make -B MEM_ALIGN_CONSTRAINT=8 tests/alloc5.test # alignment 8
echo -e "a10\na2000\np\nq" | ./bin/mem_shell     # interactive, p = print memory
```
Tested: all 9 tests pass with FF, BF, NF and alignment 1, 8, 16, 32, 64.

## Key ideas (short)
- Fast pool: pool cut into equal blocks; alloc = pop head; free = push head (LIFO). No metadata in used blocks.
- Standard pool block: `[header 8B | payload | footer 8B]`, header = footer = (used bit 63 | payload size).
  User pointer = header + 8. Free block also stores prev/next in its payload.
- Split only if the rest ≥ 32 bytes (header + prev + next + footer), else give the whole block.
- Free: mark free, merge with next block (check its header), merge with previous block (check its footer,
  just before our header), otherwise insert in the address-sorted list.
