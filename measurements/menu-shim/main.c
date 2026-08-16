/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * measurements/menu-shim/main.c
 *
 * Interactive LCD menu controlled by two rotary encoders.
 *
 * Controls
 * ────────
 *   ENC2 rotate left   — cursor up
 *   ENC2 rotate right  — cursor down
 *   ENC2 button click  — select / confirm
 *   ENC1 button click  — back / cancel
 *
 * Menu items
 * ──────────
 *   Check Consumption  — live INA219 readings (bus V, current, power)
 *   Receive Data       — (placeholder)
 *   Transmit Data      — (placeholder)
 *   Read File Contents — (placeholder)
 *   About              — firmware info
 *   Help               — control reference
 *   Shut Down          — asks for confirmation before halting
 *
 * Display layout — 20×4 HD44780
 * ──────────────────────────────
 *   Row 0: title bar  (context-dependent)
 *   Rows 1–3: content (3 menu items with scrolling, or sub-screen text)
 *
 * Hardware
 * ────────
 *   LCD  PCF8574 backpack @ I2C1  addr 0x27
 *   INA219 current monitor @ I2C1 addr 0x40  (100 mΩ shunt)
 *   ENC1  A=GPIO22  B=GPIO23  SW=GPIO24
 *   ENC2  A=GPIO25  B=GPIO26  SW=GPIO27
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <uk/gpio.h>
#include <uk/i2c.h>
#include <uk/hd44780.h>
#include <uk/ina219.h>
#include <uk/encoder.h>
#include <uk/print.h>

/* ── Hardware addresses ───────────────────────────────────────────────── */
#define LCD_ADDR     0x27u
#define INA219_ADDR  0x40u
#define SHUNT_MOHM   100u

/* ── Timing ───────────────────────────────────────────────────────────── */
#define SYSTIMER_CLO   ((volatile uint32_t *)0x3F003004UL)
#define POLL_MS        5u          /* encoder poll interval          */
#define REFRESH_TICKS  200u        /* consumption refresh: 200×5ms=1s */

static void delay_ms(uint32_t ms)
{
    uint32_t start = *SYSTIMER_CLO;
    while ((*SYSTIMER_CLO - start) < ms * 1000U)
        ;
}

/* ── Application states ───────────────────────────────────────────────── */
typedef enum {
    ST_MENU,
    ST_CONSUMPTION,
    ST_RECEIVE,
    ST_TRANSMIT,
    ST_READ_FILE,
    ST_ABOUT,
    ST_HELP,
    ST_SHUTDOWN_CONFIRM,
} state_t;

/* ── Menu definition ──────────────────────────────────────────────────── */
#define MENU_COUNT    7
#define VISIBLE_ROWS  3   /* rows 1–3 on the LCD */

static const char * const _menu_labels[MENU_COUNT] = {
    "About",
    "Help",
    "Check Consumption",
    "Receive Data",
    "Transmit Data",
    "Read File Contents",
    "Shut Down",
};

/* Corresponding state entered on select */
static const state_t _menu_states[MENU_COUNT] = {
    ST_ABOUT,
    ST_HELP,
    ST_CONSUMPTION,
    ST_RECEIVE,
    ST_TRANSMIT,
    ST_READ_FILE,
    ST_SHUTDOWN_CONFIRM,
};

/* ── Global application state ─────────────────────────────────────────── */
static state_t  _state       = ST_MENU;
static int      _menu_sel    = 0;   /* highlighted item index        */
static int      _menu_top    = 0;   /* first visible item index      */
static int      _confirm_sel = 0;   /* 0=Yes 1=No in shutdown dialog */
static int      _needs_redraw = 1;  /* force full LCD redraw         */
static uint32_t _refresh_cnt = 0;   /* consumption refresh counter   */

/* ── LCD helpers ──────────────────────────────────────────────────────── */

/* Write a line using printf formatting; pads to 20 chars automatically via
 * uk_lcd_puts_row so stale characters are always erased. */
static void _lcd_row(uint8_t row, const char *fmt, ...)
{
    char buf[21];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    uk_lcd_puts_row(row, buf);
}

