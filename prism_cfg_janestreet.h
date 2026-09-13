#pragma once

// PRISM configuration: ihp-um-janestreet-prism (PRISM_CONFIG_JANESTREET)
//
// The Jane Street / IHP CMOS5L 8x4 PRISM: 32 states in two 16-state shards
// (fracturable), dual compare ("if / else if / else"), 128-bit STEW held in
// CFGMEM latch macros loaded through the CFGMEM peripheral (peripheral 4),
// one datapath per shard (24/32-bit count1 / shifter, 8-bit count2 +
// compare, 8-bit comm, 16-byte FIFO, bit-serial CRC), per-shard pin mux,
// two interrupt lines (IRQ 8 / 9).  Register map: docs/prism_interface.md
// in the design repository.
//
// Included by prism.h; do not include directly.

#define PRISM_CFG_NAME              "janestreet"
#define PRISM_PERIPHERAL_NUM        8
#define CFGMEM_PERIPHERAL_NUM       4

// ---- geometry and features -----------------------------------------------
#define PRISM_NUM_STATES            32
#define PRISM_SI_BITS               5
#define PRISM_STEW_WORDS            4           // 128-bit STEW = 4 x 32
#define PRISM_CHROMA_WORDS          (PRISM_NUM_STATES * PRISM_STEW_WORDS)
#define PRISM_BANK_STATES           16          // rows per CFGMEM bank / states per shard
#define PRISM_NUM_SHARDS            2
#define PRISM_HAS_CFG_SHIFT_LOADER  0
#define PRISM_HAS_CFGMEM_LOADER     1
#define PRISM_HAS_PINMUX            1
#define PRISM_HAS_FIFO              1           // 16 bytes per shard, RX or TX
#define PRISM_HAS_TX_FIFO           1
#define PRISM_HAS_CRC               1
#define PRISM_HAS_BP_COND           1
#define PRISM_COUNT1_MASK           0xFFFFFFFFu
#define PRISM_IN_MASK               0xFFFFFFFFu
#define PRISM_OUT_MASK              0x1FFFFFu

// ---- CFGMEM loader peripheral (base PERI_BASE_ADDRESS(4)) -----------------
//   0x00 + i*4  write: shift a 32-bit word into "lo" macro i (i = 0..3).
//               read : "hi" macro i output word at the CTRL row address.
//   0x20 + i*4  write: shift the lo[i] output word into "hi" macro i (with
//               BYP_LO set the written data goes straight through lo).
//   0x1f        control BYTE: [3:0] row address, [4] address select (1 =
//               use [3:0], 0 = PRISM drives it), [5] loader busy (RO),
//               [6] bypass lo, [7] bypass hi.
//   Each macro is 16 rows: a shift write puts the new word in row 0 and
//   moves row r to r+1; the first word written ends in row 15.
#define CFGMEM_BASE_ADDRESS     ((uintptr_t)PERI_BASE_ADDRESS(CFGMEM_PERIPHERAL_NUM))
#define CFGMEM_REG_LO(i)        ((i) * 4)
#define CFGMEM_REG_HI(i)        (0x20 + (i) * 4)
#define CFGMEM_REG_CTRL         0x1f
#define CFGMEM_CTRL_ADDR(r)     ((r) & 0xfu)
#define CFGMEM_CTRL_ADDR_SEL    (1u << 4)
#define CFGMEM_CTRL_BUSY        (1u << 5)
#define CFGMEM_CTRL_BYP_LO      (1u << 6)
#define CFGMEM_CTRL_BYP_HI      (1u << 7)

