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

#include "sys/types.h"
// Adapt absolute types from types.h, arch-free
typedef uint8_t uchar;
typedef uint32_t uint;
typedef uint16_t ushort;

/** An example of hdr defination from arch-boot.h's, arch-free
 * // Adapt header struct from elf.h
 * typedef elf32hdr_t elfhdr;
 * typedef elf32_phdr_t proghdr;
 **/

#include "aim/boot.h"

// Size of single sector
#define SECTSIZE  512   
// Physical sector number offset in MBR entry
#define P_SEC_OFFS 8   

// Pointer to mbr entrys at fixed addr
uint8_t *mbr;

// the following codes are arch-free when out and in is properly replaced
static void
waitdisk(void)
{
    // Wait for disk ready.
    while((inb(0x1F7) & 0xC0) != 0x40)
        ;
}

// Read a single sector at offset into dst.
static void readsect(void *dst, uint offset)
{
    // Issue command.
    waitdisk();
    outb(0x1F2, 1);   // count = 1
    outb(0x1F3, offset);
    outb(0x1F4, offset >> 8);
    outb(0x1F5, offset >> 16);
    outb(0x1F6, (offset >> 24) | 0xE0);
    outb(0x1F7, 0x20);  // cmd 0x20 - read sectors

    // Read data.
    waitdisk();
    insl(0x1F0, dst, SECTSIZE/4);
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked.
static void
readseg(uchar* pa, uint count, uint offset)
{
    uchar* epa;

    epa = pa + count;

    // Round down to sector boundary.
    pa -= offset % SECTSIZE;

    // Translate from bytes to sectors; kernel starts at sector 1.
    offset = (offset / SECTSIZE);

    // If this is too slow, we could read lots of sectors at a time.
    // We'd write more to memory than asked, but it doesn't matter --
    // we load in increasing order.
    for(; pa < epa; pa += SECTSIZE, offset++)
        readsect(pa, offset);
}


/**
 * Analyze elf at 2nd partition, load prog segments into memory,
 * clean bss and call its entry
 **/
void bootmain(void)
{
    elfhdr *elf;
    proghdr *ph, *eph;
    void (*entry)(void);
    uchar* pa;

    mbr = (uint8_t *)(0x7c00 + 0x1ce);      // 2nd partition
    uint32_t hd_offs = (*(uint32_t *)(mbr + P_SEC_OFFS)) * SECTSIZE; 
                                            // calc disk offs

    elf = (elfhdr*)0x10000;                 // scratch space

    // Read at least all headers into buffer
    readseg((uint8_t*)elf, SECTSIZE * 16, hd_offs);

    // check MAGIC (using bit op to avoid pointer aliasing)
    // uint32_t magic = 0;
    // magic = elf->e_ident[3] << 24 | elf->e_ident[2] << 16 
    //   | elf->e_ident[1] << 8 | elf->e_ident[0];
    // if(magic != ELF_MAGIC)
    //    return;                             // bootasm.S will be in inf loop

    // Load each prog seg (ignores ph flags).
    ph = (proghdr*)((uchar*)elf + elf->e_phoff);
    // End of prog hdr
    eph = ph + elf->e_phnum;

    for(; ph < eph; ph++){
        // Assigned memory addr
        pa = (uchar*)ph->p_paddr;
        // Read seg from disk
        readseg(pa, ph->p_filesz, ph->p_offset + hd_offs);
        // Clean .bss
        if(ph->p_memsz > ph->p_filesz)
            stosb(pa + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
    }

    // Call the entry point from the ELF header.
    // Does not return! (jmp)
    entry = (void(*)(void))(elf->e_entry);
    entry();
}




