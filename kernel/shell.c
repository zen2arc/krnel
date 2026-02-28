#include "kernel.h"

#define MAX_ARGS 20
#define MAX_ALIASES 20

static char* args[MAX_ARGS];
static int arg_count = 0;

typedef struct {
    char name[32];
    char value[128];
} alias_t;

static alias_t aliases[MAX_ALIASES];
static int alias_count = 0;

// login state
static int logged_in = 0;
static char line[BUFFER_SIZE];

// read a single character from keyboard
static char read_char(void) {
    while (1) {
        int c = read_key();
        if (c >= 32 && c <= 126) return (char)c;
        if (c == '\n' || c == '\b') return (char)c;
    }
}

// read a string from keyboard with optional echo
static void read_string(char* buffer, int max_len, int show_chars) {
    int pos = 0;
    while (1) {
        char c = read_char();
        if (c == '\n') {
            buffer[pos] = '\0';
            vga_write("\n", 0x0F);
            break;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                vga_write("\b \b", 0x0F);
            }
        } else if (pos < max_len - 1) {
            buffer[pos++] = c;
            if (show_chars) {
                char str[2] = {c, '\0'};
                vga_write(str, 0x0F);
            } else {
                vga_write("*", 0x0F);
            }
        }
    }
}

// first boot setup: create root and normal user
static void first_boot_setup(void) {
    clear_screen();
    vga_write("first boot detected\n", 0x0E);
    vga_write("please set up system accounts\n\n", 0x0F);

    // create root user
    char root_password[MAX_PASSWORD];
    while (1) {
        vga_write("set root password: ", 0x0F);
        read_string(root_password, MAX_PASSWORD, 0);
        if (strlen(root_password) > 0) break;
        vga_write("password cannot be empty\n", 0x04);
    }
    user_add("root", root_password, 1);
    vga_write("root account created\n", 0x0A);

    // create normal user
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    while (1) {
        vga_write("create a normal user\n", 0x0F);
        vga_write("username: ", 0x0F);
        read_string(username, MAX_USERNAME, 1);
        vga_write("password: ", 0x0F);
        read_string(password, MAX_PASSWORD, 0);
        if (strlen(username) == 0 || strlen(password) == 0) {
            vga_write("username and password cannot be blank\n", 0x04);
        } else if (user_add(username, password, 0) == 0) {
            vga_write("user created successfully\n\n", 0x0A);
            break;
        } else {
            vga_write("failed to create user, try again\n", 0x04);
        }
    }

    vga_write("setup complete! please login\n\n", 0x0A);
}

static void login_prompt(void) {
    clear_screen();
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    int attempts = 0;

    while (attempts < 3) {
        vga_write("kTTY 1.0.0rc1 tty1\n", 0x0F);
        vga_write("login: ", 0x0F);
        read_string(username, MAX_USERNAME, 1);
        vga_write("password: ", 0x0F);
        read_string(password, MAX_PASSWORD, 0);

        if (user_login(username, password) == 0) {
            vga_write("\nlogin successful!\n", 0x0F);
            logged_in = 1;
            clear_screen();
            return;
        } else {
            clear_screen();
            vga_write("\nlogin failed: invalid username or password\n", 0x04);
            attempts++;
        }
    }
    vga_write("\ntoo many failed attempts. system halted.\n", 0x04);
    while (1);
}

// shell prompt
static void prompt(void) {
    vga_write(user_get_name(), 0x0A);
    vga_write("@kTTY ", 0x0F);
    vga_write(fs_getcwd(), 0x0B);
    vga_write(" $ ", 0x0F);
}

// parse command line into arguments
static void parse_command(char* line) {
    arg_count = 0;
    char* token = strtok(line, " \t\n");
    while (token && arg_count < MAX_ARGS - 1) {
        args[arg_count++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[arg_count] = NULL;
}

// alias expansion
static char* expand_alias(const char* cmd) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(aliases[i].name, cmd) == 0) return aliases[i].value;
    return NULL;
}

// add alias
static void add_alias(const char* name, const char* value) {
    if (alias_count >= MAX_ALIASES) {
        vga_write("too many aliases\n", 0x04);
        return;
    }
    for (int i = 0; i < alias_count; i++)
        if (strcmp(aliases[i].name, name) == 0) {
            strcpy(aliases[i].value, value);
            return;
        }
    strcpy(aliases[alias_count].name, name);
    strcpy(aliases[alias_count].value, value);
    alias_count++;
}

// list aliases
static void list_aliases(void) {
    vga_write("aliases:\n", 0x0F);
    for (int i = 0; i < alias_count; i++) {
        char line[100];
        snprintf(line, sizeof(line), "%s='%s'\n", aliases[i].name, aliases[i].value);
        vga_write(line, 0x0A);
    }
}

// change directory
static void change_directory(char* path) {
    if (!path || strlen(path) == 0) {
        fs_chdir(user_get_home());
    } else if (fs_chdir(path) != 0) {
        char error[50];
        snprintf(error, sizeof(error), "cd: %s: No such directory\n", path);
        vga_write(error, 0x04);
    }
}

// list directory contents
static void list_directory(void) {
    char buffer[4096];
    if (fs_list(buffer, sizeof(buffer)) > 0)
        vga_write(buffer, 0x0F);
    else
        vga_write("directory is empty\n", 0x08);
}

