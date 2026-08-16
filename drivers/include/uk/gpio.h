/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Public Unikraft GPIO API for the Raspberry Pi 3 (BCM2837).
 *
 * Applications include this header; the underlying controller is the
 * unmodified FreeBSD bcm2835_gpio.c driver wired in via a KOBJ shim layer.
 *
 * Usage example:
 *   uk_gpio_init();
 *   uk_gpio_set_func(4, UK_GPIO_FUNC_OUTPUT);
 *   uk_gpio_set(4, 1);
 */

#ifndef __UK_GPIO_H__
#define __UK_GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Pin function selectors (GPFSEL register encoding) ---- */
#define UK_GPIO_FUNC_INPUT   0
#define UK_GPIO_FUNC_OUTPUT  1
#define UK_GPIO_FUNC_ALT5    2
#define UK_GPIO_FUNC_ALT4    3
#define UK_GPIO_FUNC_ALT0    4
#define UK_GPIO_FUNC_ALT1    5
#define UK_GPIO_FUNC_ALT2    6
#define UK_GPIO_FUNC_ALT3    7

/* ---- Pull-up/pull-down modes ---- */
#define UK_GPIO_PUD_OFF   0
#define UK_GPIO_PUD_DOWN  1
#define UK_GPIO_PUD_UP    2

/**
 * uk_gpio_init - Initialise the GPIO controller.
 *
 * Calls DEVICE_PROBE and DEVICE_ATTACH on the FreeBSD bcm2835_gpio driver
 * via the KOBJ shim layer.  Must be called once before any other gpio call.
 *
 * @return  0 on success, negative errno on error.
 */
int uk_gpio_init(void);

/**
 * uk_gpio_set_func - Set the function of a GPIO pin.
 *
 * @param pin   GPIO pin number (0–53).
 * @param func  One of UK_GPIO_FUNC_*.
 */
void uk_gpio_set_func(unsigned int pin, unsigned int func);

/**
 * uk_gpio_set - Drive a GPIO output pin high or low.
 *
 * @param pin   GPIO pin number.
 * @param val   1 = high, 0 = low.
 */
void uk_gpio_set(unsigned int pin, int val);

/**
 * uk_gpio_get - Read the level of a GPIO pin.
 *
 * @param pin   GPIO pin number.
 * @return      1 if the pin is high, 0 if low.
 */
int uk_gpio_get(unsigned int pin);

/**
 * uk_gpio_set_pud - Configure the pull-up/pull-down resistor of a pin.
 *
 * @param pin   GPIO pin number.
 * @param pud   UK_GPIO_PUD_OFF, UK_GPIO_PUD_DOWN, or UK_GPIO_PUD_UP.
 */
void uk_gpio_set_pud(unsigned int pin, unsigned int pud);

#ifdef __cplusplus
}
#endif

#endif /* __UK_GPIO_H__ */
