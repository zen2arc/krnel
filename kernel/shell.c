#include "kernel.h"
#include "shell.h"

#define MAX_ARGS    20
#define MAX_ALIASES 20

static char* args[MAX_ARGS];
static int   arg_count = 0;

typedef struct {
    char name[32];
    char value[128];
} alias_t;

static alias_t aliases[MAX_ALIASES];
static int     alias_count = 0;
static int     logged_in   = 0;

int shell_logged_in(void)  { return logged_in; }

void shell_prompt_redraw(void) {
    if (!logged_in) return;
    vga_write("\n", COLOUR_WHITE);
    vga_write(user_get_name(), COLOUR_LIGHT_GREEN);
    vga_write("@kTTY ",        COLOUR_WHITE);
    vga_write(fs_getcwd(),     COLOUR_LIGHT_CYAN);
    vga_write(" $ ",           COLOUR_WHITE);
}

static void read_string(char* buffer, int max_len, int show_chars) {
    int pos = 0;
    while (1) {
        int c = read_key();
        if (c == '\n' || c == '\r') { buffer[pos] = '\0'; vga_write("\n", COLOUR_WHITE); return; }
        if (c == '\b') { if (pos > 0) { pos--; vga_write("\b \b", COLOUR_WHITE); } continue; }
        if (c >= 32 && c <= 126 && pos < max_len - 1) {
            buffer[pos++] = (char)c;
            if (show_chars) { char s[2]={(char)c,'\0'}; vga_write(s,COLOUR_WHITE); }
            else vga_write("*", COLOUR_WHITE);
        }
    }
}

static void first_boot_setup(void) {
    clear_screen();
    vga_write("kTTY " KTTY_VERSION " first boot\n", COLOUR_YELLOW);
    vga_write("Setting up system accounts...\n\n", COLOUR_WHITE);

    char root_password[MAX_PASSWORD];
    while (1) {
        vga_write("Set root password: ", COLOUR_WHITE);
        read_string(root_password, MAX_PASSWORD, 0);
        if (strlen(root_password) > 0) break;
        vga_write("Password cannot be empty.\n", COLOUR_LIGHT_RED);
    }
    user_add("root", root_password, 1);
    vga_write("Root account created.\n", COLOUR_LIGHT_GREEN);

    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    while (1) {
        vga_write("\nCreate a user account\n", COLOUR_WHITE);
        vga_write("Username: ", COLOUR_WHITE); read_string(username, MAX_USERNAME, 1);
        vga_write("Password: ", COLOUR_WHITE); read_string(password, MAX_PASSWORD, 0);
        if (!strlen(username) || !strlen(password)) {
            vga_write("Username and password cannot be blank.\n", COLOUR_LIGHT_RED);
        } else if (user_add(username, password, 0) == 0) {
            vga_write("User created.\n\n", COLOUR_LIGHT_GREEN); break;
        } else {
            vga_write("Failed to create user, try again.\n", COLOUR_LIGHT_RED);
        }
    }
    vga_write("Setup complete! Please log in.\n\n", COLOUR_LIGHT_GREEN);
}

static void login_prompt(void) {
    clear_screen();
    char username[MAX_USERNAME], password[MAX_PASSWORD];
    int  attempts = 0;
    while (attempts < 3) {
        vga_write("kTTY " KTTY_VERSION " (" KRNEL_VERSION_STR ")", COLOUR_LIGHT_CYAN);
        vga_write(" /dev/tty", COLOUR_DARK_GRAY);
        char tnum[4]; itoa(current_tty + 1, tnum); vga_write(tnum, COLOUR_DARK_GRAY);
        vga_write("\n\n", COLOUR_WHITE);
        vga_write("kTTY login: ", COLOUR_WHITE); read_string(username, MAX_USERNAME, 1);
        vga_write("Password:   ", COLOUR_WHITE); read_string(password, MAX_PASSWORD, 0);
        if (user_login(username, password) == 0) {
            vga_write("\nLogin successful!\n", COLOUR_LIGHT_GREEN);
            logged_in = 1; clear_screen(); return;
        }
        clear_screen();
        vga_write("Login failed: incorrect username or password.\n\n", COLOUR_LIGHT_RED);
        attempts++;
    }
    vga_write("Too many failed attempts. System halted.\n", COLOUR_RED);
    khang();
}

