/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * uk/hd44780.h — Public API for the HD44780 character LCD driver.
 *
 * Targets HD44780-compatible displays (20×4, 16×2, …) connected via a
 * PCF8574 I2C I/O expander backpack.  uk_gpio_init(), GPIO 2/3 set to
 * ALT0, and uk_i2c_init() must be called before uk_lcd_init().
 *
 * Typical PCF8574 I2C addresses:
 *   0x27  — PCF8574  (A2=A1=A0=1)
 *   0x3F  — PCF8574A (A2=A1=A0=1)
 * Check the A0/A1/A2 solder jumpers on the backpack board.
 */
#pragma once
#include <stdint.h>
#include <stdarg.h>
/*
 * uk_lcd_init — initialise the LCD controller.
 *
 *   addr7  7-bit I2C address of the PCF8574 backpack (e.g. 0x27)
 *   cols   display width  in characters (0 → default 20)
 *   rows   display height in rows       (0 → default  4)
 *
 * Returns 0 (errors from the PCF8574 are silent; a missing device will
 * produce garbage on the display but won't crash the kernel).
 */
int  uk_lcd_init(uint8_t addr7, uint8_t cols, uint8_t rows);

/* Clear the display and return the cursor to home (0, 0). */
void uk_lcd_clear(void);

/* Return the cursor to home (0, 0) without clearing. */
void uk_lcd_home(void);

/*
 * uk_lcd_set_cursor — position the write cursor.
 *   row  0-based row    (0 = top)
 *   col  0-based column (0 = left)
 */
void uk_lcd_set_cursor(uint8_t row, uint8_t col);

/* Write a single character at the current cursor position. */
void uk_lcd_putc(char c);

/* Write a NUL-terminated string starting at the current cursor position. */
void uk_lcd_print(const char *s);

/*
 * uk_lcd_puts_row — write a string to an entire row.
 *
 * Sets the cursor to (row, 0), writes the string, then pads the rest of
 * the row with spaces so any previously displayed content is erased.
 * The string is silently truncated if it exceeds the display width.
 */
void uk_lcd_puts_row(uint8_t row, const char *s);

void uk_lcd_printf(uint8_t row, uint8_t col, const char *fmt, ...);

/* Turn the backlight on (on != 0) or off (on == 0). */
void uk_lcd_backlight(int on);
