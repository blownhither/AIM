
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
    struct page_node *parent;
    void *paddr;
    bool flags;
    
    // controlled by interface
    struct page_node *next;
    struct page_node *pre;
};

struct page_node *pool[NLEVEL]; // pool for free pages

static uint64_t global_empty_pages = 0;

/*************** Data structure interface ***************************/
static struct page_node *new_page_node() {
    struct page_node *temp = 
        (struct page_node *)kmalloc(sizeof(struct page_node), GFP_ZERO);
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
    if(pool[n] != NULL) {
        node->next = pool[n]->next;
        if(pool[n]->next != NULL)
           pool[n]->next->pre = node;
    }
    pool[n] = node;
}

static struct page_node *from_pool(int n) {
    struct page_node *ret = pool[n];
    if(pool[n] != NULL) {
        pool[n] = pool[n]->next;
        pool[n]->pre = NULL;
    }
    return ret;
}

/*************** Inner Util Functions ***************************/
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

static struct page_node *split_page_node(int order) {
    // split until specified order has available node
    // assume pool[order] is empty
    struct page_node *target;
    int top = order; 
    
    while(top < NLEVEL) {
        target = from_pool(top);
        if(target != NULL)
            break;
        top ++;
    }
    if(top >= NLEVEL) {
        return NULL;
    }

    struct page_node *l = NULL, *r = NULL;
    // split from top to order (loop recursive)
    for(int i=top; i>order; --i) {
        target->flags = NODE_SPLIT;
        l = new_page_node();
        r = new_page_node();
        target->left = l;
        l->sib = r;
        r->sib = l;
        l->parent = r->parent = target; 
        l->flags = NODE_USED;
        r->flags = NODE_UNUSED;
        add_pool(i, r);
        target = l;
    }

    // use l
    return l;
}

/*************** Interfaces and bundle parts ***************************/
// return with paddr in pages, to be bundled in the __alloc
int bundle_pages_alloc(struct pages *pages) {
    int n = (pages->size + PGSIZE - 1) / PGSIZE;
    int npages = 0x1, order = 0;
    while(npages < n) {
        npages <<= 1;
        order ++;
    }
    if(order >= NLEVEL)
        return EOF;
    struct page_node *node = from_pool(order);
    if(node == NULL) {
        // need split
        node = split_page_node(order + 1);  // split higher order
        if(node == NULL)
            return EOF;
    }
            
    node->flags = NODE_USED;
    pages->paddr = (size_t)node->paddr;
    
    global_empty_pages -= npages;
    return 1;

}

// free a page (paddr) for page pool without affecting page table
int bundle_pages_free(struct pages *pages) {
    int n = (pages->size + PGSIZE - 1) / PGSIZE;
    int npages = 0x1, order = 0;
    while(npages < n) {
        npages <<= 1;
        order ++;
    }
    if(order >= NLEVEL)
        return EOF;


    //TODO: affect page table ? 
    global_empty_pages += npages;
}

//TODO: what to get?!?!
int bundle_get_free() {
    return (int)(global_empty_pages * PGSIZE);
}

void page_alloc_init(void *start, void *end) {
    // Initialize
    for(int i=0; i<NLEVEL; ++i)
        pool[i] = NULL;
    
    // Round addr conservatively
    start = (void *)PGROUNDUP((uint32_t)start);
    end = (void *)PGROUNDDOWN((uint32_t)end);

    global_empty_pages = (end - start) / PGSIZE;
    // Free every page
    for(int i=0; i<NLEVEL; i++) {
        start = page_init_range(start, end, NLEVEL - 1 - i); 
        if(start >= end) 
            break;
    }

}

