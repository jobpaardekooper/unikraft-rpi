/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * ground_station.c — Linux/Raspbian 32-bit equivalent of the Unikraft
 * menu-shim-lora-logging firmware.
 *
 * Identical hardware, identical application logic.  Linux replaces the
 * bare-metal drivers with /dev/gpiomem (BCM2835 register mmap), /dev/i2c-1
 * (ioctl), and /dev/ttyAMA0 (termios).  FatFs + EMMC are replaced by a
 * plain stdio file on the native filesystem.
 *
 * Hardware (same pinout as Unikraft build):
 *   LCD    PCF8574 @ I2C1 0x27   GPIO 2/3  (kernel i2c-bcm2835)
 *   INA219          @ I2C1 0x40   GPIO 2/3
 *   ENC1   A=GPIO22  B=GPIO23  SW=GPIO24
 *   ENC2   A=GPIO26  B=GPIO25  SW=GPIO27
 *   E220   /dev/ttyAMA0  M0=GPIO17  M1=GPIO16  AUX=GPIO13
 *   LOG    ~/LOG.CSV  (ordinary file — SD card is the root filesystem)
 *
 * ── Prerequisites on Raspbian ────────────────────────────────────────────
 *   1. Enable I2C:
 *        sudo raspi-config → Interface Options → I2C → Enable
 *   2. Configure UART (free ttyAMA0 from Bluetooth):
 *        sudo raspi-config → Interface Options → Serial Port
 *            "login shell over serial?" → No
 *            "serial hardware enabled?" → Yes
 *        In /boot/config.txt add:
 *            dtoverlay=disable-bt
 *        sudo reboot
 *   3. Group membership (no sudo needed at runtime):
 *        sudo adduser $USER gpio    # /dev/gpiomem
 *        sudo adduser $USER i2c     # /dev/i2c-1
 *        sudo adduser $USER dialout # /dev/ttyAMA0
 *        (log out and back in)
 *
 * ── Build ────────────────────────────────────────────────────────────────
 *   gcc -O2 -Wall -Wextra -o ground_station ground_station.c
 *
 * ── Run ──────────────────────────────────────────────────────────────────
 *   ./ground_station
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

/* =========================================================================
 * Timing helpers
 * ========================================================================= */

static void delay_us(uint32_t us)
{
    struct timespec ts = {
        .tv_sec  = us / 1000000u,
        .tv_nsec = (us % 1000000u) * 1000L,
    };
    nanosleep(&ts, NULL);
}

static void delay_ms(uint32_t ms)
{
    delay_us(ms * 1000u);
}

/* Monotonic microsecond counter (wraps at ~4 295 s on 32-bit — acceptable) */
static uint32_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000u + ts.tv_nsec / 1000u);
}

/* =========================================================================
 * GPIO — direct BCM2837 register access via /dev/gpiomem
 *
 * /dev/gpiomem maps only the GPIO block (phys 0x3F200000) starting at
 * file offset 0.  No root required — Raspbian's gpio group grants access.
 * ========================================================================= */

#define GPIO_MAP_SIZE   0x1000u

/* Register offsets (in 32-bit words) within the GPIO block */
#define GPFSEL0   0u   /* function select, 10 pins × 3 bits each */
#define GPSET0   7u    /* output set   bank 0 (GPIO  0-31) */
#define GPSET1   8u    /* output set   bank 1 (GPIO 32-53) */
#define GPCLR0  10u    /* output clear bank 0 */
#define GPCLR1  11u    /* output clear bank 1 */
#define GPLEV0  13u    /* level        bank 0 */
#define GPLEV1  14u    /* level        bank 1 */
#define GPPUD   37u    /* pull-up/down enable */
#define GPPUDCLK0 38u  /* pull clock   bank 0 */
#define GPPUDCLK1 39u  /* pull clock   bank 1 */

static volatile uint32_t *_gpio = NULL;

/* Function select codes — matches Unikraft UK_GPIO_FUNC_* values */
#define FUNC_INPUT  0u
#define FUNC_OUTPUT 1u
#define FUNC_ALT5   2u
#define FUNC_ALT4   3u
#define FUNC_ALT0   4u
#define FUNC_ALT1   5u
#define FUNC_ALT2   6u
#define FUNC_ALT3   7u

