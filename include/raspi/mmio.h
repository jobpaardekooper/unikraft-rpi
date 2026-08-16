#include <stdint.h>

/**
 * mmio_write — write a 32-bit value to a MMIO register
 * @addr: physical/alias address of the register
 * @value: 32-bit value to write
 *
 * This does a single, non-temporal store.
 */
static inline void mmio_write(uintptr_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

/**
 * mmio_read — read a 32-bit value from a MMIO register
 * @addr: physical/alias address of the register
 *
 * Returns the 32-bit contents of the register.
 */
static inline uint32_t mmio_read(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}
