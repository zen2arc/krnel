/* krnel linker */
ENTRY(_start)

SECTIONS
{
    . = 1M;

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

    .stack : ALIGN(16)
    {
        . = ALIGN(16);
        *(.stack)
    }

    _end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
    }
}
