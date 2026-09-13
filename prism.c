// PRISM (Programmable Reconfigurable Indexed State Machine) peripheral driver
// for TinyQV.  See prism.h for the API and PRISM_CONFIG selection; the
// register maps live in prism_cfg_*.h.
//
//                        /\.
//                       /  \.
//                   ..-/----\-..
//               --''  /      \  ''--
//                    /________\.

#include <stddef.h>
#include <stdio.h>
#include "prism.h"
#include "csr.h"

// ==========================================================================
// Shard selection
// ==========================================================================
#if PRISM_NUM_SHARDS > 1
static int prism_shard = 0;

void prism_set_shard(int shard)
{
    prism_shard = (shard > 0 && shard < PRISM_NUM_SHARDS) ? shard : 0;
}

int prism_get_shard(void)
{
    return prism_shard;
}

uint32_t prism_shard_reg(uint32_t offset)
{
    return PRISM_SHARD_BASE(prism_shard) + offset;
}

void prism_set_fractured(bool fractured, uint32_t out_mask0, uint32_t cond_mask0,
                         uint32_t out_mask1, uint32_t cond_mask1)
{
    prism_write32(PRISM_REG_OUT_MASK0, out_mask0);
    prism_write32(PRISM_REG_COND_MASK0, cond_mask0);
    prism_write32(PRISM_REG_OUT_MASK1, out_mask1);
    prism_write32(PRISM_REG_COND_MASK1, cond_mask1);
    prism_write32(PRISM_REG_FRAC_CFG, fractured ? 1u : 0u);
}

bool prism_is_fractured(void)
{
    return (prism_read32(PRISM_REG_FRAC_CFG) & 1u) != 0;
}
#else
void prism_set_shard(int shard) { (void)shard; }
int prism_get_shard(void) { return 0; }
uint32_t prism_shard_reg(uint32_t offset) { return offset; }
void prism_set_fractured(bool fractured, uint32_t out_mask0, uint32_t cond_mask0,
                         uint32_t out_mask1, uint32_t cond_mask1)
{
    (void)fractured; (void)out_mask0; (void)cond_mask0; (void)out_mask1; (void)cond_mask1;
}
bool prism_is_fractured(void) { return false; }
#endif

// ==========================================================================
// Configuration (chroma) loading: shift loader (sky25a)
// ==========================================================================
#if PRISM_HAS_CFG_SHIFT_LOADER

// A CFG_LSW write starts the latch loader, which pulses all stage latch
// enables in sequence (~50 clocks at the peripheral clock).  The written
// data must remain stable until the shift completes, so wait before doing
// any further PRISM access.
#define PRISM_CFG_SHIFT_US  2

void prism_cfg_write(uint32_t msw, uint32_t lsw)
{
    prism_write32(PRISM_REG_CFG_MSW, msw & PRISM_STEW_MSW_MASK);
    prism_write32(PRISM_REG_CFG_LSW, lsw);
    delay_us(PRISM_CFG_SHIFT_US);
}

void prism_cfg_read(uint32_t *msw, uint32_t *lsw)
{
    *msw = prism_read32(PRISM_REG_CFG_MSW);
    *lsw = prism_read32(PRISM_REG_CFG_LSW);
}

int prism_load_config(const uint32_t *chroma, const uint32_t *expected)
{
    int errors = 0;

    for (int i = 0; i < PRISM_NUM_STATES; ++i) {
        if (expected) {
            uint32_t msw, lsw;
            prism_cfg_read(&msw, &lsw);
            if (msw != (expected[2 * i] & PRISM_STEW_MSW_MASK) ||
                lsw != expected[2 * i + 1])
                errors |= PRISM_ERR_WORD(i);
        }
        prism_cfg_write(chroma[2 * i], chroma[2 * i + 1]);
    }

    // The first word written has now been shifted through to the last stage
    uint32_t msw, lsw;
    prism_cfg_read(&msw, &lsw);
    if (msw != (chroma[0] & PRISM_STEW_MSW_MASK) || lsw != chroma[1])
        errors |= PRISM_ERR_FINAL;

    return errors;
}

