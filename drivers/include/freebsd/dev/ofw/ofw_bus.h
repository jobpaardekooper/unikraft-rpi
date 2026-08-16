/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD dev/ofw/ofw_bus.h shim.
 *
 * NOTE: intentionally uses a DIFFERENT include guard than ofw_bus_subr.h
 * so both files can be included without the second being silently skipped
 * (machine/intr.h pulls in ofw_bus_subr.h; the driver pulls in ofw_bus.h
 * which re-includes ofw_bus_subr.h).
 */
#ifndef _FREEBSD_SHIM_OFW_BUS_H_
#define _FREEBSD_SHIM_OFW_BUS_H_

#include <dev/ofw/ofw_bus_subr.h>
#include <sys/bus.h>    /* device_t */

/*
 * ofw_bus_status_okay — always OK (no disabled devices in hardcoded tree).
 */
static inline int
ofw_bus_status_okay(device_t dev __attribute__((unused)))
{ return 1; }

/*
 * ofw_bus_search_compatible — always returns the first entry whose data != 0.
 * The driver checks: if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
 *                          return ENXIO;
 * So returning compat_data[0] (data=1) lets probe succeed.
 */
static inline const struct ofw_compat_data *
ofw_bus_search_compatible(device_t dev __attribute__((unused)),
                           const struct ofw_compat_data *table)
{
    return table;  /* first entry has ocd_data = 1 */
}

/*
 * ofw_bus_get_node — return our fake node handle.
 */
static inline phandle_t
ofw_bus_get_node(device_t dev __attribute__((unused)))
{ return BCM_GPIO_FDT_NODE; }

/*
 * ofw_bus_node_is_compatible — BCM2711 check: we are BCM2835, so return 0.
 */
static inline int
ofw_bus_node_is_compatible(phandle_t node __attribute__((unused)),
                            const char *compat __attribute__((unused)))
{ return 0; }

/*
 * ofw_bus_get_node_desc — KOBJ method descriptor for the OFW bus interface.
 *
 * In FreeBSD this is generated from ofw_bus_if.m.  bcm2835_gpio.c references
 * it in its DEVMETHOD table (DEVMETHOD(ofw_bus_get_node, bcm_gpio_get_node)),
 * so the extern must be visible from the FreeBSD isolated include path.
 * The definition lives in kobj_descriptors.c (normal Unikraft path).
 */
#include <sys/kobj.h>
extern struct kobjop_desc ofw_bus_get_node_desc;

#endif /* _FREEBSD_SHIM_OFW_BUS_H_ */
