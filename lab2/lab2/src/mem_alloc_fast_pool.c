/*
 * Lab 2 - Step 1: the FAST POOLS (pool 0: 64 B blocks, pool 1: 256 B, pool 2: 1024 B)
 *
 * How it works, in simple words:
 *   - At init we get one big memory zone with my_mmap() and cut it into
 *     blocks that ALL have the same size (= max_req_size of the pool).
 *   - Free blocks are kept in a SINGLY linked list. The "next" pointer is
 *     stored INSIDE the free block itself (the memory is not used anyway).
 *   - alloc = take the FIRST block of the list (pop)
 *   - free  = put the block back at the FRONT of the list (push)
 *     -> LIFO policy (Last In, First Out): the last freed block is the
 *        first one given again.
 *   - Blocks are never split or merged, so there is no external
 *     fragmentation (but a request of 10 bytes uses a full 64-byte block:
 *     internal fragmentation).
 *
 * Answers to the Step 1 questions:
 *   1. Where are the metadata?  Inside the FREE blocks themselves (the
 *      "next" pointer). Allocated blocks have NO metadata.
 *   2. Minimum size: a free block must hold one pointer (8 bytes on 64-bit).
 *      Here every block (free or allocated) has the pool size: 64/256/1024.
 *   3. List of allocated blocks?  NO, only the free list is needed.
 *   4. Metadata of a free block: just the pointer to the next free block.
 *      Of an allocated block: nothing (the size is known from the pool).
 *   5. A freed block is inserted at the HEAD of the free list (LIFO).
 */
#include <assert.h>
#include <stdio.h>

#include "mem_alloc_fast_pool.h"
#include "my_mmap.h"
#include "mem_alloc.h"

void init_fast_pool(mem_pool_t *p, size_t size, size_t min_request_size, size_t max_request_size)
{
    (void)min_request_size;

    /* 1. reserve the memory zone of the pool */
    p->start_addr = my_mmap(size);
    assert(p->start_addr != NULL);
    p->end_addr = (char *)p->start_addr + size - 1;   /* last byte of the pool */

    /* 2. cut it into blocks of max_request_size bytes and link them:
          block 0 -> block 1 -> ... -> last block -> NULL               */
    size_t block_size = max_request_size;
    size_t nb_blocks  = size / block_size;

    char *addr = (char *)p->start_addr;
    for (size_t i = 0; i < nb_blocks; i++) {
        mem_fast_free_block_t *b = (mem_fast_free_block_t *)(addr + i * block_size);
        if (i + 1 < nb_blocks)
            b->next = (mem_fast_free_block_t *)(addr + (i + 1) * block_size);
        else
            b->next = NULL;                 /* last block: end of the list */
    }

    /* 3. the list starts with the first block (lowest address) */
    p->first_free = (nb_blocks > 0) ? p->start_addr : NULL;
}

void *mem_alloc_fast_pool(mem_pool_t *pool, size_t size)
{
    (void)size;   /* all blocks have the same size, the request size does not matter */

    mem_fast_free_block_t *b = pool->first_free;
    if (b == NULL)
        return NULL;            /* no free block left -> allocation fails */

    pool->first_free = b->next; /* remove the first block from the list (pop) */
    return b;                   /* the user can use the whole block */
}

void mem_free_fast_pool(mem_pool_t *pool, void *b)
{
    mem_fast_free_block_t *blk = (mem_fast_free_block_t *)b;

    /* put the block at the head of the free list (push) -> LIFO */
    blk->next = pool->first_free;
    pool->first_free = blk;
}

size_t mem_get_allocated_block_size_fast_pool(mem_pool_t *pool, void *addr)
{
    (void)addr;
    size_t res;
    res = pool->max_req_size;   /* every block of this pool has this size */
    return res;
}
