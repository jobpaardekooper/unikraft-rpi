/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * mmcbr_if.h — MMC bridge KOBJ interface (shim version).
 *
 * In FreeBSD this file is auto-generated from mmcbr_if.m.  This shim
 * version provides the kobjop_desc externs and dispatch macros that
 * bcm2835_sdhci.c places in its DEVMETHOD table.  The MMCBR_* functions
 * are never called in the shim (we bypass mmc.c entirely); they exist only
 * for linking and for the DEVMETHOD table to compile.
 *
 * The descriptors are defined in sdhci_kobj_descriptors.c.
 */
#pragma once
#include <shim_core.h>

/* ---- kobjop_desc externs ---- */
extern struct kobjop_desc mmcbr_update_ios_desc;
extern struct kobjop_desc mmcbr_request_desc;
extern struct kobjop_desc mmcbr_get_ro_desc;
extern struct kobjop_desc mmcbr_acquire_host_desc;
extern struct kobjop_desc mmcbr_release_host_desc;
extern struct kobjop_desc mmcbr_tune_desc;
extern struct kobjop_desc mmcbr_retune_desc;
extern struct kobjop_desc mmcbr_switch_vccq_desc;

/* Forward declarations needed by dispatch macros */
struct mmc_request;

/* ---- dispatch inlines (never called in shim path) ---- */

static inline int
MMCBR_UPDATE_IOS(device_t brdev, device_t reqdev)
{
    typedef int (*fn_t)(device_t, device_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &mmcbr_update_ios_desc);
    return fn(brdev, reqdev);
}

static inline int
MMCBR_REQUEST(device_t brdev, device_t reqdev, struct mmc_request *req)
{
    typedef int (*fn_t)(device_t, device_t, struct mmc_request *);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &mmcbr_request_desc);
    return fn(brdev, reqdev, req);
}

static inline int
MMCBR_GET_RO(device_t brdev, device_t reqdev)
{
    typedef int (*fn_t)(device_t, device_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &mmcbr_get_ro_desc);
    return fn(brdev, reqdev);
}

static inline int
MMCBR_ACQUIRE_HOST(device_t brdev, device_t reqdev)
{
    typedef int (*fn_t)(device_t, device_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &mmcbr_acquire_host_desc);
    return fn(brdev, reqdev);
}

static inline int
MMCBR_RELEASE_HOST(device_t brdev, device_t reqdev)
{
    typedef int (*fn_t)(device_t, device_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &mmcbr_release_host_desc);
    return fn(brdev, reqdev);
}

static inline int
MMCBR_TUNE(device_t brdev, device_t reqdev, int hs400)
{
    typedef int (*fn_t)(device_t, device_t, int);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &mmcbr_tune_desc);
    return fn(brdev, reqdev, hs400);
}

static inline int
MMCBR_RETUNE(device_t brdev, device_t reqdev, int reset)
{
    typedef int (*fn_t)(device_t, device_t, int);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &mmcbr_retune_desc);
    return fn(brdev, reqdev, reset);
}

static inline int
MMCBR_SWITCH_VCCQ(device_t brdev, device_t reqdev)
{
    typedef int (*fn_t)(device_t, device_t);
    fn_t fn = (fn_t)kobj_lookup(brdev->d_methods, &mmcbr_switch_vccq_desc);
    return fn(brdev, reqdev);
}

/* DEVMETHOD token names — used at compile time only */
#define mmcbr_update_ios    mmcbr_update_ios
#define mmcbr_request       mmcbr_request
#define mmcbr_get_ro        mmcbr_get_ro
#define mmcbr_acquire_host  mmcbr_acquire_host
#define mmcbr_release_host  mmcbr_release_host
#define mmcbr_tune          mmcbr_tune
#define mmcbr_retune        mmcbr_retune
#define mmcbr_switch_vccq   mmcbr_switch_vccq
