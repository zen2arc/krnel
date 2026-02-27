#include "kernel.h"

#define VIDEO_MEMORY 0xB8000
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define ATTRIBUTE 0x0F

static int cursor_x = 0;
static int cursor_y = 0;

static void update_cursor(void) {
    u16 pos = cursor_y * SCREEN_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

void clear_screen(void) {
    volatile u16* video = (volatile u16*)VIDEO_MEMORY;

    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        video[i] = (ATTRIBUTE << 8) | ' ';
    }

    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void putchar(char c, u8 color) {
    volatile u16* video = (volatile u16*)VIDEO_MEMORY;

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            video[cursor_y * SCREEN_WIDTH + cursor_x] = (color << 8) | ' ';
        }
    } else if (c >= 32 && c <= 126) {
        video[cursor_y * SCREEN_WIDTH + cursor_x] = (color << 8) | c;
        cursor_x++;
    }

    if (cursor_x >= SCREEN_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= SCREEN_HEIGHT) {
        for (int i = 0; i < SCREEN_WIDTH * (SCREEN_HEIGHT - 1); i++) {
            video[i] = video[i + SCREEN_WIDTH];
        }

        for (int i = 0; i < SCREEN_WIDTH; i++) {
            video[(SCREEN_HEIGHT - 1) * SCREEN_WIDTH + i] = (ATTRIBUTE << 8) | ' ';
        }

        cursor_y = SCREEN_HEIGHT - 1;
    }

    update_cursor();
}

void vga_write(const char* str, u8 color) {
    if (!str) return;

    while (*str) {
        putchar(*str++, color);
    }
}

void vga_get_pos(int* x, int* y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

void vga_set_pos(int x, int y) {
    if (x < 0) x = 0;
    if (x >= SCREEN_WIDTH) x = SCREEN_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= SCREEN_HEIGHT) y = SCREEN_HEIGHT - 1;

    cursor_x = x;
    cursor_y = y;
    update_cursor();
}
