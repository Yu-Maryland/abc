#!/usr/bin/env python3
import concurrent.futures
import csv
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
RESULTS = ROOT / ".autoeda" / "runtime" / "results" / "stmap52"
ABC = ROOT / "abc"
LIB = "7nm_lvt_ff.lib"
COMMAND = "stmap52"
BENCHMARKS = [
    "benchmarks/i10.aig",
    "benchmarks/ode.abc.blif",
    "benchmarks/or1200.abc.blif",
    "benchmarks/syn2.abc.blif",
]
TIMEOUT = 600
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
STIME_RE = re.compile(r'WireLoad\s*=\s*"[^"]*".*Area\s*=\s*([0-9]+(?:\.[0-9]+)?).*Delay\s*=\s*([0-9]+(?:\.[0-9]+)?)\s*ps')
STAT_PATTERNS = {
    "guard_stats": re.compile(
        r"stmap52 guard stats: exact-risk = (?P<exact_risk>\d+)  highest = (?P<highest>\d+)  lower-mod = (?P<lower_mod>\d+)  upper-mod = (?P<upper_mod>\d+)  middle-slack = (?P<middle_slack>\d+)  middle-relief = (?P<middle_relief>\d+)  reject-slack = (?P<reject_slack>\d+)  reject-highest = (?P<reject_highest>\d+)  reject-arrival = (?P<reject_arrival>\d+)  reject-area = (?P<reject_area>\d+)"
    ),
    "near_miss_stats": re.compile(r"stmap52 near-miss stats: near-miss = (?P<near_miss>\d+)"),
    "early_seed_stats": re.compile(r"stmap52 early-seed stats: early-seed = (?P<early_seed>\d+)"),
    "penalty_stats": re.compile(
        r"stmap52 penalty stats: moderate-penalty-seed = (?P<moderate_penalty_seed>\d+)  moderate-penalty-blocked = (?P<moderate_penalty_blocked>\d+)  strong-penalty-seed = (?P<strong_penalty_seed>\d+)  strong-penalty-blocked = (?P<strong_penalty_blocked>\d+)"
    ),
    "sink_pressure_stats": re.compile(
        r"stmap52 sink-pressure stats: roots = (?P<roots>\d+)  consumer-fanin = (?P<consumer_fanin>\d+)  consumer-fanout = (?P<consumer_fanout>\d+)  raw-pressure-entries = (?P<raw_pressure_entries>\d+)  raw-pressure-updates = (?P<raw_pressure_updates>\d+)  sink-pressure-entries = (?P<sink_pressure_entries>\d+)  sink-pressure-updates = (?P<sink_pressure_updates>\d+)  pressure-missed = (?P<pressure_missed>\d+)  pressure-capacity = (?P<pressure_capacity>\d+)  tracked-aig = (?P<tracked_aig>\d+)  tracked-node = (?P<tracked_node>-?\d+)  tracked-raw-ratio = (?P<tracked_raw_ratio>[0-9]+(?:\.[0-9]+)?)  tracked-sink-ratio = (?P<tracked_sink_ratio>[0-9]+(?:\.[0-9]+)?)"
    ),
    "feedback_stats": re.compile(
        r"stmap52 feedback: remap = 1  active = (?P<active>\d+)  max-load-ratio = (?P<max_load_ratio>[0-9]+(?:\.[0-9]+)?)  over-max-frac = (?P<over_max_frac>[0-9]+(?:\.[0-9]+)?)  severity = (?P<severity>[0-9]+(?:\.[0-9]+)?)  raw-pressure-entries = (?P<raw_pressure_entries>\d+)  sink-pressure-entries = (?P<sink_pressure_entries>\d+)  pressure-capacity = (?P<pressure_capacity>\d+)"
    ),
    "blended_gain_stats": re.compile(
        r"stmap52 blended-gain: base-gain = (?P<base_gain>[0-9]+(?:\.[0-9]+)?)  blend-gain = (?P<blend_gain>[0-9]+(?:\.[0-9]+)?)  selected-gain = (?P<selected_gain>[0-9]+(?:\.[0-9]+)?)  severity-threshold = (?P<severity_threshold>[0-9]+(?:\.[0-9]+)?)  sink-pressure-threshold = (?P<sink_pressure_threshold>\d+)  selected-blend-gain = (?P<selected_blend_gain>\d+)"
    ),
    "mapper_mode_stats": re.compile(
        r"stmap52 mapper-mode: local-area-cap-mode = (?P<local_area_cap_mode>\d+)  blended-gain = (?P<blended_gain>[0-9]+(?:\.[0-9]+)?)"
    ),
}


def safe_name(bench):
    return bench.replace("/", "_").replace(".", "_")


def clean(text):
    return ANSI_RE.sub("", text)


def run_abc(command, log_path, exit_path):
    proc = subprocess.run(
        [str(ABC), "-c", command],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=TIMEOUT,
    )
    log_path.write_text(proc.stdout)
    exit_path.write_text(f"{proc.returncode}\n")
    return proc.returncode, proc.stdout


def parse_stime(text):
    matches = STIME_RE.findall(clean(text))
    if not matches:
        return None
    area, delay = matches[-1]
    return {"stime_area": float(area), "stime_delay_ps": float(delay)}


def parse_stats(text):
    out = {}
    plain = clean(text)
    for name, pattern in STAT_PATTERNS.items():
        matches = list(pattern.finditer(plain))
        if not matches:
            continue
        out[name] = matches[-1].groupdict()
    return out