/* Pull modes — matches Unikraft UK_GPIO_PUD_* values */
#define PUD_OFF  0u
#define PUD_DOWN 1u
#define PUD_UP   2u

static int gpio_init(void)
{
    int fd = open("/dev/gpiomem", O_RDWR | O_SYNC | O_CLOEXEC);
    if (fd < 0) {
        perror("open /dev/gpiomem");
        return -1;
    }
    void *map = mmap(NULL, GPIO_MAP_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED) {
        perror("mmap /dev/gpiomem");
        return -1;
    }
    _gpio = (volatile uint32_t *)map;
    return 0;
}

static void gpio_set_func(unsigned int pin, unsigned int func)
{
    unsigned int reg  = GPFSEL0 + pin / 10;
    unsigned int shift = (pin % 10) * 3;
    uint32_t val = _gpio[reg];
    val &= ~(7u << shift);
    val |=  (func & 7u) << shift;
    _gpio[reg] = val;
    delay_us(1);
}

static void gpio_set(unsigned int pin, int level)
{
    if (pin < 32)
        _gpio[level ? GPSET0 : GPCLR0] = (1u << pin);
    else
        _gpio[level ? GPSET1 : GPCLR1] = (1u << (pin - 32));
}

static int gpio_get(unsigned int pin)
{
    if (pin < 32)
        return (_gpio[GPLEV0] >> pin) & 1u;
    return (_gpio[GPLEV1] >> (pin - 32)) & 1u;
}

static void gpio_set_pud(unsigned int pin, unsigned int pud)
{
    unsigned int clk_reg = (pin < 32) ? GPPUDCLK0 : GPPUDCLK1;
    unsigned int clk_bit = 1u << (pin < 32 ? pin : pin - 32);

    _gpio[GPPUD] = pud & 3u;
    delay_us(5);
    _gpio[clk_reg] = clk_bit;
    delay_us(5);
    _gpio[GPPUD] = 0;
    _gpio[clk_reg] = 0;
    delay_us(1);
}

/* =========================================================================
 * I2C — /dev/i2c-1
 * ========================================================================= */

static int _i2c_fd = -1;

static int i2c_init(void)
{
    const char *candidates[] = { "/dev/i2c-1", "/dev/i2c-2", "/dev/i2c-0", NULL };
    for (int i = 0; candidates[i]; i++) {
        _i2c_fd = open(candidates[i], O_RDWR | O_CLOEXEC);
        if (_i2c_fd >= 0) {
            fprintf(stderr, "I2C: using %s\n", candidates[i]);
            return 0;
        }
    }
    perror("open /dev/i2c-*");
    return -1;
}

static int i2c_write(uint8_t addr, const uint8_t *buf, uint16_t len)
{
    if (ioctl(_i2c_fd, I2C_SLAVE, addr) < 0)
        return -EIO;
    if (write(_i2c_fd, buf, len) != (ssize_t)len)
        return -EIO;
    return 0;
}

static int i2c_read(uint8_t addr, uint8_t *buf, uint16_t len)
{
    if (ioctl(_i2c_fd, I2C_SLAVE, addr) < 0)
        return -EIO;
    if (read(_i2c_fd, buf, len) != (ssize_t)len)
        return -EIO;
    return 0;
}

/* =========================================================================
 * HD44780 LCD via PCF8574 I2C backpack  (same protocol as bcm_hd44780.c)
 *
 * PCF8574 bit mapping:
 *   P7=D7  P6=D6  P5=D5  P4=D4  P3=BL  P2=E  P1=RW  P0=RS
 * ========================================================================= */

#define LCD_RS  0x01u
#define LCD_RW  0x02u
#define LCD_EN  0x04u
#define LCD_BL  0x08u

static const uint8_t _row_addr[4] = { 0x00u, 0x40u, 0x14u, 0x54u };

static uint8_t _lcd_addr;
static uint8_t _lcd_bl   = LCD_BL;
static uint8_t _lcd_cols = 20;
static uint8_t _lcd_rows = 4;

