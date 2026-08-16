/*
 * Copyright (C) 2018, bzt (bztsrc@github), https://github.com/bztsrc/raspi3-tutorial
 * Copyright (c) 2018, Sergey Matyukevich <https://github.com/s-matyukevich/raspberry-pi-os>
 *           (c) 2020, Santiago Pagani <santiagopagani@gmail.com>
 *
 * Portions of this file are adapted (with modifications) from:
 *     Leon de Boer (LdB-ECM), SmartStart64.S
 *     https://github.com/LdB-ECM/Raspberry-Pi-Multicore/blob/master/xRTOS/SmartStart64.S
 * Licensed under CC-BY (Attribution) — original code retained per its terms.
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include <raspi/sysregs.h>
#include <raspi/mm.h>
#include <raspi/mmu.h>
#include <uk/config.h>

.section ".text.boot"

.global _start
_start:

//"================================================================"
//  Hold startup data for later use
//"================================================================"
	adr x12, _start							// Hold boot address in high register R12
	ldr x1, =RPi_BootAddr					// Address of RPi_BootAddr
	str w12, [x1]							// Store the boot address

	ldr	x0, =0x3F000000						// No need to detect address in 64bit mode it has to be 0x3F000000 on Pi3
	ldr x1, =RPi_IO_Base_Addr				// Address of RPi_IO_Base_Addr
	str w0, [x1]							// Store the IO base address

	ldr	x0, =0xC0000000						// No need to detect address in 64bit mode it has to be 0xC0000000 on Pi3
	ldr x1, =RPi_ARM_TO_GPU_Alias			// Address of RPi_IO_Base_Addr
	str w0, [x1]							// Store the IO base address

	mov x0, #1								// Multicore support starts as 1 core
	ldr x1, =RPi_CoresReady					// Address of RPi_CoresReady
	str w0, [x1]							// Store the CoresReady count as 1

	mov x0, #0x98							// Compiled for ARM8 CPU in AARCH64 and supports 4 cores
.if (__ARM_FP == 14)
	orr x0, x0, #0x80000000				    // Set the hard float flag
.endif	
	ldr x1, =RPi_CompileMode				// Address of RPi_CompileMode
	str w0, [x1]							// Store the compiler mode  

	mrs	x0, midr_el1						// Read CPU Id register
	ldr x1, =RPi_CpuId						// Address of RPi_CpuId
	str w0, [x1]			

//"================================================================"
//  Setup stack pointers for each core and each CPU operation mode
//"================================================================"
multicore_start:
    ldr x2, = __EL2_stack_core0				// Address of EL2_stack_core0 stack pointer value
    ldr x3, = __EL1_stack_core0				// Address of EL1_stack_core0 stack pointer value
    ldr x4, = __EL0_stack_core0				// Address of EL0_stack_core0 stack pointer value
	mrs x6, mpidr_el1						// Read core id on ARM8
	ands x6, x6, #0x3						// Make cpu id bitmask
	beq set_stacks							// Ready to set core 0 stacks
    ldr x2, = __EL2_stack_core1				// Address of EL2_stack_core1 stack pointer value
    ldr x3, = __EL1_stack_core1				// Address of EL1_stack_core1 stack pointer value
    ldr x4, = __EL0_stack_core1				// Address of EL0_stack_core1 stack pointer value
	cmp x6, #1								// Check cpu id for core 1
	beq set_stacks							// Ready to set core 1 stacks
    ldr x2, = __EL2_stack_core2				// Address of EL2_stack_core2 stack pointer value
    ldr x3, = __EL1_stack_core2				// Address of EL1_stack_core2 stack pointer value
    ldr x4, = __EL0_stack_core2				// Address of EL0_stack_core2 stack pointer value
	cmp x6, #2								// Check cpu id for core 2
	beq set_stacks							// Ready to set core 1 stacks
    ldr x2, = __EL2_stack_core3				// Address of EL2_stack_core3 stack pointer value
    ldr x3, = __EL1_stack_core3				// Address of EL1_stack_core3 stack pointer value
    ldr x4, = __EL0_stack_core3				// Address of EL0_stack_core3 stack pointer value
set_stacks:
	mov	sp, x2								/* EL2 stack set */
	msr	sp_el1, x3							/* EL1 stack set */
	msr	sp_el0, x4							/* EL0 stack set */

