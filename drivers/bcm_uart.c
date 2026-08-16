/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm_uart.c — BCM2837 PL011 UART0 driver for Unikraft / RPi3.
 *
 * Native module.  Intended for use when the Unikraft serial console is
 * disabled (CONFIG_RASPI_PRINTF_SERIAL_CONSOLE=n etc.) so that UART0 is
 * free for application use (e.g. the EBYTE E220 LoRa module).
 *
 * Clock setup
 * ───────────
 * The PL011 clock (UART clock ID = 2) is set to exactly 4 MHz via the
 * VideoCore mailbox before the baud divisors are programmed.  This matches
 * the clock used by the Unikraft serial console and produces clean integer
 * divisors for all standard baud rates.
 *
 *   Baud    IBRD   FBRD   actual baud   error
 *   9600     26      3      9 598.5     0.015 %
 *   19200    13      1     19 193.5     0.034 %
 *   38400     6      9     38 402.7     0.007 %
 *   57600     4      5     57 604.2     0.007 %
 *   115200    2     11    115 107.9     0.080 %
 *
 * GPIO
 * ────
 * GPIO 14 → TXD0 (ALT0), pull: off
 * GPIO 15 → RXD0 (ALT0), pull: up (idle line = high)
 */

#include <stdint.h>
#include <stddef.h>

#include <uk/gpio.h>
#include <uk/uart.h>
#include <raspi/mbox.h>

/* =========================================================================
 * PL011 register map  (MMIO_BASE = 0x3F000000 on RPi3)
 * ========================================================================= */

#define UART0_BASE  0x3F201000UL
#define _R(off)     ((volatile uint32_t *)(UART0_BASE + (off)))

#define UART0_DR    _R(0x00)   /* Data register                  */
#define UART0_FR    _R(0x18)   /* Flag register                   */
#define UART0_IBRD  _R(0x24)   /* Integer  baud-rate divisor      */
#define UART0_FBRD  _R(0x28)   /* Fractional baud-rate divisor    */
#define UART0_LCRH  _R(0x2C)   /* Line control register           */
#define UART0_CR    _R(0x30)   /* Control register                */
#define UART0_IMSC  _R(0x38)   /* Interrupt mask set/clear        */
#define UART0_ICR   _R(0x44)   /* Interrupt clear register        */

/* FR bits */
#define FR_RXFE  (1u << 4)   /* RX FIFO empty */
#define FR_TXFF  (1u << 5)   /* TX FIFO full  */

/* LCRH bits */
#define LCRH_FEN    (1u << 4)   /* FIFO enable     */
#define LCRH_WLEN8  (3u << 5)   /* 8-bit word      */

/* CR bits */
#define CR_UARTEN (1u << 0)
#define CR_TXE    (1u << 8)
#define CR_RXE    (1u << 9)

/* =========================================================================
 * System timer (1 MHz, for timeouts)
 * ========================================================================= */

#define SYSTIMER_CLO  ((volatile uint32_t *)0x3F003004UL)

/* =========================================================================
 * Baud-rate divisor table  (UART clock = 4 000 000 Hz)
 * ========================================================================= */

struct _baud_entry { uint32_t baud; uint16_t ibrd; uint8_t fbrd; };

static const struct _baud_entry _baud_table[] = {
    {   9600, 26,  3 },
    {  19200, 13,  1 },
    {  38400,  6,  9 },
    {  57600,  4,  5 },
    { 115200,  2, 11 },
};
#define _BAUD_TABLE_LEN  (sizeof(_baud_table) / sizeof(_baud_table[0]))

/* =========================================================================
 * Public API
 * ========================================================================= */

int uk_uart_init(uint32_t baud)
{
    /* ── 1. Disable UART ────────────────────────────────────────────── */
    *UART0_CR = 0;

    /* ── 2. Set UART clock to 4 MHz via VideoCore mailbox ───────────── */
    mbox[0] = 9 * 4;
    mbox[1] = MBOX_REQUEST;
    mbox[2] = MBOX_TAG_SETCLKRATE;
    mbox[3] = 12;
    mbox[4] = 8;
    mbox[5] = 2;           /* clock ID 2 = UART clock */
    mbox[6] = 4000000;     /* 4 MHz                   */
    mbox[7] = 0;           /* no turbo                */
    mbox[8] = MBOX_TAG_LAST;
    mbox_call(MBOX_CH_PROP);

    /* ── 3. Configure GPIO 14/15 as ALT0 (UART0) ────────────────────── */
    uk_gpio_set_func(14, UK_GPIO_FUNC_ALT0);
    uk_gpio_set_func(15, UK_GPIO_FUNC_ALT0);
    uk_gpio_set_pud(14, UK_GPIO_PUD_OFF);
    uk_gpio_set_pud(15, UK_GPIO_PUD_UP);   /* keep RXD high when idle */

    /* ── 4. Clear all pending interrupts ───────────────────────────── */
    *UART0_ICR = 0x7FFu;
    *UART0_IMSC = 0;       /* mask all interrupts — we poll */

    /* ── 5. Program baud rate ───────────────────────────────────────── */
    {
        uint16_t ibrd = 26;   /* default: 9600 */
        uint8_t  fbrd = 3;
        for (size_t i = 0; i < _BAUD_TABLE_LEN; i++) {
            if (_baud_table[i].baud == baud) {
                ibrd = _baud_table[i].ibrd;
                fbrd = _baud_table[i].fbrd;
                break;
            }
        }
        *UART0_IBRD = ibrd;
        *UART0_FBRD = fbrd;
    }

    /* ── 6. 8N1 with FIFO ───────────────────────────────────────────── */
    *UART0_LCRH = LCRH_WLEN8 | LCRH_FEN;

    /* ── 7. Enable UART, TX, RX ─────────────────────────────────────── */
    *UART0_CR = CR_UARTEN | CR_TXE | CR_RXE;

    return 0;
}

void uk_uart_putc(uint8_t c)
{
    while (*UART0_FR & FR_TXFF)
        ;
    *UART0_DR = c;
}

int uk_uart_getc(void)
{
    if (*UART0_FR & FR_RXFE)
        return -1;
    return (int)(*UART0_DR & 0xFFu);
}

void uk_uart_write(const uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
        uk_uart_putc(buf[i]);
}

uint16_t uk_uart_read_timeout(uint8_t *buf, uint16_t max_len,
                               uint32_t timeout_ms)
{
    uint16_t n = 0;
    uint32_t deadline = *SYSTIMER_CLO + timeout_ms * 1000u;

    while (n < max_len) {
        int c = uk_uart_getc();
        if (c >= 0) {
            buf[n++] = (uint8_t)c;
            /* Reset deadline: wait timeout_ms after the *last* byte */
            deadline = *SYSTIMER_CLO + timeout_ms * 1000u;
        } else {
            if (*SYSTIMER_CLO >= deadline)
                break;
        }
    }
    return n;
}
