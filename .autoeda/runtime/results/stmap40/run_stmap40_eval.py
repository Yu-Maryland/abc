#!/usr/bin/env python3
import csv
import json
import re
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / ".autoeda/runtime/results/stmap40"
LIB = "7nm_lvt_ff.lib"
COMMAND = "stmap40"
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
    }


def write_csv(path, rows, fields):
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({k: row.get(k, "") for k in fields})


def parse_candidate_diags(rows):
    diag = {
        "guard_stats": [],
        "near_miss_stats": [],
        "early_seed_stats": [],
        "penalty_stats": [],
        "feedback_load_stats": [],
        "scl_load_stats": [],
        "dense_pressure_stats": [],
        "dense_pressure_top": [],
        "scl_load_top": [],
        "feedback_load_top": [],
        "moderate_penalty_seed_diag": [],
        "moderate_penalty_block_diag": [],
        "strong_penalty_seed_diag": [],
        "strong_penalty_block_diag": [],
    }
    patterns = {
        "guard_stats": re.compile(r"stmap40 guard stats: exact-risk = (\d+)\s+highest = (\d+)\s+lower-mod = (\d+)\s+upper-mod = (\d+)\s+middle-slack = (\d+)\s+middle-relief = (\d+)\s+reject-slack = (\d+)\s+reject-highest = (\d+)\s+reject-arrival = (\d+)\s+reject-area = (\d+)"),
        "near_miss_stats": re.compile(r"stmap40 near-miss stats: near-miss = (\d+)"),
        "early_seed_stats": re.compile(r"stmap40 early-seed stats: early-seed = (\d+)"),
        "penalty_stats": re.compile(r"stmap40 penalty stats: moderate-penalty-seed = (\d+)\s+moderate-penalty-blocked = (\d+)\s+strong-penalty-seed = (\d+)\s+strong-penalty-blocked = (\d+)"),
        "load_stats": re.compile(r"stmap40 (feedback-load|scl-load) stats: nodes = (\d+)\s+matched = (\d+)\s+fanout-pins = (\d+)\s+over-max = (\d+)\s+avg-load-ratio = ([0-9.]+)\s+max-load-ratio = ([0-9.]+)"),
        "load_top": re.compile(r"stmap40 (feedback-load|scl-load) top: rank = (\d+)\s+node = (\d+)\s+gate = (\S+)\s+fanouts = (\d+)\s+load = ([0-9.]+)\s+max-cap = ([0-9.]+)\s+load-ratio = ([0-9.]+)"),
        "pressure_stats": re.compile(r"stmap40 dense-pressure stats: roots = (\d+)\s+consumer-fanin = (\d+)\s+consumer-fanout = (\d+)\s+pressure-entries = (\d+)\s+pressure-updates = (\d+)\s+pressure-missed = (\d+)\s+pressure-capacity = (\d+)\s+tracked-aig = (-?\d+)\s+tracked-node = (-?\d+)\s+tracked-ratio = ([0-9.]+)"),
        "pressure_top": re.compile(r"stmap40 dense-pressure: rank = (\d+)\s+aig-id = (-?\d+)\s+mapped-node = (-?\d+)\s+consumer-load-ratio = ([0-9.]+)"),
        "moderate_diag": re.compile(r"stmap40 moderate penalty (seed|block) diag: index = (\d+)\s+node = (\d+)\s+aig-id = (-?\d+)\s+level = (\d+)\s+refs = (\d+)\s+leaves = (\d+)\s+phase = (\d+)\s+slack = ([0-9.+-]+)\s+area-save = ([0-9.+-]+)\s+arrival-delta = ([0-9.+-]+)\s+arrival-gain-margin = ([0-9.+-]+)\s+penalty-factor = ([0-9.]+)\s+area-margin = ([0-9.+-]+)"),
        "strong_diag": re.compile(r"stmap40 strong penalty (seed|block) diag: index = (\d+)\s+node = (\d+)\s+aig-id = (-?\d+)\s+level = (\d+)\s+refs = (\d+)\s+leaves = (\d+)\s+phase = (\d+)\s+slack = ([0-9.+-]+)\s+area-save = ([0-9.+-]+)\s+arrival-delta = ([0-9.+-]+)\s+arrival-gain-margin = ([0-9.+-]+)\s+penalty-factor = ([0-9.]+)\s+area-margin = ([0-9.+-]+)\s+cut-leaf-load-avg = ([0-9.]+)\s+fanout-limit = (\d+)\s+load-drive-ratio = ([0-9.]+)\s+node-pressure-ratio = ([0-9.]+)\s+cut-pressure-ratio = ([0-9.]+)\s+scl-feedback = ([0-9.]+)"),
    }
    for row in rows:
        if row["kind"] != "candidate":
            continue
        bench = row["benchmark"]
        text = clean((ROOT / row["log"]).read_text())
        for match in patterns["guard_stats"].finditer(text):
            diag["guard_stats"].append(dict(zip(["benchmark", "exact_risk", "highest", "lower_mod", "upper_mod", "middle_slack", "middle_relief", "reject_slack", "reject_highest", "reject_arrival", "reject_area"], [bench] + list(match.groups()))))
        for match in patterns["near_miss_stats"].finditer(text):
            diag["near_miss_stats"].append({"benchmark": bench, "near_miss": match.group(1)})
        for match in patterns["early_seed_stats"].finditer(text):
            diag["early_seed_stats"].append({"benchmark": bench, "early_seed": match.group(1)})
        for match in patterns["penalty_stats"].finditer(text):
            diag["penalty_stats"].append(dict(zip(["benchmark", "moderate_penalty_seed", "moderate_penalty_blocked", "strong_penalty_seed", "strong_penalty_blocked"], [bench] + list(match.groups()))))
        for match in patterns["load_stats"].finditer(text):
            key = "feedback_load_stats" if match.group(1) == "feedback-load" else "scl_load_stats"
            diag[key].append(dict(zip(["benchmark", "nodes", "matched", "fanout_pins", "over_max", "avg_load_ratio", "max_load_ratio"], [bench] + list(match.groups()[1:]))))
        for match in patterns["load_top"].finditer(text):
            key = "feedback_load_top" if match.group(1) == "feedback-load" else "scl_load_top"
            diag[key].append(dict(zip(["benchmark", "rank", "node", "gate", "fanouts", "load", "max_cap", "load_ratio"], [bench] + list(match.groups()[1:]))))
        for match in patterns["pressure_stats"].finditer(text):
            diag["dense_pressure_stats"].append(dict(zip(["benchmark", "roots", "consumer_fanin", "consumer_fanout", "pressure_entries", "pressure_updates", "pressure_missed", "pressure_capacity", "tracked_aig", "tracked_node", "tracked_ratio"], [bench] + list(match.groups()))))
        for match in patterns["pressure_top"].finditer(text):
            diag["dense_pressure_top"].append(dict(zip(["benchmark", "rank", "aig_id", "mapped_node", "consumer_load_ratio"], [bench] + list(match.groups()))))
        for match in patterns["moderate_diag"].finditer(text):
            key = "moderate_penalty_seed_diag" if match.group(1) == "seed" else "moderate_penalty_block_diag"
            diag[key].append(dict(zip(["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin"], [bench] + list(match.groups()[1:]))))
        for match in patterns["strong_diag"].finditer(text):
            key = "strong_penalty_seed_diag" if match.group(1) == "seed" else "strong_penalty_block_diag"
            diag[key].append(dict(zip(["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin", "cut_leaf_load_avg", "fanout_limit", "load_drive_ratio", "node_pressure_ratio", "cut_pressure_ratio", "scl_feedback"], [bench] + list(match.groups()[1:]))))
    return diag


