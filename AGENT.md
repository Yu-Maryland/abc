# AGENT

This repository is used for an AutoEDA autoresearch campaign on Berkeley ABC.

## Project Priority

The primary project is autonomous evolution of an advanced technology mapper that is aware of downstream:

- buffering
- gate sizing
- `stime` timing evaluation

The goal is not limited to small edits to the existing `map` command or one-off tuning of command-line mapper parameters. Acceptable directions include:

- downstream-aware mapper heuristics
- new versioned mapper families
- fused mapping plus buffering or sizing ideas
- feedback-driven remapping using downstream timing results
- SCL/`stime`-aware mapping ideas that use real Liberty timing, load, slew, and pin timing more directly than the classic mapper abstraction
- changes inside the mapper algorithm itself, including matching, recovery, cut selection, timing/area costing, fanout/load modeling, phase handling, and mapped-network reconstruction
- controlled parameter and mapper-variant exploration when it is used to guide or validate deeper algorithmic changes

## Research Goal

Create and evaluate numbered mapper variants that improve final post-flow timing and area compared with the classic ABC mapping flow. Final quality should be judged after downstream optimization and timing evaluation, not only immediately after mapping.

The campaign should prioritize deeper mapper exploration beyond wrappers that simply call `Abc_NtkMap` with different `Slew`, `Gain`, `AreaMulti`, `DelayMulti`, or related parameters. Parameter sweeps remain useful, but they should be treated as probes and baselines that reveal promising algorithmic directions. Successful later versions should increasingly investigate the mapper implementation itself and explain which internal mapping decision changed.

An especially important research direction is closing the gap between `map`'s simplified mapper delay model and the richer timing seen by `stime`. In this ABC flow, `read_lib` loads SCL/Liberty timing, but a plain `map`-level timing view can still behave like a simplified or unit-delay abstraction unless the mapped network is evaluated with downstream `topo`, `buffer`, `upsize`, `dnsize`, and `stime`. Explore ways for `stmapN` to exploit SCL timing information earlier in mapping, while still validating final QoR only with the downstream `stime` flow.

## Allowed Implementation Scope

- Default family name: `stmap`.
- Default command naming: `stmap0`, `stmap1`, `stmap2`, and so on.
- Version the experiment identity through the command name, logs, evaluation artifacts, and commit history. Do not interpret versioning as a restriction to one wrapper file.
- Prefer additive, versioned files for command glue and isolated helper experiments when that keeps comparisons clear.
- Suggested versioned helper names include `abcStmap_N.c` for command/glue in `src/base/abci/`, `mapperStmap_N.c` / `mapperStmap_N.h` for mapper helper experiments, and `sclStmap_N.c` / `sclStmap_N.h` for SCL timing helper experiments.
- Direct edits to shared mapper/SCL implementation files are allowed when the hypothesis requires them, including `abcMap.c`, `mapperCore.c`, `mapperTime.c`, `mapperMatch.c`, `mapperCut*.c`, `mapperRefs.c`, `mapperFanout.c`, `scl.c`, `sclSize.c`, `sclTime.h`, and `sclLib.h`.
- When editing shared files, keep changes targeted, log why a shared edit was needed, and preserve existing `map` behavior unless the version explicitly documents an intentional infrastructure change.
- When possible, gate shared-file behavior through the active `stmapN` path, a versioned helper call, or an explicit option so older experimental commands remain buildable and comparable.
- Keep older experimental versions buildable and comparable.
- Wire every new implementation file into the owning build/module file.
- Expose new experimental behaviors as new commands rather than silently changing unrelated stable commands.
- Every new command must support `-h` usage text.

## Repository Orientation

Important directories and files:

