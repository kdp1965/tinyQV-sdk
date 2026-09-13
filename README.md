# TinyQV SDK: Minimal area RISC-V core and accessories

C SDK for the [tinyQV](https://github.com/MichaelBell/tinyQV/) quad serial RISC-V SoC.

## Compiling a project

Get the [customised Risc-V GNU toolchain](https://github.com/MichaelBell/riscv-gnu-toolchain) for TinyQV.

Either download the release from that repo, or clone it and make with:

    ./configure --prefix=/opt/tinyQV --with-arch=rv32ec_zcb_zicond_zilsd --with-abi=ilp32e
    make

Note that this takes a while, especially first time as it downloads a bunch of submodules as part of the build process.

If the toolchain is not installed in /opt/tinyQV, then set the `RISCV_TOOLCHAIN` environment variable appropriately.

Use the `example-project` as a template for your project if building for full TinyQV, or `example-sim-project` if building for simulation (with limited RAM and flash).

## Choosing the correct version

The main branch is suitable for the ttsky25a, ttihp25b and ttgf0p2 versions of TinyQV.  Use the tt06 branch for tt06.

For TinyQV on Wafer Space run 1, use ws01 branch.

## PRISM driver configurations

`prism.h` / `prism.c` drive the PRISM peripheral of more than one TinyQV
design.  The register map is selected with `PRISM_CONFIG`:

| `PRISM_CONFIG`            | design                      | archive        |
|---------------------------|-----------------------------|----------------|
| `PRISM_CONFIG_SKY25A`     | ttsky25a-tinyQV (default)   | `tinyQV.a`, `tinyQV-sim.a` |
| `PRISM_CONFIG_JANESTREET` | ihp-um-janestreet-prism     | `tinyQV-js.a` (or link `prism_js.o` ahead of `tinyQV-sim.a` for the simulator build) |

Build the application with the same define, e.g.
`-DPRISM_CONFIG=PRISM_CONFIG_JANESTREET`, so the header and the library
agree.  The API is common; `PRISM_HAS_*` and `PRISM_NUM_SHARDS` from the
selected `prism_cfg_*.h` say which features exist.  On the two-shard
design the per-shard functions act on the shard chosen with
`prism_set_shard()`, `prism_load_chroma_ex()` also programs the pin mux,
`prism_load_shards()` loads two chromas into a fractured PRISM, and the
FIFO, CRC and conditional-breakpoint calls become available.
