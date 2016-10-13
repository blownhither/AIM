/* Copyright (C) 2016 David Gao <davidgao1001@gmail.com>
 *
 * This file is part of AIMv6.
 *
 * AIMv6 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AIMv6 is distributed in the hope that it will be useful,
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

#include "sys/types.h"

#include "aim/init.h"
#include "aim/boot.h"
#include "aim/mmu.h"
#include "aim/kalloc.h"

// The following are one-shot usage func
/*typedef struct segment_descriptor {
    uint64_t limit_12_27 :16;
    uint64_t base_0_15  :16;
    uint64_t base_16_23 :8;
    uint64_t type       :8;
    uint64_t limit_28_32:8;
    uint64_t base_24_32 :8;
} seg_desc_t;

static inline void seg_desc_fill(
        seg_desc_t *d, uint8_t type, uint32_t base, uint32_t limit
    ) 
{
    d->limit_12_27  = (limit >> 0x12) & 0xffff;
    d->base_0_15    = base & 0xffff;
    d->base_16_23   = (base >> 16) & 0xff;
    d->type         = 0x90 | (type & 0xff);
    d->limit_28_32  = 0xc0 | ((limit >> 28) & 0xf);
    d->base_24_32   = (base >> 24) & 0xff;
}
*/


extern uint32_t __bss_start_kern, __bss_end_kern;

void clear_bss_kern(){
    if (&__bss_end_kern > &__bss_start_kern)
        stosb(&__bss_start_kern, 0, 
            &__bss_end_kern - &__bss_start_kern);

}

static struct segdesc kern_gdt[NSEGS];

static void arch_load_gdt() {
    
    kern_gdt[0] = SEG(0,0,0,0);
    kern_gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
    kern_gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
    kern_gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
    kern_gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
    //TODO: kern_gdt[SEG_KCPU] = SEG(STA_W, &c->cpu, 8, 0);
    
    // lgdt a memory address
    __asm__ __volatile__ ("lgdt %0":"=m"(kern_gdt));
}

// pgindex_t entrypgdir[NPDENTRIES];

__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
  // Map VA's [0, 4MB) to PA's [0, 4MB)
  [0] = (0) | PTE_P | PTE_W | PTE_PS,
  // [1] = (1<<22) | PTE_P | PTE_W | PTE_PS,
  // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
  [0x200] = (0) | PTE_P | PTE_W | PTE_PS,
  
};

void set_control_registers();

void arch_early_init(void)
{
    arch_load_gdt();    // get a new gdt other than bootloader one
    
    // TODO: check linear page mapping
    /*
    page_index_early_map(
        (pgindex_t *)premap_addr(entrypgdir), 
        (addr_t)0, 
        (void *)KERN_BASE, 
        200<<20
    );
    page_index_early_map(
        (pgindex_t *)premap_addr(entrypgdir), 
        (addr_t)0,
        (void *)0,
        200<<20  
    );
    */
    //pgindex_t*, paddr, vaddr, size
    set_control_registers();    // also jmp to new target
    
}

void inf_loop() {
    while(1);
}

void sleep1(){
    /*  // nice try... but int80 is not yet implemented
    __asm__("mov $0x1,  %%ecx ;mov $0x0,  %%edx ;"::);
    __asm__("pushl  %%eax ;pushl  %%ebx ;pushl  %%ecx ;"::);
    __asm__("mov  %%ecx, -0x8( %%esp);mov  %%edx, -0x4( %%esp);mov $162,  %%eax ;"::);
    __asm__("lea -0x8( %%esp),  %%ebx ;xor  %%ecx,  %%ecx ;int $0x80 ;"::);
    __asm__("pop  %%ecx ;pop  %%ebx ;pop  %%eax ;ret;"::);
    */
    __asm__("cli;"::);
    while(1)
        __asm__("hlt;"::);
}

