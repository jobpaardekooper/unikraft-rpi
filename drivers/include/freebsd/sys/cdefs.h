/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/cdefs.h shim — standalone, no #include_next.
 *
 * Unikraft uses musl which does NOT provide sys/cdefs.h, so we cannot
 * forward to a system copy.  We define only the macros that bcm2835_gpio.c
 * (and its transitive includes) actually use.
 */
#pragma once

#ifndef __FBSDID
#define __FBSDID(s)
#endif

#ifndef __unused
#define __unused        __attribute__((__unused__))
#endif

#ifndef __packed
#define __packed        __attribute__((__packed__))
#endif

#ifndef __aligned
#define __aligned(x)    __attribute__((__aligned__(x)))
#endif

#ifndef __printflike
#define __printflike(fmtarg, firstvararg)
#endif

#ifndef __nonnull
#define __nonnull(args)
#endif

#ifndef __pure2
#define __pure2         __attribute__((__const__))
#endif

#ifndef __dead2
#define __dead2         __attribute__((__noreturn__))
#endif

/*
 * __diagused — marks variables used only in diagnostic assertions (KASSERT).
 * In FreeBSD this suppresses "unused variable" warnings when INVARIANTS is
 * not defined and KASSERT is a no-op.
 */
#ifndef __diagused
#define __diagused      __attribute__((__unused__))
#endif

#ifndef __BEGIN_DECLS
#define __BEGIN_DECLS
#define __END_DECLS
#endif
