# stmap96 review summary

- configured positional review pass 1: attempted with `codex exec review --uncommitted ... "<prompt>"`; local Codex CLI rejected `--uncommitted` with a positional prompt. Exit `2`; log `review_pass1_positional.log`.
- supported stdin review pass 1: completed with exit `0`; accepted one P3 diagnostic finding that unsupported networks printed `sticky-arrival-release-window = 0.50` even though the configured fallback window was `2.00`.
- accepted fix: `abcStmap_96.c` now computes `StickyArrivalWindow = ChildAigId >= 0 ? 0.50f : 2.00f`, prints that exact value, and passes the same value to `Map_Stmap93SetEmittedDriveTargetStickyArrivalWindow()`.
- revalidation after fix: build, help, smoke, path-normalization, stale-state, implementation checks, full benchmark metrics, diagnostic parsing, and CEC passed.
- configured positional review pass 2: attempted with the same configured prompt pattern; local Codex CLI rejected the positional prompt. Exit `2`; log `review_pass2_positional.log`.
- supported stdin review pass 2: completed with exit `0`; no remaining actionable correctness, build integration, stale-state, older-command regression, or diagnostic-counter findings were reported.
- rejected findings: none.
- open findings: none after pass 2.
- source changes after final review: none; only runtime review summary, canonical version log, version-log check, artifact check, campaign-state finalization, and git bookkeeping were added after the clean final source review.
