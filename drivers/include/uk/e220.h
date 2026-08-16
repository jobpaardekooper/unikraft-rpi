/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * uk/e220.h — Public API for the EBYTE E220-400T30D LoRa module driver.
 *
 * The E220 is a UART-based LoRa module operating in the 410–493 MHz band.
 * It communicates with the host via UART (default 9600 8N1) and exposes
 * three control pins:
 *
 *   M0  → GPIO 17  (header pin 11)  output — mode select bit 0
 *   M1  → GPIO 16  (header pin 36)  output — mode select bit 1
 *   AUX → GPIO 13  (header pin 33)  input  — busy indicator (low = busy)
 *
 * Operating modes (M1:M0):
 *   0:0  Normal (transparent TX/RX) — used by send and recv
 *   0:1  WOR transmit
 *   1:0  WOR receive
 *   1:1  Deep sleep / configuration
 *
 * uk_gpio_init() and uk_uart_init(9600) must be called before
 * uk_e220_init().
 */
#pragma once
#include <stdint.h>

/*
 * uk_e220_init — configure M0/M1 as outputs, AUX as input with pull-up,
 *                enter normal mode, and wait for AUX=HIGH (ready).
 *
 * Returns 0 on success, -1 if AUX never went high within 2 seconds.
 */
int uk_e220_init(void);

/*
 * uk_e220_send — transmit data in normal (transparent) mode.
 *
 * Waits for AUX=HIGH before writing, writes all bytes to UART, then
 * waits for the module to finish the over-the-air transmission
 * (AUX goes LOW then HIGH).
 *
 * @data  buffer to transmit
 * @len   number of bytes
 *
 * Returns 0 on success, -1 on AUX timeout.
 */
int uk_e220_send(const uint8_t *data, uint16_t len);

/*
 * uk_e220_recv — receive data in normal (transparent) mode.
 *
 * Reads bytes from UART until @max_len bytes are received or no new byte
 * arrives within @idle_ms milliseconds (inter-byte idle timeout).
 *
 * @buf      output buffer
 * @max_len  maximum number of bytes to read
 * @idle_ms  idle timeout in ms (200 ms is a good default)
 *
 * Returns the number of bytes received (0 if nothing arrived before the
 * first idle timeout).
 */
uint16_t uk_e220_recv(uint8_t *buf, uint16_t max_len, uint32_t idle_ms);
