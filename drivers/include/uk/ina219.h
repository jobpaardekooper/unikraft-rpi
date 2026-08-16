/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * uk/ina219.h — Public API for the INA219 current/power monitor driver.
 *
 * The INA219 is a 12-bit I2C current/power monitor.  It measures shunt and
 * bus voltage; current and power are derived from a programmable calibration
 * register.
 *
 * Default I2C addresses (A0/A1 pins):
 *   0x40  — A1=GND, A0=GND  (most common — both address pins to GND)
 *   0x41  — A1=GND, A0=VCC
 *   0x44  — A1=VCC, A0=GND
 *   0x45  — A1=VCC, A0=VCC
 *
 * uk_gpio_init(), GPIO 2/3 set to ALT0, and uk_i2c_init() must be called
 * before uk_ina219_init().
 *
 * Default calibration targets a 0.1 Ω shunt with Current_LSB = 0.1 mA.
 * Call uk_ina219_init() with r_shunt_mohm = 100 (100 mΩ = 0.1 Ω) for this
 * typical breakout board configuration.
 */
#pragma once
#include <stdint.h>

/*
 * uk_ina219_init — initialise the INA219 and program the calibration register.
 *
 *   addr7        7-bit I2C address (e.g. 0x40)
 *   r_shunt_mohm Shunt resistor value in milliohms (e.g. 100 for 0.1 Ω)
 *
 * The calibration is computed for Current_LSB = 0.1 mA, giving:
 *   Cal = 0.04096 / (0.0001 A × r_shunt_Ω)
 *
 * Returns 0 on success, -1 if the I2C write fails.
 */
int uk_ina219_init(uint8_t addr7, uint32_t r_shunt_mohm);

/*
 * uk_ina219_read_bus_mv — read the bus voltage.
 *
 * Returns the bus voltage in millivolts (0–32 000 mV).
 * Returns -1 on I2C error.
 */
int32_t uk_ina219_read_bus_mv(void);

/*
 * uk_ina219_read_shunt_uv — read the shunt (differential) voltage.
 *
 * Returns the shunt voltage in microvolts (signed, ±320 000 µV at PGA=÷8).
 * Returns INT32_MIN on I2C error.
 */
int32_t uk_ina219_read_shunt_uv(void);

/*
 * uk_ina219_read_current_ma — read the current through the shunt.
 *
 * Requires uk_ina219_init() to have been called first (calibration register
 * must be non-zero).  Current_LSB = 0.1 mA.
 *
 * Returns current in milliamps (signed).
 * Returns INT32_MIN on I2C error.
 */
int32_t uk_ina219_read_current_ma(void);

/*
 * uk_ina219_read_power_mw — read the power calculated by the INA219.
 *
 * Power_LSB = 20 × Current_LSB = 2 mW.
 *
 * Returns power in milliwatts (unsigned).
 * Returns UINT32_MAX on I2C error.
 */
uint32_t uk_ina219_read_power_mw(void);
