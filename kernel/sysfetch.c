#include "kernel.h"
#include "plugin.h"

extern u32 system_uptime;

// forward declaration so shell.c can see it
void sysfetch_run(void);

static const char* ascii_art[] = {
    "  +++++            %%%%      ............  ............  .....  .....",
    "   ++++          %%%%%%%%    ............  ............  .....  .....",
    "   ++++          %%%%%%%%    ... .... ...  ... .... ...  .....  .....",
    "   ++++  +++++   %%%%%%%%        ....          ....      ............",
    "   +++++++++       %%%%          ....          ....        ........",
    "   ++++++++        %%%%          ....          ....          ....",
    "   ++++  +++                     ....          ....          ....",
    "   ++++   ++++     %%%%          ....          ....          ....",
    "  +++++   ++++     %%%%        ........      ........      ........",
    NULL
};

void sysfetch_run(void) {
    for (int i = 0; ascii_art[i]; i++) {
        vga_write(ascii_art[i], 0x0C);
        vga_write("\n", 0x0F);
    }

    vga_write("\n", 0x0F);
    vga_write("-------------------\n", 0x07);
    vga_write("OS:      kTTY (32-bit)\n", 0x0F);
    vga_write("Kernel:  krnel\n", 0x0F);
    vga_write("Kernel version:  v0.3\n", 0x0F);
    vga_write("Version: v1.0.0rc1\n", 0x0F);
    vga_write("Uptime:  ", 0x0F);
    char buf[16];
    itoa(system_uptime / 100, buf);
    vga_write(buf, 0x0A);
    vga_write(" seconds\n", 0x0F);
    vga_write("Memory:  32 MB total\n", 0x0F);
    vga_write("User:    ", 0x0F);
    vga_write(user_get_name(), 0x0E);
    vga_write("\n", 0x0F);
    vga_write("Shell:   ash\n", 0x0F);
}

REGISTER_PLUGIN(sysfetch, sysfetch_run);