// ---- common register block (byte offsets from PRISM_BASE_ADDRESS) ----------
#define PRISM_REG_CTRL          0x00    // [31] shard 0 IRQ (RO) [30] enable [29] shard 1 IRQ (RO)
#define PRISM_REG_INT_CLR       0x03    // W byte, bit 7 clears the shard 0 interrupt
#define PRISM_REG_INT_CLR1      0x07    // W byte, bit 7 clears the shard 1 interrupt
#define PRISM_REG_DBG_CTRL      0x04    // shard 0 debugger control
#define PRISM_REG_DBG_CTRL1     0x08    // shard 1 debugger control
#define PRISM_REG_DBG_STAT      0x0C    // shard 0 in [12:0], shard 1 in [25:13]
#define PRISM_REG_STEW0         0x10    // 4 words: STEW of shard 0's current state
#define PRISM_REG_ID            0x20
#define PRISM_REG_INT_STATUS    0x24    // [3:2] semaphore seen by shard 1 / 0, [1:0] IRQs
#define PRISM_REG_DEBUG_DOUT    0x30
#define PRISM_REG_DECISION      0x34
#define PRISM_REG_OUT_DATA      0x38    // [20:0] live output vector (OR of both shards)
#define PRISM_REG_IN_DATA       0x3C    // [31:0] shard 0 input vector
#define PRISM_REG_FRAC_CFG      0x40    // [0] fractured
#define PRISM_REG_OUT_MASK0     0x44    // fractured: outputs shard 0 may drive
#define PRISM_REG_COND_MASK0    0x48
#define PRISM_REG_OUT_MASK1     0x4C
#define PRISM_REG_COND_MASK1    0x50

// ---- per-shard window: shard s at 0x100 + 0x80 * s --------------------------
#define PRISM_SHARD_BASE(s)     (0x100u + (uint32_t)(s) * 0x80u)
#define PRISM_SH_CFG0           0x00    // chroma ctrl_reg (datapath / input configuration)
#define PRISM_SH_PINMUX         0x04    // chroma pinmux_reg (uo_out[7:1] sources)
#define PRISM_SH_PRELOAD        0x08    // 32-bit
#define PRISM_SH_COUNT1         0x0C    // read count, write = load
#define PRISM_SH_COUNTS         0x10    // {comm_count[31:29], shift_count[28:24], comm, compare, count2}
#define PRISM_SH_COUNT2         0x10    // byte
#define PRISM_SH_COMPARE        0x11    // byte
#define PRISM_SH_COMM           0x12    // byte
#define PRISM_SH_HOST           0x14    // host_in[1:0]
#define PRISM_SH_TOGGLE         0x15    // W byte: toggle host_in[0], clear the interrupt
#define PRISM_SH_FLAGS          0x18    // RO: see PRISM_FLAG_*
#define PRISM_SH_CFG1           0x1C    // [19:16] FIFO almost-empty level, [23:20] almost-full level
#define PRISM_SH_FIFO           0x20    // byte: write pushes (TX mode), read pops (RX mode)
#define PRISM_SH_FIFO_STATUS    0x24    // see PRISM_FIFO_STAT_*; any write flushes
#define PRISM_SH_CRC_POLY       0x28
#define PRISM_SH_CRC            0x2C    // read value; write = preset
#define PRISM_SH_CRC_EXPECTED   0x30

// The classic register names resolve to the selected shard's window
// (see prism_set_shard()); PRISM_REG_* below are shard 0 for convenience.
#define PRISM_REG_CFG0          (PRISM_SHARD_BASE(0) + PRISM_SH_CFG0)
#define PRISM_REG_PINMUX        (PRISM_SHARD_BASE(0) + PRISM_SH_PINMUX)
#define PRISM_REG_PRELOAD       (PRISM_SHARD_BASE(0) + PRISM_SH_PRELOAD)
#define PRISM_REG_COUNT1        (PRISM_SHARD_BASE(0) + PRISM_SH_COUNT1)
#define PRISM_REG_COUNTS        (PRISM_SHARD_BASE(0) + PRISM_SH_COUNTS)
#define PRISM_REG_HOST_IN       (PRISM_SHARD_BASE(0) + PRISM_SH_HOST)
#define PRISM_REG_HOST_TOGGLE   (PRISM_SHARD_BASE(0) + PRISM_SH_TOGGLE)

// FLAGS bits
#define PRISM_FLAG_COUNT1_TERM  (1u << 0)
#define PRISM_FLAG_COUNT1_WRAP  (1u << 1)
#define PRISM_FLAG_COUNT2_CMP   (1u << 2)
#define PRISM_FLAG_COUNT2_EQ_COMM (1u << 3)
#define PRISM_FLAG_SHIFT_TERM   (1u << 4)
#define PRISM_FLAG_SHIFT_DATA   (1u << 5)
#define PRISM_FLAG_LATCHED_IN(v) (((v) >> 6) & 3u)
#define PRISM_FLAG_FIFO_EMPTY   (1u << 8)
#define PRISM_FLAG_FIFO_FULL    (1u << 9)
#define PRISM_FLAG_CRC_OK       (1u << 10)

