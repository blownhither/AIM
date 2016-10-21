
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <aim/pmm.h>
#include <aim/mmu.h>
#include <libc/string.h>
#include <aim/vmm.h>

/* Designed for allocate consecutive pages */

// 32M block at most
#define NLEVEL 13
#define MAX_BLOCK ((PGSIZE)<<(NLEVEL - 1))

#define NODE_UNUSED 0x0
#define NODE_USED   0x1
#define NODE_SPLIT  0x2

struct page_node {              // saved on kernel space
    struct page_node *left;
    struct page_node *sib;
    void *paddr;
    bool flags;
    
    // controlled by interface
    struct page_node *next;
    struct page_node *pre;
};

struct page_node *pool[NLEVEL];

static struct page_node *new_page_node() {
    struct page_node *temp = (struct page_node *)kmalloc(sizeof(struct page_node), 0);
    if(temp != NULL)
        memset(temp, 0, sizeof(struct page_node));
    return temp;
}

static void delete_page_node(struct page_node *node) {
    kfree(node);
} 

static void add_pool(int n, struct page_node *node) {
    // insert between pool[n] and its next
    node->pre = pool[n];
    node->next = pool[n]->next;
    if(pool[n]->next != NULL)
        pool[n]->next->pre = node;
    pool[n] = node;
}

static struct page_node* from_pool(int n) {
    struct page_node *ret = pool[n];
    if(pool[n] != NULL) {
        pool[n] = pool[n]->next;
        pool[n]->pre = NULL;
    }
    return ret;
}

static void *page_init_range(void *start, void *end, uint8_t order) {
    // order in [0, NLEVEL-1]
    size_t size = (PGSIZE) << order;
    struct page_node *temp;
    while((end - start) > size) {
        // free [start, start + size)
        // all these root have no sibling
        temp = new_page_node();
        temp->paddr = start;
        add_pool(order, temp);
        start += size;
    }
    return start;
}

void page_alloc_init(void *start, void *end) {
    // Initialize
    for(int i=0; i<NLEVEL; ++i)
        pool[i] = NULL;
    
    // Round addr conservatively
    start = (void *)PGROUNDUP((uint32_t)start);
    end = (void *)PGROUNDDOWN((uint32_t)end);
    
    // Free every page
    for(int i=0; i<NLEVEL; i++) {
        start = page_init_range(start, end, NLEVEL - 1 - i); 
        if(start >= end) 
            break;
    }

}

