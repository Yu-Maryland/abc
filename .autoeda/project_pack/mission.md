# Mission

## Project

abc-harness-stmap-advanced

## Goal

Run an AutoEDA research campaign against Berkeley ABC to create numbered
`stmapN` technology-mapper variants that improve final post-flow standard-cell
timing. The binding policy is `agent.md`; this mission summarizes the setup
around that policy and does not replace it.

The campaign target is an advanced downstream-aware mapper family that uses
buffering, sizing, and final `stime` feedback as the real quality signal. Classic
`map` is the baseline. Candidate commands use the `stmap` family name:
`stmap0`, `stmap1`, `stmap2`, and so on.

## Success Criteria

- Build ABC with the configured Makefile command and produce `./abc`.
- Add one numbered implementation and command per successful iteration.
- Keep all previous `stmapN` commands buildable and comparable.
- Ensure every new command supports `-h` usage text.
- Run the required flow on all four benchmark designs:
  `benchmarks/i10.aig`, `benchmarks/ode.abc.blif`,
  `benchmarks/or1200.abc.blif`, and `benchmarks/syn2.abc.blif`.
- Compare candidates against the same downstream flow using classic `map`.
- Treat final `stime` delay as the primary metric and final `stime` area as the
  secondary metric.
- Count an iteration only when build, smoke, command help, equivalence where
  applicable, complete benchmark evaluation, parseable metrics, review, logging,
  and commit requirements pass.

## Target Iterations

- Canary: exactly 1 successful iteration when launched in canary mode.
- Full campaign: 100 successful numbered `stmapN` iterations.

## Stopping Rules

- stop on target completion
- stop on explicit irrecoverable blocker
- stop on repeated deterministic failure
- stop when required benchmark, library, build, or correctness assets are
  unavailable
