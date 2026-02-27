#include "kernel.h"

/* key state */
static int shift_pressed = 0;
static int caps_lock = 0;
static int ctrl_pressed = 0;
static int alt_pressed = 0;

/* keymap */
static const char keymap_normal[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

static const char keymap_shift[128] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

int key_available(void) {
    return inb(0x64) & 0x01;
}

int read_key(void) {
    while (1) {
        if (key_available()) {
            u8 scancode = inb(0x60);

            if (scancode & 0x80) {
                u8 keycode = scancode & 0x7F;

                if (keycode == 0x2A || keycode == 0x36) {
                    shift_pressed = 0;
                } else if (keycode == 0x1D) {  /* Left Ctrl */
                    ctrl_pressed = 0;
                } else if (keycode == 0x38) {  /* Left Alt */
                    alt_pressed = 0;
                }
                continue;
            }

            if (scancode == 0x2A || scancode == 0x36) {
                shift_pressed = 1;
                continue;
            } else if (scancode == 0x1D) {  /* Left Ctrl */
                ctrl_pressed = 1;
                continue;
            } else if (scancode == 0x38) {  /* Left Alt */
                alt_pressed = 1;
                continue;
            } else if (scancode == 0x3A) {  /* Caps Lock */
                caps_lock = !caps_lock;
                continue;
            }

            if (scancode < 128) {
                char c = 0;
                int use_shift = shift_pressed ^ caps_lock;

                if (use_shift) {
                    c = keymap_shift[scancode];
                } else {
                    c = keymap_normal[scancode];
                }

                if (ctrl_pressed && c >= 'a' && c <= 'z') {
                    return c - 'a' + 1;
                }

                if (ctrl_pressed && c == '[') {
                    return 27;
                }

                if (c != 0) {
                    return (unsigned char)c;
                }
            }
        }
    }
}

int get_shift_state(void) {
    return shift_pressed;
}

int get_caps_state(void) {
    return caps_lock;
}

int get_ctrl_state(void) {
    return ctrl_pressed;
}

int get_alt_state(void) {
    return alt_pressed;
}
