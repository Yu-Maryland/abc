# stmap15 review summary

- configured prompt-form review pass 1: failed because the installed Codex CLI rejects `--uncommitted` with a positional prompt; see `review_pass1.log`.
- supported configured-tool review pass 1: completed via stdin using `codex exec review --uncommitted --model gpt-5.5 -c model_reasoning_effort="xhigh" -c service_tier="fast" --dangerously-bypass-approvals-and-sandbox`; see `review_pass1_supported.log`.
- accepted findings:
  - P2: Preserve the intended stmap14 fallback before the adaptive stmap15 profile opens. The original mode-16 implementation rejected all lower-moderate middle-slack candidates below the profile threshold instead of still allowing candidates that save at least two inverter areas.
- fix applied:
  - `src/map/mapper/mapperMatch.c` now uses a two-inverter area margin for mode 16 until the adaptive profile threshold is met, then lowers the margin to one inverter.
- revalidation after accepted fix:
  - `make ABC_USE_NO_READLINE=1` passed.
  - `./abc -c "stmap15 -h"` passed.
  - smoke flow on `benchmarks/i10.aig` passed and produced parseable final `stime` metrics.
  - all four required benchmark baseline/candidate flows completed with parseable final `stime` metrics.
  - CEC passed for all four required benchmarks.
- configured prompt-form review pass 2: failed with the same local Codex CLI argument conflict; see `review_pass2.log`.
- supported configured-tool review pass 2: completed via stdin; see `review_pass2_supported.log`.
- rejected findings: none.
- open findings: none.