int prism_verify_config(const uint32_t *expected)
{
    int errors = 0;

    for (int i = 0; i < PRISM_NUM_STATES; ++i) {
        uint32_t msw, lsw;
        prism_cfg_read(&msw, &lsw);
        if (expected &&
            (msw != (expected[2 * i] & PRISM_STEW_MSW_MASK) ||
             lsw != expected[2 * i + 1]))
            errors |= PRISM_ERR_WORD(i);

        // Recirculate the word so the array is unchanged after the shifts
        prism_cfg_write(msw, lsw);
    }

    return errors;
}

static void prism_test_pattern(uint32_t seed, int i, uint32_t *msw, uint32_t *lsw)
{
    uint32_t b = (seed + (uint32_t)i * 0x11u) & 0xFFu;
    uint32_t w = b | (b << 8) | (b << 16) | (b << 24);
    *lsw = w ^ (seed << 16);
    *msw = (w ^ (w >> 3) ^ seed) & PRISM_STEW_MSW_MASK;
}

int prism_test_config(void)
{
    uint32_t msw, lsw, exp_msw, exp_lsw;
    int errors = 0;

    prism_disable();

    // Load the first pattern "design"
    for (int i = 0; i < PRISM_NUM_STATES; ++i) {
        prism_test_pattern(0xA5, i, &msw, &lsw);
        prism_cfg_write(msw, lsw);
    }

    // Load a second pattern, validating the first as it shifts out of the
    // STEW array
    for (int i = 0; i < PRISM_NUM_STATES; ++i) {
        prism_cfg_read(&msw, &lsw);
        prism_test_pattern(0xA5, i, &exp_msw, &exp_lsw);
        printf("Expected 0x%08lX%08lX  got 0x%08lX%08lX\r\n", exp_msw, exp_lsw, msw, lsw);
        if (msw != exp_msw || lsw != exp_lsw)
            errors |= PRISM_ERR_WORD(i);

        prism_test_pattern(0x3C, i, &msw, &lsw);
        prism_cfg_write(msw, lsw);
    }

    printf("\n");
    // Verify the second pattern in place by recirculating it
    for (int i = 0; i < PRISM_NUM_STATES; ++i) {
        prism_cfg_read(&msw, &lsw);
        prism_test_pattern(0x3C, i, &exp_msw, &exp_lsw);
        printf("Expected 0x%08lX%08lX  got 0x%08lX%08lX\r\n", exp_msw, exp_lsw, msw, lsw);
        if (msw != exp_msw || lsw != exp_lsw)
            errors |= PRISM_ERR_FINAL;

        prism_cfg_write(msw, lsw);
    }

    return errors;
}
#endif // PRISM_HAS_CFG_SHIFT_LOADER

// ==========================================================================
// Configuration (chroma) loading: CFGMEM loader (janestreet)
// ==========================================================================
#if PRISM_HAS_CFGMEM_LOADER

static inline void cfgmem_write(uint32_t reg, uint32_t value)
{
    *(volatile uint32_t *)(CFGMEM_BASE_ADDRESS + reg) = value;
}

static inline uint32_t cfgmem_read(uint32_t reg)
{
    return *(volatile uint32_t *)(CFGMEM_BASE_ADDRESS + reg);
}

static inline void cfgmem_ctrl(uint8_t value)
{
    *(volatile uint8_t *)(CFGMEM_BASE_ADDRESS + CFGMEM_REG_CTRL) = value;
}

static inline uint8_t cfgmem_ctrl_read(void)
{
    return *(volatile uint8_t *)(CFGMEM_BASE_ADDRESS + CFGMEM_REG_CTRL);
}

// Wait for the shift-load FSM to finish walking the rows
static inline void cfgmem_wait(void)
{
    while (cfgmem_ctrl_read() & CFGMEM_CTRL_BUSY)
        ;
}

// Shift one word into lo macro i (row 0; older rows move up)
static void cfgmem_shift_lo(uint32_t i, uint32_t value)
{
    cfgmem_write(CFGMEM_REG_LO(i), value);
    cfgmem_wait();
}

// Shift a word into hi macro i (through the bypassed lo macro)
static void cfgmem_shift_hi(uint32_t i, uint32_t value)
{
    cfgmem_write(CFGMEM_REG_HI(i), value);
    cfgmem_wait();
}

