#include "kernel.h"
#include "tty.h"

#define KEY_LEFT   -30
#define KEY_RIGHT  -31
#define KEY_UP     -32
#define KEY_DOWN   -33
#define KEY_DELETE -34
#define KEY_HOME   -35
#define KEY_END    -36

/* modifier state */
static int shift_pressed = 0;
static int caps_lock = 0;
static int ctrl_pressed = 0;
static int alt_pressed = 0;

/* keymaps */
static const char keymap_normal[128] = {
    0, 0, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0
};

static const char keymap_shift[128] = {
    0, 0, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' ',0
};

int key_available(void) {
    return inb(0x64) & 1;
}

int read_key(void) {
    while (1) {
        if (!key_available()) continue;

        u8 sc = inb(0x60);

        if (sc & 0x80) {
            u8 code = sc & 0x7F;

            if (code == 0x2A || code == 0x36) shift_pressed = 0;
            if (code == 0x1D) ctrl_pressed = 0;
            if (code == 0x38) alt_pressed = 0;

            continue;
        }

        if (sc == 0xE0) {
            while (!key_available());
            sc = inb(0x60);

            switch (sc) {
                case 0x4B: return KEY_LEFT;
                case 0x4D: return KEY_RIGHT;
                case 0x48: return KEY_UP;
                case 0x50: return KEY_DOWN;
                case 0x47: return KEY_HOME;
                case 0x4F: return KEY_END;
                case 0x53: return KEY_DELETE;
            }
            continue;
        }

        if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; continue; }
        if (sc == 0x1D) { ctrl_pressed = 1; continue; }
        if (sc == 0x38) { alt_pressed = 1; continue; }
        if (sc == 0x3A) { caps_lock = !caps_lock; continue; }

        /* Alt + F1-F4 */
        if (alt_pressed && sc >= 0x3B && sc <= 0x3E) {
            tty_switch(sc - 0x3B);
            continue;
        }

        /* Alt + 1-4 (for compact keyboards) */
        if (alt_pressed && sc >= 0x02 && sc <= 0x05) {
            tty_switch(sc - 0x02);
            continue;
        }

        char c = 0;
        int use_shift = shift_pressed ^ caps_lock;

        if (use_shift)
            c = keymap_shift[sc];
        else
            c = keymap_normal[sc];

        if (c) {
            tty_feed_key((int)c);
            return (int)c;
        }
    }
}

/* ===== state getters ===== */

int get_shift_state(void) { return shift_pressed; }
int get_caps_state(void) { return caps_lock; }
int get_ctrl_state(void) { return ctrl_pressed; }
int get_alt_state(void) { return alt_pressed; }