static void _lcd_pcf(uint8_t byte)
{
    i2c_write(_lcd_addr, &byte, 1);
}

static void _lcd_pulse(uint8_t data)
{
    _lcd_pcf(data | LCD_EN);
    delay_us(1);
    _lcd_pcf(data & ~LCD_EN);
    delay_us(50);
}

static void _lcd_nibble(uint8_t nibble, uint8_t flags)
{
    _lcd_pulse((nibble & 0xF0u) | flags | _lcd_bl);
}

static void _lcd_byte(uint8_t byte, uint8_t flags)
{
    _lcd_nibble(byte,       flags);
    _lcd_nibble(byte << 4u, flags);
}

static void _lcd_cmd(uint8_t cmd)
{
    _lcd_byte(cmd, 0);
    delay_us(1600);
}

static void _lcd_dat(uint8_t data)
{
    _lcd_byte(data, LCD_RS);
    delay_us(50);
}

static int lcd_init(uint8_t addr7, uint8_t cols, uint8_t rows)
{
    _lcd_addr = addr7;
    _lcd_cols = cols ? cols : 20u;
    _lcd_rows = rows ? rows : 4u;
    _lcd_bl   = LCD_BL;

    delay_us(50000);
    _lcd_pcf(_lcd_bl);
    delay_us(1000);

    _lcd_nibble(0x30u, 0); delay_us(4500);
    _lcd_nibble(0x30u, 0); delay_us(4500);
    _lcd_nibble(0x30u, 0); delay_us(200);
    _lcd_nibble(0x20u, 0); delay_us(200);

    _lcd_cmd(0x28u);
    _lcd_cmd(0x08u);
    _lcd_cmd(0x01u);
    delay_us(2000);
    _lcd_cmd(0x06u);
    _lcd_cmd(0x0Cu);
    return 0;
}

static void lcd_set_cursor(uint8_t row, uint8_t col)
{
    if (row >= _lcd_rows) row = _lcd_rows - 1;
    if (col >= _lcd_cols) col = _lcd_cols - 1;
    _lcd_cmd(0x80u | (_row_addr[row] + col));
}

static void lcd_puts_row(uint8_t row, const char *s)
{
    uint8_t col = 0;
    lcd_set_cursor(row, 0);
    while (*s && col < _lcd_cols) { _lcd_dat((uint8_t)*s++); col++; }
    while (col < _lcd_cols)       { _lcd_dat(' ');           col++; }
}

static void lcd_clear(void)   { _lcd_cmd(0x01u); delay_us(2000); }
static void lcd_backlight(int on) { _lcd_bl = on ? LCD_BL : 0u; _lcd_pcf(_lcd_bl); }

static void lcd_row(uint8_t row, const char *fmt, ...)
{
    char buf[21];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    lcd_puts_row(row, buf);
}

/* =========================================================================
 * INA219  (same register protocol as bcm_ina219.c)
 * ========================================================================= */

#define INA219_REG_CONFIG  0x00u
#define INA219_REG_SHUNT   0x01u
#define INA219_REG_BUS     0x02u
#define INA219_REG_POWER   0x03u
#define INA219_REG_CURRENT 0x04u
#define INA219_REG_CALIB   0x05u
#define INA219_CFG_DEFAULT 0x399Fu

static uint8_t  _ina_addr;
static uint32_t _ina_ilsb_ua;   /* current LSB in µA */
static uint32_t _ina_plsb_mw;   /* power   LSB in mW */

