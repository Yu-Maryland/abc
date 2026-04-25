# stmap28 Review Summary

- Prompt-form review command: failed because the installed Codex CLI rejects `--uncommitted` with a positional prompt; logged in `review_pass1.log`.
- Review pass 1 supported command: completed via stdin in `review_pass1_supported.log`.
- Accepted finding: `nStmap28ModeratePenaltySeed` counted refs=4 moderate candidates even when `penalty-factor = 0.0`.
- Fix: `penalty seed diag` and `moderate-penalty-seed` now require `Stmap28PenaltyFactor > 0.0`; the refs=4 safe seed remains visible in `relief_diag.csv`.
- Revalidation after fix: build, help, smoke, full benchmark metrics, diagnostics, and CEC were rerun successfully.
- Prompt-form review pass 2: failed for the same local CLI argument conflict; logged in `review_pass2.log`.
- Review pass 2 supported command: completed via stdin in `review_pass2_supported.log`.
- Review pass 2 findings: none; the review reported no discrete correctness issue in command wiring, mode-29 gating, counter initialization, or artifacts.
- Rejected findings: none.
- Open findings: none.
