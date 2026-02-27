#ifndef ATA_H
#define ATA_H

#include "kernel.h"

/* ATA I/O Ports */
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

/* ATA registers */
#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECTOR_CNT  0x02
#define ATA_REG_LBA_LOW     0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HIGH    0x05
#define ATA_REG_DRIVE       0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07

/* ATA commands */
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_FLUSH       0xE7

/* ATA status bits */
#define ATA_STATUS_ERR      0x01
#define ATA_STATUS_DRQ      0x08
#define ATA_STATUS_SRV      0x10
#define ATA_STATUS_DF       0x20
#define ATA_STATUS_RDY      0x40
#define ATA_STATUS_BSY      0x80

/* disk geometry */
#define ATA_SECTOR_SIZE     512
#define ATA_MAX_SECTORS     256
#define ATA_TIMEOUT         100000

/* function prototypes */
int ata_init(void);
int ata_read_sectors(ata_disk_t* disk, u32 lba, u8 count, void* buffer);
int ata_write_sectors(ata_disk_t* disk, u32 lba, u8 count, const void* buffer);
ata_disk_t* ata_get_primary(void);
ata_disk_t* ata_get_secondary(void);
void ata_reset(ata_disk_t* disk);

#endif /* ATA_H */
