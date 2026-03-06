#include "kernel.h"
#include "shell.h"

#define MAX_ARGS    20
#define MAX_ALIASES 32

static char* args[MAX_ARGS];
static int   arg_count = 0;

typedef struct {
    char name[32];
    char value[128];
} alias_t;

static alias_t aliases[MAX_ALIASES];
static int     alias_count = 0;
static int     logged_in   = 0;

int  shell_logged_in(void) { return logged_in; }

static void read_string(char* buffer, int max_len, int show_chars) {
    int pos = 0;
    while (1) {
        int c = read_key();
        if (c == '\n' || c == '\r') {
            buffer[pos] = '\0';
            vga_write("\n", COLOUR_WHITE);
            return;
        }
        if (c == '\b') {
            if (pos > 0) { pos--; vga_write("\b \b", COLOUR_WHITE); }
            continue;
        }
        if (c >= 32 && c <= 126 && pos < max_len - 1) {
            buffer[pos++] = (char)c;
            if (show_chars) {
                char s[2] = {(char)c, '\0'};
                vga_write(s, COLOUR_WHITE);
            } else {
                vga_write("*", COLOUR_DARK_GRAY);
            }
        }
    }
}

static void get_hostname(char* buf, int len) {
    int sz = fs_read("/etc/hostname", buf, len - 1);
    if (sz > 0) {
        buf[sz] = '\0';
        if (buf[sz-1] == '\n') buf[sz-1] = '\0';
    } else {
        strncpy(buf, "ktty", len - 1);
        buf[len-1] = '\0';
    }
}

static void show_prompt(void) {
    const char* name = user_get_name();
    const char* home = user_get_home();
    const char* cwd  = fs_getcwd();
    u8 ucolor = user_is_root() ? COLOUR_LIGHT_RED : COLOUR_LIGHT_GREEN;

    char hostname[64];
    get_hostname(hostname, sizeof(hostname));

    /* build display path — substitute ~ for home prefix */
    char dpath[128];
    usize hlen = strlen(home);
    if (hlen > 0 && strncmp(cwd, home, hlen) == 0 &&
        (cwd[hlen] == '\0' || cwd[hlen] == '/')) {
        snprintf(dpath, sizeof(dpath), "~%s", cwd + hlen);
    } else {
        strncpy(dpath, cwd, sizeof(dpath) - 1);
        dpath[sizeof(dpath) - 1] = '\0';
    }

    vga_write(name,     ucolor);
    vga_write("@",      COLOUR_WHITE);
    vga_write(hostname, COLOUR_LIGHT_GREEN);
    vga_write(":",      COLOUR_WHITE);
    vga_write(dpath,    COLOUR_LIGHT_CYAN);
    vga_write(user_is_root() ? " # " : " % ", COLOUR_WHITE);
}

void shell_prompt_redraw(void) {
    if (!logged_in) return;
    vga_write("\n", COLOUR_WHITE);
    show_prompt();
}

static void first_boot_setup(void) {
    clear_screen();
    vga_write("kTTY " KTTY_VERSION " first boot setup\n", COLOUR_YELLOW);

    /* --- root password --- */
    char root_password[MAX_PASSWORD];
    while (1) {
        vga_write("Set root password: ", COLOUR_WHITE);
        read_string(root_password, MAX_PASSWORD, 0);
        if (strlen(root_password) > 0) break;
        vga_write("Password cannot be empty.\n", COLOUR_LIGHT_RED);
    }
    user_add("root", root_password, 1);
    vga_write("Root account created.\n\n", COLOUR_LIGHT_GREEN);

    /* --- regular user --- */
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    while (1) {
        vga_write("Create a user account\n", COLOUR_WHITE);
        vga_write("Username: ", COLOUR_WHITE);
        read_string(username, MAX_USERNAME, 1);
        vga_write("Password: ", COLOUR_WHITE);
        read_string(password, MAX_PASSWORD, 0);
        if (!strlen(username) || !strlen(password)) {
            vga_write("Username and password cannot be blank.\n", COLOUR_LIGHT_RED);
            continue;
        }
        if (user_add(username, password, 0) == 0) {
            vga_write("User created.\n", COLOUR_LIGHT_GREEN);
            break;
        }
        vga_write("Failed to create user, try again.\n", COLOUR_LIGHT_RED);
    }

    /* --- sudo group membership --- */
    char yn[4];
    char sudo_q[64];
    snprintf(sudo_q, sizeof(sudo_q),
             "Add '%s' to the sudo group? [Y/n]: ", username);
    vga_write(sudo_q, COLOUR_WHITE);
    read_string(yn, sizeof(yn), 1);
    if (yn[0] == '\0' || yn[0] == 'Y' || yn[0] == 'y') {
        user_add_sudo(username);
        vga_write("User added to sudo group.\n", COLOUR_LIGHT_GREEN);
    }

    vga_write("\nSetup complete! Please log in.\n\n", COLOUR_LIGHT_GREEN);
}

