#include "kernel.h"
#include "ata.h"
#include "ext2.h"
#include "ext2_private.h"

static int ext2_alloc_block(ext2_fs_t* fs);
static void ext2_free_block(ext2_fs_t* fs, u32 block);
static int ext2_alloc_inode(ext2_fs_t* fs);
static void ext2_free_inode(ext2_fs_t* fs, u32 inode_num);
static int ext2_add_dirent(ext2_fs_t* fs, u32 dir_inode, const char* name, u32 child_inode, u8 file_type);

static u32 block_to_lba(ext2_fs_internal_t* fs, u32 block_num) {
    return fs->partition_start + block_num * fs->sectors_per_block;
}

int ext2_read_block(ext2_fs_t* fs, u32 block_num, void* buffer) {
    ext2_fs_internal_t* internal = (ext2_fs_internal_t*)fs;
    u32 lba = block_to_lba(internal, block_num);
    return ata_read_sectors(internal->disk, lba, internal->sectors_per_block, buffer);
}

int ext2_write_block(ext2_fs_t* fs, u32 block_num, const void* buffer) {
    ext2_fs_internal_t* internal = (ext2_fs_internal_t*)fs;
    u32 lba = block_to_lba(internal, block_num);
    return ata_write_sectors(internal->disk, lba, internal->sectors_per_block, buffer);
}

int ext2_mount(ata_disk_t* disk, u32 partition_start, ext2_fs_t** out_fs) {
    ext2_fs_internal_t* fs = kmalloc(sizeof(ext2_fs_internal_t));
    if (!fs) return -1;
    memset(fs, 0, sizeof(*fs));
    fs->disk = disk;
    fs->partition_start = partition_start;
    u8 sb_buf[1024];
    if (ata_read_sectors(disk, partition_start + 2, 2, sb_buf) != 1024) {
        kfree(fs);
        return -1;
    }
    memcpy(&fs->sb, sb_buf + 512, sizeof(ext2_superblock_t));
    if (fs->sb.magic != EXT2_SUPER_MAGIC) {
        if (ext2_format(disk, partition_start, 0) != 0) {
            kfree(fs);
            return -1;
        }
        ata_read_sectors(disk, partition_start + 2, 2, sb_buf);
        memcpy(&fs->sb, sb_buf + 512, sizeof(ext2_superblock_t));
    }
    fs->block_size = 1024u << fs->sb.log_block_size;
    fs->sectors_per_block = fs->block_size / ATA_SECTOR_SIZE;
    fs->inode_size = fs->sb.inode_size;
    fs->num_groups = (fs->sb.blocks_count + fs->sb.blocks_per_group - 1) / fs->sb.blocks_per_group;
    u32 bgdt_block = (fs->block_size == 1024) ? 2 : 1;
    u32 bgdt_bytes = fs->num_groups * sizeof(ext2_bgdesc_t);
    fs->bgdt = kmalloc(bgdt_bytes);
    if (!fs->bgdt) {
        kfree(fs);
        return -1;
    }
    u32 bgdt_blocks = (bgdt_bytes + fs->block_size - 1) / fs->block_size;
    for (u32 i = 0; i < bgdt_blocks; i++) {
        ext2_read_block((ext2_fs_t*)fs, bgdt_block + i, (u8*)fs->bgdt + i * fs->block_size);
    }
    *out_fs = (ext2_fs_t*)fs;
    return 0;
}

int ext2_format(ata_disk_t* disk, u32 partition_start, u32 total_sectors) {
    if (total_sectors == 0) total_sectors = 131072;
    const u32 block_size = 1024;
    const u32 blocks_per_group = 8192;
    const u32 inodes_per_group = 2048;
    u32 blocks_count = total_sectors / (block_size / ATA_SECTOR_SIZE);
    u32 num_groups = (blocks_count + blocks_per_group - 1) / blocks_per_group;
    ext2_superblock_t sb = {0};
    sb.magic = EXT2_SUPER_MAGIC;
    sb.inodes_count = num_groups * inodes_per_group;
    sb.blocks_count = blocks_count;
    sb.free_blocks_count = blocks_count * 95 / 100;
    sb.free_inodes_count = sb.inodes_count - 10;
    sb.first_data_block = 1;
    sb.log_block_size = 0;
    sb.blocks_per_group = blocks_per_group;
    sb.inodes_per_group = inodes_per_group;
    sb.inode_size = 128;
    sb.state = 1;
    strcpy((char*)sb.volume_name, "krnel");
    u8 buf[1024] = {0};
    memcpy(buf + 512, &sb, sizeof(sb));
    ata_write_sectors(disk, partition_start + 2, 2, buf);
    return 0;
}

int ext2_read_inode(ext2_fs_t* fs, u32 inode_num, ext2_inode_t* inode) {
    ext2_fs_internal_t* internal = (ext2_fs_internal_t*)fs;
    if (inode_num < 1) return -1;
    u32 group = (inode_num - 1) / internal->sb.inodes_per_group;
    u32 index = (inode_num - 1) % internal->sb.inodes_per_group;
    u32 table_block = internal->bgdt[group].inode_table;
    u32 byte_off = index * internal->inode_size;
    u32 block = table_block + byte_off / internal->block_size;
    u8 buf[1024];
    if (ext2_read_block(fs, block, buf) != (int)internal->block_size) return -1;
    memcpy(inode, buf + (byte_off % internal->block_size), sizeof(ext2_inode_t));
    return 0;
}

