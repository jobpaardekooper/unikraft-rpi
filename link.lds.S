/*
 * Copyright (C) 2018, bzt (bztsrc@github), https://github.com/bztsrc/raspi3-tutorial
 * Copyright (C) 2020, Santiago Pagani <santiagopagani@gmail.com>
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
#include <uk/arch/limits.h>
#include <uk/plat/common/common.lds.h>

#define RAM_BASE_ADDR	0x80000

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_libraspiplat_entry)

SECTIONS
{
    . = RAM_BASE_ADDR;

	/* Place DTB binary at the beginning of the RAM */
	_dtb = .;
	//. = . + DTB_RESERVED_SIZE;

	/* Code */
	_text = .;
	.text :
	{
		KEEP(*(.text.boot))
		*(.text)
		*(.text.*)
		*(.gnu.linkonce.t*)
	} :text
	_etext = .;

	. = ALIGN(__PAGE_SIZE);
	EXCEPTION_SECTIONS

	CTORTAB_SECTION

	INITTAB_SECTION

	/* Read-only data */
	. = ALIGN(__PAGE_SIZE);
	_rodata = .;
	.rodata :
	{
		*(.rodata)
		*(.rodata.*)
		*(.gnu.linkonce.r*)
	}
	_erodata = .;

	/* Constructor tables (read-only) */
	. = ALIGN(0x8);
	_ctors = .;
	.preinit_array : {
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);
	}

	. = ALIGN(0x8);
	.init_array : {
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP (*(.init_array .ctors))
		PROVIDE_HIDDEN (__init_array_end = .);
	}
	_ectors = .;

	. = ALIGN(__PAGE_SIZE);
	TLS_SECTIONS

	/* Read-write data that is initialized explicitly in code */
	_data = .;
	.data :
	{
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
	} :data
	_edata = .;

	/* Read-write data that is not initialized explicitly in code */
	. = ALIGN(__PAGE_SIZE);
	__bss_start = .;
    .bss (NOLOAD) :
	{
		*(.bss)
		*(.bss.*)
        *(COMMON)
    }
	__bss_end = .;

	/*  Per‑core stacks (16‑byte aligned, NOLOAD) */

	.stack_core0 (NOLOAD) : ALIGN(16) {
		__stack_start_core0__ = .;
		. += 512;   __EL0_stack_core0 = .;
		. += 16384; __EL1_stack_core0 = .;
		. += 512;   __EL2_stack_core0 = .;
		__stack_end_core0__ = .;
	}

	.stack_core1 (NOLOAD) : ALIGN(16) {
		__stack_start_core1__ = .;
		. += 512;   __EL0_stack_core1 = .;
		. += 1024;  __EL1_stack_core1 = .;
		. += 512;   __EL2_stack_core1 = .;
		__stack_end_core1__ = .;
	}

	.stack_core2 (NOLOAD) : ALIGN(16) {
		__stack_start_core2__ = .;
		. += 512;   __EL0_stack_core2 = .;
		. += 1024;  __EL1_stack_core2 = .;
		. += 512;   __EL2_stack_core2 = .;
		__stack_end_core2__ = .;
	}

	.stack_core3 (NOLOAD) : ALIGN(16) {
		__stack_start_core3__ = .;
		. += 512;   __EL0_stack_core3 = .;
		. += 1024;  __EL1_stack_core3 = .;
		. += 512;   __EL2_stack_core3 = .;
		__stack_end_core3__ = .;
	}

/* ------------------------------------------------------------------
 *   Initial MMU tables
 *   page‑0 : PGD
 *   page‑1 : PUD covering 0x0000_0000 – 0x3FFF_FFFF
 *   page‑2 : PMD for that PUD
 *   page‑3 : PUD covering 0x4000_0000 – 0x7FFF_FFFF  (local‑INTC window)
 *   page‑4 : PMD for that PUD
 * ---------------------------------------------------------- */

	. = ALIGN(__PAGE_SIZE);
	_pagetables = .;
	.pagetables (NOLOAD) :
	{
		. += 5 * __PAGE_SIZE;
	}

	_end = .;
	

	. = ALIGN(__PAGE_SIZE);
	.comment       0 : { *(.comment) }
	DEBUG_SYMBOLS

   /DISCARD/ : { *(.gnu*) *(.note*) }
}
__bss_size = (__bss_end - __bss_start)>>3;

PHDRS
{
    text PT_LOAD FLAGS(5); /* R E */
    data PT_LOAD FLAGS(6); /* RW */
}
