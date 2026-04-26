#!/usr/bin/env python3
import csv
import json
import re
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / ".autoeda/runtime/results/stmap46"
LIB = "7nm_lvt_ff.lib"
COMMAND = "stmap46"
BENCHMARKS = [
    "benchmarks/i10.aig",
    "benchmarks/ode.abc.blif",
    "benchmarks/or1200.abc.blif",
    "benchmarks/syn2.abc.blif",
]
TIMEOUT = 600
ANSI = re.compile(r"\x1b\[[0-9;]*m")
STIME = re.compile(r'WireLoad\s*=\s*"[^"]*".*Area\s*=\s*([0-9]+(?:\.[0-9]+)?).*Delay\s*=\s*([0-9]+(?:\.[0-9]+)?)\s*ps')


def safe_name(bench):
    return bench.replace("/", "_").replace(".", "_")


def clean(text):
    return ANSI.sub("", text)


def parse_stime(text):
    matches = STIME.findall(clean(text))
    if not matches:
        return None
    area, delay = matches[-1]
    return float(delay), float(area)


def run_abc(label, bench, flow):
    name = safe_name(bench)
    log = OUT / f"{label}_{name}.log"
    exit_path = OUT / f"{label}_{name}.exit"
    start = time.time()
    try:
        proc = subprocess.run(
            ["./abc", "-c", flow],
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=TIMEOUT,
        )
        rc = proc.returncode
        text = proc.stdout
    except subprocess.TimeoutExpired as exc:
        rc = 124
        text = (exc.stdout or "") + "\nTIMEOUT\n"
    elapsed = time.time() - start
    log.write_text(text)
    exit_path.write_text(f"{rc}\n")
    metrics = parse_stime(text)
    return {
        "kind": label,
        "command": "map" if label == "baseline" else COMMAND,
        "benchmark": bench,
        "exit_code": rc,
        "parse_ok": metrics is not None,
        "stime_delay_ps": metrics[0] if metrics else None,
        "stime_area": metrics[1] if metrics else None,
        "runtime_seconds": round(elapsed, 3),
        "log": str(log.relative_to(ROOT)),
        "abc_command": f'./abc -c "{flow}"',
    }


def write_csv(path, rows, fields):
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})


