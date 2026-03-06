#include "kernel.h"

static user_t users[MAX_USERS];
static int    user_count   = 0;
static int    current_user = -1;
static int    first_boot   = 0;

static void users_load(void) {
    char buffer[4096];
    int size = fs_read("/etc/passwd", buffer, sizeof(buffer) - 1);
    if (size <= 0) { first_boot = 1; return; }
    buffer[size] = '\0';
    user_count = 0;

    char* line = buffer;
    char* next;
    while (line && *line && user_count < MAX_USERS) {
        next = strchr(line, '\n');
        if (next) *next++ = '\0';

        char* username = strtok(line,  ":");
        char* password = strtok(NULL,  ":");
        char* uid_str  = strtok(NULL,  ":");
        char* gid_str  = strtok(NULL,  ":");
        char* root_str = strtok(NULL,  ":");
        char* home     = strtok(NULL,  ":");
        char* shell    = strtok(NULL,  ":");

        if (username && password) {
            strncpy(users[user_count].username, username, MAX_USERNAME - 1);
            strncpy(users[user_count].password, password, MAX_PASSWORD - 1);
            users[user_count].uid     = uid_str  ? (u32)atoi(uid_str)  : 0;
            users[user_count].gid     = gid_str  ? (u32)atoi(gid_str)  : 0;
            users[user_count].is_root = root_str ? (u8)atoi(root_str)  : 0;
            strncpy(users[user_count].home,  home  ? home  : "/",        sizeof(users[0].home)  - 1);
            strncpy(users[user_count].shell, shell ? shell : "/bin/ash", sizeof(users[0].shell) - 1);
            user_count++;
        }
        line = next;
    }
}

static void users_save(void) {
    char buffer[4096] = {0};
    usize pos = 0;
    for (int i = 0; i < user_count; i++) {
        pos += (usize)snprintf(buffer + pos, sizeof(buffer) - pos,
            "%s:%s:%u:%u:%d:%s:%s\n",
            users[i].username, users[i].password,
            users[i].uid,      users[i].gid,
            users[i].is_root,  users[i].home, users[i].shell);
    }
    fs_write("/etc/passwd", buffer, pos);
}

void user_init(void) {
    if (!fs_exists("/etc")) fs_mkdir("/etc");
    users_load();
    if (first_boot) user_count = 0;
}

int user_first_boot(void) {
    return first_boot && user_count == 0;
}

int user_add(const char* username, const char* password, u8 is_root) {
    if (user_count >= MAX_USERS) return -1;
    for (int i = 0; i < user_count; i++)
        if (strcmp(users[i].username, username) == 0) return -1;

    int uid = is_root ? 0 : 1000 + (user_count - 1);
    strncpy(users[user_count].username, username, MAX_USERNAME - 1);
    strncpy(users[user_count].password, password, MAX_PASSWORD - 1);
    users[user_count].uid     = (u32)uid;
    users[user_count].gid     = (u32)uid;
    users[user_count].is_root = is_root;

    if (is_root) {
        strcpy(users[user_count].home, "/root");
    } else {
        snprintf(users[user_count].home, sizeof(users[user_count].home),
                 "/home/%s", username);
    }
    strcpy(users[user_count].shell, "/bin/ash");

    /* create home dir */
    if (!is_root) {
        if (fs_create_home(username) < 0) {
            char path[64];
            snprintf(path, sizeof(path), "/home/%s", username);
            fs_mkdir(path);
        }
    }

    user_count++;
    users_save();
    return 0;
}

int user_del(const char* username) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            if (users[i].is_root) return -1;   /* can't delete root */
            if (i == current_user) user_logout();
            if (i < user_count - 1)
                memmove(&users[i], &users[i+1],
                        (usize)(user_count - i - 1) * sizeof(user_t));
            user_count--;
            users_save();
            return 0;
        }
    }
    return -1;
}

int user_login(const char* username, const char* password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0) {
            current_user = i;
            fs_chdir(users[i].home);
            return 0;
        }
    }
    return -1;
}

int  user_logout(void)  { current_user = -1; return 0; }

/* user_current: copy current username into caller's buffer */
void user_current(char* username, usize size) {
    const char* name = user_get_name();
    strncpy(username, name, size - 1);
    username[size - 1] = '\0';
}

const char* user_get_name(void) {
    return (current_user >= 0) ? users[current_user].username : "nobody";
}
const char* user_get_home(void) {
    return (current_user >= 0) ? users[current_user].home : "/";
}
u32 user_get_uid(void) {
    return (current_user >= 0) ? users[current_user].uid : 65534;
}
u8 user_is_root(void) {
    return (current_user >= 0) ? users[current_user].is_root : 0;
}

void user_debug_dump(void) {
    char buf[128];
    vga_write("User database:\n", COLOUR_WHITE);
    for (int i = 0; i < user_count; i++) {
        snprintf(buf, sizeof(buf), "  %s  uid=%u  root=%d  home=%s\n",
            users[i].username, users[i].uid,
            users[i].is_root,  users[i].home);
        vga_write(buf, COLOUR_LIGHT_GRAY);
    }
}
