# Canonical Version Log

This is the append-only experiment log for the campaign.
Each completed version should append a new `## versionN` section.

## version0 / stmap0

- hypothesis: Aligning the mapper's SCL-derived genlib gain estimate with the downstream `buffer`/sizer default (`Gain=300` instead of classic `map`'s `Gain=250`) may reduce mismatch between mapper delay scoring and final post-flow `stime`.
- motivation: This is the canary-grade diagnostic probe for the `stmap` family. It proves command registration, build integration, help text, benchmark parsing, CEC, review, logging, and commit mechanics while testing a single SCL timing-model parameter that directly relates to downstream buffering.
- command name: `stmap0`
- files changed:
  - `src/base/abci/abc.c`
  - `src/base/abci/abcStmap_0.c`
  - `src/base/abci/module.make`
  - `abclib.dsp`
  - `.gitignore`
  - `7nm_lvt_ff.lib`
  - `benchmarks/i10.aig`
  - `benchmarks/ode.abc.blif`
  - `benchmarks/or1200.abc.blif`
  - `benchmarks/syn2.abc.blif`
  - `.autoeda/project_pack/*`
  - `.autoeda/runtime/results/stmap0/*`
  - `.autoeda/runtime/versions.md`
  - `.autoeda/runtime/campaign_state.json`
- algorithm summary: `stmap0` mirrors the classic `map` command line and mapping flow, preserving all toggles and shared mapper behavior, but changes the default SCL genlib derivation gain from `250` to `300`. Users may still override the gain with `-G`.
- validation run:
  - build: `make ABC_USE_NO_READLINE=1` passed; artifact `./abc` produced.
  - help: `./abc -c "stmap0 -h"` printed usage text with default `-G` shown as `300.00`.
  - smoke: `read_lib 7nm_lvt_ff.lib; read benchmarks/i10.aig; resyn; resyn2; dch -v; stmap0; topo; buffer; upsize -v; dnsize -v; stime` passed with parseable final metrics.
  - full evaluation artifacts: `.autoeda/runtime/results/stmap0/summary.json`, `metrics.csv`, `comparison.csv`, raw baseline/candidate logs, CEC temporaries, and CEC logs.
- benchmark results:
  - `benchmarks/i10.aig`: baseline delay `207.24 ps`, area `1263.44`; candidate delay `207.61 ps`, area `1272.54`; delay delta `+0.37 ps` (`+0.18%`), area delta `+9.10` (`+0.72%`).
  - `benchmarks/ode.abc.blif`: baseline delay `531.32 ps`, area `12491.21`; candidate delay `561.22 ps`, area `11444.25`; delay delta `+29.90 ps` (`+5.63%`), area delta `-1046.96` (`-8.38%`).
  - `benchmarks/or1200.abc.blif`: baseline delay `587.08 ps`, area `4798.34`; candidate delay `573.53 ps`, area `4765.68`; delay delta `-13.55 ps` (`-2.31%`), area delta `-32.66` (`-0.68%`).
  - `benchmarks/syn2.abc.blif`: baseline delay `508.82 ps`, area `21296.60`; candidate delay `515.37 ps`, area `20161.69`; delay delta `+6.55 ps` (`+1.29%`), area delta `-1134.91` (`-5.33%`).
- correctness results:
  - build passed.
  - numbered implementation exists: `src/base/abci/abcStmap_0.c`.
  - numbered command exists and is registered as `stmap0`.
  - command help passed.
  - smoke flow passed.
  - benchmark metrics passed for all four required designs.
  - CEC passed for all four required designs using original and candidate strashed AIG temporaries under `.autoeda/runtime/results/stmap0/`.
- review pass 1:
  - configured prompt form failed because the installed Codex CLI rejects `--uncommitted` together with a positional prompt; failed attempts are logged in `review_pass1.log` and `review_pass1_stdin.log`.
  - supported configured-tool invocation completed: `codex exec review --uncommitted --model gpt-5.5 -c model_reasoning_effort="xhigh" -c service_tier="fast" --dangerously-bypass-approvals-and-sandbox`; log: `.autoeda/runtime/results/stmap0/review_pass1_supported.log`.
- accepted findings:
  - P1: Add `abcStmap_0.c` to the Windows/MSVC source list in `abclib.dsp`; addressed.
  - P2: Append the completed `stmap0` record to the canonical version log; addressed.
  - P3: Exclude live supervisor PID/lock state and append-only launcher transcript from the commit; addressed by specific `.gitignore` entries for machine-local runtime state while keeping stable harness and result logs tracked.
- rejected findings: none.
- open findings: none.
- review pass 2: skipped because accepted changes after pass 1 were mechanical build-list/logging/ignore updates; affected local behavior was revalidated instead.
- commit: local commit created with message `stmap0: gain-aligned SCL genlib probe`; the exact hash is intentionally left to Git history rather than embedded in this self-referential log record.
- push: disabled by `git.yaml`; no push performed.
- next-step recommendation: `Gain=300` gives useful evidence but is not uniformly better: it improved `or1200` delay and reduced area on three larger designs, while delaying `i10`, `ode`, and `syn2`. For `stmap1`, move beyond a single gain probe toward a mapper-internal or SCL-aware load/fanout hypothesis, such as adding a critical fanout/load proxy or required-time bias only where downstream `stime` shows sensitivity.
