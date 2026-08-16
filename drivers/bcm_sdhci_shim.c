/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * bcm_sdhci_shim.c — BCM2837 EMMC (Arasan SDHCI) API translation layer.
 *
 * Architecture
 * ────────────
 * bcm2835_sdhci.c (unmodified FreeBSD driver) is used for hardware
 * initialisation only: its bcm_sdhci_probe + bcm_sdhci_attach path powers
 * on the EMMC peripheral via the VideoCore mailbox and queries the base
 * clock frequency.
 *
 * The actual SD card initialisation (GPIO mux, EMMC host controller reset,
 * clock divisor setup) and all SD protocol traffic (CMD0..CMD24) are handled
 * here via direct polled MMIO — the same approach as bcm_bsc_shim.c for I2C.
 *
 * Why not route through sdhci_generic_request()?
 * ─────────────────────────────────────────────
 * sdhci.c uses full FreeBSD kobj dispatch (((kobj_t)dev)->ops->cls),
 * callout, taskqueue, and bus_dma in load-bearing ways that are
 * incompatible with our simplified shim.  sdhci.c is therefore NOT
 * compiled; we stub sdhci_init_slot / sdhci_start_slot and all
 * sdhci_generic_* functions here.
 *
 * bus_alloc_resource_any / bus_release_resource
 * ─────────────────────────────────────────────
 * bcm2835_sdhci.c uses the single-resource variant bus_alloc_resource_any().
 * Both MEM (0x3F300000) and IRQ resources are implemented here.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include <uk/print.h>
#include <uk/alloc.h>

#include <uk/sdcard.h>
#include <raspi/mbox.h>
#include <bcm_sdhci_internal.h>

/* =========================================================================
 * BCM2837 System Timer (1 MHz free-running counter, low 32 bits)
 * ========================================================================= */
#define SYSTIMER_CLO  (*(volatile uint32_t *)0x3F003004UL)

static void _us_delay(uint32_t us)
{
    uint32_t t = SYSTIMER_CLO + us;
    while ((int32_t)(t - SYSTIMER_CLO) > 0)
        ;
}

/* =========================================================================
 * EMMC register map (BCM2837 Arasan SDHCI, base 0x3F300000)
 * ========================================================================= */
#define EMMC_BASE       0x3F300000UL

#define EMMC_ARG2       0x00U   /* ACMD23 / DMA address          */
#define EMMC_BLKSIZECNT 0x04U   /* [31:16] block count, [9:0] blk size */
#define EMMC_ARG1       0x08U   /* command argument              */
#define EMMC_CMDTM      0x0CU   /* command & transfer mode       */
#define EMMC_RESP0      0x10U
#define EMMC_RESP1      0x14U
#define EMMC_RESP2      0x18U
#define EMMC_RESP3      0x1CU
#define EMMC_DATA       0x20U   /* PIO data FIFO (32-bit access) */
#define EMMC_STATUS     0x24U
#define EMMC_CONTROL0   0x28U
#define EMMC_CONTROL1   0x2CU
#define EMMC_INTERRUPT  0x30U   /* write 1 to clear              */
#define EMMC_INT_MASK   0x34U
#define EMMC_INT_EN     0x38U
#define EMMC_CONTROL2   0x3CU

/* CONTROL1 bits */
#define C1_CLK_INTLEN   (1U << 0)
#define C1_CLK_STABLE   (1U << 1)
#define C1_CLK_EN       (1U << 2)
#define C1_CLK_FREQ_MASK 0x0000FFC0U    /* bits 15:6 (FREQ8 + FREQ_MS2) */
#define C1_CLK_TOUT_MAX (0xEU << 16)
#define C1_SRST_ALL     (1U << 24)
#define C1_SRST_CMD     (1U << 25)
#define C1_SRST_DATA    (1U << 26)

/* CMDTM response type (bits 17:16) */
#define RT_NONE         (0U << 16)
#define RT_136          (1U << 16)
#define RT_48           (2U << 16)
#define RT_48B          (3U << 16)  /* 48-bit + busy on DAT0 */

/* CMDTM flags */
#define TM_CRC_EN       (1U << 19)
#define TM_IDX_EN       (1U << 20)
#define TM_ISDATA       (1U << 21)
#define TM_DAT_DIR_RD   (1U << 4)
#define TM_BLKCNT_EN    (1U << 1)