/* ── Menu rendering ───────────────────────────────────────────────────── */
static void _render_menu(void)
{
    uk_lcd_puts_row(0, "   ** MAIN MENU **  ");
    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int idx = _menu_top + i;
        if (idx >= MENU_COUNT) {
            uk_lcd_puts_row((uint8_t)(1 + i), "");
        } else {
            _lcd_row((uint8_t)(1 + i), "%c %-18s",
                     (idx == _menu_sel) ? '>' : ' ',
                     _menu_labels[idx]);
        }
    }
}

/* ── Consumption screen ───────────────────────────────────────────────── */
static void _render_consumption(void)
{
    int32_t  bus_mv     = uk_ina219_read_bus_mv();
    int32_t  current_ma = uk_ina219_read_current_ma();
    uint32_t power_mw   = uk_ina219_read_power_mw();

    uk_lcd_puts_row(0, " Check Consumption  ");

    if (bus_mv < 0)
        uk_lcd_puts_row(1, " Bus:  error        ");
    else
        _lcd_row(1, " Bus:  %d.%03d V",
                 bus_mv / 1000, bus_mv % 1000);

    if (current_ma == INT32_MIN)
        uk_lcd_puts_row(2, " Curr: error        ");
    else {
        int32_t abs_ma = current_ma < 0 ? -current_ma : current_ma;
        _lcd_row(2, " Curr: %s%d mA",
                 current_ma < 0 ? "-" : "", abs_ma);
    }

    if (power_mw == UINT32_MAX)
        uk_lcd_puts_row(3, " Pwr:  error        ");
    else
        _lcd_row(3, " Pwr:  %u mW", power_mw);
}

/* ── Placeholder screen ───────────────────────────────────────────────── */
static void _render_placeholder(const char *title)
{
    uk_lcd_puts_row(0, title);
    uk_lcd_puts_row(1, "");
    uk_lcd_puts_row(2, " Not implemented");
    uk_lcd_puts_row(3, " [ENC1] Back");
}

/* ── About screen ─────────────────────────────────────────────────────── */
static void _render_about(void)
{
    uk_lcd_puts_row(0, "      ** About **   ");
    uk_lcd_puts_row(1, " Unikraft on RPi3");
    uk_lcd_puts_row(2, " Telesto v0.16.3");
    uk_lcd_puts_row(3, " [ENC1] Back");
}

/* ── Help screen ──────────────────────────────────────────────────────── */
static void _render_help(void)
{
    uk_lcd_puts_row(0, "      ** Help **    ");
    uk_lcd_puts_row(1, " ENC2 rot: navigate");
    uk_lcd_puts_row(2, " ENC2 btn: select");
    uk_lcd_puts_row(3, " ENC1 btn: back");
}

/* ── Shutdown confirmation ────────────────────────────────────────────── */
static void _render_shutdown_confirm(void)
{
    uk_lcd_puts_row(0, "   ** Shut Down? ** ");
    _lcd_row(1, " %c Yes", (_confirm_sel == 0) ? '>' : ' ');
    _lcd_row(2, " %c No",  (_confirm_sel == 1) ? '>' : ' ');
    uk_lcd_puts_row(3, " [ENC2] confirm");
}

/* ── Full redraw dispatcher ───────────────────────────────────────────── */
static void _redraw(void)
{
    switch (_state) {
    case ST_MENU:              _render_menu();                          break;
    case ST_CONSUMPTION:       _render_consumption();                   break;
    case ST_RECEIVE:           _render_placeholder(" Receive Data     "); break;
    case ST_TRANSMIT:          _render_placeholder(" Transmit Data    "); break;
    case ST_READ_FILE:         _render_placeholder(" Read File Conts  "); break;
    case ST_ABOUT:             _render_about();                         break;
    case ST_HELP:              _render_help();                          break;
    case ST_SHUTDOWN_CONFIRM:  _render_shutdown_confirm();              break;
    }
    _needs_redraw = 0;
}

/* ── Scroll helper — keep _menu_top in sync with _menu_sel ───────────── */
static void _scroll_to_sel(void)
{
    if (_menu_sel < _menu_top)
        _menu_top = _menu_sel;
    else if (_menu_sel >= _menu_top + VISIBLE_ROWS)
        _menu_top = _menu_sel - VISIBLE_ROWS + 1;
}