static int _ina_write(uint8_t reg, uint16_t val)
{
    uint8_t buf[3] = { reg, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
    return i2c_write(_ina_addr, buf, 3);
}

static int _ina_read(uint8_t reg, uint16_t *out)
{
    uint8_t r[2];
    if (i2c_write(_ina_addr, &reg, 1)) return -EIO;
    if (i2c_read(_ina_addr, r, 2))     return -EIO;
    *out = ((uint16_t)r[0] << 8) | r[1];
    return 0;
}

static int ina219_init(uint8_t addr7, uint32_t r_shunt_mohm)
{
    _ina_addr    = addr7;
    if (r_shunt_mohm == 0) r_shunt_mohm = 100;
    uint16_t cal = (uint16_t)(40960000UL / (100UL * r_shunt_mohm));
    _ina_ilsb_ua = 100;
    _ina_plsb_mw = 2;
    if (_ina_write(INA219_REG_CONFIG, INA219_CFG_DEFAULT)) return -1;
    if (_ina_write(INA219_REG_CALIB,  cal))                return -1;
    return 0;
}

static int32_t ina219_read_bus_mv(void)
{
    uint16_t raw;
    if (_ina_read(INA219_REG_BUS, &raw)) return -1;
    return (int32_t)((raw >> 3) * 4);
}

static int32_t ina219_read_current_ma(void)
{
    uint16_t raw;
    if (_ina_read(INA219_REG_CURRENT, &raw)) return INT32_MIN;
    int32_t s = (int32_t)(int16_t)raw;
    if (s >= 0) return  (int32_t)((uint32_t) s * _ina_ilsb_ua / 1000u);
    else        return -(int32_t)((uint32_t)(-s) * _ina_ilsb_ua / 1000u);
}

static uint32_t ina219_read_power_mw(void)
{
    uint16_t raw;
    if (_ina_read(INA219_REG_POWER, &raw)) return UINT32_MAX;
    return (uint32_t)raw * _ina_plsb_mw;
}

/* =========================================================================
 * Dual rotary encoder  (same grey-code decoder as bcm_encoder.c)
 *
 * ENC1: A=GPIO22  B=GPIO23  SW=GPIO24
 * ENC2: A=GPIO26  B=GPIO25  SW=GPIO27
 * ========================================================================= */

#define UK_ENC1 0
#define UK_ENC2 1

static const unsigned int _pin_a[2]  = { 22, 26 };
static const unsigned int _pin_b[2]  = { 23, 25 };
static const unsigned int _pin_sw[2] = { 24, 27 };

static const int8_t _enc_table[16] = {
    0, -1, +1,  0,
   +1,  0,  0, -1,
   -1,  0,  0, +1,
    0, +1, -1,  0,
};

static uint8_t _enc_prev[2];
static int8_t  _enc_raw[2];

#define DEBOUNCE_TICKS 6
static uint8_t _btn_cnt[2];
static uint8_t _btn_armed[2];

static void encoder_init(void)
{
    for (int i = 0; i < 2; i++) {
        gpio_set_func(_pin_a[i],  FUNC_INPUT);
        gpio_set_func(_pin_b[i],  FUNC_INPUT);
        gpio_set_func(_pin_sw[i], FUNC_INPUT);
        gpio_set_pud(_pin_a[i],  PUD_UP);
        gpio_set_pud(_pin_b[i],  PUD_UP);
        gpio_set_pud(_pin_sw[i], PUD_UP);
        uint8_t a = gpio_get(_pin_a[i]) ? 1u : 0u;
        uint8_t b = gpio_get(_pin_b[i]) ? 1u : 0u;
        _enc_prev[i]  = (uint8_t)((a << 1) | b);
        _enc_raw[i]   = 0;
        _btn_cnt[i]   = 0;
        _btn_armed[i] = 0;
    }
}

static int encoder_poll(int enc)
{
    if (enc < 0 || enc > 1) return 0;
    uint8_t a   = gpio_get(_pin_a[enc]) ? 1u : 0u;
    uint8_t b   = gpio_get(_pin_b[enc]) ? 1u : 0u;
    uint8_t cur = (uint8_t)((a << 1) | b);
    int8_t  d   = _enc_table[(_enc_prev[enc] << 2) | cur];
    _enc_prev[enc] = cur;
    if (!d) return 0;
    _enc_raw[enc] = (int8_t)(_enc_raw[enc] + d);
    if (_enc_raw[enc] >= 4)  { _enc_raw[enc] = 0; return +1; }
    if (_enc_raw[enc] <= -4) { _enc_raw[enc] = 0; return -1; }
    return 0;
}

static int encoder_button(int enc)
{
    if (enc < 0 || enc > 1) return 0;
    int low = (gpio_get(_pin_sw[enc]) == 0);
    if (low) {
        if (_btn_cnt[enc] < 255) _btn_cnt[enc]++;
    } else {
        _btn_cnt[enc]   = 0;
        _btn_armed[enc] = 0;
    }
    if (_btn_cnt[enc] == DEBOUNCE_TICKS && !_btn_armed[enc]) {
        _btn_armed[enc] = 1;
        return 1;
    }
    return 0;
}

/* =========================================================================
 * UART — /dev/ttyAMA0 (PL011, the same as Unikraft UART0)
 *
 * On RPi3 ttyAMA0 is normally given to Bluetooth; to reclaim it:
 *   Add "dtoverlay=disable-bt" to /boot/config.txt and reboot.
 * ========================================================================= */

static int _uart_fd = -1;

static int uart_init(uint32_t baud)
{
    /* Try the canonical path first, fall back to alias */
    _uart_fd = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (_uart_fd < 0) {
        _uart_fd = open("/dev/serial0", O_RDWR | O_NOCTTY | O_CLOEXEC);
        if (_uart_fd < 0) {
            perror("open /dev/ttyAMA0");
            return -1;
        }
    }

    struct termios tty;
    if (tcgetattr(_uart_fd, &tty) < 0) { perror("tcgetattr"); return -1; }

    /* Raw mode — no echo, no signals, no special processing */
    cfmakeraw(&tty);

    /* Baud rate */
    speed_t speed = B9600;
    switch (baud) {
    case 19200:  speed = B19200;  break;
    case 38400:  speed = B38400;  break;
    case 57600:  speed = B57600;  break;
    case 115200: speed = B115200; break;
    default:     speed = B9600;   break;
    }
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    /* 8N1, no flow control */
    tty.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    tty.c_cflag |=  (CS8 | CLOCAL | CREAD);

    /* Non-blocking read */
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(_uart_fd, TCSANOW, &tty) < 0) { perror("tcsetattr"); return -1; }
    tcflush(_uart_fd, TCIOFLUSH);
    return 0;
}