/* INTERRUPT / STATUS bits */
#define INT_CMD_DONE    (1U << 0)
#define INT_DATA_DONE   (1U << 1)
#define INT_WRITE_RDY   (1U << 4)
#define INT_READ_RDY    (1U << 5)
#define INT_ERR         (1U << 15)
#define INT_ALL_ERR     0xFFFF0000U

#define ST_CMD_INHIBIT  (1U << 0)
#define ST_DAT_INHIBIT  (1U << 1)
#define ST_DAT_ACTIVE   (1U << 2)

/* =========================================================================
 * Register helpers
 * ========================================================================= */

static inline uint32_t _emmc_rd(uint32_t off)
{
    return *(volatile uint32_t *)(EMMC_BASE + off);
}

/* Write delay: 2 SD clock cycles between control-register writes.
 * f_out = _clk_hz / (2*divisor)  →  2 cycles = 4*divisor / _clk_hz µs */
static uint32_t _wr_delay_us = 0;

static void _emmc_wr(uint32_t off, uint32_t val)
{
    *(volatile uint32_t *)(EMMC_BASE + off) = val;
    if (_wr_delay_us)
        _us_delay(_wr_delay_us);
}

/* Data FIFO writes — no inter-write delay */
static inline void _emmc_wr_data(uint32_t val)
{
    *(volatile uint32_t *)(EMMC_BASE + EMMC_DATA) = val;
}

/* =========================================================================
 * Clock management
 * ========================================================================= */
#define CLK_ID_EMMC     1U
#define CLK_BASE_HZ     50000000U   /* fallback if mailbox read fails */

static uint32_t _clk_hz = CLK_BASE_HZ;

static void _set_clock(uint32_t divisor)
{
    /* Disable SD output clock */
    uint32_t c1 = _emmc_rd(EMMC_CONTROL1);
    c1 &= ~(C1_CLK_EN | C1_CLK_FREQ_MASK | (0xFU << 16));
    _emmc_wr(EMMC_CONTROL1, c1);
    _us_delay(20);

    /* Set new divisor and enable internal clock */
    uint32_t lo = divisor & 0xFFU;
    uint32_t hi = (divisor >> 8) & 0x3U;
    c1 |= C1_CLK_INTLEN | C1_CLK_TOUT_MAX | (lo << 8) | (hi << 6);
    _emmc_wr(EMMC_CONTROL1, c1);
    _us_delay(20);

    /* Wait for internal clock stable (max 100 ms) */
    uint32_t deadline = SYSTIMER_CLO + 100000U;
    while (!(_emmc_rd(EMMC_CONTROL1) & C1_CLK_STABLE)) {
        if ((int32_t)(SYSTIMER_CLO - deadline) >= 0) {
            uk_pr_err("sdcard: clock never stabilised\n");
            return;
        }
        _us_delay(10);
    }

    /* Enable output clock */
    c1 = _emmc_rd(EMMC_CONTROL1);
    c1 |= C1_CLK_EN;
    _emmc_wr(EMMC_CONTROL1, c1);
    _us_delay(100);

    /* Write-delay: ≥2 SD clock cycles (Arasan quirk) */
    uint32_t us2 = (4U * divisor * 1000000U + _clk_hz - 1U) / _clk_hz;
    _wr_delay_us = (us2 < 1U) ? 1U : us2;
}

/* =========================================================================
 * Command engine
 * ========================================================================= */
#define POLL_LIMIT  1000000U    /* ~1 s polling budget */

static int _wait_irq(uint32_t mask, uint32_t *out)
{
    for (uint32_t i = 0; i < POLL_LIMIT; i++) {
        uint32_t v = _emmc_rd(EMMC_INTERRUPT);
        if (v & (mask | INT_ERR)) { *out = v; return 0; }
        _us_delay(1);
    }
    return -ETIMEDOUT;
}

static uint32_t _rca   = 0;
static int      _is_hc = 0;
static int      _inited = 0;

/* Diagnostic info on last error */
uint32_t uk_sdcard_dbg_phase = 0;
uint32_t uk_sdcard_dbg_irq   = 0;

