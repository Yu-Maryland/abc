# stmap73 review summary

- configured positional prompt form: attempted for passes 1, 2, and 3; local Codex CLI rejected the positional prompt with `--uncommitted`, matching the known campaign CLI limitation. Logs: `review_pass1_positional.log`, `review_pass2_positional.log`, `review_pass3_positional.log`.
- supported configured-tool stdin review: completed for passes 1, 2, and 3 with exit code 0.
- accepted source finding from pass 1: stored/replaced accepted-pressure witnesses could lose source provenance after the first 64 diagnostic source lines. Fix: `Map_Stmap73RecordAcceptedPressureWitness()` now prints every stored or replaced source row in addition to the first 64 candidates. Revalidated with build, help, smoke, full benchmark metrics, accepted-pressure parsing, final-witness parsing, and CEC.
- accepted source finding from pass 2: accepted-pressure witnesses could be recorded without a real stmap56 pressure class. Fix: the recording gate now requires pressure agreement, pressure-near, cut-only pressure, moderate penalty, or strong penalty before recording. Revalidated with build, help, smoke, full benchmark metrics, accepted-pressure parsing, final-witness parsing, and CEC.
- accepted metadata finding from pass 3: finalize the canonical version log and campaign state before counting the iteration. Addressed by this version entry and finalized state update.
- rejected findings: none.
- open findings: none after metadata finalization.