//"================================================================"
//  Initilize MPID/MPIDR registers for all Cores
//"================================================================"
	mrs	x0, midr_el1
	mrs	x1, mpidr_el1
	msr	vpidr_el2, x0
	msr	vmpidr_el2, x1

//"================================================================"
//  Initialize Generic Timers for Core0
//"================================================================"
	mrs	x0, cnthctl_el2
	orr	x0, x0, #0x3						/* Enable EL1 access to timers */
	msr	cnthctl_el2, x0
	msr	cntvoff_el2, xzr
	
//"================================================================"
//  Disable coprocessor traps for all Cores
//"================================================================"
.if (__ARM_FP == 14)
	mov	x0, #0x33ff
	msr	cptr_el2, x0						// Disable coprocessor traps to EL2
	msr	hstr_el2, xzr						// Disable coprocessor traps to EL2
	ldr	x0, =CPACR_EL1_VALUE
	msr	cpacr_el1, x0						// Enable FP/SIMD at EL1
.endif

//"================================================================"
//  Initialize HCR_EL2 so EL1 is 64 bits for all Cores
//"================================================================"
	ldr	x0, =HCR_EL2_VALUE					// 64bit EL1
	msr	hcr_el2, x0

//"================================================================"
//  Initialize SCTLR_EL1 for all Cores
//"================================================================"
    /*  RES1 bits (29,28,23,22,20,11) to 1
	 *  RES0 bits (31,30,27,21,17,13,10,6) +
	 *  UCI,EE,EOE,WXN,nTWE,nTWI,UCT,DZE,I,UMA,SED,ITD,
	 *  CP15BEN,SA0,SA,C,A,M to 0 */
	mov	x0, #0x0800
	movk	x0, #0x30d0, lsl #16
	orr    x0, x0, #(0x1 << 2)            // The C bit on (data cache). 
	orr    x0, x0, #(0x1 << 12)           // The I bit on (instruction cache)
	msr	sctlr_el1, x0

//"================================================================"
//  Set up the exception vector table for EL2
//"================================================================"
	ldr		x0, =SCTLR_EL2_VALUE
	msr		sctlr_el2, x0

//"================================================================"
//  Return to the EL1_SP1 mode from EL2 for all Cores
//"================================================================"
	mov	x0, #0x3c5							// EL1_SP1 | D | A | I | F
	msr	spsr_el2, x0						// Set spsr_el2 with settings
	adr	x0, exit_el1						// Address to exit EL2
	msr	elr_el2, x0							// Set elevated return register
	eret									// Call elevated return
exit_el1:

//"================================================================"
//  Set vector table for EL1 for Cores (Yep all use same vectors)
//"================================================================"
    ldr x0, =vectors_el1						
    msr vbar_el1,x0

	mrs x6, mpidr_el1						// Read core id on ARM8
	and x6, x6, #0x3						// Make cpu id bitmask
	cbz x6, cpu0_exit_multicore_park		// Core0 continues on


//"================================================================"
//  Enable Mailbox IRQs for secondary cores
//"================================================================"
	ldr   x0, =0x40000054     // CORE1_MBOX_IRQCNTL
    mov   w1, #1
    str   w1, [x0]
    ldr   x0, =0x40000058     // CORE2_MBOX_IRQCNTL
    str   w1, [x0]
    ldr   x0, =0x4000005C     // CORE3_MBOX_IRQCNTL
    str   w1, [x0]

//"================================================================"
//      Now park Core 1,2,3 into secondary spinloop on BCM2837
//"================================================================"
	ldr x1, =RPi_CoresReady					// Address of CoreReady count
	ldr w0, [x1]							// Load current core count 
	add w0, w0, #1							// Add one as core about to go ready
	str w0, [x1]							// Update CoreReady count
	b  StartSecondarySpin					// Jump to setup secondary spin
cpu0_exit_multicore_park:

//"================================================================"
//  About to go to into C kernel clear BSS (Core0 only)
//"================================================================"
	ldr x3, =__bss_end
	ldr x0, =__bss_start
	cmp	x0, x3
	bcs	.bss_cleared
.bss_zero_loop:
	str	wzr, [x0], 4
	cmp	x3, x0
	bhi	.bss_zero_loop
.bss_cleared:

//"================================================================"
//			Core0 will bring Core 1,2,3 to secondary spin 
//"================================================================"
.equ spin_cpu1, 0xe0
	mov x1, #spin_cpu1						// Spin core1 jump address
	ldr x2, =multicore_start				// Function we are going to call
	str	x2, [x1]							// Store the function address to core1
	sev										// Wake core1 up
 	ldr x3, =RPi_CoresReady					// Set CoresReady count address
