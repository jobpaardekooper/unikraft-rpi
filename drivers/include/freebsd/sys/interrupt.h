/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/interrupt.h shim.
 *
 * Provides the interrupt-source registration API and the intr_map_data
 * types that bcm2835_gpio.c uses for its PIC implementation.
 *
 * intr_isrc_dispatch → stub returning FILTER_STRAY (per-pin GPIO IRQ
 *   dispatch is not implemented; bank-level IRQs still reach the driver).
 * intr_isrc_register / intr_pic_register → succeed silently.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* Interrupt map data type tags */
#define INTR_MAP_DATA_FDT    1
#define INTR_MAP_DATA_GPIO   3

/* bus_setup_intr flags */
#define INTR_TYPE_MISC   0
#define INTR_TYPE_BIO    0x0010   /* block I/O interrupt (bcm2835_sdhci.c) */
#define INTR_MPSAFE      0

/* Filter return codes */
#define FILTER_HANDLED   1
#define FILTER_STRAY     0

/* Interrupt source (embedded in bcm_gpio_irqsrc) */
struct intr_irqsrc {
    uint32_t isrc_handlers;  /* number of registered handlers */
};

/* Generic interrupt map data (type-tagged union base) */
struct intr_map_data {
    int type;
};

/* FDT-style interrupt mapping: 2 cells (IRQ number + flags) */
struct intr_map_data_fdt {
    int      type;     /* = INTR_MAP_DATA_FDT */
    uint32_t ncells;
    uint32_t cells[4];
};

/* GPIO-style interrupt mapping */
struct intr_map_data_gpio {
    int      type;           /* = INTR_MAP_DATA_GPIO */
    uint32_t gpio_pin_num;
    uint32_t gpio_intr_mode;
};

/*
 * intr_isrc_dispatch — stub: per-pin GPIO interrupts not dispatched.
 * Returns FILTER_STRAY so the caller can mask the stray interrupt.
 */
static inline int
intr_isrc_dispatch(struct intr_irqsrc *isrc __attribute__((unused)),
                   void *frame __attribute__((unused)))
{
    return FILTER_STRAY;
}

/* Register an interrupt source — always succeeds in the shim. */
static inline int
intr_isrc_register(struct intr_irqsrc *isrc __attribute__((unused)),
                   void *dev __attribute__((unused)),
                   int flags __attribute__((unused)),
                   const char *fmt __attribute__((unused)), ...)
{
    return 0;
}

/* Register the GPIO controller as a PIC — return non-NULL to signal success. */
static inline void *
intr_pic_register(void *dev __attribute__((unused)),
                  uintptr_t xref __attribute__((unused)))
{
    return (void *)1;
}