// ---- CTRL (common) --------------------------------------------------------
#define PRISM_CTRL_ENABLE           (1u << 30)
#define PRISM_CTRL_INTERRUPT        (1u << 31)  // shard 0 interrupt pending (RO)
#define PRISM_CTRL_INTERRUPT1       (1u << 29)  // shard 1 interrupt pending (RO)

// ---- CFG0 bits (the chroma ctrl_reg; prism_set_ctrl() writes them here) ----
#define PRISM_CTRL_SHIFT_IN_SEL(n)  ((uint32_t)((n) & 3u) << 0)  // shifter input pin ui_in[n]
#define PRISM_CTRL_CLR_NOT_LOAD     (1u << 6)   // OUT_COUNT1_CLEAR_LOAD clears instead of loading
#define PRISM_CTRL_LATCH_IN_OUT     (1u << 7)   // in[13:12] = latched outputs instead of latched inputs
#define PRISM_CTRL_SHIFT_EN         (1u << 8)
#define PRISM_CTRL_SHIFT_DIR        (1u << 9)   // 0 = MSB first, 1 = LSB first
#define PRISM_CTRL_SHIFT_WIDE       (1u << 10)  // count1 shifts instead of comm
#define PRISM_CTRL_SHIFT_24_EN      PRISM_CTRL_SHIFT_WIDE
#define PRISM_CTRL_COUNT32          (1u << 11)  // 32-bit count1 (else 24)
#define PRISM_CTRL_COUNT2_DEC       (1u << 12)
#define PRISM_CTRL_LATCH_EN         (1u << 13)  // OUT_LATCH captures the latched inputs
#define PRISM_CTRL_COUNT_UP         (1u << 14)
#define PRISM_CTRL_WRAP_PRELOAD     (1u << 15)  // count-up rolls over at preload
#define PRISM_CTRL_SHIFT_LOAD_ONE   (1u << 16)  // wide load sets the shift count to 1
#define PRISM_CTRL_COMM_LOAD_ONE    (1u << 17)  // comm load sets its count to 1
#define PRISM_CTRL_IN_SYNC_SEL(n)   ((uint32_t)((n) & 3u) << 18)  // 0 = 2 flops, 1 = 1 flop, 2 = raw
#define PRISM_CTRL_CRC_MODE(n)      ((uint32_t)((n) & 3u) << 20)  // 0 off, 1 CRC8, 2 CRC16, 3 CRC32
#define PRISM_CTRL_CRC_REFLECT      (1u << 22)
#define PRISM_CTRL_FIFO_DIR_TX      (1u << 23)  // 0 = RX (FSM pushes, host reads), 1 = TX
#define PRISM_CTRL_SEMA_SET_WINS    (1u << 24)
#define PRISM_CTRL_CRC_INIT_ONES    (1u << 25)
#define PRISM_CTRL_CRC_XOR_OUT      (1u << 26)
#define PRISM_CTRL_CRC_SRC_OUT      (1u << 27)  // CRC over the shifter output bit
#define PRISM_CTRL_CFG_MASK         0x0FFFFFFFu // CFG0 bits

// Not available on this design (uo_out readback lives in the GPIO block)
#define PRISM_CTRL_GET_OUT_PINS(v)  (0u)

// ---- pin mux: 3 bits per uo_out[k+1], k = 0..6 -----------------------------
#define PRISM_PINMUX_PIN_OUT(n)     ((uint32_t)(n) & 3u)     // this shard's pin_out[n]
#define PRISM_PINMUX_COND_OUT(n)    (4u + ((uint32_t)(n) & 1u))
#define PRISM_PINMUX_SHIFT_DATA     6u
#define PRISM_PINMUX_NONE           7u                        // shard does not drive the pin
#define PRISM_PINMUX(pin, sel)      ((uint32_t)((sel) & 7u) << (3 * ((pin) - 1)))
#define PRISM_PINMUX_ALL_NONE       0x1FFFFFu