// Read row r of hi macro i (no bypass)
static uint32_t cfgmem_read_hi(uint32_t i, uint32_t row)
{
    cfgmem_ctrl(CFGMEM_CTRL_ADDR_SEL | CFGMEM_CTRL_ADDR(row));
    return cfgmem_read(CFGMEM_REG_LO(i));
}

// Read row r of lo macro i, looking through the bypassed hi macro
static uint32_t cfgmem_read_lo(uint32_t i, uint32_t row)
{
    cfgmem_ctrl(CFGMEM_CTRL_BYP_HI | CFGMEM_CTRL_ADDR_SEL | CFGMEM_CTRL_ADDR(row));
    return cfgmem_read(CFGMEM_REG_LO(i));
}

// Hand the row address back to the PRISM and clear the bypasses
static void cfgmem_release(void)
{
    cfgmem_ctrl(0);
}

// Load bank B (hi macros) from hi_words and bank A (lo macros) from
// lo_words, each PRISM_BANK_STATES states highest state first.  The last
// word shifted in lands in row 0.  Either pointer may be NULL to leave that
// bank alone (bank B must be written before bank A).
static void cfgmem_load_banks(const uint32_t *lo_words, const uint32_t *hi_words)
{
    if (hi_words) {
        cfgmem_ctrl(CFGMEM_CTRL_BYP_LO);
        for (uint32_t k = 0; k < PRISM_BANK_STATES; k++)
            for (uint32_t j = 0; j < PRISM_STEW_WORDS; j++)
                cfgmem_shift_hi(PRISM_STEW_WORDS - 1 - j, *hi_words++);
    }
    cfgmem_ctrl(0);
    if (lo_words) {
        for (uint32_t k = 0; k < PRISM_BANK_STATES; k++)
            for (uint32_t j = 0; j < PRISM_STEW_WORDS; j++)
                cfgmem_shift_lo(PRISM_STEW_WORDS - 1 - j, *lo_words++);
    }
    cfgmem_release();
}

// Compare one bank against words[] (highest state first); returns
// PRISM_ERR_WORD(row) per mismatching row.
static int cfgmem_verify_bank(const uint32_t *words, bool hi)
{
    int errors = 0;
    for (uint32_t s = 0; s < PRISM_BANK_STATES; s++) {
        const uint32_t *ws = words + (PRISM_BANK_STATES - 1 - s) * PRISM_STEW_WORDS;
        for (uint32_t j = 0; j < PRISM_STEW_WORDS; j++) {
            uint32_t inst = PRISM_STEW_WORDS - 1 - j;
            uint32_t got  = hi ? cfgmem_read_hi(inst, s) : cfgmem_read_lo(inst, s);
            if (got != ws[j])
                errors |= PRISM_ERR_WORD(s);
        }
    }
    cfgmem_release();
    return errors;
}

// A full-geometry chroma lists states 31..0; states 31..16 go to bank B
#define CHROMA_HI(c)  (c)
#define CHROMA_LO(c)  ((c) + PRISM_BANK_STATES * PRISM_STEW_WORDS)

int prism_load_config(const uint32_t *chroma, const uint32_t *expected)
{
    (void)expected;     // random access readback verifies the new contents
    cfgmem_load_banks(CHROMA_LO(chroma), CHROMA_HI(chroma));
    int errors = cfgmem_verify_bank(CHROMA_LO(chroma), false) |
                 cfgmem_verify_bank(CHROMA_HI(chroma), true);
    return errors ? (errors | PRISM_ERR_FINAL) : 0;
}

int prism_verify_config(const uint32_t *expected)
{
    if (!expected)
        return 0;
    return cfgmem_verify_bank(CHROMA_LO(expected), false) |
           cfgmem_verify_bank(CHROMA_HI(expected), true);
}