static void uart_write(const uint8_t *buf, uint16_t len)
{
    ssize_t written = 0;
    while (written < (ssize_t)len) {
        ssize_t n = write(_uart_fd, buf + written, len - written);
        if (n > 0) written += n;
    }
}

static uint16_t uart_read_timeout(uint8_t *buf, uint16_t max_len,
                                   uint32_t idle_ms)
{
    uint16_t n = 0;
    uint32_t deadline = now_us() + idle_ms * 1000u;

    while (n < max_len) {
        uint8_t c;
        ssize_t r = read(_uart_fd, &c, 1);
        if (r == 1) {
            buf[n++] = c;
            deadline = now_us() + idle_ms * 1000u;
        } else {
            if (now_us() >= deadline) break;
            delay_us(500);
        }
    }
    return n;
}

/* =========================================================================
 * E220 LoRa driver  (same protocol as bcm_e220.c)
 *
 * M0=GPIO17  M1=GPIO16  AUX=GPIO13
 * GPIO 14/15 (UART TX/RX) are already managed by the ttyAMA0 kernel driver.
 * ========================================================================= */

#define PIN_M0   17u
#define PIN_M1   16u
#define PIN_AUX  13u

static int _e220_wait_aux(uint32_t timeout_ms)
{
    uint32_t deadline = now_us() + timeout_ms * 1000u;
    while (!gpio_get(PIN_AUX)) {
        if (now_us() >= deadline) return -1;
        delay_us(100);
    }
    return 0;
}

static int e220_init(void)
{
    gpio_set_func(PIN_M0, FUNC_OUTPUT);
    gpio_set_func(PIN_M1, FUNC_OUTPUT);
    gpio_set(PIN_M0, 0);
    gpio_set(PIN_M1, 0);
    gpio_set_func(PIN_AUX, FUNC_INPUT);
    gpio_set_pud(PIN_AUX, PUD_UP);
    delay_ms(10);
    return _e220_wait_aux(2000);
}

static int e220_send(const uint8_t *data, uint16_t len)
{
    gpio_set(PIN_M0, 0);
    gpio_set(PIN_M1, 0);
    if (_e220_wait_aux(500)) return -1;
    uart_write(data, len);
    delay_ms(5);
    _e220_wait_aux(5000);
    return 0;
}