def run_cec(bench):
    name = safe_name(bench)
    orig = OUT / f"cec_{name}_orig.aig"
    cand = OUT / f"cec_{name}_{COMMAND}.aig"
    orig_flow = f"read_lib {LIB}; read {bench}; strash; write_aiger {orig}"
    cand_flow = f"read_lib {LIB}; read {bench}; resyn; resyn2; dch -v; {COMMAND}; topo; buffer; upsize -v; dnsize -v; stime; strash; write_aiger {cand}"
    orig_res = run_abc(f"cec_{name}_orig_write", bench, orig_flow)
    cand_res = run_abc(f"cec_{name}_candidate_write", bench, cand_flow)
    log = OUT / f"cec_{name}.log"
    exit_path = OUT / f"cec_{name}.exit"
    start = time.time()
    try:
        proc = subprocess.run(["./abc", "-c", f"cec {orig} {cand}"], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=TIMEOUT)
        rc = proc.returncode
        text = proc.stdout
    except subprocess.TimeoutExpired as exc:
        rc = 124
        text = (exc.stdout or "") + "\nTIMEOUT\n"
    log.write_text(text)
    exit_path.write_text(f"{rc}\n")
    passed = rc == 0 and "Networks are equivalent" in text
    return {
        "benchmark": bench,
        "orig_exit": orig_res["exit_code"],
        "candidate_exit": cand_res["exit_code"],
        "cec_exit": rc,
        "passed": passed,
        "runtime_seconds": round(time.time() - start, 3),
        "orig": str(orig.relative_to(ROOT)),
        "candidate": str(cand.relative_to(ROOT)),
        "log": str(log.relative_to(ROOT)),
    }


