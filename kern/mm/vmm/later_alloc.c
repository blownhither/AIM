
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <libc/string.h>
#include <aim/panic.h>
#include <aim/mmu.h>
#include <aim/pmm.h>
#include <aim/vmm.h>
#include <aim/console.h>
#include <asm.h>

#define printk kprintf
#define GFP_LEVEL_MASK 0xf
#define MAX_KMALLOC_K ((PAGE_SIZE<<(NUM_AREA_ORDERS-1))>>10)
#define MAX_GET_FREE_PAGE_TRIES 4

#define MF_USED 0xffaa0055
#define MF_FREE 0x0055ffaa

// #define MZYDEBUG

/* 
 * A block header. This is in front of every malloc-block, whether free or not.
 */
 // _length is the occupied size, _next used when block is free
struct block_header {
    unsigned long bh_flags;
    union {
        unsigned long ubh_length;
        struct block_header *fbh_next;
    } vp;
};

#define bh_length vp.ubh_length
#define bh_next   vp.fbh_next
#define BH(p) ((struct block_header *)(p))

/* 
 * The page descriptor is at the front of every page that malloc has in use. 
 */
 // .order is for block size, while nfree is num of free block
struct page_descriptor {
    struct page_descriptor *next;
    struct block_header *firstfree;
    int order;
    int nfree;
};

#define PAGE_DESC(p) ((struct page_descriptor *)(((unsigned long)(p)) & PAGE_MASK))

/*
 * A size descriptor describes a specific class of malloc sizes.
 * Each class of sizes has its own freelist.
 */
struct size_descriptor {
    struct page_descriptor *firstfree;
    struct page_descriptor *dmafree; /* DMA-able memory */
    int size;
    int nblocks;

    int nmallocs;
    int nfrees;         // nfrees * size = freeBytes
    int nbytesmalloced;
    int npages;
    unsigned long gfporder; /* number of pages in the area required */
};

// bucket
struct size_descriptor sizes[] = { 
    { NULL, NULL,  32,127, 0,0,0,0, 0},
    { NULL, NULL,  64, 63, 0,0,0,0, 0 },
    { NULL, NULL, 128, 31, 0,0,0,0, 0 },
    { NULL, NULL, 252, 16, 0,0,0,0, 0 },
    { NULL, NULL, 508,  8, 0,0,0,0, 0 },
    { NULL, NULL,1020,  4, 0,0,0,0, 0 },
    { NULL, NULL,2040,  2, 0,0,0,0, 0 },
    { NULL, NULL,4096-16,  1, 0,0,0,0, 0 },
    //{ NULL, NULL,8192-16,  1, 0,0,0,0, 1 },
    //{ NULL, NULL,16384-16,  1, 0,0,0,0, 2 },
    //{ NULL, NULL,32768-16,  1, 0,0,0,0, 3 },
    //{ NULL, NULL,65536-16,  1, 0,0,0,0, 4 },
    //{ NULL, NULL,131072-16,  1, 0,0,0,0, 5 },
    { NULL, NULL,   0,  0, 0,0,0,0, 0 }
};

#define NBLOCKS(order)          (sizes[order].nblocks)
#define BLOCKSIZE(order)        (sizes[order].size)
#define AREASIZE(order)		(PAGE_SIZE<<(sizes[order].gfporder))
#define PAGE_MASK           (~(PGSIZE-1))

long later_alloc_init ()
{
    int order;

    for (order = 0;BLOCKSIZE(order);order++)
    {
        if ((NBLOCKS (order)*BLOCKSIZE(order) + sizeof (struct page_descriptor)) >
                AREASIZE(order)) 
        {
            printk ("Cannot use %d bytes out of %d in order = %d block mallocs\n",
                NBLOCKS (order) * BLOCKSIZE(order) + 
                sizeof (struct page_descriptor),
                (int) AREASIZE(order),
                BLOCKSIZE (order));
            panic ("This only happens if someone messes with kmalloc");
        }
    }
    return 0;
}

