/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm_hd44780.c — HD44780 character LCD driver for Unikraft / RPi3.
 *
 * This is a native Unikraft module — no FreeBSD shim layer is involved.
 * It sits directly on top of uk_i2c_write() from the BCM2835 I2C shim.
 *
 * Target hardware
 * ───────────────
 * Any HD44780-compatible character LCD (20×4, 16×2, …) connected via a
 * PCF8574 I2C I/O expander backpack.
 *
 * PCF8574 bit → LCD signal mapping (most common backpack boards):
 *
 *   Bit 7 (P7) → D7  ┐
 *   Bit 6 (P6) → D6  │  4-bit data bus (upper nibble only in 4-bit mode)
 *   Bit 5 (P5) → D5  │
 *   Bit 4 (P4) → D4  ┘
 *   Bit 3 (P3) → Backlight enable  (1 = on)
 *   Bit 2 (P2) → E   Enable strobe (latches data on falling edge)
 *   Bit 1 (P1) → RW  Read/Write    (always 0 — write only)
 *   Bit 0 (P0) → RS  Register Sel  (0 = command, 1 = character data)
 *
 * Protocol
 * ────────
 * The HD44780 is driven in 4-bit mode.  Sending one logical byte requires
 * two nibble writes, each consisting of two I2C writes (E=1 then E=0) to
 * strobe the data into the controller.  One character → 4 I2C writes.
 *
 * Timing
 * ──────
 * Delays use the BCM2837 System Timer free-running 1 MHz counter at
 * physical address 0x3F003004 (CLO register).  This gives accurate μs
 * delays without depending on the Unikraft scheduler or a calibrated
 * busy-wait loop.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h> 

#include <uk/print.h>
#include <uk/i2c.h>
#include <uk/hd44780.h>

/* =========================================================================
 * PCF8574 control bit masks
 * ========================================================================= */

#define LCD_RS  0x01u   /* Register Select: 0 = command, 1 = char data */
#define LCD_RW  0x02u   /* Read/Write: always driven low (write)        */
#define LCD_EN  0x04u   /* Enable strobe — data latched on falling edge */
#define LCD_BL  0x08u   /* Backlight: 1 = on                            */
/* Data nibble occupies bits [7:4] of the byte sent to the PCF8574.     */

/* =========================================================================
 * HD44780 DDRAM row start addresses
 *
 * Valid for 16×2, 20×2, 20×4, and 40×2 displays.
 * A 20×4 display uses rows 0/1 (first physical line pair) and
 * rows 2/3 starting at 0x14/0x54 (second physical line pair).
 * ========================================================================= */

static const uint8_t _row_addr[4] = { 0x00u, 0x40u, 0x14u, 0x54u };

/* =========================================================================
 * Driver state
 * ========================================================================= */

static uint8_t _i2c_addr;
static uint8_t _bl   = LCD_BL;   /* backlight byte OR'd into every write */
static uint8_t _cols = 20;
static uint8_t _rows = 4;

/* =========================================================================
 * Timing — BCM2837 System Timer CLO (1 MHz, counts microseconds)
 * ========================================================================= */

#define SYSTIMER_CLO  ((volatile uint32_t *)0x3F003004UL)

static void _delay_us(uint32_t us)
{
    uint32_t start = *SYSTIMER_CLO;
    while ((*SYSTIMER_CLO - start) < us)
        ;
}

/* =========================================================================
 * Low-level PCF8574 → HD44780 helpers
 * ========================================================================= */

/* Write one raw byte to the PCF8574. */
static void _pcf_write(uint8_t byte)
{
    uk_i2c_write(_i2c_addr, &byte, 1);
}

/*
 * _pulse_enable — toggle the E pin high then low to latch a nibble.
 * `data` already contains the nibble in bits [7:4] plus RS/BL.
 */
static void _pulse_enable(uint8_t data)
{
    _pcf_write(data | LCD_EN);
    _delay_us(1);               /* E pulse width ≥ 230 ns              */
    _pcf_write(data & ~LCD_EN);
    _delay_us(50);              /* settling time after falling edge     */
}

/*
 * _write_nibble — send one nibble; the nibble must be in bits [7:4].
 * flags: OR combination of LCD_RS / LCD_RW.
 */
