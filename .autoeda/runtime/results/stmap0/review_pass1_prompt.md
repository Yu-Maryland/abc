Review the uncommitted stmap0 implementation for the AutoEDA ABC campaign.

Binding project context:
- New command should be `stmap0` and must keep existing `map` behavior unchanged.
- stmap0 hypothesis: align the SCL-derived genlib gain estimate with downstream buffer/sizer default by using Gain=300 instead of classic map's Gain=250 while preserving the rest of map's command behavior.
- Required correctness gates already run: `make ABC_USE_NO_READLINE=1`, `./abc -c "stmap0 -h"`, smoke flow on `benchmarks/i10.aig`, full baseline/candidate flows on all four required benchmarks, parseable final `stime` metrics, and CEC for all four benchmarks.

Please focus on:
- command registration and help text correctness
- whether `src/base/abci/abcStmap_0.c` preserves map semantics except for the intended default Gain
- build integration in `src/base/abci/module.make`
- risks to older commands or shared mapper behavior
- missing validation or logging that should block counting iteration 0

Do not treat the large required benchmark/library assets as implementation bugs unless their inclusion breaks the project pack.
