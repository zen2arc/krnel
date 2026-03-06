#include "tty.h"
#include "kernel.h"
#include "shell.h"

struct tty ttys[MAX_TTYS];
int current_tty = 0;

static void tty_save_screen(struct tty* t) {
    volatile u16* vga = (volatile u16*)VIDEO_MEMORY;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
        t->screen_buf[i] = vga[i];
    vga_get_pos(&t->cursor_x, &t->cursor_y);
}

static void tty_restore_screen(struct tty* t) {
    volatile u16* vga = (volatile u16*)VIDEO_MEMORY;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
        vga[i] = t->screen_buf[i];
    vga_set_pos(t->cursor_x, t->cursor_y);
}

void tty_init(void) {
    for (int i = 0; i < MAX_TTYS; i++) {
        ttys[i].id       = i;
        ttys[i].in_head  = 0;
        ttys[i].in_tail  = 0;
        ttys[i].cursor_x = 0;
        ttys[i].cursor_y = 0;
        ttys[i].active   = (i == 0);

        for (int j = 0; j < SCREEN_WIDTH * SCREEN_HEIGHT; j++)
            ttys[i].screen_buf[j] = (COLOUR_WHITE << 8) | ' ';
    }

    tty_save_screen(&ttys[0]);
}

void tty_switch(int n) {
    if (n < 0 || n >= MAX_TTYS || n == current_tty) return;

    tty_save_screen(&ttys[current_tty]);
    current_tty = n;
    tty_restore_screen(&ttys[current_tty]);

    if (shell_logged_in()) {
        shell_prompt_redraw();
    } else {
        shell_init();
    }
}

void tty_write(const char* str, u8 color) {
    if (!str) return;
    while (*str) putchar(*str++, color);
}

int tty_getc(void) {
    struct tty* t = &ttys[current_tty];
    if (t->in_head == t->in_tail) return 0;
    int c = (u8)t->input_buf[t->in_tail];
    t->in_tail = (t->in_tail + 1) % TTY_BUF_SIZE;
    return c;
}

int tty_read(char* buf, usize size) {
    if (!buf || size == 0) return 0;
    usize i = 0;
    while (i < size - 1) {
        /* block until a character arrives */
        int c;
        while ((c = tty_getc()) == 0) { /* spin */ }
        if (c == '\n' || c == '\r') { break; }
        buf[i++] = (char)c;
    }
    buf[i] = '\0';
    return (int)i;
}

void tty_feed_key(int key) {
    struct tty* t = &ttys[current_tty];
    int next = (t->in_head + 1) % TTY_BUF_SIZE;
    if (next == t->in_tail) return;  /* buffer full, drop */
    t->input_buf[t->in_head] = (char)(key & 0xFF);
    t->in_head = next;
}
