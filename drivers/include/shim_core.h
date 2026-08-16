/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * shim_core.h — driver-agnostic KOBJ/shim infrastructure.
 *
 * This is the single source of truth for types and macros that every FreeBSD
 * driver ported through the shim layer needs, regardless of which peripheral
 * is being ported.  It deliberately has no knowledge of any specific driver.
 *
 * To port a new driver:
 *   1. Create drivers/include/<driver>_internal.h that #includes this file.
 *   2. Add any driver-specific bus functions, constants, and kobjop_desc
 *      externs to that per-driver header.
 *   3. The FreeBSD-path shim headers (sys/kobj.h, sys/bus.h, …) already
 *      include this file — no changes to them are required.
 *
 * Include constraints
 * ──────────────────
 * Only <stdint.h> and <stddef.h> are included here.  This keeps the header
 * safe to include from both:
 *   • shim-side files compiled with the normal Unikraft include path
 *   • FreeBSD-side shim stubs compiled with the isolated FreeBSD include path
 */
#ifndef _SHIM_CORE_H_
#define _SHIM_CORE_H_

#include <stdint.h>
#include <stddef.h>

/* =========================================================
 * KOBJ method descriptor and dispatch table
 * ========================================================= */

struct kobjop_desc {
    const char *name;
    void       *default_func;
};

typedef struct kobj_method {
    const struct kobjop_desc *desc;
    void                     *func;
} kobj_method_t;

typedef kobj_method_t device_method_t;

#define DEVMETHOD_END           { NULL, NULL }
#define DEVMETHOD(method, func) { &method##_desc, (void *)(func) }

static inline void *
kobj_lookup(const kobj_method_t *methods, const struct kobjop_desc *desc)
{
    const kobj_method_t *m;
    for (m = methods; m->desc != NULL; m++)
        if (m->desc == desc)
            return m->func;
    return desc->default_func;
}

/* =========================================================
 * driver_t — FreeBSD driver descriptor
 * ========================================================= */

typedef struct driver {
    const char          *name;
    const kobj_method_t *methods;
    size_t               size;      /* sizeof(softc) */
} driver_t;

/* =========================================================
 * shim_driver_reg — written by constructor, read by uk_<drv>_init()
 *
 * Each ported driver defines one instance of this struct (in its own
 * kobj_descriptors.c equivalent).  The EARLY_DRIVER_MODULE /
 * DRIVER_MODULE constructor calls the driver's _shim_<drv>_register()
 * function which fills it in before uk_main() runs.
 * ========================================================= */

struct shim_driver_reg {
    const kobj_method_t *sdr_methods;
    size_t               sdr_softc_size;
};

/* =========================================================
 * device_t — minimal device handle for KOBJ dispatch
 * ========================================================= */

struct device {
    void                   *d_softc;
    const kobj_method_t    *d_methods;
    const char             *d_nameunit;
};
typedef struct device *device_t;

static inline void *
device_get_softc(device_t dev) { return dev ? dev->d_softc : NULL; }

static inline void
device_set_desc(device_t dev __attribute__((unused)),
                const char *desc __attribute__((unused))) {}

static inline const char *
device_get_nameunit(device_t dev)
{
    return (dev && dev->d_nameunit) ? dev->d_nameunit : "dev0";
}

/* =========================================================
 * struct resource — MMIO / IRQ resource handle
 * ========================================================= */

#define SYS_RES_MEMORY  1
#define SYS_RES_IRQ     3
#define RF_ACTIVE       1
#define RF_SHAREABLE    2   /* IRQ sharing flag (sys/rman.h in FreeBSD) */
#define DEVICE_UNIT_ANY (-1)/* auto-assign unit number (sys/bus.h in FreeBSD) */

typedef uintptr_t bus_space_tag_t;
typedef uintptr_t bus_space_handle_t;

struct resource {
    bus_space_tag_t    r_bustag;
    bus_space_handle_t r_bushandle;
    int                r_type;
    int                r_rid;
    uintptr_t          r_start;
};

struct resource_spec {
    int type;
    int rid;
    int flags;
};

static inline bus_space_tag_t
rman_get_bustag(struct resource *r) { return r->r_bustag; }

static inline bus_space_handle_t
rman_get_bushandle(struct resource *r) { return r->r_bushandle; }

/* =========================================================
 * Interrupt types — used by bus_setup_intr flags argument
 * ========================================================= */

