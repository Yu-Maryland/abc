# AutoEDA stmap Campaign Final Report

Generated: 2026-04-26T22:56:25Z

Evidence sources: `.autoeda/runtime/versions.md` and `.autoeda/runtime/results/stmap0` through `.autoeda/runtime/results/stmap99`. The final iteration commit pushed to `origin/master` is `af838335a780743e7d71bc6b2e9308c6a3c21a44`.

## setup_summary

- Target repo: `/Users/cunxiy/Documents/research/autoresearch-eda/abc-harness`
- Campaign family: `stmap`
- Completed iterations: `100` (`stmap0` through `stmap99`)
- Baseline command: `map`
- Library: `7nm_lvt_ff.lib`
- Required benchmarks: `benchmarks/i10.aig`, `benchmarks/ode.abc.blif`, `benchmarks/or1200.abc.blif`, `benchmarks/syn2.abc.blif`
- Baseline flow: `./abc -c "read_lib 7nm_lvt_ff.lib; read <design>; resyn; resyn2; dch -v; map; topo; buffer; upsize -v; dnsize -v; stime"`
- Candidate flow pattern: `read_lib; read; resyn; resyn2; dch -v; stmapN; topo; buffer; upsize -v; dnsize -v; stime`
- Ranking objective: minimize final `stime` delay per benchmark, with final `stime` area as the secondary metric.

## results_summary

The campaign finished with all 100 comparison tables present and 100 canonical version-log entries. The final endpoint, `stmap99`, is a controlled rollback of the late `stmap94` behavior and gives a stable all-benchmark delay improvement versus `map`.

| summary | version | avg delay ps | avg delay delta ps | avg delay delta pct | avg area | avg area delta | geomean area |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline | `map` | 458.62 | 0.00 | 0.00% | 9962.40 | 0.00 | 6337.09 |
| best campaign mean delay | `stmap54` | 440.34 | -18.27 | -3.98% | 9942.80 | -19.59 | 6332.31 |
| final endpoint | `stmap99` | 440.50 | -18.11 | -3.95% | 9934.06 | -28.34 | 6329.82 |

Best per-benchmark delay results:

| benchmark | best version | candidate delay ps | delay delta ps | delay delta pct | candidate area | area delta |
|---|---:|---:|---:|---:|---:|---:|
| `benchmarks/i10.aig` | `stmap2` | 198.64 | -8.60 | -4.15% | 1303.10 | 39.66 |
| `benchmarks/ode.abc.blif` | `stmap1` | 502.82 | -28.50 | -5.36% | 11613.14 | -878.07 |
| `benchmarks/or1200.abc.blif` | `stmap2` | 553.77 | -33.31 | -5.67% | 5005.96 | 207.62 |
| `benchmarks/syn2.abc.blif` | `stmap1` | 478.52 | -30.30 | -5.95% | 22004.14 | 707.54 |

Final `stmap99` results:

| benchmark | baseline delay ps | candidate delay ps | delay delta ps | delay delta pct | baseline area | candidate area | area delta |
|---|---:|---:|---:|---:|---:|---:|---:|
| `benchmarks/i10.aig` | 207.24 | 198.64 | -8.60 | -4.15% | 1263.44 | 1303.10 | 39.66 |
| `benchmarks/ode.abc.blif` | 531.32 | 522.05 | -9.27 | -1.74% | 12491.21 | 11334.38 | -1156.83 |
| `benchmarks/or1200.abc.blif` | 587.08 | 561.55 | -25.53 | -4.35% | 4798.34 | 4895.15 | 96.81 |
| `benchmarks/syn2.abc.blif` | 508.82 | 479.78 | -29.04 | -5.71% | 21296.60 | 22203.59 | 906.99 |

Key outcome: the campaign found earlier single-benchmark optima in `stmap1` and `stmap2`, the best average-delay point in `stmap54`, and a final robust endpoint in `stmap99` that improves all four benchmarks. 94 of the 100 iterations improved delay on all four required benchmarks.

## per_benchmark_trends

Plots written under `.autoeda/runtime/plots`:

- `overall_delay_progression.png`: candidate average delay, required `avg_so_far`, required `best_so_far`, and baseline average delay.
- `per_benchmark_delay_trends.png`: per-benchmark candidate delay and per-benchmark best-so-far delay against baseline.
- `delay_area_tradeoff.png`: average final delay versus average final area by iteration.
- `iteration_summary.csv`: parsed iteration-level summary used for the plots and report tables.

The average-delay best-so-far trace drops sharply in the first few iterations, then improves more gradually until `stmap54` reaches 440.34 ps. The final late-family endpoint keeps the same `i10`, `ode`, and `or1200` values as the stable post-sticky endpoint and lands at 440.50 ps average delay, only 0.16 ps behind the best mean-delay point.

