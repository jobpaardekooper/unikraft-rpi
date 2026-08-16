/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/malloc.h shim.
 *
 * Maps FreeBSD kernel malloc/free to Unikraft uk_malloc/uk_free.
 * MALLOC_DECLARE / MALLOC_DEFINE are no-ops: memory types are unused.
 */
#pragma once
#include <stddef.h>
#include <uk/alloc.h>

/* Memory allocation flags */
#define M_NOWAIT    0x0001  /* do not block */
#define M_WAITOK    0x0002  /* ok to block  */
#define M_ZERO      0x0100  /* zero the allocation */

/* Memory type descriptor (opaque in shim).
 * Also defined in shim_core.h so that files compiled without this header
 * still have the type; guard against double-definition. */
#ifndef _SHIM_MALLOC_TYPE_DEFINED_
#define _SHIM_MALLOC_TYPE_DEFINED_
struct malloc_type {
    const char *ks_shortdesc;
};
#endif

/* M_DEVBUF — used for driver buffers (bcm2835_sdhci, sdhci) */
extern struct malloc_type _M_DEVBUF;
#define M_DEVBUF  (&_M_DEVBUF)

/* M_TEMP — temporary kernel allocations */
extern struct malloc_type _M_TEMP;
#define M_TEMP    (&_M_TEMP)

#define MALLOC_DECLARE(type)    extern struct malloc_type _##type
#define MALLOC_DEFINE(type, shortdesc, longdesc) \
    struct malloc_type _##type = { shortdesc }

static inline void *
_shim_malloc(size_t size, struct malloc_type *mtype __attribute__((unused)),
             int flags)
{
    void *p = uk_malloc(uk_alloc_get_default(), size);
    if (p && (flags & M_ZERO))
        __builtin_memset(p, 0, size);
    return p;
}

static inline void
_shim_free(void *addr, struct malloc_type *mtype __attribute__((unused)))
{
    uk_free(uk_alloc_get_default(), addr);
}

#define malloc(size, type, flags)  _shim_malloc(size, type, flags)
#define free(addr, type)           _shim_free(addr, type)
