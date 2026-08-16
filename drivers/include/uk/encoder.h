/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * uk/encoder.h — Public API for dual rotary encoder driver.
 *
 * Targets two KY-040-style rotary encoders connected directly to BCM2837
 * GPIO pins (pull-ups enabled in software):
 *
 *   ENC1 — "back" encoder
 *     A  → GPIO 22  (header pin 15)
 *     B  → GPIO 23  (header pin 16)
 *     SW → GPIO 24  (header pin 18)
 *
 *   ENC2 — "navigate / select" encoder
 *     A  → GPIO 25  (header pin 22)
 *     B  → GPIO 26  (header pin 37)
 *     SW → GPIO 27  (header pin 13)
 *
 * uk_gpio_init() must be called before uk_encoder_init().
 *
 * All functions are poll-based; call them from a tight loop at a fixed
 * interval (5–20 ms recommended) for best responsiveness.
 */
#pragma once
#include <stdint.h>

/* Encoder identifiers */
#define UK_ENC1  0   /* "back"            encoder */
#define UK_ENC2  1   /* "navigate/select" encoder */

/*
 * uk_encoder_init — configure encoder GPIO pins as inputs with pull-ups.
 * Must be called once after uk_gpio_init().
 */
void uk_encoder_init(void);

/*
 * uk_encoder_poll — read rotation since last call.
 *
 *   enc  UK_ENC1 or UK_ENC2
 *
 * Returns:
 *   +1  clockwise     (one detent right  — "down" in a menu)
 *   -1  counter-CW    (one detent left   — "up"   in a menu)
 *    0  no movement
 *
 * Based on a 4-step grey-code lookup; call at a steady interval so
 * bouncing transitions are absorbed between polls.
 */
int uk_encoder_poll(int enc);

/*
 * uk_encoder_button — edge-triggered button query.
 *
 *   enc  UK_ENC1 or UK_ENC2
 *
 * Returns 1 the first time the button is seen pressed after having been
 * released; 0 otherwise.  Built-in debounce: button must be stable for
 * ~30 ms (6 calls at 5 ms interval) before registering.
 */
int uk_encoder_button(int enc);
