#include "vfs.h"

void vfs_init(void) {
    vga_write("vfs initialized (ext2 backend)\n", COLOUR_LIGHT_GREEN);
}

int vfs_mkdir(const char* name) { return fs_mkdir(name); }
int vfs_create(const char* name, u8 type) { return fs_create(name, type); }
int vfs_delete(const char* name) { return fs_delete(name); }
int vfs_list(char* buffer, usize size) { return fs_list(buffer, size); }
int vfs_read(const char* name, char* buffer, usize size) { return fs_read(name, buffer, size); }
int vfs_write(const char* name, const char* data, usize size) { return fs_write(name, data, size); }
int vfs_chdir(const char* dir) { return fs_chdir(dir); }
const char* vfs_getcwd(void) { return fs_getcwd(); }
int vfs_exists(const char* name) { return fs_exists(name); }
int vfs_size(const char* name) { return fs_size(name); }
int vfs_type(const char* name) { return fs_type(name); }
int vfs_create_home(const char* username) { return fs_create_home(username); }
int vfs_mount_disk(void) { return fs_mount_disk(); }
