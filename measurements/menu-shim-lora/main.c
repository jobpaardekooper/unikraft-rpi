/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * measurements/menu-shim-lora/main.c
 *
 * Interactive LCD menu with LoRa send/receive via EBYTE E220-400T30D.
 * Extends menu-shim with working Receive Data and Transmit Data screens.
 *
 * IMPORTANT: CONFIG_RASPI_PRINTF_SERIAL_CONSOLE (and the other two serial
 * console options) must be DISABLED in the build config.  The UART0 pins
 * are used exclusively by the E220 module.
 *
 * Controls
 * ────────
 *   ENC2 rotate left   — cursor up
 *   ENC2 rotate right  — cursor down
 *   ENC2 button click  — select / confirm / send
 *   ENC1 button click  — back / cancel
 *
 * Menu
 * ────
 *   About              — firmware info
 *   Help               — control reference
 *   Check Consumption  — live INA219 readings
 *   Receive Data       — listen for incoming LoRa packets (live display)
 *   Transmit Data      — send a PING message, show TX count
 *   Read File Contents — placeholder
 *   Shut Down          — asks for confirmation
 *
 * Hardware
 * ────────
 *   LCD    PCF8574 @ I2C1  0x27    GPIO 2/3
 *   INA219             @ I2C1  0x40    GPIO 2/3
 *   ENC1   A=GPIO22 B=GPIO23 SW=GPIO24
 *   ENC2   A=GPIO26 B=GPIO25 SW=GPIO27  (A/B swapped vs schematic)
 *   E220   UART0 GPIO14/15, M0=GPIO17, M1=GPIO16, AUX=GPIO13
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#include <uk/gpio.h>
#include <uk/i2c.h>
#include <uk/hd44780.h>
#include <uk/ina219.h>
#include <uk/encoder.h>
#include <uk/uart.h>
#include <uk/e220.h>
#include <uk/print.h>

/* ── Addresses ────────────────────────────────────────────────────────── */
#define LCD_ADDR     0x27u
#define INA219_ADDR  0x40u
#define SHUNT_MOHM   100u

/* ── Timing ───────────────────────────────────────────────────────────── */
#define SYSTIMER_CLO    ((volatile uint32_t *)0x3F003004UL)
#define POLL_MS         5u
#define REFRESH_TICKS   200u    /* 200 × 5 ms = 1 s  (consumption) */

static void delay_ms(uint32_t ms)
{
    uint32_t start = *SYSTIMER_CLO;
    while ((*SYSTIMER_CLO - start) < ms * 1000U)
        ;
}

/* ── Application states ───────────────────────────────────────────────── */
typedef enum {
    ST_MENU,
    ST_ABOUT,
    ST_HELP,
    ST_CONSUMPTION,
    ST_RECEIVE,
    ST_TRANSMIT,
    ST_READ_FILE,
    ST_SHUTDOWN_CONFIRM,
} state_t;

/* ── Menu definition ──────────────────────────────────────────────────── */
#define MENU_COUNT   7
#define VISIBLE_ROWS 3

static const char * const _menu_labels[MENU_COUNT] = {
    "About",
    "Help",
    "Check Consumption",
    "Receive Data",
    "Transmit Data",
    "Read File Contents",
    "Shut Down",
};

static const state_t _menu_states[MENU_COUNT] = {
    ST_ABOUT,
    ST_HELP,
    ST_CONSUMPTION,
    ST_RECEIVE,
    ST_TRANSMIT,
    ST_READ_FILE,
    ST_SHUTDOWN_CONFIRM,
};

/* ── Global state ─────────────────────────────────────────────────────── */
static state_t  _state        = ST_MENU;
static int      _menu_sel     = 0;
static int      _menu_top     = 0;
static int      _confirm_sel  = 1;   /* default "No" */
static int      _needs_redraw = 1;
static uint32_t _refresh_cnt  = 0;

/* ── TX state ─────────────────────────────────────────────────────────── */
static uint32_t _tx_count   = 0;    /* messages sent this session */
static int      _tx_status  = 0;    /* 0=idle 1=sending 2=sent 3=error */

/* ── RX state ─────────────────────────────────────────────────────────── */
#define RX_BUF_SIZE  64
static uint8_t  _rx_buf[RX_BUF_SIZE];
static uint16_t _rx_len     = 0;
static int      _rx_updated = 0;    /* 1 when new data available */