int prism_load_shards(const uint32_t *chroma_a, uint32_t ctrl_a, uint32_t pinmux_a,
                      const uint32_t *chroma_b, uint32_t ctrl_b, uint32_t pinmux_b)
{
    int errors;

    prism_disable();
    cfgmem_load_banks(CHROMA_LO(chroma_a), CHROMA_LO(chroma_b));
    errors = cfgmem_verify_bank(CHROMA_LO(chroma_a), false) |
             cfgmem_verify_bank(CHROMA_LO(chroma_b), true);
    if (errors)
        errors |= PRISM_ERR_FINAL;

    prism_set_fractured(true, PRISM_OUT_MASK, 0x3u, PRISM_OUT_MASK, 0x3u);

    int saved = prism_get_shard();
    prism_set_shard(0);
    prism_write32(prism_shard_reg(PRISM_SH_CFG0), ctrl_a & PRISM_CTRL_CFG_MASK);
    prism_write32(prism_shard_reg(PRISM_SH_PINMUX), pinmux_a);
    if ((prism_read32(prism_shard_reg(PRISM_SH_CFG0)) & PRISM_CTRL_CFG_MASK) !=
            (ctrl_a & PRISM_CTRL_CFG_MASK))
        errors |= PRISM_ERR_CTRL;
    prism_set_shard(1);
    prism_write32(prism_shard_reg(PRISM_SH_CFG0), ctrl_b & PRISM_CTRL_CFG_MASK);
    prism_write32(prism_shard_reg(PRISM_SH_PINMUX), pinmux_b);
    if ((prism_read32(prism_shard_reg(PRISM_SH_CFG0)) & PRISM_CTRL_CFG_MASK) !=
            (ctrl_b & PRISM_CTRL_CFG_MASK))
        errors |= PRISM_ERR_CTRL;
    prism_set_shard(saved);

    if (errors == 0)
        prism_enable();
    return errors;
}

static uint32_t cfgmem_test_pattern(uint32_t seed, uint32_t s, uint32_t j)
{
    uint32_t b = (seed + s * 0x11u + j * 0x47u) & 0xFFu;
    return (b | (b << 8) | (b << 16) | (b << 24)) ^ (seed << 16) ^ (s << 24);
}

int prism_test_config(void)
{
    static uint32_t pat[PRISM_CHROMA_WORDS];
    int errors = 0;

    prism_disable();

    // First pattern into both banks, verify
    for (uint32_t s = 0; s < PRISM_NUM_STATES; s++)
        for (uint32_t j = 0; j < PRISM_STEW_WORDS; j++)
            pat[(PRISM_NUM_STATES - 1 - s) * PRISM_STEW_WORDS + j] = cfgmem_test_pattern(0xA5, s, j);
    cfgmem_load_banks(CHROMA_LO(pat), CHROMA_HI(pat));
    if (cfgmem_verify_bank(CHROMA_LO(pat), false) | cfgmem_verify_bank(CHROMA_HI(pat), true))
        errors |= PRISM_ERR_WORD(0);

    // Second pattern displaces the first, verify again
    for (uint32_t s = 0; s < PRISM_NUM_STATES; s++)
        for (uint32_t j = 0; j < PRISM_STEW_WORDS; j++)
            pat[(PRISM_NUM_STATES - 1 - s) * PRISM_STEW_WORDS + j] = cfgmem_test_pattern(0x3C, s, j);
    cfgmem_load_banks(CHROMA_LO(pat), CHROMA_HI(pat));
    if (cfgmem_verify_bank(CHROMA_LO(pat), false) | cfgmem_verify_bank(CHROMA_HI(pat), true))
        errors |= PRISM_ERR_FINAL;

    return errors;
}
#endif // PRISM_HAS_CFGMEM_LOADER

// ==========================================================================
// Chroma load front ends (common)
// ==========================================================================
int prism_load_chroma_ex(const uint32_t *chroma, uint32_t ctrl_reg, uint32_t pinmux_reg)
{
    prism_disable();

    int errors = prism_load_config(chroma, NULL);

    prism_set_ctrl(ctrl_reg & PRISM_CTRL_CFG_MASK);
    if ((prism_get_ctrl() & PRISM_CTRL_CFG_MASK) !=
            (ctrl_reg & PRISM_CTRL_CFG_MASK))
        errors |= PRISM_ERR_CTRL;
#if PRISM_HAS_PINMUX
    prism_set_pinmux(pinmux_reg);
#else
    (void)pinmux_reg;
#endif

    if (errors == 0)
        prism_enable();

    return errors;
}

int prism_load_chroma_verify(const uint32_t *chroma, const uint32_t *expected,
                             uint32_t ctrl_reg)
{
    prism_disable();

    int errors = prism_load_config(chroma, expected);

    prism_set_ctrl(ctrl_reg & PRISM_CTRL_CFG_MASK);
    if ((prism_get_ctrl() & PRISM_CTRL_CFG_MASK) !=
            (ctrl_reg & PRISM_CTRL_CFG_MASK))
        errors |= PRISM_ERR_CTRL;

    if (errors == 0)
        prism_enable();

    return errors;
}