// ---- DBG_CTRL bits (per shard) ---------------------------------------------
#define PRISM_DBG_HALT_REQ          (1u << 0)
#define PRISM_DBG_STEP              (1u << 1)
#define PRISM_DBG_BP0_EN            (1u << 2)
#define PRISM_DBG_BP1_EN            (1u << 3)
#define PRISM_DBG_BP0_SI(n)         ((uint32_t)((n) & 0x1Fu) << 4)
#define PRISM_DBG_BP1_SI(n)         ((uint32_t)((n) & 0x1Fu) << 9)
#define PRISM_DBG_BP_SI_MASK        0x1Fu
#define PRISM_DBG_BP0_COND(c)       ((uint32_t)((c) & 3u) << 14)   // PRISM_BPC_*
#define PRISM_DBG_BP1_COND(c)       ((uint32_t)((c) & 3u) << 16)
#define PRISM_DBG_NEW_SI            (1u << 18)
#define PRISM_DBG_NEW_SI_VAL(n)     ((uint32_t)((n) & 0x1Fu) << 19)
#define PRISM_DBG_CTRL_MASK         0x3FFFFu    // persistent bits [17:0]

// Breakpoint conditions.  ENTRY halts before the state's outputs act; the
// others halt inside the state in the cycle the decision tree matches, with
// that cycle's transition and outputs held off.
#define PRISM_BPC_ENTRY             0u
#define PRISM_BPC_IF                1u          // tree 0 matches
#define PRISM_BPC_ELSE_IF           2u          // tree 1 taken (matches, tree 0 does not)
#define PRISM_BPC_ANY               3u          // either taken: the state exits

// DBG_STAT fields, after prism_dbg_status() has selected the shard's 13 bits
#define PRISM_DBG_STAT_CURR_SI(v)   ((v) & 0x1Fu)
#define PRISM_DBG_STAT_NEXT_SI(v)   (((v) >> 5) & 0x1Fu)
#define PRISM_DBG_STAT_HALTED       (1u << 10)
#define PRISM_DBG_STAT_BREAK        (1u << 11)
#define PRISM_DBG_STAT_BREAK1       (1u << 12)
#define PRISM_DBG_STAT_SHARD_BITS   13

// ---- FIFO_STATUS fields ------------------------------------------------------
#define PRISM_FIFO_STAT_EMPTY       (1u << 0)
#define PRISM_FIFO_STAT_FULL        (1u << 1)
#define PRISM_FIFO_STAT_ALMOST_EMPTY (1u << 2)
#define PRISM_FIFO_STAT_ALMOST_FULL (1u << 3)
#define PRISM_FIFO_STAT_COUNT(v)    (((v) >> 8) & 0x1Fu)
#define PRISM_FIFO_DEPTH            16

// ==========================================================================
// PRISM FSM input vector (per shard, PRISM_REG_IN_DATA shows shard 0)
//   in[6:0]   ui_in[6:0] (2-flop / 1-flop / raw per IN_SYNC_SEL)
//   in[7]     shift_data (serial output of the active shifter)
//   in[9:8]   host_in[1:0]
//   in[10]    count1 terminal (== 0 down; == preload / max up)
//   in[11]    count2 >= compare
//   in[13:12] latched inputs (or latched outputs with LATCH_IN_OUT)
//   in[14]    shift count == 0 (all bits shifted)
//   in[15]    count2 == comm
//   in[20]    fifo_empty      in[21] fifo_full     in[22] crc_ok
//   in[23]    count1 wrapped  in[24] semaphore     in[25] other shard halted
//   in[26]    fifo almost full                     in[27] fifo almost empty
//
// PRISM FSM output vector (per shard)
//   out[3:0]  pin_out[3:0] (routed to uo_out[7:1] by PINMUX)
//   out[4]    OUT_LATCH            out[5]  OUT_FIFO_WR_RD (push RX / pop TX)
//   out[6]    OUT_COUNT1_INC_DEC   out[7]  OUT_COUNT1_CLEAR_LOAD
//   out[8]    OUT_SHIFT            out[9]  OUT_COUNT2_INC
//   out[10]   OUT_COUNT2_DEC       out[11] OUT_COUNT2_CLEAR
//   out[12]   OUT_CRC_CLEAR        out[13] OUT_CRC_UPDATE
//   out[14]   OUT_HOST_INTERRUPT   out[15] OUT_SEMA_CLEAR
//   out[16]   OUT_COMM_LOAD        out[17] OUT_LOAD_CRC
//   out[19]   OUT_SEMA_SET
// ==========================================================================
