/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/sysctl.h shim — all sysctl operations are no-ops.
 *
 * SYSCTL_ADD_NODE  returns (struct sysctl_oid *)NULL  (result is assigned)
 * SYSCTL_ADD_PROC  returns (void)0                   (result is discarded)
 *
 * The bcm_gpio_sysctl_init() function will compile and link without error,
 * but will do nothing at runtime.
 */
#pragma once
#include <stddef.h>

/* Flag constants used by the driver */
#define OID_AUTO            (-1)
#define CTLFLAG_RD          0x80000000
#define CTLFLAG_RW          0x40000000
#define CTLFLAG_RWTUN       0x40000000  /* tunable + RW, same value for shim */
#define CTLTYPE_STRING      2
#define CTLTYPE_UINT        6
#define CTLFLAG_MPSAFE      0
#define CTLFLAG_NEEDGIANT   0

/* Opaque sysctl types */
struct sysctl_oid;
struct sysctl_oid_list;
struct sysctl_ctx_list;

struct sysctl_req {
    void *newptr;   /* NULL means read-only query */
};

/*
 * SYSCTL_HANDLER_ARGS: the macro used as the parameter list for
 * sysctl handler functions (bcm_gpio_func_proc).
 */
#define SYSCTL_HANDLER_ARGS \
    struct sysctl_oid *oidp __attribute__((unused)), \
    void *arg1, \
    size_t arg2 __attribute__((unused)), \
    struct sysctl_req *req

/* ADD_NODE: returns NULL (caller assigns result to a local variable) */
#define SYSCTL_ADD_NODE(ctx, parent, nbr, name, access, handler, descr) \
    ((struct sysctl_oid *)NULL)

/* ADD_PROC: result is discarded — cast to void explicitly */
#define SYSCTL_ADD_PROC(ctx, parent, nbr, name, access, ptr, arg, \
                        handler, fmt, descr) \
    ((void)0)

#define SYSCTL_CHILDREN(node)  ((struct sysctl_oid_list *)NULL)

static inline struct sysctl_ctx_list *
device_get_sysctl_ctx(void *dev __attribute__((unused))) { return NULL; }

static inline struct sysctl_oid *
device_get_sysctl_tree(void *dev __attribute__((unused))) { return NULL; }

static inline int
sysctl_handle_string(struct sysctl_oid *oidp __attribute__((unused)),
                     void *arg1 __attribute__((unused)),
                     size_t arg2 __attribute__((unused)),
                     struct sysctl_req *req __attribute__((unused)))
{
    return 0;
}

static inline int
sysctl_handle_int(struct sysctl_oid *oidp __attribute__((unused)),
                  void *arg1 __attribute__((unused)),
                  int   arg2 __attribute__((unused)),
                  struct sysctl_req *req  __attribute__((unused)))
{
    return 0;
}

/* SYSCTL_ADD_INT — no-op, result discarded */
#define SYSCTL_ADD_INT(ctx, parent, nbr, name, access, ptr, val, descr) \
    ((void)0)

/* SYSCTL_ADD_UINT — no-op */
#define SYSCTL_ADD_UINT(ctx, parent, nbr, name, access, ptr, val, descr) \
    ((void)0)

/*
 * Top-level SYSCTL_NODE / SYSCTL_INT / SYSCTL_UINT macros.
 * sdhci.c emits these at file scope (outside any function).
 * They expand to nothing so the translation unit still compiles.
 */
#define SYSCTL_NODE(parent, nbr, name, access, handler, descr)  /* no-op */
#define SYSCTL_INT(parent, nbr, name, access, ptr, val, descr)  /* no-op */
#define SYSCTL_UINT(parent, nbr, name, access, ptr, val, descr) /* no-op */

/* SYSCTL_DECL — declares an extern sysctl tree node; no-op in shim */
#define SYSCTL_DECL(name)  /* no-op */

/*
 * TUNABLE_INT — declares a boot-time tunable integer; no-op in shim.
 * bcm2835_sdhci.c uses TUNABLE_INT at file scope to register tunables.
 */
#define TUNABLE_INT(path, ptr)  /* no-op */
#define TUNABLE_STR(path, ptr, len)  /* no-op */
