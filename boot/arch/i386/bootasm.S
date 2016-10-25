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

#include "asm.h"

// reduced from mmu.h, x86 memory management
#define CR0_PE 0x00000001 
#define SEG_KCODE 1
#define SEG_KDATA 2

.code16  
.globl bootasm  
bootasm:
    cli                 // no interrupt
    xorw    %ax, %ax    // plain data seg 
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
                        // set cs in the future

seta20.1:		        // shut down A20
    inb     $0x64, %al
    testb   $0x2, %al
    jnz     seta20.1
    movb    $0xd1, %al
    outb    %al, $0x64

seta20.2:
    inb     $0x64, %al
    testb   $0x2, %al
    jnz     seta20.2
    movb    $0xdf, %al
    outb    %al, $0x60

    push    %ebp
    // call    memory_detection
    pop     %ebp
    call    protected_mode  // no return

protected_mode:

    lgdt    gdtdesc     // load global descrptor table
    movl    %cr0, %eax  // switch to protected mode
    orl     $CR0_PE, %eax
    movl    %eax, %cr0
                        // set cs
    ljmp   $(SEG_KCODE<<3), $start32
    
.code32
start32:
    movw    $(SEG_KDATA<<3), %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    $0, %ax
    movw    %ax, %fs
    movw    %ax, %gs


    movl    $bootasm, %esp 
    call    bootmain    // $esp = 0x7bfc

loop_boot:
    jmp     loop_boot

.p2align 2              // align 2^2 B
gdt:
    SEG_NULLASM
    SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)
    SEG_ASM(STA_W, 0x0, 0xffffffff)
gdtdesc:
    .word (gdtdesc - gdt - 1)
    .long gdt
