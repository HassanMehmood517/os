#define _GNU_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h>

#include "mem_alloc.h"
#include "mem_alloc_types.h"
#include "mem_alloc_fast_pool.h"
#include "mem_alloc_standard_pool.h"

/* Number of memory pools managed by the allocator */
#define NB_MEM_POOLS 4

#define ULONG(x) ((long unsigned int)(x))

/* Pointers to the original malloc functions */
void *(*o_malloc)(size_t) = NULL;
void (*o_free)(void *) = NULL;
void *(*o_realloc)(void *, size_t) = NULL;
void *(*o_calloc)(size_t, size_t) = NULL;

/* Array of memory pool descriptors (indexed by pool id) */
static mem_pool_t mem_pools[NB_MEM_POOLS];

/* Note: the other fields will be setup by the init procedure */
static mem_pool_t fast_pool_1_64 = {
    .pool_id = 0,
    .pool_name = "pool-0-fast (1_64)",
    .total_pool_size = MEM_POOL_0_SIZE,
    .min_req_size = 1,
    .max_req_size = 64,
    .pool_type = FAST_POOL};

/* Note: the other fields will be setup by the init procedure */
static mem_pool_t fast_pool_65_256 = {
    .pool_id = 1,
    .pool_name = "pool-1-fast (65_256)",
    .total_pool_size = MEM_POOL_1_SIZE,
    .min_req_size = 65,
    .max_req_size = 256,
    .pool_type = FAST_POOL};

/* Note: the other fields will be setup by the init procedure */
static mem_pool_t fast_pool_257_1024 = {
    .pool_id = 2,
    .pool_name = "pool-2-fast (257_1024)",
    .total_pool_size = MEM_POOL_2_SIZE,
    .min_req_size = 257,
    .max_req_size = 1024,
    .pool_type = FAST_POOL};

/* Note: the other fields will be setup by the init procedure */
static mem_pool_t standard_pool_1025_and_above = {
    .pool_id = 3,
    .pool_name = "pool-3-std (1024_Max)",
    .total_pool_size = MEM_POOL_3_SIZE,
    .min_req_size = 1025,
    .max_req_size = SIZE_MAX,
    .pool_type = STANDARD_POOL};

/* Safety check (bonus): count allocations and frees to detect memory leaks */
static long nb_alloc = 0;
static long nb_free  = 0;

/* This function is automatically called upon the termination of a process. */
void run_at_exit(void)
{
    fprintf(stderr, "YEAH B-)\n");
    /* Safety check: memory leak = some blocks were allocated but never freed */
    if (nb_alloc != nb_free)
        fprintf(stderr, "WARNING: memory leak: %ld block(s) allocated but never freed\n",
                nb_alloc - nb_free);
}

/* 
 * Returns the id of the pool in charge of a given block size.
 * Note:
 * We assume that the contents of the pools are consistent
 * (pools covering all possible block sizes,
 * sorted by increasing sizes,
 * no overlap).
 */
static int find_pool_from_block_size(size_t size)
{
    int i;
    int res = -1;
    debug_printf("size=%lu\n", size);
    for (i = 0; i < NB_MEM_POOLS; i++)
    {
        if ((size >= mem_pools[i].min_req_size) && (size <= mem_pools[i].max_req_size))
        {
            res = i;
            break;
        }
    }
    debug_printf("will return %d\n", res);
    assert(res >= 0);
    return res;
}

/*
 * Returns the id of the pool in charge of a given block.
 */
int find_pool_from_address(void *addr)
{
    int i;
    int res = -1;
    debug_printf("enter - addr = %p\n", addr);
    for (i = 0; i < NB_MEM_POOLS; i++)
    {
        if ((addr >= mem_pools[i].start_addr) && (addr <= mem_pools[i].end_addr))
        {
            res = i;
            break;
        }
    }
    debug_printf("will return %d\n", res);
    return res;
}

