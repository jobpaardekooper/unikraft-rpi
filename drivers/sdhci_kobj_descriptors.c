/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * sdhci_kobj_descriptors.c — kobjop_desc instances for the SDHCI interface,
 * the MMCBR interface, and the shim registration infrastructure for the
 * BCM2835 SDHCI driver (bcm2835_sdhci.c).
 *
 * Analogous to kobj_descriptors.c (GPIO) and bsc_kobj_descriptors.c (BSC).
 * Compiled with the normal Unikraft include path (not the FreeBSD isolated
 * path) so it can reference shim_core.h types directly.
 */

#include <shim_core.h>

/* =========================================================================
 * shim_sdhci_driver_reg — filled by the constructor in bcm2835_sdhci.c
 * ========================================================================= */

struct shim_driver_reg shim_sdhci_driver_reg = { NULL, 0 };

void _shim_sdhci_register(const void *methods, size_t softc_size)
{
    shim_sdhci_driver_reg.sdr_methods    = (const kobj_method_t *)methods;
    shim_sdhci_driver_reg.sdr_softc_size = softc_size;
}

/* =========================================================================
 * Generic bus ivar / child management descriptors
 * (defined here so both GPIO and SDHCI devmethod tables can link against them)
 * ========================================================================= */

struct kobjop_desc bus_read_ivar_desc   = { "bus_read_ivar",   NULL };
struct kobjop_desc bus_write_ivar_desc  = { "bus_write_ivar",  NULL };
struct kobjop_desc bus_add_child_desc   = { "bus_add_child",   NULL };

/* =========================================================================
 * SDHCI host-controller interface (sdhci_if.m in FreeBSD)
 * ========================================================================= */

struct kobjop_desc sdhci_read_1_desc                 = { "sdhci_read_1",                  NULL };
struct kobjop_desc sdhci_read_2_desc                 = { "sdhci_read_2",                  NULL };
struct kobjop_desc sdhci_read_4_desc                 = { "sdhci_read_4",                  NULL };
struct kobjop_desc sdhci_read_multi_4_desc           = { "sdhci_read_multi_4",            NULL };
struct kobjop_desc sdhci_write_1_desc                = { "sdhci_write_1",                 NULL };
struct kobjop_desc sdhci_write_2_desc                = { "sdhci_write_2",                 NULL };
struct kobjop_desc sdhci_write_4_desc                = { "sdhci_write_4",                 NULL };
struct kobjop_desc sdhci_write_multi_4_desc          = { "sdhci_write_multi_4",           NULL };
struct kobjop_desc sdhci_get_card_present_desc       = { "sdhci_get_card_present",        NULL };
struct kobjop_desc sdhci_platform_will_handle_desc   = { "sdhci_platform_will_handle",    NULL };
struct kobjop_desc sdhci_platform_start_transfer_desc= { "sdhci_platform_start_transfer", NULL };
struct kobjop_desc sdhci_platform_finish_transfer_desc={ "sdhci_platform_finish_transfer",NULL };
struct kobjop_desc sdhci_set_uhs_timing_desc         = { "sdhci_set_uhs_timing",          NULL };
struct kobjop_desc sdhci_reset_desc                  = { "sdhci_reset",                   NULL };

/* =========================================================================
 * MMC bridge interface (mmcbr_if.m in FreeBSD)
 * ========================================================================= */

struct kobjop_desc mmcbr_update_ios_desc   = { "mmcbr_update_ios",   NULL };
struct kobjop_desc mmcbr_request_desc      = { "mmcbr_request",      NULL };
struct kobjop_desc mmcbr_get_ro_desc       = { "mmcbr_get_ro",       NULL };
struct kobjop_desc mmcbr_acquire_host_desc = { "mmcbr_acquire_host", NULL };
struct kobjop_desc mmcbr_release_host_desc = { "mmcbr_release_host", NULL };
struct kobjop_desc mmcbr_tune_desc         = { "mmcbr_tune",         NULL };
struct kobjop_desc mmcbr_retune_desc       = { "mmcbr_retune",       NULL };
struct kobjop_desc mmcbr_switch_vccq_desc  = { "mmcbr_switch_vccq",  NULL };

/* =========================================================================
 * Global taskqueue_bus (stub — all taskqueue operations are no-ops)
 * ========================================================================= */
struct taskqueue *taskqueue_bus = NULL;

/* =========================================================================
 * malloc_type instances referenced by sdhci.c and bcm2835_sdhci.c
 * ========================================================================= */
struct malloc_type _M_DEVBUF = { "devbuf" };
struct malloc_type _M_TEMP   = { "temp"   };
