/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * arm/broadcom/bcm2835/bcm2835_vcbus.h shim.
 *
 * The VideoCore bus address translation API converts between ARM physical
 * addresses and the VideoCore (VC) bus addresses used for DMA.  On RPi3,
 * the SDRAM is mapped at 0xC0000000 on the VideoCore side.
 *
 * bcm2835_sdhci.c calls bcm283x_dmabus_peripheral_lowaddr() as the
 * DMA tag upper address limit.  Since our DMA path is fully stubbed,
 * this value is never used for real transfers; returning 0xFFFFFFFF
 * (BUS_SPACE_MAXADDR_32BIT) satisfies the bus_dma_tag_create call.
 */
#pragma once
#include <stdint.h>

/* Peripheral bus DMA address ceiling (32-bit DMA capable region) */
static inline uintptr_t
bcm283x_dmabus_peripheral_lowaddr(void)
{
    return 0xFFFFFFFFUL;
}

/* ARM→VC and VC→ARM address translations (identity in shim context) */
static inline uintptr_t
bcm283x_arm_to_vc(uintptr_t addr)
{
    return addr | 0xC0000000UL;
}

static inline uintptr_t
bcm283x_vc_to_arm(uintptr_t addr)
{
    return addr & ~0xC0000000UL;
}
