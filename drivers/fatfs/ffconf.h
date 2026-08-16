/*---------------------------------------------------------------------------
 * ffconf.h — FatFs R0.15 (rev 80386) configuration for Unikraft / BCM2837.
 *
 * FFCONF_DEF must match FF_DEFINED in ff.h (80386 for the abbrev/fatfs mirror).
 *---------------------------------------------------------------------------*/

#ifndef _FFCONF_H_
#define _FFCONF_H_

/* ── Revision ID — MUST match FF_DEFINED in ff.h ───────────────────────── */
#define FFCONF_DEF       80386

/* ── Functional configuration ──────────────────────────────────────────── */
#define FF_FS_READONLY   0          /* 0=R/W, 1=read-only                  */
#define FF_FS_MINIMIZE   0          /* 0=all features included              */
#define FF_USE_FIND      0
#define FF_USE_MKFS      0          /* no disk format (card pre-formatted)  */
#define FF_USE_FASTSEEK  0
#define FF_USE_EXPAND    0
#define FF_USE_CHMOD     0
#define FF_USE_LABEL     0
#define FF_USE_FORWARD   0
#define FF_USE_STRFUNC   2          /* 2=f_printf + f_gets enabled          */
#define FF_PRINT_LLI     0
#define FF_PRINT_FLOAT   0
#define FF_STRF_ENCODE   0

/* ── Code page ──────────────────────────────────────────────────────────── */
#define FF_CODE_PAGE     437        /* US ASCII                             */

/* ── Long file name ─────────────────────────────────────────────────────── */
#define FF_USE_LFN       0          /* 0=no LFN (8.3 names only)            */
#define FF_MAX_LFN       255
#define FF_LFN_UNICODE   0
#define FF_LFN_BUF       255
#define FF_SFN_BUF       12
#define FF_FS_RPATH      0
#define FF_PATH_DEPTH    10

/* ── Volume / drive configuration ──────────────────────────────────────── */
#define FF_VOLUMES       1
#define FF_STR_VOLUME_ID 0
#define FF_VOLUME_STRS   "SD"
#define FF_MULTI_PARTITION  0       /* single partition per volume          */
#define FF_MIN_SS        512
#define FF_MAX_SS        512        /* fixed 512-byte sectors               */
#define FF_LBA64         0
#define FF_MIN_GPT       0x10000000
#define FF_USE_TRIM      0
#define FF_FS_TINY       0
#define FF_FS_EXFAT      0

/* ── RTC — no RTC on bare-metal ─────────────────────────────────────────── */
#define FF_FS_NORTC      1
#define FF_NORTC_MON     1
#define FF_NORTC_MDAY    1
#define FF_NORTC_YEAR    2025
#define FF_FS_CRTIME     0
#define FF_FS_NOFSINFO   0

/* ── OS / locking ───────────────────────────────────────────────────────── */
#define FF_FS_LOCK       0          /* no OS-level file locking             */
#define FF_FS_REENTRANT  0
#define FF_FS_TIMEOUT    1000

#endif /* _FFCONF_H_ */
