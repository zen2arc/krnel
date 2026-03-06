#include "kernel.h"

u32 system_uptime = 0;

static void init_write_sysfiles(void) {
    if (!fs_exists("/etc/os-release")) {
        const char* os_release =
            "NAME=kTTY\n"
            "VERSION=" KTTY_VERSION "\n"
            "ID=ktty\n"
            "ID_LIKE=unix\n"
            "PRETTY_NAME=\"kTTY " KTTY_VERSION "\"\n"
            "VERSION_ID=" KTTY_VERSION "\n"
            "HOME_URL=https://github.com/zen2arc/kTTY\n"
            "KERNEL_VERSION=" KRNEL_VERSION "\n";
        fs_write("/etc/os-release", os_release, strlen(os_release));
    }

    /* /etc/hostname */
    if (!fs_exists("/etc/hostname")) {
        const char* hostname = "ktty";
        fs_write("/etc/hostname", hostname, strlen(hostname));
    }

    /* /etc/motd — message of the day */
    if (!fs_exists("/etc/motd")) {
        const char* motd =
            "Welcome to kTTY " KTTY_VERSION "!\n"
            "Type 'help' for available commands.\n";
        fs_write("/etc/motd", motd, strlen(motd));
    }

    /* /etc/shells */
    if (!fs_exists("/etc/shells")) {
        const char* shells = "/bin/ash\n/bin/sh\n";
        fs_write("/etc/shells", shells, strlen(shells));
    }
}

static void print_boot_banner(void) {
    vga_write("[    0.000000] kTTY " KTTY_VERSION
              " (" KRNEL_VERSION_STR ") booting\n", COLOUR_LIGHT_GRAY);
}

__attribute__((force_align_arg_pointer))
void kmain(unsigned int magic, unsigned int mb_info_addr) {
    (void)magic;
    (void)mb_info_addr;

    mem_init();
    print_boot_banner();
    vga_write("[    0.001] memory initialized\n",    COLOUR_LIGHT_GRAY);

    fs_init();
    vga_write("[    0.020] filesystem mounted\n",    COLOUR_LIGHT_GRAY);

    init_write_sysfiles();
    vga_write("[    0.025] system files written\n",  COLOUR_LIGHT_GRAY);

    vfs_init();
    vga_write("[    0.030] vfs layer ready\n",       COLOUR_LIGHT_GRAY);

    user_init();
    vga_write("[    0.035] user system initialized\n", COLOUR_LIGHT_GRAY);

    tty_init();
    vga_write("[    0.040] tty initialized\n",       COLOUR_LIGHT_GRAY);

    proc_init();
    vga_write("[    0.045] process table ready\n",   COLOUR_LIGHT_GRAY);

    plugins_init();
    vga_write("[    0.050] plugins loaded\n",        COLOUR_LIGHT_GRAY);

    vga_write("[    0.055] init complete — starting shell\n\n",
              COLOUR_LIGHT_GREEN);

    shell_init();

    shell_run();

    /* should never reach here */
    khang();
}
