/* SPDX-License-Identifier: BSD-3-Clause */
/* barriers.h: ARMv8 barrier/cache macros for Raspberry Pi 3B. */

#ifndef __RASPI_BARRIERS_H__
#define __RASPI_BARRIERS_H__

/* Invalidate entire I-cache to PoU */
#define InvalidateInstructionCache() \
	__asm__ volatile ("ic iallu" ::: "memory")

/* Flush entire branch predictor / TLB + ensure synchronization */
#define FlushBranchTargetCache() \
	__asm__ volatile ("dsb ish; tlbi vmalle1is; dsb ish; isb" ::: "memory")

/* Send Event – wake up any cores in WFE/WFI */
#define SendEvent() \
	__asm__ volatile ("sev" ::: "memory")

/* Instruction Synchronization Barrier */
#define InstructionSyncBarrier() \
	__asm__ volatile ("isb sy" ::: "memory")

/* Data Synchronization Barrier */
#define DataSyncBarrier() \
	__asm__ volatile ("dsb sy" ::: "memory")

/* Data Memory Barrier */
#define DataMemBarrier() \
	__asm__ volatile ("dmb sy" ::: "memory")

#endif /* __RASPI_BARRIERS_H__ */
