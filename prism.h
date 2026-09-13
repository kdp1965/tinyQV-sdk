#pragma once

// PRISM (Programmable Reconfigurable Indexed State Machine) peripheral driver
// for TinyQV.
//
//                        /\.
//                       /  \.
//                   ..-/----\-..
//               --''  /      \  ''--
//                    /________\.
//
// The PRISM executes a programmable Mealy FSM ("chroma") loaded at runtime
// into a State Information Table (SIT) of State Execution Words (STEWs).
// Chroma arrays are in load order as produced by the PRISM Yosys backend
// (highest state first, PRISM_STEW_WORDS words per state, MSW first).
//
// Several PRISM designs exist with different geometries and register maps.
// Select one with PRISM_CONFIG before including this header (or on the
// compiler command line, -DPRISM_CONFIG=PRISM_CONFIG_JANESTREET):
//
//   PRISM_CONFIG_SKY25A      ttsky25a-tinyQV: 8 states, 45-bit STEW shifted
//                            in through CFG_MSW/LSW, one FSM, 3-byte RX FIFO
//                            (default, the original API)
//   PRISM_CONFIG_JANESTREET  ihp-um-janestreet-prism: 32 states in two
//                            fracturable shards, 128-bit STEW loaded through
//                            the CFGMEM peripheral, per-shard datapath with
//                            16-byte FIFO and CRC, pin mux, two interrupts
//
// The function set below is common; functions that a configuration cannot
// implement degrade gracefully (documented per function).  Feature macros
// PRISM_HAS_* and PRISM_NUM_SHARDS from the selected prism_cfg_*.h tell an
// application what is there.  On multi-shard designs every per-shard
// function (counters, comm, host bits, FIFO, CRC, debugger, interrupt
// clear) acts on the shard selected by prism_set_shard(); the default is
// shard 0, which is the whole machine when not fractured.

#include <stdint.h>
#include <stdbool.h>
#include "gpio.h"

#define PRISM_CONFIG_SKY25A         1
#define PRISM_CONFIG_JANESTREET     2

#ifndef PRISM_CONFIG
#define PRISM_CONFIG                PRISM_CONFIG_SKY25A
#endif

#if PRISM_CONFIG == PRISM_CONFIG_SKY25A
#include "prism_cfg_sky25a.h"
#elif PRISM_CONFIG == PRISM_CONFIG_JANESTREET
#include "prism_cfg_janestreet.h"
#else
#error "Unknown PRISM_CONFIG"
#endif

#define PRISM_BASE_ADDRESS      ((uintptr_t)PERI_BASE_ADDRESS(PRISM_PERIPHERAL_NUM))

// Error bits returned by the config load/verify functions
#define PRISM_ERR_WORD(i)           (1 << (i))  // outgoing word i mismatched
#define PRISM_ERR_FINAL             (1 << 16)   // post-load readback mismatched
#define PRISM_ERR_CTRL              (1 << 17)   // CTRL readback mismatched

// ==========================================================================
// Low level register access
// ==========================================================================
static inline void prism_write32(uint32_t reg, uint32_t value)
{
    *(volatile uint32_t*)(PRISM_BASE_ADDRESS + reg) = value;
}

static inline uint32_t prism_read32(uint32_t reg)
{
    return *(volatile uint32_t*)(PRISM_BASE_ADDRESS + reg);
}

static inline void prism_write8(uint32_t reg, uint8_t value)
{
    *(volatile uint8_t*)(PRISM_BASE_ADDRESS + reg) = value;
}

static inline uint8_t prism_read8(uint32_t reg)
{
    return *(volatile uint8_t*)(PRISM_BASE_ADDRESS + reg);
}

// ==========================================================================
// Shards (PRISM_NUM_SHARDS > 1 only; no-ops / 0 otherwise)
// ==========================================================================
void prism_set_shard(int shard);            // select the shard the per-shard
                                            //   functions act on (0 or 1)
int prism_get_shard(void);
uint32_t prism_shard_reg(uint32_t offset);  // register address of the selected
                                            //   shard's window (PRISM_SH_*)

// Fracture the PRISM into two shards.  The masks say which outputs /
// conditional outputs each shard may drive (they reset to 0, so a fractured
// PRISM drives nothing until they are set; pass all ones to let each shard
// drive everything its own datapath needs).
void prism_set_fractured(bool fractured, uint32_t out_mask0, uint32_t cond_mask0,
                         uint32_t out_mask1, uint32_t cond_mask1);
