
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <aim/pmm.h>
#include <aim/mmu.h>
#include <libc/string.h>

/* Designed for allocate consecutive pages */

// 32M block at most
#define NLEVEL 13
#define MAX_BLOCK ((PGSIZE)<<(NLEVEL - 1))

//TODO: write in the pages?
typedef struct mem_run {
    struct mem_run *last, *next;
} run;

static run *pool[NLEVEL];

static void *page_free_single(void *start) {
    memset(start, 0, PGSIZE);
    return start + PGSIZE;
}

static void *page_free_range(void *start, void *end, uint8_t order) {

    size_t size = (PGSIZE) << order;
    while((end - start) > size) {
        // memset(start, 0, size);
        run *temp = (run *)start;
        temp->last = NULL;
        temp->next = pool[order];
        if(pool[order] != NULL)
            pool[order]->last = temp;
        pool[order] = temp;
        start += size;
    }
    return start;
}

void page_alloc_init(void *start, void *end) {
    for(int i=0; i<NLEVEL; ++i)
        pool[i] = NULL;
    start = (void *)PGROUNDUP((uint32_t)start);
    end = (void *)PGROUNDDOWN((uint32_t)end);
    for(int i=0; i<NLEVEL; i++) {
        start = page_free_range(start, end, NLEVEL - 1 - i);
        if(start >= end) 
            break;
    }

}