static uint16_t e220_recv(uint8_t *buf, uint16_t max_len, uint32_t idle_ms)
{
    gpio_set(PIN_M0, 0);
    gpio_set(PIN_M1, 0);
    return uart_read_timeout(buf, max_len, idle_ms);
}

/* =========================================================================
 * SD card / logging
 *
 * On Linux the SD card is already the root filesystem.  Logging to a file
 * replaces FatFs; behaviour is identical from the application's perspective.
 * ========================================================================= */

#define LOG_PATH  "./LOG.CSV"
#define CSV_HDR   "seq,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,speed_mps,alt_m\n"

static int      _sd_ok   = 0;
static uint32_t _log_seq = 0;
static char     _sd_status[21];

static int sdcard_init(void)
{
    /* Verify we can open/create the log file */
    FILE *fp = fopen(LOG_PATH, "a");
    if (!fp) {
        fprintf(stderr, "Cannot open log file %s: %s\n", LOG_PATH, strerror(errno));
        return -1;
    }
    /* Write header if new / empty */
    fseek(fp, 0, SEEK_END);
    if (ftell(fp) == 0)
        fputs(CSV_HDR, fp);
    fclose(fp);
    return 0;
}

static void log_write_row(int32_t ax, int32_t ay, int32_t az,
                           int32_t gx, int32_t gy, int32_t gz,
                           int32_t spd, int32_t alt)
{
    if (!_sd_ok) {
        strncpy(_sd_status, " SD not ready", sizeof(_sd_status));
        return;
    }
    FILE *fp = fopen(LOG_PATH, "a");
    if (!fp) {
        snprintf(_sd_status, sizeof(_sd_status), " fopen err");
        return;
    }
    _log_seq++;
    int n = fprintf(fp, "%u,%d,%d,%d,%d,%d,%d,%d,%d\n",
                    (unsigned)_log_seq,
                    (int)ax, (int)ay, (int)az,
                    (int)gx, (int)gy, (int)gz,
                    (int)spd, (int)alt);
    fclose(fp);
    if (n > 0)
        snprintf(_sd_status, sizeof(_sd_status), " Logged #%u", (unsigned)_log_seq);
    else
        strncpy(_sd_status, " Write failed", sizeof(_sd_status));
}

/* =========================================================================
 * Application — identical logic to menu-shim-lora-logging/main.c
 * ========================================================================= */

/* ── Addresses ─────────────────────────────────────────────────────────── */
#define LCD_ADDR    0x27u
#define INA219_ADDR 0x40u
#define SHUNT_MOHM  100u

/* ── Poll interval ──────────────────────────────────────────────────────── */
#define POLL_MS      5u
#define REFRESH_TICKS 200u    /* 200 × 5 ms = 1 s */

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

/* ── TX state ────────────────────────────────────────────────────────────── */
static uint32_t _tx_count  = 0;
static int      _tx_status = 0;

/* ── RX / log state ─────────────────────────────────────────────────────── */
#define RX_BUF_SIZE 80
static uint8_t  _rx_buf[RX_BUF_SIZE];
static uint16_t _rx_len = 0;

static int32_t _last_ax=0, _last_ay=0, _last_az=0;
static int32_t _last_gx=0, _last_gy=0, _last_gz=0;
static int32_t _last_spd=0, _last_alt=0;
static int     _last_valid = 0;

/* ── Telemetry parser ────────────────────────────────────────────────────── */
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

/* ── Screen renderers ────────────────────────────────────────────────────── */
static void _render_menu(void)
{
    lcd_puts_row(0, "   ** MAIN MENU **  ");
    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int idx = _menu_top + i;
        if (idx >= MENU_COUNT)
            lcd_puts_row((uint8_t)(1 + i), "");
        else
            lcd_row((uint8_t)(1 + i), "%c %-18s",
                    (idx == _menu_sel) ? '>' : ' ',
                    _menu_labels[idx]);
    }
}

static void _render_about(void)
{
    lcd_puts_row(0, "      ** About **   ");
    lcd_puts_row(1, " Linux on RPi3");
    lcd_puts_row(2, " Raspbian 32-bit");
    lcd_puts_row(3, " [ENC1] Back");
}