/* ── Input handling ───────────────────────────────────────────────────── */
static void _handle_input(void)
{
    int rot  = uk_encoder_poll(UK_ENC2);    /* +1=down, -1=up             */
    int sel  = uk_encoder_button(UK_ENC2);  /* 1 = select pressed         */
    int back = uk_encoder_button(UK_ENC1);  /* 1 = back pressed           */

    switch (_state) {

    /* ── Main menu ────────────────────────────────────────────────────── */
    case ST_MENU:
        if (rot != 0) {
            _menu_sel += rot;
            if (_menu_sel < 0)          _menu_sel = 0;
            if (_menu_sel >= MENU_COUNT) _menu_sel = MENU_COUNT - 1;
            _scroll_to_sel();
            _needs_redraw = 1;
        }
        if (sel) {
            _state        = _menu_states[_menu_sel];
            _confirm_sel  = 1;   /* default "No" in shutdown dialog */
            _refresh_cnt  = 0;
            _needs_redraw = 1;
        }
        break;

    /* ── Consumption (live, refreshes automatically) ──────────────────── */
    case ST_CONSUMPTION:
        if (back) {
            _state = ST_MENU;
            _needs_redraw = 1;
        }
        break;

    /* ── Static / placeholder screens ────────────────────────────────── */
    case ST_RECEIVE:
    case ST_TRANSMIT:
    case ST_READ_FILE:
    case ST_ABOUT:
    case ST_HELP:
        if (back) {
            _state = ST_MENU;
            _needs_redraw = 1;
        }
        break;

    /* ── Shutdown confirmation ────────────────────────────────────────── */
    case ST_SHUTDOWN_CONFIRM:
        if (rot != 0) {
            _confirm_sel = (_confirm_sel + rot + 2) % 2;   /* wrap 0..1 */
            _needs_redraw = 1;
        }
        if (sel) {
            if (_confirm_sel == 0) {
                /* "Yes" — clear LCD and halt */
                uk_lcd_clear();
                uk_lcd_puts_row(0, "  Shutting down...  ");
                delay_ms(1000);
                uk_lcd_backlight(0);
                uk_pr_info("menu: user requested shutdown\n");
                /* Unikraft halt */
                for (;;)
                    __asm__ volatile("wfi");
            } else {
                /* "No" — return to menu */
                _state = ST_MENU;
                _needs_redraw = 1;
            }
        }
        if (back) {
            _state = ST_MENU;
            _needs_redraw = 1;
        }
        break;
    }
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    int rc;

    /* ── GPIO ─────────────────────────────────────────────────────────── */
    rc = uk_gpio_init();
    if (rc) { uk_pr_err("uk_gpio_init() failed: %d\n", rc); return rc; }

    uk_gpio_set_func(2, UK_GPIO_FUNC_ALT0);
    uk_gpio_set_func(3, UK_GPIO_FUNC_ALT0);

    /* ── I2C ──────────────────────────────────────────────────────────── */
    rc = uk_i2c_init();
    if (rc) { uk_pr_err("uk_i2c_init() failed: %d\n", rc); return rc; }

    /* ── LCD ──────────────────────────────────────────────────────────── */
    rc = uk_lcd_init(LCD_ADDR, 20, 4);
    if (rc) { uk_pr_err("uk_lcd_init() failed: %d\n", rc); return rc; }

    /* ── INA219 ───────────────────────────────────────────────────────── */
    rc = uk_ina219_init(INA219_ADDR, SHUNT_MOHM);
    if (rc) {
        uk_pr_err("uk_ina219_init() failed: %d — consumption screen "
                  "will show errors\n", rc);
        /* Non-fatal: menu still works without INA219 */
    }

    /* ── Encoders ─────────────────────────────────────────────────────── */
    uk_encoder_init();

    printf("Menu running.\n");

    /* ── Main loop ────────────────────────────────────────────────────── */
    for (;;) {
        _handle_input();

        /* Periodic refresh for the consumption screen */
        if (_state == ST_CONSUMPTION) {
            _refresh_cnt++;
            if (_refresh_cnt >= REFRESH_TICKS) {
                _refresh_cnt  = 0;
                _needs_redraw = 1;
            }
        }

        if (_needs_redraw)
            _redraw();

        delay_ms(POLL_MS);
    }

    return 0;
}