bool prism_is_fractured(void);

// ==========================================================================
// Configuration (chroma) loading
// ==========================================================================

#if PRISM_HAS_CFG_SHIFT_LOADER
// Shift one STEW into config stage 0 (advances all stages).
void prism_cfg_write(uint32_t msw, uint32_t lsw);

// Read the STEW currently held in the last config stage (the next word to
// be pushed out by a prism_cfg_write).
void prism_cfg_read(uint32_t *msw, uint32_t *lsw);
#endif

// Load all PRISM_NUM_STATES words of chroma[] into the SIT.  On the shift
// loader, if expected is non-NULL the word shifted out is compared against
// expected[] before each write (validates the previously loaded chroma).
// On the CFGMEM loader every row is read back after the load instead and
// expected is ignored.  Returns 0 on success, else PRISM_ERR_WORD(i) /
// PRISM_ERR_FINAL.  The PRISM should be disabled while reloading.
int prism_load_config(const uint32_t *chroma, const uint32_t *expected);

// Verify the SIT against expected[] (load order).  Shift loader: by
// recirculation (the array is unchanged afterwards); CFGMEM loader: by
// random read.  Pass expected=NULL to just exercise the array.
int prism_verify_config(const uint32_t *expected);

// Full chroma load: disable the PRISM, load the SIT, program the chroma's
// control word (ctrl_reg as emitted by the Yosys PRISM backend; the CFG0 of
// the selected shard on multi-shard designs), verify, then enable.  On any
// error the PRISM is left disabled.  Returns 0 or PRISM_ERR_* mask.
int prism_load_chroma(const uint32_t *chroma, uint32_t ctrl_reg);

// As prism_load_chroma and also program the chroma's pinmux_reg (designs
// with PRISM_HAS_PINMUX; ignored otherwise).
int prism_load_chroma_ex(const uint32_t *chroma, uint32_t ctrl_reg, uint32_t pinmux_reg);

// As prism_load_chroma, additionally validating the previously loaded
// design (expected[], may be NULL) where the loader supports it.
int prism_load_chroma_verify(const uint32_t *chroma, const uint32_t *expected,
                             uint32_t ctrl_reg);

#if PRISM_NUM_SHARDS > 1
// Load two chromas into a fractured PRISM: the PRISM_BANK_STATES lowest
// states of chroma_a into shard 0 and of chroma_b into shard 1 (both
// compiled for the full geometry; only their low bank is used), program
// each shard's ctrl_reg / pinmux_reg, set the fracture masks to all ones,
// fracture and enable.  Returns 0 or PRISM_ERR_* mask.
int prism_load_shards(const uint32_t *chroma_a, uint32_t ctrl_a, uint32_t pinmux_a,
                      const uint32_t *chroma_b, uint32_t ctrl_b, uint32_t pinmux_b);
#endif

// Self test of the SIT storage: loads a pattern, loads a second pattern
// while validating the first, verifies the second.  Leaves the PRISM
// disabled.  Returns 0 on pass, else PRISM_ERR_* mask.
int prism_test_config(void);

// ==========================================================================
// Control word / enable / interrupt
// ==========================================================================
// On single-shard designs CTRL holds the chroma configuration bits and
// ENABLE.  On multi-shard designs the configuration bits are the selected
// shard's CFG0 and ENABLE is the common enable; prism_set_ctrl / get_ctrl
// present them as one word so chroma ctrl_reg values work unchanged.
void prism_set_ctrl(uint32_t ctrl);         // write CFG bits + ENABLE
uint32_t prism_get_ctrl(void);              // CFG bits | ENABLE | INTERRUPT
void prism_enable(void);                    // set ENABLE, keep config bits
void prism_disable(void);                   // clear ENABLE, keep config bits
                                            //   (resets FSM and counters)
bool prism_is_enabled(void);
bool prism_interrupt_pending(void);         // selected shard's interrupt
void prism_clear_interrupt(void);           // clear it (byte write)

#if PRISM_HAS_PINMUX
void prism_set_pinmux(uint32_t pinmux);     // selected shard's uo_out sources
uint32_t prism_get_pinmux(void);
#endif

// Route uo_out pins to the PRISM: bit n of pin_mask selects uo_out[n]
// (bit 0 is ignored - uo_out[0] is the TinyQV UART TX).
void prism_claim_pins(uint8_t pin_mask);

