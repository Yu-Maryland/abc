# ABC STMAP Implementation Skill

Use this skill when implementing or validating a numbered `stmapN` candidate.

## Version Discipline

- Use the next unskipped command name: `stmap0`, `stmap1`, `stmap2`, and so on.
- Preserve previous `stmapN` commands and helper files. A new version may share
  infrastructure only when older versions remain buildable and comparable.
- Record the version identity in command name, log paths, artifacts, and commit
  message.
- Every new command must support `-h` usage text.

## Preferred Change Pattern

- Add command/glue code in a versioned file such as
  `src/base/abci/abcStmap_N.c`; wire it into `src/base/abci/module.make`.
- For deeper mapper work, add versioned helpers such as
  `src/map/mapper/mapperStmap_N.c` and `.h`; wire them into
  `src/map/mapper/module.make`.
- For SCL timing helpers, add `src/map/scl/sclStmap_N.c` and `.h`; wire them
  into `src/map/scl/module.make`.
- Shared edits to `abcMap.c`, `mapperCore.c`, `mapperTime.c`,
  `mapperMatch.c`, `mapperCut*.c`, `mapperRefs.c`, `mapperFanout.c`,
  `scl.c`, `sclSize.c`, `sclTime.h`, or `sclLib.h` are allowed only when the
  hypothesis requires them. Gate shared behavior through the active `stmapN`
  path, a versioned helper call, or an explicit option.
- If a version is only a parameter probe around `Abc_NtkMap`, label it as a
  diagnostic probe and state which mapper decision it is intended to expose.
  Do not let the campaign become a sequence of wrapper-only variants.

## Build And Smoke

- Build from the repo root with:

```sh
make ABC_USE_NO_READLINE=1
```

- Confirm the binary exists at `./abc`.
- Confirm command help:

```sh
./abc -c "stmapN -h"
```

- Smoke with `benchmarks/i10.aig`:

```sh
./abc -c "read_lib 7nm_lvt_ff.lib; read benchmarks/i10.aig; resyn; resyn2; dch -v; stmapN; topo; buffer; upsize -v; dnsize -v; stime"
```

## Benchmark Evaluation

- Run the same downstream flow for baseline `map` and candidate `stmapN` on all
  required benchmarks.
- Use `read_lib 7nm_lvt_ff.lib` before reading each design.
- Allow up to 10 minutes per design. Run the four designs in parallel when local
  resources allow.
- Parse the final `stime` line beginning with `WireLoad = ...`; extract
  `Delay = ... ps` as primary and `Area = ...` as secondary. Ignore earlier
  delay/area text from buffering or sizing commands unless explicitly logged as
  diagnostic data.

## Correctness

- For each applicable benchmark, compare original behavior with the post-flow
  candidate using ABC `cec`. Store temporaries under
  `.autoeda/runtime/results/stmapN/`.
- Capture the original temporary from `read_lib 7nm_lvt_ff.lib; read <design>;
  strash`; capture the candidate temporary after the full candidate flow plus
  `strash`; then run `./abc -c "cec <original_temp> <candidate_temp>"`.
- If direct CEC on mapped BLIF is unreliable, strash both original and candidate
  before writing temporary AIG/BLIF artifacts, then run `cec` on those files.
- Do not count versions with crashes, non-equivalence, timeouts, missing metrics,
  or incomplete benchmark coverage.

## Logging And Review

- Append the completed iteration to `.autoeda/runtime/versions.md` with
  hypothesis, motivation, files changed, command name, algorithm summary,
  validation, benchmark results, correctness results, review findings, commit,
  and next-step recommendation.
- Write raw logs and parsed metric tables under
  `.autoeda/runtime/results/stmapN/`.
- Run the configured review tool for substantial planning or implementation.
  Address accepted findings, revalidate affected behavior, and record accepted,
  rejected, and open findings.
  Mechanical non-semantic changes may skip a second review only when the log says
  why.
- Git push is disabled by default in `git.yaml`; create local commits only when
  the iteration passes and do not push unless the user explicitly changes the
  project-pack git policy.