Per-benchmark trend conclusions:

- `i10`: best delay is 198.64 ps from `stmap2`; late endpoints preserve this value.
- `ode`: best delay is 502.82 ps from `stmap1`; later stable endpoints trade that best delay for lower area versus baseline.
- `or1200`: best delay is 553.77 ps from `stmap2`; late `stmap94`/`stmap99` behavior is the best endpoint from the emitted-drive investigation at 561.55 ps.
- `syn2`: best delay is 478.52 ps from `stmap1`; `stmap54` gives the best campaign-wide compromise with `syn2` at 479.14 ps.

## best_reproduction_flows

Best campaign-wide average-delay reproduction flow:

```sh
./abc -c "read_lib 7nm_lvt_ff.lib; read <design>; resyn; resyn2; dch -v; stmap54; topo; buffer; upsize -v; dnsize -v; stime"
```

Use this command with each required benchmark substituted for `<design>`. The exact per-design commands are:

```sh
./abc -c "read_lib 7nm_lvt_ff.lib; read benchmarks/i10.aig; resyn; resyn2; dch -v; stmap54; topo; buffer; upsize -v; dnsize -v; stime"
./abc -c "read_lib 7nm_lvt_ff.lib; read benchmarks/ode.abc.blif; resyn; resyn2; dch -v; stmap54; topo; buffer; upsize -v; dnsize -v; stime"
./abc -c "read_lib 7nm_lvt_ff.lib; read benchmarks/or1200.abc.blif; resyn; resyn2; dch -v; stmap54; topo; buffer; upsize -v; dnsize -v; stime"
./abc -c "read_lib 7nm_lvt_ff.lib; read benchmarks/syn2.abc.blif; resyn; resyn2; dch -v; stmap54; topo; buffer; upsize -v; dnsize -v; stime"
```

Per-benchmark best-delay commands:

```sh
./abc -c "read_lib 7nm_lvt_ff.lib; read benchmarks/i10.aig; resyn; resyn2; dch -v; stmap2; topo; buffer; upsize -v; dnsize -v; stime"
./abc -c "read_lib 7nm_lvt_ff.lib; read benchmarks/ode.abc.blif; resyn; resyn2; dch -v; stmap1; topo; buffer; upsize -v; dnsize -v; stime"
./abc -c "read_lib 7nm_lvt_ff.lib; read benchmarks/or1200.abc.blif; resyn; resyn2; dch -v; stmap2; topo; buffer; upsize -v; dnsize -v; stime"
./abc -c "read_lib 7nm_lvt_ff.lib; read benchmarks/syn2.abc.blif; resyn; resyn2; dch -v; stmap1; topo; buffer; upsize -v; dnsize -v; stime"
```

Final stable endpoint reproduction flow:

```sh
./abc -c "read_lib 7nm_lvt_ff.lib; read <design>; resyn; resyn2; dch -v; stmap99; topo; buffer; upsize -v; dnsize -v; stime"
```

`stmap99` is the preferred final endpoint for reproducing the late-family conclusion: the less intrusive post-sticky emitted-drive behavior was better than the forced-survival variants `stmap95` through `stmap98` on final downstream timing.

## correctness_summary

The final campaign state is based on completed iteration records, not speculative runs. All 100 iterations have baseline and candidate logs for all four required benchmarks, all 100 have CEC logs for all four required benchmarks, and all 100 have parsed comparison tables. `stmap99` additionally records the current final artifact schema: build, help, smoke, path-normalization, stale-state, implementation checks, full benchmark metrics, artifact check, version-log check, and CEC all passed under `.autoeda/runtime/results/stmap99/`.

The final `stmap99` CEC summary reports equivalence for each required benchmark:

| benchmark | CEC status |
|---|---:|
| `benchmarks/i10.aig` | pass |
| `benchmarks/ode.abc.blif` | pass |
| `benchmarks/or1200.abc.blif` | pass |
| `benchmarks/syn2.abc.blif` | pass |

No final-report result is taken from an unlogged manual run; the tables and plots are derived from `comparison.csv` files under `.autoeda/runtime/results`.

## review_summary

The project required a configured review loop. The configured positional prompt command was attempted where applicable, but this local Codex CLI rejects the positional prompt form when combined with `--uncommitted`; those failures were logged as review-tool invocation incompatibilities. The supported stdin form of the same configured review tool was then run, and accepted findings were addressed with revalidation as recorded in `versions.md`.

Late-review examples are representative of the closeout quality gate: `stmap95` accepted and fixed a sticky-veto source issue, `stmap96` accepted and fixed a diagnostic-banner issue, `stmap97` corrected runtime state bookkeeping, and `stmap98` plus `stmap99` completed two supported review passes with no actionable findings. The final source state after `stmap99` has no open review findings recorded in the canonical version log.