int get_order (int size)
{
    int order;

    /* Add the size of the header */
    size += sizeof(struct block_header); 
    for (order = 0; BLOCKSIZE(order); order++)
        if (size <= BLOCKSIZE (order))
            return order; 
    return -1;
}


void *later_kmalloc (size_t size, int priority)
{
    unsigned long flags;
    int order,tries,i,sz;
    int dma_flag;
    struct block_header *p;
    struct page_descriptor *page;

    // TODO: dma_flag = (priority & GFP_DMA);
    dma_flag = 0;
    // priority &= GFP_LEVEL_MASK;

    order = get_order (size);
    if (order < 0) {
        panic("kmalloc of too large a block");
        printk ("kmalloc of too large a block (%d bytes).\n",size);
        return (NULL);
    }

    save_flags(flags);	// save into falgs

    /* It seems VERY unlikely to me that it would be possible that this 
       loop will get executed more than once. */
    tries = MAX_GET_FREE_PAGE_TRIES; 
    while (tries --)
    {
        /* Try to allocate a "recently" freed memory block */
        // cli ();
        if ((page = (dma_flag ? sizes[order].dmafree : sizes[order].firstfree)) &&
                (p = page->firstfree))
        {
            if (p->bh_flags == MF_FREE)
            {
                page->firstfree = p->bh_next;
                page->nfree--;
                if (!page->nfree)
                {
                    sizes[order].firstfree = page->next;
                    page->next = NULL;
                }
                restore_flags(flags);

                sizes [order].nmallocs++;
                sizes [order].nbytesmalloced += size;
                p->bh_flags =  MF_USED; /* As of now this block is officially in use */
                p->bh_length = size;
                return p+1; /* Pointer arithmetic: increments past header */
            }
            panic("Problem: block on freelist is not free");
            printk ("Problem: block on freelist at %08lx isn't free.\n",(long)p);
            return (NULL);
        }
        restore_flags(flags);

        sz = BLOCKSIZE(order); /* sz is the size of the blocks we're dealing with */

        /* This can be done with ints on: This is private to this invocation */

        if (dma_flag) {
            // page = (struct page_descriptor *) __get_dma_pages (priority & GFP_LEVEL_MASK, sizes[order].gfporder);
            panic("dma_flag is on");
        }
        else  {
            page = (struct page_descriptor *)(uint32_t)pgalloc();
            // page = (struct page_descriptor *) __get_free_pages (priority & GFP_LEVEL_MASK, sizes[order].gfporder);
        }

        if (!page) {
            /*static unsigned long last = 0;
              if (last + 10*HZ < jiffies) {
              last = jiffies;
              printk ("Couldn't get a free page.....\n");
              }*/
            return NULL;
        }
#ifdef MZYDEBUG
        printk ("Got page %08x to use for %d byte mallocs....",(long)page,sz);
#endif
        sizes[order].npages++;

        /* Loop for all but last block: */
        for (i=NBLOCKS(order),p=BH (page+1);i > 1;i--,p=p->bh_next) 
        {
            p->bh_flags = MF_FREE;
            p->bh_next = BH ( ((long)p)+sz);
        }
        /* Last block: */
        p->bh_flags = MF_FREE;
        p->bh_next = NULL;

        page->order = order;
        page->nfree = NBLOCKS(order); 
        page->firstfree = BH(page+1);
#ifdef MZYDEBUG
        printk ("%d blocks per page\n",page->nfree);
#endif
        /* Now we're going to muck with the "global" freelist for this size:
           this should be uninterruptible */
        // cli ();
        /* 
         * sizes[order].firstfree used to be NULL, otherwise we wouldn't be
         * here, but you never know.... 
         */
        page->next = sizes[order].firstfree;
        if (dma_flag)
            sizes[order].dmafree = page;
        else
            sizes[order].firstfree = page;
        restore_flags(flags);
    }

    /* Pray that printk won't cause this to happen again :-) */

    printk ("Hey. This is very funny. I tried %d times to allocate a whole\n"
            "new page for an object only %d bytes long, but some other process\n"
            "beat me to actually allocating it. Also note that this 'error'\n"
            "message is soooo very long to catch your attention. I'd appreciate\n"
            "it if you'd be so kind as to report what conditions caused this to\n"
            "the author of this kmalloc: wolff@dutecai.et.tudelft.nl.\n"
            "(Executive summary: This can't happen)\n", 
            MAX_GET_FREE_PAGE_TRIES,
            size);
    return NULL;
}

