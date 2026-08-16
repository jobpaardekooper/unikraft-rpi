/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * diskio.c — FatFs disk I/O adapter for uk_sdcard_*.
 *
 * Only physical drive 0 is supported (the single SD card).
 */

#include "diskio.h"
#include <uk/sdcard.h>

static int _sd_inited = 0;

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    if (uk_sdcard_init() == 0)
        _sd_inited = 1;
    return _sd_inited ? 0 : STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) return STA_NOINIT;
    return _sd_inited ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buf, LBA_t sector, UINT count)
{
    if (pdrv != 0 || !_sd_inited) return RES_NOTRDY;
    for (UINT i = 0; i < count; i++) {
        if (uk_sdcard_read_block(sector + i, buf + i * 512) != 0)
            return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buf, LBA_t sector, UINT count)
{
    if (pdrv != 0 || !_sd_inited) return RES_NOTRDY;
    for (UINT i = 0; i < count; i++) {
        if (uk_sdcard_write_block(sector + i, buf + i * 512) != 0)
            return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buf)
{
    if (pdrv != 0) return RES_PARERR;
    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;                      /* writes are synchronous */
    case GET_SECTOR_SIZE:
        *(WORD *)buf = 512;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buf = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
