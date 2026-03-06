#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t   usize;

/* ===================== version ===================== */
#define KTTY_VERSION      "0.2.0-beta"
#define KRNEL_VERSION     "0.4"
#define KRNEL_VERSION_STR "krnel 0.4"

/* =================== FS constants ================== */
#define FS_MAX_FILES  128
#define FS_MAX_NAME   32
#define FS_BLOCK_SIZE 512
#define BUFFER_SIZE   256
#define HISTORY_SIZE  50
#define MAX_USERS     50
#define MAX_USERNAME  32
#define MAX_PASSWORD  32

/* =================== VGA constants ================= */
#define VIDEO_MEMORY  0xB8000
#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 25

/* ==================== colours ====================== */
#define COLOUR_BLACK           0x00
#define COLOUR_BLUE            0x01
#define COLOUR_GREEN           0x02
#define COLOUR_CYAN            0x03
#define COLOUR_RED             0x04
#define COLOUR_MAGENTA         0x05
#define COLOUR_BROWN           0x06
#define COLOUR_LIGHT_GRAY      0x07
#define COLOUR_DARK_GRAY       0x08
#define COLOUR_LIGHT_BLUE      0x09
#define COLOUR_LIGHT_GREEN     0x0A
#define COLOUR_LIGHT_CYAN      0x0B
#define COLOUR_LIGHT_RED       0x0C
#define COLOUR_LIGHT_MAGENTA   0x0D
#define COLOUR_YELLOW          0x0E
#define COLOUR_WHITE           0x0F

#define COLOUR_DEBUG_INFO        COLOUR_MAGENTA
#define COLOUR_DEBUG_ERROR       COLOUR_LIGHT_RED
#define COLOUR_DEBUG_FATAL_ERROR COLOUR_RED

/* ==================== forward decls ================ */
struct ata_disk_s;
typedef struct ata_disk_s ata_disk_t;

struct ext2_fs_s;
typedef struct ext2_fs_s ext2_fs_t;

/* ==================== kmain ======================== */
void kmain(unsigned int magic, unsigned int mb_info_addr);

static inline void khang(void) {
    while (1) __asm__ volatile ("hlt");
}

/* ==================== VGA ========================== */
void clear_screen(void);
void vga_write(const char* str, u8 color);
void vga_write_rgb(const char* str, u8 r, u8 g, u8 b); /* maps to nearest 16-color */
void vga_set_pos(int x, int y);
void vga_get_pos(int* x, int* y);
void vga_clear_row(int y);
void vga_put_char_at(int x, int y, char c, u8 color);
void vga_draw_row(int y, int start_x, int width, const char* str, int len, u8 color);
void putchar(char c, u8 color);
void itoa(int n, char* str);

/* ==================== memory ======================= */
void* kmalloc(usize size);
void  kfree(void* ptr);
void  mem_init(void);

/* ==================== string ======================= */
usize   strlen(const char* str);
char*   strcpy(char* dest, const char* src);
char*   strncpy(char* dest, const char* src, usize n);
char*   strcat(char* dest, const char* src);
char*   strchr(const char* str, int c);
char*   strrchr(const char* str, int c);
char*   strstr(const char* haystack, const char* needle);
int     strcmp(const char* s1, const char* s2);
int     strncmp(const char* s1, const char* s2, usize n);
int     strcasecmp(const char* s1, const char* s2);
void*   memset(void* ptr, int value, usize num);
void*   memcpy(void* dest, const void* src, usize num);
void*   memmove(void* dest, const void* src, usize num);
int     memcmp(const void* ptr1, const void* ptr2, usize num);
char*   strtok(char* str, const char* delim);
char*   strdup(const char* str);
int     snprintf(char* str, usize size, const char* format, ...);
int     vsnprintf(char* str, usize size, const char* format, va_list args);
char*   strrev(char* str);
char*   strlwr(char* str);
char*   strupr(char* str);
usize   strnlen(const char* str, usize maxlen);
void*   memccpy(void* dest, const void* src, int c, usize n);
void*   memchr(const void* ptr, int value, usize num);
char**  strsplit(const char* str, const char* delim, int* count);
void    strfree(char** array);
int     atoi(const char* str);

/* ==================== keyboard ===================== */
int read_key(void);
int key_available(void);
int get_shift_state(void);
int get_caps_state(void);
int get_ctrl_state(void);
int get_alt_state(void);

