/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm2835_bscvar.h — BCM2835 BSC (I2C) driver softc definition.
 *
 * Shim version: mirrors the upstream FreeBSD structure layout so
 * bcm2835_bsc.c compiles unchanged.  Fields that are no-ops in the
 * shim (sc_iicbus, sc_intrhand, sc_mtx) are present for ABI
 * compatibility but are not functionally used by the shim's polled
 * transfer path.
 */
#ifndef _BCM2835_BSCVAR_H_
#define _BCM2835_BSCVAR_H_

#include <stdint.h>
#include <sys/mutex.h>
#include <dev/iicbus/iicbus.h>      /* struct iic_msg, IIC_M_RD */

struct bcm_bsc_softc {
    device_t            sc_dev;
    struct resource    *sc_mem_res;
    struct resource    *sc_irq_res;
    void               *sc_intrhand;
    bus_space_tag_t     sc_bst;
    bus_space_handle_t  sc_bsh;
    struct mtx          sc_mtx;
    device_t            sc_iicbus;
    int                 sc_debug;

    /* Per-transfer state (updated by fill/empty helpers and ISR) */
    int                 sc_flags;
    struct iic_msg     *sc_curmsg;
    uint16_t            sc_totlen;
    uint16_t            sc_replen;
    uint16_t            sc_dlen;
    uint16_t            sc_resid;
    uint8_t            *sc_data;
};

/* sc_flags bits */
#define BCM_I2C_BUSY    0x01
#define BCM_I2C_READ    0x02
#define BCM_I2C_DONE    0x04
#define BCM_I2C_ERROR   0x08

/* Lock macros — single-threaded shim: no real contention, no-op locks */
#define BCM_BSC_LOCK(sc)    mtx_lock(&(sc)->sc_mtx)
#define BCM_BSC_UNLOCK(sc)  mtx_unlock(&(sc)->sc_mtx)

#endif /* _BCM2835_BSCVAR_H_ */
