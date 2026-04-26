# stmap53 review summary

- Review tool: `codex exec review --uncommitted --model gpt-5.5 -c model_reasoning_effort="xhigh" -c service_tier="fast" --dangerously-bypass-approvals-and-sandbox`.
- Positional prompt attempts for passes 1, 2, and 3 were run to match the configured pattern. The installed Codex CLI rejected each positional prompt when combined with `--uncommitted`; the failures are logged in `review_pass*_positional.log`.
- Supported stdin invocations with the same prompts completed successfully for passes 1, 2, and 3.

Accepted findings:

- Pass 1: `[P2] Reject zero-pressure sides in stmap53 exception`. Fixed by requiring both node and cut sink-pressure ratios to be present, within the bounded pressure band, and close to each other before admitting the pressure-near exception. Revalidated with build, help, smoke, full metrics, and CEC.
- Pass 2: `[P2] Gate the exception on pressure entries`. Fixed by passing the measured sink-pressure entry count through `Map_Stmap45SetSclLoadFeedbackWithEntries()` for stmap53 and gating the exception on feedback severity `>= 0.85` and sink-pressure entries `>= 8000`, matching the severe-pressure branch. Revalidated with build, help, smoke, full metrics, and CEC.

Rejected findings: none.

Open findings: none. Pass 3 reported no actionable correctness issues in the final uncommitted changes.
