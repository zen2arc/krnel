#include "kernel.h"

u32 system_uptime = 0;

__attribute__((force_align_arg_pointer))
void kmain(unsigned int magic, unsigned int mb_info_addr) {
    (void)magic;
    (void)mb_info_addr;

    mem_init();
    vga_write("krnel v0.3 - memory initialized\n", 0x0A);

    fs_init();
    vga_write("filesystem initialized\n", 0x0A);

    user_init();
    vga_write("user system initialized\n", 0x0A);

    if (user_first_boot()) {
        vga_write("first boot detected - creating root user\n", 0x0E);
        user_add("root", "root", 1);
    }

    char username[32];
    char password[32];
    int logged_in = 0;

    while (!logged_in) {
        vga_write("\nlogin: ", 0x0F);
        int pos = 0;
        while (1) {
            int c = read_key();
            if (c == '\n') {
                username[pos] = 0;
                vga_write("\n", 0x0F);
                break;
            }
            if (c == '\b' && pos > 0) {
                pos--;
                vga_write("\b \b", 0x0F);
            } else if (c >= 32 && c <= 126) {
                username[pos++] = c;
                char ch[2] = {c, 0};
                vga_write(ch, 0x0F);
            }
        }

        vga_write("password: ", 0x0F);
        pos = 0;
        while (1) {
            int c = read_key();
            if (c == '\n') {
                password[pos] = 0;
                vga_write("\n", 0x0F);
                break;
            }
            if (c == '\b' && pos > 0) {
                pos--;
                vga_write("\b \b", 0x0F);
            } else if (c >= 32 && c <= 126) {
                password[pos++] = c;
                vga_write("*", 0x0F);
            }
        }

        if (user_login(username, password) == 0) {
            vga_write("login successful\n", 0x0A);
            logged_in = 1;
        } else {
            vga_write("invalid username or password\n", 0x04);
        }
    }

    shell_init();
    vga_write("welcome to kTTY! type 'help' for commands\n", 0x0F);

    shell_run();

    /* yeah hell naw it should never reach this */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
