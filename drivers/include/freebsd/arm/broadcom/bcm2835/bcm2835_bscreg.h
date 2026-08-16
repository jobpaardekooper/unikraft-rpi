/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm2835_bscreg.h — BCM2835/6/7 BSC (I2C) register map.
 *
 * Register offsets from the BCM2835 ARM Peripherals datasheet §3.
 * BCM2835 = BCM2836 = BCM2837 — same peripheral map, only MMIO base differs.
 *
 * RPi 3 (BCM2837) peripheral base: 0x3F000000
 *   BSC0  (I2C0, GPIO 0/1):  0x3F205000  — camera / display, avoid
 *   BSC1  (I2C1, GPIO 2/3):  0x3F804000  — user bus (header pins 3/5)
 *   BSC2  (internal HDMI):   0x3F805000  — avoid
 */
#ifndef _BCM2835_BSCREG_H_
#define _BCM2835_BSCREG_H_

/* Register offsets -------------------------------------------------------- */
#define BCM_BSC_CTRL        0x00    /* Control */
#define BCM_BSC_STATUS      0x04    /* Status */
#define BCM_BSC_DLEN        0x08    /* Data Length */
#define BCM_BSC_SLAVE       0x0C    /* Slave Address */
#define BCM_BSC_DATA        0x10    /* Data FIFO */
#define BCM_BSC_CLOCK       0x14    /* Clock Divider */
#define BCM_BSC_DELAY       0x18    /* Data Delay */
#define BCM_BSC_CLKT        0x1C    /* Clock Stretch Timeout */

/* Control register (BCM_BSC_CTRL) bits ------------------------------------ */
#define BCM_BSC_CTRL_I2CEN  (1u << 15) /* I2C enable */
#define BCM_BSC_CTRL_INTR   (1u << 10) /* interrupt on RX */
#define BCM_BSC_CTRL_INTT   (1u <<  9) /* interrupt on TX */
#define BCM_BSC_CTRL_INTD   (1u <<  8) /* interrupt on DONE */
#define BCM_BSC_CTRL_ST     (1u <<  7) /* start transfer */
#define BCM_BSC_CTRL_CLEAR1 (1u <<  5) /* clear FIFO (write 1) */
#define BCM_BSC_CTRL_CLEAR0 (1u <<  4) /* clear FIFO (write 1) */
#define BCM_BSC_CTRL_READ   (1u <<  0) /* 1 = read, 0 = write */
#define BCM_BSC_CTRL_INT_ALL \
    (BCM_BSC_CTRL_INTR | BCM_BSC_CTRL_INTT | BCM_BSC_CTRL_INTD)

/* Status register (BCM_BSC_STATUS) bits ----------------------------------- */
#define BCM_BSC_STATUS_CLKT (1u <<  9) /* clock stretch timeout */
#define BCM_BSC_STATUS_ERR  (1u <<  8) /* ACK error (NACK received) */
#define BCM_BSC_STATUS_RXF  (1u <<  7) /* FIFO full */
#define BCM_BSC_STATUS_TXE  (1u <<  6) /* FIFO empty */
#define BCM_BSC_STATUS_RXD  (1u <<  5) /* FIFO contains data */
#define BCM_BSC_STATUS_TXD  (1u <<  4) /* FIFO has space */
#define BCM_BSC_STATUS_RXR  (1u <<  3) /* FIFO needs reading (3/4 full) */
#define BCM_BSC_STATUS_TXW  (1u <<  2) /* FIFO needs writing (1/4 full) */
#define BCM_BSC_STATUS_DONE (1u <<  1) /* transfer complete */
#define BCM_BSC_STATUS_TA   (1u <<  0) /* transfer active */

/* Bits cleared on write to acknowledge */
#define BCM_BSC_STATUS_CLRBITS \
    (BCM_BSC_STATUS_CLKT | BCM_BSC_STATUS_ERR | BCM_BSC_STATUS_DONE)

/* Error bits */
#define BCM_BSC_STATUS_ERRBITS \
    (BCM_BSC_STATUS_ERR | BCM_BSC_STATUS_CLKT)

/* Physical base addresses on RPi 3 (BCM2837) ------------------------------ */
#define BCM_BSC1_PBASE      0x3F804000UL

/* Core clock feeding the BSC divider (250 MHz typ., 150 MHz min.) --------- */
#define BCM_BSC_CORE_CLK    150000000U

/* MMIO accessors (bus_space_read/write_4 defined in machine/bus.h shim) --- */
#define BCM_BSC_READ(sc, reg) \
    bus_space_read_4((sc)->sc_bst, (sc)->sc_bsh, (reg))
#define BCM_BSC_WRITE(sc, reg, val) \
    bus_space_write_4((sc)->sc_bst, (sc)->sc_bsh, (reg), (val))

#endif /* _BCM2835_BSCREG_H_ */
