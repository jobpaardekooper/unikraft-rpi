/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/rman.h shim — re-exports from shim_core.h.
 * struct resource, rman_get_bustag/bushandle all live there.
 */
#pragma once
#include <shim_core.h>

/*
 * rman_get_start — return the physical start address of a resource.
 * Used by bcm2835_sdhci.c to compute sc_sdhci_buffer_phys:
 *   sc->sc_sdhci_buffer_phys = rman_get_start(sc->sc_mem_res) + SDHCI_BUFFER;
 */
static inline uintptr_t
rman_get_start(struct resource *r) { return r ? r->r_start : 0; }
