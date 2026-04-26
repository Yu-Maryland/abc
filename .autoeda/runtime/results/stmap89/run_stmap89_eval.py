#!/usr/bin/env python3
import concurrent.futures
import csv
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
RESULTS = ROOT / ".autoeda" / "runtime" / "results" / "stmap89"
ABC = ROOT / "abc"
LIB = "7nm_lvt_ff.lib"
COMMAND = "stmap89"
BENCHMARKS = [
    "benchmarks/i10.aig",
    "benchmarks/ode.abc.blif",
    "benchmarks/or1200.abc.blif",
    "benchmarks/syn2.abc.blif",
]
TIMEOUT = 600
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
STIME_RE = re.compile(
    r'WireLoad\s*=\s*"[^"]*".*Area\s*=\s*([0-9]+(?:\.[0-9]+)?).*Delay\s*=\s*([0-9]+(?:\.[0-9]+)?)\s*ps'
)
STMAP_LINE_RE = re.compile(r"^stmap89 ([^:]+):\s*(.*)$")
KV_RE = re.compile(r"([A-Za-z0-9_-]+) = (.*?)(?=  [A-Za-z0-9_-]+ = |$)")


def safe_name(path):
    return path.replace("/", "_").replace(".", "_")


def clean(text):
    return ANSI_RE.sub("", text)


def key_name(text):
    return text.strip().replace("-", "_").replace(" ", "_")


def run_abc(flow, log, exit_file):
    cmd = [str(ABC), "-c", flow]
    try:
        proc = subprocess.run(
            cmd,
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=TIMEOUT,
        )
        text = proc.stdout
        rc = proc.returncode
    except subprocess.TimeoutExpired as exc:
        text = (exc.stdout or "") + f"\nTIMEOUT after {TIMEOUT}s\n"
        rc = 124
    log.write_text(text)
    exit_file.write_text(f"{rc}\n")
    return rc, text


def parse_stime(text):
    matches = list(STIME_RE.finditer(clean(text)))
    if not matches:
        return {}
    m = matches[-1]
    return {"stime_area": float(m.group(1)), "stime_delay_ps": float(m.group(2))}


def parse_stmap89_rows(bench, text):
    rows_by_kind = {}
    for line in clean(text).splitlines():
        m = STMAP_LINE_RE.match(line)
        if not m:
            continue
        kind = key_name(m.group(1))
        row = {"benchmark": bench}
        for key, value in KV_RE.findall(m.group(2)):
            row[key_name(key)] = value.strip()
        rows_by_kind.setdefault(kind, []).append(row)
    return rows_by_kind


