# stmap64 Review Summary

- review tool: `codex_review` via `codex exec review --uncommitted --model gpt-5.5 -c model_reasoning_effort="xhigh" -c service_tier="fast" --dangerously-bypass-approvals-and-sandbox`
- configured positional prompt form: attempted on all four passes and rejected by the installed Codex CLI because `--uncommitted` cannot be combined with a positional prompt. Logs: `review_pass1_positional.log`, `review_pass2_positional.log`, `review_pass3_positional.log`, `review_pass4_positional.log`.
- supported stdin review form: completed on all four passes.

## Findings And Fixes

- pass 1 accepted finding: preserve `vOrigNodeIds` through DFS network duplication before final witness matching. Fix: added `Abc_NtkDupOrigNodeIds()` and used it in `Abc_NtkDup`, `Abc_NtkDupDfs`, and `Abc_NtkDupDfsNoBarBufs`.
- pass 2 accepted finding: scope final-witness global state to the mapped network lineage so an unrelated later `stime` cannot print stale witnesses. Fix: added command-scoped final-witness lifecycle state and updated it as duplicated networks replace the mapped network.
- pass 3 accepted finding: preserve phase/literal identity for final witness matching. Fix: mapper witness records now carry phase, final matching compares exact original AIG literal, and `final_witness.csv` records `phase`.
- pass 4 result: no discrete correctness findings; review reported the final stmap64 witness plumbing is scoped and preserved across relevant duplication paths.

## Revalidation

- build, help, smoke, syn2 witness, stale-witness lifecycle, full benchmark evaluation, and CEC were rerun after the final phase fix.
- final full evaluation log: `eval_driver_after_review3.log` with exit code `0`.
- final CEC summary: all four required benchmarks passed.
