#include "kernel.h"

static char history[HISTORY_SIZE][BUFFER_SIZE];
static int history_count = 0;
static int history_current = -1;
static int history_index = 0;

void history_add(const char* command) {
    if (!command || strlen(command) == 0) return;

    if (history_count > 0 && strcmp(history[(history_index - 1 + HISTORY_SIZE) % HISTORY_SIZE], command) == 0) {
        return;
    }

    strncpy(history[history_index], command, BUFFER_SIZE - 1);
    history[history_index][BUFFER_SIZE - 1] = '\0';

    history_index = (history_index + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) {
        history_count++;
    }

    history_current = -1;
}

void history_prev(char* buffer, usize size) {
    if (history_count == 0) return;

    if (history_current == -1) {
        history_current = history_count - 1;
    } else if (history_current > 0) {
        history_current--;
    }

    int idx = (history_index - history_count + history_current) % HISTORY_SIZE;
    if (idx < 0) idx += HISTORY_SIZE;

    strncpy(buffer, history[idx], size - 1);
    buffer[size - 1] = '\0';
}

void history_next(char* buffer, usize size) {
    if (history_current == -1) return;

    if (history_current < history_count - 1) {
        history_current++;
        int idx = (history_index - history_count + history_current) % HISTORY_SIZE;
        if (idx < 0) idx += HISTORY_SIZE;

        strncpy(buffer, history[idx], size - 1);
        buffer[size - 1] = '\0';
    } else {
        history_current = -1;
        buffer[0] = '\0';
    }
}

void history_show(void) {
    vga_write("Command history:\n", 0x0F);

    for (int i = 0; i < history_count; i++) {
        int idx = (history_index - history_count + i) % HISTORY_SIZE;
        if (idx < 0) idx += HISTORY_SIZE;

        char num[16];
        itoa(i + 1, num);
        vga_write("  ", 0x0F);
        vga_write(num, 0x0A);
        vga_write(": ", 0x0F);
        vga_write(history[idx], 0x0F);
        vga_write("\n", 0x0F);
    }
}