- `src/base/abci/`: ABC command registration and command wrappers
- `src/base/abci/abcMap.c`: ABC-to-mapper interface. This file strongly impacts `map` because it derives or selects the mapping library, applies area/delay multipliers, builds the supergate library, creates the mapper manager, transfers timing constraints, calls `Map_Mapping`, and reconstructs the mapped network.
- `src/map/mapper/`: classic mapper internals
- `src/map/mapper/mapperCore.c`: core mapping loop, including cut computation, truth-table derivation, delay matching, area-flow recovery, exact-area recovery, phase-aware recovery, and switching recovery.
- `src/map/mapper/mapperTime.c`, `mapperMatch.c`, `mapperCut*.c`, `mapperRefs.c`, `mapperFanout.c`: timing, match selection, cut management, reference/area accounting, and fanout/load-related logic used by the core mapper.
- `src/map/scl/`: standard-cell mapping, buffering, sizing, and timing-related code
- `src/map/scl/scl.c`: SCL commands including `stime`
- `src/map/scl/sclSize.c`, `sclTime.h`, `sclLib.h`: SCL timing engine, arrival/departure/load/slew computation, and Liberty table lookup used by `stime`, buffering, and sizing.
- `lib/`: bundled libraries and support assets
- `test/`: example scripts and tests
- `benchmarks/`: available benchmark design in this harness repo
- `Makefile`: primary local build entry point

Important command areas:

- classic baseline mapper: `map`
- mapped-network ordering: `topo`
- buffering: `buffer`
- sizing: `upsize`, `dnsize`
- timing/area evaluation: `stime`
- equivalence checking: `cec`

## Mapper And SCL Timing Exploration Guidance

Use `src/base/abci/abcMap.c` and the `src/map/mapper/` package as first-class research surfaces, not just as black-box code behind `map`.

Important `abcMap.c` behavior:

- `Abc_NtkMap` is the top-level `map` interface. It reads the current genlib, checks whether an SCL library with delay information is loaded, calls `Abc_SclDeriveGenlib(...)` when possible, transfers delays/profile data, applies `AreaMulti` and `DelayMulti`, derives the supergate library, optionally computes switching activity, calls `Abc_NtkToMap`, configures the mapper manager, calls `Map_Mapping`, and reconstructs the mapped network with `Abc_NtkFromMap`.
- `Abc_NtkToMap` converts an ABC strashed network into a `Map_Man_t`, creates mapper nodes, transfers PI arrival and PO required-time constraints, preserves choice-node links, and sets mapper outputs.
- `Abc_NtkFromMap`, `Abc_NodeFromMapSuper_rec`, `Abc_NodeFromMapPhase_rec`, and `Abc_NodeFromMap_rec` reconstruct the selected supergates/phases into a mapped ABC network. These choices can affect what downstream `topo`, `buffer`, `upsize`, `dnsize`, and `stime` see.
- `Abc_NtkMapCopyCiArrival`, `Abc_NtkMapCopyCoRequired`, `Abc_NtkMapCopyCiArrivalCon`, and `Abc_NtkMapCopyCoRequiredCon` bridge arrival/required constraints into the mapper. The `Scl_Con*` path is relevant when explicit SCL constraints are active.
- `Abc_NtkFromMapSuperChoice`, `Abc_NodeSuperChoice`, `Abc_NodeFromMapCutPhase`, and `Abc_NodeFromMapSuperChoice_rec` are relevant if a version explores choices/superchoices.
- The mini-mapping helpers and timing setters are secondary research surfaces for serialization/debugging and controlled timing-constraint experiments.

Important core mapper behavior:

- `Map_Mapping` in `mapperCore.c` is the central schedule. It computes choice levels, cuts, truth tables, delay-oriented matches, references, base area, area-flow recovery, exact-area recovery, phase-aware recovery, and optional switching recovery.
- Mapping mode `0` is delay matching; mode `1` is area-flow recovery; mode `2` is exact-area recovery; mode `3` is phase-aware exact-area recovery; mode `4` is switching recovery.
- `mapperTime.c` controls mapper arrival and required-time propagation; `mapperMatch.c` chooses matches; `mapperCut*.c` controls cut enumeration and cut data; `mapperRefs.c` controls reference and area accounting; `mapperFanout.c` and `Map_ManCreateNodeDelays` are relevant for fanout/load proxy experiments.

