#ifndef VFS_H
#define VFS_H

#include "kernel.h"

void vfs_init(void);

int vfs_mkdir(const char* name);
int vfs_create(const char* name, u8 type);
int vfs_delete(const char* name);
int vfs_list(char* buffer, usize size);
int vfs_read(const char* name, char* buffer, usize size);
int vfs_write(const char* name, const char* data, usize size);
int vfs_chdir(const char* dir);
const char* vfs_getcwd(void);
int vfs_exists(const char* name);
int vfs_size(const char* name);
int vfs_type(const char* name);
int vfs_create_home(const char* username);
int vfs_mount_disk(void);

#endif
