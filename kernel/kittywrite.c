#include "kernel.h"

#define EDITOR_BUFFER_SIZE 4096

static int vga_get_y(void) {
    int x, y;
    vga_get_pos(&x, &y);
    return y;
}

/* disable stack checking for this function */
__attribute__((no_stack_protector))
void kittywrite(const char* filename) {
    if (!filename) {
        vga_write("Usage: kittywrite <filename>\n", 0x04);
        return;
    }

    vga_write("\nkittywrite! catful of a text editor\n", 0x0F);
    vga_write("use Ctrl+S to save, Ctrl+Q to quit\n", 0x0F);
    vga_write("yo are editing: ", 0x0F);
    vga_write(filename, 0x0A);
    vga_write("\n", 0x0F);
    vga_write("----------------------------------------\n", 0x0F);

    char buffer[EDITOR_BUFFER_SIZE];
    int file_size = 0;

    if (fs_exists(filename)) {
        file_size = fs_read(filename, buffer, sizeof(buffer) - 1);
        if (file_size > 0) {
            buffer[file_size] = '\0';
        } else {
            buffer[0] = '\0';
            file_size = 0;
        }
    } else {
        buffer[0] = '\0';
        file_size = 0;
    }

    if (file_size > 0) {
        vga_write(buffer, 0x0F);
    } else {
        vga_write("(New file)\n", 0x0E);
    }

    int cursor = file_size;
    int modified = 0;
    int start_y = vga_get_y();

    while (1) {
        int c = read_key();

        if (c == 19) { /* Ctrl+S */
            if (fs_write(filename, buffer, strlen(buffer)) > 0) {
                vga_write("\nyay, file saved!\n", 0x0A);
                modified = 0;
            } else {
                vga_write("\nawh, save failed!\n", 0x04);
            }
            break;
        } else if (c == 17) { /* Ctrl+Q */
            if (modified) {
                vga_write("\nfile was modified. wanna save? (y/n): ", 0x0E);
                int confirm = read_key();
                vga_write("\n", 0x0F);

                if (confirm == 'y' || confirm == 'Y') {
                    if (fs_write(filename, buffer, strlen(buffer)) > 0) {
                        vga_write("yay, file saved!\n", 0x0A);
                    } else {
                        vga_write("awh, save failed!\n", 0x04);
                    }
                } else {
                    vga_write("changes were discarded.\n", 0x0E);
                }
            }
            break;
        } else if (c == '\n') {
            if (cursor < EDITOR_BUFFER_SIZE - 2) {
                memmove(buffer + cursor + 1, buffer + cursor,
                       strlen(buffer + cursor) + 1);
                buffer[cursor] = '\n';
                cursor++;
                modified = 1;
                vga_write("\n", 0x0F);
            }
        } else if (c == '\b') {
            if (cursor > 0) {
                memmove(buffer + cursor - 1, buffer + cursor,
                       strlen(buffer + cursor) + 1);
                cursor--;
                modified = 1;

                int x, y;
                vga_get_pos(&x, &y);
                if (x > 0) {
                    vga_set_pos(x - 1, y);
                } else if (y > start_y) {
                    vga_set_pos(79, y - 1);
                }
                vga_write(" ", 0x0F);
                vga_get_pos(&x, &y);
                vga_set_pos(x - 1, y);
            }
        } else if (c >= 32 && c <= 126) {
            if (cursor < EDITOR_BUFFER_SIZE - 2) {
                memmove(buffer + cursor + 1, buffer + cursor,
                       strlen(buffer + cursor) + 1);
                buffer[cursor] = (char)c;
                cursor++;
                modified = 1;

                char str[2] = {(char)c, '\0'};
                vga_write(str, 0x0F);
            }
        }
    }

    vga_write("\n----------------------------------------\n", 0x0F);
    vga_write("kittywrite closed. goodbye!\n", 0x0F);
}
