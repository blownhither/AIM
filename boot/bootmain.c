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

#include <sys/types.h>
// TODO :Adapt type from types.h, TODO: use exact num?
typedef uint8_t uchar;
typedef uint32_t uint;
typedef uint16_t ushort;

#include "elf.h"
typedef elf32hdr_t elfhdr;
typedef elf32_phdr_t proghdr;

#include <aim/boot.h>

#define SECTSIZE  512   
#define SECTORSTART 8   
#define MBRSIZE 16

uint8_t *mbr;


void readseg(uchar*, uint, uint);

void bootmain(void)
{
  elfhdr *elf;
  proghdr *ph, *eph;
  void (*entry)(void);
  uchar* pa;

  mbr = (uint8_t *)(0x7c00 + 0x1ce);      // 2nd partition
  uint32_t kOffset = (*(uint32_t *)(mbr + SECTORSTART))*SECTSIZE;
  

  elf = (elfhdr*)0x10000;  // scratch space

  // Read 1st page off disk
  readseg((uint8_t*)elf, 7777, kOffset);

  // Is this an ELF executable?
  if((*(uint32_t *)(elf->e_ident)) != ELF_MAGIC)
    return;  // let bootasm.S handle error

  // Load each program segment (ignores ph flags).
  ph = (proghdr*)((uchar*)elf + elf->e_phoff);
  eph = ph + elf->e_phnum;
  for(; ph < eph; ph++){
    pa = (uchar*)ph->p_paddr;
    readseg(pa, ph->p_filesz, ph->p_offset);
    if(ph->p_memsz > ph->p_filesz)
      stosb(pa + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
  }

  // Call the entry point from the ELF header.
  // Does not return!
  entry = (void(*)(void))(elf->e_entry);
  entry();
}

void
waitdisk(void)
{
  // Wait for disk ready.
  while((inb(0x1F7) & 0xC0) != 0x40)
    ;
}

// Read a single sector at offset into dst.
void
readsect(void *dst, uint offset)
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
void
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
