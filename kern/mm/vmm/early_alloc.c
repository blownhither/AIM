#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <aim/vmm.h>
#include <aim/pmm.h>
#include <aim/panic.h>
#include <aim/mmu.h>
#include <aim/console.h>

/* This file is for simple allocator */

// #define NEXT_BLOCK(x) (void *)(&(x).start + (x).size)
#define BLOCK_SIZE 0x8
#define BLOCK_MASK (BLOCK_SIZE - 1)
#define BLOCK_ROUNDUP(x) (((x) + BLOCK_SIZE - 1) & ~BLOCK_MASK)
//#define EARLY_BUF_SIZE (4<<12)
    
#define MZY_DEBUG    
    
struct early_header {
    uint16_t size;      // free space left
    void *start;        // start of free space
    void *head;         // start of whole space
    bool initialized;
} ;

struct early_list {
    struct early_list *pre;
    struct early_list *next;
} freelist;

static struct early_header temp_eh;

static void e_list_add(struct early_list *new, struct early_list *head) {
    head->next->pre = new;
    new->next = head->next;
    new->pre = head;
    head->next = new;
}

static void e_list_del(struct early_list *head) {
    head->pre->next = head->next;
    head->next->pre = head->pre;
}

static bool e_list_empty(struct early_list *head) {
    return head->next == head;
}

// continous stack space for early simple allocator
void early_simple_init(struct early_header *eh, 
    void *start, uint16_t size) 
{
    eh->start = eh->head = start;
    eh->size = size;
    eh->initialized = true;

    freelist.pre = &freelist;
    freelist.next = &freelist;

    for (int i=0; i<size; i+=sizeof(struct page_node)) {
        e_list_add(start+i, &freelist);
    }
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

void early_simple_free(void *obj) {
    // TODO:
}

void sleep1();
void page_alloc_init(addr_t start, addr_t end);
static void __simple_free(void *obj) {}
static size_t __simple_size(void *obj) { return 0; }

static struct simple_allocator temp_simple_allocator = {
	.alloc	= early_simple_alloc,
	.free	= __simple_free,
    .size	= __simple_size
};


void master_early_simple_alloc(void *start, void *end) {
    //static uint8_t buf[EARLY_BUF_SIZE];

    // using early_buf (__end, __early_buf_top)
    early_simple_init(&temp_eh, start, end-start);
    
    // temp_simple_allocator.alloc = early_simple_alloc;
    
    set_simple_allocator(&temp_simple_allocator);
}

// when finish with early, note that [__end, get_early_end()] is used by simple
void *get_early_end() {
    return temp_eh.start;
}

