# stmap19 Review Summary

- review pass 1 prompt-form invocation: failed because the installed Codex CLI rejects `--uncommitted` with a positional prompt; log: `.autoeda/runtime/results/stmap19/review_pass1.log`.
- review pass 1 supported invocation: completed via stdin with the configured review tool; accepted findings were the stale `agent-running` runtime state and an early-seed diagnostic overcount; log: `.autoeda/runtime/results/stmap19/review_pass1_supported.log`.
- accepted pass 1 fixes: gated `nStmap19EarlySeed` on pre-profile relaxed-path admissions and moved campaign state to a restartable status pending final completion. Build, help, smoke, full benchmark evaluation, diagnostic parsing, and CEC were rerun.
- review pass 2 prompt-form invocation: failed for the same local CLI positional-prompt conflict; log: `.autoeda/runtime/results/stmap19/review_pass2.log`.
- review pass 2 supported invocation: completed via stdin; accepted finding was to record `stmap19` in the canonical version log before completing the iteration; log: `.autoeda/runtime/results/stmap19/review_pass2_supported.log`.
- rejected findings: none.
- open findings: none.
