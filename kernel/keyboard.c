#include "kernel.h"
#include "tty.h"

#define KEY_LEFT_VAL   -30
#define KEY_RIGHT_VAL  -31
#define KEY_UP_VAL     -32
#define KEY_DOWN_VAL   -33
#define KEY_DELETE_VAL -34
#define KEY_HOME_VAL   -35
#define KEY_END_VAL    -36

#define I8042_DATA    0x60
#define I8042_STATUS  0x64
#define I8042_CMD     0x64

#define I8042_SR_OBF  0x01   /* output buffer full  */
#define I8042_SR_IBF  0x02   /* input  buffer full  */

static int shift_pressed = 0;
static int caps_lock     = 0;
static int ctrl_pressed  = 0;
static int alt_pressed   = 0;

/* US QWERTY keymaps */
static const char keymap_normal[128] = {
    0,   0,  '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0
};

static const char keymap_shift[128] = {
    0,   0,  '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ', 0
};

static void kb_wait_write(void) {
    int t = 100000;
    while (--t && (inb(I8042_STATUS) & I8042_SR_IBF));
}
static void kb_wait_read(void) {
    int t = 100000;
    while (--t && !(inb(I8042_STATUS) & I8042_SR_OBF));
}
static void kb_flush(void) {
    int t = 32;
    while (--t && (inb(I8042_STATUS) & I8042_SR_OBF))
        inb(I8042_DATA);
}

void keyboard_init(void) {
    kb_wait_write(); outb(I8042_CMD, 0xAD);  /* disable port 1 */
    kb_wait_write(); outb(I8042_CMD, 0xA7);  /* disable port 2 (if any) */

    kb_flush();

    kb_wait_write(); outb(I8042_CMD, 0x20);
    kb_wait_read();
    u8 cfg = inb(I8042_DATA);

    cfg |=  0x01;   /* enable keyboard interrupt */
    cfg &= ~0x40;   /* disable scan-code translation (keep set-1 raw) */

    kb_wait_write(); outb(I8042_CMD, 0x60);
    kb_wait_write(); outb(I8042_DATA, cfg);

    kb_wait_write(); outb(I8042_CMD, 0xAE);

    kb_wait_write(); outb(I8042_DATA, 0xF0);   /* set scan-code set */
    kb_wait_read();  inb(I8042_DATA);           /* ACK */
    kb_wait_write(); outb(I8042_DATA, 0x01);   /* set 1 */
    kb_wait_read();  inb(I8042_DATA);           /* ACK */

    kb_wait_write(); outb(I8042_DATA, 0xF4);
    kb_wait_read();  inb(I8042_DATA);           /* ACK */
}

int key_available(void) {
    return (int)(inb(I8042_STATUS) & I8042_SR_OBF);
}

int read_key(void) {
    while (1) {
        while (!key_available()) { /* spin */ }

        u8 sc = inb(I8042_DATA);

        /* key release */
        if (sc & 0x80) {
            u8 code = sc & 0x7F;
            if (code == 0x2A || code == 0x36) shift_pressed = 0;
            if (code == 0x1D) ctrl_pressed = 0;
            if (code == 0x38) alt_pressed  = 0;
            continue;
        }

        /* extended scan-code prefix (E0 xx) */
        if (sc == 0xE0) {
            /* wait for the second byte */
            while (!key_available()) { /* spin */ }
            u8 ext = inb(I8042_DATA);
            if (ext & 0x80) continue;   /* release of extended key — ignore */
            switch (ext) {
                case 0x48: return KEY_UP_VAL;
                case 0x50: return KEY_DOWN_VAL;
                case 0x4B: return KEY_LEFT_VAL;
                case 0x4D: return KEY_RIGHT_VAL;
                case 0x47: return KEY_HOME_VAL;
                case 0x4F: return KEY_END_VAL;
                case 0x53: return KEY_DELETE_VAL;
                /* Ctrl+Right / Ctrl+Left (word jump — treat as plain arrow) */
                case 0x1D: ctrl_pressed = 1; continue;
                default:   continue;
            }
        }

        /* modifier keys */
        if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; continue; }
        if (sc == 0x1D)               { ctrl_pressed  = 1; continue; }
        if (sc == 0x38)               { alt_pressed   = 1; continue; }
        if (sc == 0x3A)               { caps_lock = !caps_lock; continue; }

        /* Alt+F1–F4 → TTY switch */
        if (alt_pressed && sc >= 0x3B && sc <= 0x3E) {
            tty_switch(sc - 0x3B); continue;
        }
        /* Alt+1–4 → TTY switch (compact keyboards) */
        if (alt_pressed && sc >= 0x02 && sc <= 0x05) {
            tty_switch(sc - 0x02); continue;
        }

        /* normal character */
        int use_shift = shift_pressed ^ caps_lock;
        char c = use_shift ? keymap_shift[sc] : keymap_normal[sc];
        if (!c) continue;

        /* Ctrl+letter → control character */
        if (ctrl_pressed && c >= 'a' && c <= 'z') return c - 'a' + 1;
        if (ctrl_pressed && c >= 'A' && c <= 'Z') return c - 'A' + 1;

        tty_feed_key((int)c);
        return (int)(u8)c;
    }
}

int get_shift_state(void) { return shift_pressed; }
int get_caps_state(void)  { return caps_lock;     }
