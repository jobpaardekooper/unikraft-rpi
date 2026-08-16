/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD dev/gpio/gpiobusvar.h shim.
 *
 * struct gpio_pin  — per-pin state (name, caps, flags).
 * driver_t         — defined in sys/module.h (included via sys/bus.h).
 * gpiobus_add_bus  — returns a dummy non-NULL device so bcm_gpio_attach
 *                    doesn't fail at "if (sc->sc_busdev == NULL) goto fail".
 */
#pragma once
#include <stdint.h>
#include <sys/gpio.h>
#include <sys/bus.h>

/* Per-pin descriptor stored in bcm_gpio_softc.sc_gpio_pins[] */
struct gpio_pin {
    uint32_t gp_pin;                /* GPIO pin number */
    uint32_t gp_caps;               /* capability flags */
    uint32_t gp_flags;              /* direction / pull flags */
    char     gp_name[GPIOMAXNAME]; /* human-readable name */
};

/*
 * gpiobus_add_bus — create the GPIO bus child device.
 * We return a pointer to a static dummy device; the shim never actually
 * attaches bus children, so this just needs to be non-NULL.
 */
static inline device_t
gpiobus_add_bus(device_t dev __attribute__((unused)))
{
    /* Return the parent device itself as a placeholder bus device. */
    return dev;
}
