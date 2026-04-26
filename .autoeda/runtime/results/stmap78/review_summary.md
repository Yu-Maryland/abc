# stmap78 Review Summary

- Pass 1 positional configured invocation: rejected by installed Codex CLI because `--uncommitted` conflicts with positional prompt; logged in `review_pass1_positional.log` with exit 2.
- Pass 1 supported stdin invocation: completed with one accepted P2 diagnostic finding. The demand-path stack reported the outer requested phase for inverter-fallback parents while leaf recursion used the opposite cut phase.
- Accepted fix: added `Abc_Stmap78DemandPathSetTopPhase()` and temporarily switches the stack top phase during fallback cut recursion, then restores it before creating the inverter.
- Revalidation after fix: build/help/smoke/path-normalization/stale-state/full eval/CEC passed; `fallback_phase_consistency.log` reports `mismatches=0`.
- Pass 2 positional configured invocation: same local CLI rejection; logged in `review_pass2_positional.log` with exit 2.
- Pass 2 supported stdin invocation: completed with no actionable correctness, build-integration, diagnostic lifecycle, or maintainability findings.

Open findings: none.
Rejected findings: none.
