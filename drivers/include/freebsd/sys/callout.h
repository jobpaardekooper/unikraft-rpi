/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/callout.h shim.
 *
 * Callouts are timer-driven callbacks.  In the polled Unikraft shim all
 * callout operations are no-ops: the SDHCI card-poll and timeout callouts
 * are never fired because we use direct-MMIO polling instead of waiting
 * for hardware interrupts.
 */
#pragma once
#include <stdint.h>

struct callout {
    int _dummy;
};

static inline void
callout_init(struct callout *c __attribute__((unused)),
             int mpsafe __attribute__((unused))) {}

static inline void
callout_init_mtx(struct callout *c __attribute__((unused)),
                 void *mtx __attribute__((unused)),
                 int flags __attribute__((unused))) {}

static inline int
callout_reset(struct callout *c __attribute__((unused)),
              int ticks __attribute__((unused)),
              void (*func)(void *) __attribute__((unused)),
              void *arg __attribute__((unused)))
{ return 0; }

static inline int
callout_stop(struct callout *c __attribute__((unused)))
{ return 0; }

static inline int
callout_drain(struct callout *c __attribute__((unused)))
{ return 0; }

static inline int
callout_active(struct callout *c __attribute__((unused)))
{ return 0; }

static inline int
callout_pending(struct callout *c __attribute__((unused)))
{ return 0; }
