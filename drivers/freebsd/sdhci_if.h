/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * sdhci_if.h — SDHCI KOBJ interface (shim version).
 *
 * In FreeBSD this file is auto-generated from sdhci_if.m.  This shim
 * version provides the same SDHCI_READ_X/SDHCI_WRITE_X dispatch macros
 * and kobjop_desc externs, using the simplified kobj_lookup() from
 * shim_core.h rather than the full FreeBSD kobj dispatch mechanism.
 *
 * The descriptors are defined in sdhci_kobj_descriptors.c.
 */
#pragma once
#include <shim_core.h>
#include <dev/sdhci/sdhci.h>   /* struct sdhci_slot */

/* ---- kobjop_desc externs ---- */
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

/* ---- dispatch inlines ---- */

static inline uint8_t
SDHCI_READ_1(device_t brdev, struct sdhci_slot *slot, bus_size_t off)
{
    typedef uint8_t (*fn_t)(device_t, struct sdhci_slot *, bus_size_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_read_1_desc);
    return fn(brdev, slot, off);
}

static inline uint16_t
SDHCI_READ_2(device_t brdev, struct sdhci_slot *slot, bus_size_t off)
{
    typedef uint16_t (*fn_t)(device_t, struct sdhci_slot *, bus_size_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_read_2_desc);
    return fn(brdev, slot, off);
}

static inline uint32_t
SDHCI_READ_4(device_t brdev, struct sdhci_slot *slot, bus_size_t off)
{
    typedef uint32_t (*fn_t)(device_t, struct sdhci_slot *, bus_size_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_read_4_desc);
    return fn(brdev, slot, off);
}

static inline void
SDHCI_READ_MULTI_4(device_t brdev, struct sdhci_slot *slot, bus_size_t off,
                   uint32_t *data, bus_size_t count)
{
    typedef void (*fn_t)(device_t, struct sdhci_slot *, bus_size_t,
                         uint32_t *, bus_size_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_read_multi_4_desc);
    fn(brdev, slot, off, data, count);
}

static inline void
SDHCI_WRITE_1(device_t brdev, struct sdhci_slot *slot, bus_size_t off, uint8_t val)
{
    typedef void (*fn_t)(device_t, struct sdhci_slot *, bus_size_t, uint8_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_write_1_desc);
    fn(brdev, slot, off, val);
}

static inline void
SDHCI_WRITE_2(device_t brdev, struct sdhci_slot *slot, bus_size_t off, uint16_t val)
{
    typedef void (*fn_t)(device_t, struct sdhci_slot *, bus_size_t, uint16_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_write_2_desc);
    fn(brdev, slot, off, val);
}

static inline void
SDHCI_WRITE_4(device_t brdev, struct sdhci_slot *slot, bus_size_t off, uint32_t val)
{
    typedef void (*fn_t)(device_t, struct sdhci_slot *, bus_size_t, uint32_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_write_4_desc);
    fn(brdev, slot, off, val);
}

static inline void
SDHCI_WRITE_MULTI_4(device_t brdev, struct sdhci_slot *slot, bus_size_t off,
                    uint32_t *data, bus_size_t count)
{
    typedef void (*fn_t)(device_t, struct sdhci_slot *, bus_size_t,
                         uint32_t *, bus_size_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_write_multi_4_desc);
    fn(brdev, slot, off, data, count);
}

static inline int
SDHCI_PLATFORM_WILL_HANDLE(device_t brdev, struct sdhci_slot *slot)
{
    typedef int (*fn_t)(device_t, struct sdhci_slot *);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_platform_will_handle_desc);
    return fn(brdev, slot);
}

static inline void
SDHCI_PLATFORM_START_TRANSFER(device_t brdev, struct sdhci_slot *slot,
                               uint32_t *intmask)
{
    typedef void (*fn_t)(device_t, struct sdhci_slot *, uint32_t *);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_platform_start_transfer_desc);
    fn(brdev, slot, intmask);
}

static inline void
SDHCI_PLATFORM_FINISH_TRANSFER(device_t brdev, struct sdhci_slot *slot)
{
    typedef void (*fn_t)(device_t, struct sdhci_slot *);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_platform_finish_transfer_desc);
    fn(brdev, slot);
}

static inline int
SDHCI_GET_CARD_PRESENT(device_t brdev, struct sdhci_slot *slot)
{
    typedef int (*fn_t)(device_t, struct sdhci_slot *);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_get_card_present_desc);
    return fn(brdev, slot);
}

static inline void
SDHCI_SET_UHS_TIMING(device_t brdev, struct sdhci_slot *slot)
{
    typedef void (*fn_t)(device_t, struct sdhci_slot *);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &sdhci_set_uhs_timing_desc);
    fn(brdev, slot);
}

/*
 * SDHCI_RESET — called from sdhci_generic_reset (which we stub).
 * Defined as a no-op macro so that the stub compiles without needing
 * to call kobj_lookup (which would require a descriptor we don't expose).
 */
#define SDHCI_RESET(brdev, slot, mask) \
    SDHCI_WRITE_1((brdev), (slot), 0x2F, (mask))   /* SDHCI_SOFTWARE_RESET */

/* ---- sdhci_if.m DEVMETHOD token strings ---- */
/* These string names must match the tokens used in bcm2835_sdhci.c's
 * DEVMETHOD() table entries; they are used only at compile time.        */
#define sdhci_read_1                    sdhci_read_1
#define sdhci_read_2                    sdhci_read_2
#define sdhci_read_4                    sdhci_read_4
#define sdhci_read_multi_4              sdhci_read_multi_4
#define sdhci_write_1                   sdhci_write_1
#define sdhci_write_2                   sdhci_write_2
#define sdhci_write_4                   sdhci_write_4
#define sdhci_write_multi_4             sdhci_write_multi_4
#define sdhci_get_card_present          sdhci_get_card_present
#define sdhci_platform_will_handle      sdhci_platform_will_handle
#define sdhci_platform_start_transfer   sdhci_platform_start_transfer
#define sdhci_platform_finish_transfer  sdhci_platform_finish_transfer
#define sdhci_set_uhs_timing            sdhci_set_uhs_timing
#define sdhci_reset                     sdhci_reset
