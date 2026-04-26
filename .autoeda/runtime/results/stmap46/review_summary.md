# stmap46 review summary

- configured review command attempted: `codex exec review --uncommitted --model gpt-5.5 -c model_reasoning_effort="xhigh" -c service_tier="fast" --dangerously-bypass-approvals-and-sandbox "<prompt>"`
- configured positional prompt result: failed locally because the installed Codex CLI rejects `--uncommitted` with a positional prompt; see `review_pass1_positional.log`.
- supported configured-tool invocation used for both review passes: same command and review prompt via stdin.
- review pass 1: accepted one P2 source finding. The tight exception originally only checked arrival gain and could bypass pressure agreement for candidates with slack as loose as the middle-relief window.
- accepted fix: `Map_MatchHasStmap46TightException()` now requires the high-gain predicate and `Slack <= 1.10 * SlackMargin + Epsilon`.
- revalidation after fix: build, help, smoke, full benchmark metrics, diagnostic parsing, and CEC all passed.
- review pass 2: completed with no actionable correctness issues in the changed source.
- rejected findings: none.
- open findings: none.
