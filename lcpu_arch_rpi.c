/* SPDX-License-Identifier: BSD-3-Clause */
/*
* Authors: Wei Chen <wei.chen@arm.com> (minimal version)
* Adapted for SMP on Raspberry Pi by <Octavian Mihaila mhoctavianiulian@gmail.com>
*
* This file implements the architecture-specific LCPU routines for the
* Raspberry Pi platform in Unikraft. It provides the hooks that the common
* SMP code (unikraft/plat/common/lcpu.c) calls:
*    - lcpu_arch_init()
*    - lcpu_arch_idx()
*    - lcpu_arch_id()
*    - lcpu_arch_mp_init()
*    - lcpu_arch_start()
*    - lcpu_arch_wakeup()
*    - lcpu_arch_jump_to()
*/

#include <uk/plat/common/lcpu.h>
#include <uk/print.h>
#include <uk/atomic.h>
#include <raspi/barriers.h>
#include <raspi/sysregs.h>
#include <stdint.h>
#include <errno.h>
#include <arm/irq.h>
#include <raspi/irq.h>

/* Helper function for reading the current value of MPIDR_EL1. */
static inline uint64_t read_mpidr_el1(void)
{
    uint64_t mpidr;
    asm volatile("mrs %0, mpidr_el1" : "=r" (mpidr));
    return mpidr;
}

/**
 *  Returns the sequential index (0…n-1) for the current core.
 *  Mask off the lower two bits of MPIDR_EL1.
 */
__lcpuidx lcpu_arch_idx(void)
{
    uint64_t mpidr = read_mpidr_el1();
    return (__lcpuidx)(mpidr & 0x3);
}

/**
 *  Returns the hardware (physical) ID of the current CPU.
 */
__lcpuid lcpu_arch_id(void)
{
    return read_mpidr_el1();
}

/**
 * lcpu_arch_init:
 *  Perform any architecture-specific initialization on the current core.
 *  For example, setting up per-core registers, local timers, or caches.
 *  For now, we assume nothing special is needed.
 *
 * @param this_lcpu Pointer to the current LCPU structure.
 * @return 0 on success, negative error code on failure.
 */
int lcpu_arch_init(struct lcpu *this_lcpu)
{
    return 0;
}

/**
 * lcpu_arch_mp_init:
 *  Called on the BSP to discover and allocate secondary cores.
 * 
 * @param arg Optional parameter from boot code.
 * @return 0 on success.
 */
int lcpu_arch_mp_init(void *arg)
{
    for (__lcpuid id = 1; id < CONFIG_UKPLAT_LCPU_MAXCOUNT; id++) {
        struct lcpu *lc = lcpu_alloc(id);
    }

    DataSyncBarrier(); /* Make sure all cores see the new config */

    return 0;
}

/**
 * start_on_core:
 *  Helper function to send a function to a secondart core that is in a spin loop.
 *
 * @param core The core to start the function on.
 * @param fn The function to execute.
 * @param arg The argument to pass to the function.
 */

void start_on_core(unsigned core, void (*fn)(void *), void *arg)
{
    uint64_t a = (uint64_t)(uintptr_t)arg;

    DataSyncBarrier();

    /* 2) Split the 64-bit argument across two 32-bit mailboxes */
    *(volatile uint32_t *)QA7_MB1(core) = (uint32_t)(a & 0xFFFFFFFF);
    *(volatile uint32_t *)QA7_MB2(core) = (uint32_t)(a >> 32);

    /* 3) Poke the fn-pointer into mailbox-0 (low 32 bits only) */
    *(volatile uint32_t *)QA7_MB0(core) = (uint32_t)(uintptr_t)fn;

    DataSyncBarrier();
    SendEvent();
}

/**
 * flush_to_ram:
 *  Flushes the cache lines for the given address range to RAM.
 *
 * @param addr The address to flush.
 * @param len The length of the range to flush.
 */
static inline void flush_to_ram(const void *addr, size_t len)
{
    uintptr_t p = (uintptr_t)addr & ~(CACHE_LINE_SIZE-1);
    uintptr_t end = (uintptr_t)addr + len;

    for (; p < end; p += CACHE_LINE_SIZE)
            __asm__ volatile("dc civac, %0" :: "r"(p) : "memory");
    
    DataSyncBarrier();
}

/**
 * lcpu_arch_start:
 *  Start a secondary core by passing it an entry function
 *
 * @param lcpu Pointer to the LCPU structure for the core to start.
 * @param flags Flags for starting the core (unused).
 * @return 0 on success, negative error code on failure.
 */
int lcpu_arch_start(struct lcpu *lcpu, unsigned long flags) {
    // Before starting the core, make sure everything is in order.
    flush_to_ram(lcpu, sizeof(*lcpu));

    start_on_core(lcpu->id, lcpu_entry_default, lcpu);

    return 0;
}

/**
 * lcpu_arch_run:
 *  Enqueue a function to be executed on the specified core.
 *
 * @param lcpu Pointer to the LCPU structure for the core to run on.
 * @param fn Pointer to the function to execute.
 * @param flags Flags for running the function (unused).
 * @return 0 on success, negative error code on failure.
 */
int lcpu_arch_run(struct lcpu *lcpu, const struct ukplat_lcpu_func *fn, unsigned long flags)
{
    int rc = lcpu_fn_enqueue(lcpu, fn);
    if (rc) return rc;

    // Make sure the fn object is visible before sending the IPI */
    DataSyncBarrier();

    send_ipi(lcpu->id);

    return 0;
}

/**
 * lcpu_arch_wakeup:
 *  Wake up a core that is in a low-power state.
 *
 * @param lcpu Pointer to the LCPU structure for the core to wake up.
 * @return 0 on success, negative error code on failure.
 */
int lcpu_arch_wakeup(struct lcpu *lcpu)
{
    send_ipi(lcpu->id);

    return 0;
}

/**
 * lcpu_arch_jump_to:
 *  Switches the CPU to a new stack and jumps to the given entry point.
 *  This function never returns.
 *
 * @param sp The new stack pointer.
 * @param entry The entry function to jump to.
 */
void __noreturn lcpu_arch_jump_to(void *sp, ukplat_lcpu_entry_t entry)
{
    asm volatile (
        "mov sp, %0\n"  // Set the new stack pointer
        "br %1\n"       // Branch to the entry function
        :
        : "r" (sp), "r" (entry)
        : "sp"
    );
    __builtin_unreachable();
}

void ukplat_lcpu_enable_irq(void)
{
    local_irq_enable();
}

void ukplat_lcpu_disable_irq(void)
{
    local_irq_disable();
}

unsigned long ukplat_lcpu_save_irqf(void)
{
    unsigned long flags;

    local_irq_save(flags);

    return flags;
}

void ukplat_lcpu_restore_irqf(unsigned long flags)
{
    local_irq_restore(flags);
}

int ukplat_lcpu_irqs_disabled(void)
{
    return irqs_disabled();
}

void ukplat_lcpu_irqs_handle_pending(void)
{
    // TODO
}
