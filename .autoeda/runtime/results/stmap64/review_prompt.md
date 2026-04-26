Review the uncommitted stmap64 AutoEDA iteration for correctness risks.

Context:
- Project pack requires one numbered stmap implementation per successful iteration.
- stmap64 should preserve classic map and stmap0 through stmap63 behavior.
- stmap64 intentionally keeps the stmap63 load-drop cut-only mapping policy unchanged.
- The new behavior is diagnostic: record command-scoped accepted cut-only load-drop seeds during the stmap64 final remap, then print a gated final-witness report from the downstream stime call after topo, buffer, upsize, and dnsize.
- The shared sclSize.c stime hook must be a no-op unless stmap64 has recorded witnesses.

Files to review:
- src/base/abci/abcStmap_64.c
- src/base/abci/abc.c
- src/base/abci/module.make
- src/map/mapper/mapperMatch.c
- src/map/scl/sclSize.c
- abclib.dsp
- .autoeda/runtime/results/stmap64/*
- .autoeda/runtime/campaign_state.json

Validation already run:
- make ABC_USE_NO_READLINE=1
- ./abc -c "stmap64 -h"
- smoke: read_lib 7nm_lvt_ff.lib; read benchmarks/i10.aig; resyn; resyn2; dch -v; stmap64; topo; buffer; upsize -v; dnsize -v; stime
- full baseline and candidate evaluation on benchmarks/i10.aig, benchmarks/ode.abc.blif, benchmarks/or1200.abc.blif, and benchmarks/syn2.abc.blif
- CEC passed on all four benchmark designs using strashed AIG temporaries

Focus on:
- command registration/build integration
- whether the mapper witness recorder leaks state into other commands
- whether the shared stime hook can affect non-stmap64 flows
- memory lifetime and reset behavior around pBoundedPressureRatios and witness state
- whether the final-witness diagnostic is parseable and does not disturb final stime metric parsing
- whether any older stmap behavior is changed unintentionally