static void login_prompt(void) {
    clear_screen();
    char username[MAX_USERNAME], password[MAX_PASSWORD];
    int  attempts = 0;
    while (attempts < 3) {
        vga_write("kTTY " KTTY_VERSION " (" KRNEL_VERSION_STR ")",
                  COLOUR_WHITE);
        vga_write(" /dev/tty", COLOUR_WHITE);
        char tnum[4]; itoa(current_tty + 1, tnum);
        vga_write(tnum, COLOUR_WHITE);
        vga_write("\n\n", COLOUR_WHITE);

        vga_write("kTTY login: ", COLOUR_WHITE);
        read_string(username, MAX_USERNAME, 1);
        vga_write("Password: ", COLOUR_WHITE);
        read_string(password, MAX_PASSWORD, 0);

        if (user_login(username, password) == 0) {
            logged_in = 1;
            clear_screen();
            /* print motd — single occurrence, right here after login */
            char motd[512];
            int msz = fs_read("/etc/motd", motd, sizeof(motd) - 1);
            if (msz > 0) { motd[msz] = '\0'; vga_write(motd, COLOUR_LIGHT_CYAN); }
            return;
        }
        clear_screen();
        vga_write("Login incorrect.\n\n", COLOUR_LIGHT_RED);
        attempts++;
    }
    vga_write("Too many failed attempts. System halted.\n", COLOUR_RED);
    khang();
}

static void parse_command(char* line) {
    arg_count = 0;
    char* token = strtok(line, " \t\n");
    while (token && arg_count < MAX_ARGS - 1) {
        args[arg_count++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[arg_count] = NULL;
}

static char* expand_alias(const char* cmd) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(aliases[i].name, cmd) == 0) return aliases[i].value;
    return NULL;
}

static void add_alias(const char* name, const char* value) {
    if (alias_count >= MAX_ALIASES) {
        vga_write("alias: too many aliases\n", COLOUR_LIGHT_RED); return;
    }
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            strncpy(aliases[i].value, value, sizeof(aliases[0].value) - 1);
            return;
        }
    }
    strncpy(aliases[alias_count].name,  name,  sizeof(aliases[0].name)  - 1);
    strncpy(aliases[alias_count].value, value, sizeof(aliases[0].value) - 1);
    alias_count++;
}

static void list_aliases(void) {
    if (alias_count == 0) { vga_write("(no aliases)\n", COLOUR_DARK_GRAY); return; }
    for (int i = 0; i < alias_count; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s='%s'\n", aliases[i].name, aliases[i].value);
        vga_write(buf, COLOUR_LIGHT_CYAN);
    }
}

static void cmd_cd(char* path) {
    if (!path || !strlen(path)) {
        fs_chdir(user_get_home()); return;
    }
    /* support ~ expansion */
    char expanded[128];
    if (path[0] == '~') {
        snprintf(expanded, sizeof(expanded), "%s%s", user_get_home(), path + 1);
        path = expanded;
    }
    if (fs_chdir(path) != 0) {
        char err[96];
        snprintf(err, sizeof(err), "cd: %s: No such file or directory\n", path);
        vga_write(err, COLOUR_LIGHT_RED);
    }
}

