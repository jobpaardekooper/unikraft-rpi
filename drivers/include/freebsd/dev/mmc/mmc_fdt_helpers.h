/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dev/mmc/mmc_fdt_helpers.h shim.
 *
 * FreeBSD FDT MMC helpers parse the Device Tree to configure host
 * parameters (bus width, speeds, voltages).  The shim stubs them out:
 * bcm2835_sdhci.c calls mmc_fdt_parse() which is a no-op here, so the
 * slot->host fields retain the values set by sdhci_init_slot().
 *
 * struct mmc_helper mirrors the FreeBSD layout closely enough that
 * bcm_sdhci_update_ios() can reference .vmmc_supply / .vqmmc_supply.
 * Both are always NULL because mmc_fdt_parse() never populates them,
 * so the regulator_enable/disable branches are never taken.
 */
#pragma once
#include <stdint.h>
#include <dev/mmc/bridge.h>

/* Opaque regulator handle — always NULL in the shim */
struct regulator;

/*
 * struct mmc_helper — mirrors FreeBSD's dev/mmc/mmc_fdt_helpers.h.
 * Only the fields referenced by bcm2835_sdhci.c need to be present.
 */
struct mmc_helper {
    struct regulator *vmmc_supply;   /* VDD rail supply (optional) */
    struct regulator *vqmmc_supply;  /* VCCQ signalling rail (optional) */
    int               _dummy;        /* padding */
};

/*
 * mmc_fdt_parse — parse DTree MMC node into mmc_host.
 * Stub: leaves host unchanged (initialized by sdhci_init_slot).
 * vmmc_supply and vqmmc_supply are left NULL — regulator paths skipped.
 */
static inline void
mmc_fdt_parse(void *dev __attribute__((unused)),
              int unit __attribute__((unused)),
              struct mmc_helper *helper __attribute__((unused)),
              struct mmc_host *host __attribute__((unused)))
{
    /* No FDT on Unikraft/RPi3 bare-metal — defaults are sufficient */
}

/*
 * regulator_enable / regulator_disable — no-op stubs.
 * Called only when vmmc_supply / vqmmc_supply are non-NULL.
 * Since mmc_fdt_parse() never sets them, these are dead code.
 */
static inline int
regulator_enable(struct regulator *reg __attribute__((unused)))
{ return 0; }

static inline int
regulator_disable(struct regulator *reg __attribute__((unused)))
{ return 0; }