def parse_candidate_diags(rows):
    diag = {
        "guard_stats": [],
        "near_miss_stats": [],
        "early_seed_stats": [],
        "penalty_stats": [],
        "feedback_stats": [],
        "sink_pressure_stats": [],
        "moderate_penalty_block_diag": [],
        "moderate_penalty_seed_diag": [],
        "tight_exception_block_diag": [],
        "tight_exception_seed_diag": [],
        "strong_penalty_block_diag": [],
        "strong_penalty_seed_diag": [],
    }
    patterns = {
        "guard": re.compile(r"stmap46 guard stats: exact-risk = (\d+)\s+highest = (\d+)\s+lower-mod = (\d+)\s+upper-mod = (\d+)\s+middle-slack = (\d+)\s+middle-relief = (\d+)\s+reject-slack = (\d+)\s+reject-highest = (\d+)\s+reject-arrival = (\d+)\s+reject-area = (\d+)"),
        "near": re.compile(r"stmap46 near-miss stats: near-miss = (\d+)"),
        "early": re.compile(r"stmap46 early-seed stats: early-seed = (\d+)"),
        "penalty": re.compile(r"stmap46 penalty stats: moderate-penalty-seed = (\d+)\s+moderate-penalty-blocked = (\d+)\s+tight-exception-seed = (\d+)\s+tight-exception-blocked = (\d+)\s+strong-penalty-seed = (\d+)\s+strong-penalty-blocked = (\d+)"),
        "feedback": re.compile(r"stmap46 feedback: remap = 1\s+active = (\d+)\s+max-load-ratio = ([0-9.]+)\s+over-max-frac = ([0-9.]+)\s+severity = ([0-9.]+)\s+raw-pressure-entries = (\d+)\s+sink-pressure-entries = (\d+)\s+pressure-capacity = (\d+)"),
        "pressure": re.compile(r"stmap46 sink-pressure stats: roots = (\d+)\s+consumer-fanin = (\d+)\s+consumer-fanout = (\d+)\s+raw-pressure-entries = (\d+)\s+raw-pressure-updates = (\d+)\s+sink-pressure-entries = (\d+)\s+sink-pressure-updates = (\d+)\s+pressure-missed = (\d+)\s+pressure-capacity = (\d+)\s+tracked-aig = (-?\d+)\s+tracked-node = (-?\d+)\s+tracked-raw-ratio = ([0-9.]+)\s+tracked-sink-ratio = ([0-9.]+)"),
        "moderate": re.compile(r"stmap46 moderate penalty (seed|block) diag: index = (\d+)\s+node = (\d+)\s+aig-id = (-?\d+)\s+level = (\d+)\s+refs = (\d+)\s+leaves = (\d+)\s+phase = (\d+)\s+slack = ([0-9.+-]+)\s+area-save = ([0-9.+-]+)\s+arrival-delta = ([0-9.+-]+)\s+arrival-gain-margin = ([0-9.+-]+)\s+penalty-factor = ([0-9.]+)\s+area-margin = ([0-9.+-]+)\s+node-sink-pressure-ratio = ([0-9.]+)\s+cut-sink-pressure-ratio = ([0-9.]+)\s+scl-feedback = ([0-9.]+)"),
        "tight": re.compile(r"stmap46 tight exception (seed|block) diag: index = (\d+)\s+node = (\d+)\s+aig-id = (-?\d+)\s+level = (\d+)\s+refs = (\d+)\s+leaves = (\d+)\s+phase = (\d+)\s+slack = ([0-9.+-]+)\s+area-save = ([0-9.+-]+)\s+arrival-delta = ([0-9.+-]+)\s+arrival-gain-margin = ([0-9.+-]+)\s+area-margin = ([0-9.+-]+)\s+node-sink-pressure-ratio = ([0-9.]+)\s+cut-sink-pressure-ratio = ([0-9.]+)\s+scl-feedback = ([0-9.]+)"),
        "strong": re.compile(r"stmap46 strong penalty (seed|block) diag: index = (\d+)\s+node = (\d+)\s+aig-id = (-?\d+)\s+level = (\d+)\s+refs = (\d+)\s+leaves = (\d+)\s+phase = (\d+)\s+slack = ([0-9.+-]+)\s+area-save = ([0-9.+-]+)\s+arrival-delta = ([0-9.+-]+)\s+arrival-gain-margin = ([0-9.+-]+)\s+penalty-factor = ([0-9.]+)\s+area-margin = ([0-9.+-]+)\s+cut-leaf-load-avg = ([0-9.]+)\s+fanout-limit = (\d+)\s+load-drive-ratio = ([0-9.]+)\s+node-sink-pressure-ratio = ([0-9.]+)\s+cut-sink-pressure-ratio = ([0-9.]+)\s+scl-feedback = ([0-9.]+)"),
    }
    for row in rows:
        if row["kind"] != "candidate":
            continue
        bench = row["benchmark"]
        text = clean((ROOT / row["log"]).read_text())
        for match in patterns["guard"].finditer(text):
            fields = ["benchmark", "exact_risk", "highest", "lower_mod", "upper_mod", "middle_slack", "middle_relief", "reject_slack", "reject_highest", "reject_arrival", "reject_area"]
            diag["guard_stats"].append(dict(zip(fields, [bench] + list(match.groups()))))
        for match in patterns["near"].finditer(text):
            diag["near_miss_stats"].append({"benchmark": bench, "near_miss": match.group(1)})
        for match in patterns["early"].finditer(text):
            diag["early_seed_stats"].append({"benchmark": bench, "early_seed": match.group(1)})
        for match in patterns["penalty"].finditer(text):
            fields = ["benchmark", "moderate_penalty_seed", "moderate_penalty_blocked", "tight_exception_seed", "tight_exception_blocked", "strong_penalty_seed", "strong_penalty_blocked"]
            diag["penalty_stats"].append(dict(zip(fields, [bench] + list(match.groups()))))
        for match in patterns["feedback"].finditer(text):
            fields = ["benchmark", "active", "max_load_ratio", "over_max_frac", "severity", "raw_pressure_entries", "sink_pressure_entries", "pressure_capacity"]
            diag["feedback_stats"].append(dict(zip(fields, [bench] + list(match.groups()))))
        for match in patterns["pressure"].finditer(text):
            fields = ["benchmark", "roots", "consumer_fanin", "consumer_fanout", "raw_pressure_entries", "raw_pressure_updates", "sink_pressure_entries", "sink_pressure_updates", "pressure_missed", "pressure_capacity", "tracked_aig", "tracked_node", "tracked_raw_ratio", "tracked_sink_ratio"]
            diag["sink_pressure_stats"].append(dict(zip(fields, [bench] + list(match.groups()))))
        for match in patterns["moderate"].finditer(text):
            key = "moderate_penalty_seed_diag" if match.group(1) == "seed" else "moderate_penalty_block_diag"
            fields = ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin", "node_sink_pressure_ratio", "cut_sink_pressure_ratio", "scl_feedback"]
            diag[key].append(dict(zip(fields, [bench] + list(match.groups()[1:]))))
        for match in patterns["tight"].finditer(text):
            key = "tight_exception_seed_diag" if match.group(1) == "seed" else "tight_exception_block_diag"
            fields = ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "area_margin", "node_sink_pressure_ratio", "cut_sink_pressure_ratio", "scl_feedback"]
            diag[key].append(dict(zip(fields, [bench] + list(match.groups()[1:]))))
        for match in patterns["strong"].finditer(text):
            key = "strong_penalty_seed_diag" if match.group(1) == "seed" else "strong_penalty_block_diag"
            fields = ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin", "cut_leaf_load_avg", "fanout_limit", "load_drive_ratio", "node_sink_pressure_ratio", "cut_sink_pressure_ratio", "scl_feedback"]
            diag[key].append(dict(zip(fields, [bench] + list(match.groups()[1:]))))
    return diag