The key timing limitation to investigate:

- `read_lib` plus `map` uses `Abc_SclDeriveGenlib` to collapse SCL/Liberty timing into the mapper's genlib/supergate abstraction.
- `stime` later uses the SCL timing engine with cell pin timing tables, load, slew, wire-load behavior, and mapped-network context.
- Therefore a candidate can look reasonable to the mapper's simplified delay model but still underperform in final `stime`, or the opposite.
- Versions should explore whether selected SCL timing signals can be brought into mapping decisions earlier without breaking mapper invariants.

Promising SCL/`stime`-aware exploration directions:

- Derive better mapper delay estimates from SCL timing by varying or replacing how `Abc_SclDeriveGenlib` is called, including slew/gain/load assumptions, minimum gate filters, or profile transfer behavior.
- Add a versioned mapping path that uses `stime` or `Abc_SclTimePerform`-style timing after an initial mapping to identify critical nodes, high-load fanouts, or poor slew points, then remap or bias a second mapping pass.
- Feed SCL-derived criticality back into mapper required times or node delay penalties before `Map_MappingMatches`.
- Adjust cut/match scoring to penalize choices that create bad downstream load/slew/fanout behavior even when mapper arrival looks good.
- Explore reconstruction choices in `Abc_NtkFromMap` where locally equivalent supergate/phase choices may lead to different downstream `stime`.
- Add instrumentation to correlate mapper arrival, area flow, selected supergates, fanout/load proxies, and final `stime` critical-path data.
- Try small two-pass flows inside a versioned `stmapN`: first map, evaluate or approximate SCL timing, then remap with local penalties or required-time changes. Keep the flow explicit, logged, and equivalence checked.

Parameter and mapper-variant exploration remains allowed:

- Sweeps of `Slew`, `Gain`, `AreaMulti`, `DelayMulti`, `DelayTarget`, `LogFan`, `nGatesMin`, `fRecovery`, `fSwitching`, `fSkipFanout`, `fUseProfile`, and `fUseBuffs` are still useful.
- Use parameter sweeps to identify promising regions, then convert the insight into targeted algorithmic changes when possible.
- Do not spend the whole campaign only creating wrappers around `Abc_NtkMap` with new constants unless the version explicitly explains why the wrapper is still scientifically useful.
- When a version is primarily a parameter variant, log that it is a parameter probe and state what internal mapper behavior it is meant to diagnose.

## Evaluation Contract

Default baseline flow:

```sh
abc -c 'read <library> ; read <design> ; resyn; resyn2; dch -v; map; topo; buffer; upsize -v; dnsize -v; stime'
```

Default candidate flow template:

```sh
abc -c 'read <library> ; read <design> ; resyn; resyn2; dch -v; stmapN; topo; buffer; upsize -v; dnsize -v; stime'
```

Use the required repo benchmark set:

- `benchmarks/i10.aig`
- `benchmarks/ode.abc.blif`
- `benchmarks/or1200.abc.blif`
- `benchmarks/syn2.abc.blif`

The harness agent should configure all four benchmark designs unless the user explicitly changes the benchmark policy. If it finds additional valid benchmark designs or libraries in the repo, it may add them only as optional supplemental coverage, and it must prefer concrete existing paths over placeholders.

Metrics:

- primary: final `stime` delay
- secondary: final `stime` area
- required: every benchmark must build, run, and produce parseable final metrics

Objective:

- compare each candidate against the classic `map` baseline under the same downstream flow
- prefer delay improvement first, area second
- do not count a version if required benchmark output is missing, crashes, times out, or is not parseable

Timeouts and parallelism:

- allow up to 10 minutes per benchmark design evaluation
- allow up to 10 minutes per review pass
- run benchmarks in parallel when more than one benchmark is configured

