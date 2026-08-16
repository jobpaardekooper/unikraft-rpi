/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm_e220.c — EBYTE E220-400T30D LoRa module driver for Unikraft / RPi3.
 *
 * Native module.  Sits on top of uk_gpio_* (for M0/M1/AUX) and
 * uk_uart_* (for data).
 *
 * Pin map
 * ───────
 *   M0  = GPIO 17  (output) — mode select bit 0
 *   M1  = GPIO 16  (output) — mode select bit 1
 *   AUX = GPIO 13  (input)  — 1 = idle/ready, 0 = busy
 *
 * Normal (transparent) mode: M0=0, M1=0.
 * All send/recv operations use normal mode only.
 *
 * AUX timing
 * ──────────
 * After a mode change the module pulls AUX low for up to 2 ms before
 * asserting it high.  After writing the last UART byte the module pulls
 * AUX low while it is transmitting over the air, then asserts high again.
 * We wait for AUX=HIGH after each operation.
 */

#include <stdint.h>
#include <stddef.h>

#include <uk/gpio.h>
#include <uk/uart.h>
#include <uk/e220.h>

/* =========================================================================
 * Pin constants
 * ========================================================================= */

#define PIN_M0   17u
#define PIN_M1   16u
#define PIN_AUX  13u

/* =========================================================================
 * System timer for delays / timeouts
 * ========================================================================= */

#define SYSTIMER_CLO  ((volatile uint32_t *)0x3F003004UL)

static void _delay_ms(uint32_t ms)
{
    uint32_t start = *SYSTIMER_CLO;
    while ((*SYSTIMER_CLO - start) < ms * 1000u)
        ;
}

/* Wait for AUX to go HIGH.  Returns 0 on success, -1 on timeout. */
static int _wait_aux_high(uint32_t timeout_ms)
{
    uint32_t deadline = *SYSTIMER_CLO + timeout_ms * 1000u;
    while (!uk_gpio_get(PIN_AUX)) {
        if (*SYSTIMER_CLO >= deadline)
            return -1;
    }
    return 0;
}

/* =========================================================================
 * Public API
 * ========================================================================= */

int uk_e220_init(void)
{
    /* M0/M1: outputs, start low (normal mode) */
    uk_gpio_set_func(PIN_M0, UK_GPIO_FUNC_OUTPUT);
    uk_gpio_set_func(PIN_M1, UK_GPIO_FUNC_OUTPUT);
    uk_gpio_set(PIN_M0, 0);
    uk_gpio_set(PIN_M1, 0);

    /* AUX: input with pull-up */
    uk_gpio_set_func(PIN_AUX, UK_GPIO_FUNC_INPUT);
    uk_gpio_set_pud(PIN_AUX,  UK_GPIO_PUD_UP);

    /* Allow the module to settle after mode pins are driven */
    _delay_ms(10);

    /* Wait for AUX=HIGH (module ready) — up to 2 s */
    return _wait_aux_high(2000);
}

int uk_e220_send(const uint8_t *data, uint16_t len)
{
    /* Ensure normal mode */
    uk_gpio_set(PIN_M0, 0);
    uk_gpio_set(PIN_M1, 0);

    /* Wait for module to be idle */
    if (_wait_aux_high(500))
        return -1;

    /* Write payload */
    uk_uart_write(data, len);

    /*
     * The module needs ~2 ms to start processing the UART bytes before
     * AUX falls.  Polling immediately would see AUX still high and return
     * too early.
     */
    _delay_ms(5);

    /* Wait for transmission to complete (AUX low → high) */
    _wait_aux_high(5000);   /* worst case: long packet + slow air data rate */

    return 0;
}

uint16_t uk_e220_recv(uint8_t *buf, uint16_t max_len, uint32_t idle_ms)
{
    /* Ensure normal mode */
    uk_gpio_set(PIN_M0, 0);
    uk_gpio_set(PIN_M1, 0);

    return uk_uart_read_timeout(buf, max_len, idle_ms);
}
