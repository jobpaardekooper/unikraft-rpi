/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/bus_space.h shim.
 *
 * bus_space_read_4 / bus_space_write_4 → volatile 32-bit MMIO.
 * On Unikraft/RPi3 the peripheral physical address space is identity-mapped,
 * so the bus_space_handle_t IS the virtual address.
 */
#pragma once
#include <stdint.h>

typedef uintptr_t bus_space_tag_t;
typedef uintptr_t bus_space_handle_t;

static inline uint32_t
bus_space_read_4(bus_space_tag_t t __attribute__((unused)),
                 bus_space_handle_t h,
                 unsigned long offset)
{
    return *(volatile uint32_t *)(h + offset);
}

static inline void
bus_space_write_4(bus_space_tag_t t __attribute__((unused)),
                  bus_space_handle_t h,
                  unsigned long offset,
                  uint32_t val)
{
    *(volatile uint32_t *)(h + offset) = val;
}

/*
 * bus_space_read_multi_4 / bus_space_write_multi_4
 *
 * Used by bcm_sdhci_read_multi_4 / bcm_sdhci_write_multi_4 for PIO
 * data FIFO transfers.  count is in 32-bit words.
 */
static inline void
bus_space_read_multi_4(bus_space_tag_t t __attribute__((unused)),
                       bus_space_handle_t h,
                       unsigned long offset,
                       uint32_t *datap, size_t count)
{
    volatile uint32_t *reg = (volatile uint32_t *)(h + offset);
    while (count--)
        *datap++ = *reg;
}

static inline void
bus_space_write_multi_4(bus_space_tag_t t __attribute__((unused)),
                        bus_space_handle_t h,
                        unsigned long offset,
                        const uint32_t *datap, size_t count)
{
    volatile uint32_t *reg = (volatile uint32_t *)(h + offset);
    while (count--)
        *reg = *datap++;
}
