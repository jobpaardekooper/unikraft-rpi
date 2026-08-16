/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/kernel.h shim.
 *
 * KASSERT  → compile-away assertion (bare-metal: no kernel panic infra)
 * SYSINIT  → no-op (constructor-based init not needed here)
 *
 * Driver registration macros
 * ──────────────────────────
 * Both macros expand to a __attribute__((constructor)) function that calls
 * the corresponding _shim_<drv>_register() function before uk_main() runs.
 * Using a function call (rather than writing the registry struct directly)
 * keeps this header free of any driver-specific struct names.
 *
 * Adding a new driver:
 *   1. Declare void _shim_<drv>_register(const void *, size_t) here.
 *   2. Add the appropriate MODULE macro (EARLY_DRIVER_MODULE or
 *      DRIVER_MODULE) — or reuse an existing one if the calling convention
 *      matches.
 *   3. Implement _shim_<drv>_register() in the driver's kobj_descriptors.c.
 */
#pragma once
#include <strings.h>   /* ffs() */

/* Suppress KASSERT in shim mode (must NOT evaluate exp — it may reference
 * variables declared only under #ifdef INVARIANTS in driver code). */
#define KASSERT(exp, msg)  do { } while (0)

/* SYSINIT is a no-op — the shim uses __attribute__((constructor)) instead */
#define SYSINIT(ident, subsystem, order, func, arg)  /* nothing */

/* Convenient integer types used across the kernel */
#ifndef __bitcount32
#define __bitcount32(x) __builtin_popcount((unsigned)(x))
#endif

/*
 * hz — system timer frequency.  Used by bcm_bsc_transfer as mtx_sleep
 * timeout.  The value never matters because bcm_bsc_transfer is bypassed
 * by the polled shim, but the symbol must link.
 */
#ifndef hz
#define hz  100
#endif

/* =========================================================================
 * Driver registration function declarations.
 *
 * These are implemented in each driver's kobj_descriptors.c (or equivalent)
 * on the normal Unikraft include path.  Using void* avoids any dependency on
 * kobj_method_t here, since this header is compiled from the FreeBSD
 * isolated include path where that type may not yet be visible.
 * ========================================================================= */

/* GPIO driver — bcm2835_gpio.c, defined in kobj_descriptors.c */
void _shim_gpio_register(const void *methods, size_t softc_size);

/* BSC / I2C driver — bcm2835_bsc.c, defined in bsc_kobj_descriptors.c */
void _shim_bsc_register(const void *methods, size_t softc_size);

/* SDHCI driver — bcm2835_sdhci.c, defined in sdhci_kobj_descriptors.c */
void _shim_sdhci_register(const void *methods, size_t softc_size);

/* =========================================================================
 * EARLY_DRIVER_MODULE — used by bcm2835_gpio.c
 *
 * "Early" means the driver is registered before the main bus scan.  In the
 * shim context this distinction is irrelevant; both macros produce a
 * constructor that runs before uk_main().
 * ========================================================================= */
#define EARLY_DRIVER_MODULE(name, busname, driver_var, evh, arg, pass)  \
    static void __attribute__((constructor))                             \
    _gpio_drv_##name##_##busname##_reg(void) {                          \
        _shim_gpio_register((driver_var).methods, (driver_var).size);   \
    }

/* =========================================================================
 * DRIVER_MODULE — used by bcm2835_bsc.c and bcm2835_sdhci.c.
 *
 * bcm2835_bsc.c emits two calls:
 *   DRIVER_MODULE(iicbus,      bcm2835_bsc, iicbus_driver,  0, 0)
 *   DRIVER_MODULE(bcm2835_bsc, simplebus,   bcm_bsc_driver, 0, 0)
 *
 * bcm2835_sdhci.c emits (compiled with -DFREEBSD_SHIM_SDHCI=1):
 *   DRIVER_MODULE(sdhci_bcm, simplebus, bcm_sdhci_driver, NULL, NULL)
 *
 * The BSC variant (default) calls _shim_bsc_register with a NULL guard.
 * The SDHCI variant (selected by -DFREEBSD_SHIM_SDHCI=1 per-file flag)
 * calls _shim_sdhci_register instead.
 *
 * MODULE_VERSION / MODULE_DEPEND are no-ops in the shim.
 * ========================================================================= */
#ifdef FREEBSD_SHIM_SDHCI
#  define DRIVER_MODULE(name, busname, driver, evh, arg)              \
    static void __attribute__((constructor))                          \
    _sdhci_drv_##name##_##busname##_reg(void) {                      \
        if ((driver).methods != NULL)                                 \
            _shim_sdhci_register((driver).methods, (driver).size);   \
    }
#else
#  define DRIVER_MODULE(name, busname, driver, evh, arg)              \
    static void __attribute__((constructor))                          \
    _bsc_drv_##name##_##busname##_reg(void) {                        \
        if ((driver).methods != NULL)                                 \
            _shim_bsc_register((driver).methods, (driver).size);     \
    }
#endif

#define MODULE_VERSION(name, version)     /* no-op */
#define MODULE_DEPEND(name, dep, lo, hi, exact)  /* no-op */
