/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm_gpio_internal.h — GPIO-driver-specific shim types and declarations.
 *
 * Builds on shim_core.h (driver-agnostic KOBJ infrastructure) and adds
 * only what the BCM2835 GPIO driver port requires:
 *   - Driver registration function and registry extern
 *   - Array-variant bus resource allocation (bus_alloc_resources)
 *   - GPIO, PIC, OFW, and FDT kobjop_desc externs
 *
 * Included by:
 *   • bcm_gpio_shim.c       (Unikraft-side API layer)
 *   • kobj_descriptors.c    (KOBJ descriptor and registry definitions)
 *   • FreeBSD-path shim headers that still need the GPIO registry
 *     (sys/kobj.h, sys/bus.h, sys/rman.h, sys/proc.h, sys/module.h)
 *     → those now forward to shim_core.h; this header is NOT in the
 *       FreeBSD isolated include path and must not be included from there.
 */
#ifndef _BCM_GPIO_INTERNAL_H_
#define _BCM_GPIO_INTERNAL_H_

#include <shim_core.h>

/* =========================================================
 * GPIO driver registration
 *
 * _shim_gpio_register() is called from the EARLY_DRIVER_MODULE
 * constructor defined in sys/kernel.h (FreeBSD include path).
 * Using a function (rather than a direct struct write) keeps kernel.h
 * free of any driver-specific names, matching the DRIVER_MODULE pattern.
 * ========================================================= */

void _shim_gpio_register(const void *methods, size_t softc_size);
extern struct shim_driver_reg shim_gpio_driver_reg;

/* =========================================================
 * GPIO-specific bus resource allocation (array variant)
 *
 * bcm2835_gpio.c uses bus_alloc_resources() with a resource_spec[]
 * array.  Implemented in bcm_gpio_shim.c.
 * ========================================================= */

int  bus_alloc_resources(device_t dev, struct resource_spec *spec,
                         struct resource **res);
void bus_release_resources(device_t dev, struct resource_spec *spec,
                           struct resource **res);

/* =========================================================
 * GPIO protocol kobjop_desc externs (defined in kobj_descriptors.c)
 * ========================================================= */

extern struct kobjop_desc gpio_get_bus_desc;
extern struct kobjop_desc gpio_pin_max_desc;
extern struct kobjop_desc gpio_pin_getname_desc;
extern struct kobjop_desc gpio_pin_getflags_desc;
extern struct kobjop_desc gpio_pin_getcaps_desc;
extern struct kobjop_desc gpio_pin_setflags_desc;
extern struct kobjop_desc gpio_pin_get_desc;
extern struct kobjop_desc gpio_pin_set_desc;
extern struct kobjop_desc gpio_pin_toggle_desc;

/* =========================================================
 * PIC interface kobjop_desc externs (defined in kobj_descriptors.c)
 * ========================================================= */

extern struct kobjop_desc pic_disable_intr_desc;
extern struct kobjop_desc pic_enable_intr_desc;
extern struct kobjop_desc pic_map_intr_desc;
extern struct kobjop_desc pic_post_filter_desc;
extern struct kobjop_desc pic_post_ithread_desc;
extern struct kobjop_desc pic_pre_ithread_desc;
extern struct kobjop_desc pic_setup_intr_desc;
extern struct kobjop_desc pic_teardown_intr_desc;

/* =========================================================
 * OFW bus and FDT pinctrl kobjop_desc externs
 * ========================================================= */

extern struct kobjop_desc ofw_bus_get_node_desc;
extern struct kobjop_desc fdt_pinctrl_configure_desc;

#endif /* _BCM_GPIO_INTERNAL_H_ */
