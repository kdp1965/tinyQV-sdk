#pragma once

// PRISM configuration: ttsky25a-tinyQV (PRISM_CONFIG_SKY25A)
//
// The original 2-tile PRISM: 8 states, 45-bit STEW held in a latch based
// State Information Table that is loaded as a shift register through
// CFG_MSW / CFG_LSW.  One FSM, one set of peripheral registers, a 24-bit
// counter / shifter that doubles as a 3-byte RX FIFO, no CRC.
//
// Included by prism.h; do not include directly.

#define PRISM_CFG_NAME              "sky25a"
#define PRISM_PERIPHERAL_NUM        8

// ---- geometry and features -----------------------------------------------
#define PRISM_NUM_STATES            8
#define PRISM_SI_BITS               3
#define PRISM_STEW_WORDS            2           // MSW + LSW per state
#define PRISM_CHROMA_WORDS          (PRISM_NUM_STATES * PRISM_STEW_WORDS)
#define PRISM_STEW_MSW_MASK         0xFFFu      // STEW bits [43:32]
#define PRISM_NUM_SHARDS            1
#define PRISM_HAS_CFG_SHIFT_LOADER  1           // STEWs shift through CFG_MSW/LSW
#define PRISM_HAS_CFGMEM_LOADER     0
#define PRISM_HAS_PINMUX            0
#define PRISM_HAS_FIFO              1           // 3-byte RX FIFO (count1 in FIFO_24 mode)
#define PRISM_HAS_TX_FIFO           0
#define PRISM_HAS_CRC               0
#define PRISM_HAS_BP_COND           0
#define PRISM_COUNT1_MASK           0xFFFFFFu
#define PRISM_IN_MASK               0xFFFFu
#define PRISM_OUT_MASK              0x7FFu

// ---- register map (byte offsets from PRISM_BASE_ADDRESS) -----------------
#define PRISM_REG_CTRL          0x00    // R/W  Control register (latch based)
#define PRISM_REG_INT_CLR       0x03    // W    Byte write 0x80 clears interrupt
#define PRISM_REG_DBG_CTRL      0x04    // R/W  Debugger control (latch based)
#define PRISM_REG_DBG_STAT      0x0C    // R    Debugger / FSM status
#define PRISM_REG_CFG_LSW       0x10    // W    STEW[31:0], write starts shift
                                        // R    Config stage 7 STEW[31:0]
#define PRISM_REG_CFG_MSW       0x14    // W    STEW[43:32], write before LSW
                                        // R    Config stage 7 STEW[43:32]
#define PRISM_REG_COMM_DATA     0x18    // R/W  8-bit comm register (byte access)
#define PRISM_REG_FIFO_DATA     0x19    // R    Byte read pops 3-byte RX FIFO
#define PRISM_REG_FIFO_STAT     0x1A    // R    FIFO status (byte access)
#define PRISM_REG_HOST_IN       0x1B    // R/W  host_in[1:0] (byte access)
#define PRISM_REG_PRELOAD       0x20    // R/W  24-bit count1 preload / SR load
#define PRISM_REG_HOST_TOGGLE   0x21    // W    Byte write toggles host_in[0],
                                        //      clears interrupt AND writes
                                        //      count2_compare with the byte
#define PRISM_REG_COUNT_VAL     0x24    // R    [23:0]=count1, [31:24]=count2
#define PRISM_REG_COMPARE       0x28    // R/W  8-bit count2 compare (byte access)
#define PRISM_REG_ID            0x30    // R    PRISM architecture ID word
#define PRISM_REG_DECISION      0x34    // R    Decision tree (LUT ins + match)
#define PRISM_REG_OUT_DATA      0x38    // R    [10:0] live FSM output vector
#define PRISM_REG_IN_DATA       0x3C    // R    [15:0] live FSM input vector

// ---- CTRL register bits (chroma ctrl_reg lives here with ENABLE) ---------
#define PRISM_CTRL_SHIFT_IN_SEL(n)  ((uint32_t)((n) & 3u) << 0)  // shift in from ui_in[n]
#define PRISM_CTRL_SHIFT_OUT_SEL(n) ((uint32_t)((n) & 3u) << 2)  // 0=off, 1..3 => uo_out[4+n]
#define PRISM_CTRL_COND_OUT_SEL(n)  ((uint32_t)((n) & 3u) << 4)  // 0=off, 1..3 => uo_out[1+n]
#define PRISM_CTRL_LOAD4            (1u << 6)   // out[4] loads comm from preload byte
#define PRISM_CTRL_LATCH_IN_OUT     (1u << 7)   // in[13:12] = registered out[6]/out[1]
#define PRISM_CTRL_SHIFT_EN         (1u << 8)   // enable shift operations
#define PRISM_CTRL_SHIFT_DIR        (1u << 9)   // 0=left (MSB first), 1=right
#define PRISM_CTRL_SHIFT_24_EN      (1u << 10)  // shift 24-bit count1, else 8-bit comm
#define PRISM_CTRL_FIFO_24          (1u << 11)  // count1 acts as 3-byte RX FIFO
#define PRISM_CTRL_COUNT2_DEC       (1u << 12)  // enable count2 decrement via out[5]
#define PRISM_CTRL_LATCH_EN         (1u << 13)  // 'latch3': out[2] latches inputs;
                                                //   with FIFO_24, uo_out[1]=fifo_full