// Enable/disable the PRISM user interrupt of the selected shard in the
// RISC-V mie CSR.  Handle it by defining tqv_user_interrupt08(void) (shard 0)
// and, on two-shard designs, tqv_user_interrupt09(void) (shard 1).
void prism_enable_interrupt(void);
void prism_disable_interrupt(void);

// ==========================================================================
// PRISM peripheral values: counters, compare, shift register, comm
// register, host input bits (selected shard)
// ==========================================================================
void prism_set_count1_preload(uint32_t value);      // preload / wide SR load
uint32_t prism_get_count1_preload(void);
uint32_t prism_get_count1(void);                    // live count1 / wide SR
void prism_set_count1(uint32_t value);              // direct load (multi-shard designs)
uint8_t prism_get_count2(void);                     // live 8-bit counter
void prism_set_count2_compare(uint8_t value);
uint8_t prism_get_count2_compare(void);

// Wide shift register convenience wrappers: the value is staged in the
// preload register (the chroma transfers it with the count1 load output)
// and the shifted/received value is read from the live count1 register.
void prism_shift24_write(uint32_t value);
uint32_t prism_shift24_read(void);

void prism_comm_write(uint8_t value);               // 8-bit comm register
uint8_t prism_comm_read(void);

void prism_host_write(uint8_t bits);                // host_in[1:0]
uint8_t prism_host_read(void);

// Toggle host_in[0] and clear the interrupt in a single write.
void prism_host_toggle(void);

// ==========================================================================
// FIFO (selected shard).  sky25a: 3-byte RX FIFO in the 24-bit register;
// janestreet: 16 bytes, RX or TX per CFG0 FIFO_DIR_TX.
// ==========================================================================
uint32_t prism_fifo_status(void);                   // PRISM_FIFO_STAT_* fields
bool prism_fifo_empty(void);
bool prism_fifo_full(void);
uint8_t prism_fifo_level(void);                     // buffered bytes
uint8_t prism_fifo_pop(void);                       // unchecked pop (byte read)
int prism_fifo_read(void);                          // -1 if empty, else byte
bool prism_fifo_push(uint8_t value);                // TX mode: false if full /
                                                    //   no TX FIFO on this design
void prism_fifo_flush(void);                        // no-op without a flushable FIFO
#if PRISM_HAS_TX_FIFO
void prism_fifo_set_levels(uint8_t almost_empty, uint8_t almost_full);
#endif

// ==========================================================================
// CRC (PRISM_HAS_CRC designs, selected shard)
// ==========================================================================
#if PRISM_HAS_CRC
void prism_crc_set_poly(uint32_t poly);
void prism_crc_set_expected(uint32_t expected);
void prism_crc_preset(uint32_t value);              // write the CRC register
uint32_t prism_crc_get(void);
bool prism_crc_ok(void);                            // value == expected
#endif

// ==========================================================================
// Debugger (selected shard).  Only functional while the PRISM is enabled.
// Halting (by request, breakpoint or step) raises the shard's interrupt.
// ==========================================================================
uint32_t prism_dbg_status(void);                    // PRISM_DBG_STAT_* fields
uint32_t prism_dbg_get_ctrl(void);                  // persistent DBG_CTRL bits
bool prism_dbg_is_halted(void);
uint8_t prism_dbg_curr_state(void);
uint8_t prism_dbg_next_state(void);
bool prism_dbg_break_active(void);

bool prism_dbg_halt(void);                          // halt, wait until halted
bool prism_dbg_wait_halt(uint32_t timeout_us);
void prism_dbg_resume(void);                        // resume from halt/break
bool prism_dbg_step(void);                          // execute one transition
void prism_dbg_set_breakpoint(int bp, uint8_t state);   // bp = 0 or 1, on entry
// Conditional breakpoint (PRISM_HAS_BP_COND; cond = PRISM_BPC_*): halt inside
// the state when its decision tree matches, before the transition's outputs
// act.  Falls back to an entry breakpoint on designs without conditions.
void prism_dbg_set_breakpoint_cond(int bp, uint8_t state, uint32_t cond);
void prism_dbg_clear_breakpoint(int bp);
void prism_dbg_set_state(uint8_t state);            // force state (while halted)

// Live visibility into the FSM
uint32_t prism_get_inputs(void);                    // input vector
uint32_t prism_get_outputs(void);                   // output vector
uint32_t prism_get_decision_tree(void);             // LUT inputs + match
uint32_t prism_get_id(void);                        // architecture ID word
uint8_t prism_get_out_pins(void);                   // latched uo_out[7:1]
                                                    //   (0 where not readable)
