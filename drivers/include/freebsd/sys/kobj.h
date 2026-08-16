/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/kobj.h shim — re-exports from shim_core.h.
 *
 * All KOBJ types (kobjop_desc, kobj_method_t, driver_t, …) live in
 * shim_core.h so they are shared between FreeBSD driver sources (compiled
 * with the isolated include path) and shim-side files compiled with the
 * normal Unikraft include path.  Per-driver extras live in the driver's own
 * <driver>_internal.h, which is NOT on the FreeBSD isolated include path.
 */
#pragma once
#include <shim_core.h>
