/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Santiago Pagani <santiagopagani@gmail.com>
 *
 * Copyright (c) 2020, NEC Laboratories Europe GmbH, NEC Corporation.
 *                     All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */

#include <uk/print.h>
#include <uk/essentials.h>
#include <uk/intctlr.h>
#include <uk/plat/lcpu.h>
#include <raspi/irq.h>
#include <raspi/time.h>
#include <raspi/raspi_info.h>
#include <arm/time.h>
#include <raspi/barriers.h>
#include <uspienv/interrupt.h>


static int rpi_configure_irq(struct uk_intctlr_irq *irq)
{
	/* No extra config needed for the Pi’s intc. */
	return 0;
}

static int rpi_fdt_xlat(const void *fdt, int nodeoffset,
						__u32 index, struct uk_intctlr_irq *irq)
{
	return -ENOTSUP; /* Not using FDT for this driver. */
}

static void rpi_mask_irq(unsigned int hwirq)
{
	if (hwirq == RPI_HWIRQ_ARM_SYSTEM_TIMER_IRQ_3) {
		*DISABLE_IRQS_1 = IRQS_1_SYSTEM_TIMER_IRQ_3;
	} else if (hwirq == RPI_HWIRQ_RASPI_USB) {
		*DISABLE_IRQS_1 = IRQS_1_USB_IRQ;
	} else if (hwirq == RPI_HWIRQ_ARM_SIDE_TIMER) {
		*DISABLE_BASIC_IRQS = IRQS_BASIC_ARM_TIMER_IRQ;
	} else if (hwirq == RPI_HWIRQ_ARM_GENERIC_TIMER) {
		uint64_t ctl = get_el0(cntv_ctl);
		ctl &= ~GT_TIMER_ENABLE; /* disable timer entirely */
		set_el0(cntv_ctl, ctl);
	}
}

static void rpi_unmask_irq(unsigned int hwirq)
{
	if (hwirq == RPI_HWIRQ_ARM_SYSTEM_TIMER_IRQ_3) {
		*ENABLE_IRQS_1 = IRQS_1_SYSTEM_TIMER_IRQ_3;
	} else if (hwirq == RPI_HWIRQ_RASPI_USB) {
		*ENABLE_IRQS_1 = IRQS_1_USB_IRQ;
	} else if (hwirq == RPI_HWIRQ_ARM_SIDE_TIMER) {
		*ENABLE_BASIC_IRQS = IRQS_BASIC_ARM_TIMER_IRQ;
		/* Clear + enable side-timer. */
		raspi_arm_side_timer_irq_clear();
		raspi_arm_side_timer_irq_enable();
	} else if (hwirq == RPI_HWIRQ_ARM_GENERIC_TIMER) {
		/* Re-enable the ARM generic timer */
		uint64_t ctl = get_el0(cntv_ctl);
		ctl |= GT_TIMER_ENABLE;
		set_el0(cntv_ctl, ctl);
	}
}

static struct uk_intctlr_driver_ops rpi_intctlr_ops = {
	.configure_irq = rpi_configure_irq,
	.fdt_xlat      = rpi_fdt_xlat,
	.mask_irq      = rpi_mask_irq,
	.unmask_irq    = rpi_unmask_irq,
};

/* The global descriptor for Pi intc driver */
static struct uk_intctlr_desc rpi_intctlr_desc = {
	.name = "rpi-intc",
	.ops  = &rpi_intctlr_ops,
};

int uk_intctlr_probe(void)
{
	return uk_intctlr_register(&rpi_intctlr_desc);
}

int ukplat_irq_register(unsigned long irq, irq_handler_func_t func, void *arg)
{
	if (!func) {
		uk_pr_err("ukplat_irq_register: invalid handler\n");
		return -1;
	}

	/* Translate from "platform IRQ" to actual hardware line: */
	unsigned int hwirq = 0;
	switch (irq) {
		case IRQ_ID_ARM_GENERIC_TIMER:
			hwirq = RPI_HWIRQ_ARM_GENERIC_TIMER;
			break;
		case IRQ_ID_RASPI_ARM_SIDE_TIMER:
			hwirq = RPI_HWIRQ_ARM_SIDE_TIMER;
			break;
		case IRQ_ID_RASPI_USB:
			hwirq = RPI_HWIRQ_RASPI_USB;
			break;
		case IRQ_ID_RASPI_ARM_SYSTEM_TIMER_IRQ_3:
			hwirq = RPI_HWIRQ_ARM_SYSTEM_TIMER_IRQ_3;
			break;
		default:
			uk_pr_crit("ukplat_irq_register: unsupported IRQ %lu\n", irq);
			return -1;
	}

	/*
	* Use the uk_intctlr library to register:
	* The hardware line number
	* The function pointer and arg
	*/
	return uk_intctlr_irq_register(hwirq, func, arg);
}