/* Last successfully parsed telemetry (units: g, °/s, m/s, m) */
static int32_t _last_ax = 0, _last_ay = 0, _last_az = 0;
static int32_t _last_gx = 0, _last_gy = 0, _last_gz = 0;
static int32_t _last_spd = 0, _last_alt = 0;
static int     _last_valid = 0;

/* =========================================================================
 * Telemetry parser
 * Expects "ax,ay,az,gx,gy,gz,speed,alt\n" with units:
 *   ax/ay/az in g, gx/gy/gz in °/s, speed in m/s, alt in m.
 * Returns 1 if all 8 fields were successfully parsed.
 * ========================================================================= */
static int _parse_packet(const uint8_t *buf, uint16_t len,
                         int32_t *ax, int32_t *ay, int32_t *az,
                         int32_t *gx, int32_t *gy, int32_t *gz,
                         int32_t *spd, int32_t *alt)
{
    char tmp[RX_BUF_SIZE + 1];
    uint16_t n = len < RX_BUF_SIZE ? len : RX_BUF_SIZE;
    memcpy(tmp, buf, n);
    tmp[n] = '\0';
    int r = sscanf(tmp, "%d,%d,%d,%d,%d,%d,%d,%d",
                   (int *)ax, (int *)ay, (int *)az,
                   (int *)gx, (int *)gy, (int *)gz,
                   (int *)spd, (int *)alt);
    return (r == 8) ? 1 : 0;
}

/* =========================================================================
 * LCD helpers
 * ========================================================================= */

static void _lcd_row(uint8_t row, const char *fmt, ...)
{
    char buf[21];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    uk_lcd_puts_row(row, buf);
}

/* =========================================================================
 * Screen renderers
 * ========================================================================= */

static void _render_menu(void)
{
    uk_lcd_puts_row(0, "   ** MAIN MENU **  ");
    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int idx = _menu_top + i;
        if (idx >= MENU_COUNT)
            uk_lcd_puts_row((uint8_t)(1 + i), "");
        else
            _lcd_row((uint8_t)(1 + i), "%c %-18s",
                     (idx == _menu_sel) ? '>' : ' ',
                     _menu_labels[idx]);
    }
}

static void _render_about(void)
{
    uk_lcd_puts_row(0, "      ** About **   ");
    uk_lcd_puts_row(1, " Unikraft on RPi3");
    uk_lcd_puts_row(2, " Telesto v0.16.3");
    uk_lcd_puts_row(3, " [ENC1] Back");
}

static void _render_help(void)
{
    uk_lcd_puts_row(0, "      ** Help **    ");
    uk_lcd_puts_row(1, " ENC2 rot: navigate");
    uk_lcd_puts_row(2, " ENC2 btn: select");
    uk_lcd_puts_row(3, " ENC1 btn: back");
}

static void _render_consumption(void)
{
    int32_t  bus_mv     = uk_ina219_read_bus_mv();
    int32_t  current_ma = uk_ina219_read_current_ma();
    uint32_t power_mw   = uk_ina219_read_power_mw();

    uk_lcd_puts_row(0, " Check Consumption  ");

    if (bus_mv < 0)
        uk_lcd_puts_row(1, " Bus:  error");
    else
        _lcd_row(1, " Bus:  %d.%03d V", bus_mv / 1000, bus_mv % 1000);

    if (current_ma == INT32_MIN)
        uk_lcd_puts_row(2, " Curr: error");
    else {
        int32_t abs_ma = current_ma < 0 ? -current_ma : current_ma;
        _lcd_row(2, " Curr: %s%d mA",
                 current_ma < 0 ? "-" : "", abs_ma);
    }

    if (power_mw == UINT32_MAX)
        uk_lcd_puts_row(3, " Pwr:  error");
    else
        _lcd_row(3, " Pwr:  %u mW", power_mw);
}