static int _cmd(uint32_t idx, uint32_t flags, uint32_t arg, uint32_t *resp)
{
    for (uint32_t i = 0; i < POLL_LIMIT; i++) {
        if (!(_emmc_rd(EMMC_STATUS) & ST_CMD_INHIBIT)) break;
        _us_delay(1);
        if (i == POLL_LIMIT - 1) return -ETIMEDOUT;
    }
    if (flags & TM_ISDATA) {
        for (uint32_t i = 0; i < POLL_LIMIT; i++) {
            if (!(_emmc_rd(EMMC_STATUS) & ST_DAT_INHIBIT)) break;
            _us_delay(1);
            if (i == POLL_LIMIT - 1) return -ETIMEDOUT;
        }
    }

    _emmc_wr(EMMC_INTERRUPT, 0xFFFFFFFFU);
    _emmc_wr(EMMC_ARG1,  arg);
    _emmc_wr(EMMC_CMDTM, (idx << 24) | flags);

    uint32_t irq;
    int rc = _wait_irq(INT_CMD_DONE, &irq);
    if (rc) return rc;
    if (irq & INT_ERR) {
        uk_sdcard_dbg_phase = 1;
        uk_sdcard_dbg_irq   = irq;
        uk_pr_err("sdcard: CMD%u error: INTERRUPT=0x%08X STATUS=0x%08X\n",
                  idx, irq, _emmc_rd(EMMC_STATUS));
        _emmc_wr(EMMC_INTERRUPT, irq);
        return -EIO;
    }
    _emmc_wr(EMMC_INTERRUPT, INT_CMD_DONE);

    if (resp) {
        if ((flags & (3U << 16)) == RT_136) {
            resp[0] = _emmc_rd(EMMC_RESP0);
            resp[1] = _emmc_rd(EMMC_RESP1);
            resp[2] = _emmc_rd(EMMC_RESP2);
            resp[3] = _emmc_rd(EMMC_RESP3);
        } else {
            resp[0] = _emmc_rd(EMMC_RESP0);
        }
    }

    if ((flags & (3U << 16)) == RT_48B) {
        uint32_t deadline = SYSTIMER_CLO + 2000000U;
        while (_emmc_rd(EMMC_STATUS) & ST_DAT_ACTIVE) {
            if ((int32_t)(SYSTIMER_CLO - deadline) >= 0) break;
            _us_delay(10);
        }
    }
    return 0;
}

/* =========================================================================
 * Static resources provided to bcm2835_sdhci.c via bus_alloc_resource_any
 * ========================================================================= */

static struct resource _sdhci_res_mem = {
    .r_bustag    = 0,
    .r_bushandle = EMMC_BASE,
    .r_type      = SYS_RES_MEMORY,
    .r_rid       = 0,
    .r_start     = EMMC_BASE,
};

static struct resource _sdhci_res_irq = {
    .r_bustag    = 0,
    .r_bushandle = 0,
    .r_type      = SYS_RES_IRQ,
    .r_rid       = 0,
    .r_start     = 62,  /* BCM2837 EMMC IRQ: VC IRQ 62 (ARM IRQ 30) */
};

/* =========================================================================
 * _bcm_sdhci_alloc_resource — SDHCI resource allocator.
 * Called by bus_alloc_resource_any() in bus_stubs.c when dev->d_nameunit
 * starts with "sdhci".  bus_release_resource, bus_setup_intr,
 * bus_teardown_intr, device_add_child, and bus_generic_add_child are all
 * defined in bus_stubs.c.
 * ========================================================================= */

struct resource *
_bcm_sdhci_alloc_resource(device_t dev  __attribute__((unused)),
                           int type,
                           int *rid      __attribute__((unused)),
                           unsigned int flags __attribute__((unused)))
{
    if (type == SYS_RES_MEMORY)
        return &_sdhci_res_mem;
    if (type == SYS_RES_IRQ)
        return &_sdhci_res_irq;
    return NULL;
}

/* =========================================================================
 * bcm2835_mbox_set_power_state — power EMMC on/off via VideoCore mailbox
 * ========================================================================= */

int bcm2835_mbox_set_power_state(uint32_t dev_id, int on)
{
    mbox[0] = 8 * 4;
    mbox[1] = MBOX_REQUEST;
    mbox[2] = MBOX_TAG_SETPOWER;        /* 0x28001 */
    mbox[3] = 8;
    mbox[4] = 8;
    mbox[5] = dev_id;
    mbox[6] = on ? 3U : 0U;            /* bit0=state, bit1=wait */
    mbox[7] = 0;
    if (!mbox_call(MBOX_CH_PROP)) {
        uk_pr_err("sdhci: mbox set_power_state failed (dev=%u on=%d)\n",
                  dev_id, on);
        return -EIO;
    }
    return 0;
}