int prism_load_chroma(const uint32_t *chroma, uint32_t ctrl_reg)
{
    return prism_load_chroma_verify(chroma, NULL, ctrl_reg);
}

// ==========================================================================
// Control word / enable / interrupt
// ==========================================================================
#if PRISM_NUM_SHARDS > 1

void prism_set_ctrl(uint32_t ctrl)
{
    prism_write32(prism_shard_reg(PRISM_SH_CFG0), ctrl & PRISM_CTRL_CFG_MASK);
    prism_write32(PRISM_REG_CTRL, ctrl & PRISM_CTRL_ENABLE);
}

uint32_t prism_get_ctrl(void)
{
    uint32_t common = prism_read32(PRISM_REG_CTRL);
    uint32_t irq    = prism_shard ? ((common & PRISM_CTRL_INTERRUPT1) ? PRISM_CTRL_INTERRUPT : 0u)
                                  : (common & PRISM_CTRL_INTERRUPT);
    return (prism_read32(prism_shard_reg(PRISM_SH_CFG0)) & PRISM_CTRL_CFG_MASK) |
           (common & PRISM_CTRL_ENABLE) | irq;
}

void prism_enable(void)
{
    prism_write32(PRISM_REG_CTRL, PRISM_CTRL_ENABLE);
}

void prism_disable(void)
{
    prism_write32(PRISM_REG_CTRL, 0);
}

bool prism_is_enabled(void)
{
    return (prism_read32(PRISM_REG_CTRL) & PRISM_CTRL_ENABLE) != 0;
}

bool prism_interrupt_pending(void)
{
    return (prism_read32(PRISM_REG_CTRL) &
            (prism_shard ? PRISM_CTRL_INTERRUPT1 : PRISM_CTRL_INTERRUPT)) != 0;
}

void prism_clear_interrupt(void)
{
    prism_write8(prism_shard ? PRISM_REG_INT_CLR1 : PRISM_REG_INT_CLR, 0x80);
}

void prism_set_pinmux(uint32_t pinmux)
{
    prism_write32(prism_shard_reg(PRISM_SH_PINMUX), pinmux);
}

uint32_t prism_get_pinmux(void)
{
    return prism_read32(prism_shard_reg(PRISM_SH_PINMUX));
}

void prism_enable_interrupt(void)
{
    enable_interrupt(PRISM_PERIPHERAL_NUM + prism_shard);
}

void prism_disable_interrupt(void)
{
    disable_interrupt(PRISM_PERIPHERAL_NUM + prism_shard);
}

#else // single shard: CTRL holds config bits and ENABLE

void prism_set_ctrl(uint32_t ctrl)
{
    prism_write32(PRISM_REG_CTRL, ctrl & (PRISM_CTRL_CFG_MASK | PRISM_CTRL_ENABLE));
}

uint32_t prism_get_ctrl(void)
{
    return prism_read32(PRISM_REG_CTRL);
}

void prism_enable(void)
{
    prism_set_ctrl((prism_get_ctrl() & PRISM_CTRL_CFG_MASK) | PRISM_CTRL_ENABLE);
}

void prism_disable(void)
{
    prism_set_ctrl(prism_get_ctrl() & PRISM_CTRL_CFG_MASK);
}

bool prism_is_enabled(void)
{
    return (prism_get_ctrl() & PRISM_CTRL_ENABLE) != 0;
}

bool prism_interrupt_pending(void)
{
    return (prism_get_ctrl() & PRISM_CTRL_INTERRUPT) != 0;
}

void prism_clear_interrupt(void)
{
    prism_write8(PRISM_REG_INT_CLR, 0x80);
}

void prism_enable_interrupt(void)
{
    enable_interrupt(PRISM_PERIPHERAL_NUM);
}

void prism_disable_interrupt(void)
{
    disable_interrupt(PRISM_PERIPHERAL_NUM);
}

#endif

void prism_claim_pins(uint8_t pin_mask)
{
    for (int pin = 1; pin < 8; ++pin)
        if (pin_mask & (1u << pin))
            set_gpio_func(pin, PRISM_PERIPHERAL_NUM);
}

