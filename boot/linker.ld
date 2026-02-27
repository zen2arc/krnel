/* krnel linker */
ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* multiboot header MUST be at the beginning, okay? */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
        *(.text.*)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* discord?? */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
    }
}
