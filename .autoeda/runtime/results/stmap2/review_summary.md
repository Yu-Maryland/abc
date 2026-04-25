# stmap2 Review Summary

- configured prompt-form review: failed because local Codex CLI rejects `--uncommitted` with a positional prompt; see `review_pass1.log`.
- configured supported review: completed with prompt on stdin; see `review_pass1_supported.log`.
- accepted findings:
  - P2: mode `3` also affected switching recovery under `stmap2 -p`; fixed by limiting the slack-aware guard to mapper modes `1..3`.
- rejected findings: none.
- open findings: none.
- revalidation:
  - `make ABC_USE_NO_READLINE=1` passed after the fix; see `build_revalidate.log`.
  - `stmap2 -h` passed after the fix; see `help_revalidate.log`.
  - default `i10` smoke passed with final `stime` delay `198.64 ps`, area `1303.10`; see `smoke_i10_revalidate.log`.
  - `stmap2 -p` smoke passed with final `stime` delay `218.43 ps`, area `1753.80`; see `smoke_i10_power_revalidate.log`.
  - full default benchmark evaluation and CEC were rerun after the fix; see `summary.json`, `metrics.csv`, `comparison.csv`, and `cec_*.log`.
- second review: skipped because the accepted change was a narrow one-line mode guard fix directly addressing the review finding and all affected behavior was revalidated.
