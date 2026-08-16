/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bsc_kobj_descriptors.c — KOBJ descriptors and driver registry for the
 * BCM2835 BSC (I2C) shim.
 *
 * Compiled with the normal Unikraft include path (not the FreeBSD shim
 * path), so only bcm_bsc_internal.h is needed.
 *
 * Provides:
 *   shim_bsc_driver_reg  — written by the DRIVER_MODULE constructor in
 *                          bcm2835_bsc.c, read by uk_i2c_init().
 *   iicbus_driver        — stub for the first DRIVER_MODULE call which
 *                          registers the iicbus child bus (methods=NULL,
 *                          so the constructor skips it).
 *   iicbus_{callback,reset,transfer}_desc — kobjop_desc instances used
 *                          by DEVMETHOD() in bcm_bsc_methods[].
 *   _shim_bsc_register() — called from the DRIVER_MODULE constructor macro
 *                          defined in sys/kernel.h.
 */

#include <bcm_bsc_internal.h>

/* ---- BSC driver registry — written by constructor, read by uk_i2c_init ---- */
struct shim_driver_reg shim_bsc_driver_reg = { NULL, 0 };

/*
 * _shim_bsc_register — called from the DRIVER_MODULE(bcm2835_bsc, …)
 * constructor.  Only updates the registry the first time (prevents the
 * DRIVER_MODULE(iicbus, …) stub from overwriting if it somehow had methods).
 */
void _shim_bsc_register(const void *methods, size_t softc_size)
{
    if (shim_bsc_driver_reg.sdr_methods == NULL) {
        shim_bsc_driver_reg.sdr_methods    = (const kobj_method_t *)methods;
        shim_bsc_driver_reg.sdr_softc_size = softc_size;
    }
}

/* ---- iicbus_driver stub (methods=NULL → constructor skips registration) ---- */
driver_t iicbus_driver = { "iicbus", NULL, 0 };

/* ---- iicbus KOBJ method descriptors ---- */
struct kobjop_desc iicbus_callback_desc = { "iicbus_callback", NULL };
struct kobjop_desc iicbus_reset_desc    = { "iicbus_reset",    NULL };
struct kobjop_desc iicbus_transfer_desc = { "iicbus_transfer", NULL };

/* device_add_child is defined in bus_stubs.c (shared stub for all shims) */