static void show_prompt(void) {
    vga_write(user_get_name(), COLOUR_LIGHT_GREEN);
    vga_write("@kTTY ",        COLOUR_WHITE);
    vga_write(fs_getcwd(),     COLOUR_LIGHT_CYAN);
    vga_write(" $ ",           COLOUR_WHITE);
}

static void parse_command(char* line) {
    arg_count = 0;
    char* token = strtok(line, " \t\n");
    while (token && arg_count < MAX_ARGS - 1) { args[arg_count++] = token; token = strtok(NULL, " \t\n"); }
    args[arg_count] = NULL;
}

static char* expand_alias(const char* cmd) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(aliases[i].name, cmd) == 0) return aliases[i].value;
    return NULL;
}

static void add_alias(const char* name, const char* value) {
    if (alias_count >= MAX_ALIASES) { vga_write("Too many aliases.\n", COLOUR_LIGHT_RED); return; }
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            strncpy(aliases[i].value, value, sizeof(aliases[i].value)-1); return;
        }
    }
    strncpy(aliases[alias_count].name,  name,  sizeof(aliases[0].name) -1);
    strncpy(aliases[alias_count].value, value, sizeof(aliases[0].value)-1);
    alias_count++;
}

static void list_aliases(void) {
    vga_write("Aliases:\n", COLOUR_WHITE);
    for (int i = 0; i < alias_count; i++) {
        char buf[128]; snprintf(buf, sizeof(buf), "  %s='%s'\n", aliases[i].name, aliases[i].value);
        vga_write(buf, COLOUR_LIGHT_CYAN);
    }
}

static void cmd_cd(char* path) {
    if (!path || !strlen(path)) { fs_chdir(user_get_home()); return; }
    if (fs_chdir(path) != 0) {
        char err[80]; snprintf(err,sizeof(err),"cd: %s: No such directory\n",path);
        vga_write(err, COLOUR_LIGHT_RED);
    }
}
static void cmd_ls(void) {
    char buf[4096];
    if (fs_list(buf, sizeof(buf)) > 0) vga_write(buf, COLOUR_WHITE);
    else vga_write("(empty)\n", COLOUR_DARK_GRAY);
}
static void cmd_cat(const char* f) {
    if (!f) { vga_write("Usage: cat <file>\n", COLOUR_LIGHT_RED); return; }
    char buf[4096]; int sz = fs_read(f, buf, sizeof(buf)-1);
    if (sz < 0) { char err[80]; snprintf(err,sizeof(err),"cat: %s: No such file\n",f); vga_write(err,COLOUR_LIGHT_RED); return; }
    buf[sz]='\0'; vga_write(buf,COLOUR_WHITE);
    if (sz>0 && buf[sz-1]!='\n') vga_write("\n",COLOUR_WHITE);
}
static void cmd_touch(const char* f) {
    if (!f) { vga_write("Usage: touch <file>\n",COLOUR_LIGHT_RED); return; }
    if (fs_exists(f)) { vga_write("touch: file exists\n",COLOUR_YELLOW); return; }
    if (fs_create(f,1)<0) vga_write("touch: failed\n",COLOUR_LIGHT_RED);
}
static void cmd_rm(const char* f) {
    if (!f) { vga_write("Usage: rm <file>\n",COLOUR_LIGHT_RED); return; }
    if (fs_delete(f)!=0) { char err[80]; snprintf(err,sizeof(err),"rm: %s: cannot remove\n",f); vga_write(err,COLOUR_LIGHT_RED); }
}
static void cmd_mkdir(const char* d) {
    if (!d) { vga_write("Usage: mkdir <dir>\n",COLOUR_LIGHT_RED); return; }
    if (fs_exists(d)) { vga_write("mkdir: already exists\n",COLOUR_YELLOW); return; }
    if (fs_mkdir(d)<0) vga_write("mkdir: failed\n",COLOUR_LIGHT_RED);
}
static void cmd_echo(void) {
    for (int i=1;i<arg_count;i++) { vga_write(args[i],COLOUR_WHITE); if(i<arg_count-1) vga_write(" ",COLOUR_WHITE); }
    vga_write("\n",COLOUR_WHITE);
}
static void cmd_help(void) {
    vga_write("kTTY " KTTY_VERSION " built-in commands:\n",           COLOUR_YELLOW);
    vga_write("  Navigation : cd [dir]  ls  pwd\n",                    COLOUR_WHITE);
    vga_write("  Files      : cat  touch  rm  mkdir\n",                COLOUR_WHITE);
    vga_write("  Text       : echo  kittywrite <file>\n",              COLOUR_WHITE);
    vga_write("  System     : ps  sysfetch  uname  hostname\n",        COLOUR_WHITE);
    vga_write("  Users      : whoami  useradd  userdel\n",             COLOUR_WHITE);
    vga_write("  Shell      : history  alias  clear  help\n",          COLOUR_WHITE);
    vga_write("  Session    : exit  logout\n",                          COLOUR_WHITE);
    vga_write("  Arrow Up/Down: history | Left/Right/Home/End: cursor\n", COLOUR_DARK_GRAY);
}
static void cmd_uname(void) {
    if (arg_count>1 && strcmp(args[1],"-a")==0)
        vga_write("kTTY kTTY-" KTTY_VERSION " " KRNEL_VERSION_STR " #1 x86\n",COLOUR_WHITE);
    else vga_write("kTTY\n",COLOUR_WHITE);
}
static void cmd_hostname(void) {
    char buf[64]; int sz=fs_read("/etc/hostname",buf,sizeof(buf)-1);
    if (sz>0) { buf[sz]='\0'; if(buf[sz-1]=='\n') buf[sz-1]='\0'; vga_write(buf,COLOUR_WHITE); vga_write("\n",COLOUR_WHITE); }
    else vga_write("ktty\n",COLOUR_WHITE);
}

