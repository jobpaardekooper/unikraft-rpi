/* GPIO shim smoke-test for Unikraft on Raspberry Pi 3
 *
 * Tests:
 *   1. Init     — run DEVICE_PROBE + DEVICE_ATTACH via KOBJ shim
 *   2. Output   — set GPIO 4 (pin 7) as output and drive it high then low
 *   3. Input    — read GPIO 17 (pin 11, driven LOW by GPIO 4 via jumper)
 *   4. Alt fn   — set GPIO 11 (SPI0 SCLK) to ALT0
 *   5. Loopback — drive GPIO 4 HIGH and confirm GPIO 17 reads 1
 *
 * Wiring: connect GPIO 4 (pin 7) to GPIO 17 (pin 11) with a jumper wire.
 */

#include <stdio.h>
#include <stdint.h>
#include <uk/print.h>
#include <uk/gpio.h>

int main(void)
{
    int ret, level;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== GPIO shim smoke-test start ===\n");

    /* 1. Init */
    ret = uk_gpio_init();
    if (ret != 0) {
        uk_pr_err("FAIL: uk_gpio_init() = %d\n", ret);
        return 1;
    }
    printf("PASS: GPIO driver initialised\n");

    /* 2. Output: drive GPIO 4 high */
    uk_gpio_set_func(4, UK_GPIO_FUNC_OUTPUT);
    uk_gpio_set(4, 1);
    printf("INFO: GPIO 4 set to OUTPUT HIGH\n");

    /* Drive low */
    uk_gpio_set(4, 0);
    printf("INFO: GPIO 4 set LOW\n");

    /* 3. Input: read GPIO 17 — GPIO 4 is LOW, jumper pulls 17 LOW too */
    uk_gpio_set_func(17, UK_GPIO_FUNC_INPUT);
    uk_gpio_set_pud(17, UK_GPIO_PUD_UP);
    level = uk_gpio_get(17);
    printf("INFO: GPIO 17 = %d (GPIO 4 is LOW, expected 0 via jumper)\n", level);
    if (level != 0)
        uk_pr_warn("WARN: expected 0, got %d — is the jumper connected?\n", level);

    /* 4. Alt function: GPIO 11 → SPI0 SCLK (ALT0), verified by GPFSEL readback */
    uk_gpio_set_func(11, UK_GPIO_FUNC_ALT0);
    {
        volatile uint32_t *gpfsel1 = (volatile uint32_t *)0x3F200004;
        int bits = (*gpfsel1 >> 3) & 0x7;  /* bits [5:3] = GPIO 11 function */
        printf("INFO: GPIO 11 GPFSEL1[5:3] = 0b%d%d%d (ALT0 = 0b100)\n",
               (bits >> 2) & 1, (bits >> 1) & 1, bits & 1);
        if (bits == 4)
            printf("PASS: GPIO 11 configured as ALT0 (SPI0 SCLK)\n");
        else
            uk_pr_err("FAIL: GPIO 11 GPFSEL readback got %d, expected 4\n", bits);
    }

    /* 5. Loopback: drive GPIO 4 HIGH and confirm GPIO 17 follows */
    uk_gpio_set_func(4, UK_GPIO_FUNC_OUTPUT);
    uk_gpio_set(4, 1);
    level = uk_gpio_get(17);
    printf("INFO: GPIO 4 HIGH → GPIO 17 = %d (expected 1)\n", level);
    if (level == 1)
        printf("PASS: loopback GPIO 4 → GPIO 17\n");
    else
        uk_pr_err("FAIL: loopback GPIO 4 → GPIO 17 (got %d)\n", level);

    printf("=== GPIO shim smoke-test done ===\n");
    return 0;
}