static void cmd_ls(int show_all) {
    char buf[4096];
    if (fs_list(buf, sizeof(buf)) > 0) {
        /* colour directories differently */
        char* p = buf;
        while (*p) {
            char* nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            int is_dir = 0;
            int l = strlen(p);
            if (l > 0 && p[l-1] == '/') is_dir = 1;
            if (!show_all && p[0] == '.') { if (nl) { *nl='\n'; p=nl+1; } else break; continue; }
            vga_write(p, is_dir ? COLOUR_LIGHT_CYAN : COLOUR_WHITE);
            vga_write("\n", COLOUR_WHITE);
            if (!nl) break;
            *nl = '\n'; p = nl + 1;
        }
    } else {
        vga_write("(empty)\n", COLOUR_WHITE);
    }
}

static void cmd_cat(const char* f) {
    if (!f) { vga_write("Usage: cat <file>\n", COLOUR_LIGHT_RED); return; }
    char buf[4096];
    int sz = fs_read(f, buf, sizeof(buf) - 1);
    if (sz < 0) {
        char err[96];
        snprintf(err, sizeof(err), "cat: %s: No such file or directory\n", f);
        vga_write(err, COLOUR_LIGHT_RED); return;
    }
    buf[sz] = '\0';
    vga_write(buf, COLOUR_WHITE);
    if (sz > 0 && buf[sz-1] != '\n') vga_write("\n", COLOUR_WHITE);
}

static void cmd_touch(const char* f) {
    if (!f) { vga_write("Usage: touch <file>\n", COLOUR_LIGHT_RED); return; }
    if (fs_exists(f)) return;   /* already exists, silently ok like real touch */
    if (fs_create(f, 1) < 0) vga_write("touch: cannot create file\n", COLOUR_LIGHT_RED);
}

static void cmd_rm(void) {
    int i_start = 1;
    for (; i_start < arg_count; i_start++)
        if (args[i_start][0] != '-') break;

    if (i_start >= arg_count) {
        vga_write("Usage: rm [-f] <file> [file ...]\n", COLOUR_LIGHT_RED); return;
    }
    for (int i = i_start; i < arg_count; i++) {
        if (fs_delete(args[i]) != 0) {
            char err[96];
            snprintf(err, sizeof(err), "rm: cannot remove '%s'\n", args[i]);
            vga_write(err, COLOUR_LIGHT_RED);
        }
    }
}

static void cmd_mkdir(const char* d) {
    if (!d) { vga_write("Usage: mkdir <dir>\n", COLOUR_LIGHT_RED); return; }
    if (fs_exists(d)) { vga_write("mkdir: already exists\n", COLOUR_YELLOW); return; }
    if (fs_mkdir(d) < 0) vga_write("mkdir: cannot create directory\n", COLOUR_LIGHT_RED);
}

static void cmd_cp(void) {
    if (arg_count < 3) { vga_write("Usage: cp <src> <dst>\n", COLOUR_LIGHT_RED); return; }
    char buf[4096];
    int sz = fs_read(args[1], buf, sizeof(buf) - 1);
    if (sz < 0) {
        char err[96]; snprintf(err, sizeof(err), "cp: %s: No such file\n", args[1]);
        vga_write(err, COLOUR_LIGHT_RED); return;
    }
    buf[sz] = '\0';
    if (fs_write(args[2], buf, (usize)sz) < 0)
        vga_write("cp: write failed\n", COLOUR_LIGHT_RED);
}

static void cmd_mv(void) {
    if (arg_count < 3) { vga_write("Usage: mv <src> <dst>\n", COLOUR_LIGHT_RED); return; }
    char buf[4096];
    int sz = fs_read(args[1], buf, sizeof(buf) - 1);
    if (sz < 0) {
        char err[96]; snprintf(err, sizeof(err), "mv: %s: No such file\n", args[1]);
        vga_write(err, COLOUR_LIGHT_RED); return;
    }
    buf[sz] = '\0';
    if (fs_write(args[2], buf, (usize)sz) < 0) {
        vga_write("mv: write failed\n", COLOUR_LIGHT_RED); return;
    }
    fs_delete(args[1]);
}

