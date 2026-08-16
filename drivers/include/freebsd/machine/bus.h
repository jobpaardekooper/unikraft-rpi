/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD machine/bus.h shim — redirects to sys/bus_space.h and provides
 * bus_dma stubs.
 *
 * bus_dma is the FreeBSD DMA mapping API.  On Unikraft/RPi3, the ARM
 * peripheral address space is identity-mapped (VA = PA for MMIO and RAM),
 * so all bus_dma operations reduce to malloc + a trivial address identity.
 * All functions are stubs that return success without real DMA mapping.
 */
#pragma once
#include <sys/bus_space.h>
#include <stdint.h>
#include <stddef.h>
#include <uk/alloc.h>

/* ---- bus_dma types ---- */

typedef void *bus_dma_tag_t;    /* opaque DMA tag */
typedef void *bus_dmamap_t;     /* opaque DMA map */
typedef uintptr_t bus_addr_t;   /* bus (physical) address */
typedef size_t bus_size_t;      /* bus transfer size */

typedef struct {
    bus_addr_t ds_addr;
    bus_size_t ds_len;
} bus_dma_segment_t;

typedef void (*bus_dmamap_callback_t)(void *, bus_dma_segment_t *, int, int);
typedef void (*bus_dmamap_callback2_t)(void *, bus_dma_segment_t *, int,
                                       bus_size_t, int);

/* ---- bus_dma constants ---- */

#define BUS_SPACE_MAXADDR_32BIT  0xFFFFFFFFUL
#define BUS_SPACE_MAXADDR        (~(bus_addr_t)0)
#define BUS_SPACE_MAXSIZE_32BIT  0xFFFFFFFFUL
#define BUS_SPACE_MAXSIZE        (~(bus_size_t)0)

#define BUS_DMA_NOWAIT           0x0001
#define BUS_DMA_WAITOK           0x0002
#define BUS_DMA_ALLOCNOW         0x0004
#define BUS_DMA_COHERENT         0x0008
#define BUS_DMA_ZERO             0x0010
#define BUS_DMA_BUS1             0x0020
#define BUS_DMA_BUS2             0x0040
#define BUS_DMASYNC_PREREAD      0x01
#define BUS_DMASYNC_POSTREAD     0x02
#define BUS_DMASYNC_PREWRITE     0x04
#define BUS_DMASYNC_POSTWRITE    0x08

/* ---- bus_dma function stubs ---- */

/*
 * bus_get_dma_tag — return a dummy parent tag.
 * bcm2835_sdhci.c passes this as the parent to bus_dma_tag_create.
 */
static inline bus_dma_tag_t
bus_get_dma_tag(void *dev __attribute__((unused)))
{ return (bus_dma_tag_t)1; }

/*
 * bus_dma_tag_create — always succeeds, stores a dummy tag.
 */
static inline int
bus_dma_tag_create(bus_dma_tag_t parent __attribute__((unused)),
                   bus_size_t alignment __attribute__((unused)),
                   bus_addr_t boundary __attribute__((unused)),
                   bus_addr_t lowaddr __attribute__((unused)),
                   bus_addr_t highaddr __attribute__((unused)),
                   void *filtfunc __attribute__((unused)),
                   void *filtfuncarg __attribute__((unused)),
                   bus_size_t maxsize __attribute__((unused)),
                   int nsegments __attribute__((unused)),
                   bus_size_t maxsegsz __attribute__((unused)),
                   int flags __attribute__((unused)),
                   void *lockfunc __attribute__((unused)),
                   void *lockfuncarg __attribute__((unused)),
                   bus_dma_tag_t *dmat)
{
    *dmat = (bus_dma_tag_t)1;  /* dummy non-NULL tag */
    return 0;
}

static inline int
bus_dma_tag_destroy(bus_dma_tag_t dmat __attribute__((unused)))
{ return 0; }

/*
 * bus_dmamem_alloc — allocate a DMA-safe buffer.
 * On RPi3 all RAM is DMA-safe; just use uk_malloc.
 */
static inline int
bus_dmamem_alloc(bus_dma_tag_t dmat __attribute__((unused)),
                 void **vaddr, int flags,
                 bus_dmamap_t *mapp)
{
    /* Use tag as size hint if non-trivial; otherwise use 512 bytes */
    *vaddr = uk_malloc(uk_alloc_get_default(), 4096);
    if (!*vaddr)
        return 12; /* ENOMEM */
    if (flags & BUS_DMA_ZERO)
        __builtin_memset(*vaddr, 0, 4096);
    *mapp = (bus_dmamap_t)(uintptr_t)*vaddr;
    return 0;
}

static inline void
bus_dmamem_free(bus_dma_tag_t dmat __attribute__((unused)),
                void *vaddr,
                bus_dmamap_t map __attribute__((unused)))
{
    uk_free(uk_alloc_get_default(), vaddr);
}

static inline int
bus_dmamap_create(bus_dma_tag_t dmat __attribute__((unused)),
                  int flags __attribute__((unused)),
                  bus_dmamap_t *mapp)
{
    *mapp = (bus_dmamap_t)1;
    return 0;
}

static inline int
bus_dmamap_destroy(bus_dma_tag_t dmat __attribute__((unused)),
                   bus_dmamap_t map __attribute__((unused)))
{ return 0; }

/*
 * bus_dmamap_load — identity mapping: PA == VA on RPi3 (no IOMMU).
 * Calls the callback synchronously with a single segment.
 */
static inline int
bus_dmamap_load(bus_dma_tag_t dmat __attribute__((unused)),
                bus_dmamap_t map __attribute__((unused)),
                void *buf, bus_size_t buflen,
                bus_dmamap_callback_t callback, void *callback_arg,
                int flags __attribute__((unused)))
{
    bus_dma_segment_t seg;
    seg.ds_addr = (bus_addr_t)(uintptr_t)buf;
    seg.ds_len  = buflen;
    callback(callback_arg, &seg, 1, 0);
    return 0;
}

static inline void
bus_dmamap_unload(bus_dma_tag_t dmat __attribute__((unused)),
                  bus_dmamap_t map __attribute__((unused))) {}

/*
 * bus_dmamap_sync — on a coherent (non-cached) mapping, a DSB suffices.
 */
static inline void
bus_dmamap_sync(bus_dma_tag_t dmat __attribute__((unused)),
                bus_dmamap_t map __attribute__((unused)),
                int op __attribute__((unused)))
{
    __asm__ volatile("dsb sy" ::: "memory");
}
