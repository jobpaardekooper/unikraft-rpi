/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/mutex.h shim.
 *
 * mtx_lock_spin / mtx_unlock_spin  → save/restore IRQ flags (critical section).
 * On AArch64 Unikraft we use local_irq_save / local_irq_restore assembled
 * inline.  The flags are stored inside struct mtx so the matching unlock
 * can restore exactly the state captured by lock.
 */
#pragma once
#include <stdint.h>

#define MTX_SPIN    0x1
#define MTX_DEF     0x0

struct mtx {
    unsigned long irq_flags;
    int           initialised;
};

/* Save DAIF and disable IRQs (AArch64) */
static inline unsigned long __mtx_save_irqf(void)
{
    unsigned long flags;
    __asm__ volatile(
        "mrs %0, daif\n\t"
        "msr daifset, #2\n\t"   /* set IRQ mask bit */
        : "=r"(flags)
        :
        : "memory");
    return flags;
}

static inline void __mtx_restore_irqf(unsigned long flags)
{
    __asm__ volatile(
        "msr daif, %0\n\t"
        :
        : "r"(flags)
        : "memory");
}

static inline void mtx_init(struct mtx *m,
    const char *name __attribute__((unused)),
    const char *type __attribute__((unused)),
    int opts __attribute__((unused)))
{
    m->irq_flags   = 0;
    m->initialised = 1;
}

static inline void mtx_destroy(struct mtx *m) { m->initialised = 0; }

static inline void mtx_lock_spin(struct mtx *m)
{
    m->irq_flags = __mtx_save_irqf();
}

static inline void mtx_unlock_spin(struct mtx *m)
{
    __mtx_restore_irqf(m->irq_flags);
}

/* MA_OWNED assertion — no-op in shim */
#define mtx_assert(m, what)  do { (void)(m); (void)(what); } while (0)
#define MA_OWNED    0

/*
 * MTX_DEF (non-spin) lock/unlock — single-threaded cooperative shim.
 * No real contention is possible, so these are no-ops.
 */
static inline void mtx_lock(struct mtx *m __attribute__((unused)))   {}
static inline void mtx_unlock(struct mtx *m __attribute__((unused))) {}

/*
 * mtx_sleep — used by bcm_bsc_transfer() to wait for the ISR to set
 * BCM_I2C_DONE.  Since bus_setup_intr is a no-op and we bypass
 * bcm_bsc_transfer entirely in favour of our polled uk_i2c_write/read,
 * this function is dead code.  It is present only for linkage.
 */
static inline int
mtx_sleep(void *chan    __attribute__((unused)),
          struct mtx *m __attribute__((unused)),
          int    pri    __attribute__((unused)),
          const char *w __attribute__((unused)),
          int    timo   __attribute__((unused)))
{
    return 0;   /* never called from shim paths */
}

/*
 * wakeup — counterpart to mtx_sleep; called from bcm_bsc_intr() which
 * is also dead code in polled mode.
 */
static inline void wakeup(void *chan __attribute__((unused))) {}
