/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * measurements/lcd-shim/main.c — HD44780 20×4 LCD demo on Unikraft / RPi3.
 *
 * Wiring
 * ──────
 *  PCF8574 backpack → I2C1 (BSC1):
 *    SDA → GPIO 2 (header pin 3)
 *    SCL → GPIO 3 (header pin 5)
 *    GND → pin 6
 *    VCC → pin 2 (5 V — LCD backlight/logic; pull-ups to 3.3 V via 4.7 kΩ)
 *
 * The PCF8574 I2C address is typically 0x27 (PCF8574 with A0=A1=A2=1).
 * Adjust LCD_ADDR below if your backpack uses a different address.
 *
 * Display layout (20×4)
 * ─────────────────────
 * Page 1 (5 seconds):
 *   ┌────────────────────┐
 *   │    Powered by      │  row 0
 *   │o.   .o       _ _   │  row 1
 *   │Oo   Oo  ___ (_) |  │  row 2
 *   │oO   oO ' _ `| | |/ │  row 3
 *   └────────────────────┘
 *
 * Page 2 (permanent):
 *   ┌────────────────────┐
 *   │oOo oOO| | | | |   │  row 0
 *   │ OoOoO._,._:_:_,\  │  row 1
 *   │     Atlas 0.16.3   │  row 2
 *   │    Hello world!    │  row 3
 *   └────────────────────┘
 */

#include <stdio.h>
#include <stdint.h>

#include <uk/gpio.h>
#include <uk/i2c.h>
#include <uk/hd44780.h>
#include <uk/print.h>

/* PCF8574 I2C address — adjust if your backpack uses a different address */
#define LCD_ADDR  0x27

/* BCM2837 System Timer CLO (1 MHz free-running counter, 1 count = 1 μs) */
#define SYSTIMER_CLO  ((volatile uint32_t *)0x3F003004UL)

static void delay_ms(uint32_t ms)
{
    uint32_t start = *SYSTIMER_CLO;
    while ((*SYSTIMER_CLO - start) < ms * 1000U)
        ;
}

int main(void)
{
    int rc;

    /* ── GPIO init ─────────────────────────────────────────────────────── */
    rc = uk_gpio_init();
    if (rc) {
        uk_pr_err("uk_gpio_init() failed: %d\n", rc);
        return rc;
    }

    /* Set GPIO 2 (SDA) and GPIO 3 (SCL) to ALT0 (I2C1 / BSC1) */
    uk_gpio_set_func(2, UK_GPIO_FUNC_ALT0);
    uk_gpio_set_func(3, UK_GPIO_FUNC_ALT0);

    /* ── I2C init ──────────────────────────────────────────────────────── */
    rc = uk_i2c_init();
    if (rc) {
        uk_pr_err("uk_i2c_init() failed: %d\n", rc);
        return rc;
    }

    /* ── LCD init ──────────────────────────────────────────────────────── */
    rc = uk_lcd_init(LCD_ADDR, 20, 4);
    if (rc) {
        uk_pr_err("uk_lcd_init() failed: %d\n", rc);
        return rc;
    }

    /* ── Page 1: top half of the Unikraft ASCII logo (5 seconds) ──────── */
    uk_lcd_puts_row(0, "    Powered by");
    uk_lcd_puts_row(1, "o.   .o       _ _");
    uk_lcd_puts_row(2, "Oo   Oo  ___ (_) |");
    uk_lcd_puts_row(3, "oO   oO ' _ `| | |/");

    delay_ms(5000);

    /* ── Page 2: bottom half + version + greeting (stays forever) ─────── */
    uk_lcd_puts_row(0, "oOo oOO| | | | |");
    uk_lcd_puts_row(1, " OoOoO._,._:_:_,\\");
    uk_lcd_puts_row(2, "     Telesto 0.16.3");
    uk_lcd_puts_row(3, "    Hello world!");

    /* Also print to serial so we can confirm execution without the LCD */
    printf("\nLCD demo complete.\n");
    printf("Page 2 is now shown on the display.\n");

    return 0;
}
