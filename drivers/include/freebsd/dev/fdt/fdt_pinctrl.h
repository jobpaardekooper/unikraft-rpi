/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD dev/fdt/fdt_pinctrl.h shim — no-ops.
 *
 * fdt_pinctrl_register     → no-op
 * fdt_pinctrl_configure_tree → no-op
 *
 * Also declares fdt_pinctrl_configure_desc for the DEVMETHOD table.
 */
#pragma once
#include <sys/bus.h>
#include <sys/kobj.h>

/* KOBJ method descriptor for fdt_pinctrl_configure */
extern struct kobjop_desc fdt_pinctrl_configure_desc;

static inline void
fdt_pinctrl_register(device_t dev __attribute__((unused)),
                     const char *prop __attribute__((unused))) {}

static inline void
fdt_pinctrl_configure_tree(device_t dev __attribute__((unused))) {}