.WaitCore1ACK:
	ldr	w1, [x3]							// Read CoresReady count
	cmp	w1, #2								// Wait for setting of second core ready
	bne	.WaitCore1ACK						// Core1 not ready so read again
.equ spin_cpu2, 0xe8
	mov x1, #spin_cpu2						// Spin core2 jump address
	ldr x2, =multicore_start				// Function we are going to call
	str	x2, [x1]							// Store the function address to core2
	sev										// Wake core2 up
 	ldr x3, =RPi_CoresReady					// Set CoresReady count address
.WaitCore2ACK:
	ldr	w1, [x3]							// Read CoresReady count
	cmp	w1, #3								// Wait for setting of third core ready
	bne	.WaitCore2ACK						// Core2 not ready so read again
.equ spin_cpu3, 0xf0
	mov x1, #spin_cpu3						// Spin core3 jump address
	ldr x2, =multicore_start				// Function we are going to call
	str	x2, [x1]							// Store the function address to core3
	sev										// Wake core3 up
 	ldr x3, =RPi_CoresReady					// Set CoresReady count address
.WaitCore3ACK:
	ldr	w1, [x3]							// Read CoresReady count
	cmp	w1, #4								// Wait for setting of third core ready
	bne	.WaitCore3ACK						// Core3 not ready so read again

// Continue with BSP (core 0)
master:
	// Make sure everything is synchronized before getting into el1
	isb sy
	dsb sy

el1_entry:
	bl 	create_page_tables

	// Start to enable MMU
	/*
	 * Using dsb here to guarantee the create_pagetables has
	 * been done.
	 */
	dsb sy

	adrp	x0, _pagetables				
	msr		ttbr1_el1, x30
	isb		
	msr		ttbr0_el1, x0
	isb		

	/* Clear the Monitor Debug System control register */
	msr mdscr_el1, xzr

	/* Invalidate the TLB to avoid stale one */
	tlbi vmalle1
	dsb nsh

	ldr		x0, =(TCR_VALUE)		
	msr		tcr_el1, x0

	ldr		x0, =(MAIR_VALUE)
	msr		mair_el1, x0

	ldr		x6, =_libraspiplat_entry
	ldr		x5, =SCTLR_EL1_VALUE_MMU_ENABLED
	msr		sctlr_el1, x5

	isb

#if CONFIG_RASPI_WATERMARK_STACK
watermark_stack_start:
    ldr     x1, =VA_START
    ldr     w2, =0x10000
watermark_stack_loop: 
	cbz     w2, watermark_stack_done
    str     x2, [x1], #8
    sub     w2, w2, #1
    cbnz    w2, watermark_stack_loop
watermark_stack_done:
#endif

clear_bss_start:
	// Clear bss
	ldr     x1, =__bss_start
	ldr     w2, =__bss_size
clear_bss_loop:
	cbz     w2, clear_bss_done
	str     xzr, [x1], #8
	sub     w2, w2, #1
	cbnz    w2, clear_bss_loop
clear_bss_done:

    // Set the stack before our code
	msr		SPSel, #1
    ldr     x1, =_start
	mov		sp, x1