def run_cec(bench):
    name = safe_name(bench)
    orig = OUT / f"cec_{name}_orig.aig"
    cand = OUT / f"cec_{name}_{COMMAND}.aig"
    orig_flow = f"read_lib {LIB}; read {bench}; strash; write_aiger {orig}"
    cand_flow = f"read_lib {LIB}; read {bench}; resyn; resyn2; dch -v; {COMMAND}; topo; buffer; upsize -v; dnsize -v; stime; strash; write_aiger {cand}"
    orig_res = run_abc(f"cec_{name}_orig_write", bench, orig_flow)
    cand_res = run_abc(f"cec_{name}_{COMMAND}_write", bench, cand_flow)
    log = OUT / f"cec_{name}.log"
    exit_path = OUT / f"cec_{name}.exit"
    try:
        proc = subprocess.run(
            ["./abc", "-c", f"cec {orig} {cand}"],
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=TIMEOUT,
        )
        rc = proc.returncode
        text = proc.stdout
    except subprocess.TimeoutExpired as exc:
        rc = 124
        text = (exc.stdout or "") + "\nTIMEOUT\n"
    log.write_text(text)
    exit_path.write_text(f"{rc}\n")
    return {
        "benchmark": bench,
        "passed": rc == 0 and "Networks are equivalent" in text,
        "returncode": rc,
        "original_write_exit": orig_res["exit_code"],
        "candidate_write_exit": cand_res["exit_code"],
        "original": str(orig.relative_to(ROOT)),
        "candidate": str(cand.relative_to(ROOT)),
        "log": str(log.relative_to(ROOT)),
    }


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    jobs = []
    with ThreadPoolExecutor(max_workers=4) as pool:
        for bench in BENCHMARKS:
            baseline = f"read_lib {LIB}; read {bench}; resyn; resyn2; dch -v; map; topo; buffer; upsize -v; dnsize -v; stime"
            candidate = f"read_lib {LIB}; read {bench}; resyn; resyn2; dch -v; {COMMAND}; topo; buffer; upsize -v; dnsize -v; stime"
            jobs.append(pool.submit(run_abc, "baseline", bench, baseline))
            jobs.append(pool.submit(run_abc, "candidate", bench, candidate))
        rows = [job.result() for job in as_completed(jobs)]
    rows.sort(key=lambda row: (BENCHMARKS.index(row["benchmark"]), row["kind"]))
    write_csv(OUT / "metrics.csv", rows, ["kind", "command", "benchmark", "exit_code", "parse_ok", "stime_delay_ps", "stime_area", "runtime_seconds", "log", "abc_command"])

    by_key = {(row["kind"], row["benchmark"]): row for row in rows}
    comparison = []
    for bench in BENCHMARKS:
        base = by_key[("baseline", bench)]
        cand = by_key[("candidate", bench)]
        delay_delta = cand["stime_delay_ps"] - base["stime_delay_ps"]
        area_delta = cand["stime_area"] - base["stime_area"]
        comparison.append({
            "benchmark": bench,
            "baseline_delay_ps": base["stime_delay_ps"],
            "candidate_delay_ps": cand["stime_delay_ps"],
            "delay_delta_ps": delay_delta,
            "delay_delta_pct": 100.0 * delay_delta / base["stime_delay_ps"],
            "baseline_area": base["stime_area"],
            "candidate_area": cand["stime_area"],
            "area_delta": area_delta,
            "area_delta_pct": 100.0 * area_delta / base["stime_area"],
        })
    write_csv(OUT / "comparison.csv", comparison, list(comparison[0].keys()))

    diag = parse_candidate_diags(rows)
    diag_fields = {
        "guard_stats": ["benchmark", "exact_risk", "highest", "lower_mod", "upper_mod", "middle_slack", "middle_relief", "reject_slack", "reject_highest", "reject_arrival", "reject_area"],
        "near_miss_stats": ["benchmark", "near_miss"],
        "early_seed_stats": ["benchmark", "early_seed"],
        "penalty_stats": ["benchmark", "moderate_penalty_seed", "moderate_penalty_blocked", "tight_exception_seed", "tight_exception_blocked", "strong_penalty_seed", "strong_penalty_blocked"],
        "feedback_stats": ["benchmark", "active", "max_load_ratio", "over_max_frac", "severity", "raw_pressure_entries", "sink_pressure_entries", "pressure_capacity"],
        "sink_pressure_stats": ["benchmark", "roots", "consumer_fanin", "consumer_fanout", "raw_pressure_entries", "raw_pressure_updates", "sink_pressure_entries", "sink_pressure_updates", "pressure_missed", "pressure_capacity", "tracked_aig", "tracked_node", "tracked_raw_ratio", "tracked_sink_ratio"],
        "moderate_penalty_seed_diag": ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin", "node_sink_pressure_ratio", "cut_sink_pressure_ratio", "scl_feedback"],
        "moderate_penalty_block_diag": ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin", "node_sink_pressure_ratio", "cut_sink_pressure_ratio", "scl_feedback"],
        "tight_exception_seed_diag": ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "area_margin", "node_sink_pressure_ratio", "cut_sink_pressure_ratio", "scl_feedback"],
        "tight_exception_block_diag": ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "area_margin", "node_sink_pressure_ratio", "cut_sink_pressure_ratio", "scl_feedback"],
        "strong_penalty_seed_diag": ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin", "cut_leaf_load_avg", "fanout_limit", "load_drive_ratio", "node_sink_pressure_ratio", "cut_sink_pressure_ratio", "scl_feedback"],
        "strong_penalty_block_diag": ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin", "cut_leaf_load_avg", "fanout_limit", "load_drive_ratio", "node_sink_pressure_ratio", "cut_sink_pressure_ratio", "scl_feedback"],
    }
    for key, fields in diag_fields.items():
        write_csv(OUT / f"{key}.csv", diag[key], fields)

    with ThreadPoolExecutor(max_workers=4) as pool:
        cec = [job.result() for job in as_completed([pool.submit(run_cec, bench) for bench in BENCHMARKS])]
    cec.sort(key=lambda row: BENCHMARKS.index(row["benchmark"]))
    (OUT / "cec_summary.json").write_text(json.dumps(cec, indent=2) + "\n")

    complete = all(row["exit_code"] == 0 and row["parse_ok"] for row in rows)
    complete = complete and all(row["passed"] and row["original_write_exit"] == 0 and row["candidate_write_exit"] == 0 for row in cec)
    summary = {
        "command": COMMAND,
        "library": LIB,
        "benchmarks": BENCHMARKS,
        "flow_results": rows,
        "comparison": comparison,
        "cec": cec,
        "diagnostic_counts": {key: len(value) for key, value in diag.items()},
        "complete": complete,
    }
    (OUT / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    return 0 if complete else 1


if __name__ == "__main__":
    raise SystemExit(main())
