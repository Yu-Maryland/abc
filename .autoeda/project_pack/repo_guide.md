# Repo Guide

## Build

Use the repository Makefile from the repo root:

```sh
make ABC_USE_NO_READLINE=1
```

The expected binary is `./abc`. The no-readline build avoids an optional
interactive dependency and is sufficient for `./abc -c` batch flows.
If `./abc` is absent in a fresh checkout, build it before running help, smoke,
baseline, candidate, or CEC commands.

## Required Inputs

- Liberty/SCL library: `7nm_lvt_ff.lib`
- Required benchmarks: `benchmarks/i10.aig`, `benchmarks/ode.abc.blif`,
  `benchmarks/or1200.abc.blif`, `benchmarks/syn2.abc.blif`
- `i10.aig` also exists at repo root; treat it as duplicate supplemental input,
  not a replacement for `benchmarks/i10.aig`.

Use ABC's SCL Liberty loader for the library:

```sh
read_lib 7nm_lvt_ff.lib
```

Do not load the Liberty file with the generic `read` command.

## Evaluation Flow

Baseline:

```sh
./abc -c "read_lib 7nm_lvt_ff.lib; read <design>; resyn; resyn2; dch -v; map; topo; buffer; upsize -v; dnsize -v; stime"
```

Candidate:

```sh
./abc -c "read_lib 7nm_lvt_ff.lib; read <design>; resyn; resyn2; dch -v; stmapN; topo; buffer; upsize -v; dnsize -v; stime"
```

Parse the final `stime` line that begins with `WireLoad = ...`; extract
`Delay = ... ps` as primary and `Area = ...` as secondary.

## Source Areas

- `src/base/abci/abc.c`: command declarations and registration. Classic `map`
  is registered in the `SC mapping` section and implemented near
  `Abc_CommandMap`.
- `src/base/abci/abcMap.c`: ABC-to-classic-mapper bridge, SCL-to-genlib
  derivation, mapper manager setup, `Map_Mapping`, and mapped-network
  reconstruction.
- `src/base/abci/module.make`: add versioned command/glue files here.
- `src/map/mapper/`: classic mapper internals. `mapperCore.c` owns the main
  schedule, while `mapperTime.c`, `mapperMatch.c`, `mapperCut*.c`,
  `mapperRefs.c`, and `mapperFanout.c` own timing, match choice, cut handling,
  area/reference accounting, and fanout/load proxies.
- `src/map/mapper/module.make`: add mapper helper files here.
- `src/map/scl/`: standard-cell Liberty timing, buffering, and sizing. `scl.c`
  registers `read_lib`, `topo`, `buffer`, `upsize`, `dnsize`, and `stime`;
  `sclSize.c`, `sclTime.h`, and `sclLib.h` are the timing/area surfaces.
- `src/map/scl/module.make`: add SCL helper files here.

## Correctness Notes

For CEC, write temporary original and post-flow designs under
`.autoeda/runtime/results/<version>/` and compare original versus candidate
after strashing where needed. Keep benchmark output, command logs, parsed
metrics, and CEC logs with the version results.
The original temporary should be captured from `read_lib <library>; read
<design>; strash`; the candidate temporary should be captured after the full
candidate downstream flow followed by `strash`.
