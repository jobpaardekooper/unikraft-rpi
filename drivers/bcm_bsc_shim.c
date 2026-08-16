/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm_bsc_shim.c — BSC (I2C) API translation layer for Unikraft / RPi3.
 *
 * Architecture
 * ────────────
 * bcm2835_bsc.c (unmodified FreeBSD driver) is used for hardware
 * initialisation only: its bcm_bsc_probe + bcm_bsc_attach path resets the
 * BSC1 controller and sets the 100 kHz clock divider via the existing KOBJ
 * dispatch mechanism.
 *
 * Transfers are NOT routed through bcm_bsc_transfer() because that function
 * is interrupt-driven (it calls mtx_sleep() waiting for bcm_bsc_intr() to
 * set BCM_I2C_DONE).  Instead, uk_i2c_write / uk_i2c_read / uk_i2c_write_read
 * bypass the FreeBSD transfer path entirely and operate on the BSC1 MMIO
 * registers directly in a tight polling loop.
 *
 * This is safe because:
 *   • We are single-threaded (cooperative scheduler, no preemption).
 *   • The FIFO is managed inline: TX bytes are fed as TXD is asserted;
 *     RX bytes are drained as RXD is asserted.
 *   • Transfers of up to 255 bytes are supported (DLEN is 16 bits but
 *     the FIFO is only 16 bytes deep, so >16 byte transfers require
 *     mid-transfer FIFO management — handled by the polling loops below).
 *
 * bus_alloc_resource_any / bus_release_resource
 * ─────────────────────────────────────────────
 * bcm2835_bsc.c uses bus_alloc_resource_any() (single-resource variant)
 * rather than the bus_alloc_resources() array variant used by bcm2835_gpio.c.
 * Both variants are implemented here.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include <uk/print.h>
#include <uk/alloc.h>

#include <uk/i2c.h>
#include <bcm_bsc_internal.h>

/* =========================================================================
 * BSC1 MMIO constants
 * ========================================================================= */
#define BSC1_BASE   0x3F804000UL

/* Register offsets (from bcm2835_bscreg.h, duplicated for the shim side) */
#define _BSC_CTRL   0x00
#define _BSC_STATUS 0x04
#define _BSC_DLEN   0x08
#define _BSC_SLAVE  0x0C
#define _BSC_DATA   0x10
#define _BSC_CLOCK  0x14

/* Control bits */
#define _CTRL_I2CEN (1u << 15)
#define _CTRL_ST    (1u <<  7)
#define _CTRL_CLR0  (1u <<  4)
#define _CTRL_READ  (1u <<  0)

/* Status bits */
#define _ST_CLKT    (1u <<  9)
#define _ST_ERR     (1u <<  8)
#define _ST_RXD     (1u <<  5)
#define _ST_TXD     (1u <<  4)
#define _ST_DONE    (1u <<  1)
#define _ST_TA      (1u <<  0)
#define _ST_ERRBITS (_ST_ERR | _ST_CLKT)
#define _ST_CLRBITS (_ST_CLKT | _ST_ERR | _ST_DONE)

/* Polling iteration limit (~several hundred ms at typical loop speed) */
#define BSC_POLL_LIMIT  500000

static inline volatile uint32_t *_bsc(uint32_t off)
{
    return (volatile uint32_t *)(BSC1_BASE + off);
}

static inline void _bsc_reset(void)
{
    *_bsc(_BSC_CTRL)   = _CTRL_I2CEN;
    *_bsc(_BSC_STATUS) = _ST_CLRBITS;          /* ack / clear error bits */
    *_bsc(_BSC_CTRL)   = _CTRL_I2CEN | _CTRL_CLR0;  /* flush FIFO */
}

/* =========================================================================
 * Static resources handed to bcm_bsc_attach
 * ========================================================================= */

static struct resource _bsc_res_mem = {
    .r_bustag    = 0,
    .r_bushandle = BSC1_BASE,
    .r_type      = SYS_RES_MEMORY,
    .r_rid       = 0,
    .r_start     = BSC1_BASE,
};

static struct resource _bsc_res_irq = {
    .r_bustag    = 0,
    .r_bushandle = 0,
    .r_type      = SYS_RES_IRQ,
    .r_rid       = 0,
    .r_start     = 53,  /* BCM2837 BSC1 GPU IRQ 53 (DTS: <2 21>) */
};

/* =========================================================================
 * _bcm_bsc_alloc_resource — BSC/I2C resource allocator.
 * Called by bus_alloc_resource_any() in bus_stubs.c when dev->d_nameunit
 * starts with "iichb".  bus_release_resource is a no-op in bus_stubs.c.
 * ========================================================================= */

