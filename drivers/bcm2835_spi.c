/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * BCM2835 SPI0 driver for Unikraft / Raspberry Pi 3.
 *
 * Derived from FreeBSD sys/arm/broadcom/bcm2835/bcm2835_spi.c
 * Original copyright:
 *   Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
 *   Copyright (c) 2013 Luiz Otavio O Souza <loos@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Ported to Unikraft bare-metal (polled mode) by Mihnea Firoiu.
 *
 * Key differences from the FreeBSD original
 * -----------------------------------------
 *  - FreeBSD uses bus_space_read/write macros backed by an FDT resource;
 *    here we use direct volatile 32-bit MMIO reads/writes to fixed
 *    physical addresses (identity-mapped on RPi 3 in Unikraft).
 *  - FreeBSD uses interrupt-driven transfers with mtx_sleep; here we
 *    use a simple polling loop (BCM2835 datasheet §10.6.1).  This keeps
 *    the driver free of scheduler / IRQ dependencies, making it safe to
 *    call early in platform bring-up.
 *  - FreeBSD uses the OFW bus layer (probe/attach/detach) and a child
 *    "spibus" device.  Here we expose a minimal init/transfer API that
 *    can later be wrapped in a Unikraft ukbus driver.
 *  - The sysctl knobs (clock, cpol, cpha, cspol0/1) are omitted; the
 *    clock can be changed via bcm2835_spi_set_clock().
 */

#include <errno.h>
#include <stddef.h>
#include <uk/print.h>
#include <uk/essentials.h>
#include <uk/spi.h>
#include <bcm2835_spireg.h>

/* --------------------------------------------------------------------
 * Low-level MMIO helpers
 * -------------------------------------------------------------------- */

/**
 * Read a 32-bit register at (base + offset).
 * The RPi 3 platform maps peripheral physical addresses 1:1 into the
 * virtual address space, so a bare pointer cast is sufficient.
 */
static inline __u32 spi_read(__u32 offset)
{
    return *(volatile __u32 *)(BCM2835_SPI0_BASE + offset);
}

/**
 * Write a 32-bit register at (base + offset).
 */
static inline void spi_write(__u32 offset, __u32 val)
{
    *(volatile __u32 *)(BCM2835_SPI0_BASE + offset) = val;
}

/**
 * Read-modify-write: clear the bits in @mask then set the bits in @val.
 * Mirrors bcm_spi_modifyreg() in the FreeBSD driver.
 */
static inline void spi_modifyreg(__u32 offset, __u32 mask, __u32 val)
{
    __u32 reg = spi_read(offset);
    reg &= ~mask;
    reg |= val;
    spi_write(offset, reg);
}

/* --------------------------------------------------------------------
 * GPIO helper – configure pins 7-11 to ALT0 for SPI0
 *
 * The BCM2835/7 GPFSEL registers each hold ten 3-bit fields:
 *   GPFSELn[3k+2 : 3k] = function for GPIO (10n + k)
 * ALT0 = 0b100 = 4.
 *
 * SPI0 pin assignment:
 *   GPIO  7 = /CE1   -> GPFSEL0, field 7  (bits 23:21)
 *   GPIO  8 = /CE0   -> GPFSEL0, field 8  (bits 26:24)
 *   GPIO  9 = MISO   -> GPFSEL0, field 9  (bits 29:27)
 *   GPIO 10 = MOSI   -> GPFSEL1, field 0  (bits  2:0 )
 *   GPIO 11 = SCLK   -> GPFSEL1, field 1  (bits  5:3 )
 * -------------------------------------------------------------------- */
static inline __u32 gpio_read(__u32 offset)
{
    return *(volatile __u32 *)(BCM2835_GPIO_BASE + offset);
}

static inline void gpio_write(__u32 offset, __u32 val)
{
    *(volatile __u32 *)(BCM2835_GPIO_BASE + offset) = val;
}

/**
 * Busy-wait for @n NOP cycles (used to satisfy GPIO timing requirements).
 */
static void gpio_delay(unsigned int n)
{
    while (n--)
        __asm__ volatile("nop");
}

