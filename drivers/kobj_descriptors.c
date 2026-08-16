/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * kobj_descriptors.c — defines all 24 kobjop_desc instances plus
 * shim_gpio_driver_reg storage and _shim_thread.
 *
 * Only bcm_gpio_internal.h is needed; no FreeBSD isolated include path required.
 */

#include <bcm_gpio_internal.h>

/* ---- shim_gpio_driver_reg storage and registration function ---- */
struct shim_driver_reg shim_gpio_driver_reg = { NULL, 0 };

void _shim_gpio_register(const void *methods, size_t softc_size)
{
    shim_gpio_driver_reg.sdr_methods    = (const kobj_method_t *)methods;
    shim_gpio_driver_reg.sdr_softc_size = softc_size;
}

/* ---- Device interface ---- */
struct kobjop_desc device_probe_desc          = { "device_probe",   NULL };
struct kobjop_desc device_attach_desc         = { "device_attach",  NULL };
struct kobjop_desc device_detach_desc         = { "device_detach",  NULL };

/* ---- Bus interface ---- */
struct kobjop_desc bus_setup_intr_desc        = { "bus_setup_intr",    NULL };
struct kobjop_desc bus_teardown_intr_desc     = { "bus_teardown_intr", NULL };

/* ---- GPIO protocol ---- */
struct kobjop_desc gpio_get_bus_desc          = { "gpio_get_bus",      NULL };
struct kobjop_desc gpio_pin_max_desc          = { "gpio_pin_max",      NULL };
struct kobjop_desc gpio_pin_getname_desc      = { "gpio_pin_getname",  NULL };
struct kobjop_desc gpio_pin_getflags_desc     = { "gpio_pin_getflags", NULL };
struct kobjop_desc gpio_pin_getcaps_desc      = { "gpio_pin_getcaps",  NULL };
struct kobjop_desc gpio_pin_setflags_desc     = { "gpio_pin_setflags", NULL };
struct kobjop_desc gpio_pin_get_desc          = { "gpio_pin_get",      NULL };
struct kobjop_desc gpio_pin_set_desc          = { "gpio_pin_set",      NULL };
struct kobjop_desc gpio_pin_toggle_desc       = { "gpio_pin_toggle",   NULL };

/* ---- PIC interface ---- */
struct kobjop_desc pic_disable_intr_desc      = { "pic_disable_intr",  NULL };
struct kobjop_desc pic_enable_intr_desc       = { "pic_enable_intr",   NULL };
struct kobjop_desc pic_map_intr_desc          = { "pic_map_intr",      NULL };
struct kobjop_desc pic_post_filter_desc       = { "pic_post_filter",   NULL };
struct kobjop_desc pic_post_ithread_desc      = { "pic_post_ithread",  NULL };
struct kobjop_desc pic_pre_ithread_desc       = { "pic_pre_ithread",   NULL };
struct kobjop_desc pic_setup_intr_desc        = { "pic_setup_intr",    NULL };
struct kobjop_desc pic_teardown_intr_desc     = { "pic_teardown_intr", NULL };

/* ---- OFW bus interface ---- */
struct kobjop_desc ofw_bus_get_node_desc      = { "ofw_bus_get_node",  NULL };

/* ---- FDT pinctrl interface ---- */
struct kobjop_desc fdt_pinctrl_configure_desc = { "fdt_pinctrl_configure", NULL };

/* ---- dummy thread for curthread ---- */
struct thread _shim_thread = { NULL };
