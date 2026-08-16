/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/param.h shim — pulls in the standard C headers that
 * bcm2835_gpio.c expects to find transitively, and defines the few
 * FreeBSD constants used outside the bus/KOBJ subsystem.
 */
#pragma once
#include <sys/cdefs.h>  /* __diagused, __unused, __packed, ... */
#include <string.h>
#include <strings.h>   /* ffs(), strcasecmp() */
#include <stdio.h>
#include <errno.h>
#include <sys/types.h> /* FreeBSD types (our shim) */

/* ENOTSUP: not universally defined by glibc errno.h */
#ifndef ENOTSUP
#  ifdef EOPNOTSUPP
#    define ENOTSUP EOPNOTSUPP
#  else
#    define ENOTSUP 95
#  endif
#endif

/* Silence "unused variable" warnings from the probe path */
#define bootverbose  0

/* Resource allocation flags */
#define RF_ACTIVE    1

/* Resource types */
#define SYS_RES_MEMORY  1
#define SYS_RES_IRQ     3

/* Mutex assertion flag (used by BCM_GPIO_LOCK_ASSERT) */
#define MA_OWNED   0

/* Bus probe return values */
#define BUS_PROBE_DEFAULT   0
#define BUS_PROBE_NOWILDCARD (-2000000000)

/* BUS_PASS_* constants for EARLY_DRIVER_MODULE */
#define BUS_PASS_INTERRUPT  (1 << 2)
#define BUS_PASS_ORDER_LATE 9

#ifndef nitems
#define nitems(x)  (sizeof((x)) / sizeof((x)[0]))
#endif

/* Branch prediction hints */
#ifndef __predict_false
#  define __predict_false(x)  __builtin_expect(!!(x), 0)
#endif
#ifndef __predict_true
#  define __predict_true(x)   __builtin_expect(!!(x), 1)
#endif

/* __printflike — format string checker (GCC attribute) */
#ifndef __printflike
#  define __printflike(fmtarg, firstvararg) \
    __attribute__((__format__(__printf__, fmtarg, firstvararg)))
#endif

/* __inline — FreeBSD compat alias for inline */
#ifndef __inline
#  define __inline  inline
#endif

/*
 * BUS_READ_IVAR / BUS_WRITE_IVAR — device ivar accessors.
 * Used by mmcbrvar.h generated accessor inlines (mmcbr_get_bus_mode etc.).
 * These are never called in the shim (we bypass mmc.c), so return ENOENT.
 */
static inline int
BUS_READ_IVAR(void *dev __attribute__((unused)),
              void *child __attribute__((unused)),
              int index __attribute__((unused)),
              uintptr_t *result)
{
    *result = 0;
    return 95; /* ENOTSUP */
}

static inline int
BUS_WRITE_IVAR(void *dev __attribute__((unused)),
               void *child __attribute__((unused)),
               int index __attribute__((unused)),
               uintptr_t value __attribute__((unused)))
{
    return 95; /* ENOTSUP */
}

/*
 * __BUS_ACCESSOR generates inline mmcbr_get_XX / mmcbr_set_XX functions.
 * These call BUS_READ_IVAR / BUS_WRITE_IVAR on the parent device.
 * Since we stub BUS_READ/WRITE_IVAR to return 0/ENOTSUP, and since
 * these functions are never called in the shim path, the generated
 * code just needs to compile cleanly.
 *
 * vdev  - lowercase prefix (e.g. "mmcbr")
 * var   - lowercase field name (e.g. "bus_mode")
 * IVAR  - uppercase ivar constant (e.g. "BUS_MODE")
 * type  - C type of the value (e.g. int)
 */
#define __BUS_ACCESSOR(vdev, var, IVAR, ivar, type)                          \
static __inline type vdev##_get_##var(void *dev __attribute__((unused)))     \
{                                                                             \
    uintptr_t v = 0;                                                          \
    (void)BUS_READ_IVAR(NULL, dev, IVAR##_IVAR_##ivar, &v);                  \
    return (type)v;                                                           \
}                                                                             \
static __inline void vdev##_set_##var(void *dev __attribute__((unused)),     \
                                       type val __attribute__((unused)))     \
{                                                                             \
    (void)BUS_WRITE_IVAR(NULL, dev, IVAR##_IVAR_##ivar, (uintptr_t)val);     \
}

/* device_get_parent — return NULL in shim (no real bus hierarchy) */
static inline void *
device_get_parent(void *dev __attribute__((unused)))
{ return NULL; }

/* maxphys — maximum physical I/O transfer size (used by sdhci_dma_alloc) */
#ifndef maxphys
#  define maxphys  (128 * 1024)
#endif

/* PAGE_SIZE — used by ALLOCATED_DMA_SEGS calculation in bcm2835_sdhci.c */
#ifndef PAGE_SIZE
#  define PAGE_SIZE  4096U
#endif

/* rounddown / roundup — integer alignment helpers */
#ifndef rounddown
#  define rounddown(x, y)  (((x) / (y)) * (y))
#endif
#ifndef roundup
#  define roundup(x, y)    ((((x) + (y) - 1) / (y)) * (y))
#endif

/* min / max — safe generic min/max (avoid double-evaluation with typeof) */
#ifndef min
#  define min(a, b)  ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#  define max(a, b)  ((a) > (b) ? (a) : (b))
#endif
