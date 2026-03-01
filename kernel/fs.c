#include "kernel.h"
#include "ata.h"
#include "ext2.h"

static ext2_fs_t* fs = NULL;
static u32 cwd_inode = 2;
static char cwd[64] = "/";

static int fs_resolve_inode(const char* path) {
    if (!path || !*path) return -1;
    if (strcmp(path, "/") == 0) return 2;

    u32 current = (path[0] == '/') ? 2 : cwd_inode;
    char component[64];
    const char* p = (path[0] == '/') ? path + 1 : path;

    while (*p) {
        int i = 0;
        while (*p && *p != '/' && i < 63) component[i++] = *p++;
        component[i] = '\0';
        if (*p == '/') p++;
        if (component[0] == '\0') continue;

        int inode = ext2_find_inode(fs, current, component, NULL);
        if (inode < 0) return -1;
        current = inode;
    }
    return current;
}

static int fs_get_parent(const char* path, char* basename) {
    if (!path || !*path) return -1;
    char tmp[128];
    strncpy(tmp, path, sizeof(tmp)-1); tmp[sizeof(tmp)-1] = '\0';

    char* last = strrchr(tmp, '/');
    if (!last) {
        strcpy(basename, tmp);
        return cwd_inode;
    }
    if (last == tmp) {
        strcpy(basename, last + 1);
        return 2;
    }
    *last = '\0';
    strcpy(basename, last + 1);
    return fs_resolve_inode(tmp);
}

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
    if (!name || !*name) return -1;
    char basename[64];
    int parent = fs_get_parent(name, basename);
    if (parent < 0 || !basename[0]) return -1;
    return ext2_mkdir(fs, parent, basename);
}

int fs_create(const char* name, u8 type) {
    (void)type;
    if (!name || !*name) return -1;
    char basename[64];
    int parent = fs_get_parent(name, basename);
    if (parent < 0 || !basename[0]) return -1;
    return ext2_create_file(fs, parent, basename, 0x8000 | 0644);
}

int fs_delete(const char* name) {
    return ext2_unlink(fs, cwd_inode, name);
}

int fs_list(char* buffer, usize size) {
    if (!fs || !buffer || size < 2) return -1;

    ext2_inode_t dir;
    if (ext2_read_inode(fs, cwd_inode, &dir) != 0)
        return -1;

    char* p = buffer;
    usize left = size - 1;
    int entries = 0;

    u8 blk[1024];

    for (int i = 0; i < 12 && dir.block[i]; ++i) {
        if (ext2_read_block(fs, dir.block[i], blk) <= 0)
            continue;

        u32 off = 0;
        while (off < 1024) {
            ext2_dirent_t* de = (ext2_dirent_t*)(blk + off);
            if (de->inode == 0 || de->rec_len == 0) break;

            if ((de->name_len == 1 && de->name[0] == '.') ||
                (de->name_len == 2 && de->name[0] == '.' && de->name[1] == '.')) {
                off += de->rec_len;
                continue;
            }

            usize namelen = de->name_len;
            if (namelen + 1 >= left) break;

            memcpy(p, de->name, namelen);
            p[namelen] = '\n';
            p += namelen + 1;
            left -= namelen + 1;
            entries++;

            off += de->rec_len;
        }
    }

    *p = '\0';
    return (entries > 0) ? (int)(p - buffer) : 0;
}

int fs_read(const char* name, char* buffer, usize size) {
    int inode_num = fs_resolve_inode(name);
    if (inode_num < 0) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) < 0) return -1;
    return ext2_read_file(fs, &inode, 0, size, buffer);
}

int fs_write(const char* name, const char* data, usize size) {
    int inode_num = fs_resolve_inode(name);
    if (inode_num < 0) {
        if (fs_create(name, 0) < 0) return -1;
        inode_num = fs_resolve_inode(name);
    }
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) < 0) return -1;
    int written = ext2_write_file(fs, &inode, 0, size, data);
    if (written > 0) ext2_write_inode(fs, inode_num, &inode);
    return written;
}

int fs_chdir(const char* dir) {
    if (!dir || !*dir) return -1;

    if (strcmp(dir, "/") == 0) {
        cwd_inode = 2;
        strcpy(cwd, "/");
        return 0;
    }

    if (strcmp(dir, "..") == 0) {
        if (cwd_inode == 2) return 0;
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
    return fs_resolve_inode(name) >= 0;
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
    if (!username || strcmp(username, "root") == 0)
        return 0;

    if (ext2_find_inode(fs, 2, "home", NULL) <= 0) {
        ext2_mkdir(fs, 2, "home");
    }

    int home_inode = ext2_find_inode(fs, 2, "home", NULL);
    if (home_inode <= 0) return -1;

    return ext2_mkdir(fs, home_inode, username);
}

int fs_mount_disk(void) {
    return 0;
}
