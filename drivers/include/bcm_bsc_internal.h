/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm_bsc_internal.h — BSC/I2C-driver-specific shim types and declarations.
 *
 * Builds on shim_core.h (driver-agnostic KOBJ infrastructure) and adds
 * only what the BCM2835 BSC (I2C) driver port requires:
 *   - Driver registration function and registry extern
 *   - Single-resource bus allocation variants (bus_alloc_resource_any)
 *   - Child device management stubs (device_add_child, bus_generic_detach …)
 *   - iicbus kobjop_desc externs
 *
 * Included by:
 *   • bcm_bsc_shim.c          (Unikraft-side API layer + polled transfers)
 *   • bsc_kobj_descriptors.c  (KOBJ descriptor and registry definitions)
 *
 * NOT included from the FreeBSD isolated include path.  FreeBSD-path headers
 * (sys/kobj.h, sys/bus.h, …) include shim_core.h directly.
 */
#ifndef _BCM_BSC_INTERNAL_H_
#define _BCM_BSC_INTERNAL_H_

#include <shim_core.h>

/* =========================================================
 * BSC driver registration
 *
 * _shim_bsc_register() is called from the DRIVER_MODULE constructor
 * defined in sys/kernel.h (FreeBSD include path).  Using a function
 * avoids referencing shim_bsc_driver_reg from the FreeBSD compile unit.
 * ========================================================= */

void _shim_bsc_register(const void *methods, size_t softc_size);
extern struct shim_driver_reg shim_bsc_driver_reg;

/* =========================================================
 * BSC-specific resource allocation constants and functions
 *
 * bcm2835_bsc.c uses bus_alloc_resource_any() (single-resource variant)
 * and RF_SHAREABLE for IRQ sharing.  Implemented in bcm_bsc_shim.c.
 * ========================================================= */

/* RF_SHAREABLE and DEVICE_UNIT_ANY are in shim_core.h (needed by FreeBSD
 * driver sources compiled with the isolated include path). */

struct resource *bus_alloc_resource_any(device_t dev, int type, int *rid,
                                        unsigned int flags);
void bus_release_resource(device_t dev, int type, int rid,
                          struct resource *r);

/* =========================================================
 * Child device management
 *
 * bcm_bsc_attach() calls device_add_child() to attach an iicbus child
 * and checks the result for NULL.  We return a pointer to a static dummy
 * device so the NULL-check passes; no real bus enumeration takes place.
 * The remaining functions are no-ops: the shim never attaches real
 * iicbus children.
 * ========================================================= */

device_t device_add_child(device_t dev, const char *devname, int unit);

/* bus_generic_detach, bus_delayed_attach_children, and bus_attach_children
 * are in shim_core.h (needed by FreeBSD driver sources on the isolated path). */

/* bus_generic_setup_intr and bus_generic_teardown_intr are in shim_core.h
 * (used by both GPIO and BSC drivers). */

/* =========================================================
 * iicbus interface kobjop_desc externs (defined in bsc_kobj_descriptors.c)
 * ========================================================= */

extern struct kobjop_desc iicbus_callback_desc;
extern struct kobjop_desc iicbus_reset_desc;
extern struct kobjop_desc iicbus_transfer_desc;

#endif /* _BCM_BSC_INTERNAL_H_ */