static void _render_receive(void)
{
    uk_lcd_puts_row(0, "  ** Receive Data **");
    if (!_last_valid) {
        uk_lcd_puts_row(1, " Waiting...");
        uk_lcd_puts_row(2, "");
        uk_lcd_puts_row(3, " [ENC1] Back");
    } else {
        _lcd_row(1, " A:%d %d %dg",
                 (int)_last_ax, (int)_last_ay, (int)_last_az);
        _lcd_row(2, " G:%d %d %d",
                 (int)_last_gx, (int)_last_gy, (int)_last_gz);
        _lcd_row(3, " %dm/s %dm [ENC1]Bk",
                 (int)_last_spd, (int)_last_alt);
    }
}

static void _render_transmit(void)
{
    uk_lcd_puts_row(0, " ** Transmit Data **");
    switch (_tx_status) {
    case 0:
        uk_lcd_puts_row(1, " Ready to send");
        uk_lcd_puts_row(2, " Msg: PING");
        break;
    case 1:
        uk_lcd_puts_row(1, " Sending...");
        uk_lcd_puts_row(2, "");
        break;
    case 2:
        uk_lcd_puts_row(1, " Sent!");
        _lcd_row(2, " Count: %u", _tx_count);
        break;
    case 3:
        uk_lcd_puts_row(1, " TX Error!");
        uk_lcd_puts_row(2, " Check E220");
        break;
    }
    uk_lcd_puts_row(3, " [ENC2]Send [ENC1]Bk");
}

static void _render_read_file(void)
{
    uk_lcd_puts_row(0, " Read File Contents ");
    uk_lcd_puts_row(1, "");
    uk_lcd_puts_row(2, " Not implemented");
    uk_lcd_puts_row(3, " [ENC1] Back");
}

static void _render_shutdown_confirm(void)
{
    uk_lcd_puts_row(0, "   ** Shut Down? ** ");
    _lcd_row(1, " %c Yes", (_confirm_sel == 0) ? '>' : ' ');
    _lcd_row(2, " %c No",  (_confirm_sel == 1) ? '>' : ' ');
    uk_lcd_puts_row(3, " [ENC2] confirm");
}

static void _redraw(void)
{
    switch (_state) {
    case ST_MENU:             _render_menu();             break;
    case ST_ABOUT:            _render_about();            break;
    case ST_HELP:             _render_help();             break;
    case ST_CONSUMPTION:      _render_consumption();      break;
    case ST_RECEIVE:          _render_receive();          break;
    case ST_TRANSMIT:         _render_transmit();         break;
    case ST_READ_FILE:        _render_read_file();        break;
    case ST_SHUTDOWN_CONFIRM: _render_shutdown_confirm(); break;
    }
    _needs_redraw = 0;
}

/* =========================================================================
 * Input handling
 * ========================================================================= */

static void _scroll_to_sel(void)
{
    if (_menu_sel < _menu_top)
        _menu_top = _menu_sel;
    else if (_menu_sel >= _menu_top + VISIBLE_ROWS)
        _menu_top = _menu_sel - VISIBLE_ROWS + 1;
}

static void _handle_input(void)
{
    int rot  = uk_encoder_poll(UK_ENC2);
    int sel  = uk_encoder_button(UK_ENC2);
    int back = uk_encoder_button(UK_ENC1);

    switch (_state) {

    case ST_MENU:
        if (rot) {
            _menu_sel += rot;
            if (_menu_sel < 0)           _menu_sel = 0;
            if (_menu_sel >= MENU_COUNT) _menu_sel = MENU_COUNT - 1;
            _scroll_to_sel();
            _needs_redraw = 1;
        }
        if (sel) {
            _state       = _menu_states[_menu_sel];
            _confirm_sel = 1;
            _tx_status   = 0;
            _rx_len      = 0;
            _rx_updated  = 0;
            _refresh_cnt = 0;
            _needs_redraw = 1;
        }
        break;

    case ST_ABOUT:
    case ST_HELP:
    case ST_READ_FILE:
        if (back) { _state = ST_MENU; _needs_redraw = 1; }
        break;

    case ST_CONSUMPTION:
        if (back) { _state = ST_MENU; _needs_redraw = 1; }
        break;

    case ST_RECEIVE:
        if (back) { _state = ST_MENU; _needs_redraw = 1; }
        break;

    case ST_TRANSMIT:
        if (back) { _state = ST_MENU; _needs_redraw = 1; }
        if (sel && _tx_status != 1) {
            /* Trigger a send on the next loop iteration */
            _tx_status    = 1;
            _needs_redraw = 1;
        }
        break;

    case ST_SHUTDOWN_CONFIRM:
        if (rot) {
            _confirm_sel = (_confirm_sel + rot + 2) % 2;
            _needs_redraw = 1;
        }
        if (sel) {
            if (_confirm_sel == 0) {
                uk_lcd_clear();
                uk_lcd_puts_row(0, "  Shutting down...  ");
                delay_ms(1000);
                uk_lcd_backlight(0);
                for (;;) __asm__ volatile("wfi");
            } else {
                _state = ST_MENU;
                _needs_redraw = 1;
            }
        }
        if (back) { _state = ST_MENU; _needs_redraw = 1; }
        break;
    }
}

