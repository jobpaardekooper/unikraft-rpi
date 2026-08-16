/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * measurements/menu-shim-lora-logging/main.c
 *
 * Ground-station firmware: receives LoRa telemetry and logs it to a CSV
 * file on the SD card using FatFs.
 *
 * Expected LoRa packet format (ASCII, newline-terminated):
 *   "ax,ay,az,gx,gy,gz,speed,alt\n"
 *
 * Field units (sender convention — signed integers):
 *   ax/ay/az  — acceleration  (g,    e.g. 20 = 20 g)
 *   gx/gy/gz  — angular rate  (°/s,  e.g. 1080 = 1080 °/s)
 *   speed     — speed         (m/s)
 *   alt       — altitude      (m)
 *
 * CSV log file: "0:/LOG.CSV" on the first FAT partition of the SD card.
 * Header written once on first open; each received packet appends one row.
 *
 * Controls
 * ────────
 *   ENC2 rotate   — navigate menu
 *   ENC2 button   — select / confirm
 *   ENC1 button   — back / cancel
 *
 * Hardware
 * ────────
 *   LCD    PCF8574 @ I2C1  0x27    GPIO 2/3
 *   INA219             @ I2C1  0x40    GPIO 2/3
 *   ENC1   A=GPIO22 B=GPIO23 SW=GPIO24
 *   ENC2   A=GPIO26 B=GPIO25 SW=GPIO27
 *   E220   UART0 GPIO14/15, M0=GPIO17, M1=GPIO16, AUX=GPIO13
 *   SD     Arasan SDHCI, GPIO48-53 (native slot)
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
#include <uk/sdcard.h>
#include <uk/print.h>

/* FatFs — ff.h is copied to unikraft/include/uk/ by the build script */
#include <uk/ff.h>

/* ── Addresses ─────────────────────────────────────────────────────────── */
#define LCD_ADDR     0x27u
#define INA219_ADDR  0x40u
#define SHUNT_MOHM   100u

/* ── Timing ────────────────────────────────────────────────────────────── */
#define SYSTIMER_CLO    ((volatile uint32_t *)0x3F003004UL)
#define POLL_MS         5u
#define REFRESH_TICKS   200u    /* 200 × 5 ms = 1 s */

static void delay_ms(uint32_t ms)
{
    uint32_t start = *SYSTIMER_CLO;
    while ((*SYSTIMER_CLO - start) < ms * 1000U) ;
}

/* ── Application states ─────────────────────────────────────────────────── */
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

/* ── Menu definition ────────────────────────────────────────────────────── */
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
    ST_ABOUT, ST_HELP, ST_CONSUMPTION,
    ST_RECEIVE, ST_TRANSMIT, ST_READ_FILE, ST_SHUTDOWN_CONFIRM,
};

/* ── Global UI state ────────────────────────────────────────────────────── */
static state_t  _state        = ST_MENU;
static int      _menu_sel     = 0;
static int      _menu_top     = 0;
static int      _confirm_sel  = 1;
static int      _needs_redraw = 1;
static uint32_t _refresh_cnt  = 0;

/* ── TX state ───────────────────────────────────────────────────────────── */
static uint32_t _tx_count  = 0;
static int      _tx_status = 0;   /* 0=idle 1=sending 2=sent 3=error */

/* ── RX / log state ─────────────────────────────────────────────────────── */
#define RX_BUF_SIZE  80
static uint8_t  _rx_buf[RX_BUF_SIZE];
static uint16_t _rx_len    = 0;

/* Last successfully parsed telemetry fields */
static int32_t  _last_ax = 0, _last_ay = 0, _last_az = 0;
static int32_t  _last_gx = 0, _last_gy = 0, _last_gz = 0;
static int32_t  _last_spd = 0, _last_alt = 0;
static int      _last_valid = 0;

/* ── SD / FatFs state ───────────────────────────────────────────────────── */
static FATFS    _fs_work;
static int      _sd_ok      = 0;   /* 1 = SD card initialised and mounted */
static uint32_t _log_seq    = 0;   /* CSV row counter                      */
static char     _sd_status[21];    /* one-line status shown on LCD         */

