#include "drivers/filesystem.h"

#include "common/ports.h"
#include "common/utils.h"
#include "ff.h"

#include <stdint.h>

#define ATA_PRIMARY_IO_BASE 0x1F0
#define ATA_PRIMARY_CTRL_BASE 0x3F6
#define ATA_SECONDARY_IO_BASE 0x170
#define ATA_SECONDARY_CTRL_BASE 0x376

#define ATA_REG_DATA      0x00
#define ATA_REG_SECCOUNT0 0x02
#define ATA_REG_LBA0      0x03
#define ATA_REG_LBA1      0x04
#define ATA_REG_LBA2      0x05
#define ATA_REG_HDDEVSEL  0x06
#define ATA_REG_COMMAND   0x07
#define ATA_REG_STATUS    0x07

#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_WRITE_PIO  0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY   0xEC

#define ATA_SR_BSY 0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

#define FS_OK 0
#define FS_ERR -1

typedef struct {
    FIL file;
    uint8_t used;
} fs_fd_entry_t;

static FATFS g_volumes[FS_MAX_VOLUMES];
static fs_disk_info_t g_disks[FS_MAX_VOLUMES];
static fs_fd_entry_t g_fd_table[FS_MAX_OPEN_FILES];

typedef struct {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t slavebit;
} ata_slot_t;

static const ata_slot_t g_ata_slots[FS_MAX_VOLUMES] = {
    { ATA_PRIMARY_IO_BASE, ATA_PRIMARY_CTRL_BASE, 0 },
    { ATA_PRIMARY_IO_BASE, ATA_PRIMARY_CTRL_BASE, 1 },
    { ATA_SECONDARY_IO_BASE, ATA_SECONDARY_CTRL_BASE, 0 }
};

static inline void outsw(uint16_t port, const void *buf, uint32_t words) {
    __asm__ volatile("cld; rep outsw" : : "d"(port), "S"(buf), "c"(words) : "memory");
}

static inline void insw(uint16_t port, void *buf, uint32_t words) {
    __asm__ volatile("cld; rep insw" : "=D"(buf), "=c"(words) : "d"(port), "0"(buf), "1"(words) : "memory");
}

static void ata_io_delay(uint16_t io_base) {
    (void)inb(io_base + ATA_REG_STATUS);
    (void)inb(io_base + ATA_REG_STATUS);
    (void)inb(io_base + ATA_REG_STATUS);
    (void)inb(io_base + ATA_REG_STATUS);
}

static int ata_wait_not_busy(uint16_t io_base) {
    for (uint32_t i = 0; i < 100000; ++i) {
        if (!(inb(io_base + ATA_REG_STATUS) & ATA_SR_BSY)) {
            return FS_OK;
        }
    }
    return FS_ERR;
}

static int ata_wait_drq(uint16_t io_base) {
    for (uint32_t i = 0; i < 100000; ++i) {
        uint8_t st = inb(io_base + ATA_REG_STATUS);
        if (st & ATA_SR_ERR) return FS_ERR;
        if (!(st & ATA_SR_BSY) && (st & ATA_SR_DRQ)) return FS_OK;
    }
    return FS_ERR;
}

static fs_device_type_t detect_devtype(int slavebit, fs_disk_info_t *ctrl) {
    if (!ctrl) return FS_DEV_NONE;

    const uint16_t io = ctrl->io_base;
    const uint16_t ctl = ctrl->ctrl_base;

    outb(ctl, 0x04);
    ata_io_delay(io);
    outb(ctl, 0x00);
    ata_io_delay(io);

    outb(io + ATA_REG_HDDEVSEL, (uint8_t)(0xA0 | (slavebit << 4)));
    ata_io_delay(io);

    uint8_t cl = inb(io + ATA_REG_LBA1);
    uint8_t ch = inb(io + ATA_REG_LBA2);

    if (cl == 0x00 && ch == 0x00) return FS_DEV_PATA;
    if (cl == 0x14 && ch == 0xEB) return FS_DEV_PATAPI;
    if (cl == 0x3C && ch == 0xC3) return FS_DEV_SATA;
    if (cl == 0x69 && ch == 0x96) return FS_DEV_SATAPI;
    return FS_DEV_UNKNOWN;
}