int ukplat_irq_init(void)
{
	/* Possibly flush caches, etc. */
	DataSyncBarrier();
	InvalidateInstructionCache();
	FlushBranchTargetCache();
	DataSyncBarrier();
	InstructionSyncBarrier();
	DataMemBarrier();

	/* Clear + disable all lines first: */
	*DISABLE_BASIC_IRQS = 0xFFFFFFFF;
	*DISABLE_IRQS_1     = 0xFFFFFFFF;
	*DISABLE_IRQS_2     = 0xFFFFFFFF;

	uk_intctlr_probe();
	uk_intctlr_init(NULL);

	/* 3) Re-enable CPU-level interrupts */
	ukplat_lcpu_enable_irq();

	return 0;
}

/*
* The main interrupt dispatcher.  Called from exception vector code.
* Instead of calling each handler array manually, the central library
* function uk_intctlr_irq_handle(regs, <line>) is called.
*/
void ukplat_irq_handle(struct __regs *regs)
{
	// Local per-core mailbox IPI (RPI_HWIRQ_MB_RUN)
	uint32_t core = lcpu_arch_idx();

	/* Read the per-core IRQ source register */
	uint32_t src = mmio_read(IRQ_SRC_BASE + core*4);

	/* INT_SRC_MBOX0 is bit-4 (0x10) in that src reg */
	if (src & INT_SRC_MBOX0) {
		/* clear the mailbox bit by writing ‘1’ to the RDCLR reg */
		mmio_write(MBOX0_RDCLR_BASE + core*0x10, 1);

		/* Dispatch to the “mailbox run” IRQ line */
		uk_intctlr_irq_handle(regs, RPI_HWIRQ_MB_RUN);
		return;
	}

	// Check “normal” lines 0..63 in the Pi’s intc.
	for (unsigned nIRQ = 0; nIRQ < IRQS_MAX; nIRQ++) {
		// Determine which "pending" register to read
		volatile uint32_t *pend_reg = (nIRQ < 32) ? IRQ_PENDING_1 : IRQ_PENDING_2;

		// Build the bitmask for our 'nIRQ' in that register
		uint32_t irq_mask = 1U << (nIRQ & 31);

		// Check if that bit is set
		if (*pend_reg & irq_mask) {
			// Found a pending interrupt => forward to library.
			uk_intctlr_irq_handle(regs, nIRQ);
			return;
		}
	}

	// Check the "basic" bit for the side timer
	__u32 irq_bits = *IRQ_BASIC_PENDING & *ENABLE_BASIC_IRQS;
	if (irq_bits & IRQS_BASIC_ARM_TIMER_IRQ) {
		uk_intctlr_irq_handle(regs, RPI_HWIRQ_ARM_SIDE_TIMER);
		return;
	}

	// Check the generic timer bit in CNTV_CTL
	if (get_el0(cntv_ctl) & GT_TIMER_IRQ_STATUS) {
		uk_intctlr_irq_handle(regs, RPI_HWIRQ_ARM_GENERIC_TIMER);
		return;
	}

	uk_pr_crit("ukplat_irq_handle: Unhandled IRQ\n");
	uk_pr_crit("IRQ_BASIC_PENDING: %u\n", *IRQ_BASIC_PENDING);
	uk_pr_crit("IRQ_PENDING_1: %u\n", *IRQ_PENDING_1);
	uk_pr_crit("IRQ_PENDING_2: %u\n", *IRQ_PENDING_2);
	uk_pr_crit("ENABLE_BASIC_IRQS: %u\n", *ENABLE_BASIC_IRQS);
	uk_pr_crit("ENABLE_IRQS_1: %u\n", *ENABLE_IRQS_1);
	uk_pr_crit("ENABLE_IRQS_2: %u\n", *ENABLE_IRQS_2);
	uk_pr_crit("DISABLE_BASIC_IRQS: %u\n", *DISABLE_BASIC_IRQS);
	uk_pr_crit("DISABLE_IRQS_1: %u\n", *DISABLE_IRQS_1);
	uk_pr_crit("DISABLE_IRQS_2: %u\n", *DISABLE_IRQS_2);
	uk_pr_crit("get_el0(cntv_ctl): %lu\n", get_el0(cntv_ctl));

	while (1);
} 

void show_invalid_entry_message(int type)
{
	uk_pr_crit("IRQ: %d\n", type);
}

void show_invalid_entry_message_el1_sync(uint64_t esr_el, uint64_t far_el)
{
	uk_pr_crit("ESR_EL1: %lx, FAR_EL1: %lx, SCTLR_EL1:%lx, ELR_EL1:%lx\n", esr_el, far_el, get_sctlr_el1(), get_elr_el1());
}

void ukplat_lcpu_halt_irq(void)
{
	UK_ASSERT(ukplat_lcpu_irqs_disabled());

	ukplat_lcpu_enable_irq();
	halt();
	ukplat_lcpu_disable_irq();
}