/* ── CSV log helpers ────────────────────────────────────────────────────── */
#define CSV_PATH  "0:/LOG.CSV"
#define CSV_HDR   "seq,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,speed_mps,alt_m\n"

/*
 * _log_open_append — open (or create) LOG.CSV and seek to end.
 * Writes the CSV header if the file is new (size == 0).
 * Returns FR_OK on success.
 */
static FRESULT _log_open_append(FIL *fp)
{
    FRESULT fr = f_open(fp, CSV_PATH, FA_OPEN_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return fr;

    if (f_size(fp) == 0) {
        UINT bw;
        f_write(fp, CSV_HDR, sizeof(CSV_HDR) - 1, &bw);
    }
    /* Seek to end for append */
    return f_lseek(fp, f_size(fp));
}

/*
 * _log_write_row — append one telemetry row to LOG.CSV.
 */
static void _log_write_row(int32_t ax, int32_t ay, int32_t az,
                            int32_t gx, int32_t gy, int32_t gz,
                            int32_t spd, int32_t alt)
{
    if (!_sd_ok) {
        strncpy(_sd_status, " SD not ready", sizeof(_sd_status));
        return;
    }

    FIL fp;
    FRESULT fr = _log_open_append(&fp);
    if (fr != FR_OK) {
        snprintf(_sd_status, sizeof(_sd_status), " SD err %d", (int)fr);
        return;
    }

    _log_seq++;
    /* f_printf: subset of printf — supports %d, %u, %s, %c */
    int n = f_printf(&fp, "%u,%d,%d,%d,%d,%d,%d,%d,%d\n",
                     (unsigned)_log_seq,
                     (int)ax, (int)ay, (int)az,
                     (int)gx, (int)gy, (int)gz,
                     (int)spd, (int)alt);
    f_close(&fp);

    if (n > 0)
        snprintf(_sd_status, sizeof(_sd_status), " Logged #%u", (unsigned)_log_seq);
    else
        strncpy(_sd_status, " Write failed", sizeof(_sd_status));
}

/* ── Telemetry parser ──────────────────────────────────────────────────── */
/*
 * Expects "ax,ay,az,gx,gy,gz,speed,alt\n" with units:
 *   ax/ay/az in g, gx/gy/gz in °/s, speed in m/s, alt in m.
 * Returns 1 if all 8 fields were successfully parsed.
 */
static int _parse_packet(const uint8_t *buf, uint16_t len,
                          int32_t *ax, int32_t *ay, int32_t *az,
                          int32_t *gx, int32_t *gy, int32_t *gz,
                          int32_t *spd, int32_t *alt)
{
    /* Copy to a null-terminated C string (already sanitised) */
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
        int32_t a = current_ma < 0 ? -current_ma : current_ma;
        _lcd_row(2, " Curr: %s%d mA", current_ma < 0 ? "-" : "", (int)a);
    }

    if (power_mw == UINT32_MAX)
        uk_lcd_puts_row(3, " Pwr:  error");
    else
        _lcd_row(3, " Pwr:  %u mW", (unsigned)power_mw);
}

static void _render_receive(void)
{
    int32_t cur_ma = uk_ina219_read_current_ma();
    uk_lcd_puts_row(0, "  ** Receive+Log ** ");
    if (!_last_valid) {
        uk_lcd_puts_row(1, " Waiting...");
        uk_lcd_puts_row(2, _sd_ok ? " SD ready" : " SD FAILED");
    } else {
        _lcd_row(1, " A:%d %d %dg",
                 (int)_last_ax, (int)_last_ay, (int)_last_az);
        _lcd_row(2, " G:%d %d %d",
                 (int)_last_gx, (int)_last_gy, (int)_last_gz);
    }
    /* Row 3: live current + log status / back hint (20-char field) */
    const char *hint = _sd_status[0] ? _sd_status + 1 : "[ENC1]Bk";
    if (cur_ma == INT32_MIN)
        _lcd_row(3, " Curr:err %s", hint);
    else
        _lcd_row(3, " %dmA %s", (int)cur_ma, hint);
}