static int ata_identify_sector_count(fs_disk_info_t *disk) {
    uint16_t id_data[256];
    uint16_t io = disk->io_base;

    outb(io + ATA_REG_HDDEVSEL, (uint8_t)(0xA0 | (disk->slavebit << 4)));
    ata_io_delay(io);

    outb(io + ATA_REG_SECCOUNT0, 0);
    outb(io + ATA_REG_LBA0, 0);
    outb(io + ATA_REG_LBA1, 0);
    outb(io + ATA_REG_LBA2, 0);
    outb(io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    if (inb(io + ATA_REG_STATUS) == 0) return FS_ERR;
    if (ata_wait_drq(io) != FS_OK) return FS_ERR;

    insw(io + ATA_REG_DATA, id_data, 256);
    disk->sector_count = ((uint32_t)id_data[61] << 16) | id_data[60];
    return FS_OK;
}

static int ata_pio_read28(const fs_disk_info_t *disk, uint32_t lba, void *buffer) {
    uint16_t io = disk->io_base;

    if (ata_wait_not_busy(io) != FS_OK) return FS_ERR;

    outb(io + ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | (disk->slavebit << 4) | ((lba >> 24) & 0x0F)));
    outb(io + ATA_REG_SECCOUNT0, 1);
    outb(io + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(io + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(io + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    if (ata_wait_drq(io) != FS_OK) return FS_ERR;

    insw(io + ATA_REG_DATA, buffer, 256);
    ata_io_delay(io);
    return FS_OK;
}

static int ata_pio_write28(const fs_disk_info_t *disk, uint32_t lba, const void *buffer) {
    uint16_t io = disk->io_base;

    if (ata_wait_not_busy(io) != FS_OK) return FS_ERR;

    outb(io + ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | (disk->slavebit << 4) | ((lba >> 24) & 0x0F)));
    outb(io + ATA_REG_SECCOUNT0, 1);
    outb(io + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(io + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(io + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    if (ata_wait_drq(io) != FS_OK) return FS_ERR;

    outsw(io + ATA_REG_DATA, buffer, 256);
    outb(io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    if (ata_wait_not_busy(io) != FS_OK) return FS_ERR;

    return FS_OK;
}

int fs_disk_initialize(uint8_t pdrv) {
    if (pdrv >= FS_MAX_VOLUMES) return FS_ERR;
    if (!g_disks[pdrv].present) return FS_ERR;
    return FS_OK;
}

int fs_disk_status(uint8_t pdrv) {
    if (pdrv >= FS_MAX_VOLUMES) return FS_ERR;
    return g_disks[pdrv].present ? FS_OK : FS_ERR;
}

int fs_disk_read_sectors(uint8_t pdrv, void *buf, uint32_t lba, uint32_t count) {
    if (pdrv >= FS_MAX_VOLUMES || !buf || count == 0) return FS_ERR;
    if (!g_disks[pdrv].present) return FS_ERR;

    uint8_t *cursor = (uint8_t *)buf;
    for (uint32_t i = 0; i < count; ++i) {
        if (ata_pio_read28(&g_disks[pdrv], lba + i, cursor + (i * 512U)) != FS_OK) {
            return FS_ERR;
        }
    }
    return FS_OK;
}

int fs_disk_write_sectors(uint8_t pdrv, const void *buf, uint32_t lba, uint32_t count) {
    if (pdrv >= FS_MAX_VOLUMES || !buf || count == 0) return FS_ERR;
    if (!g_disks[pdrv].present) return FS_ERR;

    const uint8_t *cursor = (const uint8_t *)buf;
    for (uint32_t i = 0; i < count; ++i) {
        if (ata_pio_write28(&g_disks[pdrv], lba + i, cursor + (i * 512U)) != FS_OK) {
            return FS_ERR;
        }
    }
    return FS_OK;
}

int fs_disk_get_sector_count(uint8_t pdrv, uint32_t *out_sector_count) {
    if (pdrv >= FS_MAX_VOLUMES || !out_sector_count) return FS_ERR;
    if (!g_disks[pdrv].present) return FS_ERR;

    *out_sector_count = g_disks[pdrv].sector_count;
    return FS_OK;
}

static int fs_alloc_fd(void) {
    for (int i = 0; i < FS_MAX_OPEN_FILES; ++i) {
        if (!g_fd_table[i].used) {
            g_fd_table[i].used = 1;
            return i;
        }
    }
    return -1;
}

static int fs_map_fresult(FRESULT res) {
    if (res == FR_OK) return 0;
    if (res == FR_NO_FILE || res == FR_NO_PATH) return -2;
    if (res == FR_DENIED || res == FR_EXIST || res == FR_WRITE_PROTECTED) return -3;
    if (res == FR_NOT_READY || res == FR_DISK_ERR || res == FR_INT_ERR) return -4;
    return -1;
}

void filesystem_init(void) {
    memset(g_disks, 0, sizeof(g_disks));
    memset(g_fd_table, 0, sizeof(g_fd_table));

    for (uint8_t drive = 0; drive < FS_MAX_VOLUMES; ++drive) {
        fs_disk_info_t *d = &g_disks[drive];
        d->io_base = g_ata_slots[drive].io_base;
        d->ctrl_base = g_ata_slots[drive].ctrl_base;
        d->slavebit = g_ata_slots[drive].slavebit;

        d->type = detect_devtype(d->slavebit, d);
        d->present = (d->type == FS_DEV_PATA || d->type == FS_DEV_SATA);

        if (d->present && ata_identify_sector_count(d) != FS_OK) {
            d->present = 0;
            d->type = FS_DEV_NONE;
        }
    }

    if (g_disks[0].present) {
        (void)f_mount(&g_volumes[0], "0:", 1);
    }
    if (g_disks[1].present) {
        (void)f_mount(&g_volumes[1], "1:", 1);
    }
    if (g_disks[2].present) {
        (void)f_mount(&g_volumes[2], "2:", 1);
    }
}

int filesystem_is_ready(uint8_t drive) {
    if (drive >= FS_MAX_VOLUMES) return 0;
    return g_disks[drive].present;
}

const fs_disk_info_t *filesystem_get_disk_info(uint8_t drive) {
    if (drive >= FS_MAX_VOLUMES) return (const fs_disk_info_t *)0;
    return &g_disks[drive];
}

int fs_open(const char *path) {
    if (!path) return -1;

    int fd = fs_alloc_fd();
    if (fd < 0) return -1;

    FRESULT res = f_open(&g_fd_table[fd].file, path, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
    if (res != FR_OK) {
        g_fd_table[fd].used = 0;
        return fs_map_fresult(res);
    }

    return fd;
}

int fs_read(int fd, void *buf, int size) {
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !g_fd_table[fd].used || !buf || size < 0) return -1;

    UINT br = 0;
    FRESULT res = f_read(&g_fd_table[fd].file, buf, (UINT)size, &br);
    if (res != FR_OK) return fs_map_fresult(res);

    return (int)br;
}

int fs_write(int fd, const void *buf, int size) {
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !g_fd_table[fd].used || !buf || size < 0) return -1;

    UINT bw = 0;
    FRESULT res = f_write(&g_fd_table[fd].file, buf, (UINT)size, &bw);
    if (res != FR_OK) return fs_map_fresult(res);

    return (int)bw;
}

int fs_close(int fd) {
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !g_fd_table[fd].used) return -1;

    FRESULT res = f_close(&g_fd_table[fd].file);
    g_fd_table[fd].used = 0;

    return fs_map_fresult(res);
}

void filesystem_example_usage(void) {
    int fd;
    char text[] = "SBoy28 FAT write test\n";
    char rx[32];

    fd = fs_open("1:/test.txt");
    if (fd < 0) return;

    (void)fs_write(fd, text, (int)(sizeof(text) - 1));
    (void)f_lseek(&g_fd_table[fd].file, 0);
    (void)fs_read(fd, rx, (int)(sizeof(rx) - 1));
    (void)fs_close(fd);
}