void later_kfree (void *ptr,int size)
{
    unsigned long flags;
    int order;
    register struct block_header *p = ((struct block_header *)ptr) - 1;
    struct page_descriptor *page,*pg2;

    page = PAGE_DESC (p);
    order = page->order;
    if ((order < 0) || 
            (order > sizeof (sizes)/sizeof (sizes[0])) ||
            (((long)(page->next)) & ~PAGE_MASK) ||
            (p->bh_flags != MF_USED))
    {
        printk ("kfree of non-kmalloced memory: %p, next= %p, order=%d\n",
            p, page->next, page->order
        );
        return;
    }
    if (size &&
            size != p->bh_length)
    {
        printk ("Trying to free pointer at %p with wrong size: %d instead of %lu.\n",
            p, size, p->bh_length
        );
        return;
    }

    size = p->bh_length;
    p->bh_flags = MF_FREE; /* As of now this block is officially free */
    save_flags(flags);
    // cli();
    p->bh_next = page->firstfree;
    page->firstfree = p;
    page->nfree ++;

    if (page->nfree == 1)
    { /* Page went from full to one free block: put it on the freelist.  Do not bother
         trying to put it on the DMA list. */
        if (page->next)
        {
            printk ("Page %p already on freelist dazed and confused....\n", page);
        }
        else
        {
            page->next = sizes[order].firstfree;
            sizes[order].firstfree = page;
        }
    }

    /* If page is completely free, free it */
    if (page->nfree == NBLOCKS (page->order))
    {
#ifdef MZYDEBUG
        printk ("Freeing page %08x.\n", (long)page);
#endif
        if (sizes[order].firstfree == page)
        {
            sizes[order].firstfree = page->next;
        }
        else if (sizes[order].dmafree == page)
        {
            sizes[order].dmafree = page->next;
        }
        else
        {
            for (pg2=sizes[order].firstfree;
                    (pg2 != NULL) && (pg2->next != page);
                    pg2=pg2->next
            )
                /* Nothing */;
            if (!pg2)
                for (pg2=sizes[order].dmafree;
                        (pg2 != NULL) && (pg2->next != page);
                        pg2=pg2->next
            )
                 /* Nothing */;
            if (pg2 != NULL)
                pg2->next = page->next;
            else
                printk ("Ooops. page %p doesn't show on freelist.\n", page);
        }
        /* FIXME: I'm sure we should do something with npages here (like npages--) */
        pgfree((addr_t)(uint32_t)page);
    }
    restore_flags(flags);

    /* FIXME: ?? Are these increment & decrement operations guaranteed to be
     *	     atomic? Could an IRQ not occur between the read & the write?
     *	     Maybe yes on a x86 with GCC...??
     */
    sizes[order].nfrees++;      /* Noncritical (monitoring) admin stuff */
    sizes[order].nbytesmalloced -= size;
}

size_t bundle_simple_size(void *paddr) {
    // use block header to get size
    struct block_header *p = (struct block_header *)paddr - 1;
    return p->bh_length;
}

void *bundle_kmalloc(size_t size, gfp_t flags) {
    void *ret = later_kmalloc(size, 0);
    if(flags & GFP_ZERO) {
        //TODO: memset
    }
    return ret;
}

void bundle_kfree(void *paddr) {
    later_kfree(paddr, bundle_simple_size(paddr));
}


static struct simple_allocator later_simple_allocator = {
    .alloc  = bundle_kmalloc,
    .free   = bundle_kfree,
    .size   = bundle_simple_size
};

void master_later_alloc() {
    later_alloc_init();
    set_simple_allocator(&later_simple_allocator);
}