#include "kernel.h"

#define KW_MAX_LINES    200
#define KW_MAX_COL      (SCREEN_WIDTH - 1)   /* chars per line */
#define KW_HEADER_ROWS  2
#define KW_FOOTER_ROWS  1
#define KW_EDIT_ROWS    (SCREEN_HEIGHT - KW_HEADER_ROWS - KW_FOOTER_ROWS)

static char kw_lines[KW_MAX_LINES][SCREEN_WIDTH];  /* each line NUL-terminated */
static int  kw_num_lines;
static int  kw_cur_line;
static int  kw_cur_col;
static int  kw_scroll;
static int  kw_modified;

static void kw_place_cursor(void) {
    int screen_y = KW_HEADER_ROWS + (kw_cur_line - kw_scroll);
    vga_set_pos(kw_cur_col, screen_y);
}

static void kw_draw_header(const char* filename) {
    char title[SCREEN_WIDTH + 1];
    snprintf(title, SCREEN_WIDTH + 1, " kittywrite | %s%s",
             filename, kw_modified ? " [+]" : "");
    /* pad to full width */
    int len = (int)strlen(title);
    for (int i = len; i < SCREEN_WIDTH; i++) title[i] = ' ';
    title[SCREEN_WIDTH] = '\0';
    for (int i = 0; i < SCREEN_WIDTH; i++)
        vga_put_char_at(i, 0, title[i], 0x1F);  /* white on blue */
}

/* Draw the hint bar (row 1) */
static void kw_draw_hints(void) {
    const char* hints = " Ctrl+S save  Ctrl+Q quit  Arrows move  Home/End";
    int len = (int)strlen(hints);
    for (int i = 0; i < SCREEN_WIDTH; i++)
        vga_put_char_at(i, 1, i < len ? hints[i] : ' ', 0x08);  /* dark gray on black */
}

static void kw_draw_status(void) {
    char buf[SCREEN_WIDTH + 1];
    snprintf(buf, SCREEN_WIDTH + 1, " Ln %-3d  Col %-3d  Lines: %d",
             kw_cur_line + 1, kw_cur_col + 1, kw_num_lines);
    int len = (int)strlen(buf);
    for (int i = 0; i < SCREEN_WIDTH; i++)
        vga_put_char_at(i, SCREEN_HEIGHT - 1, i < len ? buf[i] : ' ', 0x30);  /* black on cyan */
}

static void kw_draw_line(int line_idx) {
    int screen_y = KW_HEADER_ROWS + (line_idx - kw_scroll);
    if (screen_y < KW_HEADER_ROWS || screen_y >= SCREEN_HEIGHT - KW_FOOTER_ROWS) return;
    const char* text = (line_idx < kw_num_lines) ? kw_lines[line_idx] : "";
    int len = (int)strlen(text);
    vga_draw_row(screen_y, 0, SCREEN_WIDTH, text, len, COLOUR_WHITE);
}

static void kw_draw_all(const char* filename) {
    kw_draw_header(filename);
    kw_draw_hints();
    for (int i = kw_scroll; i < kw_scroll + KW_EDIT_ROWS && i <= kw_num_lines; i++)
        kw_draw_line(i);
    kw_draw_status();
    kw_place_cursor();
}

static void kw_ensure_visible(const char* filename) {
    int changed = 0;
    if (kw_cur_line < kw_scroll) {
        kw_scroll = kw_cur_line; changed = 1;
    } else if (kw_cur_line >= kw_scroll + KW_EDIT_ROWS) {
        kw_scroll = kw_cur_line - KW_EDIT_ROWS + 1;
        if (kw_scroll < 0) kw_scroll = 0;
        changed = 1;
    }
    if (changed) kw_draw_all(filename);
}

static void kw_load(const char* filename) {
    static char raw[KW_MAX_LINES * SCREEN_WIDTH];
    kw_num_lines = 1;
    kw_cur_line  = 0;
    kw_cur_col   = 0;
    kw_scroll    = 0;
    kw_modified  = 0;
    kw_lines[0][0] = '\0';

    if (!fs_exists(filename)) return;

    int sz = fs_read(filename, raw, (usize)(sizeof(raw) - 1));
    if (sz <= 0) return;
    raw[sz] = '\0';

    kw_num_lines = 0;
    char* p = raw;
    while (*p && kw_num_lines < KW_MAX_LINES) {
        char* nl  = strchr(p, '\n');
        usize len = nl ? (usize)(nl - p) : strlen(p);
        if (len >= (usize)SCREEN_WIDTH) len = (usize)(SCREEN_WIDTH - 1);
        memcpy(kw_lines[kw_num_lines], p, len);
        kw_lines[kw_num_lines][len] = '\0';
        kw_num_lines++;
        if (!nl) break;
        p = nl + 1;
    }
    if (kw_num_lines == 0) { kw_lines[0][0] = '\0'; kw_num_lines = 1; }
}