struct resource *
_bcm_bsc_alloc_resource(device_t dev  __attribute__((unused)),
                         int type,
                         int *rid      __attribute__((unused)),
                         unsigned int flags __attribute__((unused)))
{
    if (type == SYS_RES_MEMORY)
        return &_bsc_res_mem;
    if (type == SYS_RES_IRQ)
        return &_bsc_res_irq;
    return NULL;
}

/* =========================================================================
 * uk_i2c_init — probe + attach via KOBJ, then confirm hardware is up
 * ========================================================================= */

static struct device _bsc_device = {
    .d_softc    = NULL,
    .d_methods  = NULL,
    .d_nameunit = "iichb0",
};

int uk_i2c_init(void)
{
    int rc;

    if (shim_bsc_driver_reg.sdr_methods == NULL) {
        uk_pr_err("uk_i2c_init: DRIVER_MODULE constructor did not run\n");
        return -ENXIO;
    }

    _bsc_device.d_methods = shim_bsc_driver_reg.sdr_methods;
    _bsc_device.d_softc   = uk_calloc(uk_alloc_get_default(), 1,
                                       shim_bsc_driver_reg.sdr_softc_size);
    if (!_bsc_device.d_softc) {
        uk_pr_err("uk_i2c_init: failed to allocate softc\n");
        return -ENOMEM;
    }

    rc = DEVICE_PROBE(&_bsc_device);
    if (rc != 0) {
        uk_pr_err("uk_i2c_init: DEVICE_PROBE = %d\n", rc);
        goto fail;
    }

    rc = DEVICE_ATTACH(&_bsc_device);
    if (rc != 0) {
        uk_pr_err("uk_i2c_init: DEVICE_ATTACH = %d\n", rc);
        goto fail;
    }

    uk_pr_info("uk_i2c_init: BCM2835 BSC1 (I2C1) initialised at 100 kHz\n");
    return 0;

fail:
    uk_free(uk_alloc_get_default(), _bsc_device.d_softc);
    _bsc_device.d_softc = NULL;
    return -ENXIO;
}

/* =========================================================================
 * uk_i2c_write — polled write, bypasses bcm_bsc_transfer entirely
 * ========================================================================= */

int uk_i2c_write(uint8_t addr7, const uint8_t *buf, uint16_t len)
{
    uint32_t status;
    uint16_t tx_sent = 0;
    int n;

    _bsc_reset();

    *_bsc(_BSC_SLAVE) = addr7;
    *_bsc(_BSC_DLEN)  = len;

    /* Pre-fill FIFO (up to 16 bytes) before asserting ST */
    while (tx_sent < len && tx_sent < 16) {
        *_bsc(_BSC_DATA) = buf[tx_sent++];
    }

    /* Start transfer — write direction (READ bit = 0), no interrupt flags */
    *_bsc(_BSC_CTRL) = _CTRL_I2CEN | _CTRL_ST;

    for (n = BSC_POLL_LIMIT; n > 0; n--) {
        status = *_bsc(_BSC_STATUS);

        if (status & _ST_ERRBITS) {
            _bsc_reset();
            return (status & _ST_ERR) ? -EIO : -ETIMEDOUT;
        }

        /* Feed more bytes while there is space and data remaining */
        while (tx_sent < len && (status & _ST_TXD)) {
            *_bsc(_BSC_DATA) = buf[tx_sent++];
            status = *_bsc(_BSC_STATUS);
        }

        if (status & _ST_DONE) {
            _bsc_reset();
            return 0;
        }
    }

    _bsc_reset();
    return -ETIMEDOUT;
}

/* =========================================================================
 * uk_i2c_read — polled read
 * ========================================================================= */

int uk_i2c_read(uint8_t addr7, uint8_t *buf, uint16_t len)
{
    uint32_t status;
    uint16_t rx_got = 0;
    int n;

    _bsc_reset();

    *_bsc(_BSC_SLAVE) = addr7;
    *_bsc(_BSC_DLEN)  = len;

    /* Start transfer — read direction */
    *_bsc(_BSC_CTRL) = _CTRL_I2CEN | _CTRL_ST | _CTRL_READ;

    for (n = BSC_POLL_LIMIT; n > 0; n--) {
        status = *_bsc(_BSC_STATUS);

        if (status & _ST_ERRBITS) {
            _bsc_reset();
            return (status & _ST_ERR) ? -EIO : -ETIMEDOUT;
        }

        /* Drain the FIFO as data arrives */
        while (rx_got < len && (status & _ST_RXD)) {
            buf[rx_got++] = (uint8_t)*_bsc(_BSC_DATA);
            status = *_bsc(_BSC_STATUS);
        }

        if ((status & _ST_DONE) && rx_got >= len) {
            _bsc_reset();
            return 0;
        }

        /* DONE set but FIFO not fully drained yet — keep draining */
        if (status & _ST_DONE) {
            while (rx_got < len && (status & _ST_RXD)) {
                buf[rx_got++] = (uint8_t)*_bsc(_BSC_DATA);
                status = *_bsc(_BSC_STATUS);
            }
            _bsc_reset();
            return (rx_got == len) ? 0 : -EIO;
        }
    }

    _bsc_reset();
    return -ETIMEDOUT;
}

