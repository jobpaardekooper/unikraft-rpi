/*-----------------------------------------------------------------------
 * Low-level disk I/O interface for FatFs  (integer type definitions)
 * Compatible with ChaN FatFs R0.15.
 *-----------------------------------------------------------------------*/

#ifndef _DISKIO_H_
#define _DISKIO_H_

#include <stdint.h>

/* Status bits returned by disk_status() */
#define STA_NOINIT   0x01   /* Drive not initialised */
#define STA_NODISK   0x02   /* No medium in the drive */
#define STA_PROTECT  0x04   /* Write protected */

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef unsigned int UINT;
typedef DWORD    LBA_t;
typedef BYTE     DSTATUS;

typedef enum {
    RES_OK = 0,
    RES_ERROR,
    RES_WRPRT,
    RES_NOTRDY,
    RES_PARERR
} DRESULT;

/* ioctl command codes */
#define CTRL_SYNC       0
#define GET_SECTOR_COUNT 1
#define GET_SECTOR_SIZE  2
#define GET_BLOCK_SIZE   3

DSTATUS disk_initialize(BYTE pdrv);
DSTATUS disk_status(BYTE pdrv);
DRESULT disk_read(BYTE pdrv, BYTE *buf, LBA_t sector, UINT count);
DRESULT disk_write(BYTE pdrv, const BYTE *buf, LBA_t sector, UINT count);
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buf);

#endif /* _DISKIO_H_ */
