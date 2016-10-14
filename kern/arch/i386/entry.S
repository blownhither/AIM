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

.text

.globl master_early_entry

.globl kern_entry
kern_entry = master_early_entry - KERN_BASE

master_early_entry:
    cli     // no interrupt
    cld

/* //TODO: these are updated in new repo
mov	$kstack_top, %esp
xor	%eax, %eax
push	%eax		// eip=0
push	%eax		// ebp=0
mov	%esp, %ebp
*/

    mov	$__kstack_top - KERN_BASE, %esp
    xor	%eax, %eax
    push	%eax		// eip=0
    push	%eax		// ebp=0
    mov %esp, %ebp
    call clear_bss_kern

    call master_early_init

    pop %ebp
    ret
    
#include "aim/mmu.h"
#include "cr.h"

.globl set_control_registers
set_control_registers:
    // Turn on page size extension for 4Mbyte pages
    movl    %cr4, %eax
    orl     $(CR4_PSE), %eax
    movl    %eax, %cr4
    // Set page directory
    movl    $entrypgdir-KERN_BASE, %eax
    movl    %eax, %cr3

    // Turn on paging.
    movl    %cr0, %eax
    orl     $(CR0_PG|CR0_WP), %eax
    movl    %eax, %cr0

    // Set up the stack pointer.
    // movl    $__kstack_top, %esp
    add     $KERN_BASE, %esp

    xor	    %eax, %eax
    push	%eax		// eip=0
    push	%eax		// ebp=0
    
    // Jump and switch to executing at
    // high addresses. The indirect call is needed because
    // the assembler produces a PC-relative instruction
    // for a direct jump.
    mov $inf_loop, %eax
    jmp *%eax
     
