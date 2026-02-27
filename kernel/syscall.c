#include "kernel.h"

u64 syscall(u64 num, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5) {
    (void)a2; (void)a3; (void)a4; (void)a5; /* unused */

    switch (num) {
        case 0:
            proc_exit((int)a1);
            break;
        case 1:
            /* safe 64 to 32-bit pointer conversion */
            {
                u32 ptr = (u32)a1;
                kprint((const char*)ptr);
                return strlen((const char*)ptr);
            }
        default:
            break;
    }

    return 0;
}

void kprint(const char* str) {
    vga_write(str, 0x0F);  /* print with default white color */
}
