# stmap75 Review Summary

- Review pass 1 used the configured `codex_review` command shape first; the local CLI rejected the positional prompt with `--uncommitted`, logged in `review_pass1_positional.log`. The same prompt via stdin completed in `review_pass1.log` with no discrete correctness findings.
- Review pass 2 accepted a P2 diagnostic scoping finding: a global watch list could conflate benchmark-local AIG IDs across designs. The implementation was changed to choose one benchmark-specific watch ID per network and then rebuilt/revalidated.
- Review pass 3 accepted two post-fix findings: benchmark matching needed exact normalized names rather than substring matching, and disabling diagnostics needed to reset counters/IDs to avoid stale rows in a reused ABC process. Both fixes were implemented and revalidated.
- Review pass 4 accepted a P3 path-name finding: `./benchmarks/i10.aig` produced network name `./benchmarks/i10` and missed the exact `i10` watch. The matcher now strips directory prefixes before exact basename matching.
- Review pass 5, after the path-normalization fix and post-fix validation, reported no discrete correctness issues. Residual risk is limited to diagnostic coverage for future benchmark names outside the required family; unknown basenames intentionally remain disabled.

Accepted findings addressed:
- Per-network watch selection prevents cross-design AIG-ID collisions.
- Exact basename matching covers canonical and equivalent benchmark paths without substring false positives.
- Enable/disable paths reset selected-match diagnostic state.
- Unknown-network and stale-state checks confirm watched `i10` rows do not leak into an unknown `decoder` network in the same ABC process.

Rejected findings:
- None.

Open findings:
- None.