/* =========================================================================
 * uk_i2c_write_read — write then repeated-START read (register-addressed)
 *
 * The BCM2837 BSC silicon bug described in the driver header (and the
 * linux/issues/254 problem report) requires a specific sequencing to
 * generate a repeated-START rather than a STOP between the write and read.
 * We replicate the workaround from bcm_bsc_transfer here:
 *   1. Set DLEN = wlen and start the write WITHOUT prefilling the FIFO
 *      (keeps xfer_count > 0 so the controller stays in TX state).
 *   2. Wait for the Transfer Active (TA) bit to confirm the hardware
 *      has latched the direction and length.
 *   3. Preset DLEN = rlen and CTRL = I2CEN | ST | READ | INT_ALL so the
 *      controller will issue a repeated-START after the write completes.
 *   4. Fill the TX FIFO with the write data; the controller finishes
 *      the write and immediately issues a repeated-START into the read.
 *   5. Poll for RX data and DONE.
 * ========================================================================= */

int uk_i2c_write_read(uint8_t addr7,
                      const uint8_t *wbuf, uint16_t wlen,
                      uint8_t *rbuf,       uint16_t rlen)
{
    uint32_t status;
    uint16_t tx_sent = 0, rx_got = 0;
    int n;

    _bsc_reset();

    *_bsc(_BSC_SLAVE) = addr7;

    /* Step 1: start write with empty FIFO (xfer_count stays > 0) */
    *_bsc(_BSC_DLEN) = wlen;
    *_bsc(_BSC_CTRL) = _CTRL_I2CEN | _CTRL_ST;   /* write, no INT, no data */

    /* Step 2: wait for TA — controller has latched DLEN and direction */
    for (n = BSC_POLL_LIMIT; n > 0; n--) {
        status = *_bsc(_BSC_STATUS);
        if (status & _ST_ERRBITS) {
            _bsc_reset();
            return (status & _ST_ERR) ? -EIO : -ETIMEDOUT;
        }
        if (status & _ST_TA)
            break;
    }
    if (n == 0) {
        _bsc_reset();
        return -ETIMEDOUT;
    }

    /* Step 3: preset read parameters — this triggers the repeated-START
     * after the write phase completes */
    *_bsc(_BSC_DLEN) = rlen;
    *_bsc(_BSC_CTRL) = _CTRL_I2CEN | _CTRL_ST | _CTRL_READ;

    /* Step 4: fill TX FIFO with write data */
    while (tx_sent < wlen) {
        status = *_bsc(_BSC_STATUS);
        if (status & _ST_ERRBITS) {
            _bsc_reset();
            return (status & _ST_ERR) ? -EIO : -ETIMEDOUT;
        }
        if (status & _ST_TXD)
            *_bsc(_BSC_DATA) = wbuf[tx_sent++];
    }

    /* Step 5: poll for RX data and DONE */
    for (n = BSC_POLL_LIMIT; n > 0; n--) {
        status = *_bsc(_BSC_STATUS);

        if (status & _ST_ERRBITS) {
            _bsc_reset();
            return (status & _ST_ERR) ? -EIO : -ETIMEDOUT;
        }

        while (rx_got < rlen && (status & _ST_RXD)) {
            rbuf[rx_got++] = (uint8_t)*_bsc(_BSC_DATA);
            status = *_bsc(_BSC_STATUS);
        }

        if (status & _ST_DONE) {
            /* Drain any remaining bytes */
            while (rx_got < rlen && (status & _ST_RXD)) {
                rbuf[rx_got++] = (uint8_t)*_bsc(_BSC_DATA);
                status = *_bsc(_BSC_STATUS);
            }
            _bsc_reset();
            return (rx_got == rlen) ? 0 : -EIO;
        }
    }

    _bsc_reset();
    return -ETIMEDOUT;
}