static void _render_help(void)
{
    lcd_puts_row(0, "      ** Help **    ");
    lcd_puts_row(1, " ENC2 rot: navigate");
    lcd_puts_row(2, " ENC2 btn: select");
    lcd_puts_row(3, " ENC1 btn: back");
}

static void _render_consumption(void)
{
    int32_t  bus_mv     = ina219_read_bus_mv();
    int32_t  current_ma = ina219_read_current_ma();
    uint32_t power_mw   = ina219_read_power_mw();

    lcd_puts_row(0, " Check Consumption  ");

    if (bus_mv < 0)
        lcd_puts_row(1, " Bus:  error");
    else
        lcd_row(1, " Bus:  %d.%03d V", bus_mv / 1000, bus_mv % 1000);

    if (current_ma == INT32_MIN)
        lcd_puts_row(2, " Curr: error");
    else {
        int32_t a = current_ma < 0 ? -current_ma : current_ma;
        lcd_row(2, " Curr: %s%d mA", current_ma < 0 ? "-" : "", (int)a);
    }

    if (power_mw == UINT32_MAX)
        lcd_puts_row(3, " Pwr:  error");
    else
        lcd_row(3, " Pwr:  %u mW", (unsigned)power_mw);
}

static void _render_receive(void)
{
    int32_t cur_ma = ina219_read_current_ma();
    lcd_puts_row(0, "  ** Receive+Log ** ");
    if (!_last_valid) {
        lcd_puts_row(1, " Waiting...");
        lcd_puts_row(2, _sd_ok ? " SD ready" : " SD FAILED");
    } else {
        lcd_row(1, " A:%d %d %dg",
                (int)_last_ax, (int)_last_ay, (int)_last_az);
        lcd_row(2, " G:%d %d %d",
                (int)_last_gx, (int)_last_gy, (int)_last_gz);
    }
    /* Row 3: live current + log status / back hint (20-char field) */
    const char *hint = _sd_status[0] ? _sd_status + 1 : "[ENC1]Bk";
    if (cur_ma == INT32_MIN)
        lcd_row(3, " Curr:err %s", hint);
    else
        lcd_row(3, " %dmA %s", (int)cur_ma, hint);
}

static void _render_transmit(void)
{
    int32_t cur_ma = ina219_read_current_ma();
    lcd_puts_row(0, " ** Transmit Data **");
    switch (_tx_status) {
    case 0: lcd_puts_row(1, " Ready  Msg:PING"); break;
    case 1: lcd_puts_row(1, " Sending..."); break;
    case 2: lcd_row(1, " Sent! Count:%u", (unsigned)_tx_count); break;
    case 3: lcd_puts_row(1, " TX Error!"); break;
    }
    if (cur_ma == INT32_MIN)
        lcd_puts_row(2, " Curr: error");
    else
        lcd_row(2, " Curr: %d mA", (int)cur_ma);
    lcd_puts_row(3, " [ENC2]Send [ENC1]Bk");
}

static void _render_read_file(void)
{
    lcd_puts_row(0, "  ** Log File **    ");
    if (!_sd_ok) {
        lcd_puts_row(1, " SD card failed");
        lcd_puts_row(2, " No log available");
        lcd_puts_row(3, " [ENC1] Back");
        return;
    }
    lcd_row(1, " Entries: %u", (unsigned)_log_seq);
    if (_last_valid)
        lcd_row(2, " Spd:%dm/s Alt:%dm", (int)_last_spd, (int)_last_alt);
    else
        lcd_puts_row(2, " No data yet");
    lcd_puts_row(3, " [ENC1] Back");
}

