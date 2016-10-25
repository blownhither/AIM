
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <libc/string.h>
#include <aim/panic.h>


#define GFP_LEVEL_MASK 0xf

/* I want this low enough for a while to catch errors.
   I want this number to be increased in the near future:
        loadable device drivers should use this function to get memory */

#define MAX_KMALLOC_K ((PAGE_SIZE<<(NUM_AREA_ORDERS-1))>>10)


/* This defines how many times we should try to allocate a free page before
   giving up. Normally this shouldn't happen at all. */
#define MAX_GET_FREE_PAGE_TRIES 4


/* Private flags. */

#define MF_USED 0xffaa0055
#define MF_FREE 0x0055ffaa


/* 
 * Much care has gone into making these routines in this file reentrant.
 *
 * The fancy bookkeeping of nbytesmalloced and the like are only used to
 * report them to the user (oooohhhhh, aaaaahhhhh....) are not 
 * protected by cli(). (If that goes wrong. So what?)
 *
 * These routines restore the interrupt status to allow calling with ints
 * off. 
 */

/* 
 * A block header. This is in front of every malloc-block, whether free or not.
 */
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
	int nfrees;
	int nbytesmalloced;
	int npages;
	unsigned long gfporder; /* number of pages in the area required */
};

/*
 * For now it is unsafe to allocate bucket sizes between n & n=16 where n is
 * 4096 * any power of two
 */

struct size_descriptor sizes[] = { 
	{ NULL, NULL,  32,127, 0,0,0,0, 0},
	{ NULL, NULL,  64, 63, 0,0,0,0, 0 },
	{ NULL, NULL, 128, 31, 0,0,0,0, 0 },
	{ NULL, NULL, 252, 16, 0,0,0,0, 0 },
	{ NULL, NULL, 508,  8, 0,0,0,0, 0 },
	{ NULL, NULL,1020,  4, 0,0,0,0, 0 },
	{ NULL, NULL,2040,  2, 0,0,0,0, 0 },
	{ NULL, NULL,4096-16,  1, 0,0,0,0, 0 },
	{ NULL, NULL,8192-16,  1, 0,0,0,0, 1 },
	{ NULL, NULL,16384-16,  1, 0,0,0,0, 2 },
	{ NULL, NULL,32768-16,  1, 0,0,0,0, 3 },
	{ NULL, NULL,65536-16,  1, 0,0,0,0, 4 },
	{ NULL, NULL,131072-16,  1, 0,0,0,0, 5 },
	{ NULL, NULL,   0,  0, 0,0,0,0, 0 }
};


#define NBLOCKS(order)          (sizes[order].nblocks)
#define BLOCKSIZE(order)        (sizes[order].size)
#define AREASIZE(order)		(PAGE_SIZE<<(sizes[order].gfporder))


long kmalloc_init (long start_mem,long end_mem)
{
	int order;

/* 
 * Check the static info array. Things will blow up terribly if it's
 * incorrect. This is a late "compile time" check.....
 */
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
return start_mem;
}



int get_order (int size)
{
	int order;

	/* Add the size of the header */
	size += sizeof (struct block_header); 
	for (order = 0;BLOCKSIZE(order);order++)
		if (size <= BLOCKSIZE (order))
			return order; 
	return -1;
}