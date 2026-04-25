# stmap27 review summary

- configured prompt-form review pass 1 failed because the local Codex CLI rejects `--uncommitted` with a positional prompt; see `review_pass1.log`.
- supported configured-tool review pass 1 ran the same prompt via stdin; see `review_pass1_supported.log`.
- accepted pass-1 finding: include `stmap27` in `Map_MatchStmap13CountRisk()` so mode 28 populates the adaptive profile counters.
- fix: changed the counter guard from `fSkipFanout > 27` to `fSkipFanout > 28` in `src/map/mapper/mapperMatch.c`.
- revalidation after the accepted finding: build, help, smoke, full benchmark metrics, diagnostic parsing, CEC, and artifact check passed.
- configured prompt-form review pass 2 failed for the same local CLI argument conflict; see `review_pass2.log`.
- supported configured-tool review pass 2 ran via stdin; see `review_pass2_supported.log`.
- accepted pass-2 findings: none.
- rejected findings: none.
- open findings: none.
