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

#ifndef _ARCH_MMU_H
#define _ARCH_MMU_H

#ifndef __ASSEMBLER__

#define PHYSTOP 0xE000000   

// same thing with four names
typedef uint32_t	pde_t;
typedef uint32_t	pte_t;
typedef uint8_t     vaddr_t;

typedef pde_t	pgindex_t;

typedef union page_directory_entry {
    struct{
        uint32_t present    :1;
        uint32_t writable   :1;
        uint32_t user       :1;
        uint32_t write_through  :1;
        uint32_t cache_disable  :1;
        uint32_t accessed   :1;
        uint32_t dirty      :1;
        uint32_t            :2;
        uint32_t system_use :3;
        uint32_t PPN        :20;    // physical page number
    };
    pgindex_t val;
} pgdir_ent_t;

#include "arch-boot.h"
static inline void *memset (void *addr, int data, size_t len) {
    stosb(addr, data, len);
    return addr;
}

// the following macro is from xv6
// A virtual address 'la' has a three-part structure as follows:
//
// +--------10------+-------10-------+---------12----------+
// | Page Directory |   Page Table   | Offset within Page  |
// |      Index     |      Index     |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(va) --/ \--- PTX(va) --/

// page directory index
#define PDX(va)         (((uint32_t)(va) >> PDXSHIFT) & 0x3FF)

// page table index
#define PTX(va)         (((uint32_t)(va) >> PTXSHIFT) & 0x3FF)

// construct virtual address from indexes and offset
#define PGADDR(d, t, o) ((uint32_t)((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))

// Page directory and page table constants.
// #define NPDENTRIES      1024    // # directory entries per page directory
// #define NPTENTRIES      1024    // # PTEs per page table
#define PDE_PER_PD      1024
#define PTE_PER_PT      1024
#define PGSIZE          4096    // bytes mapped by a page

#define PGSHIFT         12      // log2(PGSIZE)
#define PTXSHIFT        12      // offset of PTX in a linear address
#define PDXSHIFT        22      // offset of PDX in a linear address

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

// Page table/directory entry flags.
#define PTE_P           0x001   // Present
#define PTE_W           0x002   // Writeable
#define PTE_U           0x004   // User
#define PTE_PWT         0x008   // Write-Through
#define PTE_PCD         0x010   // Cache-Disable
#define PTE_A           0x020   // Accessed
#define PTE_D           0x040   // Dirty
#define PTE_PS          0x080   // Page Size
#define PTE_MBZ         0x180   // Bits must be zero

// Address in page table or page directory entry
#define PTE_ADDR(pte)   ((uint32_t)(pte) & ~0xFFF)
#define PTE_FLAGS(pte)  ((uint32_t)(pte) &  0xFFF)

#endif /* !__ASSEMBLER__ */

#endif /* !_ARCH_MMU_H */

