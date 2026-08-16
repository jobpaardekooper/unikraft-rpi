/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bus_stubs.c — shared bus function stubs for all FreeBSD driver shims.
 *
 * When the SDHCI, GPIO, and I2C/BSC shims are all linked together, each of
 * those drivers needs bus_setup_intr, bus_teardown_intr, bus_release_resource,
 * device_add_child, bus_generic_add_child, and bus_alloc_resource_any.  Having
 * a definition of each function in every per-driver shim file creates "multiple
 * definition" linker errors.
 *
 * This file is the single owner of those symbols.  The driver-specific
 * resource allocators are provided as _bcm_bsc_alloc_resource() and
 * _bcm_sdhci_alloc_resource() by their respective shim files; weak default
 * implementations here ensure that missing drivers do not cause linker errors.
 *
 * Compiled for: CONFIG_LIBBCM2835_SDCARD
 * Include path: -I$(LIBRASPIPLAT_BASE)/drivers/include (from SDCARD CINCLUDES)
 */
#include <shim_core.h>
#include <uk/print.h>

/* =========================================================================
 * bus_setup_intr / bus_teardown_intr — no real IRQs in polled operation
 * ========================================================================= */

int bus_setup_intr(device_t dev __attribute__((unused)),
                   struct resource *r,
                   int flags __attribute__((unused)),
                   driver_filter_t filter __attribute__((unused)),
                   driver_intr_t ithread __attribute__((unused)),
                   void *arg __attribute__((unused)),
                   void **cookiep)
{
    if (cookiep)
        *cookiep = r;
    uk_pr_info("bus_stubs: IRQ %lu registered as no-op (polled mode)\n",
               r ? (unsigned long)r->r_start : 0UL);
    return 0;
}

int bus_teardown_intr(device_t dev __attribute__((unused)),
                      struct resource *r __attribute__((unused)),
                      void *cookie __attribute__((unused)))
{
    return 0;
}

/* =========================================================================
 * bus_release_resource — no-op: all shim resources are statically allocated
 * ========================================================================= */

void bus_release_resource(device_t dev __attribute__((unused)),
                          int type     __attribute__((unused)),
                          int rid      __attribute__((unused)),
                          struct resource *r __attribute__((unused))) {}

/* =========================================================================
 * device_add_child / bus_generic_add_child — return a shared dummy device
 * ========================================================================= */

static struct device _shim_dummy_child = {
    .d_softc    = NULL,
    .d_methods  = NULL,
    .d_nameunit = "child0",
};

device_t device_add_child(device_t dev   __attribute__((unused)),
                          const char *name __attribute__((unused)),
                          int unit         __attribute__((unused)))
{
    return &_shim_dummy_child;
}

device_t bus_generic_add_child(device_t dev    __attribute__((unused)),
                               int order        __attribute__((unused)),
                               const char *name __attribute__((unused)),
                               int unit         __attribute__((unused)))
{
    return &_shim_dummy_child;
}

/* =========================================================================
 * bus_alloc_resource_any — dispatch to driver-private implementations.
 *
 * Each driver shim exposes a private function named _bcm_<drv>_alloc_resource
 * with the same signature.  The dispatcher resolves the right one by looking
 * at the device's nameunit string:
 *   "iichb*"  → BSC / I2C shim
 *   "sdhci*"  → SDHCI shim
 *
 * Weak defaults return NULL so that a shim can be omitted without causing
 * undefined-reference linker errors.
 * ========================================================================= */

__attribute__((weak))
struct resource *_bcm_bsc_alloc_resource(
    device_t dev __attribute__((unused)),
    int type __attribute__((unused)),
    int *rid __attribute__((unused)),
    unsigned int flags __attribute__((unused)))
{
    return NULL;
}

__attribute__((weak))
struct resource *_bcm_sdhci_alloc_resource(
    device_t dev __attribute__((unused)),
    int type __attribute__((unused)),
    int *rid __attribute__((unused)),
    unsigned int flags __attribute__((unused)))
{
    return NULL;
}

struct resource *bus_alloc_resource_any(device_t dev, int type, int *rid,
                                        unsigned int flags)
{
    if (dev && dev->d_nameunit) {
        if (__builtin_strncmp(dev->d_nameunit, "iichb", 5) == 0)
            return _bcm_bsc_alloc_resource(dev, type, rid, flags);
        if (__builtin_strncmp(dev->d_nameunit, "sdhci", 5) == 0)
            return _bcm_sdhci_alloc_resource(dev, type, rid, flags);
    }
    return NULL;
}