static void cmd_echo(void) {
    int start = 1, newline = 1;
    if (arg_count > 1 && strcmp(args[1], "-n") == 0) { newline = 0; start = 2; }
    for (int i = start; i < arg_count; i++) {
        vga_write(args[i], COLOUR_WHITE);
        if (i < arg_count - 1) vga_write(" ", COLOUR_WHITE);
    }
    if (newline) vga_write("\n", COLOUR_WHITE);
}

/* chmod <octal> <file>  e.g.  chmod 755 myscript */
static void cmd_chmod(void) {
    if (arg_count < 3) {
        vga_write("Usage: chmod <mode> <file>\n", COLOUR_LIGHT_RED); return;
    }
    const char* mstr = args[1];
    u16 mode = 0;
    while (*mstr >= '0' && *mstr <= '7')
        mode = (u16)(mode * 8 + (*mstr++ - '0'));
    if (fs_chmod(args[2], mode) != 0) {
        char err[96];
        snprintf(err, sizeof(err), "chmod: cannot change mode of '%s'\n", args[2]);
        vga_write(err, COLOUR_LIGHT_RED);
    }
}

static void cmd_help(void) {
    vga_write("kTTY " KTTY_VERSION " ksh built-ins\n", COLOUR_YELLOW);
    vga_write("  Navigation : cd [dir]  ls [-a]  pwd\n",              COLOUR_WHITE);
    vga_write("  Files      : cat  touch  rm [-f]  mkdir  cp  mv\n",  COLOUR_WHITE);
    vga_write("  Text       : echo [-n]  kittywrite <file>\n",        COLOUR_WHITE);
    vga_write("  System     : ps  sysfetch  uname [-a]  hostname\n",  COLOUR_WHITE);
    vga_write("  Users      : id  whoami  useradd  userdel  passwd\n",COLOUR_WHITE);
    vga_write("  Privilege  : sudo <cmd>  sudo -l  sudo -i\n",        COLOUR_WHITE);
    vga_write("  Shell      : history  alias  unalias  clear  help\n",COLOUR_WHITE);
    vga_write("  Perms      : chmod <mode> <file>\n",                 COLOUR_WHITE);
    vga_write("  Session    : exit  logout\n",                        COLOUR_WHITE);
    vga_write("  Scripts    : ./script.sh (also chmodded files)\n",COLOUR_WHITE);
    vga_write("  Keys       : Up/Down=history  Left/Right/Home/End\n",COLOUR_WHITE);
}

static void cmd_uname(void) {
    if (arg_count > 1 && strcmp(args[1], "-a") == 0)
        vga_write("kTTY kTTY-" KTTY_VERSION " " KRNEL_VERSION_STR " #1 SMP x86\n", COLOUR_WHITE);
    else
        vga_write("kTTY\n", COLOUR_WHITE);
}

static void cmd_hostname(void) {
    char buf[64];
    get_hostname(buf, sizeof(buf));
    vga_write(buf, COLOUR_WHITE);
    vga_write("\n", COLOUR_WHITE);
}

static void cmd_id(void) {
    char buf[128];
    u32 uid = user_get_uid();
    const char* name = user_get_name();
    const char* groups = user_is_sudo() ? ",sudo" : "";
    snprintf(buf, sizeof(buf),
             "uid=%u(%s) gid=%u(%s) groups=%u(%s)%s\n",
             uid, name, uid, name, uid, name, groups);
    vga_write(buf, COLOUR_WHITE);
}

static void cmd_passwd(void) {
    const char* target = (arg_count > 1) ? args[1] : user_get_name();
    if (strcmp(target, user_get_name()) != 0 && !user_is_root()) {
        vga_write("passwd: permission denied\n", COLOUR_LIGHT_RED); return;
    }
    if (!user_is_root()) {
        vga_write("(current) password: ", COLOUR_WHITE);
        char cur[MAX_PASSWORD];
        read_string(cur, MAX_PASSWORD, 0);
        if (!user_check_password(cur)) {
            vga_write("passwd: Authentication failure\n", COLOUR_LIGHT_RED); return;
        }
    }
    char p1[MAX_PASSWORD], p2[MAX_PASSWORD];
    vga_write("New password: ",    COLOUR_WHITE); read_string(p1, MAX_PASSWORD, 0);
    vga_write("Retype new: ",      COLOUR_WHITE); read_string(p2, MAX_PASSWORD, 0);
    if (strcmp(p1, p2) != 0) { vga_write("passwd: passwords do not match\n", COLOUR_LIGHT_RED); return; }
    if (!strlen(p1))          { vga_write("passwd: password cannot be empty\n", COLOUR_LIGHT_RED); return; }
    vga_write("passwd: Not yet implemented\n", COLOUR_YELLOW);
}