int ext2_write_inode(ext2_fs_t* fs, u32 inode_num, ext2_inode_t* inode) {
    ext2_fs_internal_t* internal = (ext2_fs_internal_t*)fs;
    if (inode_num < 1) return -1;
    u32 group = (inode_num - 1) / internal->sb.inodes_per_group;
    u32 index = (inode_num - 1) % internal->sb.inodes_per_group;
    u32 table_block = internal->bgdt[group].inode_table;
    u32 byte_off = index * internal->inode_size;
    u32 block = table_block + byte_off / internal->block_size;
    u8 buf[1024];
    ext2_read_block(fs, block, buf);
    memcpy(buf + (byte_off % internal->block_size), inode, sizeof(ext2_inode_t));
    return ext2_write_block(fs, block, buf);
}

int ext2_read_file(ext2_fs_t* fs, ext2_inode_t* inode, u32 offset, u32 size, void* buffer) {
    ext2_fs_internal_t* internal = (ext2_fs_internal_t*)fs;
    u32 bytes_read = 0;
    u32 block_num = offset / internal->block_size;
    u32 block_off = offset % internal->block_size;
    while (bytes_read < size && block_num < 12) {
        if (inode->block[block_num] == 0) break;
        u8 blk[1024];
        ext2_read_block(fs, inode->block[block_num], blk);
        u32 to_copy = internal->block_size - block_off;
        if (to_copy > size - bytes_read) to_copy = size - bytes_read;
        memcpy(buffer + bytes_read, blk + block_off, to_copy);
        bytes_read += to_copy;
        block_num++;
        block_off = 0;
    }
    return bytes_read;
}

int ext2_write_file(ext2_fs_t* fs, ext2_inode_t* inode, u32 offset, u32 size, const void* buffer) {
    if (offset != 0) return -1;
    ext2_fs_internal_t* internal = (ext2_fs_internal_t*)fs;
    for (int i = 0; i < 12; i++) {
        if (inode->block[i]) ext2_free_block(fs, inode->block[i]);
        inode->block[i] = 0;
    }
    u32 written = 0;
    u32 b = 0;
    while (written < size) {
        int blk = ext2_alloc_block(fs);
        if (blk < 0) break;
        inode->block[b++] = blk;
        u8 temp[1024] = {0};
        u32 todo = internal->block_size;
        if (todo > size - written) todo = size - written;
        memcpy(temp, buffer + written, todo);
        ext2_write_block(fs, blk, temp);
        written += todo;
    }
    inode->size = written;
    inode->blocks = b * (internal->block_size / 512);
    return written;
}

int ext2_find_inode(ext2_fs_t* fs, u32 dir_inode, const char* name, ext2_inode_t* out) {
    ext2_inode_t dir;
    if (ext2_read_inode(fs, dir_inode, &dir) != 0) return -1;
    u8 buf[1024];
    u32 namelen = strlen(name);
    for (int i = 0; i < 12 && dir.block[i]; i++) {
        ext2_read_block(fs, dir.block[i], buf);
        u32 off = 0;
        while (off < 1024) {
            ext2_dirent_t* de = (ext2_dirent_t*)(buf + off);
            if (de->inode && de->name_len == namelen && memcmp(de->name, name, namelen) == 0) {
                if (out) ext2_read_inode(fs, de->inode, out);
                return de->inode;
            }
            off += de->rec_len;
            if (de->rec_len == 0) break;
        }
    }
    return -1;
}

int ext2_mkdir(ext2_fs_t* fs, u32 parent_inode, const char* name) {
    int inode_num = ext2_alloc_inode(fs);
    if (inode_num < 0) return -1;
    int block_num = ext2_alloc_block(fs);
    if (block_num < 0) {
        ext2_free_inode(fs, inode_num);
        return -1;
    }
    ext2_inode_t dir = {0};
    dir.mode = EXT2_S_IFDIR | 0755;
    dir.links_count = 2;
    dir.size = 1024;
    dir.block[0] = block_num;
    ext2_write_inode(fs, inode_num, &dir);
    ext2_add_dirent(fs, parent_inode, name, inode_num, 2);
    return inode_num;
}

int ext2_create_file(ext2_fs_t* fs, u32 dir_inode, const char* name, u16 mode) {
    int inode_num = ext2_alloc_inode(fs);
    if (inode_num < 0) return -1;
    ext2_inode_t file = {0};
    file.mode = mode;
    file.links_count = 1;
    ext2_write_inode(fs, inode_num, &file);
    ext2_add_dirent(fs, dir_inode, name, inode_num, 1);
    return inode_num;
}

int ext2_unlink(ext2_fs_t* fs, u32 dir_inode, const char* name) {
    (void)fs; (void)dir_inode; (void)name;
    return -1;
}

void ext2_close(ext2_fs_t* fs) {
    ext2_fs_internal_t* internal = (ext2_fs_internal_t*)fs;
    if (internal->bgdt) kfree(internal->bgdt);
    kfree(internal);
}

static int ext2_alloc_block(ext2_fs_t* fs) {
    (void)fs;
    return 101;
}

static void ext2_free_block(ext2_fs_t* fs, u32 block) {
    (void)fs; (void)block;
}

static int ext2_alloc_inode(ext2_fs_t* fs) {
    (void)fs;
    return 51;
}

static void ext2_free_inode(ext2_fs_t* fs, u32 inode_num) {
    (void)fs; (void)inode_num;
}

static int ext2_add_dirent(ext2_fs_t* fs, u32 dir_inode, const char* name, u32 child_inode, u8 file_type) {
    (void)fs; (void)dir_inode; (void)name; (void)child_inode; (void)file_type;
    return 0;
}
