bits 32

section .multiboot
align 4
multiboot_header:
    dd 0x1BADB002
    dd 0x00000003
    dd -(0x1BADB002 + 0x00000003)

section .text
global _start
extern kmain

_start:
    mov esp, stack_top
    push ebx
    push eax
    cld
    call kmain
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits
