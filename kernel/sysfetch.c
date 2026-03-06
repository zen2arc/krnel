#include "kernel.h"
#include "plugin.h"

extern u32 system_uptime;

void sysfetch_run(void) {
    /* ---- kitty ASCII art ---- */
    static const char* cat[] = {
        "   /\\_____/\\   ",
        "  /  o   o  \\  ",
        " ( ==  ^  == ) ",
        "  )         (  ",
        " (           ) ",
        "( (  )   (  ) )",
        "(__(__)___(__))",
        "               ",
        "               ",
        NULL
    };

    /* ---- system info fields ---- */
    char uptime_buf[32];
    char uid_buf[16];
    u32 up_sec = system_uptime / 100;
    u32 up_min = up_sec / 60;
    u32 up_hr  = up_min / 60;
    up_sec %= 60; up_min %= 60;
    if (up_hr > 0)
        snprintf(uptime_buf, sizeof(uptime_buf), "%uh %um %us", up_hr, up_min, up_sec);
    else if (up_min > 0)
        snprintf(uptime_buf, sizeof(uptime_buf), "%um %us", up_min, up_sec);
    else
        snprintf(uptime_buf, sizeof(uptime_buf), "%us", up_sec);

    snprintf(uid_buf, sizeof(uid_buf), "%u", user_get_uid());

    /* hostname */
    char hostname[64] = "ktty";
    {
        char hbuf[64];
        int  hsz = fs_read("/etc/hostname", hbuf, sizeof(hbuf) - 1);
        if (hsz > 0) {
            hbuf[hsz] = '\0';
            if (hbuf[hsz-1] == '\n') hbuf[hsz-1] = '\0';
            strncpy(hostname, hbuf, sizeof(hostname) - 1);
        }
    }

    struct { const char* label; const char* value; u8 lcolor; } info[] = {
        { "OS",      "kTTY " KTTY_VERSION,     COLOUR_LIGHT_CYAN    },
        { "Kernel",  KRNEL_VERSION_STR,        COLOUR_LIGHT_CYAN    },
        { "Shell",   "ash",                    COLOUR_LIGHT_CYAN    },
        { "Uptime",  uptime_buf,               COLOUR_LIGHT_CYAN    },
        { "Memory",  "32 MB",                  COLOUR_LIGHT_CYAN    },
        { "User",    user_get_name(),           COLOUR_LIGHT_CYAN    },
        { "UID",     uid_buf,                  COLOUR_LIGHT_CYAN    },
        { "Host",    hostname,                 COLOUR_LIGHT_CYAN    },
        { NULL,      NULL,                     0                    }
    };

    vga_write("\n", COLOUR_WHITE);

    /* header: user@host */
    vga_write("  ", COLOUR_WHITE);
    vga_write(user_get_name(), COLOUR_LIGHT_GREEN);
    vga_write("@",             COLOUR_WHITE);
    vga_write(hostname,        COLOUR_LIGHT_GREEN);
    vga_write("\n", COLOUR_WHITE);

    /* separator */
    vga_write("  ", COLOUR_WHITE);
    {
        int sep_len = (int)strlen(user_get_name()) + 1 + (int)strlen(hostname);
        for (int i = 0; i < sep_len; i++) putchar('-', COLOUR_DARK_GRAY);
    }
    vga_write("\n", COLOUR_WHITE);

    /* rows: cat art on left, info on right */
    for (int row = 0; cat[row] || info[row].label; row++) {
        /* cat column */
        if (cat[row]) {
            vga_write(cat[row], COLOUR_YELLOW);
        } else {
            vga_write("               ", COLOUR_WHITE);
        }

        /* info column */
        if (info[row].label) {
            vga_write("  ", COLOUR_WHITE);
            vga_write(info[row].label, info[row].lcolor);
            vga_write(": ", COLOUR_WHITE);
            vga_write(info[row].value, COLOUR_WHITE);
        }
        vga_write("\n", COLOUR_WHITE);
    }

    /* colour palette strip */
    vga_write("\n  ", COLOUR_WHITE);
    for (u8 col = 0; col < 8; col++)  putchar(' ', (u8)(col | (col << 4)));
    vga_write("  ", COLOUR_WHITE);
    for (u8 col = 8; col < 16; col++) putchar(' ', (u8)(col | (col << 4)));
    vga_write("\n\n", COLOUR_WHITE);
}

REGISTER_PLUGIN(sysfetch, sysfetch_run);