static void _write_nibble(uint8_t nibble, uint8_t flags)
{
    _pulse_enable((nibble & 0xF0u) | flags | _bl);
}

/* _write_byte — send a full byte as two nibbles (high nibble first). */
static void _write_byte(uint8_t byte, uint8_t flags)
{
    _write_nibble(byte,        flags);   /* bits [7:4] */
    _write_nibble(byte << 4u, flags);   /* bits [3:0] shifted up */
}

/* Send an HD44780 command byte and wait for execution. */
static void _cmd(uint8_t cmd)
{
    _write_byte(cmd, 0);
    _delay_us(1600);   /* worst-case execution time: 1.53 ms */
}

/* Send a character data byte. */
static void _dat(uint8_t data)
{
    _write_byte(data, LCD_RS);
    _delay_us(50);             /* data write time: 43 μs max */
}

/* =========================================================================
 * Public API
 * ========================================================================= */

int uk_lcd_init(uint8_t addr7, uint8_t cols, uint8_t rows)
{
    _i2c_addr = addr7;
    _cols     = cols ? cols : 20u;
    _rows     = rows ? rows : 4u;
    _bl       = LCD_BL;

    /* Allow the LCD power supply to stabilise (spec: ≥ 40 ms). */
    _delay_us(50000);

    /* Assert backlight before the init sequence so the display is lit. */
    _pcf_write(_bl);
    _delay_us(1000);

    /*
     * HD44780 power-on initialisation in 4-bit mode
     * (datasheet Figure 24, "Initializing by Instruction"):
     *
     * Step 1–3: Send 0x30 three times in 8-bit form (only the high nibble
     *           matters during the reset sequence).  This unconditionally
     *           resets the controller regardless of its current state.
     */
    _write_nibble(0x30u, 0); _delay_us(4500);
    _write_nibble(0x30u, 0); _delay_us(4500);
    _write_nibble(0x30u, 0); _delay_us(200);

    /* Step 4: Switch to 4-bit bus width. */
    _write_nibble(0x20u, 0); _delay_us(200);

    /* From here on every write is a proper 4-bit two-nibble transaction. */
    _cmd(0x28u);   /* Function set: 4-bit, 2-line display, 5×8 dots     */
    _cmd(0x08u);   /* Display off                                        */
    _cmd(0x01u);   /* Display clear (needs ≥ 1.53 ms — _cmd covers it)  */
    _delay_us(2000);
    _cmd(0x06u);   /* Entry mode: cursor right, no display shift         */
    _cmd(0x0Cu);   /* Display on, cursor off, blink off                  */

    uk_pr_info("hd44780: %ux%u LCD ready at I2C 0x%02x\n",
               _cols, _rows, addr7);
    return 0;
}

void uk_lcd_clear(void)
{
    _cmd(0x01u);
    _delay_us(2000);
}

void uk_lcd_home(void)
{
    _cmd(0x02u);
    _delay_us(2000);
}

void uk_lcd_set_cursor(uint8_t row, uint8_t col)
{
    if (row >= _rows) row = _rows - 1;
    if (col >= _cols) col = _cols - 1;
    _cmd(0x80u | (_row_addr[row] + col));
}

void uk_lcd_putc(char c)
{
    _dat((uint8_t)c);
}

void uk_lcd_print(const char *s)
{
    while (*s)
        _dat((uint8_t)*s++);
}

void uk_lcd_puts_row(uint8_t row, const char *s)
{
    uint8_t col = 0;

    uk_lcd_set_cursor(row, 0);
    while (*s && col < _cols) {
        _dat((uint8_t)*s++);
        col++;
    }
    /* Pad the rest of the row with spaces to erase stale content. */
    while (col < _cols) {
        _dat(' ');
        col++;
    }
}

void uk_lcd_printf(uint8_t row, uint8_t col, const char *fmt, ...)
{
    char buf[21];   /* max one LCD row + NUL */
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    uk_lcd_set_cursor(row, col);
    uk_lcd_print(buf);
}

void uk_lcd_backlight(int on)
{
    _bl = on ? LCD_BL : 0u;
    _pcf_write(_bl);
}