def run_flow(kind, bench):
    safe = safe_name(bench)
    if kind == "baseline":
        flow = f"read_lib {LIB}; read {bench}; resyn; resyn2; dch -v; map; topo; buffer; upsize -v; dnsize -v; stime"
    else:
        flow = f"read_lib {LIB}; read {bench}; resyn; resyn2; dch -v; {COMMAND}; topo; buffer; upsize -v; dnsize -v; stime"
    log = RESULTS / f"{kind}_{safe}.log"
    exit_file = RESULTS / f"{kind}_{safe}.exit"
    rc, text = run_abc(flow, log, exit_file)
    return {
        "benchmark": bench,
        "kind": kind,
        "returncode": rc,
        "metrics": parse_stime(text),
        "diagnostics": parse_stmap89_rows(bench, text) if kind == "candidate" else {},
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
    passed = rc == 0 and (
        "Networks are equivalent" in text or "Networks are equivalent after structural hashing" in text
    )
    return {
        "benchmark": bench,
        "passed": passed,
        "returncode": rc,
        "original": str(orig.relative_to(ROOT)),
        "candidate": str(cand.relative_to(ROOT)),
        "log": str(log.relative_to(ROOT)),
    }


def write_csv(path, rows, preferred=None):
    rows = list(rows)
    if preferred is None:
        preferred = []
    fields = []
    for field in preferred:
        if field not in fields:
            fields.append(field)
    for row in rows:
        for field in row:
            if field not in fields:
                fields.append(field)
    if not fields:
        fields = ["benchmark"]
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def as_int(row, key, default=0):
    try:
        return int(row.get(key, default))
    except (TypeError, ValueError):
        return default


def as_float(row, key, default=None):
    try:
        return float(row.get(key))
    except (TypeError, ValueError):
        return default


def summarize_candidate_cuts(rows):
    out = []
    by_bench = {bench: [] for bench in BENCHMARKS}
    for row in rows:
        by_bench.setdefault(row.get("benchmark"), []).append(row)
    for bench in BENCHMARKS:
        bench_rows = by_bench.get(bench, [])
        stats = {}
        for row in bench_rows:
            if "rows" in row and "viable" in row and "accepted_updates" in row:
                stats = row
        data_rows = [r for r in bench_rows if "cut_ordinal" in r]
        viable = [r for r in data_rows if as_int(r, "viable") == 1]
        accepted = [r for r in data_rows if as_int(r, "selected_update") == 1]
        phase0 = [r for r in viable if r.get("child_requested_phase") == "0"]
        phase1 = [r for r in viable if r.get("child_requested_phase") == "1"]
        accepted_phase0 = [r for r in accepted if r.get("child_requested_phase") == "0"]
        accepted_phase1 = [r for r in accepted if r.get("child_requested_phase") == "1"]
        nonselected_phase0 = [r for r in phase0 if r.get("reason") == "nonselected"]
        phase0_arrivals = [as_float(r, "arrival") for r in phase0 if as_float(r, "arrival") is not None]
        phase0_areas = [as_float(r, "area_flow") for r in phase0 if as_float(r, "area_flow") is not None]
        phase1_arrivals = [as_float(r, "arrival") for r in phase1 if as_float(r, "arrival") is not None]
        phase1_areas = [as_float(r, "area_flow") for r in phase1 if as_float(r, "area_flow") is not None]
        out.append(
            {
                "benchmark": bench,
                "parent_aig": stats.get("parent_aig", data_rows[0].get("parent_aig") if data_rows else "-1"),
                "child_aig": stats.get("child_aig", data_rows[0].get("child_aig") if data_rows else "-1"),
                "rows": stats.get("rows", str(len(data_rows))),
                "viable": stats.get("viable", str(len(viable))),
                "accepted_updates": stats.get("accepted_updates", str(len(accepted))),
                "child_phase0": stats.get("child_phase0", ""),
                "child_phase1": stats.get("child_phase1", ""),
                "child_unknown": stats.get("child_unknown", ""),
                "child_missing": stats.get("child_missing", ""),
                "viable_child_phase0": len(phase0),
                "viable_child_phase1": len(phase1),
                "accepted_child_phase0": len(accepted_phase0),
                "accepted_child_phase1": len(accepted_phase1),
                "nonselected_child_phase0": len(nonselected_phase0),
                "phase0_gates": "+".join(sorted({r.get("gate", "?") for r in phase0})),
                "phase1_gates": "+".join(sorted({r.get("gate", "?") for r in phase1})),
                "phase0_min_arrival": min(phase0_arrivals) if phase0_arrivals else "",
                "phase0_min_area_flow": min(phase0_areas) if phase0_areas else "",
                "phase1_min_arrival": min(phase1_arrivals) if phase1_arrivals else "",
                "phase1_min_area_flow": min(phase1_areas) if phase1_areas else "",
            }
        )
    return out


def summarize_parent_cuts(rows):
    out = []
    by_bench = {bench: [] for bench in BENCHMARKS}
    for row in rows:
        by_bench.setdefault(row.get("benchmark"), []).append(row)
    for bench in BENCHMARKS:
        bench_rows = by_bench.get(bench, [])
        stats = {}
        data_rows = []
        for row in bench_rows:
            if "phase0_has_cut" in row:
                stats = row
            elif "phase" in row:
                data_rows.append(row)
        phases = {row.get("phase"): row for row in data_rows}
        p0 = phases.get("0", {})
        p1 = phases.get("1", {})
        out.append(
            {
                "benchmark": bench,
                "child_aig": stats.get("child_aig", p0.get("child_aig", p1.get("child_aig", "-1"))),
                "parent_aig": stats.get("parent_aig", p0.get("parent_aig", p1.get("parent_aig", "-1"))),
                "requests": stats.get("requests", ""),
                "rows": stats.get("rows", str(len(data_rows))),
                "phase0_has_cut": stats.get("phase0_has_cut", ""),
                "phase1_has_cut": stats.get("phase1_has_cut", ""),
                "child_phase0_count": stats.get("child_phase0", ""),
                "child_phase1_count": stats.get("child_phase1", ""),
                "child_missing": stats.get("child_missing", ""),
                "phase0_gate": p0.get("gate", ""),
                "phase0_child_requested_phase": p0.get("child_requested_phase", ""),
                "phase1_gate": p1.get("gate", ""),
                "phase1_child_requested_phase": p1.get("child_requested_phase", ""),
            }
        )
    return out


def main():
    RESULTS.mkdir(parents=True, exist_ok=True)
    eval_log = RESULTS / "eval.log"
    eval_exit = RESULTS / "eval.exit"
    flow_results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
        futs = []
        for bench in BENCHMARKS:
            futs.append(ex.submit(run_flow, "baseline", bench))
            futs.append(ex.submit(run_flow, "candidate", bench))
        for fut in concurrent.futures.as_completed(futs):
            flow_results.append(fut.result())
    flow_results.sort(key=lambda r: (r["benchmark"], r["kind"]))

    diagnostics = {}
    for result in flow_results:
        if result["kind"] != "candidate":
            continue
        for kind, rows in result["diagnostics"].items():
            diagnostics.setdefault(kind, []).extend(rows)

    for kind, rows in sorted(diagnostics.items()):
        write_csv(RESULTS / f"{kind}.csv", rows, preferred=["benchmark"])

    metric_rows = []
    for result in flow_results:
        row = {
            "benchmark": result["benchmark"],
            "kind": result["kind"],
            "returncode": result["returncode"],
            "stime_delay_ps": result["metrics"].get("stime_delay_ps", ""),
            "stime_area": result["metrics"].get("stime_area", ""),
            "log": result["log"],
            "command": result["command"],
        }
        metric_rows.append(row)
    write_csv(
        RESULTS / "metrics.csv",
        metric_rows,
        preferred=["benchmark", "kind", "returncode", "stime_delay_ps", "stime_area", "log", "command"],
    )

    comparison_rows = []
    for bench in BENCHMARKS:
        base = next(r for r in flow_results if r["benchmark"] == bench and r["kind"] == "baseline")
        cand = next(r for r in flow_results if r["benchmark"] == bench and r["kind"] == "candidate")
        bd = base["metrics"].get("stime_delay_ps")
        cd = cand["metrics"].get("stime_delay_ps")
        ba = base["metrics"].get("stime_area")
        ca = cand["metrics"].get("stime_area")
        comparison_rows.append(
            {
                "benchmark": bench,
                "baseline_delay_ps": bd,
                "candidate_delay_ps": cd,
                "delay_delta_ps": cd - bd if bd is not None and cd is not None else "",
                "delay_delta_pct": 100.0 * (cd - bd) / bd if bd else "",
                "baseline_area": ba,
                "candidate_area": ca,
                "area_delta": ca - ba if ba is not None and ca is not None else "",
                "area_delta_pct": 100.0 * (ca - ba) / ba if ba else "",
            }
        )
    write_csv(RESULTS / "comparison.csv", comparison_rows)

    write_csv(RESULTS / "candidate_cut_summary.csv", summarize_candidate_cuts(diagnostics.get("candidate_cut", []) + diagnostics.get("candidate_cut_stats", [])))
    write_csv(RESULTS / "parent_cut_summary.csv", summarize_parent_cuts(diagnostics.get("parent_cut", []) + diagnostics.get("parent_cut_stats", [])))

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
        cec_results = list(ex.map(run_cec, BENCHMARKS))
    (RESULTS / "cec_summary.json").write_text(json.dumps(cec_results, indent=2) + "\n")

    summary = {
        "command": COMMAND,
        "library": LIB,
        "benchmarks": BENCHMARKS,
        "flow_results": flow_results,
        "diagnostic_kinds": sorted(diagnostics),
        "cec_results": cec_results,
    }
    (RESULTS / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")

    ok = True
    for result in flow_results:
        ok = ok and result["returncode"] == 0 and bool(result["metrics"])
    ok = ok and all(r.get("passed") for r in cec_results)
    eval_log.write_text(
        "\n".join(
            [
                f"{COMMAND} evaluation complete",
                f"flow_results={len(flow_results)}",
                f"diagnostic_kinds={','.join(sorted(diagnostics))}",
                f"cec_passed={sum(1 for r in cec_results if r.get('passed'))}/{len(cec_results)}",
                f"status={'PASS' if ok else 'FAIL'}",
            ]
        )
        + "\n"
    )
    eval_exit.write_text("0\n" if ok else "1\n")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
