#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "asm.h"

.code16  

.globl memory_detection

memory_detection:
    
    xor     %bp, %bp
    
    mov     $0xe820, %eax   // Function code
    xor     %ebx, %ebx      // Continuation
    mov     $0x9000, %di    // Pointer to buffer
    mov     $0x999, %ecx    // Buffer size
    mov     $0x0534D4150, %edx  // Signature
    int     $0x15 

    mov     $0x0534D4150, %edx  // Signature
    cmp     %eax, %edx          // check signature
    jne     failed
    test    %ebx, %ebx      // unexpected end of list
    je      failed
    jmp     jmpin

e820lp:
    mov     $0xe820, %eax
    movl    $0x1, 0x20(%di)
    mov     $24, %ecx
    int     $0x15
    jc      end_detection
    mov     $0x0534D4150, %edx

jmpin:
    jcxz    skipent
    cmp     $20, %cl
    jbe     notext
    testl   $0x1, 0x20(%di)
    je      skipent

notext:
    mov     0x8(%edi), %ecx
    or      0x12(%edi), %ecx
    jz      skipent
    inc     %bp
    add     $24, %edi

skipent:
    test    %ebx, %ebx
    jne     e820lp

end_detection:
    //TODO: mov mmap_end, %bp
    clc
    ret
failed:
    jmp failed

    