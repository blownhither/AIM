#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <aim/vmm.h>
#include <aim/panic.h>
#include <aim/mmu.h>

/* This file is for simple allocator */

// #define NEXT_BLOCK(x) (void *)(&(x).start + (x).size)
#define BLOCK_SIZE 0x8
#define BLOCK_MASK (BLOCK_SIZE - 1)
#define BLOCK_ROUNDUP(x) (((x) + BLOCK_SIZE - 1) & BLOCK_MASK)
#define EARLY_BUF_SIZE (1<<12)
    
#define MZY_DEBUG    
    
struct early_header {
    uint16_t size;      // free space left
    void *start;        // start of free space
    void *head;         // start of whole space
    bool initialized;
} ;

static struct early_header temp_eh;

// continous stack space for early simple allocator
void early_simple_init(struct early_header *eh, 
    void *start, uint16_t size) 
{
    eh->start = eh->head = start;
    eh->size = size;
    eh->initialized = true;
}

void *early_simple_alloc(size_t size, gfp_t flags) {
    size = BLOCK_ROUNDUP(size);
    if(size > temp_eh.size || !temp_eh.initialized) {
        
        #ifdef MZY_DEBUG
        panic("early_simple_alloc failed");
        #endif
        
        return NULL;    
    }
    void *ret = temp_eh.start;
    temp_eh.size -= size;
    temp_eh.start += size;
    return ret;
}

void sleep1();
void page_alloc_init(void *start, void *end);

static struct simple_allocator temp_simple_allocator = {
	.alloc	= early_simple_alloc,
	.free	= NULL,
    .size	= NULL
};

void master_early_simple_alloc() {
    static uint8_t buf[EARLY_BUF_SIZE];
    
    early_simple_init(&temp_eh, (void *)buf, EARLY_BUF_SIZE);
    // temp_simple_allocator.alloc = early_simple_alloc;
    
    set_simple_allocator(&temp_simple_allocator);
    
    // TODO: upper bound?
    page_alloc_init(&__end, (void *)KERN_BASE + PHYSTOP);
    
    sleep1();
}