/* =========================================================================
 * Background tasks (called once per poll tick)
 * ========================================================================= */

static void _task_consumption(void)
{
    _refresh_cnt++;
    if (_refresh_cnt >= REFRESH_TICKS) {
        _refresh_cnt  = 0;
        _needs_redraw = 1;
    }
}

static void _task_receive(void)
{
    uint16_t n = uk_e220_recv(_rx_buf, RX_BUF_SIZE, 50);
    if (n == 0) return;

    for (uint16_t i = 0; i < n; i++) {
        if (_rx_buf[i] < 0x20 || _rx_buf[i] > 0x7E)
            _rx_buf[i] = (_rx_buf[i] == '\n' || _rx_buf[i] == '\r') ? '\0' : '.';
    }
    _rx_buf[n] = '\0';
    _rx_len     = n;
    _rx_updated = 1;

    int32_t ax, ay, az, gx, gy, gz, spd, alt;
    if (_parse_packet(_rx_buf, n, &ax, &ay, &az, &gx, &gy, &gz, &spd, &alt)) {
        _last_ax = ax; _last_ay = ay; _last_az = az;
        _last_gx = gx; _last_gy = gy; _last_gz = gz;
        _last_spd = spd; _last_alt = alt;
        _last_valid = 1;
    }
    _needs_redraw = 1;
}

static void _task_transmit(void)
{
    if (_tx_status != 1)
        return;

    /* Draw "Sending..." before blocking on the RF transmission */
    _redraw();

    static const uint8_t _ping[] = "PING\r\n";
    int rc = uk_e220_send(_ping, sizeof(_ping) - 1);

    if (rc == 0) {
        _tx_count++;
        _tx_status = 2;
    } else {
        _tx_status = 3;
    }
    _needs_redraw = 1;
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    /* ── GPIO ─────────────────────────────────────────────────────────── */
    if (uk_gpio_init()) return -1;
    uk_gpio_set_func(2, UK_GPIO_FUNC_ALT0);   /* SDA */
    uk_gpio_set_func(3, UK_GPIO_FUNC_ALT0);   /* SCL */

    /* ── I2C ──────────────────────────────────────────────────────────── */
    if (uk_i2c_init()) return -1;

    /* ── LCD ──────────────────────────────────────────────────────────── */
    uk_lcd_init(LCD_ADDR, 20, 4);
    uk_lcd_puts_row(0, "   Booting...       ");

    /* ── INA219 ───────────────────────────────────────────────────────── */
    uk_ina219_init(INA219_ADDR, SHUNT_MOHM);  /* non-fatal if absent */

    /* ── Encoders ─────────────────────────────────────────────────────── */
    uk_encoder_init();

    /* ── UART + E220 ──────────────────────────────────────────────────── */
    uk_uart_init(9600);
    if (uk_e220_init()) {
        uk_lcd_puts_row(1, " E220 not ready!");
        uk_lcd_puts_row(2, " Check M0/M1/AUX");
        uk_lcd_puts_row(3, " wiring");
        delay_ms(3000);
        /* Continue anyway — menu still works without LoRa */
    }

    _needs_redraw = 1;

    /* ── Main loop ────────────────────────────────────────────────────── */
    for (;;) {
        _handle_input();

        switch (_state) {
        case ST_CONSUMPTION: _task_consumption(); break;
        case ST_RECEIVE:     _task_receive();     break;
        case ST_TRANSMIT:    _task_transmit();    break;
        default: break;
        }

        if (_needs_redraw)
            _redraw();

        delay_ms(POLL_MS);
    }

    return 0;
}