// ==========================================================================
// Counters, compare, shift register, comm register, host bits
// ==========================================================================
#if PRISM_NUM_SHARDS > 1

void prism_set_count1_preload(uint32_t value)
{
    prism_write32(prism_shard_reg(PRISM_SH_PRELOAD), value & PRISM_COUNT1_MASK);
}

uint32_t prism_get_count1_preload(void)
{
    return prism_read32(prism_shard_reg(PRISM_SH_PRELOAD)) & PRISM_COUNT1_MASK;
}

uint32_t prism_get_count1(void)
{
    return prism_read32(prism_shard_reg(PRISM_SH_COUNT1)) & PRISM_COUNT1_MASK;
}

void prism_set_count1(uint32_t value)
{
    prism_write32(prism_shard_reg(PRISM_SH_COUNT1), value);
}

uint8_t prism_get_count2(void)
{
    return prism_read8(prism_shard_reg(PRISM_SH_COUNT2));
}

void prism_set_count2_compare(uint8_t value)
{
    prism_write8(prism_shard_reg(PRISM_SH_COMPARE), value);
}

uint8_t prism_get_count2_compare(void)
{
    return prism_read8(prism_shard_reg(PRISM_SH_COMPARE));
}

void prism_comm_write(uint8_t value)
{
    prism_write8(prism_shard_reg(PRISM_SH_COMM), value);
}

uint8_t prism_comm_read(void)
{
    return prism_read8(prism_shard_reg(PRISM_SH_COMM));
}

void prism_host_write(uint8_t bits)
{
    prism_write32(prism_shard_reg(PRISM_SH_HOST), bits & 3u);
}

uint8_t prism_host_read(void)
{
    return prism_read8(prism_shard_reg(PRISM_SH_HOST)) & 3u;
}

void prism_host_toggle(void)
{
    prism_write8(prism_shard_reg(PRISM_SH_TOGGLE), 0);
}

#else

void prism_set_count1_preload(uint32_t value)
{
    prism_write32(PRISM_REG_PRELOAD, value & PRISM_COUNT1_MASK);
}

uint32_t prism_get_count1_preload(void)
{
    return prism_read32(PRISM_REG_PRELOAD) & PRISM_COUNT1_MASK;
}

uint32_t prism_get_count1(void)
{
    return prism_read32(PRISM_REG_COUNT_VAL) & PRISM_COUNT1_MASK;
}

void prism_set_count1(uint32_t value)
{
    // No direct load on this design: stage it in the preload register
    prism_set_count1_preload(value);
}

uint8_t prism_get_count2(void)
{
    return (uint8_t)(prism_read32(PRISM_REG_COUNT_VAL) >> 24);
}

void prism_set_count2_compare(uint8_t value)
{
    prism_write8(PRISM_REG_COMPARE, value);
}

uint8_t prism_get_count2_compare(void)
{
    return prism_read8(PRISM_REG_COMPARE);
}

void prism_comm_write(uint8_t value)
{
    prism_write8(PRISM_REG_COMM_DATA, value);
}

uint8_t prism_comm_read(void)
{
    return prism_read8(PRISM_REG_COMM_DATA);
}

void prism_host_write(uint8_t bits)
{
    prism_write8(PRISM_REG_HOST_IN, bits & 3u);
}

uint8_t prism_host_read(void)
{
    return prism_read8(PRISM_REG_HOST_IN) & 3u;
}

void prism_host_toggle(void)
{
    // The auto-toggle write also lands in the count2 compare latch, so
    // rewrite the current compare value to preserve it
    prism_write8(PRISM_REG_HOST_TOGGLE, prism_get_count2_compare());
}

#endif

void prism_shift24_write(uint32_t value)
{
    prism_set_count1_preload(value);
}

uint32_t prism_shift24_read(void)
{
    return prism_get_count1();
}

// ==========================================================================
// FIFO
// ==========================================================================
#if PRISM_HAS_TX_FIFO   // janestreet: 16-byte FIFO per shard, RX or TX

uint32_t prism_fifo_status(void)
{
    return prism_read32(prism_shard_reg(PRISM_SH_FIFO_STATUS));
}

uint8_t prism_fifo_level(void)
{
    return (uint8_t)PRISM_FIFO_STAT_COUNT(prism_fifo_status());
}

