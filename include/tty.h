#ifndef TTY_H
#define TTY_H

#include "kernel.h"

#define MAX_TTYS 4
#define TTY_BUF_SIZE 1024
#define SCREEN_SIZE (80 * 25 * 2)

struct tty {
    int id;
    char input_buf[TTY_BUF_SIZE];
    int in_head, in_tail;
    uint16_t screen_buf[SCREEN_SIZE / 2];
    int cursor_x, cursor_y;
    int active;
};

extern struct tty ttys[MAX_TTYS];
extern int current_tty;

void tty_init(void);
void tty_switch(int n);
void tty_write(const char* str, u8 color);
int tty_read(char* buf, usize size);
int tty_getc(void);
void tty_feed_key(int key);

#endif
