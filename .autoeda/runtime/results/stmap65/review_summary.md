# stmap65 Review Summary

- Review tool: `codex exec review --uncommitted --model gpt-5.5 -c model_reasoning_effort="xhigh" -c service_tier="fast" --dangerously-bypass-approvals-and-sandbox`.
- Positional prompt behavior: both configured positional invocations were attempted and rejected by the local Codex CLI when combined with `--uncommitted`; see `review_pass1_positional.log` and `review_pass2_positional.log`.
- Supported invocation: both reviews completed with the same prompts through stdin; see `review_pass1.log` and `review_pass2.log`.
- Accepted findings: pass 1 found that the cut-only diagnostic gate did not include the new stmap65 strong-node guard when computing `raw_expected`.
- Fix applied: `Map_MatchStmap61PrintCutOnlyGateDiag()` now includes the stmap65 guard with node pressure in `[1.25, 1.55]` while preserving the stmap63 diagnostic path with node pressure in `[1.05, 1.55]`.
- Revalidation after accepted finding: build, help, smoke, full benchmark metrics, diagnostic parsing, and CEC were rerun and passed.
- Review pass 2 findings: no actionable correctness issues.
- Rejected findings: none.
- Open findings: none.
