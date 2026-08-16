/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/conf.h shim — character/block device stubs.
 * Only needed when MMCCAM is defined; since we never define MMCCAM,
 * this header is compiled only for linkage of opaque types.
 */
#pragma once
#include <stdint.h>

/* dev_t: musl defines it as unsigned long; only define if not already present */
#ifndef _DEV_T_DEFINED_
#ifndef __dev_t_defined
typedef int _shim_dev_t;  /* placeholder; FreeBSD dev_t not used in shim */
#endif
#endif

/*
 * dumping — set to non-zero during a kernel core dump.
 * bcm_sdhci_will_handle_transfer() checks this to skip DMA during a dump.
 * Always 0 in the shim (defined in bcm_sdhci_shim.c).
 */
extern int dumping;
