Review the final uncommitted stmap65 iteration after addressing review pass 1.

Accepted pass-1 finding:
- The stmap61 cut-only diagnostic helper did not model the new stmap65 strong-node load-drop guard when computing `fLoadDropGate` / `raw_expected`.

Fix applied:
- `Map_MatchStmap61PrintCutOnlyGateDiag()` now treats the load-drop diagnostic gate as active when either the original stmap63 guard is enabled with node pressure in [1.05, 1.55], or the stmap65 strong-node guard is enabled with node pressure in [1.25, 1.55]. Shared feedback, entry, cut pressure, arrival, and slack gates remain common.

Final stmap65 behavior:
- New command `stmap65` is registered and build-wired.
- It keeps bounded-pressure mapper mode 57 and selected-gain behavior.
- It does not enable the broader stmap63 load-drop guard.
- It enables only `Map_Stmap65SetStrongNodeLoadDropGuard(1)` around the final remap and resets it immediately afterward.
- The strong-node guard admits cut-only candidates only with node pressure 1.25 to 1.55, cut pressure 1.95 to 2.20, severe feedback/entry gates, moderate deep-seed gain, strong arrival improvement, and slack pass.

Post-fix validation:
- `make ABC_USE_NO_READLINE=1` passed.
- `./abc -c "stmap65 -h"` passed.
- Smoke flow on `benchmarks/i10.aig` passed with final stime delay 198.64 ps and area 1303.10.
- Full benchmark metrics and CEC reran successfully on all four required benchmarks.
- stmap65 matches stmap64/stmap63 final QoR on the required suite while blocking the previous syn2 cut-only seed (`cut_only_exception_seed = 0` for syn2).

Please report only actionable correctness, build integration, behavior scoping, diagnostic consistency, artifact, or validation issues. Include severity, file/line, and concrete failure mode.