static void _render_shutdown_confirm(void)
{
    lcd_puts_row(0, "   ** Shut Down? ** ");
    lcd_row(1, " %c Yes", (_confirm_sel == 0) ? '>' : ' ');
    lcd_row(2, " %c No",  (_confirm_sel == 1) ? '>' : ' ');
    lcd_puts_row(3, " [ENC2] confirm");
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

/* ── Input handling ──────────────────────────────────────────────────────── */
static void _scroll_to_sel(void)
{
    if (_menu_sel < _menu_top)
        _menu_top = _menu_sel;
    else if (_menu_sel >= _menu_top + VISIBLE_ROWS)
        _menu_top = _menu_sel - VISIBLE_ROWS + 1;
}

static void _handle_input(void)
{
    int rot  = encoder_poll(UK_ENC2);
    int sel  = encoder_button(UK_ENC2);
    int back = encoder_button(UK_ENC1);

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
                lcd_clear();
                lcd_puts_row(0, "  Shutting down...  ");
                delay_ms(1000);
                lcd_backlight(0);
                sync();
                system("sudo shutdown -h now");
                /* If shutdown fails (e.g. no sudo), just exit */
                exit(0);
            } else {
                _state = ST_MENU;
                _needs_redraw = 1;
            }
        }
        if (back) { _state = ST_MENU; _needs_redraw = 1; }
        break;
    }
}

/* ── Background tasks ────────────────────────────────────────────────────── */
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
    uint16_t n = e220_recv(_rx_buf, RX_BUF_SIZE, 50);
    if (n == 0) return;

    for (uint16_t i = 0; i < n; i++) {
        if (_rx_buf[i] < 0x20 || _rx_buf[i] > 0x7E)
            _rx_buf[i] = (_rx_buf[i] == '\n' || _rx_buf[i] == '\r') ? '\0' : '.';
    }
    _rx_buf[n] = '\0';
    _rx_len = n;

    int32_t ax, ay, az, gx, gy, gz, spd, alt;
    if (_parse_packet(_rx_buf, n, &ax, &ay, &az, &gx, &gy, &gz, &spd, &alt)) {
        _last_ax = ax; _last_ay = ay; _last_az = az;
        _last_gx = gx; _last_gy = gy; _last_gz = gz;
        _last_spd = spd; _last_alt = alt;
        _last_valid = 1;
        log_write_row(ax, ay, az, gx, gy, gz, spd, alt);
    } else {
        snprintf(_sd_status, sizeof(_sd_status), " ??:%.16s", (char *)_rx_buf);
        /* Print raw bytes to stderr so we can see what arrived corrupted */
        fprintf(stderr, "RX PARSE FAIL (%u bytes): [", (unsigned)n);
        for (uint16_t i = 0; i < n; i++) {
            uint8_t b = _rx_buf[i];
            if (b >= 0x20 && b <= 0x7E)
                fputc(b, stderr);
            else
                fprintf(stderr, "\\x%02X", b);
        }
        fprintf(stderr, "]\n");
    }
    _needs_redraw = 1;
}

static void _task_transmit(void)
{
    if (_tx_status != 1) return;
    _redraw();
    static const uint8_t _ping[] = "PING\r\n";
    int rc = e220_send(_ping, sizeof(_ping) - 1);
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
    if (gpio_init() < 0) {
        fprintf(stderr, "GPIO init failed. Are you in the 'gpio' group?\n");
        return 1;
    }

    /* ── I2C ──────────────────────────────────────────────────────────── */
    if (i2c_init() < 0) {
        fprintf(stderr, "I2C init failed. Is I2C enabled? Are you in 'i2c' group?\n");
        return 1;
    }

    /* ── LCD ──────────────────────────────────────────────────────────── */
    lcd_init(LCD_ADDR, 20, 4);
    lcd_puts_row(0, "   Booting...       ");

    /* ── INA219 ───────────────────────────────────────────────────────── */
    ina219_init(INA219_ADDR, SHUNT_MOHM);

    /* ── Encoders ─────────────────────────────────────────────────────── */
    encoder_init();

    /* ── UART + E220 ──────────────────────────────────────────────────── */
    if (uart_init(9600) < 0) {
        lcd_puts_row(1, " UART init failed!  ");
        delay_ms(2000);
    } else if (e220_init() < 0) {
        lcd_puts_row(1, " E220 not ready!    ");
        delay_ms(2000);
    }

    /* ── Log file (replaces SD card + FatFs) ─────────────────────────── */
    lcd_puts_row(1, " Init log file...   ");
    if (sdcard_init() == 0) {
        _sd_ok = 1;
        lcd_puts_row(1, " Log OK - LOG.CSV   ");
    } else {
        lcd_puts_row(1, " Log init failed!   ");
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
