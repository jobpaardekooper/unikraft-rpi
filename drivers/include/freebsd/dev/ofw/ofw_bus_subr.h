/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD dev/ofw/ofw_bus_subr.h shim.
 *
 * No real FDT blob is parsed.  All OFW queries return hardcoded BCM2837
 * (Raspberry Pi 3) values so bcm_gpio_attach() succeeds:
 *
 *   GPIO peripheral base : 0x3F200000  (MMIO_BASE + 0x200000)
 *   GPIO register size   : 0xB4 bytes
 *   IRQ bank 0           : line 49
 *   IRQ bank 1           : line 50
 *
 * BCM2835 GPIO peripherals datasheet §6.1.
 *
 * NOTE: ofw_bus_subr.h and ofw_bus.h intentionally use DIFFERENT include
 * guards so that machine/intr.h can pull in ofw_bus_subr.h and later the
 * driver can pull in ofw_bus.h (which re-includes ofw_bus_subr.h) without
 * either being silently skipped.
 */
#ifndef _FREEBSD_SHIM_OFW_BUS_SUBR_H_
#define _FREEBSD_SHIM_OFW_BUS_SUBR_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>   /* strcmp */
#include <stdlib.h>   /* NULL  */
#include <sys/types.h>

#ifndef MMIO_BASE
#  define MMIO_BASE  0x3F000000UL
#endif

/* BCM2837 GPIO peripheral physical base and size */
#define BCM2835_GPIO_BASE   (MMIO_BASE + 0x200000UL)
#define BCM2835_GPIO_SIZE   0xB4U

/* Hardware IRQ lines for GPIO bank 0 and bank 1 */
#define BCM2835_GPIO_IRQ_BANK0   49U
#define BCM2835_GPIO_IRQ_BANK1   50U

/* Fake FDT node handle — just a non-zero sentinel */
#define BCM_GPIO_FDT_NODE  ((phandle_t)1)

/* ofw_compat_data: maps a compatible string to an opaque integer */
struct ofw_compat_data {
    const char *ocd_str;
    uintptr_t   ocd_data;
};

/* ---- OF_* primitives ---- */

static inline phandle_t OF_child(phandle_t node __attribute__((unused)))
    { return 0; }  /* no children in hardcoded tree */

static inline phandle_t OF_peer(phandle_t node __attribute__((unused)))
    { return 0; }  /* no siblings */

static inline phandle_t OF_node_from_xref(phandle_t xref)
    { return xref; }

static inline phandle_t OF_xref_from_node(phandle_t node)
    { return node; }

/*
 * OF_hasprop — only "gpio-controller" property is present on our fake node.
 */
static inline int OF_hasprop(phandle_t node __attribute__((unused)),
                              const char *prop)
{
    return strcmp(prop, "gpio-controller") == 0 ? 1 : 0;
}

/*
 * OF_getprop — returns -1 (property not found) for all queries.
 */
static inline ssize_t OF_getprop(phandle_t node __attribute__((unused)),
                                  const char *prop __attribute__((unused)),
                                  void *buf __attribute__((unused)),
                                  size_t len __attribute__((unused)))
{
    return -1;
}

/*
 * OF_getprop_alloc — not found; set *bufp = NULL and return -1.
 */
static inline ssize_t
OF_getprop_alloc(phandle_t node __attribute__((unused)),
                 const char *prop __attribute__((unused)),
                 void **bufp)
{
    *bufp = NULL;
    return -1;
}

/*
 * OF_getencprop — returns -1 for all encoded properties.
 */
static inline ssize_t
OF_getencprop(phandle_t node __attribute__((unused)),
              const char *prop __attribute__((unused)),
              uint32_t *buf __attribute__((unused)),
              size_t len __attribute__((unused)))
{
    return -1;
}

/*
 * OF_getencprop_alloc_multi — returns -1 (no multi-cell properties).
 * Per CLAUDE.md: this stub causes bcm_gpio_get_ro_pins to return -1,
 * which makes bcm_gpio_get_reserved_pins return 0 (success, no ro pins).
 */
static inline int
OF_getencprop_alloc_multi(phandle_t node __attribute__((unused)),
                           const char *prop __attribute__((unused)),
                           int elsz __attribute__((unused)),
                           void **bufp)
{
    *bufp = NULL;
    return -1;
}

/* Free a property buffer — safe to call with NULL. */
static inline void OF_prop_free(void *buf __attribute__((unused))) {}

#endif /* _FREEBSD_SHIM_OFW_BUS_SUBR_H_ */
