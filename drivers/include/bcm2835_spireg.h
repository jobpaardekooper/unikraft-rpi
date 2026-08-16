/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * BCM2835 SPI0 register definitions.
 *
 * Derived from FreeBSD sys/arm/broadcom/bcm2835/bcm2835_spireg.h
 * Original authors:
 *   Oleksandr Tymoshenko <gonzo@freebsd.org>
 *   Luiz Otavio O Souza <loos@freebsd.org>
 *
 * Ported to Unikraft / Raspberry Pi 3 bare-metal by Mihnea Firoiu.
 */

#ifndef __BCM2835_SPIREG_H__
#define __BCM2835_SPIREG_H__

/* Peripheral base address for RPi 3 (BCM2837) */
#define BCM2835_PERIPHERAL_BASE     0x3F000000UL

/* SPI0 controller base offset from peripheral base */
#define BCM2835_SPI0_OFFSET         0x00204000UL
#define BCM2835_SPI0_BASE           (BCM2835_PERIPHERAL_BASE + BCM2835_SPI0_OFFSET)

/* GPIO base offset (needed to configure pin mux) */
#define BCM2835_GPIO_OFFSET         0x00200000UL
#define BCM2835_GPIO_BASE           (BCM2835_PERIPHERAL_BASE + BCM2835_GPIO_OFFSET)

/* ---- SPI0 register offsets ---- */
#define SPI_CS      0x00    /* Control and Status */
#define SPI_FIFO    0x04    /* TX/RX FIFO */
#define SPI_CLK     0x08    /* Clock divider */
#define SPI_DLEN    0x0C    /* Data length (DMA mode) */
#define SPI_LTOH    0x10    /* LoSSI TOH */
#define SPI_DC      0x14    /* DMA DREQ controls */

/* ---- SPI_CS bit definitions ---- */
#define SPI_CS_CS_MASK      0x00000003  /* Chip-select field */
#define SPI_CS_CPHA         0x00000004  /* Clock phase */
#define SPI_CS_CPOL         0x00000008  /* Clock polarity */
#define SPI_CS_CLEAR_TXFIFO 0x00000010  /* Clear TX FIFO */
#define SPI_CS_CLEAR_RXFIFO 0x00000020  /* Clear RX FIFO */
#define SPI_CS_CSPOL        0x00000040  /* Chip-select polarity */
#define SPI_CS_TA           0x00000080  /* Transfer Active */
#define SPI_CS_DMAEN        0x00000100  /* DMA enable */
#define SPI_CS_INTD         0x00000200  /* Interrupt on DONE */
#define SPI_CS_INTR         0x00000400  /* Interrupt on RXR */
#define SPI_CS_ADCS         0x00000800  /* Auto-deassert CS */
#define SPI_CS_REN          0x00001000  /* Read enable (LoSSI) */
#define SPI_CS_LEN          0x00002000  /* LoSSI enable */
#define SPI_CS_DONE         0x00010000  /* Transfer done */
#define SPI_CS_RXD          0x00020000  /* RX FIFO has data */
#define SPI_CS_TXD          0x00040000  /* TX FIFO can accept data */
#define SPI_CS_RXR          0x00080000  /* RX FIFO needs reading */
#define SPI_CS_RXF          0x00100000  /* RX FIFO full */
#define SPI_CS_CSPOL0       0x00200000  /* CS0 polarity */
#define SPI_CS_CSPOL1       0x00400000  /* CS1 polarity */
#define SPI_CS_CSPOL2       0x00800000  /* CS2 polarity */

/* Mask of all bits that control the transfer (used for rmw) */
#define SPI_CS_MASK  (SPI_CS_CS_MASK | SPI_CS_CPHA | SPI_CS_CPOL | \
                      SPI_CS_CSPOL  | SPI_CS_TA    | SPI_CS_DMAEN | \
                      SPI_CS_INTD   | SPI_CS_INTR  | SPI_CS_ADCS  | \
                      SPI_CS_REN    | SPI_CS_LEN   | SPI_CS_CSPOL0| \
                      SPI_CS_CSPOL1 | SPI_CS_CSPOL2)

/* ---- SPI_CLK bit definitions ---- */
#define SPI_CLK_MASK    0x0000FFFF

/* Core clock on RPi 3 is 250 MHz */
#define SPI_CORE_CLK    250000000U

/* ---- GPIO alternate function ALT0 = 0x4 ---- */
/*
 * SPI0 pins (BCM GPIO numbering):
 *   GPIO 7  = CE1  (FSEL0, field 7)   -> needs ALT0
 *   GPIO 8  = CE0  (FSEL0, field 8)   -> needs ALT0
 *   GPIO 9  = MISO (FSEL0, field 9)   -> needs ALT0
 *   GPIO 10 = MOSI (FSEL1, field 0)   -> needs ALT0
 *   GPIO 11 = SCLK (FSEL1, field 1)   -> needs ALT0
 *
 * GPFSEL registers: each register holds 10 3-bit fields.
 * GPFSEL0 covers GPIO 0-9, GPFSEL1 covers GPIO 10-19.
 * ALT0 = 0b100 = 4
 */
#define GPIO_FSEL_ALT0  4U

/* GPIO pull-up/down registers */
#define GPIO_GPPUD      0x94
#define GPIO_GPPUDCLK0  0x98

#endif /* __BCM2835_SPIREG_H__ */