/* =========================================================================
 * bcm2835_mbox_get_clock_rate — read EMMC base clock via VideoCore mailbox
 * Returns rate in Hz via *hz; result = 0 on success.
 * ========================================================================= */

int bcm2835_mbox_get_clock_rate(uint32_t clock_id, uint32_t *hz_out)
{
    mbox[0] = 8 * 4;
    mbox[1] = MBOX_REQUEST;
    mbox[2] = 0x00030002U;             /* GET_CLOCK_RATE */
    mbox[3] = 8;
    mbox[4] = 8;
    mbox[5] = clock_id;
    mbox[6] = 0;
    mbox[7] = 0;
    if (!mbox_call(MBOX_CH_PROP) || mbox[6] == 0) {
        *hz_out = 0;
        return -EIO;
    }
    *hz_out = mbox[6];
    return 0;
}

/* =========================================================================
 * sdhci globals (normally defined in sdhci.c which we do not compile)
 * ========================================================================= */

unsigned int sdhci_quirk_clear = 0;
unsigned int sdhci_quirk_set   = 0;

/*
 * dumping — kernel dump-in-progress flag (declared in sys/conf.h).
 * Always 0 in Unikraft (no kernel dump support).
 */
int dumping = 0;

/* bootverbose is a macro (#define bootverbose 0) from sys/param.h shim */

/* =========================================================================
 * sdhci_init_slot — minimal init of struct sdhci_slot.
 *
 * The real sdhci.c version does DMA allocation and SDHCI hardware reset.
 * We skip that: DMA is stubbed, and the EMMC hardware reset is done later
 * in uk_sdcard_init() via direct MMIO.  We only set the fields that
 * bcm_sdhci_attach() reads after calling sdhci_init_slot().
 * ========================================================================= */

/* Pulled from dev/sdhci/sdhci.h (FreeBSD isolated path).
 * We need the struct layout to access slot->bus and slot->num.
 * Rather than pulling in the FreeBSD header here, we cast through a
 * pointer to the first two device_t fields — both are pointers at known
 * offsets.  The actual struct sdhci_slot layout starts with:
 *   struct mtx  mtx;          // offset 0  (8 bytes on AArch64)
 *   u_int       quirks;       // offset 8
 *   u_int       caps;         // offset 12
 *   u_int       caps2;        // offset 16
 *   device_t    bus;          // offset 20 (pointer, 8 bytes)
 *   device_t    dev;          // offset 28
 *   u_char      num;          // offset 36
 * Use opaque pointer arithmetic to avoid depending on the full struct here.
 *
 * But bcm_sdhci_attach accesses sc->sc_slot.clock (used in WR4 DELAY calc)
 * and sc->sc_slot.opt (set to SDHCI_PLATFORM_TRANSFER = 0x02).
 * Both are zero-initialised by uk_calloc, so they are safe.
 *
 * The struct is zero-filled by uk_calloc, so the only things we need to
 * set explicitly are slot->bus = dev and slot->num = 0.
 * We use a layout-compatible accessor struct to set exactly those fields.
 */
struct _sdhci_slot_head {
    /* struct mtx mtx (16 bytes on aarch64: unsigned long + int) */
    unsigned long _mtx_irqflags;
    int           _mtx_initialised;
    int           _mtx_pad;
    /* u_int quirks, caps, caps2 */
    unsigned int  _quirks;
    unsigned int  _caps;
    unsigned int  _caps2;
    /* device_t bus, dev */
    device_t      bus;
    device_t      dev;
    /* u_char num */
    unsigned char num;
};

int sdhci_init_slot(device_t dev, struct sdhci_slot *slot, int num)
{
    struct _sdhci_slot_head *h = (struct _sdhci_slot_head *)(void *)slot;
    h->bus          = dev;
    h->dev          = dev;
    h->num          = (unsigned char)num;
    h->_mtx_initialised = 1;
    return 0;
}

/* =========================================================================
 * sdhci_start_slot — no-op (we bypass the MMC bus entirely)
 * ========================================================================= */

void sdhci_start_slot(struct sdhci_slot *slot __attribute__((unused))) {}

/* =========================================================================
 * sdhci_cleanup_slot / sdhci_finish_data — no-op
 * ========================================================================= */

int sdhci_cleanup_slot(struct sdhci_slot *slot __attribute__((unused)))
{ return 0; }