static void cmd_exec_script(const char* path) {
    char abs[128];
    if (path[0] == '.' && path[1] == '/') {
        const char* cwd = fs_getcwd();
        if (strcmp(cwd, "/") == 0)
            snprintf(abs, sizeof(abs), "/%s", path + 2);
        else
            snprintf(abs, sizeof(abs), "%s/%s", cwd, path + 2);
    } else if (path[0] != '/') {
        const char* cwd = fs_getcwd();
        if (strcmp(cwd, "/") == 0)
            snprintf(abs, sizeof(abs), "/%s", path);
        else
            snprintf(abs, sizeof(abs), "%s/%s", cwd, path);
    } else {
        strncpy(abs, path, sizeof(abs) - 1);
        abs[sizeof(abs) - 1] = '\0';
    }

    char buf[4096];
    int sz = fs_read(abs, buf, sizeof(buf) - 1);
    if (sz < 0) {
        char err[96];
        snprintf(err, sizeof(err), "ksh: %s: No such file or directory\n", path);
        vga_write(err, COLOUR_LIGHT_RED); return;
    }
    buf[sz] = '\0';

    char* p = buf;
    /* skip shebang line if present */
    if (p[0] == '#' && p[1] == '!') {
        char* nl = strchr(p, '\n');
        if (nl) p = nl + 1; else return;
    }
    while (p && *p) {
        char* nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        while (*p == ' ' || *p == '\t') p++;
        int ll = (int)strlen(p);
        while (ll > 0 && (p[ll-1] == '\r' || p[ll-1] == ' ')) ll--;
        p[ll] = '\0';
        if (*p && *p != '#') {
            char copy[BUFFER_SIZE];
            strncpy(copy, p, sizeof(copy) - 1);
            copy[sizeof(copy) - 1] = '\0';
            execute_command(copy);
        }
        if (!nl) break;
        p = nl + 1;
    }
}

static void cmd_sudo(void) {
    if (arg_count < 2) {
        vga_write("Usage: sudo [-l] <command> [args...]\n", COLOUR_LIGHT_RED);
        return;
    }

    if (strcmp(args[1], "-l") == 0) {
        if (user_is_sudo()) {
            char buf[96];
            snprintf(buf, sizeof(buf),
                     "User %s may run the following commands: ALL\n",
                     user_get_name());
            vga_write(buf, COLOUR_WHITE);
        } else {
            char buf[96];
            snprintf(buf, sizeof(buf),
                     "User %s is not allowed to run sudo.\n",
                     user_get_name());
            vga_write(buf, COLOUR_LIGHT_RED);
        }
        return;
    }

    if (strcmp(args[1], "-i") == 0) {
        if (!user_is_sudo() && !user_is_root()) {
            char buf[80];
            snprintf(buf, sizeof(buf),
                     "%s is not in the sudoers file.\n", user_get_name());
            vga_write(buf, COLOUR_LIGHT_RED); return;
        }
        if (!user_is_root()) {
            char prompt[64];
            snprintf(prompt, sizeof(prompt),
                     "[sudo] password for %s: ", user_get_name());
            vga_write(prompt, COLOUR_WHITE);
            char pw[MAX_PASSWORD];
            read_string(pw, MAX_PASSWORD, 0);
            if (!user_check_password(pw)) {
                vga_write("sudo: Authentication failure\n", COLOUR_LIGHT_RED); return;
            }
        }
        user_sudo_elevate();
        vga_write("(sudo) Type 'exit' to drop root.\n", COLOUR_YELLOW);
        return;
    }

    if (!user_is_root()) {
        if (!user_is_sudo()) {
            char buf[96];
            snprintf(buf, sizeof(buf),
                     "%s is not in the sudoers file.  "
                     "This incident will be reported.\n",
                     user_get_name());
            vga_write(buf, COLOUR_LIGHT_RED); return;
        }
        char prompt[64];
        snprintf(prompt, sizeof(prompt),
                 "[sudo] password for %s: ", user_get_name());
        vga_write(prompt, COLOUR_WHITE);
        char pw[MAX_PASSWORD];
        read_string(pw, MAX_PASSWORD, 0);
        if (!user_check_password(pw)) {
            vga_write("sudo: Authentication failure\n", COLOUR_LIGHT_RED); return;
        }
    }

    char subcmd[BUFFER_SIZE];
    subcmd[0] = '\0';
    for (int i = 1; i < arg_count; i++) {
        int l = (int)strlen(subcmd);
        if (i > 1 && l < BUFFER_SIZE - 2) {
            subcmd[l] = ' '; subcmd[l+1] = '\0'; l++;
        }
        strncpy(subcmd + l, args[i], (usize)(BUFFER_SIZE - l - 1));
    }

    user_sudo_elevate();
    execute_command(subcmd);
    user_sudo_drop();
}