static void _render_transmit(void)
{
    int32_t cur_ma = uk_ina219_read_current_ma();
    uk_lcd_puts_row(0, " ** Transmit Data **");
    switch (_tx_status) {
    case 0: uk_lcd_puts_row(1, " Ready  Msg:PING"); break;
    case 1: uk_lcd_puts_row(1, " Sending..."); break;
    case 2: _lcd_row(1, " Sent! Count:%u", (unsigned)_tx_count); break;
    case 3: uk_lcd_puts_row(1, " TX Error!"); break;
    }
    if (cur_ma == INT32_MIN)
        uk_lcd_puts_row(2, " Curr: error");
    else
        _lcd_row(2, " Curr: %d mA", (int)cur_ma);
    uk_lcd_puts_row(3, " [ENC2]Send [ENC1]Bk");
}

/*
 * _render_read_file — show summary of the log file: entry count + last row.
 */
static void _render_read_file(void)
{
    uk_lcd_puts_row(0, "  ** Log File **    ");
    if (!_sd_ok) {
        uk_lcd_puts_row(1, " SD card failed");
        uk_lcd_puts_row(2, " No log available");
        uk_lcd_puts_row(3, " [ENC1] Back");
        return;
    }
    _lcd_row(1, " Entries: %u", (unsigned)_log_seq);
    if (_last_valid) {
        _lcd_row(2, " Spd:%dm/s Alt:%dm",
                 (int)_last_spd, (int)_last_alt);
    } else {
        uk_lcd_puts_row(2, " No data yet");
    }
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
            _refresh_cnt = 0;
            _needs_redraw = 1;
            /* Reset status line when entering receive screen */
            if (_state == ST_RECEIVE)
                _sd_status[0] = '\0';
        }
        break;

    case ST_ABOUT:
    case ST_HELP:
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
            _tx_status    = 1;
            _needs_redraw = 1;
        }
        break;

    case ST_READ_FILE:
        if (back) { _state = ST_MENU; _needs_redraw = 1; }
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

/*
 * _task_receive — poll LoRa, parse telemetry, log to CSV.
 */
static void _task_receive(void)
{
    uint16_t n = uk_e220_recv(_rx_buf, RX_BUF_SIZE, 50);
    if (n == 0) return;

    /* Null-terminate for sscanf; replace non-printable with '.' for LCD */
    for (uint16_t i = 0; i < n; i++) {
        if (_rx_buf[i] < 0x20 || _rx_buf[i] > 0x7E)
            _rx_buf[i] = (_rx_buf[i] == '\n' || _rx_buf[i] == '\r') ? '\0' : '.';
    }
    _rx_buf[n] = '\0';
    _rx_len = n;

    int32_t ax, ay, az, gx, gy, gz, spd, alt;
    if (_parse_packet(_rx_buf, n, &ax, &ay, &az, &gx, &gy, &gz, &spd, &alt)) {
        /* Valid telemetry: store and log */
        _last_ax = ax; _last_ay = ay; _last_az = az;
        _last_gx = gx; _last_gy = gy; _last_gz = gz;
        _last_spd = spd; _last_alt = alt;
        _last_valid = 1;
        _log_write_row(ax, ay, az, gx, gy, gz, spd, alt);
    } else {
        /* Unparseable packet — show raw on status line */
        snprintf(_sd_status, sizeof(_sd_status), " ??:%.16s", (char *)_rx_buf);
    }
    _needs_redraw = 1;
}

static void _task_transmit(void)
{
    if (_tx_status != 1) return;
    _redraw();
    static const uint8_t _ping[] = "PING\r\n";
    int rc = uk_e220_send(_ping, sizeof(_ping) - 1);
    _tx_status    = (rc == 0) ? 2 : 3;
    if (rc == 0) _tx_count++;
    _needs_redraw = 1;
}

/* =========================================================================
 * main
 * ========================================================================= */