/**
 * Configure SPI0 GPIO pins (7-11) to ALT0.
 * Mirrors bcm_gpio_set_alternate() calls in bcm_spi_attach().
 */
static void bcm2835_spi_gpio_init(void)
{
    __u32 reg;

    /*
     * GPFSEL0: set ALT0 for GPIO 7 (CE1), 8 (CE0), 9 (MISO).
     * Each field is 3 bits; bit position = gpio_number * 3.
     *
     * GPIO 7  -> bits 23:21
     * GPIO 8  -> bits 26:24
     * GPIO 9  -> bits 29:27
     */
    reg = gpio_read(0x00 /* GPFSEL0 */);
    reg &= ~((7u << 21) | (7u << 24) | (7u << 27));
    reg |=  ((GPIO_FSEL_ALT0 << 21) |
             (GPIO_FSEL_ALT0 << 24) |
             (GPIO_FSEL_ALT0 << 27));
    gpio_write(0x00, reg);

    /*
     * GPFSEL1: set ALT0 for GPIO 10 (MOSI), 11 (SCLK).
     * GPIO 10 -> bits 2:0  (field 0 of GPFSEL1)
     * GPIO 11 -> bits 5:3  (field 1 of GPFSEL1)
     */
    reg = gpio_read(0x04 /* GPFSEL1 */);
    reg &= ~((7u << 0) | (7u << 3));
    reg |=  ((GPIO_FSEL_ALT0 << 0) |
             (GPIO_FSEL_ALT0 << 3));
    gpio_write(0x04, reg);

    /*
     * Disable pull-up/down on SPI pins (they have their own termination).
     * BCM2835 datasheet §6.1 pull-up/down sequence:
     *   1. Write GPPUD = 0 (disable)
     *   2. Wait 150 cycles
     *   3. Write mask to GPPUDCLK0
     *   4. Wait 150 cycles
     *   5. Clear GPPUD and GPPUDCLK0
     */
    gpio_write(GPIO_GPPUD, 0);
    gpio_delay(150);
    /* Pins 7-11 = bits 7-11 in GPPUDCLK0 */
    gpio_write(GPIO_GPPUDCLK0, (1u << 7) | (1u << 8) | (1u << 9) |
                                (1u << 10) | (1u << 11));
    gpio_delay(150);
    gpio_write(GPIO_GPPUD, 0);
    gpio_write(GPIO_GPPUDCLK0, 0);
}

/* --------------------------------------------------------------------
 * Public driver API
 * -------------------------------------------------------------------- */

int spi_init(void)
{
    /* Configure GPIO pins 7-11 for SPI0 ALT0 function */
    bcm2835_spi_gpio_init();

    /*
     * Enable the SPI controller: clear both FIFOs.
     * This corresponds to the lines in bcm_spi_attach() just before
     * device_add_child().  We start in SPI mode 0 (CPOL=0, CPHA=0).
     */
    spi_write(SPI_CS, SPI_CS_CLEAR_TXFIFO | SPI_CS_CLEAR_RXFIFO);

    /* Default clock: 500 kHz (same as FreeBSD driver) */
    spi_write(SPI_CLK, SPI_CORE_CLK / 500000);

    uk_pr_info("bcm2835_spi: SPI0 initialised at 500 kHz\n");
    return 0;
}

void spi_set_clock(__u32 hz)
{
    __u32 divider;

    if (hz == 0) {
        uk_pr_err("bcm2835_spi: invalid clock frequency 0\n");
        return;
    }

    /*
     * The clock divider must be an even number (odd values are rounded
     * down by the hardware).  A value of 0 means 65536.
     * Match the rounding from the FreeBSD sysctl handler.
     */
    divider = SPI_CORE_CLK / hz;
    if (divider <= 1)
        divider = 2;
    else if (divider & 1)
        divider--;          /* round down to even */
    if (divider > 0xFFFF)
        divider = 0;        /* 0 => 65536 */

    spi_write(SPI_CLK, divider & SPI_CLK_MASK);

    uk_pr_info("bcm2835_spi: clock set to ~%u Hz (divider=%u)\n",
               divider ? (SPI_CORE_CLK / divider) : (SPI_CORE_CLK / 65536),
               divider);
}

