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

#ifndef __RASPI_IRQ_H__
#define __RASPI_IRQ_H__

#include <uk/intctlr.h>
#include <raspi/sysregs.h>
#include <raspi/barriers.h>
#include <raspi/mmio.h>

// Platform-level IRQ IDs (the old 4 you already expose).
#define IRQ_ID_ARM_GENERIC_TIMER             0
#define IRQ_ID_RASPI_ARM_SIDE_TIMER          1
#define IRQ_ID_RASPI_USB                     2
#define IRQ_ID_RASPI_ARM_SYSTEM_TIMER_IRQ_3  3
#define RPI_HWIRQ_MB_RUN 4
#define RPI_HWIRQ_MB_WAKE 5
#define IRQS_MAX                             64

// Hardware IRQ lines in the Pi’s interrupt controller
#define RPI_HWIRQ_ARM_GENERIC_TIMER          63
#define RPI_HWIRQ_ARM_SIDE_TIMER            64
#define RPI_HWIRQ_RASPI_USB                  9
#define RPI_HWIRQ_ARM_SYSTEM_TIMER_IRQ_3     3

#ifndef MMIO_BASE
#define MMIO_BASE  0x3F000000
#endif

#define LOCAL_INTC_BASE   0x40000000UL
#define IRQ_SRC_BASE 0x40000060
#define MBOX0_RDCLR_BASE 0x400000C0

#define CORE0_MBOX_IRQCNTL   0x50
#define CORE1_MBOX_IRQCNTL   0x54
#define CORE2_MBOX_IRQCNTL   0x58
#define CORE3_MBOX_IRQCNTL   0x5C

#define QA7_MB0(c)  (0x40000080u + ((c) << 4))  /* mailbox-0 SET */
#define QA7_MB1(c)  (0x40000084u + ((c) << 4))  /* mailbox-1 SET: arg low */
#define QA7_MB2(c)  (0x40000088u + ((c) << 4))  /* mailbox-2 SET: arg high */

#define INT_SRC_MBOX0   (1U << 4)    /* Mailbox 0 pending bit in COREn_IRQ_SOURCE */

#define IRQ_BASIC_PENDING	((volatile __u32 *)(MMIO_BASE+0x0000B200))
#define IRQ_PENDING_1		((volatile __u32 *)(MMIO_BASE+0x0000B204))
#define IRQ_PENDING_2		((volatile __u32 *)(MMIO_BASE+0x0000B208))
#define FIQ_CONTROL			((volatile __u32 *)(MMIO_BASE+0x0000B20C))
#define ENABLE_IRQS_1		((volatile __u32 *)(MMIO_BASE+0x0000B210))
#define ENABLE_IRQS_2		((volatile __u32 *)(MMIO_BASE+0x0000B214))
#define ENABLE_BASIC_IRQS	((volatile __u32 *)(MMIO_BASE+0x0000B218))
#define DISABLE_IRQS_1		((volatile __u32 *)(MMIO_BASE+0x0000B21C))
#define DISABLE_IRQS_2		((volatile __u32 *)(MMIO_BASE+0x0000B220))
#define DISABLE_BASIC_IRQS	((volatile __u32 *)(MMIO_BASE+0x0000B224))

#define IRQS_BASIC_ARM_TIMER_IRQ	(1 << 0)

#define IRQS_1_SYSTEM_TIMER_IRQ_0	(1 << 0)
#define IRQS_1_SYSTEM_TIMER_IRQ_1	(1 << 1)
#define IRQS_1_SYSTEM_TIMER_IRQ_2	(1 << 2)
#define IRQS_1_SYSTEM_TIMER_IRQ_3	(1 << 3)
#define IRQS_1_USB_IRQ				(1 << 9)

typedef int (*irq_handler_func_t)(void *);


/* The platform-level functions that the rest of the code calls
 * to register and init interrupts. 
 */
int ukplat_irq_register(unsigned long irq, irq_handler_func_t func, void *arg);
int ukplat_irq_init(void);

/* This function is called by the exception/IRQ vector
 * and passes the regs and hardware line to uk_intctlr. 
 */
void ukplat_irq_handle(struct __regs *regs);



/* Mailbox-0 “set” register for core n (n==0...3) */
static inline uintptr_t MAILBOX0_SET(uint32_t n)
{
    return LOCAL_INTC_BASE + 0x80 + (n * 0x10);
}

/**
 * send_ipi() — kick core n out of WFI by setting mailbox 0
 * n: target core index (0–3)
 */
static inline void send_ipi(uint32_t n)
{
    uintptr_t reg = MAILBOX0_SET(n);

    /* Ensure all prior memory accesses complete before we set the mailbox */
    __asm__ volatile("dsb sy" ::: "memory");

    /* Write any non-zero to bit 0 → mailbox IRQ line goes high */
    mmio_write(reg, 1);

    /* Make sure the write really hits the bus before we proceed */
    __asm__ volatile("dsb sy" ::: "memory");
}

#endif /* __RASPI_IRQ_H__ */