## Correctness Gates

An iteration counts only if all of these pass:

1. the repository builds
2. the new numbered implementation exists
3. the new numbered command exists and is callable
4. `stmapN -h` usage text works
5. a smoke run succeeds
6. combinational equivalence checking passes for original versus mapped/post-mapped designs where applicable
7. the version is evaluated on the required benchmark flow, or on an explicitly logged alternate flow only when the default flow is invalid for that variant
8. benchmark outputs are complete and parseable
9. the AutoEDA version log is updated with hypothesis, validation, results, and next-step recommendation
10. the configured review loop is completed
11. accepted review findings are addressed and revalidated
12. the version is committed if git policy allows

Do not count incomplete, crashing, non-equivalent, timed-out, or partially evaluated versions.

## Iteration Contract

Target successful iterations for smoke testing this harness:

- 1 canary iteration first

For a full campaign, use 100 successful numbered `stmapN` iterations unless the user changes the target.

Each successful iteration must:

- implement one clear hypothesis
- preserve older variants
- update build integration
- run build, smoke, correctness, and benchmark evaluation
- record final results in AutoEDA runtime logs
- include a next-step recommendation for the next version

Do not skip version numbers in the normal successful sequence.

## Review Policy

Substantial planning or implementation work must follow a review loop:

1. draft plan or implementation
2. run the configured review tool
3. apply accepted findings
4. revalidate affected behavior
5. run a second review when changes are substantial
6. record accepted, rejected, and open findings

Purely mechanical non-semantic edits may skip the second review only when clearly logged.

## Logging And Artifacts

Use AutoEDA runtime files as the campaign source of truth:

- `.autoeda/project_pack/agent.md`
- `.autoeda/project_pack/mission.md`
- `.autoeda/project_pack/repo_guide.md`
- `.autoeda/project_pack/evaluation.yaml`
- `.autoeda/project_pack/git.yaml`
- `.autoeda/project_pack/supervisor.yaml`
- `.autoeda/project_pack/skills/`
- `.autoeda/runtime/campaign_state.json`
- `.autoeda/runtime/versions.md`
- `.autoeda/runtime/results/`
- `.autoeda/runtime/final_report.md`

Do not ignore `.autoeda/` for this campaign. The generated project pack and runtime artifacts are intended to be committed as part of the evolving research record. The only exception is material that contains raw credentials, machine-local secrets, or transient process state that is not useful for reproduction.

Each version log entry should include:

- hypothesis
- motivation
- files changed
- command name
- algorithm summary
- validation run
- benchmark results
- correctness results
- review findings accepted or rejected
- commit
- next-step recommendation

## Git Policy

- Use existing local git authentication only.
- Never store tokens, secrets, or credentials in repository files, skill files, prompts, or logs.
- Prefer one coherent commit per successful numbered version.
- Each successful iteration commit should include the implementation changes and the corresponding `.autoeda/` project-pack/runtime updates needed to reproduce and audit that iteration.
- Do not add `.autoeda/` to `.gitignore` or `.git/info/exclude`; if a generated file inside `.autoeda/` contains real secrets or purely local transient state, fix the generated setup or ignore only that specific unsafe file.
- Push only when allowed by the generated AutoEDA git policy and local authentication.
- Do not force-push.
- Keep canary push disabled unless explicitly changed by the user.

## Stop Conditions

Stop when:

- target iterations and final report are complete
- an irrecoverable blocker is found and reported
- required benchmark, library, build, or correctness assets are unavailable
- repeated deterministic failure indicates the setup must be revised

## Practical Guidance

- Reuse existing ABC infrastructure when it is sound.
- For exploratory implementations, add isolated new `.c` and `.h` files rather than mutating stable code paths unnecessarily.
- Keep command glue separate from deeper implementation where possible.
- Favor one clear hypothesis per version.
- Final `stime` results are the main comparison point.
- Functional correctness is mandatory for mapper changes.