// display a file
static void display_file(const char* filename) {
    char buffer[4096];
    int size = fs_read(filename, buffer, sizeof(buffer)-1);
    if (size < 0) {
        char error[50];
        snprintf(error, sizeof(error), "cat: %s: No such file\n", filename);
        vga_write(error, 0x04);
        return;
    }
    buffer[size] = '\0';
    vga_write(buffer, 0x0F);
    if (size > 0 && buffer[size-1] != '\n') vga_write("\n", 0x0F);
}

// create a file
static void create_file(const char* filename) {
    if (fs_exists(filename)) {
        char error[50];
        snprintf(error, sizeof(error), "touch: %s: File exists\n", filename);
        vga_write(error, 0x04);
        return;
    }
    if (fs_create(filename, 1) < 0) {
        vga_write("touch: Failed to create file\n", 0x04);
    } else {
        vga_write("File created\n", 0x0A);
    }
}

// remove a file
static void remove_file(const char* filename) {
    if (fs_delete(filename) != 0) {
        char error[50];
        snprintf(error, sizeof(error), "rm: %s: Cannot remove\n", filename);
        vga_write(error, 0x04);
    }
}

// make a directory
static void make_directory(const char* dirname) {
    if (fs_exists(dirname)) {
        char error[50];
        snprintf(error, sizeof(error), "mkdir: %s: File exists\n", dirname);
        vga_write(error, 0x04);
        return;
    }
    if (fs_mkdir(dirname) < 0)
        vga_write("mkdir: Failed to create directory\n", 0x04);
}

// echo command (output arguments)
static void echo_command(void) {
    for (int i = 1; i < arg_count; i++) {
        vga_write(args[i], 0x0F);
        if (i < arg_count-1) vga_write(" ", 0x0F);
    }
    vga_write("\n", 0x0F);
}

// show help for commands
static void show_help(void) {
    vga_write("built-in commands:\n", 0x0E);
    vga_write(" cd [dir]   ls   cat <file>   touch <file>   rm <file>\n", 0x0F);
    vga_write(" mkdir <dir>   rmdir <dir>   echo [text]   clear   help\n", 0x0F);
    vga_write(" history   alias   pwd   whoami   ps   sysfetch   exit\n", 0x0F);
    vga_write(" ./script.sh   kittywrite <file>\n", 0x0F);
}

// execute a command
void execute_command(char* line) {
    if (!line || strlen(line) == 0) return;
    history_add(line);

    char* expanded = expand_alias(line);
    if (expanded) {
        char copy[BUFFER_SIZE];
        strcpy(copy, expanded);
        execute_command(copy);
        return;
    }

    parse_command(line);
    if (arg_count == 0) return;

    // built-in commands
    if (strcmp(args[0], "cd") == 0) change_directory(arg_count > 1 ? args[1] : NULL);
    else if (strcmp(args[0], "ls") == 0) list_directory();
    else if (strcmp(args[0], "cat") == 0) display_file(args[1]);
    else if (strcmp(args[0], "touch") == 0) create_file(args[1]);
    else if (strcmp(args[0], "rm") == 0) remove_file(args[1]);
    else if (strcmp(args[0], "mkdir") == 0) make_directory(args[1]);
    else if (strcmp(args[0], "echo") == 0) echo_command();
    else if (strcmp(args[0], "clear") == 0) clear_screen();
    else if (strcmp(args[0], "help") == 0) show_help();
    else if (strcmp(args[0], "history") == 0) history_show();
    else if (strcmp(args[0], "alias") == 0) {
        if (arg_count == 1) list_aliases();
        else if (arg_count >= 3) add_alias(args[1], args[2]);
    }
    else if (strcmp(args[0], "pwd") == 0) {
        vga_write(fs_getcwd(), 0x0F);
        vga_write("\n", 0x0F);
    }
    else if (strcmp(args[0], "whoami") == 0) {
        vga_write(user_get_name(), 0x0F);
        vga_write("\n", 0x0F);
    }
    else if (strcmp(args[0], "ps") == 0) {
        char buffer[1024];
        proc_get_list(buffer, sizeof(buffer));
        vga_write(buffer, 0x0F);
    }
    else if (strcmp(args[0], "sysfetch") == 0) {
        extern void sysfetch_run(void);
        sysfetch_run();
    }
    else if (strcmp(args[0], "kittywrite") == 0) {
        if (arg_count < 2) vga_write("usage: kittywrite <file>\n", 0x04);
        else kittywrite(args[1]);
    }
    else if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "logout") == 0) {
        vga_write("logging out...\n", 0x08);
        user_logout();
        clear_screen();
        logged_in = 0;
    }
    else {
        char error[50];
        snprintf(error, sizeof(error), "ash: %s: command not found\n", args[0]);
        vga_write(error, 0x04);
    }
}

// initialize shell
void shell_init(void) {
    add_alias("ll", "ls");
    add_alias("..", "cd ..");
    add_alias("~", "cd /");

    // run first boot setup if needed
    if (user_first_boot()) first_boot_setup();

    // login loop
    while (!logged_in) login_prompt();
}

// run shell
void shell_run(void) {
    while (logged_in) {
        prompt();
        int pos = 0;
        line[0] = '\0';
        while (1) {
            char c = read_char();
            if (c == '\n') {
                vga_write("\n", 0x0F);
                break;
            } else if (c == '\b' && pos > 0) {
                pos--;
                line[pos] = '\0';
                vga_write("\b \b", 0x0F);
            } else if (c >= 32 && pos < BUFFER_SIZE - 1) {
                line[pos++] = c;
                line[pos] = '\0';
                char str[2] = {c, '\0'};
                vga_write(str, 0x0F);
            }
        }
        if (strlen(line) > 0) execute_command(line);
    }
}
