#include "kernel.h"
#include "ata.h"
#include "ext2.h"

static ext2_fs_t* fs     = NULL;
static u32        cwd_inode = 2;
static char       cwd[128]  = "/";

static int fs_resolve_inode(const char* path) {
    if (!path || !*path) return -1;
    if (strcmp(path, "/") == 0) return 2;

    u32         current = (path[0] == '/') ? 2 : cwd_inode;
    char        component[64];
    const char* p = (path[0] == '/') ? path + 1 : path;

    while (*p) {
        int i = 0;
        while (*p && *p != '/' && i < 63) component[i++] = *p++;
        component[i] = '\0';
        if (*p == '/') p++;
        if (component[0] == '\0') continue;
        int inode = ext2_find_inode(fs, current, component, NULL);
        if (inode < 0) return -1;
        current = (u32)inode;
    }
    return (int)current;
}

static int fs_get_parent(const char* path, char* basename) {
    if (!path || !*path) return -1;
    char tmp[128];
    strncpy(tmp, path, sizeof(tmp)-1); tmp[sizeof(tmp)-1] = '\0';

    char* last = strrchr(tmp, '/');
    if (!last) { strcpy(basename, tmp); return (int)cwd_inode; }
    if (last == tmp) { strcpy(basename, last + 1); return 2; }
    *last = '\0';
    strcpy(basename, last + 1);
    return fs_resolve_inode(tmp);
}

static void fs_mkdirp(const char* path) {
    char buf[128];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* p = buf;
    if (*p == '/') p++;   /* skip leading slash */

    while (*p) {
        char* slash = strchr(p, '/');
        if (slash) *slash = '\0';

        /* build partial path: "/" + everything up to and including current component */
        char partial[128];
        partial[0] = '/';
        partial[1] = '\0';
        /* p points into buf; append buf[0..component_end] after the leading '/' */
        usize prefix_len = (usize)(p - buf);          /* chars before current component */
        usize comp_len   = strlen(p);
        usize total      = prefix_len + comp_len;
        if (total < sizeof(partial) - 1) {
            memcpy(partial + 1, buf, total);
            partial[1 + total] = '\0';
        }

        /* create if missing */
        if (fs_resolve_inode(partial) < 0) {
            char base[64];
            int parent = fs_get_parent(partial, base);
            if (parent >= 0 && base[0])
                ext2_mkdir(fs, (u32)parent, base);
        }

        if (!slash) break;
        *slash = '/';   /* restore so buf remains a valid path string */
        p = slash + 1;
    }
}

void fs_init(void) {
    ata_init();
    ata_disk_t* disk = ata_get_primary();
    if (!disk) {
        vga_write("fs: no disk found, using RAM fallback\n", COLOUR_LIGHT_RED);
        return;
    }
    if (ext2_mount(disk, 0, &fs) != 0) {
        vga_write("fs: formatting disk...\n", COLOUR_YELLOW);
        ext2_format(disk, 0, 131072);
        if (ext2_mount(disk, 0, &fs) != 0) {
            vga_write("fs: fatal mount failure\n", COLOUR_RED);
            khang();
        }
    }

    /* ---- standard UNIX directory tree ---- */
    static const char* std_dirs[] = {
        /* base dirs */
        "bin", "sbin", "lib", "lib64",
        "usr", "usr/bin", "usr/sbin", "usr/lib", "usr/include", "usr/share",
        "etc", "etc/init.d",
        "home",
        "root",
        "tmp",
        "var", "var/log", "var/run", "var/tmp",
        "proc",
        "sys",
        "dev",
        "mnt",
        "opt",
        "run",
        "boot",
        NULL
    };

    for (int i = 0; std_dirs[i]; i++) {
        if (fs_resolve_inode(std_dirs[i]) < 0)
            fs_mkdirp(std_dirs[i]);
    }
}

int fs_mkdir(const char* name) {
    if (!name || !*name) return -1;
    if (!fs) return -1;
    char basename[64];
    int parent = fs_get_parent(name, basename);
    if (parent < 0 || !basename[0]) return -1;
    return ext2_mkdir(fs, (u32)parent, basename);
}

int fs_create(const char* name, u8 type) {
    (void)type;
    if (!name || !*name || !fs) return -1;
    char basename[64];
    int parent = fs_get_parent(name, basename);
    if (parent < 0 || !basename[0]) return -1;
    return ext2_create_file(fs, (u32)parent, basename, 0x8000 | 0644);
}

int fs_delete(const char* name) {
    if (!fs) return -1;
    return ext2_unlink(fs, cwd_inode, name);
}