uint8_t prism_fifo_pop(void)
{
    return prism_read8(prism_shard_reg(PRISM_SH_FIFO));
}

bool prism_fifo_push(uint8_t value)
{
    if (prism_fifo_full())
        return false;
    prism_write8(prism_shard_reg(PRISM_SH_FIFO), value);
    return true;
}

void prism_fifo_flush(void)
{
    prism_write32(prism_shard_reg(PRISM_SH_FIFO_STATUS), 0);
}

void prism_fifo_set_levels(uint8_t almost_empty, uint8_t almost_full)
{
    uint32_t cfg1 = prism_read32(prism_shard_reg(PRISM_SH_CFG1)) & ~0x00FF0000u;
    cfg1 |= ((uint32_t)(almost_empty & 0xFu) << 16) | ((uint32_t)(almost_full & 0xFu) << 20);
    prism_write32(prism_shard_reg(PRISM_SH_CFG1), cfg1);
}

#else                   // sky25a: 3-byte RX FIFO

uint32_t prism_fifo_status(void)
{
    return prism_read8(PRISM_REG_FIFO_STAT);
}

uint8_t prism_fifo_level(void)
{
    return (uint8_t)PRISM_FIFO_STAT_COUNT(prism_fifo_status());
}

uint8_t prism_fifo_pop(void)
{
    return prism_read8(PRISM_REG_FIFO_DATA);
}

bool prism_fifo_push(uint8_t value)
{
    (void)value;
    return false;
}

void prism_fifo_flush(void)
{
}

#endif

bool prism_fifo_empty(void)
{
    return (prism_fifo_status() & PRISM_FIFO_STAT_EMPTY) != 0;
}

bool prism_fifo_full(void)
{
    return (prism_fifo_status() & PRISM_FIFO_STAT_FULL) != 0;
}

int prism_fifo_read(void)
{
    if (prism_fifo_empty())
        return -1;
    return prism_fifo_pop();
}

// ==========================================================================
// CRC
// ==========================================================================
#if PRISM_HAS_CRC

void prism_crc_set_poly(uint32_t poly)
{
    prism_write32(prism_shard_reg(PRISM_SH_CRC_POLY), poly);
}

void prism_crc_set_expected(uint32_t expected)
{
    prism_write32(prism_shard_reg(PRISM_SH_CRC_EXPECTED), expected);
}

void prism_crc_preset(uint32_t value)
{
    prism_write32(prism_shard_reg(PRISM_SH_CRC), value);
}

uint32_t prism_crc_get(void)
{
    return prism_read32(prism_shard_reg(PRISM_SH_CRC));
}

bool prism_crc_ok(void)
{
    return (prism_read32(prism_shard_reg(PRISM_SH_FLAGS)) & PRISM_FLAG_CRC_OK) != 0;
}

#endif

// ==========================================================================
// Debugger
// ==========================================================================
#if PRISM_NUM_SHARDS > 1
#define DBG_CTRL_REG    (prism_shard ? PRISM_REG_DBG_CTRL1 : PRISM_REG_DBG_CTRL)
#else
#define DBG_CTRL_REG    PRISM_REG_DBG_CTRL
#endif

uint32_t prism_dbg_status(void)
{
#if PRISM_NUM_SHARDS > 1
    return (prism_read32(PRISM_REG_DBG_STAT) >> (prism_shard * PRISM_DBG_STAT_SHARD_BITS)) &
           ((1u << PRISM_DBG_STAT_SHARD_BITS) - 1u);
#else
    return prism_read32(PRISM_REG_DBG_STAT);
#endif
}

uint32_t prism_dbg_get_ctrl(void)
{
    return prism_read32(DBG_CTRL_REG) & PRISM_DBG_CTRL_MASK;
}

bool prism_dbg_is_halted(void)
{
    return (prism_dbg_status() & PRISM_DBG_STAT_HALTED) != 0;
}

uint8_t prism_dbg_curr_state(void)
{
    return PRISM_DBG_STAT_CURR_SI(prism_dbg_status());
}

uint8_t prism_dbg_next_state(void)
{
    return PRISM_DBG_STAT_NEXT_SI(prism_dbg_status());
}

bool prism_dbg_break_active(void)
{
    return (prism_dbg_status() & (PRISM_DBG_STAT_BREAK | PRISM_DBG_STAT_BREAK1)) != 0;
}