jump_to_C:
// Recover initial timer value
	mov		w0, w10
	mov		w1, w11
	mov		w2, w12
	mov		w3, w13
	br 		x6

	.macro	create_pgd_entry, tbl, virt, tmp1, tmp2
	create_table_entry \tbl, \virt, PGD_SHIFT, \tmp1, \tmp2
	create_table_entry \tbl, \virt, PUD_SHIFT, \tmp1, \tmp2
	.endm

	.macro	create_table_entry, tbl, virt, shift, tmp1, tmp2
	lsr	\tmp1, \virt, #\shift
	and	\tmp1, \tmp1, #PTRS_PER_TABLE - 1			// table index
	add	\tmp2, \tbl, #PAGE_SIZE
	orr	\tmp2, \tmp2, #MM_TYPE_PAGE_TABLE	
	str	\tmp2, [\tbl, \tmp1, lsl #3]
	add	\tbl, \tbl, #PAGE_SIZE					// next level table page
	.endm

	.macro	create_block_map, tbl, phys, start, end, flags, tmp1
	lsr	\start, \start, #SECTION_SHIFT
	and	\start, \start, #PTRS_PER_TABLE - 1			// table index
	lsr	\end, \end, #SECTION_SHIFT
	and	\end, \end, #PTRS_PER_TABLE - 1				// table end index
	lsr	\phys, \phys, #SECTION_SHIFT
	mov	\tmp1, #\flags
	orr	\phys, \tmp1, \phys, lsl #SECTION_SHIFT			// table entry
9999:	str	\phys, [\tbl, \start, lsl #3]				// store the entry
	add	\start, \start, #1					// next entry
	add	\phys, \phys, #SECTION_SIZE				// next block
	cmp	\start, \end
	b.ls	9999b
	.endm

//"================================================================"
// Two-level page tables: PGD→PUD→PMD.  
// PUD[0] maps RAM and SoC devices via 2 MiB sections.  
// PUD[1] gets its own PMD for a 2 MiB local-INTC region.  
// Keeps all MMIO confined to one page, RAM mapping untouched.
//"================================================================"

create_page_tables:
	mov	x29, x30						// save return address

	// clear 5 contiguous pages reserved in .pagetables
	adrp    x0, _pagetables
	mov     x1, #(PG_DIR_SIZE + 2*PAGE_SIZE) // 5 × 4 KiB = 20 KiB
	bl      memzero

	// Build PGD, PUD‑0, PMD‑0
	adrp    x0, _pagetables           // x0 = PGD base (page 0)
	mov     x1, #VA_START
	create_pgd_entry  x0, x1, x2, x3  // alloc PUD‑0 (page 1) + PMD‑0 (page 2)

	/* Mapping kernel and init stack*/
	mov 	x1, xzr											// start mapping from physical offset 0
	mov 	x2, #VA_START									// first virtual address
	ldr	x3, =(VA_START + DEVICE_BASE - SECTION_SIZE)		// last virtual address
	create_block_map x0, x1, x2, x3, MMU_FLAGS, x4

	/* Mapping device memory*/
	mov 	x1, #DEVICE_BASE					// start mapping from device base address 
	ldr 	x2, =(VA_START + DEVICE_BASE)				// first virtual address
	ldr	x3, =(VA_START + PHYS_MEMORY_SIZE - SECTION_SIZE)	// last virtual address
	create_block_map x0, x1, x2, x3, MMU_DEVICE_FLAGS, x4

	// Add PUD‑idx 1 (covers 0x4000 0000 – 0x7FFF FFFF)
	adrp    x5, _pagetables          // x5 = PGD base again
	add     x6, x5, #PAGE_SIZE       // x6 = PUD page 1

	/* Compute PUD index for VA 0x4000_0000 – it is 1             */
	mov     x7, #(VA_START + 0x40000000)
	lsr     x7, x7, #PUD_SHIFT
	and     x7, x7, #PTRS_PER_TABLE-1

	// Point PUD[1] to a *new* blank PMD page
	add     x4, x5, #(4*PAGE_SIZE)   // page 4 in the .pagetables area
	orr     x4, x4, #MM_TYPE_PAGE_TABLE
	str     x4, [x6, x7, lsl #3]

	/* Map first 2 MiB at 0x4000 0000 (local‑INTC) as device      */
	and     x4, x4, #PAGE_MASK       // x4 = PMD‑1 base (phys addr)
	mov     x1, #0x40000000          // phys
	ldr     x2, =(VA_START + 0x40000000) // virt
	ldr     x3, =(VA_START + 0x40000000) // single section
	create_block_map x4, x1, x2, x3, MMU_DEVICE_FLAGS, x5

	mov	x30, x29						// restore return address
	ret

    .balign 4
StartSecondarySpin:
    wfe

.spin:
    /* Compute this core’s mailbox-0_RD base in x7 */
    mov   x5, #192
    movk  x5, #0x4000, lsl #16        /* x5 = 0x4000_00C0 */
    mrs   x6, MPIDR_EL1
    and   x6, x6, #0x3
    ubfiz x6, x6, #4, #4              /* x6 = core_id<<4 */
    add   x7, x5, x6                  /* x7 = mailbox-0_RD for this core */

    /* Fetch fn */
    ldr   w4, [x7]
    cbz   w4, StartSecondarySpin

    /* Rebuild the 64-bit argument */
    ldr   w0, [x7, #4]                /* mailbox-1_RD → low half */
    ldr   w1, [x7, #8]                /* mailbox-2_RD → high half */
    orr   x0, x0, x1, lsl #32

    /* clear all four slots */
    str     w4, [x7]
    str     w0, [x7, #4]
    str     w1, [x7, #8]
    str     w2, [x7, #12]

	mov   x28, x4          // save the function pointer
	mov   x29, x0          // save the argument low  64 bits
	mov   x30, x1          // save the argument high 64 bits (if any)

	bl    enable_secondary_mmu

	/* Restore the argument exactly as the caller expects */
	mov   x0, x29
	mov   x1, x30
	br    x28              // jump to fn(arg) with MMU & caches ON

    /* Never returns */
    b     StartSecondarySpin

    .balign 0x80
irq_handler_stub:
    // Minimal context-save. Only touching X0–X3 here
    stp    x0, x1, [sp, #-16]!      
    stp    x2, x3, [sp, #-16]!

    // Clear the local timer interrupt
	movz  x0, #0x0040          // bottom half
    movk  x0, #0x4000, lsl #16 // top half

    ldr    w1, [x0]
    bic    w1, w1, #8          // clear the interrupt‐enable bit
    str    w1, [x0]

	bl     ukplat_irq_handle

    // Restore context
    ldp    x2, x3, [sp], #16
    ldp    x0, x1, [sp], #16

    // Return from exception
    eret

    .macro vector handler
      .balign 0x80
      b \handler
    .endm

    .balign 0x800
VectorTable:
    // EL1, SP1, IRQ slot:
    .rept 5*4    // skip 5 entries
      .balign 0x80
      b hang    
    .endr
    .balign 0x80
    b irq_handler_stub

hang:
	b hang

/*
* Core-0 already built the page–tables in the .pagetables
* area.  We just point TTBR0_EL1 at the same physical pages.
*/
enable_secondary_mmu:
        adrp    x0, _pagetables         // physical address
        msr     ttbr0_el1, x0           // use it for user&kernel space
        isb

        ldr     x0, =MAIR_VALUE
        msr     mair_el1, x0
        ldr     x0, =TCR_VALUE          // SH=11 inner-shareable!
        msr     tcr_el1,  x0
        dsb     sy
        isb

        ldr     x0, =SCTLR_EL1_VALUE_MMU_ENABLED
        msr     sctlr_el1, x0
        isb
        ret

//"================================================================"
//       	   DATA FOR SMARTSTART64  EXPOSED TO INTERFACE 
//"================================================================"
.section ".data.smartstart64", "aw"
.balign 8

.globl RPi_IO_Base_Addr;				// Make sure Pi_IO_Base_Addr label is global
RPi_IO_Base_Addr : .4byte 0;			// Peripheral Base addr is 4 byte variable in 64bit mode

.globl RPi_ARM_TO_GPU_Alias;			// Make sure RPi_ARM_TO_GPU_Alias label is global
RPi_ARM_TO_GPU_Alias: .4byte 0;			// ARM to GPU alias is 4 byte variable in 32bit mode

.globl RPi_BootAddr;					// Make sure RPi_BootAddr label is global
RPi_BootAddr : .4byte 0;				// CPU boot address is 4 byte variable in 64bit mode

.globl RPi_CoresReady;					// Make sure RPi_CoresReady label is global
RPi_CoresReady : .4byte 0;				// CPU cores ready for use is 4 byte variable in 32bit mode

.globl RPi_CPUBootMode;					// Make sure RPi_CPUBootMode label is global
RPi_CPUBootMode : .4byte 0;				// CPU Boot Mode is 4 byte variable in 64bit mode

.globl RPi_CpuId;						// Make sure RPi_CpuId label is global
RPi_CpuId : .4byte 0;					// CPU Id is 4 byte variable in 64bit mode

.globl RPi_CompileMode;					// Make sure RPi_CompileMode label is global
RPi_CompileMode : .4byte 0;				// Compile mode is 4 byte variable in 64bit mode

.globl RPi_SmartStartVer;				// Make sure RPi_SmartStartVer label is global
RPi_SmartStartVer : .4byte 0x00020104;  // SmartStart version is 4 byte variable in 32bit mode

.balign 8
.globl RPi_coreCB_PTR;
RPi_coreCB_PTR: 
.8byte 0x0;								// Core 0 Control Block pointer
.8byte 0x0;								// Core 1 Control Block pointer	
.8byte 0x0;								// Core 2 Control Block pointer
.8byte 0x0;								// Core 3 Control Block pointer