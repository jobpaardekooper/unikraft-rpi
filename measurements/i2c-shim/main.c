/* I2C (BSC) shim smoke-test for Unikraft on Raspberry Pi 3
 *
 * Tests:
 *   1. GPIO init  — GPIO driver must be up first (I2C depends on it)
 *   2. GPIO 2/3   — configure SDA/SCL pins as ALT0 (I2C1 function)
 *   3. I2C init   — run DEVICE_PROBE + DEVICE_ATTACH via KOBJ shim
 *   4. Scan       — try slave addresses 0x08–0x77; print responding ones
 *   5. Read reg   — if 0x48 (ADS1115 / PCF8563 / common address) responds,
 *                   do a write_read to read its first register
 *
 * Wiring:
 *   SDA  → GPIO 2 (pin 3)   SCL  → GPIO 3 (pin 5)
 *   Pull-ups: 4.7 kΩ to 3.3 V on each line (many breakout boards include these)
 *
 * Without any device connected, test 4 will find no slaves (all NACK) which
 * is normal — a successful scan with no devices is still a PASS because it
 * confirms the bus initialised and the controller is generating START/STOP
 * sequences without hanging.
 */

#include <stdio.h>
#include <stdint.h>
#include <uk/print.h>
#include <uk/gpio.h>
#include <uk/i2c.h>

int main(void)
{
    int ret;
    int found = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== I2C shim smoke-test start ===\n");

    /* 1. GPIO init (required by I2C config dependency) */
    ret = uk_gpio_init();
    if (ret != 0) {
        uk_pr_err("FAIL: uk_gpio_init() = %d\n", ret);
        return 1;
    }
    printf("PASS: GPIO driver initialised\n");

    /* 2. Configure GPIO 2 (SDA) and GPIO 3 (SCL) as ALT0 = I2C1 */
    uk_gpio_set_func(2, UK_GPIO_FUNC_ALT0);
    uk_gpio_set_func(3, UK_GPIO_FUNC_ALT0);
    printf("INFO: GPIO 2/3 configured as ALT0 (I2C1 SDA/SCL)\n");

    /* 3. I2C init */
    ret = uk_i2c_init();
    if (ret != 0) {
        uk_pr_err("FAIL: uk_i2c_init() = %d\n", ret);
        return 1;
    }
    printf("PASS: I2C (BSC1) driver initialised\n");

    /* 4. Bus scan: probe addresses 0x08–0x77 */
    printf("INFO: scanning I2C bus...\n");
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        uint8_t dummy;
        ret = uk_i2c_read(addr, &dummy, 1);
        if (ret == 0) {
            printf("INFO: device found at 0x%02x\n", addr);
            found++;
        }
    }

    if (found == 0)
        printf("INFO: no devices found (expected if bus is unloaded)\n");
    else
        printf("PASS: found %d device(s) on bus\n", found);

    /* 5. If 0x48 responds, read its first register (works for ADS1115,
     *    TMP102, PCF8563, etc. which all use 0x48 as a common address). */
    {
        uint8_t reg_addr = 0x00;
        uint8_t reg_val[2] = { 0, 0 };
        ret = uk_i2c_write_read(0x48, &reg_addr, 1, reg_val, 2);
        if (ret == 0)
            printf("INFO: 0x48 reg[0] = 0x%02x%02x\n",
                   reg_val[0], reg_val[1]);
        else
            printf("INFO: 0x48 not present (ret=%d)\n", ret);
    }

    printf("=== I2C shim smoke-test done ===\n");
    return 0;
}
