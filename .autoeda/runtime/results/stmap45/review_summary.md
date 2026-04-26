# stmap45 review summary

- configured prompt-form review: attempted once as `codex exec review --uncommitted ... "<prompt>"`; the installed Codex CLI rejected the positional prompt with `--uncommitted`. Log: `review_pass1_positional.log`.
- review pass 1: supported configured-tool invocation with the same prompt via stdin completed successfully. Log: `review_pass1.log`.
- accepted findings: one P2 source finding. Mode 46 could admit tight-critical moderate soft seeds with the lighter one-inverter margin without checking SCL pressure agreement.
- accepted fix: `mapperMatch.c` now distinguishes mode-46 moderate soft seeds from positive-penalty candidates, computes node/cut pressure agreement for every moderate soft seed, and requires that agreement before any such seed can avoid strict fallback.
- revalidation after accepted fix: build, `stmap45 -h`, smoke, full four-benchmark evaluation, parseable metrics, diagnostics, and all CEC checks passed.
- review pass 2: supported configured-tool invocation completed against the patched implementation and refreshed artifacts. Log: `review_pass2.log`.
- review pass 2 findings: no discrete correctness issues.
- rejected findings: none.
- open findings: none.
