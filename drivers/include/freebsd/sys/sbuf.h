/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/sbuf.h shim.
 *
 * sbuf (string buffer) is used for sysctl output in sdhci.c debug paths.
 * All operations are no-ops: debug output is already suppressed by
 * sdhci_debug == 0 (sysctl stub returns 0).
 */
#pragma once
#include <stddef.h>
#include <stdarg.h>

struct sbuf {
    int _dummy;
};

static inline struct sbuf *
sbuf_new(struct sbuf *s __attribute__((unused)),
         char *buf __attribute__((unused)),
         int length __attribute__((unused)),
         int flags __attribute__((unused)))
{ return NULL; }

static inline struct sbuf *
sbuf_new_auto(void)
{ return NULL; }

static inline int
sbuf_printf(struct sbuf *s __attribute__((unused)),
            const char *fmt __attribute__((unused)), ...)
{ return 0; }

static inline int
sbuf_vprintf(struct sbuf *s __attribute__((unused)),
             const char *fmt __attribute__((unused)),
             va_list ap __attribute__((unused)))
{ return 0; }

static inline void
sbuf_cat(struct sbuf *s __attribute__((unused)),
         const char *str __attribute__((unused))) {}

static inline int
sbuf_finish(struct sbuf *s __attribute__((unused)))
{ return 0; }

static inline void
sbuf_delete(struct sbuf *s __attribute__((unused))) {}

static inline const char *
sbuf_data(struct sbuf *s __attribute__((unused)))
{ return ""; }

#define SBUF_AUTOEXTEND  0x0002
