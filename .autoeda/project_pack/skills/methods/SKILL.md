# ABC STMAP Research Methods Skill

Use this skill when choosing a hypothesis for the next `stmapN` version.

## Scientific Discipline

- One successful version should test one clear hypothesis.
- Parameter sweeps are allowed as probes, but the campaign should not become a
  sequence of wrappers around `Abc_NtkMap` constants. Convert useful sweep
  results into targeted mapper or SCL timing changes.
- Compare against classic `map` only after the full downstream flow:
  `topo; buffer; upsize -v; dnsize -v; stime`.
- Optimize final `stime` delay first and area second. Preserve correctness even
  when a timing result looks promising.
- Use the canary iteration to prove build, command registration, help text,
  smoke, benchmark parsing, CEC, review, logging, and local commit mechanics.
  After canary, move quickly toward mapper-internal or SCL-aware hypotheses.

## High-Value Research Directions

- Improve how SCL/Liberty timing is collapsed into mapper genlib/supergate
  timing in `Abc_SclDeriveGenlib` usage, including slew, gain, load assumptions,
  minimum gate filters, and profile transfer behavior.
- Feed SCL-derived criticality, load, fanout, or slew proxies into mapper
  required times or match penalties before `Map_MappingMatches`.
- Adjust cut or match scoring to penalize choices that create poor downstream
  load, slew, fanout, or buffer insertion behavior.
- Explore two-pass mapping: first map and evaluate or approximate SCL timing,
  then remap with explicit penalties or required-time updates.
- Investigate reconstruction choices in `Abc_NtkFromMap` and phase/supergate
  selection when locally equivalent choices produce different downstream
  `stime` results.
- Add instrumentation that correlates mapper arrival, area flow, selected
  supergates, fanout/load proxies, and final `stime` critical-path data.

## Useful Probe Parameters

Treat changes to `Slew`, `Gain`, `AreaMulti`, `DelayMulti`, `DelayTarget`,
`LogFan`, `nGatesMin`, `fRecovery`, `fSwitching`, `fSkipFanout`,
`fUseProfile`, and `fUseBuffs` as diagnostic probes unless a version explains
why the parameterized behavior is itself the hypothesis.

## Signals To Record

- Baseline and candidate final `stime` delay and area for each required
  benchmark.
- Baseline command line and candidate command line for each benchmark, including
  the exact `read_lib`, design, mapper command, and downstream optimization flow.
- Whether improvements are uniform or concentrated in one design.
- Critical-path, fanout/load, and slew diagnostics when available.
- Mapper internal observations: selected match classes, recovery mode behavior,
  area-flow changes, required-time changes, and reconstruction differences.
- CEC status and any conditions under which CEC needed a strashed temporary.

## Out Of Scope

- Skipping required benchmarks to claim success.
- Counting versions with unparseable metrics, failed CEC, missing help text, or
  incomplete review.
- Silently changing `map` or unrelated stable commands without documenting an
  intentional infrastructure reason.
- Replacing the binding `agent.md` policy with a generic optimization policy.
