/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * uk/sdcard.h — BCM2837 EMMC (Arasan SDHCI) raw sector I/O API.
 *
 * Provides 512-byte sector read/write.
 * Call uk_sdcard_init() once at boot before any read/write.
 */
#ifndef _UK_SDCARD_H_
#define _UK_SDCARD_H_

#include <stdint.h>

/**
 * uk_sdcard_init - Reset the Arasan SDHCI, set the EMMC clock to 50 MHz
 * via the VideoCore mailbox, and run the full SD card initialisation
 * sequence (CMD0 → ACMD41 → CMD7 → 25 MHz data clock).
 *
 * Returns 0 on success, negative errno on failure.
 */
int uk_sdcard_init(void);

/**
 * uk_sdcard_read_block - Read one 512-byte sector.
 * @lba: Logical block address.
 * @buf: 512-byte buffer (4-byte aligned).
 * Returns 0 on success, negative errno on failure.
 */
int uk_sdcard_read_block(uint32_t lba, uint8_t *buf);

/**
 * uk_sdcard_write_block - Write one 512-byte sector.
 * @lba: Logical block address.
 * @buf: 512-byte buffer (4-byte aligned).
 * Returns 0 on success, negative errno on failure.
 */
int uk_sdcard_write_block(uint32_t lba, const uint8_t *buf);

/*
 * Diagnostic: set by the driver whenever INT_ERR is detected.
 *   uk_sdcard_dbg_phase  — 1=CMD phase of a command, 2=READ_RDY wait,
 *                          3=DATA_DONE wait
 *   uk_sdcard_dbg_irq    — raw INTERRUPT register value at that moment
 * Use these to decode which hardware error fired (see INTERRUPT bits 31:16).
 */
extern uint32_t uk_sdcard_dbg_phase;
extern uint32_t uk_sdcard_dbg_irq;

#endif /* _UK_SDCARD_H_ */