def main():
    jobs = []
    with ThreadPoolExecutor(max_workers=4) as pool:
        for bench in BENCHMARKS:
            base_flow = f"read_lib {LIB}; read {bench}; resyn; resyn2; dch -v; map; topo; buffer; upsize -v; dnsize -v; stime"
            cand_flow = f"read_lib {LIB}; read {bench}; resyn; resyn2; dch -v; {COMMAND}; topo; buffer; upsize -v; dnsize -v; stime"
            jobs.append(pool.submit(run_abc, "baseline", bench, base_flow))
            jobs.append(pool.submit(run_abc, "candidate", bench, cand_flow))
        rows = [f.result() for f in as_completed(jobs)]
    rows.sort(key=lambda r: (BENCHMARKS.index(r["benchmark"]), r["kind"]))
    write_csv(OUT / "metrics.csv", rows, ["kind", "command", "benchmark", "exit_code", "parse_ok", "stime_delay_ps", "stime_area", "runtime_seconds", "log"])

    by_key = {(r["kind"], r["benchmark"]): r for r in rows}
    comparison = []
    for bench in BENCHMARKS:
        b = by_key[("baseline", bench)]
        c = by_key[("candidate", bench)]
        delay_delta = c["stime_delay_ps"] - b["stime_delay_ps"]
        area_delta = c["stime_area"] - b["stime_area"]
        comparison.append({
            "benchmark": bench,
            "baseline_delay_ps": b["stime_delay_ps"],
            "candidate_delay_ps": c["stime_delay_ps"],
            "delay_delta_ps": delay_delta,
            "delay_delta_pct": 100.0 * delay_delta / b["stime_delay_ps"],
            "baseline_area": b["stime_area"],
            "candidate_area": c["stime_area"],
            "area_delta": area_delta,
            "area_delta_pct": 100.0 * area_delta / b["stime_area"],
        })
    write_csv(OUT / "comparison.csv", comparison, list(comparison[0].keys()))

    diag = parse_candidate_diags(rows)
    fields = {
        "guard_stats": ["benchmark", "exact_risk", "highest", "lower_mod", "upper_mod", "middle_slack", "middle_relief", "reject_slack", "reject_highest", "reject_arrival", "reject_area"],
        "near_miss_stats": ["benchmark", "near_miss"],
        "early_seed_stats": ["benchmark", "early_seed"],
        "penalty_stats": ["benchmark", "moderate_penalty_seed", "moderate_penalty_blocked", "strong_penalty_seed", "strong_penalty_blocked"],
        "feedback_load_stats": ["benchmark", "nodes", "matched", "fanout_pins", "over_max", "avg_load_ratio", "max_load_ratio"],
        "scl_load_stats": ["benchmark", "nodes", "matched", "fanout_pins", "over_max", "avg_load_ratio", "max_load_ratio"],
        "feedback_load_top": ["benchmark", "rank", "node", "gate", "fanouts", "load", "max_cap", "load_ratio"],
        "scl_load_top": ["benchmark", "rank", "node", "gate", "fanouts", "load", "max_cap", "load_ratio"],
        "dense_pressure_stats": ["benchmark", "roots", "consumer_fanin", "consumer_fanout", "pressure_entries", "pressure_updates", "pressure_missed", "pressure_capacity", "tracked_aig", "tracked_node", "tracked_ratio"],
        "dense_pressure_top": ["benchmark", "rank", "aig_id", "mapped_node", "consumer_load_ratio"],
        "moderate_penalty_seed_diag": ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin"],
        "moderate_penalty_block_diag": ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin"],
        "strong_penalty_seed_diag": ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin", "cut_leaf_load_avg", "fanout_limit", "load_drive_ratio", "node_pressure_ratio", "cut_pressure_ratio", "scl_feedback"],
        "strong_penalty_block_diag": ["benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase", "slack", "area_save", "arrival_delta", "arrival_gain_margin", "penalty_factor", "area_margin", "cut_leaf_load_avg", "fanout_limit", "load_drive_ratio", "node_pressure_ratio", "cut_pressure_ratio", "scl_feedback"],
    }
    for key, flds in fields.items():
        write_csv(OUT / f"{key}.csv", diag[key], flds)

    with ThreadPoolExecutor(max_workers=4) as pool:
        cec_rows = [f.result() for f in as_completed([pool.submit(run_cec, b) for b in BENCHMARKS])]
    cec_rows.sort(key=lambda r: BENCHMARKS.index(r["benchmark"]))
    (OUT / "cec_summary.json").write_text(json.dumps(cec_rows, indent=2) + "\n")

    summary = {
        "command": COMMAND,
        "benchmarks": BENCHMARKS,
        "metrics": rows,
        "comparison": comparison,
        "cec": cec_rows,
        "all_metrics_parseable": all(r["exit_code"] == 0 and r["parse_ok"] for r in rows),
        "all_cec_passed": all(r["passed"] for r in cec_rows),
    }
    (OUT / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    if not summary["all_metrics_parseable"] or not summary["all_cec_passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
