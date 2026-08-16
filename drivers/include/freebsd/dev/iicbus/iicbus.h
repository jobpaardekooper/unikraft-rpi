/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dev/iicbus/iicbus.h shim — I2C message types and iicbus interface stubs.
 *
 * The real FreeBSD iicbus is a bus abstraction layer between the host
 * controller (BSC) and client devices (sensors, EEPROMs …).  In the
 * Unikraft shim that layer is replaced by the direct-MMIO polled transfer
 * functions in bcm_bsc_shim.c, so only the data types and a few constants
 * are needed here.
 */
#ifndef _DEV_IICBUS_IICBUS_H_
#define _DEV_IICBUS_IICBUS_H_

#include <stdint.h>
#include <shim_core.h>  /* device_t, driver_t */

/* I2C message descriptor (same layout as Linux struct i2c_msg) ------------ */
struct iic_msg {
    uint16_t  slave;    /* 8-bit address: 7-bit << 1, lsb = R/W */
    uint16_t  flags;
    uint16_t  len;
    uint8_t  *buf;
};

#define IIC_M_WR    0x0000      /* write direction (default) */
#define IIC_M_RD    0x0001      /* read direction */

/* Bus frequency returned when no real iicbus child is present ------------- */
#define IICBUS_GET_FREQUENCY(iicbus, speed)  100000U    /* 100 kHz default */

/* Null callback — used by DEVMETHOD(iicbus_callback, iicbus_null_callback) */
static inline void
iicbus_null_callback(device_t dev   __attribute__((unused)),
                     int      index __attribute__((unused)),
                     caddr_t  data  __attribute__((unused))) {}

/*
 * iicbus_driver — stub for the DRIVER_MODULE(iicbus, …) macro.
 * The macro fires a constructor that checks driver.methods != NULL before
 * registering; this stub has methods=NULL so it is silently skipped.
 */
extern driver_t iicbus_driver;

#endif /* _DEV_IICBUS_IICBUS_H_ */
