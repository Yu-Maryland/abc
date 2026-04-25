Review the uncommitted stmap1 AutoEDA iteration for correctness and campaign-policy issues.

Focus areas:
- The new stmap1 command in src/base/abci/abcStmap_1.c and its registration/build integration.
- The gated mapperMatch.c change that interprets fSkipFanout == 2 as the stmap1 selective high-fanout wide-cut guard, while preserving classic map and stmap0 behavior.
- Evaluation/logging artifacts under .autoeda/runtime/results/stmap1 and runtime state/log updates.
- Any missing correctness, help, smoke, benchmark, CEC, or review requirements from the project pack.

Report findings by severity with concrete file/line references. Do not make edits.
