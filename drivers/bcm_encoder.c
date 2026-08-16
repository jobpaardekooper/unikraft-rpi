/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm_encoder.c — Dual rotary encoder driver for Unikraft / RPi3.
 *
 * Native module; no FreeBSD shim involved.  Sits directly on the GPIO API
 * (uk_gpio_get / uk_gpio_set_func / uk_gpio_set_pud).
 *
 * Pin assignment
 * ──────────────
 *   ENC1  A=GPIO22  B=GPIO23  SW=GPIO24
 *   ENC2  A=GPIO25  B=GPIO26  SW=GPIO27
 *
 * Quadrature decoding
 * ───────────────────
 * The grey-code output of a KY-040 encoder cycles through four states per
 * physical detent:
 *
 *   CW  (right / down): AB = 11 → 10 → 00 → 01 → 11
 *   CCW (left  / up):   AB = 11 → 01 → 00 → 10 → 11
 *
 * A 16-entry lookup table indexed by (prev_AB << 2) | cur_AB maps each
 * 2→2 bit transition to {-1, 0, +1}.  The raw counts are accumulated and
 * an event is emitted every 4 raw steps so that one physical detent yields
 * exactly one ±1 event.
 *
 * Button debounce
 * ───────────────
 * The switch is active-low (pulled high by the internal pull-up).
 * A press is registered after DEBOUNCE_TICKS consecutive low samples; the
 * next event cannot fire until the button is released.
 */

#include <stdint.h>
#include <uk/gpio.h>
#include <uk/encoder.h>

/* =========================================================================
 * Pin map
 * ========================================================================= */

static const unsigned int _pin_a[2]  = { 22, 26 };
static const unsigned int _pin_b[2]  = { 23, 25 };
static const unsigned int _pin_sw[2] = { 24, 27 };

/* =========================================================================
 * Quadrature decoder state
 * ========================================================================= */

/*
 * Lookup table: index = (prev_AB << 2) | cur_AB
 *
 *   AB state encoding: bit1=A, bit0=B
 *
 *   CW  sequence: 11(3)→10(2)→00(0)→01(1)→11(3)
 *   CCW sequence: 11(3)→01(1)→00(0)→10(2)→11(3)
 */
static const int8_t _enc_table[16] = {
    /*        cur: 00  01  10  11  */
    /* prev 00 */   0, -1, +1,  0,
    /* prev 01 */  +1,  0,  0, -1,
    /* prev 10 */  -1,  0,  0, +1,
    /* prev 11 */   0, +1, -1,  0,
};

static uint8_t _prev_ab[2];     /* last AB state for each encoder */
static int8_t  _raw_count[2];   /* accumulated raw transitions     */

/* =========================================================================
 * Button debounce state
 * ========================================================================= */

#define DEBOUNCE_TICKS  6   /* ~30 ms at 5 ms poll interval */

static uint8_t _btn_low_cnt[2];  /* consecutive low-sample count */
static uint8_t _btn_armed[2];    /* 1 = waiting for release      */

/* =========================================================================
 * Public API
 * ========================================================================= */

void uk_encoder_init(void)
{
    for (int i = 0; i < 2; i++) {
        /* Set all three pins as inputs with internal pull-ups */
        uk_gpio_set_func(_pin_a[i],  UK_GPIO_FUNC_INPUT);
        uk_gpio_set_func(_pin_b[i],  UK_GPIO_FUNC_INPUT);
        uk_gpio_set_func(_pin_sw[i], UK_GPIO_FUNC_INPUT);

        uk_gpio_set_pud(_pin_a[i],  UK_GPIO_PUD_UP);
        uk_gpio_set_pud(_pin_b[i],  UK_GPIO_PUD_UP);
        uk_gpio_set_pud(_pin_sw[i], UK_GPIO_PUD_UP);

        /* Seed the previous state so the first poll doesn't produce a spike */
        uint8_t a = uk_gpio_get(_pin_a[i]) ? 1u : 0u;
        uint8_t b = uk_gpio_get(_pin_b[i]) ? 1u : 0u;
        _prev_ab[i]    = (uint8_t)((a << 1) | b);
        _raw_count[i]  = 0;
        _btn_low_cnt[i] = 0;
        _btn_armed[i]   = 0;
    }
}

int uk_encoder_poll(int enc)
{
    if (enc < 0 || enc > 1)
        return 0;

    uint8_t a   = uk_gpio_get(_pin_a[enc]) ? 1u : 0u;
    uint8_t b   = uk_gpio_get(_pin_b[enc]) ? 1u : 0u;
    uint8_t cur = (uint8_t)((a << 1) | b);

    uint8_t idx = (uint8_t)((_prev_ab[enc] << 2) | cur);
    int8_t  raw = _enc_table[idx];

    _prev_ab[enc] = cur;

    if (raw == 0)
        return 0;

    _raw_count[enc] = (int8_t)(_raw_count[enc] + raw);

    /* Emit an event every 4 raw steps (one full detent) */
    if (_raw_count[enc] >= 4) {
        _raw_count[enc] = 0;
        return +1;
    }
    if (_raw_count[enc] <= -4) {
        _raw_count[enc] = 0;
        return -1;
    }
    return 0;
}

int uk_encoder_button(int enc)
{
    if (enc < 0 || enc > 1)
        return 0;

    int sw_low = (uk_gpio_get(_pin_sw[enc]) == 0);   /* active-low */

    if (sw_low) {
        if (_btn_low_cnt[enc] < 255)
            _btn_low_cnt[enc]++;
    } else {
        _btn_low_cnt[enc] = 0;
        _btn_armed[enc]   = 0;   /* button released — ready for next press */
    }

    /* Fire once when debounce threshold is first reached */
    if (_btn_low_cnt[enc] == DEBOUNCE_TICKS && !_btn_armed[enc]) {
        _btn_armed[enc] = 1;
        return 1;
    }
    return 0;
}
