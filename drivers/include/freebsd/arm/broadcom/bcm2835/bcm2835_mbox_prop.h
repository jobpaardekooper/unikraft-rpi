/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * arm/broadcom/bcm2835/bcm2835_mbox_prop.h shim.
 *
 * Wraps the Unikraft mbox.c mailbox driver with the FreeBSD
 * bcm2835_mbox_prop API that bcm2835_sdhci.c uses:
 *
 *   bcm2835_mbox_set_power_state()  — powers the EMMC peripheral on/off
 *   bcm2835_mbox_get_clock_rate()   — reads the EMMC base clock frequency
 *
 * The implementations live in bcm_sdhci_shim.c (compiled with normal
 * Unikraft include path) and are declared here for the FreeBSD isolated
 * compilation unit.
 */
#pragma once
#include <stdint.h>

/* Power state IDs (VideoCore firmware mailbox) */
#define BCM2835_MBOX_POWER_ID_EMMC   0x00000000U  /* SD card / Arasan EMMC */
#define BCM2835_MBOX_POWER_ID_UART0  0x00000001U
#define BCM2835_MBOX_POWER_ID_USB    0x00000003U

/* Clock IDs */
#define BCM2835_MBOX_CLOCK_ID_EMMC   0x00000001U  /* Arasan EMMC (bcm2835) */
#define BCM2835_MBOX_CLOCK_ID_CORE   0x00000004U
#define BCM2838_MBOX_CLOCK_ID_EMMC2  0x0000000CU  /* EMMC2 (bcm2838/RPi4) */

int bcm2835_mbox_set_power_state(uint32_t dev_id, int on);
/* Note: parameter renamed from 'hz' to avoid clash with #define hz 100 */
int bcm2835_mbox_get_clock_rate(uint32_t clock_id, uint32_t *hz_out);
