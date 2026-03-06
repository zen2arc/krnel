#include "kernel.h"

#define DEFAULT_ATTR 0x0F

static int cursor_x = 0;
static int cursor_y = 0;

static void update_cursor(void) {
    u16 pos = (u16)(cursor_y * SCREEN_WIDTH + cursor_x);
    outb(0x3D4, 0x0F); outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

static void scroll_up(void) {
    volatile u16* video = (volatile u16*)VIDEO_MEMORY;
    for (int i = 0; i < SCREEN_WIDTH * (SCREEN_HEIGHT - 1); i++)
        video[i] = video[i + SCREEN_WIDTH];
    for (int i = 0; i < SCREEN_WIDTH; i++)
        video[(SCREEN_HEIGHT - 1) * SCREEN_WIDTH + i] = (DEFAULT_ATTR << 8) | ' ';
    cursor_y = SCREEN_HEIGHT - 1;
}

void clear_screen(void) {
    volatile u16* video = (volatile u16*)VIDEO_MEMORY;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
        video[i] = (DEFAULT_ATTR << 8) | ' ';
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void vga_clear_row(int y) {
    volatile u16* video = (volatile u16*)VIDEO_MEMORY;
    if (y < 0 || y >= SCREEN_HEIGHT) return;
    for (int i = 0; i < SCREEN_WIDTH; i++)
        video[y * SCREEN_WIDTH + i] = (DEFAULT_ATTR << 8) | ' ';
}

void putchar(char c, u8 color) {
    volatile u16* video = (volatile u16*)VIDEO_MEMORY;

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            video[cursor_y * SCREEN_WIDTH + cursor_x] = ((u16)color << 8) | ' ';
        }
    } else if (c >= 32) {
        video[cursor_y * SCREEN_WIDTH + cursor_x] = ((u16)color << 8) | (u8)c;
        cursor_x++;
    }

    if (cursor_x >= SCREEN_WIDTH) { cursor_x = 0; cursor_y++; }
    if (cursor_y >= SCREEN_HEIGHT) scroll_up();

    update_cursor();
}

void vga_write(const char* str, u8 color) {
    if (!str) return;
    while (*str) putchar(*str++, color);
}

void vga_write_rgb(const char* str, u8 r, u8 g, u8 b) {
    u8 color;
    int bright = (r > 127 || g > 127 || b > 127);
    if (r >= g && r >= b)      color = bright ? COLOUR_LIGHT_RED   : COLOUR_RED;
    else if (g >= r && g >= b) color = bright ? COLOUR_LIGHT_GREEN : COLOUR_GREEN;
    else                       color = bright ? COLOUR_LIGHT_BLUE  : COLOUR_BLUE;
    vga_write(str, color);
}

void vga_get_pos(int* x, int* y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

void vga_set_pos(int x, int y) {
    if (x < 0) x = 0;
    if (x >= SCREEN_WIDTH)  x = SCREEN_WIDTH  - 1;
    if (y < 0) y = 0;
    if (y >= SCREEN_HEIGHT) y = SCREEN_HEIGHT - 1;
    cursor_x = x;
    cursor_y = y;
    update_cursor();
}

void vga_put_char_at(int x, int y, char c, u8 color) {
    if (x < 0 || x >= SCREEN_WIDTH)  return;
    if (y < 0 || y >= SCREEN_HEIGHT) return;
    volatile u16* video = (volatile u16*)VIDEO_MEMORY;
    video[y * SCREEN_WIDTH + x] = ((u16)color << 8) | (u8)c;
}

void vga_draw_row(int y, int start_x, int width, const char* str, int len, u8 color) {
    if (y < 0 || y >= SCREEN_HEIGHT) return;
    if (start_x < 0) start_x = 0;
    if (start_x + width > SCREEN_WIDTH) width = SCREEN_WIDTH - start_x;
    volatile u16* video = (volatile u16*)VIDEO_MEMORY;
    for (int i = 0; i < width; i++) {
        char ch = (i < len) ? str[i] : ' ';
        video[y * SCREEN_WIDTH + start_x + i] = ((u16)color << 8) | (u8)ch;
    }
}
