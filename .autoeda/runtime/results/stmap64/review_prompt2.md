Review the updated uncommitted stmap64 iteration after the accepted pass-1 fix.

Pass-1 finding accepted and addressed:
- The original final-witness diagnostic matched downstream nodes only through vOrigNodeIds, but SCL topo/buffer style DFS duplication can drop that metadata.
- The fix adds origin-ID propagation to Abc_NtkDup, Abc_NtkDupDfs, and Abc_NtkDupDfsNoBarBufs through a shared helper in src/base/abc/abcNtk.c.
- The stmap64 policy remains unchanged. The source change is intended to preserve metadata only, so downstream stime witness matching is accurate.

Revalidation after the fix:
- make ABC_USE_NO_READLINE=1 passed.
- stmap64 -h passed.
- i10 smoke passed.
- targeted syn2 downstream flow now reports the admitted witness as matched: final node 11282, final load ratio 0.039, final criticality 0.272.
- full required benchmark evaluation and CEC were rerun after the fix and passed.

Please focus on:
- whether the Abc_NtkDupOrigNodeIds helper is safe for Abc_NtkDup, Abc_NtkDupDfs, and Abc_NtkDupDfsNoBarBufs
- whether origin-ID copying could corrupt networks with remapped or merged bar-buffer nodes
- whether the stmap64 witness lifecycle still avoids leaking diagnostics into other commands
- whether any remaining review pass-1 risk is unresolved
- any correctness or build integration issue that would block counting stmap64
