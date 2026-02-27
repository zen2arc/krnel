#include "kernel.h"
#include "ata.h"

struct ata_disk_s {
    u16 base;
    u16 ctrl;
    u16 slave;
    u8  present;
    u32 sectors;
    char model[41];
};

static struct ata_disk_s primary_disk;
static struct ata_disk_s secondary_disk;

static int ata_wait(ata_disk_t* disk, u8 drq_mask, int timeout) {
    struct ata_disk_s* d = (struct ata_disk_s*)disk;
    u8 status;
    while (timeout--) {
        status = inb(d->base + ATA_REG_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            if (drq_mask && !(status & ATA_STATUS_DRQ))
                continue;
            return 0;
        }
    }
    return -1;
}

static ata_disk_t* ata_detect(u16 base, int slave) {
    u16 ctrl = (base == ATA_PRIMARY_IO) ? ATA_PRIMARY_CTRL : ATA_SECONDARY_CTRL;
    struct ata_disk_s* disk = (base == ATA_PRIMARY_IO) ? &primary_disk : &secondary_disk;

    outb(base + ATA_REG_DRIVE, 0xA0 | (slave << 4));
    io_wait();

    outb(base + ATA_REG_SECTOR_CNT, 0);
    outb(base + ATA_REG_LBA_LOW, 0);
    outb(base + ATA_REG_LBA_MID, 0);
    outb(base + ATA_REG_LBA_HIGH, 0);
    outb(base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    u8 status = inb(base + ATA_REG_STATUS);
    if (status == 0) return NULL;

    while (inb(base + ATA_REG_STATUS) & ATA_STATUS_BSY);

    u8 cl = inb(base + ATA_REG_LBA_MID);
    u8 ch = inb(base + ATA_REG_LBA_HIGH);
    if (cl == 0x14 && ch == 0xEB) return NULL; /* ATAPI device, ignore */

    status = inb(base + ATA_REG_STATUS);
    if (status & ATA_STATUS_ERR) return NULL;

    u16 id[256];
    for (int i = 0; i < 256; i++) {
        id[i] = inw(base + ATA_REG_DATA);
    }

    disk->base = base;
    disk->ctrl = ctrl;
    disk->slave = slave;
    disk->present = 1;

    disk->sectors = *(u32*)&id[60];

    /* gittt model string */
    for (int i = 0; i < 20; i++) {
        u16 w = id[27 + i];
        disk->model[i*2] = (w >> 8) & 0xFF;
        disk->model[i*2 + 1] = w & 0xFF;
    }
    disk->model[40] = '\0';

    for (int i = 39; i >= 0; i--) {
        if (disk->model[i] != ' ') break;
        disk->model[i] = '\0';
    }

    return (ata_disk_t*)disk;
}

void ata_reset(ata_disk_t* disk) {
    struct ata_disk_s* d = (struct ata_disk_s*)disk;
    outb(d->ctrl, 0x04);
    io_wait();
    outb(d->ctrl, 0x00);
    ata_wait(disk, 0, ATA_TIMEOUT);
}

int ata_init(void) {
    int found = 0;

    kprint("ata_init: probing primary master...\n");
    ata_disk_t* primary_master = ata_detect(ATA_PRIMARY_IO, 0);
    if (primary_master) {
        kprint("ata0 master: ");
        kprint(primary_master->model);
        kprint("\n");
        found++;
    }

    kprint("ata_init: probing primary slave...\n");
    ata_disk_t* primary_slave = ata_detect(ATA_PRIMARY_IO, 1);
    if (primary_slave) {
        kprint("ata0 slave: ");
        kprint(primary_slave->model);
        kprint("\n");
        found++;
    }

    kprint("ata_init: probing secondary master...\n");
    ata_disk_t* secondary_master = ata_detect(ATA_SECONDARY_IO, 0);
    if (secondary_master) {
        kprint("ata1 master: ");
        kprint(secondary_master->model);
        kprint("\n");
        found++;
    }

    kprint("ata_init: probing secondary slave...\n");
    ata_disk_t* secondary_slave = ata_detect(ATA_SECONDARY_IO, 1);
    if (secondary_slave) {
        kprint("ata1 slave: ");
        kprint(secondary_slave->model);
        kprint("\n");
        found++;
    }

    if (found == 0) {
        kprint("ata_init: no disks found\n");
        return -1;
    }

    kprint("ata_init: found ");
    char num[4];
    itoa(found, num);
    kprint(num);
    kprint(" disk(s)\n");

    return 0;
}

int ata_read_sectors(ata_disk_t* disk, u32 lba, u8 count, void* buffer) {
    struct ata_disk_s* d = (struct ata_disk_s*)disk;
    if (!d->present) return -1;
    if (count == 0) return -1;
    if (lba + count > d->sectors) return -1;

    if (ata_wait(disk, 0, ATA_TIMEOUT) != 0)
        return -1;

    outb(d->base + ATA_REG_DRIVE, 0xE0 | (d->slave << 4) | ((lba >> 24) & 0x0F));
    outb(d->base + ATA_REG_SECTOR_CNT, count);
    outb(d->base + ATA_REG_LBA_LOW,   lba & 0xFF);
    outb(d->base + ATA_REG_LBA_MID,  (lba >> 8) & 0xFF);
    outb(d->base + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(d->base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    u16* buf = (u16*)buffer;
    for (int i = 0; i < count; i++) {
        if (ata_wait(disk, ATA_STATUS_DRQ, ATA_TIMEOUT) != 0)
            return -1;
        for (int j = 0; j < 256; j++)
            buf[i * 256 + j] = inw(d->base + ATA_REG_DATA);
    }
    return count * ATA_SECTOR_SIZE;
}

int ata_write_sectors(ata_disk_t* disk, u32 lba, u8 count, const void* buffer) {
    struct ata_disk_s* d = (struct ata_disk_s*)disk;
    if (!d->present) return -1;
    if (count == 0) return -1;
    if (lba + count > d->sectors) return -1;

    if (ata_wait(disk, 0, ATA_TIMEOUT) != 0)
        return -1;

    outb(d->base + ATA_REG_DRIVE, 0xE0 | (d->slave << 4) | ((lba >> 24) & 0x0F));
    outb(d->base + ATA_REG_SECTOR_CNT, count);
    outb(d->base + ATA_REG_LBA_LOW,   lba & 0xFF);
    outb(d->base + ATA_REG_LBA_MID,  (lba >> 8) & 0xFF);
    outb(d->base + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(d->base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    u16* buf = (u16*)buffer;
    for (int i = 0; i < count; i++) {
        if (ata_wait(disk, ATA_STATUS_DRQ, ATA_TIMEOUT) != 0)
            return -1;
        for (int j = 0; j < 256; j++)
            outw(d->base + ATA_REG_DATA, buf[i * 256 + j]);

        outb(d->base + ATA_REG_COMMAND, ATA_CMD_FLUSH);
        ata_wait(disk, 0, ATA_TIMEOUT);
    }

    return count * ATA_SECTOR_SIZE;
}

ata_disk_t* ata_get_primary(void)   {
    return (ata_disk_t*)&primary_disk;
}

ata_disk_t* ata_get_secondary(void) {
    return (ata_disk_t*)&secondary_disk;
}