void execute_command(char* line) {
    if (!line || !strlen(line)) return;
    history_add(line);

    char* expanded = expand_alias(line);
    if (expanded) {
        char copy[BUFFER_SIZE]; strncpy(copy,expanded,sizeof(copy)-1); copy[sizeof(copy)-1]='\0';
        execute_command(copy); return;
    }

    parse_command(line);
    if (arg_count==0) return;
    const char* cmd=args[0];

    if      (strcmp(cmd,"cd")==0)         cmd_cd(arg_count>1?args[1]:NULL);
    else if (strcmp(cmd,"ls")==0)         cmd_ls();
    else if (strcmp(cmd,"cat")==0)        cmd_cat(args[1]);
    else if (strcmp(cmd,"touch")==0)      cmd_touch(args[1]);
    else if (strcmp(cmd,"rm")==0)         cmd_rm(args[1]);
    else if (strcmp(cmd,"mkdir")==0)      cmd_mkdir(args[1]);
    else if (strcmp(cmd,"echo")==0)       cmd_echo();
    else if (strcmp(cmd,"clear")==0)      clear_screen();
    else if (strcmp(cmd,"help")==0)       cmd_help();
    else if (strcmp(cmd,"history")==0)    history_show();
    else if (strcmp(cmd,"uname")==0)      cmd_uname();
    else if (strcmp(cmd,"hostname")==0)   cmd_hostname();
    else if (strcmp(cmd,"alias")==0) {
        if (arg_count==1) list_aliases();
        else if (arg_count>=3) add_alias(args[1],args[2]);
        else vga_write("Usage: alias <n> <value>\n",COLOUR_LIGHT_RED);
    }
    else if (strcmp(cmd,"pwd")==0)    { vga_write(fs_getcwd(),COLOUR_WHITE); vga_write("\n",COLOUR_WHITE); }
    else if (strcmp(cmd,"whoami")==0) { vga_write(user_get_name(),COLOUR_WHITE); vga_write("\n",COLOUR_WHITE); }
    else if (strcmp(cmd,"ps")==0)     { char buf[1024]; proc_get_list(buf,sizeof(buf)); vga_write(buf,COLOUR_WHITE); }
    else if (strcmp(cmd,"sysfetch")==0)  sysfetch_run();
    else if (strcmp(cmd,"kittywrite")==0) {
        if (arg_count<2) vga_write("Usage: kittywrite <file>\n",COLOUR_LIGHT_RED);
        else kittywrite(args[1]);
    }
    else if (strcmp(cmd,"useradd")==0) {
        if (arg_count<3)          vga_write("Usage: useradd <user> <pass>\n",COLOUR_LIGHT_RED);
        else if (!user_is_root()) vga_write("Permission denied.\n",COLOUR_LIGHT_RED);
        else if (user_add(args[1],args[2],0)==0) vga_write("User added.\n",COLOUR_LIGHT_GREEN);
        else vga_write("useradd: failed.\n",COLOUR_LIGHT_RED);
    }
    else if (strcmp(cmd,"userdel")==0) {
        if (arg_count<2)          vga_write("Usage: userdel <user>\n",COLOUR_LIGHT_RED);
        else if (!user_is_root()) vga_write("Permission denied.\n",COLOUR_LIGHT_RED);
        else if (user_del(args[1])==0) vga_write("User deleted.\n",COLOUR_LIGHT_GREEN);
        else vga_write("userdel: failed.\n",COLOUR_LIGHT_RED);
    }
    else if (strcmp(cmd,"exit")==0 || strcmp(cmd,"logout")==0) {
        vga_write("Logging out...\n",COLOUR_DARK_GRAY);
        user_logout(); clear_screen(); logged_in=0; login_prompt();
    }
    else {
        char err[80]; snprintf(err,sizeof(err),"ash: %s: command not found\n",cmd);
        vga_write(err,COLOUR_LIGHT_RED);
    }
}