static void cmd_useradd(void) {
    if (!user_is_root()) {
        vga_write("useradd: permission denied\n", COLOUR_LIGHT_RED); return;
    }
    int add_sudo = 0;
    int uarg = 1;

    if (arg_count > 1 && strcmp(args[1], "-G") == 0) {
        if (arg_count < 3) {
            vga_write("useradd: -G requires a group name\n", COLOUR_LIGHT_RED); return;
        }
        if (strcmp(args[2], "sudo") == 0) add_sudo = 1;
        uarg = 3;
    }

    if (arg_count < uarg + 2) {
        vga_write("Usage: useradd [-G sudo] <user> <password>\n", COLOUR_LIGHT_RED);
        return;
    }

    const char* uname = args[uarg];
    const char* upass = args[uarg + 1];

    if (user_add(uname, upass, 0) != 0) {
        vga_write("useradd: failed (user exists?)\n", COLOUR_LIGHT_RED); return;
    }
    if (add_sudo) user_add_sudo(uname);

    char msg[80];
    snprintf(msg, sizeof(msg), "useradd: user '%s' created%s\n",
             uname, add_sudo ? " (sudo)" : "");
    vga_write(msg, COLOUR_LIGHT_GREEN);
}

void execute_command(char* line) {
    if (!line || !strlen(line)) return;

    while (*line == ' ' || *line == '\t') line++;
    if (!*line) return;

    history_add(line);

    char* expanded = expand_alias(line);
    if (expanded) {
        char copy[BUFFER_SIZE];
        strncpy(copy, expanded, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';
        execute_command(copy);
        return;
    }

    parse_command(line);
    if (arg_count == 0) return;
    const char* cmd = args[0];

    if      (strcmp(cmd, "cd")      == 0) cmd_cd(arg_count > 1 ? args[1] : NULL);
    else if (strcmp(cmd, "ls")      == 0) cmd_ls(arg_count > 1 && strcmp(args[1], "-a") == 0);
    else if (strcmp(cmd, "ll")      == 0) cmd_ls(0);
    else if (strcmp(cmd, "la")      == 0) cmd_ls(1);
    else if (strcmp(cmd, "cat")     == 0) cmd_cat(args[1]);
    else if (strcmp(cmd, "touch")   == 0) cmd_touch(args[1]);
    else if (strcmp(cmd, "rm")      == 0) cmd_rm();
    else if (strcmp(cmd, "mkdir")   == 0) cmd_mkdir(args[1]);
    else if (strcmp(cmd, "cp")      == 0) cmd_cp();
    else if (strcmp(cmd, "mv")      == 0) cmd_mv();
    else if (strcmp(cmd, "echo")    == 0) cmd_echo();
    else if (strcmp(cmd, "chmod")   == 0) cmd_chmod();
    else if (strcmp(cmd, "clear")   == 0) clear_screen();
    else if (strcmp(cmd, "help")    == 0 || strcmp(cmd, "?") == 0) cmd_help();
    else if (strcmp(cmd, "history") == 0) history_show();
    else if (strcmp(cmd, "uname")   == 0) cmd_uname();
    else if (strcmp(cmd, "hostname")== 0) cmd_hostname();
    else if (strcmp(cmd, "id")      == 0) cmd_id();
    else if (strcmp(cmd, "whoami")  == 0) {
        vga_write(user_get_name(), COLOUR_WHITE);
        vga_write("\n", COLOUR_WHITE);
    }
    else if (strcmp(cmd, "pwd")     == 0) {
        vga_write(fs_getcwd(), COLOUR_WHITE);
        vga_write("\n", COLOUR_WHITE);
    }
    else if (strcmp(cmd, "groups")  == 0) {
        vga_write(user_get_name(), COLOUR_WHITE);
        if (user_is_sudo()) vga_write(" sudo", COLOUR_WHITE);
        vga_write("\n", COLOUR_WHITE);
    }
    else if (strcmp(cmd, "alias")   == 0) {
        if (arg_count == 1) list_aliases();
        else if (arg_count >= 3) add_alias(args[1], args[2]);
        else vga_write("Usage: alias <name> <value>\n", COLOUR_LIGHT_RED);
    }
    else if (strcmp(cmd, "unalias") == 0) {
        if (arg_count < 2) vga_write("Usage: unalias <name>\n", COLOUR_LIGHT_RED);
        else {
            for (int i = 0; i < alias_count; i++) {
                if (strcmp(aliases[i].name, args[1]) == 0) {
                    if (i < alias_count - 1)
                        memmove(&aliases[i], &aliases[i+1],
                                (usize)(alias_count-i-1)*sizeof(alias_t));
                    alias_count--;
                    break;
                }
            }
        }
    }
    else if (strcmp(cmd, "ps")        == 0) {
        char buf[1024]; proc_get_list(buf, sizeof(buf)); vga_write(buf, COLOUR_WHITE);
    }
    else if (strcmp(cmd, "sysfetch")  == 0) sysfetch_run();
    else if (strcmp(cmd, "sudo")      == 0) cmd_sudo();
    else if (strcmp(cmd, "useradd")   == 0) cmd_useradd();
    else if (strcmp(cmd, "userdel")   == 0) {
        if (arg_count < 2)          vga_write("Usage: userdel <user>\n", COLOUR_LIGHT_RED);
        else if (!user_is_root())   vga_write("userdel: permission denied\n", COLOUR_LIGHT_RED);
        else if (user_del(args[1]) == 0) {
            char msg[64]; snprintf(msg, sizeof(msg), "userdel: '%s' removed\n", args[1]);
            vga_write(msg, COLOUR_LIGHT_GREEN);
        } else vga_write("userdel: failed\n", COLOUR_LIGHT_RED);
    }
    else if (strcmp(cmd, "passwd")    == 0) cmd_passwd();
    else if (strcmp(cmd, "kittywrite")== 0) {
        if (arg_count < 2) vga_write("Usage: kittywrite <file>\n", COLOUR_LIGHT_RED);
        else kittywrite(args[1]);
    }
    else if (strcmp(cmd, "sh")        == 0 || strcmp(cmd, "source") == 0) {
        if (arg_count < 2) vga_write("Usage: sh <script>\n", COLOUR_LIGHT_RED);
        else cmd_exec_script(args[1]);
    }
    else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "logout") == 0) {
        /* if user is in a sudo -i session, just drop elevation first */
        if (user_is_root() && !users_real_is_root()) {
            user_sudo_drop();
            vga_write("(dropped to user)\n", COLOUR_DARK_GRAY);
        } else {
            vga_write("logout\n", COLOUR_DARK_GRAY);
            user_sudo_drop();
            user_logout();
            clear_screen();
            logged_in = 0;
            login_prompt();
        }
    }
    /* ./ prefix — always try as script */
    else if (cmd[0] == '.' && cmd[1] == '/') {
        if (!fs_is_executable(cmd + 2) && !fs_is_executable(cmd)) {
            char err[96];
            snprintf(err, sizeof(err), "ksh: %s: Permission denied\n", cmd);
            vga_write(err, COLOUR_LIGHT_RED);
        } else {
            cmd_exec_script(cmd);
        }
    }
    /* bare name — only run if executable bit is set */
    else if (fs_exists(cmd)) {
        if (!fs_is_executable(cmd)) {
            char err[96];
            snprintf(err, sizeof(err), "ksh: %s: Permission denied\n", cmd);
            vga_write(err, COLOUR_LIGHT_RED);
        } else {
            cmd_exec_script(cmd);
        }
    }
    else {
        char err[96];
        snprintf(err, sizeof(err), "ksh: %s: command not found\n", cmd);
        vga_write(err, COLOUR_LIGHT_RED);
    }
}