int fs_list(char* buffer, usize size) {
    if (!fs || !buffer || size < 2) return -1;

    ext2_inode_t dir;
    if (ext2_read_inode(fs, cwd_inode, &dir) != 0) return -1;

    char*  p     = buffer;
    usize  left  = size - 1;
    int    count = 0;
    u8     blk[1024];

    for (int i = 0; i < 12 && dir.block[i]; i++) {
        if (ext2_read_block(fs, dir.block[i], blk) <= 0) continue;
        u32 off = 0;
        while (off < 1024) {
            ext2_dirent_t* de = (ext2_dirent_t*)(blk + off);
            if (de->inode == 0 || de->rec_len == 0) break;
            /* skip . and .. */
            if ((de->name_len == 1 && de->name[0] == '.') ||
                (de->name_len == 2 && de->name[0] == '.' && de->name[1] == '.')) {
                off += de->rec_len; continue;
            }
            usize nl = de->name_len;
            if (nl + 2 >= left) break;
            memcpy(p, de->name, nl);
            /* append '/' for directories */
            if (de->file_type == 2) { p[nl++] = '/'; }
            p[nl++] = '\n';
            p   += nl;
            left -= nl;
            count++;
            off += de->rec_len;
        }
    }
    *p = '\0';
    return count > 0 ? (int)(p - buffer) : 0;
}

int fs_read(const char* name, char* buffer, usize size) {
    if (!fs) return -1;
    int inode_num = fs_resolve_inode(name);
    if (inode_num < 0) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(fs, (u32)inode_num, &inode) < 0) return -1;
    return ext2_read_file(fs, &inode, 0, (u32)size, buffer);
}

int fs_write(const char* name, const char* data, usize size) {
    if (!fs) return -1;
    int inode_num = fs_resolve_inode(name);
    if (inode_num < 0) {
        if (fs_create(name, 0) < 0) return -1;
        inode_num = fs_resolve_inode(name);
        if (inode_num < 0) return -1;
    }
    ext2_inode_t inode;
    if (ext2_read_inode(fs, (u32)inode_num, &inode) < 0) return -1;
    int written = ext2_write_file(fs, &inode, 0, (u32)size, data);
    if (written > 0) ext2_write_inode(fs, (u32)inode_num, &inode);
    return written;
}

int fs_chdir(const char* dir) {
    if (!dir || !*dir || !fs) return -1;

    if (strcmp(dir, "/") == 0) {
        cwd_inode = 2; strcpy(cwd, "/"); return 0;
    }

    if (strcmp(dir, "..") == 0) {
        /* walk back one component in cwd string */
        char* last = strrchr(cwd, '/');
        if (!last || last == cwd) { cwd_inode = 2; strcpy(cwd, "/"); return 0; }
        *last = '\0';
        /* resolve new cwd */
        int ni = fs_resolve_inode(cwd);
        if (ni < 0) { strcpy(cwd, "/"); cwd_inode = 2; return 0; }
        cwd_inode = (u32)ni;
        return 0;
    }

    int inode_num = ext2_find_inode(fs, cwd_inode, dir, NULL);
    if (inode_num < 0) {
        /* try absolute path */
        inode_num = fs_resolve_inode(dir);
        if (inode_num < 0) return -1;
    }

    ext2_inode_t inode;
    if (ext2_read_inode(fs, (u32)inode_num, &inode) < 0) return -1;
    if (!(inode.mode & EXT2_S_IFDIR)) return -1;

    cwd_inode = (u32)inode_num;

    /* update cwd string */
    if (dir[0] == '/') {
        strncpy(cwd, dir, sizeof(cwd) - 1);
        cwd[sizeof(cwd)-1] = '\0';
    } else {
        if (strcmp(cwd, "/") != 0) {
            int l = (int)strlen(cwd);
            if (l + 1 + (int)strlen(dir) < (int)sizeof(cwd) - 1) {
                cwd[l] = '/';
                strcpy(cwd + l + 1, dir);
            }
        } else {
            snprintf(cwd, sizeof(cwd), "/%s", dir);
        }
    }
    return 0;
}

const char* fs_getcwd(void)              { return cwd; }

int fs_exists(const char* name) {
    if (!fs) return 0;
    return fs_resolve_inode(name) >= 0;
}

int fs_size(const char* name) {
    if (!fs) return -1;
    int n = ext2_find_inode(fs, cwd_inode, name, NULL);
    if (n < 0) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(fs, (u32)n, &inode) < 0) return -1;
    return (int)inode.size;
}

int fs_type(const char* name) {
    if (!fs) return -1;
    int n = ext2_find_inode(fs, cwd_inode, name, NULL);
    if (n < 0) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(fs, (u32)n, &inode) < 0) return -1;
    return (inode.mode & EXT2_S_IFDIR) ? 1 : 0;
}

int fs_create_home(const char* username) {
    if (!username || !fs) return -1;
    if (strcmp(username, "root") == 0) return 0;

    if (fs_resolve_inode("home") < 0)
        ext2_mkdir(fs, 2, "home");

    int home_inode = ext2_find_inode(fs, 2, "home", NULL);
    if (home_inode <= 0) return -1;
    return ext2_mkdir(fs, (u32)home_inode, username);
}

int fs_mount_disk(void) { return 0; }
