/*
 * Lab 2 - Steps 2, 4, 5: the STANDARD POOL (pool 3, requests > 1024 bytes)
 *
 * Block layout (Figure 1 of the handout). Header and footer are 8 bytes and
 * contain the same value: bit 63 = used(1)/free(0), bits 0..62 = payload size.
 *
 *   allocated block:  [ header | payload ............... | footer ]
 *   free block:       [ header | prev | next | (unused) | footer ]
 *                               ^ the user pointer points here (header + 8)
 *
 * Free blocks are in a DOUBLY linked list SORTED BY ADDRESS.
 *
 * Answers to the Step 2 questions:
 *   1. Minimum size: a free block needs header + prev + next + footer
 *      = 8 + 8 + 8 + 8 = 32 bytes (payload of at least 16 bytes).
 *      An allocated block needs header + footer + payload.
 *   2. List of allocated blocks? NO. Header/footer let us walk all blocks.
 *   3. Address returned to the user: block start + 8 (just after the header).
 *   4. free() receives that same address; the header is at address - 8.
 *   5. On free: mark header+footer as free, MERGE (coalesce) with the
 *      neighbour blocks if they are free, and insert into the free list at
 *      the place that keeps the list sorted by address.
 *   6. If the free block is bigger than needed, we SPLIT it: the first part
 *      is given to the user, the rest becomes a new free block.
 *   7. If the rest is too small to be a valid free block (< 32 bytes: not
 *      enough room for header, prev, next, footer), we do NOT split: we give
 *      the whole block to the user (a few bytes are wasted).
 *
 * Step 4 - placement policies (chosen in Makefile.config: STDPOOL_POLICY):
 *   FF = first fit : first free block big enough (fast search)
 *   BF = best fit  : block with the smallest waste (size - request)
 *   NF = next fit  : like first fit, but starts searching where the last
 *                    search stopped (wraps around at the end of the list)
 *
 * Step 5 - alignment: requested sizes are rounded up to a multiple of
 *   MEM_ALIGN_CONSTRAINT, and headers/footers
 *   take max(8, MEM_ALIGN_CONSTRAINT) bytes, so every header, payload and
 *   footer starts at an aligned address. Tested with 1, 2, 4, 8, 16, 32, 64.
 */
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "mem_alloc_types.h"
#include "mem_alloc_standard_pool.h"
#include "my_mmap.h"
#include "mem_alloc.h"

/////////////////////////////////////////////////////////////////////////////

#ifdef STDPOOL_POLICY
/* Get the value provided by the Makefile */
std_pool_placement_policy_t std_pool_policy = STDPOOL_POLICY;
#else
std_pool_placement_policy_t std_pool_policy = DEFAULT_STDPOOL_POLICY;
#endif

/////////////////////////////////////////////////////////////////////////////

#ifndef MEM_ALIGN_CONSTRAINT
#define MEM_ALIGN_CONSTRAINT 1
#endif

typedef mem_std_block_header_footer_t hf_t;   /* short name for header/footer */
typedef mem_std_free_block_t free_t;          /* short name for a free block   */

/* Space used by a header (or a footer) in memory. Normally 8 bytes.
   If the alignment is bigger than 8 (16, 32, 64) we reserve MEM_ALIGN_CONSTRAINT
   bytes so that the payload after the header is still aligned.
   (The value is stored in the first 8 bytes; the rest is padding.) */
#define HF_SIZE           (MEM_ALIGN_CONSTRAINT > sizeof(hf_t) ? (size_t)MEM_ALIGN_CONSTRAINT : sizeof(hf_t))
#define MIN_FREE_PAYLOAD  (2 * sizeof(void *))             /* 16 bytes: prev + next */
#define MIN_BLOCK         (2 * HF_SIZE + MIN_FREE_PAYLOAD) /* 32 bytes */

/* round x up to a multiple of a (a must be a power of 2) */
#define ROUND_UP(x, a)    (((x) + (a) - 1) & ~((size_t)(a) - 1))

/* the start position for the Next Fit search */
static free_t *next_fit_start = NULL;

/* ---------- small helpers ---------- */

/* footer of a block = header address + 8 + payload size */
static hf_t *footer_of(hf_t *h)
{
    return (hf_t *)((char *)h + HF_SIZE + get_block_size(h));
}

/* header of the block that comes just after this one in memory */
static hf_t *next_block(hf_t *h)
{
    return (hf_t *)((char *)footer_of(h) + HF_SIZE);
}

/* write the same size and flag in header AND footer */
static void set_block(hf_t *h, size_t size, int used)
{
    h->flag_and_size = 0;
    set_block_size(h, size);
    if (used) set_block_used(h); else set_block_free(h);
    *footer_of(h) = *h;            /* footer = copy of the header */
}

/* remove a block from the free list */
static void list_remove(mem_pool_t *pool, free_t *b)
{
    if (b->prev) b->prev->next = b->next;
    else         pool->first_free = b->next;     /* b was the head */
    if (b->next) b->next->prev = b->prev;
}

/* insert a block in the free list, keeping it sorted by address */
static void list_insert_sorted(mem_pool_t *pool, free_t *b)
{
    free_t *prev = NULL;
    free_t *cur  = pool->first_free;
    while (cur != NULL && cur < b) {   /* find the first block after b */
        prev = cur;
        cur  = cur->next;
    }
    b->prev = prev;
    b->next = cur;
    if (prev) prev->next = b; else pool->first_free = b;
    if (cur)  cur->prev  = b;
}

