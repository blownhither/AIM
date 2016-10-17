
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

.text


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
    mov $continue_early_init, %eax
    jmp *%eax
     