static int kw_save(const char* filename) {
    static char raw[KW_MAX_LINES * (SCREEN_WIDTH + 1)];
    int pos = 0;
    for (int i = 0; i < kw_num_lines; i++) {
        int ll = (int)strlen(kw_lines[i]);
        memcpy(raw + pos, kw_lines[i], (usize)ll);
        pos += ll;
        raw[pos++] = '\n';
    }
    return fs_write(filename, raw, (usize)pos);
}

__attribute__((no_stack_protector))
void kittywrite(const char* filename) {
    if (!filename) { vga_write("Usage: kittywrite <filename>\n", COLOUR_LIGHT_RED); return; }

    /* save the current VGA screen so we can restore after exit */
    kw_load(filename);

    /* blank the whole screen cleanly before drawing editor UI */
    for (int y = 0; y < SCREEN_HEIGHT; y++)
        vga_draw_row(y, 0, SCREEN_WIDTH, "", 0, COLOUR_WHITE);

    kw_draw_all(filename);

    while (1) {
        int c = read_key();

        if (c == 19) {
            if (kw_save(filename) > 0) {
                kw_modified = 0;
                kw_draw_header(filename);
                /* flash status */
                const char* msg = " Saved!";
                int ml = (int)strlen(msg);
                for (int i = 0; i < SCREEN_WIDTH; i++)
                    vga_put_char_at(i, SCREEN_HEIGHT-1, i < ml ? msg[i] : ' ', 0x20);
            } else {
                const char* msg = " Save FAILED!";
                int ml = (int)strlen(msg);
                for (int i = 0; i < SCREEN_WIDTH; i++)
                    vga_put_char_at(i, SCREEN_HEIGHT-1, i < ml ? msg[i] : ' ', 0x40);
            }
            kw_place_cursor();
            continue;
        }

        if (c == 17) {
            if (kw_modified) {
                const char* msg = " Unsaved changes. Save? (y/n): ";
                int ml = (int)strlen(msg);
                for (int i = 0; i < SCREEN_WIDTH; i++)
                    vga_put_char_at(i, SCREEN_HEIGHT-1, i < ml ? msg[i] : ' ', 0x30);
                kw_place_cursor();
                int ans = read_key();
                if (ans == 'y' || ans == 'Y') kw_save(filename);
            }
            break;
        }

        if (c == KEY_LEFT) {
            if (kw_cur_col > 0) {
                kw_cur_col--;
            } else if (kw_cur_line > 0) {
                kw_cur_line--;
                kw_cur_col = (int)strlen(kw_lines[kw_cur_line]);
                kw_ensure_visible(filename);
            }
            kw_draw_status();
            kw_place_cursor();
            continue;
        }

        if (c == KEY_RIGHT) {
            int ll = (int)strlen(kw_lines[kw_cur_line]);
            if (kw_cur_col < ll) {
                kw_cur_col++;
            } else if (kw_cur_line < kw_num_lines - 1) {
                kw_cur_line++;
                kw_cur_col = 0;
                kw_ensure_visible(filename);
            }
            kw_draw_status();
            kw_place_cursor();
            continue;
        }

        if (c == KEY_UP) {
            if (kw_cur_line > 0) {
                kw_cur_line--;
                int ll = (int)strlen(kw_lines[kw_cur_line]);
                if (kw_cur_col > ll) kw_cur_col = ll;
                kw_ensure_visible(filename);
                kw_draw_status();
                kw_place_cursor();
            }
            continue;
        }

        if (c == KEY_DOWN) {
            if (kw_cur_line < kw_num_lines - 1) {
                kw_cur_line++;
                int ll = (int)strlen(kw_lines[kw_cur_line]);
                if (kw_cur_col > ll) kw_cur_col = ll;
                kw_ensure_visible(filename);
                kw_draw_status();
                kw_place_cursor();
            }
            continue;
        }

        if (c == KEY_HOME) { kw_cur_col = 0; kw_draw_status(); kw_place_cursor(); continue; }

        if (c == KEY_END) {
            kw_cur_col = (int)strlen(kw_lines[kw_cur_line]);
            kw_draw_status(); kw_place_cursor();
            continue;
        }

        if (c == '\n' || c == '\r') {
            if (kw_num_lines >= KW_MAX_LINES) continue;
            /* shift lines below down */
            for (int i = kw_num_lines; i > kw_cur_line + 1; i--)
                memcpy(kw_lines[i], kw_lines[i-1], (usize)SCREEN_WIDTH);
            /* copy tail of current line to new line */
            char* cur = kw_lines[kw_cur_line];
            char* nxt = kw_lines[kw_cur_line + 1];
            strcpy(nxt, cur + kw_cur_col);
            cur[kw_cur_col] = '\0';
            kw_num_lines++;
            kw_cur_line++;
            kw_cur_col  = 0;
            kw_modified = 1;
            kw_ensure_visible(filename);
            kw_draw_line(kw_cur_line - 1);
            kw_draw_line(kw_cur_line);
            kw_draw_header(filename);
            kw_draw_status();
            kw_place_cursor();
            continue;
        }

        if (c == '\b') {
            char* cur = kw_lines[kw_cur_line];
            if (kw_cur_col > 0) {
                int ll = (int)strlen(cur);
                memmove(cur + kw_cur_col - 1, cur + kw_cur_col,
                        (usize)(ll - kw_cur_col + 1));
                kw_cur_col--;
                kw_modified = 1;
                kw_draw_line(kw_cur_line);
                kw_draw_header(filename);
                kw_draw_status();
                kw_place_cursor();
            } else if (kw_cur_line > 0) {
                /* merge current line into previous */
                char* prev = kw_lines[kw_cur_line - 1];
                int   pl   = (int)strlen(prev);
                int   cl   = (int)strlen(cur);
                if (pl + cl < KW_MAX_COL) {
                    memcpy(prev + pl, cur, (usize)(cl + 1));
                    for (int i = kw_cur_line; i < kw_num_lines - 1; i++)
                        memcpy(kw_lines[i], kw_lines[i+1], (usize)SCREEN_WIDTH);
                    kw_num_lines--;
                    kw_cur_line--;
                    kw_cur_col  = pl;
                    kw_modified = 1;
                    kw_ensure_visible(filename);
                    kw_draw_all(filename);
                }
            }
            continue;
        }

        if (c == KEY_DELETE) {
            char* cur = kw_lines[kw_cur_line];
            int   ll  = (int)strlen(cur);
            if (kw_cur_col < ll) {
                memmove(cur + kw_cur_col, cur + kw_cur_col + 1,
                        (usize)(ll - kw_cur_col));
                kw_modified = 1;
                kw_draw_line(kw_cur_line);
                kw_draw_header(filename);
                kw_draw_status();
                kw_place_cursor();
            } else if (kw_cur_line < kw_num_lines - 1) {
                /* merge next line */
                char* nxt = kw_lines[kw_cur_line + 1];
                int   nl  = (int)strlen(nxt);
                if (ll + nl < KW_MAX_COL) {
                    memcpy(cur + ll, nxt, (usize)(nl + 1));
                    for (int i = kw_cur_line + 1; i < kw_num_lines - 1; i++)
                        memcpy(kw_lines[i], kw_lines[i+1], (usize)SCREEN_WIDTH);
                    kw_num_lines--;
                    kw_modified = 1;
                    kw_draw_all(filename);
                }
            }
            continue;
        }

        if (c >= 32 && c <= 126) {
            char* cur = kw_lines[kw_cur_line];
            int   ll  = (int)strlen(cur);
            if (ll < KW_MAX_COL - 1) {
                memmove(cur + kw_cur_col + 1, cur + kw_cur_col,
                        (usize)(ll - kw_cur_col + 1));
                cur[kw_cur_col] = (char)c;
                kw_cur_col++;
                kw_modified = 1;
                kw_draw_line(kw_cur_line);
                kw_draw_header(filename);
                kw_draw_status();
                kw_place_cursor();
            }
        }
    }

    /* restore shell screen */
    clear_screen();
    vga_write("kittywrite: closed\n", COLOUR_DARK_GRAY);
}