/* key codes for special keys */
#define KEY_LEFT   -30
#define KEY_RIGHT  -31
#define KEY_UP     -32
#define KEY_DOWN   -33
#define KEY_DELETE -34
#define KEY_HOME   -35
#define KEY_END    -36

/* ==================== ATA ========================== */
int       ata_init(void);
int       ata_read_sectors(ata_disk_t* disk, u32 lba, u8 count, void* buffer);
int       ata_write_sectors(ata_disk_t* disk, u32 lba, u8 count, const void* buffer);
ata_disk_t* ata_get_primary(void);
ata_disk_t* ata_get_secondary(void);
void      ata_reset(ata_disk_t* disk);

/* ==================== ext2 ========================= */
#include "ext2.h"

/* ==================== filesystem =================== */
void        fs_init(void);
int         fs_create(const char* name, u8 type);
int         fs_delete(const char* name);
int         fs_list(char* buffer, usize size);
int         fs_read(const char* name, char* buffer, usize size);
int         fs_write(const char* name, const char* data, usize size);
int         fs_chdir(const char* dir);
const char* fs_getcwd(void);
int         fs_exists(const char* name);
int         fs_size(const char* name);
int         fs_type(const char* name);
int         fs_mkdir(const char* name);
int         fs_create_home(const char* username);
int         fs_mount_disk(void);

/* ==================== shell ======================== */
void shell_init(void);
void shell_run(void);
void execute_command(char* line);

/* ==================== history ====================== */
void history_add(const char* command);
void history_prev(char* buffer, usize size);
void history_next(char* buffer, usize size);
void history_show(void);

/* ==================== users ======================== */
typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    u32  uid;
    u32  gid;
    u8   is_root;
    char home[MAX_USERNAME + 8];
    char shell[32];
} user_t;

void        user_init(void);
int         user_login(const char* username, const char* password);
int         user_logout(void);
int         user_add(const char* username, const char* password, u8 is_root);
int         user_del(const char* username);
void        user_current(char* username, usize size);
u32         user_get_uid(void);
u8          user_is_root(void);
const char* user_get_name(void);
const char* user_get_home(void);
int         user_first_boot(void);
void        user_debug_dump(void);

/* ==================== process ====================== */
void proc_init(void);
int  proc_create(const char* name, u64 entry);
int  proc_create_user(const char* name, u32 entry, u8 is_user);
void proc_yield(void);
void proc_exit(int code);
int  proc_get_list(char* buffer, usize size);
u32  proc_get_pid(void);
void timer_handler(void);

/* ==================== syscall ====================== */
u64  syscall(u64 num, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5);
void kprint(const char* str);

/* ================== kittywrite ===================== */
void kittywrite(const char* filename);

/* ==================== I/O ========================== */
static inline u8 inb(u16 port) {
    u8 v; __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port)); return v;
}
static inline void outb(u16 port, u8 v) {
    __asm__ volatile ("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline u16 inw(u16 port) {
    u16 v; __asm__ volatile ("inw %1, %0" : "=a"(v) : "Nd"(port)); return v;
}
static inline void outw(u16 port, u16 v) {
    __asm__ volatile ("outw %0, %1" : : "a"(v), "Nd"(port));
}
static inline void io_wait(void) { outb(0x80, 0); }

/* ==================== globals ====================== */
extern u32 system_uptime;

/* ==================== plugin system ================ */
#include "plugin.h"

/* ==================== sysfetch ===================== */
void sysfetch_run(void);

/* ==================== VFS ========================== */
void        vfs_init(void);
int         vfs_mkdir(const char* name);
int         vfs_create(const char* name, u8 type);
int         vfs_delete(const char* name);
int         vfs_list(char* buffer, usize size);
int         vfs_read(const char* name, char* buffer, usize size);
int         vfs_write(const char* name, const char* data, usize size);
int         vfs_chdir(const char* dir);
const char* vfs_getcwd(void);
int         vfs_exists(const char* name);
int         vfs_size(const char* name);
int         vfs_type(const char* name);
int         vfs_create_home(const char* username);
int         vfs_mount_disk(void);

/* ==================== TTY ========================== */
#include "tty.h"

#endif /* KERNEL_H */
