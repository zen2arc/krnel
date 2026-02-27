#ifndef EXT2_H
#define EXT2_H
#include "kernel.h"
#include "ata.h"

#define EXT2_SUPER_MAGIC 0xEF53
#define EXT2_ROOT_INO 2
#define EXT2_S_IFREG 0x8000
#define EXT2_S_IFDIR 0x4000

typedef struct {
    u32 inodes_count;
    u32 blocks_count;
    u32 free_blocks_count;
    u32 free_inodes_count;
    u32 first_data_block;
    u32 log_block_size;
    u32 log_frag_size;
    u32 blocks_per_group;
    u32 frags_per_group;
    u32 inodes_per_group;
    u32 mtime;
    u32 wtime;
    u16 mnt_count;
    u16 max_mnt_count;
    u16 magic;
    u16 state;
    u16 errors;
    u16 minor_rev_level;
    u32 lastcheck;
    u32 checkinterval;
    u32 creator_os;
    u32 rev_level;
    u16 def_resuid;
    u16 def_resgid;
    u32 first_ino;
    u16 inode_size;
    u16 block_group_nr;
    char volume_name[16];
} ext2_superblock_t;

typedef struct {
    u32 block_bitmap;
    u32 inode_bitmap;
    u32 inode_table;
    u16 free_blocks_count;
    u16 free_inodes_count;
    u16 used_dirs_count;
    u16 pad;
} ext2_bgdesc_t;

typedef struct {
    u16 mode;
    u16 uid;
    u32 size;
    u32 atime;
    u32 ctime;
    u32 mtime;
    u16 gid;
    u16 links_count;
    u32 blocks;
    u32 block[15];
} ext2_inode_t;

typedef struct {
    u32 inode;
    u16 rec_len;
    u8 name_len;
    u8 file_type;
    char name[255];
} ext2_dirent_t;

int ext2_mount(ata_disk_t* disk, u32 partition_start, ext2_fs_t** out_fs);
int ext2_format(ata_disk_t* disk, u32 partition_start, u32 total_sectors);
int ext2_read_inode(ext2_fs_t* fs, u32 inode_num, ext2_inode_t* inode);
int ext2_write_inode(ext2_fs_t* fs, u32 inode_num, ext2_inode_t* inode);
int ext2_read_block(ext2_fs_t* fs, u32 block_num, void* buffer);
int ext2_write_block(ext2_fs_t* fs, u32 block_num, const void* buffer);
int ext2_read_file(ext2_fs_t* fs, ext2_inode_t* inode, u32 offset, u32 size, void* buffer);
int ext2_write_file(ext2_fs_t* fs, ext2_inode_t* inode, u32 offset, u32 size, const void* buffer);
int ext2_find_inode(ext2_fs_t* fs, u32 dir_inode, const char* name, ext2_inode_t* out);
int ext2_create_file(ext2_fs_t* fs, u32 dir_inode, const char* name, u16 mode);
int ext2_mkdir(ext2_fs_t* fs, u32 parent_inode, const char* name);
int ext2_unlink(ext2_fs_t* fs, u32 dir_inode, const char* name);
void ext2_close(ext2_fs_t* fs);

#endif
