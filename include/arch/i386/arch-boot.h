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

#ifndef _ARCH_BOOT_H
#define _ARCH_BOOT_H

#include "arch/i386/asm.h"

#ifndef __ASSEMBLER__



#include "elf.h"
// Adapt header struct from elf.h for i386
typedef elf32hdr_t elfhdr;
typedef elf32_phdr_t proghdr;

// MAGIC_NUMBER for x86
#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian


# else /* __ASSEMBLER__ */

// arch-free macros for assbmbler
//

#endif /* !__ASSEMBLER__ */

#endif /* !_ARCH_BOOT_H */

