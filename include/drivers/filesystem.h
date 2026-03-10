#pragma once

#include <stdint.h>

#define FS_MAX_VOLUMES 3
#define FS_MAX_OPEN_FILES 16

/*
 * ATA device signatures from CL/CH after identify probe.
 */
typedef enum {
    FS_DEV_NONE = 0,
    FS_DEV_PATA,
    FS_DEV_PATAPI,
    FS_DEV_SATA,
    FS_DEV_SATAPI,
    FS_DEV_UNKNOWN
} fs_device_type_t;

typedef struct {
    uint8_t present;
    uint8_t slavebit;
    fs_device_type_t type;
    uint32_t sector_count;
    uint16_t io_base;
    uint16_t ctrl_base;
} fs_disk_info_t;

void filesystem_init(void);
int filesystem_is_ready(uint8_t drive);
const fs_disk_info_t* filesystem_get_disk_info(uint8_t drive);

/* Kernel file API (fd-style) */
int fs_open(const char *path);
int fs_read(int fd, void *buf, int size);
int fs_write(int fd, const void *buf, int size);
int fs_close(int fd);

/* Optional helper for integration tests/demo from kernel code. */
void filesystem_example_usage(void);

/* Internal glue used by FatFs disk I/O implementation. */
int fs_disk_initialize(uint8_t pdrv);
int fs_disk_status(uint8_t pdrv);
int fs_disk_read_sectors(uint8_t pdrv, void *buf, uint32_t lba, uint32_t count);
int fs_disk_write_sectors(uint8_t pdrv, const void *buf, uint32_t lba, uint32_t count);
int fs_disk_get_sector_count(uint8_t pdrv, uint32_t *out_sector_count);
