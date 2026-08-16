/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD machine/intr.h shim (AArch64).
 *
 * arm_irq_memory_barrier  → DSB (Data Synchronisation Barrier).
 *   Called by bcm_gpio_pic_enable_intr() before unmasking a pin IRQ.
 *
 * intr_isrc_* stubs (not used in our simplified shim path — we use the
 *   inline stubs in sys/interrupt.h).
 */
#pragma once
#include <stdint.h>
#include <dev/ofw/ofw_bus_subr.h>   /* phandle_t, BCM2837 constants */

/* Data Synchronisation Barrier — required before/after IRQ mask changes */
static inline void arm_irq_memory_barrier(uint32_t irq __attribute__((unused)))
{
    __asm__ volatile("dsb sy" ::: "memory");
}
