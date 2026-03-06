#include "kernel.h"

static user_t users[MAX_USERS];
static int    user_count    = 0;
static int    current_user  = -1;
static int    first_boot    = 0;
static int    sudo_elevated = 0;  /* set by user_sudo_elevate() */

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

        char* uname   = strtok(line, ":");
        char* pass    = strtok(NULL, ":");
        char* uid_s   = strtok(NULL, ":");
        char* gid_s   = strtok(NULL, ":");
        char* root_s  = strtok(NULL, ":");
        char* sudo_s  = strtok(NULL, ":");
        char* home_s  = strtok(NULL, ":");
        char* shell_s = strtok(NULL, ":");

        if (uname && pass) {
            strncpy(users[user_count].username, uname, MAX_USERNAME - 1);
            strncpy(users[user_count].password, pass,  MAX_PASSWORD - 1);
            users[user_count].uid     = uid_s  ? (u32)atoi(uid_s)  : 0;
            users[user_count].gid     = gid_s  ? (u32)atoi(gid_s)  : 0;
            users[user_count].is_root = root_s ? (u8)atoi(root_s)  : 0;
            users[user_count].in_sudo = sudo_s ? (u8)atoi(sudo_s)  : 0;
            if (users[user_count].is_root) {
                strcpy(users[user_count].home, "/");
            } else {
                strncpy(users[user_count].home,
                        home_s ? home_s : "/",
                        sizeof(users[0].home) - 1);
            }
            strncpy(users[user_count].shell,
                    shell_s ? shell_s : "/bin/ksh",
                    sizeof(users[0].shell) - 1);
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
            "%s:%s:%u:%u:%d:%d:%s:%s\n",
            users[i].username, users[i].password,
            users[i].uid,      users[i].gid,
            users[i].is_root,  users[i].in_sudo,
            users[i].home,     users[i].shell);
    }
    fs_write("/etc/passwd", buffer, pos);
}

void user_init(void) {
    if (!fs_exists("/etc")) fs_mkdir("/etc");
    users_load();
    if (first_boot) user_count = 0;
}

int user_first_boot(void) { return first_boot && user_count == 0; }

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
    users[user_count].in_sudo = 0;

    if (is_root) {
        strcpy(users[user_count].home, "/");
    } else {
        snprintf(users[user_count].home, sizeof(users[user_count].home),
                 "/home/%s", username);
    }
    strcpy(users[user_count].shell, "/bin/ksh");

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
            if (users[i].is_root) return -1;
            if (i == current_user) user_logout();
            if (i < user_count - 1)
                memmove(&users[i], &users[i + 1],
                        (usize)(user_count - i - 1) * sizeof(user_t));
            user_count--;
            users_save();
            return 0;
        }
    }
    return -1;
}

int user_add_sudo(const char* username) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            users[i].in_sudo = 1;
            users_save();
            return 0;
        }
    }
    return -1;
}

int user_login(const char* username, const char* password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password)  == 0) {
            current_user  = i;
            sudo_elevated = 0;
            fs_chdir(users[i].home);
            return 0;
        }
    }
    return -1;
}

int user_logout(void) {
    current_user  = -1;
    sudo_elevated = 0;
    return 0;
}

void user_current(char* username, usize size) {
    strncpy(username, user_get_name(), size - 1);
    username[size - 1] = '\0';
}

const char* user_get_name(void) {
    if (sudo_elevated) return "root";
    return (current_user >= 0) ? users[current_user].username : "nobody";
}

const char* user_get_home(void) {
    if (sudo_elevated) return "/";
    return (current_user >= 0) ? users[current_user].home : "/";
}

u32 user_get_uid(void) {
    if (sudo_elevated) return 0;
    return (current_user >= 0) ? users[current_user].uid : 65534;
}

u8 user_is_root(void) {
    if (sudo_elevated) return 1;
    return (current_user >= 0) ? users[current_user].is_root : 0;
}

int user_is_sudo(void) {
    if (current_user < 0) return 0;
    return (int)(users[current_user].is_root | users[current_user].in_sudo);
}

int user_check_password(const char* password) {
    if (current_user < 0 || !password) return 0;
    return strcmp(users[current_user].password, password) == 0;
}

void user_sudo_elevate(void) { sudo_elevated = 1; }
void user_sudo_drop(void)    { sudo_elevated = 0; }

void user_debug_dump(void) {
    char buf[128];
    vga_write("User database:\n", COLOUR_WHITE);
    for (int i = 0; i < user_count; i++) {
        snprintf(buf, sizeof(buf),
            "  %-12s uid=%-5u root=%d sudo=%d home=%s\n",
            users[i].username, users[i].uid,
            users[i].is_root,  users[i].in_sudo,
            users[i].home);
        vga_write(buf, COLOUR_LIGHT_GRAY);
    }
}

int users_real_is_root(void) {
    if (current_user < 0) return 0;
    return (int)users[current_user].is_root;
}
