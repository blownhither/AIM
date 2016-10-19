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
    void *start;
    bool initialized;
} eh;

//TODO: early alloc does not free?

// continous stack space for early simple allocator
static void early_simple_init(void *start, uint16_t size) {
    eh.start = start;
    eh.size = size;
    eh.initialized = true;
}

static void *early_simple_alloc(size_t size, gfp_t flags) {
    size = BLOCK_ROUNDUP(size);
    if(size > eh.size || !eh.initialized) {
        
        #ifdef MZY_DEBUG
        panic("early_simple_alloc failed");
        #endif
        
        return NULL;    
    }
    void *ret = eh.start;
    eh.size -= size;
    eh.start += size;
    return ret;
}

static struct simple_allocator temp_simple_allocator = {
    	.alloc	= early_simple_alloc,
    	.free	= NULL,
	    .size	= NULL
    };

void sleep1();
void page_alloc_init(void *start, void *end);

static uint8_t buf[EARLY_BUF_SIZE];

void master_simple_alloc() {
    
    early_simple_init((void *)buf, EARLY_BUF_SIZE);
    // temp_simple_allocator.alloc = early_simple_alloc;
    set_simple_allocator(&temp_simple_allocator);
    
    // TODO: upper bound?
    page_alloc_init(&__end, (void *)KERN_BASE + PHYSTOP);
    
    sleep1();
}