bool prism_dbg_wait_halt(uint32_t timeout_us)
{
    uint32_t deadline = read_time() + timeout_us;
    while (!prism_dbg_is_halted()) {
        if ((int32_t)(deadline - read_time()) <= 0)
            return prism_dbg_is_halted();
    }
    return true;
}

bool prism_dbg_halt(void)
{
    prism_write32(DBG_CTRL_REG, prism_dbg_get_ctrl() | PRISM_DBG_HALT_REQ);
    return prism_dbg_wait_halt(100);
}

void prism_dbg_resume(void)
{
    // A falling edge on halt_req resumes and clears any active breakpoint
    uint32_t ctrl = prism_dbg_get_ctrl();
    prism_write32(DBG_CTRL_REG, ctrl | PRISM_DBG_HALT_REQ);
    prism_write32(DBG_CTRL_REG, ctrl & ~PRISM_DBG_HALT_REQ);
}

bool prism_dbg_step(void)
{
    if (!prism_dbg_is_halted())
        return false;

    // Rising edge on the step bit executes one state transition.  halt_req
    // is held so the FSM stays halted afterwards even when the new state is
    // not a breakpoint.
    uint32_t ctrl = (prism_dbg_get_ctrl() & ~PRISM_DBG_STEP) | PRISM_DBG_HALT_REQ;
    prism_write32(DBG_CTRL_REG, ctrl);
    prism_write32(DBG_CTRL_REG, ctrl | PRISM_DBG_STEP);
    prism_write32(DBG_CTRL_REG, ctrl);
    return prism_dbg_wait_halt(100);
}

void prism_dbg_set_breakpoint_cond(int bp, uint8_t state, uint32_t cond)
{
    uint32_t ctrl = prism_dbg_get_ctrl();
    if (bp == 0) {
        ctrl &= ~PRISM_DBG_BP0_SI(PRISM_DBG_BP_SI_MASK);
        ctrl |= PRISM_DBG_BP0_EN | PRISM_DBG_BP0_SI(state);
#if PRISM_HAS_BP_COND
        ctrl &= ~PRISM_DBG_BP0_COND(3);
        ctrl |= PRISM_DBG_BP0_COND(cond);
#endif
    } else {
        ctrl &= ~PRISM_DBG_BP1_SI(PRISM_DBG_BP_SI_MASK);
        ctrl |= PRISM_DBG_BP1_EN | PRISM_DBG_BP1_SI(state);
#if PRISM_HAS_BP_COND
        ctrl &= ~PRISM_DBG_BP1_COND(3);
        ctrl |= PRISM_DBG_BP1_COND(cond);
#endif
    }
    (void)cond;
    prism_write32(DBG_CTRL_REG, ctrl);
}

void prism_dbg_set_breakpoint(int bp, uint8_t state)
{
    prism_dbg_set_breakpoint_cond(bp, state, 0);
}

void prism_dbg_clear_breakpoint(int bp)
{
    uint32_t ctrl = prism_dbg_get_ctrl();
    ctrl &= (bp == 0) ? ~PRISM_DBG_BP0_EN : ~PRISM_DBG_BP1_EN;
    prism_write32(DBG_CTRL_REG, ctrl);
}

void prism_dbg_set_state(uint8_t state)
{
    prism_write32(DBG_CTRL_REG,
                  prism_dbg_get_ctrl() | PRISM_DBG_NEW_SI |
                  PRISM_DBG_NEW_SI_VAL(state));
}

// ==========================================================================
// Live visibility
// ==========================================================================

uint32_t prism_get_inputs(void)
{
    return prism_read32(PRISM_REG_IN_DATA) & PRISM_IN_MASK;
}

uint32_t prism_get_outputs(void)
{
    return prism_read32(PRISM_REG_OUT_DATA) & PRISM_OUT_MASK;
}

uint32_t prism_get_decision_tree(void)
{
    return prism_read32(PRISM_REG_DECISION);
}

uint32_t prism_get_id(void)
{
    return prism_read32(PRISM_REG_ID);
}

uint8_t prism_get_out_pins(void)
{
    return (uint8_t)PRISM_CTRL_GET_OUT_PINS(prism_read32(PRISM_REG_CTRL));
}
