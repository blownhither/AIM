
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <aim/pmm.h>
#include <aim/mmu.h>
#include <libc/string.h>
#include <aim/vmm.h>
#include <aim/panic.h>
#include <list.h>

/* Designed for allocate consecutive pages */

// 32M block at most
#define NLEVEL 11
#define MAX_BLOCK ((PGSIZE)<<((NLEVEL) - 1))
#define PAGE_FRAME(x) ((x)>>12)
#define BIT_COUNT(order, paddr) (PAGE_FRAME((paddr)) >> ((order)+1))
#define BLOCK_ALIGN(order, paddr) ((paddr) & ~((PGSIZE<<(order)) - 1))

// Bitmap
typedef uint8_t bitmap;     // uint8_t[ 7 ~ 0 ]
bitmap *page_map[NLEVEL];
// static int ntop_level_pages = 0;
#define MAP_SIZE(x) ((x + 7)>>3)

// Free page pool
struct page_node {
    addr_t paddr;
    struct list_head head;
};
static struct page_node pool[NLEVEL];
#define HEAD2NODE(x) (list_entry(x, struct page_node, head))

// Statistics
static uint64_t global_empty_pages = 0;

/*************** Data structure interface ***************************/
static uint8_t read_map_bitcount(int order, int bitcount) {
    if(order >= NLEVEL || page_map[order] == NULL) { 
        panic("read_map illeagal status");
    }
    bitmap *map = page_map[order];
    uint8_t temp = map[bitcount >> 3]; 
    temp = (temp >> (bitcount & 0x7)) & 0x1;
    return temp;
}

static void switch_map_bitcount(int order, int bitcount) {
    if(order >= NLEVEL || page_map[order] == NULL) { 
        panic("read_map illeagal status");
    }
    bitmap *map = page_map[order];
    uint8_t mask = 1 << (bitcount & 0x7);
    map[bitcount >> 3] ^= mask;
}

static void set_map_bitcount(int order, int bitcount, uint8_t bit) { 
    if(order >= NLEVEL || page_map[order] == NULL) { 
        panic("read_map illeagal status");
    }
    bitmap *map = page_map[order];
    uint8_t mask = 1 << (bitcount & 0x7);
    bit &= 0x1;
    map[bitcount >> 3] = (~mask | map[bitcount >> 3]) 
        | (bit << (bitcount & 0x7));
}

static struct page_node *new_page_node() {
    struct page_node *temp = 
        (struct page_node *)kmalloc(sizeof(struct page_node), GFP_ZERO);
    if(temp != NULL)
        memset(temp, 0, sizeof(struct page_node));
    else
        panic("new_page_node fail to alloc");
    return temp;
}

static void delete_page_node(struct page_node *node) {
    kfree(node);
} 

static struct page_node *add_pool(int order, addr_t paddr) {

    struct page_node *temp = new_page_node();
    temp->paddr = paddr;
    list_init(&temp->head);

    list_add(&temp->head, &pool[order].head);
    return temp;
}

static addr_t from_pool(int order) {
    addr_t ret;
    if(list_empty(&pool[order].head)) {
        return EOF;
    }
    else {
        struct page_node *temp = HEAD2NODE(pool[order].head.next);
        ret = temp->paddr; // head should not be used
        list_del(&temp->head);
        delete_page_node(temp);
    }
    return ret;
}

static struct page_node *search_pool(int order, addr_t paddr) {
    struct list_head *p, *head = &pool[order].head;
    struct page_node *temp;
    bool found = false;
    for_each(p, head) {
        temp = HEAD2NODE(p);
        if(temp->paddr == paddr) {
            found = true;
            break;
        }
    }
    if(found) 
        return temp;
    else 
        return NULL;
}

static void delete_list_node(struct list_head* head) {
    list_del(head);
}

/*************** Inner Util Functions ***************************/

static addr_t page_init_range(addr_t start, addr_t end, uint8_t order) {
    // order in [0, NLEVEL-1]
    size_t size = (PGSIZE) << order;
    while((end - start) > 2 * size) {
        // free [start, start + size) and one more to make a pair
        add_pool(order, start);
        start += size;
        add_pool(order, start);
        start += size;
    }
    return start;   // start less than or equal to end
}