/**
 * bcm2835_spi_transfer - polled full-duplex SPI transfer.
 *
 * Algorithm (BCM2835 datasheet §10.6.1 – polled mode):
 *   a) Set CS, CPOL, CPHA as required; set TA = 1.
 *   b) Poll TXD, writing bytes to SPI_FIFO; poll RXD reading bytes back
 *      until all bytes are transferred.
 *   c) Poll DONE until it goes to 1.
 *   d) Set TA = 0.
 *
 * The FreeBSD driver uses interrupt-driven transfers.  We use polled mode
 * to avoid a dependency on Unikraft's IRQ or scheduler subsystems, making
 * the driver safe to use during early platform init.
 */
int spi_transfer(struct spi_transfer *xfer)
{
    __sz written = 0;
    __sz read    = 0;
    __u32 cs_reg;
    __u32 timeout;

    if (!xfer || xfer->len == 0)
        return -EINVAL;

    if (xfer->cs < 0 || xfer->cs > 1) {
        uk_pr_err("bcm2835_spi: invalid chip select %d\n", xfer->cs);
        return -EINVAL;
    }

    /* Step a: clear FIFOs, set chip select, assert TA */
    spi_modifyreg(SPI_CS,
                  SPI_CS_CLEAR_TXFIFO | SPI_CS_CLEAR_RXFIFO,
                  SPI_CS_CLEAR_TXFIFO | SPI_CS_CLEAR_RXFIFO);

    /*
     * Set the chip-select field and assert Transfer Active (TA).
     * We preserve the CPOL/CPHA/CSPOL bits already in the register so
     * that mode configuration set by the caller survives across calls.
     */
    cs_reg = spi_read(SPI_CS);
    cs_reg &= ~(SPI_CS_CS_MASK | SPI_CS_TA);
    cs_reg |= ((__u32)xfer->cs & SPI_CS_CS_MASK) | SPI_CS_TA;
    spi_write(SPI_CS, cs_reg);

    /*
     * Step b: feed the TX FIFO while draining the RX FIFO until we
     * have transferred all bytes.
     *
     * The FIFO is 16 bytes deep on each side.  We interleave writes
     * and reads to prevent either FIFO from overflowing / underflowing.
     */
    while (read < xfer->len) {
        /* Write to TX FIFO if space is available and we still have data */
        if (written < xfer->len &&
            (spi_read(SPI_CS) & SPI_CS_TXD)) {
            __u8 byte = xfer->tx_buf ? xfer->tx_buf[written] : 0x00;
            spi_write(SPI_FIFO, byte);
            written++;
        }

        /* Read from RX FIFO if data is available */
        if (spi_read(SPI_CS) & SPI_CS_RXD) {
            __u8 byte = (__u8)(spi_read(SPI_FIFO) & 0xFF);
            if (xfer->rx_buf)
                xfer->rx_buf[read] = byte;
            read++;
        }
    }

    /*
     * Step c: wait for DONE.
     *
     * DONE is set once the last bit has been clocked out.  In normal
     * operation this happens very quickly after the last byte leaves
     * the TX FIFO, but we add a timeout to avoid hanging forever if
     * the hardware misbehaves.  The timeout is generous: at the
     * minimum supported clock (500 kHz) transferring 64 KB would take
     * ~1 second; 10 000 000 iterations of a tight loop is comfortably
     * longer than that on a 1.2 GHz Cortex-A53.
     */
    timeout = 10000000U;
    while (!(spi_read(SPI_CS) & SPI_CS_DONE)) {
        if (--timeout == 0) {
            uk_pr_err("bcm2835_spi: transfer timed out\n");
            /* Step d (cleanup): deassert TA even on error */
            spi_modifyreg(SPI_CS, SPI_CS_TA, 0);
            return -EIO;
        }
    }

    /* Step d: deassert Transfer Active */
    spi_modifyreg(SPI_CS, SPI_CS_TA, 0);

    return 0;
}