void shell_init(void) {
    add_alias("ll","ls"); add_alias("..","cd .."); add_alias("~","cd /");
    if (user_first_boot()) first_boot_setup();
    while (!logged_in) login_prompt();
}

void shell_run(void) {
    static char line[BUFFER_SIZE];

    while (logged_in) {
        show_prompt();

        int prompt_x, prompt_y;
        vga_get_pos(&prompt_x, &prompt_y);

        /* cap visible length to remaining cols on this row */
        int max_len = SCREEN_WIDTH - prompt_x - 1;
        if (max_len <= 0)           max_len = 1;
        if (max_len >= BUFFER_SIZE) max_len = BUFFER_SIZE - 1;

        int len = 0, pos = 0;
        line[0] = '\0';

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
                    pos--; len--; line[len]='\0';
                    vga_draw_row(prompt_y, prompt_x, max_len, line, len, COLOUR_WHITE);
                    vga_set_pos(prompt_x+pos, prompt_y);
                }
                continue;
            }

            if (c == KEY_DELETE) {
                if (pos < len) {
                    memmove(line+pos, line+pos+1, (usize)(len-pos-1));
                    len--; line[len]='\0';
                    vga_draw_row(prompt_y, prompt_x, max_len, line, len, COLOUR_WHITE);
                    vga_set_pos(prompt_x+pos, prompt_y);
                }
                continue;
            }

            if (c == KEY_LEFT) {
                if (pos > 0) { pos--; vga_set_pos(prompt_x+pos, prompt_y); }
                continue;
            }

            if (c == KEY_RIGHT) {
                if (pos < len) { pos++; vga_set_pos(prompt_x+pos, prompt_y); }
                continue;
            }

            if (c == KEY_HOME) { pos=0; vga_set_pos(prompt_x, prompt_y); continue; }

            if (c == KEY_END)  { pos=len; vga_set_pos(prompt_x+pos, prompt_y); continue; }

            if (c == KEY_UP) {
                char hist[BUFFER_SIZE];
                history_prev(hist, sizeof(hist));
                if (hist[0]) {
                    strncpy(line, hist, (usize)max_len); line[max_len]='\0';
                    len=(int)strlen(line); pos=len;
                    vga_draw_row(prompt_y, prompt_x, max_len, line, len, COLOUR_WHITE);
                    vga_set_pos(prompt_x+pos, prompt_y);
                }
                continue;
            }

            if (c == KEY_DOWN) {
                char hist[BUFFER_SIZE];
                history_next(hist, sizeof(hist));
                strncpy(line, hist, (usize)max_len); line[max_len]='\0';
                len=(int)strlen(line); pos=len;
                vga_draw_row(prompt_y, prompt_x, max_len, line, len, COLOUR_WHITE);
                vga_set_pos(prompt_x+pos, prompt_y);
                continue;
            }

            if (c >= 32 && c <= 126 && len < max_len) {
                memmove(line+pos+1, line+pos, (usize)(len-pos));
                line[pos]=(char)c; pos++; len++; line[len]='\0';
                vga_draw_row(prompt_y, prompt_x, max_len, line, len, COLOUR_WHITE);
                vga_set_pos(prompt_x+pos, prompt_y);
            }
        }

        if (len > 0) execute_command(line);
    }
}
