#include "kernel.h"

u32 system_uptime = 0;

__attribute__((force_align_arg_pointer))
void kmain(unsigned int magic, unsigned int mb_info_addr) {
    (void)magic;
    (void)mb_info_addr;

    mem_init();
    vga_write("memory initialized\n", 0x0A);

    fs_init();
    vga_write("filesystem initialized\n", 0x0A);

    user_init();
    vga_write("user system initialized\n", 0x0A);

    shell_init();

    vga_write("welcome to krnel!\n", 0x0F);

    shell_run();

    // this should never be reached
    while (1) {
        __asm__ volatile ("hlt");
    }
}
