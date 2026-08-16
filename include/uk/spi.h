/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Generic SPI API for Unikraft / Raspberry Pi platform.
 *
 * Applications should include this header, not the driver-specific one.
 * The underlying controller (BCM2835 SPI0) is wired in by the platform.
 */

#ifndef __UK_SPI_H__
#define __UK_SPI_H__

#include <uk/essentials.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Chip-select lines available on this platform.
 */
#define SPI_CS0  0
#define SPI_CS1  1

/**
 * A single full-duplex SPI transfer.
 *
 * Either pointer may be NULL:
 *   - tx_buf == NULL  -> send 0x00 bytes (read-only transfer)
 *   - rx_buf == NULL  -> discard received bytes (write-only transfer)
 */
struct spi_transfer {
	const __u8 *tx_buf; /**< Data to transmit (or NULL) */
	__u8       *rx_buf; /**< Buffer for received data (or NULL) */
	__sz        len;    /**< Number of bytes to transfer */
	int         cs;     /**< Chip select: SPI_CS0 or SPI_CS1 */
};

/**
 * spi_init - Initialise the platform SPI controller.
 *
 * Must be called once before any transfer.
 *
 * @return  0 on success, negative errno on error.
 */
int spi_init(void);

/**
 * spi_transfer - Perform a synchronous full-duplex SPI transfer.
 *
 * @param xfer  Pointer to a filled-in transfer descriptor.
 * @return      0 on success, -EINVAL if the descriptor is invalid,
 *              -EIO on hardware timeout.
 */
int spi_transfer(struct spi_transfer *xfer);

/**
 * spi_set_clock - Change the SPI clock frequency.
 *
 * @param hz  Desired clock in Hz.
 */
void spi_set_clock(__u32 hz);

#ifdef __cplusplus
}
#endif

#endif /* __RASPI_SPI_H__ */
