/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/gpio.h shim — GPIO pin capability flags and interrupt modes
 * used by bcm2835_gpio.c.
 */
#pragma once
#include <stdint.h>

#define GPIOMAXNAME         64

/* GPIO pin direction / drive flags */
#define GPIO_PIN_INPUT      0x0001
#define GPIO_PIN_OUTPUT     0x0002
#define GPIO_PIN_OPENDRAIN  0x0004
#define GPIO_PIN_PUSHPULL   0x0008
#define GPIO_PIN_TRISTATE   0x0010
#define GPIO_PIN_PULLUP     0x0020
#define GPIO_PIN_PULLDOWN   0x0040
#define GPIO_PIN_INVIN      0x0080
#define GPIO_PIN_INVOUT     0x0100
#define GPIO_PIN_PULSATE    0x0200

/* GPIO interrupt trigger modes */
#define GPIO_INTR_CONFORM       0x00000000
#define GPIO_INTR_LEVEL_LOW     0x00000001
#define GPIO_INTR_LEVEL_HIGH    0x00000002
#define GPIO_INTR_EDGE_RISING   0x00000004
#define GPIO_INTR_EDGE_FALLING  0x00000008
#define GPIO_INTR_EDGE_BOTH     0x00000010