void shell_init(void) {
    add_alias("..",   "cd ..");
    add_alias("...",  "cd ../..");
    add_alias("~",    "cd ~");
    add_alias("cls",  "clear");

    if (user_first_boot()) first_boot_setup();
    while (!logged_in) login_prompt();
}

void shell_run(void) {
    static char line[BUFFER_SIZE];

    while (logged_in) {
        show_prompt();

        int prompt_x, prompt_y;
        vga_get_pos(&prompt_x, &prompt_y);

        int max_len = SCREEN_WIDTH - prompt_x - 1;
        if (max_len <= 0)            max_len = 1;
        if (max_len >= BUFFER_SIZE)  max_len = BUFFER_SIZE - 1;

        int len = 0, pos = 0;
        line[0] = '\0';

#define REDRAW() \
    vga_draw_row(prompt_y, prompt_x, max_len, line, len, COLOUR_WHITE); \
    vga_set_pos(prompt_x + pos, prompt_y)

        while (1) {
            int c = read_key();

            if (c == '\n' || c == '\r') {
                line[len] = '\0';
                vga_write("\n", COLOUR_WHITE);
                break;
            }
            if (c == '\b') {
                if (pos > 0) {
                    memmove(line+pos-1, line+pos, (usize)(len-pos));
                    pos--; len--; line[len] = '\0';
                    REDRAW();
                }
                continue;
            }
            if (c == KEY_DELETE) {
                if (pos < len) {
                    memmove(line+pos, line+pos+1, (usize)(len-pos-1));
                    len--; line[len] = '\0';
                    REDRAW();
                }
                continue;
            }
            if (c == KEY_LEFT)  { if (pos > 0)   { pos--; vga_set_pos(prompt_x+pos, prompt_y); } continue; }
            if (c == KEY_RIGHT) { if (pos < len)  { pos++; vga_set_pos(prompt_x+pos, prompt_y); } continue; }
            if (c == KEY_HOME)  { pos = 0;   vga_set_pos(prompt_x,     prompt_y); continue; }
            if (c == KEY_END)   { pos = len; vga_set_pos(prompt_x+pos, prompt_y); continue; }

            if (c == KEY_UP) {
                char hist[BUFFER_SIZE];
                history_prev(hist, sizeof(hist));
                if (hist[0]) {
                    strncpy(line, hist, (usize)max_len);
                    line[max_len] = '\0';
                    len = (int)strlen(line); pos = len;
                    REDRAW();
                }
                continue;
            }
            if (c == KEY_DOWN) {
                char hist[BUFFER_SIZE];
                history_next(hist, sizeof(hist));
                strncpy(line, hist, (usize)max_len);
                line[max_len] = '\0';
                len = (int)strlen(line); pos = len;
                REDRAW();
                continue;
            }
            if (c == 3) {   /* Ctrl+C */
                line[0] = '\0'; len = 0; pos = 0;
                vga_write("^C\n", COLOUR_DARK_GRAY);
                break;
            }
            if (c == 12) {  /* Ctrl+L */
                clear_screen();
                show_prompt();
                vga_get_pos(&prompt_x, &prompt_y);
                max_len = SCREEN_WIDTH - prompt_x - 1;
                if (max_len <= 0) max_len = 1;
                if (max_len >= BUFFER_SIZE) max_len = BUFFER_SIZE - 1;
                REDRAW();
                continue;
            }
            if (c >= 32 && c <= 126 && len < max_len) {
                memmove(line+pos+1, line+pos, (usize)(len-pos));
                line[pos] = (char)c;
                pos++; len++; line[len] = '\0';
                REDRAW();
            }
        }
#undef REDRAW

        if (len > 0) execute_command(line);
    }
}
