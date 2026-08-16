/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/bus.h shim — re-exports from shim_core.h plus
 * device_printf (needs stdio.h).
 */
#pragma once
#include <shim_core.h>
#include <sys/interrupt.h>   /* INTR_TYPE_BIO, INTR_MPSAFE */
#include <stdio.h>

#define device_printf(dev, fmt, ...)  printf(fmt, ##__VA_ARGS__)

/*
 * bus_alloc_resource_any / bus_release_resource — implemented in
 * bcm_sdhci_shim.c (SDHCI) or bcm_bsc_shim.c (BSC).
 * Declared here so bcm2835_sdhci.c can call them without an implicit decl.
 */
struct resource *bus_alloc_resource_any(device_t dev, int type, int *rid,
                                        unsigned int flags);
void bus_release_resource(device_t dev, int type, int rid,
                          struct resource *r);

/*
 * device_add_child / bus_generic_add_child — implemented in bcm_sdhci_shim.c.
 * Referenced in bcm2835_sdhci.c's DEVMETHOD table at file scope.
 */
device_t device_add_child(device_t dev, const char *devname, int unit);
device_t bus_generic_add_child(device_t dev, int order,
                               const char *name, int unit);