int main(void)
{
    /* ── GPIO ─────────────────────────────────────────────────────────── */
    if (uk_gpio_init()) return -1;
    uk_gpio_set_func(2, UK_GPIO_FUNC_ALT0);
    uk_gpio_set_func(3, UK_GPIO_FUNC_ALT0);

    /* ── I2C ──────────────────────────────────────────────────────────── */
    if (uk_i2c_init()) return -1;

    /* ── LCD ──────────────────────────────────────────────────────────── */
    uk_lcd_init(LCD_ADDR, 20, 4);
    uk_lcd_puts_row(0, "   Booting...       ");

    /* ── INA219 ───────────────────────────────────────────────────────── */
    uk_ina219_init(INA219_ADDR, SHUNT_MOHM);

    /* ── Encoders ─────────────────────────────────────────────────────── */
    uk_encoder_init();

    /* ── UART + E220 ──────────────────────────────────────────────────── */
    uk_uart_init(9600);
    if (uk_e220_init()) {
        uk_lcd_puts_row(1, " E220 not ready!");
        delay_ms(2000);
    }

    /* ── SD card + FatFs ─────────────────────────────────────────────── */
    uk_lcd_puts_row(1, " Init SD card...    ");
    if (uk_sdcard_init() == 0) {
        /*
         * Diagnostic: read sector 0 (MBR) directly so we can see on the LCD
         * whether CMD17 data transfers work and what the partition table holds.
         *   Line shows:  "S0:55AA Pt=0C"   (MBR sig + partition-1 type)
         *             or "S0 rd err=-EIO"  if the raw read fails.
         * This fires before f_mount so we know where to look if mount fails.
         */
        static uint8_t _sec0[512];
        int rd0 = uk_sdcard_read_block(0, _sec0);
        if (rd0 != 0) {
            /* Show which phase failed and the raw INTERRUPT value.
             * Phase 1 = CMD17 command phase, 2 = READ_RDY wait,
             * 3 = DATA_DONE wait.  Interrupt upper 16 bits are error flags:
             *   0x0001=CMD_TIMEOUT  0x0002=CMD_CRC  0x0010=DATA_TIMEOUT
             *   0x0020=DATA_CRC     0x0040=DATA_END_BIT */
            char _diag[21];
            snprintf(_diag, sizeof(_diag), " Ph%u I=%08X     ",
                     uk_sdcard_dbg_phase, uk_sdcard_dbg_irq);
            uk_lcd_puts_row(1, _diag);
            uk_pr_err("sdcard: sector-0 raw read failed: rc=%d"
                      " phase=%u irq=0x%08X\n",
                      rd0, uk_sdcard_dbg_phase, uk_sdcard_dbg_irq);
            delay_ms(4000);
        } else {
            /* bytes 510-511: MBR boot signature (0x55 0xAA = valid)
             * byte 446+4   : partition-1 type  (0x0B/0x0C = FAT32) */
            char _diag[21];
            snprintf(_diag, sizeof(_diag), " S0:%02X%02X Pt=%02X     ",
                     _sec0[510], _sec0[511], _sec0[446 + 4]);
            uk_lcd_puts_row(1, _diag);
            uk_pr_info("sdcard: MBR sig=%02X%02X pt1_type=%02X lba_start=%lu\n",
                       _sec0[510], _sec0[511], _sec0[446 + 4],
                       (unsigned long)( (uint32_t)_sec0[446+8]
                                      | ((uint32_t)_sec0[446+9]  << 8)
                                      | ((uint32_t)_sec0[446+10] << 16)
                                      | ((uint32_t)_sec0[446+11] << 24)));
            delay_ms(2000);
        }

        FRESULT fr = f_mount(&_fs_work, "0:", 1);
        if (fr == FR_OK) {
            _sd_ok = 1;
            /* Pre-create the log file and write header if needed */
            FIL fp;
            if (_log_open_append(&fp) == FR_OK)
                f_close(&fp);
            uk_lcd_puts_row(1, " SD OK - LOG.CSV    ");
        } else {
            char _merr[21];
            snprintf(_merr, sizeof(_merr), " Mount fail FR=%-4d", (int)fr);
            uk_pr_err("sdcard: f_mount failed FR=%d\n", (int)fr);
            uk_lcd_puts_row(1, _merr);
        }
    } else {
        uk_lcd_puts_row(1, " SD init failed!    ");
    }
    delay_ms(1500);

    _sd_status[0] = '\0';
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
