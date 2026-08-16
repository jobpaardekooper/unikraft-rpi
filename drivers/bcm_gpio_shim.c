/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm_gpio_shim.c — API Translation Layer (shim component 1).
 *
 * Only includes <bcm_gpio_internal.h> for KOBJ/bus types (no FreeBSD shim
 * path needed) plus Unikraft headers for printing, allocation and IRQs.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include <uk/print.h>
#include <uk/alloc.h>
#include <raspi/irq.h>

#include <uk/gpio.h>
#include <bcm_gpio_internal.h>

/* 
 * BCM2837 GPIO hardware constants (peripherals datasheet §6.1)
 * */
#ifndef MMIO_BASE
#  define MMIO_BASE  0x3F000000UL
#endif
#define GPIO_PHYS_BASE  (MMIO_BASE + 0x200000UL)
#define GPIO_REG_SIZE   0xB4U

#define GPIO_IRQ_BANK0  49U
#define GPIO_IRQ_BANK1  50U

#define GPFSEL(bank)  (0x00U + (bank) * 4U)
#define GPSET(bank)   (0x1CU + (bank) * 4U)
#define GPCLR(bank)   (0x28U + (bank) * 4U)
#define GPLEV(bank)   (0x34U + (bank) * 4U)
#define GPPUD         0x94U
#define GPPUDCLK(b)   (0x98U + (b) * 4U)

static inline uint32_t gpio_rd(uint32_t off)
{
    return *(volatile uint32_t *)(GPIO_PHYS_BASE + off);
}
static inline void gpio_wr(uint32_t off, uint32_t val)
{
    *(volatile uint32_t *)(GPIO_PHYS_BASE + off) = val;
}
static inline void gpio_delay_cycles(unsigned n)
{
    while (n--) __asm__ volatile("nop");
}

/*
 * Static resources provided to the FreeBSD driver
 * */
static struct resource _res_mem = {
    .r_bustag    = 0,
    .r_bushandle = GPIO_PHYS_BASE,
    .r_type      = SYS_RES_MEMORY,
    .r_rid       = 0,
    .r_start     = GPIO_PHYS_BASE,
};
static struct resource _res_irq0 = {
    .r_bustag    = 0,
    .r_bushandle = 0,
    .r_type      = SYS_RES_IRQ,
    .r_rid       = 0,
    .r_start     = GPIO_IRQ_BANK0,
};
static struct resource _res_irq1 = {
    .r_bustag    = 0,
    .r_bushandle = 0,
    .r_type      = SYS_RES_IRQ,
    .r_rid       = 1,
    .r_start     = GPIO_IRQ_BANK1,
};

/*
 * bus_alloc_resources / bus_release_resources
 * */
int bus_alloc_resources(device_t dev __attribute__((unused)),
                        struct resource_spec *spec,
                        struct resource **res)
{
    int i, irq_idx = 0;
    for (i = 0; spec[i].type != -1; i++) {
        if (spec[i].type == SYS_RES_MEMORY)
            res[i] = &_res_mem;
        else if (spec[i].type == SYS_RES_IRQ)
            res[i] = (irq_idx++ == 0) ? &_res_irq0 : &_res_irq1;
        else
            res[i] = NULL;
    }
    return 0;
}

void bus_release_resources(device_t dev __attribute__((unused)),
                           struct resource_spec *spec __attribute__((unused)),
                           struct resource **res __attribute__((unused))) {}

/* bus_setup_intr / bus_teardown_intr are defined in bus_stubs.c */

/*
 * Static device instance used for KOBJ dispatch
 * */
static struct device shim_gpio_device = {
    .d_softc    = NULL,
    .d_methods  = NULL,
    .d_nameunit = "gpio0",
};

/*
 * uk_gpio_init
 * */
int uk_gpio_init(void)
{
    int rc;

    if (shim_gpio_driver_reg.sdr_methods == NULL) {
        uk_pr_err("uk_gpio_init: constructor did not run\n");
        return -ENXIO;
    }

    shim_gpio_device.d_methods = shim_gpio_driver_reg.sdr_methods;

    shim_gpio_device.d_softc =
        uk_calloc(uk_alloc_get_default(), 1,
                  shim_gpio_driver_reg.sdr_softc_size);
    if (!shim_gpio_device.d_softc) {
        uk_pr_err("uk_gpio_init: failed to allocate softc\n");
        return -ENOMEM;
    }

    rc = DEVICE_PROBE(&shim_gpio_device);
    if (rc != 0) {
        uk_pr_err("uk_gpio_init: DEVICE_PROBE = %d\n", rc);
        goto fail;
    }

    rc = DEVICE_ATTACH(&shim_gpio_device);
    if (rc != 0) {
        uk_pr_err("uk_gpio_init: DEVICE_ATTACH = %d\n", rc);
        goto fail;
    }

    uk_pr_info("uk_gpio_init: BCM2835/7 GPIO driver initialised\n");
    return 0;

fail:
    uk_free(uk_alloc_get_default(), shim_gpio_device.d_softc);
    shim_gpio_device.d_softc = NULL;
    return -ENXIO;
}

/*
 * Direct MMIO GPIO operations
 * */
void uk_gpio_set_func(unsigned int pin, unsigned int func)
{
    uint32_t bank   = pin / 10;
    uint32_t offset = (pin - bank * 10) * 3;
    uint32_t reg    = gpio_rd(GPFSEL(bank));
    reg &= ~(7U << offset);
    reg |=  (func & 7U) << offset;
    gpio_wr(GPFSEL(bank), reg);
}

void uk_gpio_set(unsigned int pin, int val)
{
    uint32_t bank = pin / 32;
    uint32_t mask = 1U << (pin % 32);
    gpio_wr(val ? GPSET(bank) : GPCLR(bank), mask);
}

int uk_gpio_get(unsigned int pin)
{
    uint32_t bank = pin / 32;
    uint32_t mask = 1U << (pin % 32);
    return (gpio_rd(GPLEV(bank)) & mask) ? 1 : 0;
}

void uk_gpio_set_pud(unsigned int pin, unsigned int pud)
{
    uint32_t bank = pin / 32;
    uint32_t mask = 1U << (pin % 32);
    gpio_wr(GPPUD, pud);
    gpio_delay_cycles(150);
    gpio_wr(GPPUDCLK(bank), mask);
    gpio_delay_cycles(150);
    gpio_wr(GPPUD, 0);
    gpio_wr(GPPUDCLK(bank), 0);
}
