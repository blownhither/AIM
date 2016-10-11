#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <aim/early_kmmap.h>
#include <aim/mmu.h>
#include <aim/panic.h>

addr_t* kalloc(void);

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

