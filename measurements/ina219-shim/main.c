/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * measurements/ina219-shim/main.c
 *
 * Reads bus voltage, shunt voltage, current and power from an INA219
 * current/power monitor and displays the live values on a 20×4 HD44780
 * LCD, refreshing once per second.
 *
 * Wiring
 * ──────
 *  INA219 + PCF8574-backed LCD both connected to I2C1 (BSC1):
 *    SDA  → GPIO 2 (header pin 3)
 *    SCL  → GPIO 3 (header pin 5)
 *    GND  → pin 6
 *    VCC  → pin 1 (3.3 V logic) or pin 2 (5 V for LCD backlight)
 *
 * Default I2C addresses:
 *   INA219  : 0x40  (A0=A1=GND — both address pins to ground)
 *   PCF8574 : 0x27  (A0=A1=A2=1  — most common LCD backpack)
 *
 * Shunt resistor: 0.1 Ω (100 mΩ) — standard INA219 breakout board.
 *
 * Display layout (20×4)
 * ─────────────────────
 *   ┌────────────────────┐
 *   │  INA219  Monitor   │  row 0  (static header)
 *   │ Bus:   XX.XXX  V   │  row 1  (bus voltage)
 *   │ Curr: XXX.X   mA   │  row 2  (current, signed)
 *   │ Pwr:  XXXX    mW   │  row 3  (power)
 *   └────────────────────┘
 */

#include <stdio.h>
#include <stdint.h>
#include <limits.h>

#include <uk/gpio.h>
#include <uk/i2c.h>
#include <uk/hd44780.h>
#include <uk/ina219.h>
#include <uk/print.h>

/* I2C addresses — adjust if your boards use different addresses */
#define LCD_ADDR    0x27u   /* PCF8574 backpack (A2=A1=A0=1) */
#define INA219_ADDR 0x40u   /* INA219 (A0=A1=GND)            */

/* Shunt resistor in milliohms (100 mΩ = 0.1 Ω, typical breakout board) */
#define SHUNT_MOHM  100u

/* BCM2837 System Timer CLO — 1 MHz free-running counter (1 count = 1 µs) */
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

    /* ── INA219 init ───────────────────────────────────────────────────── */
    rc = uk_ina219_init(INA219_ADDR, SHUNT_MOHM);
    if (rc) {
        uk_pr_err("uk_ina219_init() failed: %d\n", rc);
        uk_lcd_puts_row(0, "  INA219  Monitor");
        uk_lcd_puts_row(1, " Init failed!");
        uk_lcd_puts_row(2, " Check wiring");
        uk_lcd_puts_row(3, " addr=0x40?");
        return rc;
    }

    /* ── Static header row ─────────────────────────────────────────────── */
    uk_lcd_puts_row(0, "  INA219  Monitor");

    printf("INA219 monitor running. Reading every 1 s...\n");

    /* ── Measurement loop ──────────────────────────────────────────────── */
    while (1) {
        int32_t  bus_mv     = uk_ina219_read_bus_mv();
        int32_t  shunt_uv   = uk_ina219_read_shunt_uv();
        int32_t  current_ma = uk_ina219_read_current_ma();
        uint32_t power_mw   = uk_ina219_read_power_mw();

        /* ── Serial output ────────────────────────────────────────────── */
        if (bus_mv != -1 && current_ma != INT32_MIN && power_mw != UINT32_MAX) {
            printf("Bus: %d.%03d V  Shunt: %+d uV  "
                   "Current: %+d mA  Power: %u mW\n",
                   bus_mv / 1000, (bus_mv < 0 ? -bus_mv : bus_mv) % 1000,
                   shunt_uv,
                   current_ma,
                   power_mw);
        } else {
            printf("INA219: read error\n");
        }

        /* ── LCD update ────────────────────────────────────────────────── */
        if (bus_mv == -1) {
            uk_lcd_puts_row(1, " Bus:  read error");
        } else {
            /* Format: " Bus:  XX.XXX V" — always show 3 decimal places */
            int32_t abs_mv  = bus_mv < 0 ? -bus_mv : bus_mv;
            int32_t v_whole = abs_mv / 1000;
            int32_t v_frac  = abs_mv % 1000;
            uk_lcd_printf(1, 0, " Bus:  %s%d.%03d V",
                          bus_mv < 0 ? "-" : "", v_whole, v_frac);
        }

        if (current_ma == INT32_MIN) {
            uk_lcd_puts_row(2, " Curr: read error");
        } else {
            int32_t abs_ma = current_ma < 0 ? -current_ma : current_ma;
            uk_lcd_printf(2, 0, " Curr: %s%d mA",
                          current_ma < 0 ? "-" : "",
                          abs_ma);
        }

        if (power_mw == UINT32_MAX) {
            uk_lcd_puts_row(3, " Pwr:  read error");
        } else {
            uk_lcd_printf(3, 0, " Pwr:  %u mW", power_mw);
        }

        delay_ms(1000);
    }

    /* Unreachable — keeps the compiler happy. */
    return 0;
}
