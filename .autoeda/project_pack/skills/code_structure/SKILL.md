# ABC STMAP Code Structure Skill

Use this skill when orienting implementation work for `stmapN` variants in this
ABC repository.

## Command Layer

- `src/base/abci/abc.c` owns most command declarations, registration, and usage
  text. Add a static declaration near the existing mapper commands, register
  each `stmapN` in the `SC mapping` command section near `map`, and implement
  usage text for `stmapN -h`.
- Classic `map` is implemented as `Abc_CommandMap` near the large standard-cell
  mapping block. Use it as the CLI behavior reference, not as a reason to
  silently change `map`.
- Prefer versioned command/glue files under `src/base/abci/` when a variant is
  more than a tiny command wrapper. Suggested names are `abcStmap_N.c` and
  `abcStmap_N.h`.
- Wire new command/glue files into `src/base/abci/module.make`.

## Mapping Interface

- `src/base/abci/abcMap.c` is the main bridge from ABC networks into the classic
  mapper. It derives a mapper genlib from the loaded SCL/Liberty library through
  `Abc_SclDeriveGenlib`, creates the supergate library, builds `Map_Man_t`,
  transfers CI arrivals and CO required times, calls `Map_Mapping`, and
  reconstructs the mapped ABC network.
- `Abc_NtkToMap` converts a strashed ABC network into mapper nodes and preserves
  choice links. `Abc_NtkFromMap` and the `Abc_NodeFromMap*` helpers reconstruct
  selected supergates and phases into a mapped network that later `topo`,
  `buffer`, `upsize`, `dnsize`, and `stime` consume.
- SCL constraint bridges include `Abc_NtkMapCopyCiArrivalCon` and
  `Abc_NtkMapCopyCoRequiredCon`; use them when a hypothesis involves explicit
  SCL timing constraints.

## Classic Mapper Internals

- `src/map/mapper/mapperCore.c` owns the main `Map_Mapping` schedule: cuts,
  truth tables, delay-oriented matches, area-flow recovery, exact-area recovery,
  phase-aware recovery, and switching recovery.
- `src/map/mapper/mapperTime.c` owns mapper arrival/required-time propagation.
- `src/map/mapper/mapperMatch.c` owns match selection and is a high-leverage
  place for altered cut/supergate scoring.
- `src/map/mapper/mapperCut.c` and `mapperCutUtils.c` own cut enumeration and
  cut manipulation.
- `src/map/mapper/mapperRefs.c` owns reference counts and area accounting.
- `src/map/mapper/mapperFanout.c` and `Map_ManCreateNodeDelays` are relevant to
  fanout/load proxy experiments.
- Wire versioned mapper helper files into `src/map/mapper/module.make`.

## SCL Timing, Buffering, And Sizing

- `src/map/scl/scl.c` registers `read_lib`, `topo`, `buffer`, `upsize`,
  `dnsize`, and `stime`.
- `src/map/scl/sclSize.c` implements the timing manager used by `stime` and
  prints the final line containing `WireLoad`, `Area`, and `Delay`.
- `src/map/scl/sclTime.h`, `sclSize.h`, and `sclLib.h` expose standard-cell
  timing, slew, capacitance, area, and Liberty-library structures.
- Wire SCL helper files into `src/map/scl/module.make`.

## Required Assets

- Build entry point: `Makefile`.
- Batch binary after build: `./abc`.
- Liberty library: `7nm_lvt_ff.lib`.
- Required benchmarks: `benchmarks/i10.aig`, `benchmarks/ode.abc.blif`,
  `benchmarks/or1200.abc.blif`, and `benchmarks/syn2.abc.blif`.
