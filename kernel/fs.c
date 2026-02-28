#include "kernel.h"
#include "ata.h"
#include "ext2.h"

static ext2_fs_t* fs = NULL;
static u32 cwd_inode = 2;
static char cwd[64] = "/";

void fs_init(void) {
    (void)cwd;
    ata_init();
    ata_disk_t* disk = ata_get_primary();
    if (!disk) {
        vga_write("no disk, ram fallback\n", COLOUR_LIGHT_RED);
        return;
    }
    if (ext2_mount(disk, 0, &fs) != 0) {
        vga_write("formatting disk...\n", COLOUR_YELLOW);
        ext2_format(disk, 0, 131072);
        if (ext2_mount(disk, 0, &fs) != 0) {
            vga_write("failed to mount! stopping.\n", COLOUR_DEBUG_FATAL_ERROR);
            khang();
        }
    }
    ext2_mkdir(fs, 2, "bin");
    ext2_mkdir(fs, 2, "home");
}

int fs_mkdir(const char* name) {
    return ext2_mkdir(fs, cwd_inode, name);
}

int fs_create(const char* name, u8 type) {
    (void)type;
    return ext2_create_file(fs, cwd_inode, name, 0x8000 | 0644);
}

int fs_delete(const char* name) {
    return ext2_unlink(fs, cwd_inode, name);
}

int fs_list(char* buffer, usize size) {
    (void)buffer;
    (void)size;
    return -1;
}

int fs_read(const char* name, char* buffer, usize size) {
    int inode_num = ext2_find_inode(fs, cwd_inode, name, NULL);
    if (inode_num < 0) return -1;

    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) < 0) return -1;

    return ext2_read_file(fs, &inode, 0, size, buffer);
}

int fs_write(const char* name, const char* data, usize size) {
    int inode_num = ext2_find_inode(fs, cwd_inode, name, NULL);
    if (inode_num < 0) {
        inode_num = ext2_create_file(fs, cwd_inode, name, 0x8000 | 0644);
        if (inode_num < 0) return -1;
    }

    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) < 0) return -1;

    int written = ext2_write_file(fs, &inode, 0, size, data);
    if (written > 0) {
        ext2_write_inode(fs, inode_num, &inode);
    }
    return written;
}

int fs_chdir(const char* dir) {
    if (strcmp(dir, "/") == 0) {
        cwd_inode = 2;
        strcpy(cwd, "/");
        return 0;
    }

    int inode_num = ext2_find_inode(fs, cwd_inode, dir, NULL);
    if (inode_num < 0) return -1;

    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) < 0) return -1;

    if (!(inode.mode & EXT2_S_IFDIR)) return -1;

    cwd_inode = inode_num;
    if (strcmp(cwd, "/") == 0) {
        strcpy(cwd, "/");
        strcat(cwd, dir);
    } else {
        strcat(cwd, "/");
        strcat(cwd, dir);
    }
    return 0;
}

const char* fs_getcwd(void) {
    return cwd;
}

int fs_exists(const char* name) {
    return ext2_find_inode(fs, cwd_inode, name, NULL) >= 0;
}

int fs_size(const char* name) {
    int inode_num = ext2_find_inode(fs, cwd_inode, name, NULL);
    if (inode_num < 0) return -1;

    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) < 0) return -1;

    return inode.size;
}

int fs_type(const char* name) {
    int inode_num = ext2_find_inode(fs, cwd_inode, name, NULL);
    if (inode_num < 0) return -1;

    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) < 0) return -1;

    return (inode.mode & EXT2_S_IFDIR) ? 1 : 0;
}

int fs_create_home(const char* username) {
    char home_path[64] = "/home/";
    strcat(home_path, username);

    int old_cwd = cwd_inode;
    char old_cwd_str[64];
    strcpy(old_cwd_str, cwd);

    cwd_inode = 2;
    strcpy(cwd, "/");

    int result = ext2_mkdir(fs, cwd_inode, username);

    cwd_inode = old_cwd;
    strcpy(cwd, old_cwd_str);

    return result;
}

int fs_mount_disk(void) {
    return 0;
}