void sdhci_finish_data(struct sdhci_slot *slot __attribute__((unused))) {}

/* =========================================================================
 * sdhci_generic_* — linker stubs; never called in polled path
 * ========================================================================= */

void sdhci_generic_intr(struct sdhci_slot *slot __attribute__((unused))) {}

int sdhci_generic_update_ios(device_t brdev __attribute__((unused)),
                              device_t reqdev __attribute__((unused)))
{ return 0; }

int sdhci_generic_request(device_t brdev __attribute__((unused)),
                          device_t reqdev __attribute__((unused)),
                          struct mmc_request *req __attribute__((unused)))
{ return -EIO; }  /* unreachable; polled path never calls this */

int sdhci_generic_get_ro(device_t brdev __attribute__((unused)),
                         device_t reqdev __attribute__((unused)))
{ return 0; }

int sdhci_generic_acquire_host(device_t brdev __attribute__((unused)),
                               device_t reqdev __attribute__((unused)))
{ return 0; }

int sdhci_generic_release_host(device_t brdev __attribute__((unused)),
                               device_t reqdev __attribute__((unused)))
{ return 0; }

int sdhci_generic_read_ivar(device_t bus     __attribute__((unused)),
                            device_t child   __attribute__((unused)),
                            int which        __attribute__((unused)),
                            uintptr_t *result)
{ if (result) *result = 0; return 0; }

int sdhci_generic_write_ivar(device_t bus   __attribute__((unused)),
                             device_t child  __attribute__((unused)),
                             int which       __attribute__((unused)),
                             uintptr_t value __attribute__((unused)))
{ return 0; }

/* =========================================================================
 * Static SDHCI device instance (analogous to _bsc_device in BSC shim)
 * ========================================================================= */

static struct device _sdhci_device = {
    .d_softc    = NULL,
    .d_methods  = NULL,
    .d_nameunit = "sdhci_bcm0",
};

/* =========================================================================
 * uk_sdcard_init
 *
 * Phase 1: DEVICE_PROBE + DEVICE_ATTACH via KOBJ
 *   bcm_sdhci_attach() powers on EMMC via mailbox and queries the base clock.
 *
 * Phase 2: direct MMIO — GPIO mux, EMMC host reset, clock divisors, SD
 *   card protocol (CMD0 → CMD16).  Mirrors the working implementation in
 *   the now-retired bcm_sdcard.c.
 * ========================================================================= */

