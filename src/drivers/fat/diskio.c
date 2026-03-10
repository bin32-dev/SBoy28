#include "source/ff.h"
#include "source/diskio.h"

#include "drivers/filesystem.h"
#include <stdint.h>

DSTATUS disk_status(BYTE pdrv) {
    if (fs_disk_status(pdrv) == 0) {
        return 0;
    }
    return STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv) {
    if (fs_disk_initialize(pdrv) == 0) {
        return 0;
    }
    return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (!buff || count == 0) {
        return RES_PARERR;
    }

    if (fs_disk_read_sectors(pdrv, buff, (uint32_t)sector, (uint32_t)count) == 0) {
        return RES_OK;
    }

    return RES_ERROR;
}

#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (!buff || count == 0) {
        return RES_PARERR;
    }

    if (fs_disk_write_sectors(pdrv, buff, (uint32_t)sector, (uint32_t)count) == 0) {
        return RES_OK;
    }

    return RES_ERROR;
}
#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    uint32_t sectors;

    if (fs_disk_status(pdrv) != 0) {
        return RES_NOTRDY;
    }

    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;

        case GET_SECTOR_COUNT:
            if (!buff) return RES_PARERR;
            if (fs_disk_get_sector_count(pdrv, &sectors) != 0) return RES_ERROR;
            *(DWORD *)buff = (DWORD)sectors;
            return RES_OK;

        case GET_SECTOR_SIZE:
            if (!buff) return RES_PARERR;
            *(WORD *)buff = 512;
            return RES_OK;

        case GET_BLOCK_SIZE:
            if (!buff) return RES_PARERR;
            *(DWORD *)buff = 1;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}