void memory_init(void)
{
    int i;

    /* Register the function that will be called when the process exits. */
    atexit(run_at_exit);

    /* Init all the pools */
    mem_pools[0] = fast_pool_1_64;
    mem_pools[1] = fast_pool_65_256;
    mem_pools[2] = fast_pool_257_1024;
    mem_pools[3] = standard_pool_1025_and_above;

    for (i = 0; i < 3; i++)
    {
        init_fast_pool(&(mem_pools[i]), mem_pools[i].total_pool_size, mem_pools[i].min_req_size, mem_pools[i].max_req_size);
    }
    init_standard_pool(&(mem_pools[3]), mem_pools[3].total_pool_size, mem_pools[3].min_req_size, mem_pools[3].max_req_size);

    /* checks that the pools request sizes are not overlapping */
    assert(mem_pools[0].min_req_size == 1);
    for (i = 0; i < NB_MEM_POOLS - 1; i++)
    {
        assert(mem_pools[i].max_req_size + 1 == mem_pools[i + 1].min_req_size);
        debug_printf("mem_pools[%d]: size=%lu, min_request_size=%lu, max_request_size=%lu\n", i, mem_pools[i].total_pool_size, mem_pools[i].min_req_size, mem_pools[i].max_req_size);
    }
    debug_printf("mem_pools[%d]: size=%lu, min_request_size=%lu, max_request_size=%lu\n", i, mem_pools[i].total_pool_size, mem_pools[i].min_req_size, mem_pools[i].max_req_size);
    assert(mem_pools[NB_MEM_POOLS - 1].max_req_size == SIZE_MAX);

    /* Init the pointers to the original malloc functions */
    o_malloc = dlsym(RTLD_NEXT, "malloc");
    o_free = dlsym(RTLD_NEXT, "free");
    o_realloc = dlsym(RTLD_NEXT, "realloc");
    o_calloc = dlsym(RTLD_NEXT, "calloc");
}

/* 
 * Entry point for allocation requests.
 * Forwards the request to the appopriate pool.
 */
void *memory_alloc(size_t size)
{
    int i;
    void *alloc_addr = NULL;
    debug_printf("enter size = %lu\n", size);
    i = find_pool_from_block_size(size);
    switch (mem_pools[i].pool_type)
    {
    case FAST_POOL:
        alloc_addr = mem_alloc_fast_pool(&(mem_pools[i]), size);
        break;
    case STANDARD_POOL:
        alloc_addr = mem_alloc_standard_pool(&(mem_pools[i]), size);
        break;
    default: /* we should never reach this case */
        assert(0);
    }
    if (alloc_addr == NULL)
    {
        print_alloc_error(size);
        exit(0);
    }
    else
    {
        nb_alloc++;
        print_alloc_info(alloc_addr, size);
    }
    debug_printf("return %p\n", alloc_addr);
    return alloc_addr;
}

/* 
 * Entry point for deallocation requests.
 * Forwards the request to the appopriate pool.
 */
void memory_free(void *p)
{
    int i;

    debug_printf("enter p = %p\n", p);
    i = find_pool_from_address(p);

    /* Safety check: the address is not in any pool -> wrong free(), ignore it */
    if (i < 0)
    {
        fprintf(stderr, "WARNING: free(%p): address not managed by the allocator, ignored\n", p);
        return;
    }
    nb_free++;

    switch (mem_pools[i].pool_type)
    {
    case FAST_POOL:
        mem_free_fast_pool(&(mem_pools[i]), p);
        break;
    case STANDARD_POOL:
        mem_free_standard_pool(&(mem_pools[i]), p);
        break;
    default: /* we should never reach this case */
        assert(0);
    }
    print_free_info(p);
    debug_printf("exit\n");
}

/* Returns the payload size of an allocated block */
size_t memory_get_allocated_block_size(void *addr)
{
    int i;
    size_t res;
    i = find_pool_from_address(addr);
    debug_printf("i = %d\n", i);
    assert(i >= 0);
    switch (mem_pools[i].pool_type)
    {
    case FAST_POOL:
        res = mem_get_allocated_block_size_fast_pool(&(mem_pools[i]), addr);
        break;
    case STANDARD_POOL:
        res = mem_get_allocated_block_size_standard_pool(&(mem_pools[i]), addr);
        break;
    default: /* we should never reach this case */
        assert(0);
    }
    return res;
}

