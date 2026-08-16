/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * FreeBSD sys/types.h shim — forward to the real glibc types then add
 * the FreeBSD-specific typedefs that bcm2835_gpio.c uses.
 */
#pragma once
#include_next <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Standard FreeBSD unsigned aliases */
typedef unsigned int   u_int;
typedef unsigned long  u_long;
typedef unsigned short u_short;
typedef unsigned char  u_char;

/* Open Firmware / FDT handle types */
typedef uint32_t phandle_t;
typedef uint32_t xref_t;

/* FDT cell type (big-endian 32-bit value, stored as u32) */
typedef uint32_t pcell_t;

/* Physical address type (used by bcm2835_sdhci.c for DMA buffer address) */
typedef uintptr_t vm_paddr_t;

/* Boolean constants (FreeBSD style) */
#ifndef TRUE
#  define TRUE  1
#endif
#ifndef FALSE
#  define FALSE 0
#endif
