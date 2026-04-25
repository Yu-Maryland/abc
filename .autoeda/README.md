# abc-harness-stmap-advanced

This workspace was scaffolded by `AutoEDA`.

## Layout

- `project_pack/`: user-provided mission, policy, skills, evaluation, git, and supervisor config
- `runtime/`: campaign state, canonical version log, runtime logs, and results

## Reviewed Setup

The project pack is bound to `project_pack/agent.md`, which is an exact copy of
the repo `AGENT.md` policy. The configured campaign family is `stmap`, with one
canary iteration and 100 full-campaign successful iterations.

Required assets:

- `7nm_lvt_ff.lib`
- `benchmarks/i10.aig`
- `benchmarks/ode.abc.blif`
- `benchmarks/or1200.abc.blif`
- `benchmarks/syn2.abc.blif`

Useful commands from the repo root:

```sh
python3 -m autoeda validate-pack --workspace .autoeda
python3 -m autoeda preflight --workspace .autoeda
python3 -m autoeda launch --workspace .autoeda --canary
```

Git pushes are disabled by default in `project_pack/git.yaml`; local commits
remain allowed for successful iterations.