int uk_sdcard_init(void)
{
    int rc;
    uint32_t resp[4];

    if (_inited) return 0;

    /* ── Phase 1: KOBJ probe + attach ────────────────────────────────────── */
    if (shim_sdhci_driver_reg.sdr_methods == NULL) {
        uk_pr_err("uk_sdcard_init: DRIVER_MODULE constructor did not run\n");
        return -ENXIO;
    }

    _sdhci_device.d_methods = shim_sdhci_driver_reg.sdr_methods;
    _sdhci_device.d_softc   = uk_calloc(uk_alloc_get_default(), 1,
                                         shim_sdhci_driver_reg.sdr_softc_size);
    if (!_sdhci_device.d_softc) {
        uk_pr_err("uk_sdcard_init: failed to allocate softc\n");
        return -ENOMEM;
    }

    rc = DEVICE_PROBE(&_sdhci_device);
    if (rc != 0) {
        uk_pr_err("uk_sdcard_init: DEVICE_PROBE = %d\n", rc);
        goto fail_alloc;
    }

    rc = DEVICE_ATTACH(&_sdhci_device);
    if (rc != 0) {
        uk_pr_err("uk_sdcard_init: DEVICE_ATTACH = %d\n", rc);
        goto fail_alloc;
    }

    uk_pr_info("uk_sdcard_init: BCM2835 SDHCI attached\n");

    /* ── Phase 2: SD card hardware initialisation via direct MMIO ────────── */

    /* 2a. Set EMMC clock to 50 MHz via VideoCore mailbox */
    mbox[0] = 9 * 4;
    mbox[1] = MBOX_REQUEST;
    mbox[2] = MBOX_TAG_SETCLKRATE;     /* 0x38002 */
    mbox[3] = 12;
    mbox[4] = 8;
    mbox[5] = CLK_ID_EMMC;
    mbox[6] = CLK_BASE_HZ;             /* 50 MHz request */
    mbox[7] = 0;                       /* skip_turbo = 0 */
    mbox[8] = 0;
    if (!mbox_call(MBOX_CH_PROP)) {
        uk_pr_err("sdcard: mailbox set-clock failed\n");
        rc = -EIO;
        goto fail_alloc;
    }
    _us_delay(1000);

    /* 2b. Read back actual EMMC clock — firmware may round to nearest PLL tap */
    mbox[0] = 8 * 4;
    mbox[1] = MBOX_REQUEST;
    mbox[2] = 0x00030002U;             /* GET_CLOCK_RATE */
    mbox[3] = 8;
    mbox[4] = 8;
    mbox[5] = CLK_ID_EMMC;
    mbox[6] = 0;
    mbox[7] = 0;
    if (mbox_call(MBOX_CH_PROP) && mbox[6] != 0)
        _clk_hz = mbox[6];
    else
        _clk_hz = CLK_BASE_HZ;
    uk_pr_info("sdcard: EMMC base clock = %u Hz\n", _clk_hz);

    /* Compute clock divisors:  div = ceil(clk_hz / (2 * target_hz))
     *   init ≤  400 kHz  →  target×2 =    800 000
     *   data ≤   25 MHz  →  target×2 = 50 000 000  */
    uint32_t div_init = (_clk_hz + 799999U)   / 800000U;
    uint32_t div_data = (_clk_hz + 49999999U) / 50000000U;
    if (div_init < 1U) div_init = 1U;
    if (div_data < 1U) div_data = 1U;
    uk_pr_info("sdcard: divisors init=%u (→%u Hz)  data=%u (→%u Hz)\n",
               div_init, _clk_hz / (2U * div_init),
               div_data, _clk_hz / (2U * div_data));

    /* 3. GPIO 48–53 → ALT3 (SD1_CLK/CMD/DATA0-3 on BCM2837)
     *
     * GPFSEL4 [0x3F200010]: GPIO 40–49; bits 29:24 = GPIO49, GPIO48 */
    volatile uint32_t *gp4 = (volatile uint32_t *)0x3F200010UL;
    uint32_t f = *gp4;
    f &= ~((7U<<24)|(7U<<27));
    f |=   (7U<<24)|(7U<<27);          /* ALT3 = 7 */
    *gp4 = f;

    /* GPFSEL5 [0x3F200014]: GPIO 50–59; bits 11:0 = GPIO53–50 */
    volatile uint32_t *gp5 = (volatile uint32_t *)0x3F200014UL;
    f = *gp5;
    f &= ~((7U<<0)|(7U<<3)|(7U<<6)|(7U<<9));
    f |=   (7U<<0)|(7U<<3)|(7U<<6)|(7U<<9);
    *gp5 = f;

    /* GPIO 48 (CLK): no pull */
    volatile uint32_t *gppud     = (volatile uint32_t *)0x3F200094UL;
    volatile uint32_t *gppudclk1 = (volatile uint32_t *)0x3F20009CUL;
    *gppud = 0; _us_delay(5);
    *gppudclk1 = (1U << 16); _us_delay(5); /* GPIO48 = bit16 in bank1 */
    *gppud = 0; *gppudclk1 = 0;

    /* GPIO 49–53 (CMD, DATA0-3): pull-up */
    *gppud = 2; _us_delay(5);
    *gppudclk1 = (0x1FU << 17); _us_delay(5); /* GPIO49-53 = bits 17-21 */
    *gppud = 0; *gppudclk1 = 0;
    _us_delay(200);

    /* 4. Reset EMMC host controller */
    _wr_delay_us = 0;
    _emmc_wr(EMMC_CONTROL0, 0);
    _emmc_wr(EMMC_CONTROL1, _emmc_rd(EMMC_CONTROL1) | C1_SRST_ALL);
    _us_delay(100);
    {
        uint32_t deadline = SYSTIMER_CLO + 100000U;
        while (_emmc_rd(EMMC_CONTROL1) & (C1_SRST_ALL|C1_SRST_CMD|C1_SRST_DATA)) {
            if ((int32_t)(SYSTIMER_CLO - deadline) >= 0) {
                uk_pr_err("sdcard: reset timeout\n");
                rc = -EIO;
                goto fail_alloc;
            }
            _us_delay(10);
        }
    }

    /* 5. Initialisation clock (≤400 kHz) */
    _emmc_wr(EMMC_INT_EN,   0xFFFFFFFFU);
    _emmc_wr(EMMC_INT_MASK, 0xFFFFFFFFU);
    _set_clock(div_init);
    _us_delay(10000);   /* SD spec: ≥74 clocks ≈ 185 µs at 400 kHz */
    _emmc_wr(EMMC_INTERRUPT, 0xFFFFFFFFU);

    /* 6. CMD0 — GO_IDLE_STATE */
    _cmd(0, RT_NONE, 0, NULL);
    _us_delay(2000);

    /* 7. CMD8 — SEND_IF_COND */
    rc = _cmd(8, RT_48 | TM_CRC_EN | TM_IDX_EN, 0x000001AAU, resp);
    int v2 = (rc == 0 && (resp[0] & 0xFFU) == 0xAAU);
    _us_delay(1000);

    /* 8. ACMD41 loop — wait for card ready (max ~1 s) */
    _is_hc = 0;
    for (int i = 0; i < 100; i++) {
        rc = _cmd(55, RT_48 | TM_CRC_EN | TM_IDX_EN, 0, resp);
        if (rc) { _us_delay(10000); continue; }
        uint32_t ocr = 0x00FF8000U;
        if (v2) ocr |= (1U << 30);    /* HCS bit */
        rc = _cmd(41, RT_48, ocr, resp);
        if (rc) { _us_delay(10000); continue; }
        if (resp[0] & (1U << 31)) {
            _is_hc = (resp[0] >> 30) & 1U;
            break;
        }
        _us_delay(10000);
    }
    if (!(resp[0] & (1U << 31))) {
        uk_pr_err("sdcard: card never became ready\n");
        rc = -EIO;
        goto fail_alloc;
    }

    /* 9. CMD2 — ALL_SEND_CID */
    rc = _cmd(2, RT_136, 0, resp);
    if (rc) { uk_pr_err("sdcard: CMD2 failed (%d)\n", rc); goto fail_alloc; }
    _us_delay(1000);

    /* 10. CMD3 — SEND_RELATIVE_ADDR */
    rc = _cmd(3, RT_48 | TM_CRC_EN | TM_IDX_EN, 0, resp);
    if (rc) { uk_pr_err("sdcard: CMD3 failed (%d)\n", rc); goto fail_alloc; }
    _rca = resp[0] & 0xFFFF0000U;
    _us_delay(1000);

    /* 11. CMD7 — SELECT_CARD */
    rc = _cmd(7, RT_48B | TM_CRC_EN | TM_IDX_EN, _rca, resp);
    if (rc) { uk_pr_err("sdcard: CMD7 failed (%d)\n", rc); goto fail_alloc; }
    _us_delay(1000);

    /* 12. ACMD6 — SET_BUS_WIDTH 4-bit */
    rc = _cmd(55, RT_48 | TM_CRC_EN | TM_IDX_EN, _rca, resp);
    if (!rc) rc = _cmd(6, RT_48 | TM_CRC_EN | TM_IDX_EN, 2, resp);
    if (!rc) {
        uint32_t c0 = _emmc_rd(EMMC_CONTROL0);
        c0 |= (1U << 1);               /* 4-bit bus width */
        _emmc_wr(EMMC_CONTROL0, c0);
    }
    _us_delay(1000);

    /* 13. Raise clock to data-transfer speed (≤25 MHz) */
    _set_clock(div_data);
    _us_delay(10000);

    /* 14. CMD16 — SET_BLOCKLEN (also syncs card to new clock edge) */
    rc = _cmd(16, RT_48 | TM_CRC_EN | TM_IDX_EN, 512, resp);
    if (rc) { uk_pr_err("sdcard: CMD16 failed (%d)\n", rc); goto fail_alloc; }

    _inited = 1;
    uk_pr_info("sdcard: ready — %s, data clock %u Hz\n",
               _is_hc ? "SDHC/SDXC" : "SDSC", _clk_hz / (2U * div_data));
    return 0;

fail_alloc:
    uk_free(uk_alloc_get_default(), _sdhci_device.d_softc);
    _sdhci_device.d_softc = NULL;
    return rc ? rc : -EIO;
}

