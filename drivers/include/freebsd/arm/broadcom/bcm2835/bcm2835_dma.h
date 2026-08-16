/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * arm/broadcom/bcm2835/bcm2835_dma.h shim.
 *
 * The BCM2835 DMA engine is a separate driver in FreeBSD that
 * bcm2835_sdhci.c uses for DMA transfers.  In the Unikraft shim we
 * stub this entire subsystem: bcm_sdhci_attach() obtains a "channel"
 * (always channel 0) but never calls bcm_dma_start() because
 * SDHCI_PLATFORM_TRANSFER is enabled and our polled SD protocol in
 * bcm_sdhci_shim.c drives the FIFO directly.
 */
#pragma once
#include <stdint.h>

/* Minimum DMA transfer block size (matches BCM_SDHCI_BUFFER_SIZE = 512) */
#define BCM_DMA_BLOCK_SIZE  512U

#define BCM_DMA_CH_ANY      (-1)
#define BCM_DMA_CH_INVALID  (-1)

/* DMA request lines */
#define BCM_DMA_DREQ_NONE   0
/* EMMC DREQ — exact value does not matter since DMA is never started */
#define BCM_DMA_DREQ_EMMC   11

/* DMA configuration flags */
#define BCM_DMA_INC_ADDR    0x01
#define BCM_DMA_SAME_ADDR   0x00

/* DMA transfer widths used by bcm_sdhci_start_dma_seg */
#define BCM_DMA_32BIT       0x00  /* 32-bit (1-word) DMA burst */
#define BCM_DMA_128BIT      0x01  /* 128-bit (4-word) DMA burst */

/* Stub: allocate a DMA channel — return 0 (valid, non-INVALID) */
static inline int bcm_dma_allocate(int ch __attribute__((unused)))
{ return 0; }

static inline int bcm_dma_free(int ch __attribute__((unused)))
{ return 0; }

static inline int
bcm_dma_setup_intr(int ch __attribute__((unused)),
                   void (*func)(int, void *) __attribute__((unused)),
                   void *arg __attribute__((unused)))
{ return 0; }

static inline int
bcm_dma_setup_src(int ch __attribute__((unused)),
                  int dreq __attribute__((unused)),
                  int inc __attribute__((unused)),
                  int width __attribute__((unused)))
{ return 0; }

static inline int
bcm_dma_setup_dst(int ch __attribute__((unused)),
                  int dreq __attribute__((unused)),
                  int inc __attribute__((unused)),
                  int width __attribute__((unused)))
{ return 0; }

static inline int
bcm_dma_start(int ch __attribute__((unused)),
              uintptr_t src __attribute__((unused)),
              uintptr_t dst __attribute__((unused)),
              int len __attribute__((unused)))
{ return 0; }
