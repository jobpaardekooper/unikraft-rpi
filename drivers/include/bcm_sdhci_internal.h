/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm_sdhci_internal.h — SDHCI driver-specific shim types and declarations.
 *
 * Builds on shim_core.h (driver-agnostic KOBJ infrastructure) and adds
 * only what the BCM2835 SDHCI driver port requires:
 *   - Driver registration function and registry extern
 *   - Single-resource bus allocation (bus_alloc_resource_any)
 *   - sdhci_slot sub-types referenced from the shim side
 *   - sdhci_generic_* stubs exported from bcm_sdhci_shim.c
 *
 * Included by:
 *   • bcm_sdhci_shim.c         (Unikraft-side API layer + polled SD protocol)
 *   • sdhci_kobj_descriptors.c (KOBJ descriptor and registry definitions)
 *
 * NOT included from the FreeBSD isolated include path.  FreeBSD-path headers
 * (sys/kobj.h, sys/bus.h, …) include shim_core.h directly.
 */
#ifndef _BCM_SDHCI_INTERNAL_H_
#define _BCM_SDHCI_INTERNAL_H_

#include <shim_core.h>

/* =========================================================
 * SDHCI driver registration
 *
 * _shim_sdhci_register() is called from the DRIVER_MODULE constructor
 * defined in sys/kernel.h (FreeBSD include path).  Using a function
 * avoids referencing shim_sdhci_driver_reg from the FreeBSD compile unit.
 * ========================================================= */

void _shim_sdhci_register(const void *methods, size_t softc_size);
extern struct shim_driver_reg shim_sdhci_driver_reg;

/* =========================================================
 * SDHCI-specific resource allocation
 *
 * bcm2835_sdhci.c uses bus_alloc_resource_any() (single-resource variant)
 * and RF_SHAREABLE for IRQ sharing.  Implemented in bcm_sdhci_shim.c.
 * ========================================================= */

struct resource *bus_alloc_resource_any(device_t dev, int type, int *rid,
                                        unsigned int flags);
void bus_release_resource(device_t dev, int type, int rid,
                          struct resource *r);

/* =========================================================
 * Child device management
 *
 * bcm_sdhci_attach() calls bus_identify_children / bus_attach_children
 * (both no-ops in shim_core.h) and does NOT call device_add_child.
 * bus_generic_add_child IS referenced in the DEVMETHOD table, however,
 * so we provide a stub that just returns NULL.
 * ========================================================= */

device_t device_add_child(device_t dev, const char *devname, int unit);
device_t bus_generic_add_child(device_t dev, int order,
                               const char *name, int unit);

/* =========================================================
 * sdhci_generic_* stubs (satisfy linker; never called in polled path)
 *
 * These are defined in bcm_sdhci_shim.c.  They correspond to functions
 * that sdhci.c normally provides, but since we do not compile sdhci.c
 * (it depends on full kobj dispatch), we provide stub implementations.
 * ========================================================= */

/* Forward-declare struct sdhci_slot so the function prototypes compile.
 * The full definition is in dev/sdhci/sdhci.h (FreeBSD isolated path).
 * On the shim side we only need the pointer type for these prototypes.  */
struct sdhci_slot;
struct mmc_request;

int  sdhci_init_slot(device_t dev, struct sdhci_slot *slot, int num);
void sdhci_start_slot(struct sdhci_slot *slot);
int  sdhci_cleanup_slot(struct sdhci_slot *slot);
void sdhci_finish_data(struct sdhci_slot *slot);
void sdhci_generic_intr(struct sdhci_slot *slot);
int  sdhci_generic_update_ios(device_t brdev, device_t reqdev);
int  sdhci_generic_request(device_t brdev, device_t reqdev,
                           struct mmc_request *req);
int  sdhci_generic_get_ro(device_t brdev, device_t reqdev);
int  sdhci_generic_acquire_host(device_t brdev, device_t reqdev);
int  sdhci_generic_release_host(device_t brdev, device_t reqdev);
int  sdhci_generic_read_ivar(device_t bus, device_t child, int which,
                              uintptr_t *result);
int  sdhci_generic_write_ivar(device_t bus, device_t child, int which,
                               uintptr_t value);

/* =========================================================
 * SDHCI kobjop_desc externs (defined in sdhci_kobj_descriptors.c)
 * ========================================================= */

extern struct kobjop_desc sdhci_read_1_desc;
extern struct kobjop_desc sdhci_read_2_desc;
extern struct kobjop_desc sdhci_read_4_desc;
extern struct kobjop_desc sdhci_read_multi_4_desc;
extern struct kobjop_desc sdhci_write_1_desc;
extern struct kobjop_desc sdhci_write_2_desc;
extern struct kobjop_desc sdhci_write_4_desc;
extern struct kobjop_desc sdhci_write_multi_4_desc;
extern struct kobjop_desc sdhci_get_card_present_desc;
extern struct kobjop_desc sdhci_platform_will_handle_desc;
extern struct kobjop_desc sdhci_platform_start_transfer_desc;
extern struct kobjop_desc sdhci_platform_finish_transfer_desc;
extern struct kobjop_desc sdhci_set_uhs_timing_desc;
extern struct kobjop_desc sdhci_reset_desc;

/* MMCBR interface kobjop_desc externs */
extern struct kobjop_desc mmcbr_update_ios_desc;
extern struct kobjop_desc mmcbr_request_desc;
extern struct kobjop_desc mmcbr_get_ro_desc;
extern struct kobjop_desc mmcbr_acquire_host_desc;
extern struct kobjop_desc mmcbr_release_host_desc;

#endif /* _BCM_SDHCI_INTERNAL_H_ */
