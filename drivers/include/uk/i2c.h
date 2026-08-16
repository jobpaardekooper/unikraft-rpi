/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * uk/i2c.h — Public I2C API for Unikraft on Raspberry Pi 3.
 *
 * Targets BSC1 (I2C1): GPIO 2 = SDA (header pin 3), GPIO 3 = SCL (pin 5).
 * Default bus frequency: 100 kHz (Standard Mode).
 *
 * All functions are polled; they block until the transfer completes or
 * a timeout elapses.  No interrupt handler is required.
 *
 * Usage:
 *   uk_i2c_init();
 *   uint8_t buf[2] = { REG_ADDR, VALUE };
 *   uk_i2c_write(0x48, buf, 2);           // write REG_ADDR then VALUE
 *   uint8_t val;
 *   uk_i2c_write_read(0x48, buf, 1, &val, 1); // write reg, repeated-start, read
 */
#ifndef _UK_I2C_H_
#define _UK_I2C_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * uk_i2c_init() — initialise the BSC1 controller via the FreeBSD KOBJ shim.
 *
 * Runs bcm_bsc_probe + bcm_bsc_attach through the KOBJ dispatch mechanism.
 * bcm_bsc_attach resets the hardware (enables the controller, clears the
 * FIFO, sets 100 kHz clock divider).
 *
 * Returns 0 on success, negative errno on failure.
 */
int uk_i2c_init(void);

/**
 * uk_i2c_write() — write @len bytes from @buf to I2C slave @addr7.
 *
 * @addr7: 7-bit slave address (NOT shifted — the shim shifts internally).
 * @buf:   data to send.
 * @len:   number of bytes (1–255; limited by DLEN register).
 *
 * Returns 0 on success, -EIO on NACK, -ETIMEDOUT on timeout.
 */
int uk_i2c_write(uint8_t addr7, const uint8_t *buf, uint16_t len);

/**
 * uk_i2c_read() — read @len bytes into @buf from I2C slave @addr7.
 *
 * Returns 0 on success, -EIO on NACK, -ETIMEDOUT on timeout.
 */
int uk_i2c_read(uint8_t addr7, uint8_t *buf, uint16_t len);

/**
 * uk_i2c_write_read() — write @wlen bytes then issue a repeated-START and
 * read @rlen bytes from the same slave.  Common for register-addressed reads.
 *
 * Returns 0 on success, -EIO on NACK, -ETIMEDOUT on timeout.
 */
int uk_i2c_write_read(uint8_t addr7,
                      const uint8_t *wbuf, uint16_t wlen,
                      uint8_t *rbuf,       uint16_t rlen);

#ifdef __cplusplus
}
#endif

#endif /* _UK_I2C_H_ */
