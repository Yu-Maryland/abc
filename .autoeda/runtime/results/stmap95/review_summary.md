# stmap95 review summary

- configured positional review pass 1: attempted with `codex exec review --uncommitted ... "<prompt>"`; local Codex CLI rejected `--uncommitted` with a positional prompt. Exit `2`; log `review_pass1_positional.log`.
- supported stdin review pass 1: completed with exit `0`; accepted two findings. P2 removed the same-mode guard so a mode-2 sticky target can block later mode-3 replacements. P3 preserved the pre-veto accepted state in diagnostics with `fAcceptedBeforeSticky`.
- revalidation after pass 1 fixes: build, help, smoke, path-normalization, stale-state, implementation checks, full benchmark metrics, diagnostic parsing, and CEC passed.
- configured positional review pass 2: attempted with the same configured prompt pattern; local Codex CLI rejected the positional prompt. Exit `2`; log `review_pass2_positional.log`.
- supported stdin review pass 2: completed with exit `0`; accepted one P2 source finding that override-driven replacements could bypass the sticky veto when `fAccepted` was initially false.
- accepted pass 2 fix: emitted-drive eligibility is now computed before the sticky veto, the sticky veto applies when either the core comparator accepted the candidate or the candidate is emitted-drive-eligible, and already-selected/override counters are updated only after sticky allows the replacement.
- revalidation after pass 2 fix: build, help, smoke, path-normalization, stale-state, implementation checks, full benchmark metrics, diagnostic parsing, and CEC passed.
- configured positional review pass 3: attempted with the same configured prompt pattern; local Codex CLI rejected the positional prompt. Exit `2`; log `review_pass3_positional.log`.
- supported stdin review pass 3: completed with exit `0`; no remaining source, build integration, stale-state, older-command regression, or diagnostic-counter findings. It repeated the expected P2 state-finalization finding because `campaign_state.json` still showed stmap95 active before closeout.
- accepted state finding: stmap95 closeout finalizes `campaign_state.json` after canonical version logging and artifact checks, clearing `active_iteration` and `active_version` before commit.
- rejected findings: none.
- open findings: none after state finalization.
- source changes after final source review: none; only runtime review summary, canonical version log, version-log check, artifact check, campaign-state finalization, and git bookkeeping were added after the clean final source review.