#define PRISM_CTRL_ENABLE           (1u << 30)  // PRISM execution enable
#define PRISM_CTRL_INTERRUPT        (1u << 31)  // (read only) interrupt pending
#define PRISM_CTRL_CFG_MASK         0x3FFFu     // all configuration bits

// CTRL read-only status fields
#define PRISM_CTRL_GET_OUT_PINS(v)  (((v) >> 16) & 0x7Fu)   // latched uo_out[7:1]
#define PRISM_CTRL_UI_IN7           (1u << 23)              // direct read of ui_in[7]

// ---- DBG_CTRL register bits -----------------------------------------------
// Bits [9:0] are persistent (latch based); NEW_SI/NEW_SI_VAL act only during
// the write itself.  The debugger only operates while the PRISM is enabled.
// Any halt raises the user interrupt.
#define PRISM_DBG_HALT_REQ          (1u << 0)   // set to halt; 1->0 edge resumes
#define PRISM_DBG_STEP              (1u << 1)   // 0->1 edge single steps (halted)
#define PRISM_DBG_BP0_EN            (1u << 2)
#define PRISM_DBG_BP1_EN            (1u << 3)
#define PRISM_DBG_BP0_SI(n)         ((uint32_t)((n) & 0x7u) << 4)
#define PRISM_DBG_BP1_SI(n)         ((uint32_t)((n) & 0x7u) << 7)
#define PRISM_DBG_BP_SI_MASK        0x7u
#define PRISM_DBG_NEW_SI            (1u << 10)  // load NEW_SI_VAL as state index
#define PRISM_DBG_NEW_SI_VAL(n)     ((uint32_t)((n) & 0x7u) << 11)
#define PRISM_DBG_CTRL_MASK         0x3FFu      // persistent latch bits [9:0]

// DBG_STAT register fields
#define PRISM_DBG_STAT_CURR_SI(v)   ((v) & 0x7u)
#define PRISM_DBG_STAT_NEXT_SI(v)   (((v) >> 3) & 0x7u)
#define PRISM_DBG_STAT_HALTED       (1u << 6)
#define PRISM_DBG_STAT_BREAK        (1u << 7)   // halted due to breakpoint 0
#define PRISM_DBG_STAT_BREAK1       (1u << 8)   // halted due to breakpoint 1

// ---- FIFO_STAT register fields (byte read) --------------------------------
#define PRISM_FIFO_STAT_EMPTY       (1u << 0)
#define PRISM_FIFO_STAT_FULL        (1u << 1)
#define PRISM_FIFO_STAT_WR_PTR(v)   (((v) >> 2) & 3u)
#define PRISM_FIFO_STAT_RD_PTR(v)   (((v) >> 4) & 3u)
#define PRISM_FIFO_STAT_COUNT(v)    (((v) >> 6) & 3u)

// ==========================================================================
// PRISM FSM input vector (PRISM_REG_IN_DATA / chroma input mux selects)
//   in[6:0]   ui_in[6:0]
//   in[7]     shift_data (serial output of the active shifter)
//   in[9:8]   host_in[1:0]
//   in[10]    count1 == 0
//   in[11]    count2 >= count2_compare
//   in[13:12] latched inputs (or registered out[6]/out[1] with LATCH_IN_OUT)
//   in[14]    shift bit counter == 0 (all bits shifted)
//   in[15]    count2 == comm_data
//
// PRISM FSM output vector (PRISM_REG_OUT_DATA)
//   out[6:0]  drive uo_out[7:1] (subject to shift/cond output muxes)
//   out[2]    also latches inputs when CTRL LATCH_EN set
//   out[4]    also comm load-from-preload when CTRL LOAD4 set
//   out[5]    also count2 decrement when CTRL COUNT2_DEC set
//   out[6]    also shift enable strobe when CTRL SHIFT_EN set
//   out[7]    count1 decrement
//   out[8]    count1 load from preload (FIFO push in FIFO_24 mode)
//   out[9]    count2 increment
//   out[10]   count2 clear (out[9]+out[10] together raise the interrupt)
// ==========================================================================