/* =========================================================================
 * Data-path reset (SRST_DATA) — used for CMD17 error recovery
 * ========================================================================= */

static void _reset_data(void)
{
    _emmc_wr(EMMC_CONTROL1, _emmc_rd(EMMC_CONTROL1) | C1_SRST_DATA);
    _us_delay(100);
    uint32_t deadline = SYSTIMER_CLO + 100000U;
    while (_emmc_rd(EMMC_CONTROL1) & C1_SRST_DATA) {
        if ((int32_t)(SYSTIMER_CLO - deadline) >= 0) break;
        _us_delay(10);
    }
    _emmc_wr(EMMC_INTERRUPT, 0xFFFFFFFFU);
}

/* =========================================================================
 * uk_sdcard_read_block — CMD17 sector read (512 bytes) via polled PIO
 * ========================================================================= */

int uk_sdcard_read_block(uint32_t lba, uint8_t *buf)
{
    if (!_inited) return -ENODEV;

    uint32_t addr = _is_hc ? lba : lba * 512U;
    uint32_t resp[1];
    uint32_t irq = 0;
    int rc;

    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) {
            uk_pr_err("sdcard: CMD17 attempt %d, resetting data path\n",
                      attempt + 1);
            _reset_data();
            _us_delay(5000);
        }

        _emmc_wr(EMMC_BLKSIZECNT, (1U << 16) | 512U);

        rc = _cmd(17,
                  RT_48 | TM_CRC_EN | TM_IDX_EN | TM_ISDATA | TM_DAT_DIR_RD,
                  addr, resp);
        if (rc) continue;

        rc = _wait_irq(INT_READ_RDY, &irq);
        if (rc || (irq & INT_ERR)) {
            uk_sdcard_dbg_phase = 2;
            uk_sdcard_dbg_irq   = irq;
            uk_pr_err("sdcard: READ_RDY error (try %d): "
                      "INTERRUPT=0x%08X STATUS=0x%08X\n",
                      attempt + 1, irq, _emmc_rd(EMMC_STATUS));
            _emmc_wr(EMMC_INTERRUPT, irq);
            rc = -EIO;
            continue;
        }
        _emmc_wr(EMMC_INTERRUPT, INT_READ_RDY);

        uint32_t *w = (uint32_t *)(void *)buf;
        for (int i = 0; i < 128; i++)
            w[i] = _emmc_rd(EMMC_DATA);

        rc = _wait_irq(INT_DATA_DONE, &irq);
        if (rc || (irq & INT_ERR)) {
            uk_sdcard_dbg_phase = 3;
            uk_sdcard_dbg_irq   = irq;
            uk_pr_err("sdcard: DATA_DONE error (try %d): "
                      "INTERRUPT=0x%08X\n", attempt + 1, irq);
            _emmc_wr(EMMC_INTERRUPT, irq);
            rc = -EIO;
            continue;
        }
        _emmc_wr(EMMC_INTERRUPT, irq);
        return 0;   /* success */
    }

    uk_pr_err("sdcard: CMD17 lba=%u failed after 3 attempts "
              "(last phase=%u irq=0x%08X)\n",
              lba, uk_sdcard_dbg_phase, uk_sdcard_dbg_irq);
    return rc ? rc : -EIO;
}