/* ---------- placement policies: return the chosen free block or NULL ---------- */

static free_t *find_first_fit(mem_pool_t *pool, size_t size)
{
    for (free_t *b = pool->first_free; b != NULL; b = b->next)
        if (get_block_size(&b->header) >= size)
            return b;
    return NULL;
}

static free_t *find_best_fit(mem_pool_t *pool, size_t size)
{
    free_t *best = NULL;
    for (free_t *b = pool->first_free; b != NULL; b = b->next) {
        size_t s = get_block_size(&b->header);
        if (s >= size && (best == NULL || s < get_block_size(&best->header)))
            best = b;   /* strictly smaller waste -> on a tie keep the first */
    }
    return best;
}

static free_t *find_next_fit(mem_pool_t *pool, size_t size)
{
    free_t *start = next_fit_start ? next_fit_start : pool->first_free;
    /* from start to the end of the list ... */
    for (free_t *b = start; b != NULL; b = b->next)
        if (get_block_size(&b->header) >= size)
            return b;
    /* ... then from the beginning of the list up to start */
    for (free_t *b = pool->first_free; b != NULL && b != start; b = b->next)
        if (get_block_size(&b->header) >= size)
            return b;
    return NULL;
}

/////////////////////////////////////////////////////////////////////////////

void init_standard_pool(mem_pool_t *p, size_t size, size_t min_request_size, size_t max_request_size)
{
    (void)min_request_size; (void)max_request_size;

    p->start_addr = my_mmap(size);
    assert(p->start_addr != NULL);
    p->end_addr = (char *)p->start_addr + size - 1;

    /* at the beginning the whole pool is ONE big free block */
    free_t *b = (free_t *)p->start_addr;
    set_block(&b->header, size - 2 * HF_SIZE, 0);
    b->prev = NULL;
    b->next = NULL;
    p->first_free = b;
    next_fit_start = NULL;
}

void *mem_alloc_standard_pool(mem_pool_t *pool, size_t size)
{
    /* Step 5: keep every header/payload/footer aligned */
    if (MEM_ALIGN_CONSTRAINT > 1)
        size = ROUND_UP(size, MEM_ALIGN_CONSTRAINT);
    /* a block must be able to become a free block again later */
    if (size < MIN_FREE_PAYLOAD)
        size = MIN_FREE_PAYLOAD;

    /* 1. choose a free block with the placement policy */
    free_t *b;
    switch (std_pool_policy) {
    case BEST_FIT: b = find_best_fit(pool, size); break;
    case NEXT_FIT: b = find_next_fit(pool, size); break;
    default:       b = find_first_fit(pool, size); break;
    }
    if (b == NULL)
        return NULL;                       /* no block big enough */

    size_t bsize = get_block_size(&b->header);

    /* 2. split if the rest is big enough to be a free block, else use all */
    if (bsize - size >= MIN_BLOCK) {
        free_t *prev = b->prev, *next = b->next;

        set_block(&b->header, size, 1);                     /* the used part */

        free_t *rest = (free_t *)next_block(&b->header);    /* the free rest */
        set_block(&rest->header, bsize - size - 2 * HF_SIZE, 0);

        /* "rest" takes the place of b in the list (same order by address) */
        rest->prev = prev;
        rest->next = next;
        if (prev) prev->next = rest; else pool->first_free = rest;
        if (next) next->prev = rest;

        next_fit_start = rest;
    } else {
        next_fit_start = b->next;          /* NULL -> restart from the head */
        list_remove(pool, b);
        set_block(&b->header, bsize, 1);   /* give the whole block */
    }

    /* 3. the user gets the address just after the header */
    return (char *)b + HF_SIZE;
}

void mem_free_standard_pool(mem_pool_t *pool, void *addr)
{
    free_t *b = (free_t *)((char *)addr - HF_SIZE);     /* go back to the header */
    char *pool_end = (char *)pool->end_addr + 1;

    /* safety check: freeing a block that is already free -> ignore */
    if (is_block_free(&b->header)) {
        fprintf(stderr, "WARNING: double free at %p ignored\n", addr);
        return;
    }

    size_t size = get_block_size(&b->header);
    set_block(&b->header, size, 0);

    /* 1. merge with the NEXT block in memory if it is free */
    hf_t *nh = next_block(&b->header);
    if ((char *)nh < pool_end && is_block_free(nh)) {
        free_t *n = (free_t *)nh;
        list_remove(pool, n);
        if (next_fit_start == n) next_fit_start = b;
        size += get_block_size(nh) + 2 * HF_SIZE;   /* + its header and footer */
        set_block(&b->header, size, 0);
    }

    /* 2. merge with the PREVIOUS block in memory if it is free.
          Its footer is just before our header. */
    if ((char *)b > (char *)pool->start_addr) {
        hf_t *pf = (hf_t *)((char *)b - HF_SIZE);
        if (is_block_free(pf)) {
            free_t *p = (free_t *)((char *)pf - get_block_size(pf) - HF_SIZE);
            /* p is already in the free list: just make it bigger */
            if (next_fit_start == b) next_fit_start = p;
            set_block(&p->header, get_block_size(pf) + size + 2 * HF_SIZE, 0);
            return;
        }
    }

    /* 3. no free block before us: insert into the sorted free list */
    list_insert_sorted(pool, b);
}

size_t mem_get_allocated_block_size_standard_pool(mem_pool_t *pool, void *addr)
{
    (void)pool;
    hf_t *h = (hf_t *)((char *)addr - HF_SIZE);
    return get_block_size(h);
}