static addr_t split_page_node(int order) {
    // split (order, top]

    int top = order; 
    addr_t start;
    while(top < NLEVEL) {
       start = from_pool(top);
        if(start == EOF)
            break;
        top ++;
    }
    if(top >= NLEVEL || top == order) {
        return EOF;
    }

    // split from top to order (loop recursive)
    for(int i=top; i>order; --i) {
        // split order i to get (i-1)

        // order i block is used but don't set highest level
        if(i != NLEVEL - 1)
            switch_map_bitcount(i, BIT_COUNT(i, start));
        // add left child in pool
        add_pool(i-1, start);
        // lower level is sure to be 1
        set_map_bitcount(i-1, BIT_COUNT(i-1, start), 0);
        start = start + (PGSIZE<<order);
        
    }
    return start;
}

static uint32_t merge_page_node(int order, addr_t paddr) {
    // assume paddr is already aligned
    uint8_t temp;
    struct page_node *node;
    addr_t p1, p2;
    while(1){
        temp = read_map_bitcount(order, BIT_COUNT(order, paddr));
        if(temp) {
            // should merge
            // recycle from free pool
            
            p1 = BLOCK_ALIGN(order + 1, paddr); // plus one to get left sib
            p2 = p1 + (PGSIZE << order);    // right sib
            if(p1 == paddr) 
                node = search_pool(order, p2);
            else if(p2 == paddr)
                node = search_pool(order, p1);
            else {
                panic("merge_page_node: unexpected calculation");
            }
            
            if(node == NULL) {
                panic("merge_page_node: Illegal status missing pool node");
            }

            delete_list_node(&node->head);
            switch_map_bitcount(order, BIT_COUNT(order, paddr));

            // prepare for next loop
            order ++;
            paddr = p1;

        }
        else {
            // 0 in bitmap means sib used
            break;
        } 
    }
    return (1 << order);
}

/*************** Interfaces and bundle parts ***************************/
// Manage PADDR
void page_alloc_init(addr_t start, addr_t end) {
    // Initialize
    for(int i=0; i<NLEVEL; ++i) {
        pool[i].paddr = EOF;
        list_init(&pool[i].head);
    }
    
    // Round addr conservatively
    start = PGROUNDUP(start);
    end = PGROUNDDOWN(end);

    end = page_init_range(start, end, NLEVEL - 1); 

    global_empty_pages = (end - start) / PGSIZE;
    // Free every page
}

int bundle_pages_alloc(struct pages *pages) {
    int n = (pages->size + PGSIZE - 1) / PGSIZE;
    int npages = 0x1, order = 0;
    while(npages < n) {
        npages <<= 1;
        order ++;
    }
    if(order >= NLEVEL)
        return EOF;
    addr_t paddr = from_pool(order);
    if(EOF == paddr) {
        // need split
        paddr = split_page_node(order);  // split (order, top]
        if(paddr == EOF)
            return EOF;
        // split set bitmap 0
    }
    switch_map_bitcount(order, BIT_COUNT(order, paddr));
    
    pages->paddr = paddr;
    global_empty_pages -= npages;
    return 1;
}

int bundle_pages_free(struct pages *pages) {

    // assume [paddr, paddr+size) is inside proper block
    // TODO: and has no subblock? 
    int n = (pages->size + PGSIZE - 1) / PGSIZE;
    int npages = 0x1, order = 0;
    while(npages < n) {
        npages <<= 1;
        order ++;
    }
    if(order >= NLEVEL)
        return EOF;

    addr_t start = BLOCK_ALIGN(order, pages->paddr);
    npages = merge_page_node(order, start);    
    global_empty_pages += npages;
    
    return npages;

}

/*
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




static struct page_node *split_page_node(int order) {
    // split until specified order has available node
    // assume pool[order] is empty

}

// merge with sibling to get parent, provided the order of node
static void merge_page_node(struct page_node *node, int order) {
    if(node == NULL) {
        panic("Illegal parameter for merge_paged_node ");
    }
    // top level node has no sib
    struct page_node *temp;
    while(node->sibling != NULL && node->sibling->flags == NODE_UNUSED) {
        node->parent->flags = NODE_UNUSED;
        node->parent->paddr = NULL;
        node->parent->left = NULL;
        temp = node;

        delete_page_node(temp->sib);
        delete_page_node(temp);
        node = node->parent;
        order ++;
    }
    add_pool()
}


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

/

*/