typedef int  (*driver_filter_t)(void *);
typedef void (*driver_intr_t)(void *);

#define INTR_TYPE_MISC  0
#define INTR_MPSAFE     0

/* bus_setup_intr / bus_teardown_intr are implemented per-driver in each
 * <driver>_shim.c, because the no-op or logging behaviour may differ. */
int  bus_setup_intr(device_t dev, struct resource *r, int flags,
                    driver_filter_t filter, driver_intr_t ithread,
                    void *arg, void **cookiep);
int  bus_teardown_intr(device_t dev, struct resource *r, void *cookie);

/*
 * bus_generic_setup_intr / bus_generic_teardown_intr
 *
 * Used in DEVMETHOD tables by drivers that delegate interrupt setup to the
 * parent bus (e.g. bcm2835_gpio.c uses bus_generic_setup_intr in its
 * bcm_gpio_bus_methods[] table).  Both drivers (GPIO and BSC) reference
 * these, so they live here in shim_core.h rather than in a driver-specific
 * header.  Both are no-ops under polled operation.
 */
static inline int
bus_generic_setup_intr(device_t dev __attribute__((unused)),
                       struct resource *r __attribute__((unused)),
                       int flags __attribute__((unused)),
                       driver_filter_t filter __attribute__((unused)),
                       driver_intr_t ithread __attribute__((unused)),
                       void *arg __attribute__((unused)),
                       void **cookiep __attribute__((unused)))
{ return 0; }

static inline int
bus_generic_teardown_intr(device_t dev __attribute__((unused)),
                          struct resource *r __attribute__((unused)),
                          void *cookie __attribute__((unused)))
{ return 0; }

/*
 * Generic bus child management helpers (sys/bus.h in FreeBSD).
 * All are no-ops: the shim uses KOBJ dispatch directly and does not
 * enumerate a real child bus.
 */
static inline void
bus_attach_children(device_t dev __attribute__((unused))) {}

static inline int
bus_generic_detach(device_t dev __attribute__((unused))) { return 0; }

static inline int
bus_delayed_attach_children(device_t dev __attribute__((unused))) { return 0; }

/*
 * bus_identify_children — enumerate potential children before attach.
 * No-op: the shim has no real bus enumeration.
 */
static inline void
bus_identify_children(device_t dev __attribute__((unused))) {}

/* =========================================================
 * struct malloc_type — FreeBSD memory-type descriptor.
 * Used by M_DEVBUF / M_TEMP which sdhci_kobj_descriptors.c defines.
 * Also defined in sys/malloc.h; the guard prevents double-definition.
 * ========================================================= */

#ifndef _SHIM_MALLOC_TYPE_DEFINED_
#define _SHIM_MALLOC_TYPE_DEFINED_
struct malloc_type {
    const char *ks_shortdesc;
};
#endif

/* =========================================================
 * struct thread / curthread stub
 *
 * Only accessed in interrupt handler paths that are dead code under
 * polled operation (e.g. bcm_gpio_intr).  A single static instance
 * satisfies the linker without pulling in a scheduler.
 * ========================================================= */

struct thread {
    void *td_intr_frame;
};

extern struct thread _shim_thread;
#define curthread  (&_shim_thread)

/* =========================================================
 * Common kobjop_desc externs (device + bus interface)
 *
 * These are defined once in kobj_descriptors.c (GPIO shim side).
 * Any driver shim that links against that file gets them for free.
 * ========================================================= */

extern struct kobjop_desc device_probe_desc;
extern struct kobjop_desc device_attach_desc;
extern struct kobjop_desc device_detach_desc;
extern struct kobjop_desc bus_setup_intr_desc;
extern struct kobjop_desc bus_teardown_intr_desc;

/* bus ivar / child management descriptors (used by bcm2835_sdhci.c devmethods) */
extern struct kobjop_desc bus_read_ivar_desc;
extern struct kobjop_desc bus_write_ivar_desc;
extern struct kobjop_desc bus_add_child_desc;

/* =========================================================
 * DEVICE_PROBE / DEVICE_ATTACH dispatch macros
 * ========================================================= */

#define DEVICE_PROBE(dev)  \
    (((int (*)(device_t)) \
        kobj_lookup((dev)->d_methods, &device_probe_desc))(dev))

#define DEVICE_ATTACH(dev) \
    (((int (*)(device_t)) \
        kobj_lookup((dev)->d_methods, &device_attach_desc))(dev))

#endif /* _SHIM_CORE_H_ */
