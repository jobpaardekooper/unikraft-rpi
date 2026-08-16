/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm_ina219.c — INA219 current/power monitor driver for Unikraft / RPi3.
 *
 * Native Unikraft module — no FreeBSD shim layer involved.
 * Sits on top of uk_i2c_write() and uk_i2c_write_read() from the BCM2835
 * I2C shim (BSC1, SDA=GPIO2, SCL=GPIO3).
 *
 * INA219 register map (all registers 16-bit big-endian):
 *
 *   0x00  Configuration     RW  reset / range / ADC resolution
 *   0x01  Shunt Voltage      R  signed 16-bit, LSB = 10 µV (PGA=÷8 default)
 *   0x02  Bus Voltage        R  bits[15:3] × 4 mV; bit0 = CNVR, bit1 = OVF
 *   0x03  Power              R  unsigned 16-bit, LSB = Power_LSB (= 20×I_LSB)
 *   0x04  Current            R  signed 16-bit, LSB = Current_LSB (set by Cal)
 *   0x05  Calibration       RW  Cal = trunc(0.04096 / (I_LSB × R_shunt))
 *
 * Calibration strategy (Current_LSB = 0.1 mA):
 *   Cal = 0.04096 / (0.0001 × R_shunt_Ω)
 *   For R_shunt = 0.1 Ω (100 mΩ): Cal = 4096 = 0x1000
 *   Power_LSB = 20 × 0.1 mA = 2 mW
 */

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#include <uk/print.h>
#include <uk/i2c.h>
#include <uk/ina219.h>

/* =========================================================================
 * INA219 register addresses
 * ========================================================================= */

#define INA219_REG_CONFIG    0x00u
#define INA219_REG_SHUNT_V   0x01u
#define INA219_REG_BUS_V     0x02u
#define INA219_REG_POWER     0x03u
#define INA219_REG_CURRENT   0x04u
#define INA219_REG_CALIB     0x05u

/*
 * Default configuration word:
 *   BRNG  = 1  → bus voltage range 32 V
 *   PGA   = 11 → shunt PGA = ÷8 (±320 mV full-scale)
 *   BADC  = 0011 → 12-bit / 532 µs conversion
 *   SADC  = 0011 → 12-bit / 532 µs conversion
 *   MODE  = 111 → shunt+bus, continuous
 * Binary: 0 01 11 0011 0011 111  = 0x399F
 */
#define INA219_CONFIG_DEFAULT 0x399Fu

/* =========================================================================
 * Driver state
 * ========================================================================= */

static uint8_t  _addr;
static uint32_t _current_lsb_ua;  /* Current LSB in microamps (= 100 µA for default) */
static uint32_t _power_lsb_mw;    /* Power  LSB in milliwatts (= 2 mW for default)   */

/* =========================================================================
 * Helpers
 * ========================================================================= */

/* Write a 16-bit value to an INA219 register (big-endian). */
static int _reg_write(uint8_t reg, uint16_t value)
{
    uint8_t buf[3] = {
        reg,
        (uint8_t)(value >> 8),
        (uint8_t)(value & 0xFFu)
    };
    return uk_i2c_write(_addr, buf, 3);
}

/*
 * Read a 16-bit value from an INA219 register (big-endian).
 *
 * Split into two separate transactions instead of a repeated-START
 * uk_i2c_write_read(): the INA219 retains the register pointer after the
 * write, so a plain read immediately following is equivalent and more
 * compatible with the BCM BSC polled implementation.
 *
 * Returns 0 on success; the raw 16-bit word is written to *out.
 */
static int _reg_read(uint8_t reg, uint16_t *out)
{
    uint8_t rbuf[2];
    int rc;

    /* Step 1: set the register pointer */
    rc = uk_i2c_write(_addr, &reg, 1);
    if (rc)
        return rc;

    /* Step 2: read 2 bytes (big-endian) */
    rc = uk_i2c_read(_addr, rbuf, 2);
    if (rc)
        return rc;

    *out = ((uint16_t)rbuf[0] << 8) | rbuf[1];
    return 0;
}

/* =========================================================================
 * Public API
 * ========================================================================= */

int uk_ina219_init(uint8_t addr7, uint32_t r_shunt_mohm)
{
    int rc;
    uint16_t cal;

    _addr = addr7;

    /*
     * Compute calibration register.
     *
     * Target Current_LSB = 0.1 mA = 100 µA.
     * Cal = trunc(0.04096 / (I_LSB_A × R_shunt_Ω))
     *     = trunc(40960000 / (100 × r_shunt_mohm))   [integer arithmetic, nA/mΩ]
     *     = 40960000 / (100 × r_shunt_mohm)
     *
     * For r_shunt_mohm = 100: Cal = 40960000 / 10000 = 4096 = 0x1000
     */
    if (r_shunt_mohm == 0)
        r_shunt_mohm = 100;   /* default: 100 mΩ = 0.1 Ω */

    cal = (uint16_t)(40960000UL / (100UL * r_shunt_mohm));
    _current_lsb_ua = 100;             /* 100 µA per LSB */
    _power_lsb_mw   = 2;               /* 20 × 0.1 mA = 2 mW per LSB */

    /* Configure: 32 V range, PGA ÷8, 12-bit ADC, continuous */
    rc = _reg_write(INA219_REG_CONFIG, INA219_CONFIG_DEFAULT);
    if (rc) {
        uk_pr_err("ina219: config write failed (%d)\n", rc);
        return -1;
    }

    /* Program calibration */
    rc = _reg_write(INA219_REG_CALIB, cal);
    if (rc) {
        uk_pr_err("ina219: calibration write failed (%d)\n", rc);
        return -1;
    }

    uk_pr_info("ina219: ready at I2C 0x%02x, shunt %u mΩ, Cal=0x%04x\n",
               addr7, r_shunt_mohm, cal);
    return 0;
}

int32_t uk_ina219_read_bus_mv(void)
{
    uint16_t raw;
    if (_reg_read(INA219_REG_BUS_V, &raw))
        return -1;
    /*
     * Bits[15:3] hold the voltage; bits[1:0] are status flags.
     * LSB = 4 mV.
     */
    return (int32_t)((raw >> 3) * 4);
}

int32_t uk_ina219_read_shunt_uv(void)
{
    uint16_t raw;
    if (_reg_read(INA219_REG_SHUNT_V, &raw))
        return INT32_MIN;
    /*
     * Signed 16-bit value; LSB = 10 µV with PGA=÷8 default.
     */
    return (int32_t)(int16_t)raw * 10;
}

int32_t uk_ina219_read_current_ma(void)
{
    uint16_t raw;
    if (_reg_read(INA219_REG_CURRENT, &raw))
        return INT32_MIN;
    /*
     * Signed 16-bit; LSB = Current_LSB = 100 µA = 0.1 mA.
     * Return milliamps: raw × 100 µA / 1000 = raw / 10.
     */
    int32_t raw_signed = (int32_t)(int16_t)raw;
    /* Avoid division truncation artefacts with a sign-preserving divide. */
    if (raw_signed >= 0)
        return  (int32_t)((uint32_t) raw_signed * _current_lsb_ua / 1000u);
    else
        return -(int32_t)((uint32_t)(-raw_signed) * _current_lsb_ua / 1000u);
}

uint32_t uk_ina219_read_power_mw(void)
{
    uint16_t raw;
    if (_reg_read(INA219_REG_POWER, &raw))
        return UINT32_MAX;
    /* Unsigned; LSB = Power_LSB = 2 mW. */
    return (uint32_t)raw * _power_lsb_mw;
}