def run_flow(kind, bench):
    safe = safe_name(bench)
    if kind == "baseline":
        flow = f"read_lib {LIB}; read {bench}; resyn; resyn2; dch -v; map; topo; buffer; upsize -v; dnsize -v; stime"
    else:
        flow = f"read_lib {LIB}; read {bench}; resyn; resyn2; dch -v; {COMMAND}; topo; buffer; upsize -v; dnsize -v; stime"
    log = RESULTS / f"{kind}_{safe}.log"
    exit_file = RESULTS / f"{kind}_{safe}.exit"
    rc, text = run_abc(flow, log, exit_file)
    metrics = parse_stime(text)
    return {
        "benchmark": bench,
        "kind": kind,
        "returncode": rc,
        "metrics": metrics,
        "stats": parse_stats(text) if kind == "candidate" else {},
        "log": str(log.relative_to(ROOT)),
        "command": f"./abc -c \"{flow}\"",
    }


def write_aig(bench, suffix, flow):
    safe = safe_name(bench)
    aig = RESULTS / f"cec_{safe}_{suffix}.aig"
    log = RESULTS / f"cec_{safe}_{suffix}_write.log"
    exit_file = RESULTS / f"cec_{safe}_{suffix}_write.exit"
    rc, _ = run_abc(f"{flow}; strash; write_aiger {aig}", log, exit_file)
    return rc, aig


def run_cec(bench):
    safe = safe_name(bench)
    orig_flow = f"read_lib {LIB}; read {bench}"
    cand_flow = f"read_lib {LIB}; read {bench}; resyn; resyn2; dch -v; {COMMAND}; topo; buffer; upsize -v; dnsize -v; stime"
    rc_orig, orig = write_aig(bench, "orig", orig_flow)
    rc_cand, cand = write_aig(bench, COMMAND, cand_flow)
    log = RESULTS / f"cec_{safe}.log"
    exit_file = RESULTS / f"cec_{safe}.exit"
    if rc_orig != 0 or rc_cand != 0:
        log.write_text(f"write failed: orig={rc_orig} candidate={rc_cand}\n")
        exit_file.write_text("1\n")
        return {"benchmark": bench, "passed": False, "returncode": 1, "log": str(log.relative_to(ROOT))}
    rc, text = run_abc(f"cec {orig} {cand}", log, exit_file)
    passed = rc == 0 and ("Networks are equivalent" in text or "Networks are equivalent after structural hashing" in text)
    return {
        "benchmark": bench,
        "passed": passed,
        "returncode": rc,
        "original": str(orig.relative_to(ROOT)),
        "candidate": str(cand.relative_to(ROOT)),
        "log": str(log.relative_to(ROOT)),
    }


def write_csv(path, rows, fields):
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def main():
    RESULTS.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        futures = [pool.submit(run_flow, kind, bench) for bench in BENCHMARKS for kind in ("baseline", "candidate")]
        flow_results = [f.result() for f in concurrent.futures.as_completed(futures)]

    by_key = {(r["benchmark"], r["kind"]): r for r in flow_results}
    metrics_rows = []
    comparison_rows = []
    stats_by_name = {name: [] for name in STAT_PATTERNS}
    for bench in BENCHMARKS:
        base = by_key[(bench, "baseline")]
        cand = by_key[(bench, "candidate")]
        if base["returncode"] != 0 or cand["returncode"] != 0 or base["metrics"] is None or cand["metrics"] is None:
            raise SystemExit(f"unparseable or failed flow for {bench}: base={base['returncode']} cand={cand['returncode']}")
        for row in (base, cand):
            metrics_rows.append({
                "benchmark": bench,
                "flow": row["kind"],
                "stime_delay_ps": row["metrics"]["stime_delay_ps"],
                "stime_area": row["metrics"]["stime_area"],
                "log": row["log"],
            })
        b = base["metrics"]
        c = cand["metrics"]
        comparison_rows.append({
            "benchmark": bench,
            "baseline_delay_ps": b["stime_delay_ps"],
            "candidate_delay_ps": c["stime_delay_ps"],
            "delay_delta_ps": c["stime_delay_ps"] - b["stime_delay_ps"],
            "delay_delta_pct": 100.0 * (c["stime_delay_ps"] - b["stime_delay_ps"]) / b["stime_delay_ps"],
            "baseline_area": b["stime_area"],
            "candidate_area": c["stime_area"],
            "area_delta": c["stime_area"] - b["stime_area"],
            "area_delta_pct": 100.0 * (c["stime_area"] - b["stime_area"]) / b["stime_area"],
        })
        for name, parsed in cand["stats"].items():
            row = {"benchmark": bench}
            row.update(parsed)
            stats_by_name[name].append(row)

    write_csv(RESULTS / "metrics.csv", metrics_rows, ["benchmark", "flow", "stime_delay_ps", "stime_area", "log"])
    write_csv(
        RESULTS / "comparison.csv",
        comparison_rows,
        ["benchmark", "baseline_delay_ps", "candidate_delay_ps", "delay_delta_ps", "delay_delta_pct", "baseline_area", "candidate_area", "area_delta", "area_delta_pct"],
    )
    for name, rows in stats_by_name.items():
        if rows:
            fields = ["benchmark"] + [k for k in rows[0].keys() if k != "benchmark"]
            write_csv(RESULTS / f"{name}.csv", rows, fields)

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        cec_results = list(pool.map(run_cec, BENCHMARKS))
    (RESULTS / "cec_summary.json").write_text(json.dumps(cec_results, indent=2) + "\n")
    if not all(r["passed"] for r in cec_results):
        raise SystemExit("CEC failed")

    summary = {
        "command": COMMAND,
        "library": LIB,
        "benchmarks": BENCHMARKS,
        "flow_results": flow_results,
        "comparison": comparison_rows,
        "cec": cec_results,
    }
    (RESULTS / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")


if __name__ == "__main__":
    main()
