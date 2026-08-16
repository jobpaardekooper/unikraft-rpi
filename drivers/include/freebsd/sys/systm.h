/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/systm.h shim.
 *
 * printf  → standard stdio printf (included via param.h → stdio.h)
 * bzero   → memset(..., 0, ...) from string.h
 * DELAY   → no-op (GPIO driver uses DELAY only in bcm_gpio_set_pud BCM2835
 *           path; our shim handles set_pud directly)
 * panic   → abort() / infinite loop (KASSERT stub from kernel.h calls it)
 */
#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>   /* abort() */

/* No-op micro-second delay stub */
static inline void DELAY(int n) { (void)n; }

/* bootverbose is defined as the macro 0 in sys/param.h */

/* panic: print and hang (bare-metal, no way to gracefully exit) */
#ifndef panic
static inline __attribute__((noreturn)) void panic(const char *msg, ...) {
    (void)msg;
    for (;;) __asm__ volatile("wfi");
}
#endif
