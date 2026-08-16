/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * uk/uart.h — Public API for the BCM2837 PL011 UART0 driver.
 *
 * Targets UART0 (PL011) on GPIO 14 (TXD) / GPIO 15 (RXD).
 * These are the same pins used by the Unikraft serial console; this driver
 * must only be used when CONFIG_RASPI_PRINTF_SERIAL_CONSOLE,
 * CONFIG_RASPI_KERNEL_SERIAL_CONSOLE, and CONFIG_RASPI_DEBUG_SERIAL_CONSOLE
 * are all disabled — otherwise the two drivers will conflict.
 *
 * uk_gpio_init() must be called before uk_uart_init().
 */
#pragma once
#include <stdint.h>

/*
 * uk_uart_init — initialise the PL011 UART0 at the given baud rate.
 *
 * Sets the UART clock to 4 MHz via the VideoCore mailbox, configures
 * GPIO 14/15 as ALT0, programs the baud divisors, and enables TX+RX
 * in 8N1 mode with FIFO enabled.
 *
 * Supported baud rates: 9600, 19200, 38400, 57600, 115200.
 * Any other value silently falls back to 9600.
 *
 * Returns 0 on success.
 */
int uk_uart_init(uint32_t baud);

/*
 * uk_uart_putc — transmit one byte (blocks until TX FIFO has space).
 */
void uk_uart_putc(uint8_t c);

/*
 * uk_uart_getc — non-blocking receive.
 * Returns the received byte (0–255), or -1 if the RX FIFO is empty.
 */
int uk_uart_getc(void);

/*
 * uk_uart_write — transmit a buffer (blocks until all bytes are queued).
 */
void uk_uart_write(const uint8_t *buf, uint16_t len);

/*
 * uk_uart_read_timeout — receive up to max_len bytes.
 *
 * Reads bytes until max_len is reached or no new byte arrives within
 * timeout_ms milliseconds.  The buffer is NOT NUL-terminated.
 *
 * Returns the number of bytes actually read (0 on timeout with no data).
 */
uint16_t uk_uart_read_timeout(uint8_t *buf, uint16_t max_len,
                               uint32_t timeout_ms);
