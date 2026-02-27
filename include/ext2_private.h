#ifndef EXT2_PRIVATE_H
#define EXT2_PRIVATE_H

typedef struct {
    ata_disk_t* disk;
    u32 partition_start;

    ext2_superblock_t sb;
    ext2_bgdesc_t* bgdt;

    u32 block_size;
    u32 sectors_per_block;
    u32 inode_size;
    u32 num_groups;
} ext2_fs_internal_t;

#endif