/* =========================================================================
 * uk_sdcard_write_block — CMD24 sector write (512 bytes) via polled PIO
 * ========================================================================= */

int uk_sdcard_write_block(uint32_t lba, const uint8_t *buf)
{
    if (!_inited) return -ENODEV;

    uint32_t addr = _is_hc ? lba : lba * 512U;
    uint32_t resp[1];
    uint32_t irq = 0;
    int rc;

    _emmc_wr(EMMC_BLKSIZECNT, (1U << 16) | 512U);

    rc = _cmd(24,
              RT_48 | TM_CRC_EN | TM_IDX_EN | TM_ISDATA,
              addr, resp);
    if (rc) return rc;

    rc = _wait_irq(INT_WRITE_RDY, &irq);
    if (rc || (irq & INT_ERR)) {
        _emmc_wr(EMMC_INTERRUPT, irq);
        return rc ? rc : -EIO;
    }
    _emmc_wr(EMMC_INTERRUPT, INT_WRITE_RDY);

    const uint32_t *w = (const uint32_t *)(const void *)buf;
    for (int i = 0; i < 128; i++)
        _emmc_wr_data(w[i]);

    rc = _wait_irq(INT_DATA_DONE, &irq);
    _emmc_wr(EMMC_INTERRUPT, irq);
    return (rc || (irq & INT_ERR)) ? (rc ? rc : -EIO) : 0;
}