/*
 * Step 3: display the state of the memory.
 * To keep the output readable, we do not print one character per BYTE:
 *   - fast pools : one character per BLOCK ('X' = allocated, '.' = free),
 *                  64 blocks per line.
 *   - standard pool: one character per 64 BYTES of the pool, plus the list
 *                  of blocks in address order: [X size] or [. size]
 *                  (size = payload size, without header and footer).
 */
void print_mem_state(void)
{
    for (int i = 0; i < NB_MEM_POOLS; i++)
    {
        mem_pool_t *p = &mem_pools[i];
        size_t total = p->total_pool_size;
        printf("%s:\n", p->pool_name);

        if (p->pool_type == FAST_POOL)
        {
            size_t bs = p->max_req_size;
            size_t nb = total / bs;
            char *map = o_malloc ? o_malloc(nb) : malloc(nb);
            /* everything is allocated, except the blocks found in the free list */
            for (size_t k = 0; k < nb; k++)
                map[k] = 'X';
            for (mem_fast_free_block_t *b = p->first_free; b != NULL; b = b->next)
                map[((char *)b - (char *)p->start_addr) / bs] = '.';
            for (size_t k = 0; k < nb; k++)
            {
                if (k % 64 == 0) printf("  [");
                putchar(map[k]);
                if (k % 64 == 63 || k == nb - 1) printf("]\n");
            }
            if (o_free) o_free(map); else free(map);
        }
        else
        {
            /* walk all blocks using the headers (header + payload + footer) */
            char *a = (char *)p->start_addr;
            char *end = (char *)p->end_addr + 1;
            size_t hf = sizeof(mem_std_block_header_footer_t);
#ifdef MEM_ALIGN_CONSTRAINT
            if (MEM_ALIGN_CONSTRAINT > hf) hf = MEM_ALIGN_CONSTRAINT;  /* same rule as the standard pool */
#endif
            printf("  blocks:");
            while (a < end)
            {
                mem_std_block_header_footer_t *h = (mem_std_block_header_footer_t *)a;
                size_t sz = get_block_size(h);
                printf(" [%c %lu]", is_block_used(h) ? 'X' : '.', ULONG(sz));
                a += sz + 2 * hf;
            }
            printf("\n  map (1 char = 64 bytes): [");
            a = (char *)p->start_addr;
            size_t pos = 0;
            while (a < end)
            {
                mem_std_block_header_footer_t *h = (mem_std_block_header_footer_t *)a;
                size_t len = get_block_size(h) + 2 * hf;
                char c = is_block_used(h) ? 'X' : '.';
                for (size_t k = pos / 64; k < (pos + len) / 64; k++)
                    putchar(c);
                pos += len;
                a += len;
            }
            printf("]\n");
        }
    }
}

void print_free_info(void *addr)
{
    if (addr)
    {
        int i;
        i = find_pool_from_address(addr);

        fprintf(stderr, "FREE  at : %lu -- pool %d\n", ULONG((char*)addr - (char*)mem_pools[i].start_addr), mem_pools[i].pool_id);
    }
    else
    {
        fprintf(stderr, "FREE  at : %lu \n", ULONG(0));
    }
}

void print_alloc_info(void *addr, int size)
{
    if (addr)
    {
        int i;
        i = find_pool_from_address(addr);

        fprintf(stderr, "ALLOC at : %lu (%d byte(s)) -- pool %d\n",
                ULONG((char*)addr - (char*)mem_pools[i].start_addr), size, mem_pools[i].pool_id);
    }
    else
    {
        print_alloc_error(size);
    }
}

void print_alloc_error(int size)
{
    fprintf(stderr, "ALLOC error : can't allocate %d bytes\n", size);
}

#ifdef MAIN
/* The main function can be changed (to perform simple tests).
 * It is NOT involved in the automated tests
 */
int main(int argc, char **argv)
{

    memory_init();

    int i;
    for (i = 0; i < 10; i++)
    {
        char *b = memory_alloc(rand() % 8);
        memory_free(b);
    }

    char *a = memory_alloc(15);
    memory_free(a);

    a = memory_alloc(10);
    memory_free(a);

    fprintf(stderr, "%lu\n", (long unsigned int)(memory_alloc(9)));
    return EXIT_SUCCESS;
}
#endif /* MAIN */
