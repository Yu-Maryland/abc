Review the uncommitted stmap65 AutoEDA iteration for correctness risks.

Project constraints:
- Binding pack requires numbered stmapN variants, preserving all previous commands.
- New command is stmap65.
- Baseline/candidate flow uses read_lib 7nm_lvt_ff.lib; read design; resyn; resyn2; dch -v; mapper; topo; buffer; upsize -v; dnsize -v; stime.
- Correctness gates include build, help, smoke, all four benchmark metrics, CEC, runtime artifacts, and version logging.

Implementation hypothesis:
- stmap65 keeps the stmap64/stmap63 two-pass bounded-pressure mapper flow and mapper mode 57.
- It does not widen cut-only admission.
- It tightens the command-scoped load-drop cut-only guard so one-sided cut-pressure candidates are admitted only when node-side bounded pressure is at least 1.25 and at most 1.55, cut pressure is 1.95 to 2.20, feedback severity/pressure entry gates pass, arrival gain is strong, and slack passes.

Review focus:
- Check that stmap65 is registered and build-wired without breaking stmap0 through stmap64.
- Check that the new mapper flag is scoped to stmap65 and reset after the final remap.
- Check that stmap63/stmap64 behavior remains unchanged.
- Check that the strong-node guard cannot accidentally open the broader stmap63 guard or bypass the inherited area cap.
- Check for memory/lifetime hazards in bounded pressure handoff and cleanup.
- Check the runtime evaluation artifacts and parser for stmap65 consistency.

Validation already run:
- make clean; make ABC_USE_NO_READLINE=1 passed after the initial stale-readline object failure.
- ./abc -c "stmap65 -h" passed.
- smoke flow on benchmarks/i10.aig passed with final stime delay 198.64 ps and area 1303.10.
- Full benchmark metrics and CEC passed for benchmarks/i10.aig, benchmarks/ode.abc.blif, benchmarks/or1200.abc.blif, and benchmarks/syn2.abc.blif.

Please report actionable findings only. For each finding include severity, file/line, and the concrete failure mode.
