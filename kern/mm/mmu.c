/* Copyright (C) 2016 David Gao <davidgao1001@gmail.com>
 *
 * This file is part of AIM.
 *
 * AIM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AIM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <aim/early_kmmap.h>
#include <aim/mmu.h>
#include <aim/panic.h>

/*
 * This source file provides upper-level utilities to handle memory mappings.
 * On different systems, memory management unit (MMU)s may look different,
 * may have different names and interfaces, or may even be absent (like MIPS).
 * From the kernel's point of view, we want to unify their access interface,
 * so this wrapper is here.
 */

/*
 * It may take a lot of configuration logic to decide how early mappings should
 * be done. This leads to even more trouble when we mark these mappings
 * in proper kernel data structures later.
 * AIM uses a very simple queue located in .bss to solve the problem:
 * Early initialization routines submit mappings, and the platform-independent
 * routines will call underlying platform-dependent ones to apply them.
 * These data structure are kept in memory, and will later be used to
 * initialize the page allocator and the kmmap subsystem.
 */

int page_index_init(pgindex_t *boot_page_index)
{
	struct early_mapping *mapping = early_mapping_next(NULL);
	int ret;

	page_index_clear(boot_page_index);

	for (; mapping != NULL; mapping = early_mapping_next(mapping)) {
		ret = page_index_early_map(boot_page_index, mapping->paddr,
			(void *)mapping->vaddr, mapping->size);
		if (ret == EOF) return EOF;
	}
	return 0;
}

/* handlers after mmu start and after jump */
#define MMU_HANDLER_QUEUE_LENGTH	10
static int __mmu_handler_queue_size;
static generic_fp __mmu_handler_queue[MMU_HANDLER_QUEUE_LENGTH];

void mmu_handlers_clear(void)
{
	__mmu_handler_queue_size = 0;
}

int mmu_handlers_add(generic_fp entry)
{
	if (__mmu_handler_queue_size > MMU_HANDLER_QUEUE_LENGTH) {
		/* Bad data structure. Panic immediately to prevent damage. */
		panic("MMU handler data structure invalid.\n");
	}
	if (__mmu_handler_queue_size == MMU_HANDLER_QUEUE_LENGTH) {
		/* Queue full */
		return EOF;
	}
	__mmu_handler_queue[__mmu_handler_queue_size] = entry;
	__mmu_handler_queue_size += 1;
	return 0;
}

void mmu_handlers_apply(void)
{
	for (int i = 0; i < __mmu_handler_queue_size; ++i) {
		__mmu_handler_queue[i]();
	}
}

addr_t *kallov() {
    //TODO: implement
    return NULL;
}



// Get or alloc a page table in given pagedir 
static pte_t* walk_page_dir(pgindex_t *pgindex, vaddr_t *vaddr, int alloc) {
    pde_t *pde = (pde_t *)&pgindex[PDX(vaddr)];
    pte_t *pt;
    
    if(*pde & PTE_P){
            pt = (pte_t*)postmap_addr(PTE_ADDR(*pde));  //PTE_ADDR + KERN_BASE
    } else {
        if(!alloc || (pt = (pte_t*)kalloc()) == 0)
            return 0;
        memset(pt, 0, PGSIZE);  // static inline in arch-mmu.h
        *pde = premap_addr(pt) | PTE_P | PTE_W | PTE_U;
    }
    return &pt[PTX(vaddr)];
    
    
}

/* Map virtual address starting at @vaddr to physical pages at @paddr, with
 * VMA flags @flags (VMA_READ, etc.) Additional flags apply as in kmmap.h.
 */
int map_pages
    (
    pgindex_t *pgindex, void *vaddr, addr_t paddr, size_t size,
    uint32_t flags
    ) 
{
    //TODO: Assume similar function with xv6 mappages
    
    vaddr_t *va = (vaddr_t *)PGROUNDDOWN((uint32_t)vaddr);
    vaddr_t *end = (vaddr_t *)(PGROUNDDOWN((uint32_t)vaddr) + size - 1);
    pte_t *pte;
    for(; va <= end; va += PGSIZE) {
        if((pte = walk_page_dir(pgindex, va, 1)) == 0) 
            return -1;  // fail to get page dir
        if(*pte & PTE_P)
            panic("remap in map_pages");
        *pte = paddr | PTE_FLAGS(flags) | PTE_P;    //TODO: why P?
        paddr += PGSIZE;
        
    }
    //TODO: VMA_READ?! and other flags?

    return 0;
}


