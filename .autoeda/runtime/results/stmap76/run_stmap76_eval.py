#!/usr/bin/env python3
import concurrent.futures
import csv
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
RESULTS = ROOT / ".autoeda" / "runtime" / "results" / "stmap76"
ABC = ROOT / "abc"
LIB = "7nm_lvt_ff.lib"
COMMAND = "stmap76"
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
        r"stmap56 guard stats: exact-risk = (?P<exact_risk>\d+)  highest = (?P<highest>\d+)  lower-mod = (?P<lower_mod>\d+)  upper-mod = (?P<upper_mod>\d+)  middle-slack = (?P<middle_slack>\d+)  middle-relief = (?P<middle_relief>\d+)  reject-slack = (?P<reject_slack>\d+)  reject-highest = (?P<reject_highest>\d+)  reject-arrival = (?P<reject_arrival>\d+)  reject-area = (?P<reject_area>\d+)"
    ),
    "near_miss_stats": re.compile(r"stmap56 near-miss stats: near-miss = (?P<near_miss>\d+)"),
    "early_seed_stats": re.compile(r"stmap56 early-seed stats: early-seed = (?P<early_seed>\d+)"),
    "penalty_stats": re.compile(
        r"stmap56 penalty stats: pressure-near-exception-seed = (?P<pressure_near_exception_seed>\d+)  pressure-near-exception-blocked = (?P<pressure_near_exception_blocked>\d+)  cut-only-exception-seed = (?P<cut_only_exception_seed>\d+)  cut-only-exception-blocked = (?P<cut_only_exception_blocked>\d+)  cut-only-area-cap-blocked = (?P<cut_only_area_cap_blocked>\d+)  moderate-penalty-seed = (?P<moderate_penalty_seed>\d+)  moderate-penalty-blocked = (?P<moderate_penalty_blocked>\d+)  strong-penalty-seed = (?P<strong_penalty_seed>\d+)  strong-penalty-blocked = (?P<strong_penalty_blocked>\d+)"
    ),
    "sink_pressure_stats": re.compile(
        r"stmap65 sink-pressure stats: roots = (?P<roots>\d+)  consumer-fanin = (?P<consumer_fanin>\d+)  consumer-fanout = (?P<consumer_fanout>\d+)  raw-pressure-entries = (?P<raw_pressure_entries>\d+)  raw-pressure-updates = (?P<raw_pressure_updates>\d+)  sink-pressure-entries = (?P<sink_pressure_entries>\d+)  sink-pressure-updates = (?P<sink_pressure_updates>\d+)  pressure-missed = (?P<pressure_missed>\d+)  pressure-capacity = (?P<pressure_capacity>\d+)  tracked-aig = (?P<tracked_aig>\d+)  tracked-node = (?P<tracked_node>-?\d+)  tracked-raw-ratio = (?P<tracked_raw_ratio>[0-9]+(?:\.[0-9]+)?)  tracked-sink-ratio = (?P<tracked_sink_ratio>[0-9]+(?:\.[0-9]+)?)"
    ),
    "feedback_stats": re.compile(
        r"stmap65 feedback: remap = 1  active = (?P<active>\d+)  max-load-ratio = (?P<max_load_ratio>[0-9]+(?:\.[0-9]+)?)  over-max-frac = (?P<over_max_frac>[0-9]+(?:\.[0-9]+)?)  severity = (?P<severity>[0-9]+(?:\.[0-9]+)?)  raw-pressure-entries = (?P<raw_pressure_entries>\d+)  sink-pressure-entries = (?P<sink_pressure_entries>\d+)  pressure-capacity = (?P<pressure_capacity>\d+)"
    ),
    "blended_gain_stats": re.compile(
        r"stmap65 blended-gain: base-gain = (?P<base_gain>[0-9]+(?:\.[0-9]+)?)  blend-gain = (?P<blend_gain>[0-9]+(?:\.[0-9]+)?)  selected-gain = (?P<selected_gain>[0-9]+(?:\.[0-9]+)?)  severity-threshold = (?P<severity_threshold>[0-9]+(?:\.[0-9]+)?)  raw-pressure-threshold = (?P<raw_pressure_threshold>\d+)  selected-blend-gain = (?P<selected_blend_gain>\d+)"
    ),
    "bounded_pressure_stats": re.compile(
        r"stmap65 bounded-pressure stats: severe-transfer = (?P<severe_transfer>\d+)  raw-weight = (?P<raw_weight>[0-9]+(?:\.[0-9]+)?)  ratio-cap = (?P<ratio_cap>[0-9]+(?:\.[0-9]+)?)  bounded-pressure-entries = (?P<bounded_pressure_entries>\d+)  lifted = (?P<lifted>\d+)  capped = (?P<capped>\d+)  raw-only = (?P<raw_only>\d+)  sink-fallback = (?P<sink_fallback>\d+)  tracked-aig = (?P<tracked_aig>\d+)  tracked-raw-ratio = (?P<tracked_raw_ratio>[0-9]+(?:\.[0-9]+)?)  tracked-sink-ratio = (?P<tracked_sink_ratio>[0-9]+(?:\.[0-9]+)?)  tracked-bounded-ratio = (?P<tracked_bounded_ratio>[0-9]+(?:\.[0-9]+)?)"
    ),
    "mapper_mode_stats": re.compile(
        r"stmap65 mapper-mode: bounded-pressure-transfer-mode = (?P<bounded_pressure_transfer_mode>\d+)  cut-only-before-moderate = (?P<cut_only_before_moderate>\d+)  load-drop-cut-only-guard = (?P<load_drop_cut_only_guard>\d+)  strong-node-load-drop-guard = (?P<strong_node_load_drop_guard>\d+)  strong-node-pressure-min = (?P<strong_node_pressure_min>[0-9]+(?:\.[0-9]+)?)  blended-gain = (?P<blended_gain>[0-9]+(?:\.[0-9]+)?)  pressure-feed = (?P<pressure_feed>[-a-z]+)  raw-weight = (?P<raw_weight>[0-9]+(?:\.[0-9]+)?)  bounded-ratio-cap = (?P<bounded_ratio_cap>[0-9]+(?:\.[0-9]+)?)  inherited-cut-only-area-save-cap = (?P<inherited_cut_only_area_save_cap>[0-9]+(?:\.[0-9]+)?x-inverter)  tracked-near-miss-node = (?P<tracked_near_miss_node>-?\d+)  cut-only-gate-diag = (?P<cut_only_gate_diag>\d+)"
    ),
    "final_witness_stats": re.compile(
        r"stmap76 final-witness stats: witnesses = (?P<witnesses>\d+)  final-delay = (?P<final_delay>[0-9]+(?:\.[0-9]+)?)"
    ),
    "final_witness_summary": re.compile(
        r"stmap76 final-witness summary: witnesses = (?P<witnesses>\d+)  matched = (?P<matched>\d+)  criticality-ge-0p50 = (?P<criticality_ge_0p50>\d+)"
    ),
    "final_critical_stats": re.compile(
        r"stmap76 final-critical stats: nodes = (?P<nodes>\d+)  final-delay = (?P<final_delay>[0-9]+(?:\.[0-9]+)?)  criticality-ge-0p50 = (?P<criticality_ge_0p50>\d+)  criticality-ge-0p75 = (?P<criticality_ge_0p75>\d+)  slack-le-5ps = (?P<slack_le_5ps>\d+)"
    ),
    "selected_match_stats": re.compile(
        r"stmap76 selected-match stats: watched-aigs = (?P<watched_aigs>\d+)  rows = (?P<rows>\d+)  watch0-aig-id = (?P<watch0_aig_id>-?\d+)  watch0-rows = (?P<watch0_rows>\d+)  watch1-aig-id = (?P<watch1_aig_id>-?\d+)  watch1-rows = (?P<watch1_rows>\d+)  watch2-aig-id = (?P<watch2_aig_id>-?\d+)  watch2-rows = (?P<watch2_rows>\d+)  watch3-aig-id = (?P<watch3_aig_id>-?\d+)  watch3-rows = (?P<watch3_rows>\d+)"
    ),
    "phase_survival_stats": re.compile(
        r"stmap76 phase-survival stats: watched-aigs = (?P<watched_aigs>\d+)  matched-aigs = (?P<matched_aigs>\d+)  final-delay = (?P<final_delay>[0-9]+(?:\.[0-9]+)?)"
    ),
    "cut_only_gate_stats": re.compile(
        r"stmap61 cut-only gate stats: tracked-node = (?P<tracked_node>-?\d+)  hits = (?P<hits>\d+)  primitive-pass = (?P<primitive_pass>\d+)  raw-pass = (?P<raw_pass>\d+)  raw-blocked-by-moderate = (?P<raw_blocked_by_moderate>\d+)  raw-blocked-by-primitive = (?P<raw_blocked_by_primitive>\d+)  area-cap-pass = (?P<area_cap_pass>\d+)  area-cap-blocked = (?P<area_cap_blocked>\d+)  accepted = (?P<accepted>\d+)  soft-seed-fail = (?P<soft_seed_fail>\d+)  moderate-candidate-fail = (?P<moderate_candidate_fail>\d+)  feedback-fail = (?P<feedback_fail>\d+)  entry-fail = (?P<entry_fail>\d+)  agreement-blocked = (?P<agreement_blocked>\d+)  node-zero-fail = (?P<node_zero_fail>\d+)  cut-band-fail = (?P<cut_band_fail>\d+)  arrival-fail = (?P<arrival_fail>\d+)  slack-fail = (?P<slack_fail>\d+)"
    ),

    "near_strong_node_stats": re.compile(
        r"stmap66 near-strong-node load-drop stats: near-band = (?P<near_band>\d+)  area-cap-pass = (?P<area_cap_pass>\d+)"
    ),
    "near_miss_leaf_stats": re.compile(
        r"stmap60 near-miss leaf stats: tracked-node = (?P<tracked_node>-?\d+)  hits = (?P<hits>\d+)  leaf-rows = (?P<leaf_rows>\d+)  max-leaf-node = (?P<max_leaf_node>-?\d+)  max-leaf-aig-id = (?P<max_leaf_aig_id>-?\d+)  max-leaf-pressure-ratio = (?P<max_leaf_pressure_ratio>[0-9]+(?:\.[0-9]+)?)"
    ),
    "near_miss_leaf_summary": re.compile(
        r"stmap60 near-miss leaf summary: index = (?P<index>\d+)  near-miss-index = (?P<near_miss_index>\d+)  tracked-node = (?P<tracked_node>-?\d+)  node-aig-id = (?P<node_aig_id>-?\d+)  phase = (?P<phase>\d+)  leaves = (?P<leaves>\d+)  max-leaf-node = (?P<max_leaf_node>-?\d+)  max-leaf-aig-id = (?P<max_leaf_aig_id>-?\d+)  max-leaf-pressure-ratio = (?P<max_leaf_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  node-sink-pressure-ratio = (?P<node_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  cut-sink-pressure-ratio = (?P<cut_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  slack = (?P<slack>-?[0-9]+(?:\.[0-9]+)?)  area-save = (?P<area_save>-?[0-9]+(?:\.[0-9]+)?)  arrival-delta = (?P<arrival_delta>-?[0-9]+(?:\.[0-9]+)?)  arrival-gain-margin = (?P<arrival_gain_margin>-?[0-9]+(?:\.[0-9]+)?)  area-margin = (?P<area_margin>-?[0-9]+(?:\.[0-9]+)?)  scl-feedback = (?P<scl_feedback>[0-9]+(?:\.[0-9]+)?)"
    ),
}
MULTI_STAT_PATTERNS = {
    "final_witness": re.compile(
        r"stmap76 final-witness: index = (?P<index>\d+)  aig-id = (?P<aig_id>-?\d+)  phase = (?P<phase>\d+)  seed-node = (?P<seed_node>-?\d+)  final-node = (?P<final_node>-?\d+)  matched = (?P<matched>\d+)(?:  final-gate = (?P<final_gate>[^ ]+)  final-fanouts = (?P<final_fanouts>\d+)  final-load = (?P<final_load>[0-9]+(?:\.[0-9]+)?)  final-max-cap = (?P<final_max_cap>[0-9]+(?:\.[0-9]+)?)  final-load-ratio = (?P<final_load_ratio>[0-9]+(?:\.[0-9]+)?)  final-criticality = (?P<final_criticality>[0-9]+(?:\.[0-9]+)?)  final-slack = (?P<final_slack>-?[0-9]+(?:\.[0-9]+)?))?  seed-node-pressure-ratio = (?P<seed_node_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  seed-cut-pressure-ratio = (?P<seed_cut_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  seed-slack = (?P<seed_slack>-?[0-9]+(?:\.[0-9]+)?)  seed-area-save = (?P<seed_area_save>-?[0-9]+(?:\.[0-9]+)?)  seed-arrival-delta = (?P<seed_arrival_delta>-?[0-9]+(?:\.[0-9]+)?)  seed-arrival-gain-margin = (?P<seed_arrival_gain_margin>-?[0-9]+(?:\.[0-9]+)?)  scl-feedback = (?P<scl_feedback>[0-9]+(?:\.[0-9]+)?)"
    ),
    "final_critical_lineage": re.compile(
        r"stmap76 final-critical lineage: rank = (?P<rank>\d+)  final-node = (?P<final_node>-?\d+)  aig-id = (?P<aig_id>-?\d+)  phase = (?P<phase>-?\d+)  final-gate = (?P<final_gate>[^ ]+)  final-fanouts = (?P<final_fanouts>\d+)  final-criticality = (?P<final_criticality>[0-9]+(?:\.[0-9]+)?)  final-slack = (?P<final_slack>-?[0-9]+(?:\.[0-9]+)?)  final-arrival = (?P<final_arrival>-?[0-9]+(?:\.[0-9]+)?)  final-departure = (?P<final_departure>-?[0-9]+(?:\.[0-9]+)?)  final-slew = (?P<final_slew>[0-9]+(?:\.[0-9]+)?)  final-load = (?P<final_load>[0-9]+(?:\.[0-9]+)?)  final-max-cap = (?P<final_max_cap>[0-9]+(?:\.[0-9]+)?)  final-load-ratio = (?P<final_load_ratio>[0-9]+(?:\.[0-9]+)?)"
    ),
    "selected_match": re.compile(
        r"stmap76 selected-match: index = (?P<index>\d+)  watch-index = (?P<watch_index>\d+)  mapper-mode = (?P<mapper_mode>\d+)  node = (?P<node>-?\d+)  aig-id = (?P<aig_id>-?\d+)  level = (?P<level>\d+)  refs = (?P<refs>-?\d+)  phase = (?P<phase>\d+)  gate = (?P<gate>[^ ]+)  leaves = (?P<leaves>\d+)  cut-leaf-load-avg = (?P<cut_leaf_load_avg>[0-9]+(?:\.[0-9]+)?)  fanout-limit = (?P<fanout_limit>-?\d+)  load-drive-ratio = (?P<load_drive_ratio>[0-9]+(?:\.[0-9]+)?)  arrival = (?P<arrival>-?[0-9]+(?:\.[0-9]+)?)  required = (?P<required>-?[0-9]+(?:\.[0-9]+)?)  slack = (?P<slack>-?[0-9]+(?:\.[0-9]+)?)  area-flow = (?P<area_flow>-?[0-9]+(?:\.[0-9]+)?)  u-phase-best = (?P<u_phase_best>\d+)  node-sink-pressure-ratio = (?P<node_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  cut-sink-pressure-ratio = (?P<cut_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  scl-feedback = (?P<scl_feedback>[0-9]+(?:\.[0-9]+)?)  leaf0-node = (?P<leaf0_node>-?\d+)  leaf0-aig-id = (?P<leaf0_aig_id>-?\d+)  leaf1-node = (?P<leaf1_node>-?\d+)  leaf1-aig-id = (?P<leaf1_aig_id>-?\d+)  leaf2-node = (?P<leaf2_node>-?\d+)  leaf2-aig-id = (?P<leaf2_aig_id>-?\d+)  leaf3-node = (?P<leaf3_node>-?\d+)  leaf3-aig-id = (?P<leaf3_aig_id>-?\d+)  leaf4-node = (?P<leaf4_node>-?\d+)  leaf4-aig-id = (?P<leaf4_aig_id>-?\d+)  leaf5-node = (?P<leaf5_node>-?\d+)  leaf5-aig-id = (?P<leaf5_aig_id>-?\d+)"
    ),
    "phase_survival": re.compile(
        r"stmap76 phase-survival: watch-index = (?P<watch_index>\d+)  aig-id = (?P<aig_id>-?\d+)  final-nodes = (?P<final_nodes>\d+)  phase0-nodes = (?P<phase0_nodes>\d+)  phase0-best-node = (?P<phase0_best_node>-?\d+)  phase0-best-gate = (?P<phase0_best_gate>[^ ]+)  phase0-best-criticality = (?P<phase0_best_criticality>[0-9]+(?:\.[0-9]+)?)  phase0-best-slack = (?P<phase0_best_slack>-?[0-9]+(?:\.[0-9]+)?)  phase0-best-load-ratio = (?P<phase0_best_load_ratio>[0-9]+(?:\.[0-9]+)?)  phase1-nodes = (?P<phase1_nodes>\d+)  phase1-best-node = (?P<phase1_best_node>-?\d+)  phase1-best-gate = (?P<phase1_best_gate>[^ ]+)  phase1-best-criticality = (?P<phase1_best_criticality>[0-9]+(?:\.[0-9]+)?)  phase1-best-slack = (?P<phase1_best_slack>-?[0-9]+(?:\.[0-9]+)?)  phase1-best-load-ratio = (?P<phase1_best_load_ratio>[0-9]+(?:\.[0-9]+)?)"
    ),
    "phase_output_context": re.compile(
        r"stmap76 phase-output-context: watch-index = (?P<watch_index>\d+)  aig-id = (?P<aig_id>-?\d+)  phase = (?P<phase>\d+)  top-node = (?P<top_node>-?\d+)  top-gate = (?P<top_gate>[^ ]+)  top-criticality = (?P<top_criticality>[0-9]+(?:\.[0-9]+)?)  top-slack = (?P<top_slack>-?[0-9]+(?:\.[0-9]+)?)  top-load-ratio = (?P<top_load_ratio>[0-9]+(?:\.[0-9]+)?)  final-fanouts = (?P<final_fanouts>\d+)  node-fanouts = (?P<node_fanouts>\d+)  co-fanouts = (?P<co_fanouts>\d+)  best-fanout-node = (?P<best_fanout_node>-?\d+)  best-fanout-aig-id = (?P<best_fanout_aig_id>-?\d+)  best-fanout-phase = (?P<best_fanout_phase>-?\d+)  best-fanout-gate = (?P<best_fanout_gate>[^ ]+)  best-fanout-criticality = (?P<best_fanout_criticality>[0-9]+(?:\.[0-9]+)?)  best-fanout-slack = (?P<best_fanout_slack>-?[0-9]+(?:\.[0-9]+)?)"
    ),
    "accepted_pressure_witness_source": re.compile(
        r"stmap76 accepted-pressure witness source: index = (?P<index>\d+)  node = (?P<node>-?\d+)  aig-id = (?P<aig_id>-?\d+)  level = (?P<level>\d+)  refs = (?P<refs>-?\d+)  leaves = (?P<leaves>\d+)  phase = (?P<phase>\d+)  class = (?P<source_class>[-a-z]+)  stored = (?P<stored>\d+)  replaced = (?P<replaced>\d+)  duplicate = (?P<duplicate>\d+)  witness-score = (?P<witness_score>[0-9]+(?:\.[0-9]+)?)  slack = (?P<slack>-?[0-9]+(?:\.[0-9]+)?)  area-save = (?P<area_save>-?[0-9]+(?:\.[0-9]+)?)  arrival-delta = (?P<arrival_delta>-?[0-9]+(?:\.[0-9]+)?)  arrival-gain-margin = (?P<arrival_gain_margin>-?[0-9]+(?:\.[0-9]+)?)  penalty-factor = (?P<penalty_factor>[0-9]+(?:\.[0-9]+)?)  area-margin = (?P<area_margin>-?[0-9]+(?:\.[0-9]+)?)  pressure-agreement = (?P<pressure_agreement>\d+)  pressure-near = (?P<pressure_near>\d+)  cut-only = (?P<cut_only>\d+)  area-cap = (?P<area_cap>\d+)  node-sink-pressure-ratio = (?P<node_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  cut-sink-pressure-ratio = (?P<cut_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  scl-feedback = (?P<scl_feedback>[0-9]+(?:\.[0-9]+)?)"
    ),
    "pressure_near_witness_source": re.compile(
        r"stmap76 pressure-near witness source: index = (?P<index>\d+)  node = (?P<node>-?\d+)  aig-id = (?P<aig_id>-?\d+)  level = (?P<level>\d+)  refs = (?P<refs>-?\d+)  leaves = (?P<leaves>\d+)  phase = (?P<phase>\d+)  accepted = (?P<accepted>\d+)  stored = (?P<stored>\d+)  duplicate = (?P<duplicate>\d+)  slack = (?P<slack>-?[0-9]+(?:\.[0-9]+)?)  area-save = (?P<area_save>-?[0-9]+(?:\.[0-9]+)?)  arrival-delta = (?P<arrival_delta>-?[0-9]+(?:\.[0-9]+)?)  arrival-gain-margin = (?P<arrival_gain_margin>-?[0-9]+(?:\.[0-9]+)?)  area-margin = (?P<area_margin>-?[0-9]+(?:\.[0-9]+)?)  node-sink-pressure-ratio = (?P<node_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  cut-sink-pressure-ratio = (?P<cut_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  scl-feedback = (?P<scl_feedback>[0-9]+(?:\.[0-9]+)?)"
    ),
    "blocked_strong_witness_source": re.compile(
        r"stmap76 blocked-strong witness source: index = (?P<index>\d+)  node = (?P<node>-?\d+)  aig-id = (?P<aig_id>-?\d+)  level = (?P<level>\d+)  refs = (?P<refs>-?\d+)  leaves = (?P<leaves>\d+)  phase = (?P<phase>\d+)  stored = (?P<stored>\d+)  duplicate = (?P<duplicate>\d+)  slack = (?P<slack>-?[0-9]+(?:\.[0-9]+)?)  area-save = (?P<area_save>-?[0-9]+(?:\.[0-9]+)?)  arrival-delta = (?P<arrival_delta>-?[0-9]+(?:\.[0-9]+)?)  arrival-gain-margin = (?P<arrival_gain_margin>-?[0-9]+(?:\.[0-9]+)?)  penalty-factor = (?P<penalty_factor>[0-9]+(?:\.[0-9]+)?)  area-margin = (?P<area_margin>-?[0-9]+(?:\.[0-9]+)?)  cut-leaf-load-avg = (?P<cut_leaf_load_avg>[0-9]+(?:\.[0-9]+)?)  fanout-limit = (?P<fanout_limit>-?\d+)  load-drive-ratio = (?P<load_drive_ratio>[0-9]+(?:\.[0-9]+)?)  node-sink-pressure-ratio = (?P<node_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  cut-sink-pressure-ratio = (?P<cut_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  scl-feedback = (?P<scl_feedback>[0-9]+(?:\.[0-9]+)?)"
    ),
    "moderate_penalty_witness_source": re.compile(
        r"stmap76 moderate-penalty witness source: index = (?P<index>\d+)  node = (?P<node>-?\d+)  aig-id = (?P<aig_id>-?\d+)  level = (?P<level>\d+)  refs = (?P<refs>-?\d+)  leaves = (?P<leaves>\d+)  phase = (?P<phase>\d+)  stored = (?P<stored>\d+)  duplicate = (?P<duplicate>\d+)  slack = (?P<slack>-?[0-9]+(?:\.[0-9]+)?)  area-save = (?P<area_save>-?[0-9]+(?:\.[0-9]+)?)  arrival-delta = (?P<arrival_delta>-?[0-9]+(?:\.[0-9]+)?)  arrival-gain-margin = (?P<arrival_gain_margin>-?[0-9]+(?:\.[0-9]+)?)  penalty-factor = (?P<penalty_factor>[0-9]+(?:\.[0-9]+)?)  area-margin = (?P<area_margin>-?[0-9]+(?:\.[0-9]+)?)  node-sink-pressure-ratio = (?P<node_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  cut-sink-pressure-ratio = (?P<cut_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  area-cap = (?P<area_cap>\d+)  scl-feedback = (?P<scl_feedback>[0-9]+(?:\.[0-9]+)?)"
    ),

    "near_strong_node_diag": re.compile(
        r"stmap66 near-strong-node load-drop diag: index = (?P<index>\d+)  node = (?P<node>-?\d+)  aig-id = (?P<aig_id>-?\d+)  level = (?P<level>\d+)  refs = (?P<refs>-?\d+)  leaves = (?P<leaves>\d+)  phase = (?P<phase>\d+)  area-cap-pass = (?P<area_cap_pass>\d+)  slack = (?P<slack>-?[0-9]+(?:\.[0-9]+)?)  area-save = (?P<area_save>-?[0-9]+(?:\.[0-9]+)?)  area-save-cap = (?P<area_save_cap>-?[0-9]+(?:\.[0-9]+)?)  arrival-delta = (?P<arrival_delta>-?[0-9]+(?:\.[0-9]+)?)  arrival-gain-margin = (?P<arrival_gain_margin>-?[0-9]+(?:\.[0-9]+)?)  node-sink-pressure-ratio = (?P<node_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  cut-sink-pressure-ratio = (?P<cut_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  pressure-entries = (?P<pressure_entries>\d+)  scl-feedback = (?P<scl_feedback>[0-9]+(?:\.[0-9]+)?)"
    ),
    "near_miss_leaf_diag": re.compile(
        r"stmap60 near-miss leaf diag: index = (?P<index>\d+)  near-miss-index = (?P<near_miss_index>\d+)  tracked-node = (?P<tracked_node>-?\d+)  node-aig-id = (?P<node_aig_id>-?\d+)  phase = (?P<phase>\d+)  leaf-index = (?P<leaf_index>\d+)  leaf-node = (?P<leaf_node>-?\d+)  leaf-aig-id = (?P<leaf_aig_id>-?\d+)  leaf-pressure-ratio = (?P<leaf_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  node-sink-pressure-ratio = (?P<node_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  cut-sink-pressure-ratio = (?P<cut_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  reason = (?P<reason>[-a-z]+)"
    ),
    "cut_only_gate_diag": re.compile(
        r"stmap61 cut-only gate diag: index = (?P<index>\d+)  near-miss-index = (?P<near_miss_index>\d+)  tracked-node = (?P<tracked_node>-?\d+)  node-aig-id = (?P<node_aig_id>-?\d+)  phase = (?P<phase>\d+)  reason = (?P<reason>[-a-z]+)  first-blocker = (?P<first_blocker>[-a-z]+)  profile-open = (?P<profile_open>\d+)  early-depth = (?P<early_depth>\d+)  soft-seed = (?P<soft_seed>\d+)  moderate-candidate = (?P<moderate_candidate>\d+)  tight-critical = (?P<tight_critical>\d+)  slack-1p25-pass = (?P<slack_1p25_pass>\d+)  pressure-agreement = (?P<pressure_agreement>\d+)  pressure-near = (?P<pressure_near>\d+)  primitive-pass = (?P<primitive_pass>\d+)  raw-expected = (?P<raw_expected>\d+)  raw-pass = (?P<raw_pass>\d+)  area-cap-pass = (?P<area_cap_pass>\d+)  area-cap-blocked = (?P<area_cap_blocked>\d+)  accepted = (?P<accepted>\d+)  severe-feedback-pass = (?P<severe_feedback_pass>\d+)  pressure-entries = (?P<pressure_entries>\d+)  pressure-entries-pass = (?P<pressure_entries_pass>\d+)  moderate-gain-pass = (?P<moderate_gain_pass>\d+)  node-zero-pass = (?P<node_zero_pass>\d+)  cut-band-pass = (?P<cut_band_pass>\d+)  arrival-strength-pass = (?P<arrival_strength_pass>\d+)  slack-pass = (?P<slack_pass>\d+)  node-sink-pressure-ratio = (?P<node_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  cut-sink-pressure-ratio = (?P<cut_sink_pressure_ratio>[0-9]+(?:\.[0-9]+)?)  slack = (?P<slack>-?[0-9]+(?:\.[0-9]+)?)  slack-margin = (?P<slack_margin>-?[0-9]+(?:\.[0-9]+)?)  area-save = (?P<area_save>-?[0-9]+(?:\.[0-9]+)?)  area-save-cap = (?P<area_save_cap>-?[0-9]+(?:\.[0-9]+)?)  arrival-delta = (?P<arrival_delta>-?[0-9]+(?:\.[0-9]+)?)  arrival-gain-margin = (?P<arrival_gain_margin>-?[0-9]+(?:\.[0-9]+)?)  area-margin = (?P<area_margin>-?[0-9]+(?:\.[0-9]+)?)  one-inv-area = (?P<one_inv_area>-?[0-9]+(?:\.[0-9]+)?)  scl-feedback = (?P<scl_feedback>[0-9]+(?:\.[0-9]+)?)"
    ),
}
EMPTY_STAT_FIELDS = {
    "final_witness_stats": ["benchmark", "witnesses", "final_delay"],
    "final_witness_summary": ["benchmark", "witnesses", "matched", "criticality_ge_0p50"],
    "final_critical_stats": ["benchmark", "nodes", "final_delay", "criticality_ge_0p50", "criticality_ge_0p75", "slack_le_5ps"],
    "selected_match_stats": [
        "benchmark", "watched_aigs", "rows", "watch0_aig_id", "watch0_rows",
        "watch1_aig_id", "watch1_rows", "watch2_aig_id", "watch2_rows",
        "watch3_aig_id", "watch3_rows",
    ],
    "phase_survival_stats": ["benchmark", "watched_aigs", "matched_aigs", "final_delay"],
}
EMPTY_MULTI_FIELDS = {
    "final_witness": [
        "benchmark", "index", "aig_id", "phase", "seed_node", "final_node", "matched",
        "final_gate", "final_fanouts", "final_load", "final_max_cap", "final_load_ratio",
        "final_criticality", "final_slack", "seed_node_pressure_ratio",
        "seed_cut_pressure_ratio", "seed_slack", "seed_area_save",
        "seed_arrival_delta", "seed_arrival_gain_margin", "scl_feedback",
    ],
    "pressure_near_witness_source": [
        "benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase",
        "accepted", "stored", "duplicate", "slack", "area_save", "arrival_delta",
        "arrival_gain_margin", "area_margin", "node_sink_pressure_ratio",
        "cut_sink_pressure_ratio", "scl_feedback",
    ],
    "accepted_pressure_witness_source": [
        "benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase",
        "source_class", "stored", "replaced", "duplicate", "witness_score",
        "slack", "area_save", "arrival_delta", "arrival_gain_margin",
        "penalty_factor", "area_margin", "pressure_agreement", "pressure_near",
        "cut_only", "area_cap", "node_sink_pressure_ratio",
        "cut_sink_pressure_ratio", "scl_feedback",
    ],
    "blocked_strong_witness_source": [
        "benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase",
        "stored", "duplicate", "slack", "area_save", "arrival_delta",
        "arrival_gain_margin", "penalty_factor", "area_margin", "cut_leaf_load_avg",
        "fanout_limit", "load_drive_ratio", "node_sink_pressure_ratio",
        "cut_sink_pressure_ratio", "scl_feedback",
    ],
    "moderate_penalty_witness_source": [
        "benchmark", "index", "node", "aig_id", "level", "refs", "leaves", "phase",
        "stored", "duplicate", "slack", "area_save", "arrival_delta",
        "arrival_gain_margin", "penalty_factor", "area_margin",
        "node_sink_pressure_ratio", "cut_sink_pressure_ratio", "area_cap", "scl_feedback",
    ],
    "final_critical_lineage": [
        "benchmark", "rank", "final_node", "aig_id", "phase", "final_gate",
        "final_fanouts", "final_criticality", "final_slack", "final_arrival",
        "final_departure", "final_slew", "final_load", "final_max_cap",
        "final_load_ratio",
    ],
    "selected_match": [
        "benchmark", "index", "watch_index", "mapper_mode", "node", "aig_id",
        "level", "refs", "phase", "gate", "leaves", "cut_leaf_load_avg",
        "fanout_limit", "load_drive_ratio", "arrival", "required", "slack",
        "area_flow", "u_phase_best", "node_sink_pressure_ratio",
        "cut_sink_pressure_ratio", "scl_feedback", "leaf0_node",
        "leaf0_aig_id", "leaf1_node", "leaf1_aig_id", "leaf2_node",
        "leaf2_aig_id", "leaf3_node", "leaf3_aig_id", "leaf4_node",
        "leaf4_aig_id", "leaf5_node", "leaf5_aig_id",
    ],
    "phase_survival": [
        "benchmark", "watch_index", "aig_id", "final_nodes",
        "phase0_nodes", "phase0_best_node", "phase0_best_gate",
        "phase0_best_criticality", "phase0_best_slack",
        "phase0_best_load_ratio", "phase1_nodes", "phase1_best_node",
        "phase1_best_gate", "phase1_best_criticality", "phase1_best_slack",
        "phase1_best_load_ratio",
    ],
    "phase_output_context": [
        "benchmark", "watch_index", "aig_id", "phase", "top_node",
        "top_gate", "top_criticality", "top_slack", "top_load_ratio",
        "final_fanouts", "node_fanouts", "co_fanouts", "best_fanout_node",
        "best_fanout_aig_id", "best_fanout_phase", "best_fanout_gate",
        "best_fanout_criticality", "best_fanout_slack",
    ],
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


def parse_multi_stats(text):
    out = {}
    plain = clean(text)
    for name, pattern in MULTI_STAT_PATTERNS.items():
        matches = [m.groupdict() for m in pattern.finditer(plain)]
        if matches:
            out[name] = matches
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
        "multi_stats": parse_multi_stats(text) if kind == "candidate" else {},
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


def _float_value(row, key, default=0.0):
    value = row.get(key)
    if value in (None, ""):
        return default
    return float(value)


def write_path_proximity(multi_stats_by_name):
    source_classes = {}
    for name, label in (
        ("accepted_pressure_witness_source", "accepted-pressure"),
        ("near_strong_node_diag", "near-strong-node"),
        ("blocked_strong_witness_source", "blocked-strong-pressure"),
        ("pressure_near_witness_source", "pressure-near"),
        ("moderate_penalty_witness_source", "moderate-penalty"),
    ):
        for row in multi_stats_by_name.get(name, []):
            aig_id = row.get("aig_id") or row.get("node_aig_id")
            phase = row.get("phase")
            if aig_id in (None, "") or phase in (None, ""):
                continue
            source_classes.setdefault((row["benchmark"], aig_id, phase), set()).add(row.get("source_class", label))

    ranking = []
    for row in multi_stats_by_name.get("final_witness", []):
        key = (row["benchmark"], row.get("aig_id"), row.get("phase"))
        matched = row.get("matched") == "1"
        criticality = _float_value(row, "final_criticality") if matched else 0.0
        slack = _float_value(row, "final_slack", 1.0e9) if matched else 1.0e9
        ranking.append({
            "benchmark": row["benchmark"],
            "rank": 0,
            "aig_id": row.get("aig_id", ""),
            "phase": row.get("phase", ""),
            "seed_node": row.get("seed_node", ""),
            "final_node": row.get("final_node", ""),
            "matched": row.get("matched", ""),
            "source_classes": "+".join(sorted(source_classes.get(key, set()))),
            "final_criticality": row.get("final_criticality", ""),
            "final_slack": row.get("final_slack", ""),
            "final_load_ratio": row.get("final_load_ratio", ""),
            "seed_node_pressure_ratio": row.get("seed_node_pressure_ratio", ""),
            "seed_cut_pressure_ratio": row.get("seed_cut_pressure_ratio", ""),
            "seed_slack": row.get("seed_slack", ""),
            "seed_area_save": row.get("seed_area_save", ""),
            "seed_arrival_delta": row.get("seed_arrival_delta", ""),
        })
    ranking.sort(key=lambda r: (-_float_value(r, "final_criticality"), _float_value(r, "final_slack", 1.0e9), r["benchmark"], int(r["rank"])))
    for index, row in enumerate(ranking, start=1):
        row["rank"] = index

    rank_fields = [
        "rank", "benchmark", "aig_id", "phase", "seed_node", "final_node", "matched",
        "source_classes", "final_criticality", "final_slack", "final_load_ratio",
        "seed_node_pressure_ratio", "seed_cut_pressure_ratio", "seed_slack",
        "seed_area_save", "seed_arrival_delta",
    ]
    write_csv(RESULTS / "path_proximity_ranking.csv", ranking, rank_fields)

    summary = []
    for bench in BENCHMARKS:
        rows = [row for row in ranking if row["benchmark"] == bench]
        matched_rows = [row for row in rows if row["matched"] == "1"]
        best = matched_rows[0] if matched_rows else None
        summary.append({
            "benchmark": bench,
            "witnesses": len(rows),
            "matched": len(matched_rows),
            "criticality_ge_0p25": sum(1 for row in matched_rows if _float_value(row, "final_criticality") >= 0.25),
            "criticality_ge_0p35": sum(1 for row in matched_rows if _float_value(row, "final_criticality") >= 0.35),
            "criticality_ge_0p50": sum(1 for row in matched_rows if _float_value(row, "final_criticality") >= 0.50),
            "best_final_criticality": best["final_criticality"] if best else "",
            "best_final_slack": best["final_slack"] if best else "",
            "best_source_classes": best["source_classes"] if best else "",
        })
    write_csv(
        RESULTS / "path_proximity_summary.csv",
        summary,
        [
            "benchmark", "witnesses", "matched", "criticality_ge_0p25",
            "criticality_ge_0p35", "criticality_ge_0p50",
            "best_final_criticality", "best_final_slack", "best_source_classes",
        ],
    )


def write_final_critical_summary(multi_stats_by_name):
    rows_out = []
    for bench in BENCHMARKS:
        rows = [row for row in multi_stats_by_name.get("final_critical_lineage", []) if row["benchmark"] == bench]
        rows.sort(key=lambda r: int(r["rank"]))
        best = rows[0] if rows else {}
        rows_out.append({
            "benchmark": bench,
            "reported_rows": len(rows),
            "criticality_ge_0p50_in_top": sum(1 for row in rows if _float_value(row, "final_criticality") >= 0.50),
            "criticality_ge_0p75_in_top": sum(1 for row in rows if _float_value(row, "final_criticality") >= 0.75),
            "top_aig_id": best.get("aig_id", ""),
            "top_phase": best.get("phase", ""),
            "top_final_node": best.get("final_node", ""),
            "top_final_gate": best.get("final_gate", ""),
            "top_final_criticality": best.get("final_criticality", ""),
            "top_final_slack": best.get("final_slack", ""),
            "top_final_arrival": best.get("final_arrival", ""),
            "top_final_load_ratio": best.get("final_load_ratio", ""),
        })
    write_csv(
        RESULTS / "final_critical_summary.csv",
        rows_out,
        [
            "benchmark", "reported_rows", "criticality_ge_0p50_in_top",
            "criticality_ge_0p75_in_top", "top_aig_id", "top_phase",
            "top_final_node", "top_final_gate", "top_final_criticality",
            "top_final_slack", "top_final_arrival", "top_final_load_ratio",
        ],
    )


def write_selected_match_summary(multi_stats_by_name):
    rows_out = []
    for bench in BENCHMARKS:
        rows = [row for row in multi_stats_by_name.get("selected_match", []) if row["benchmark"] == bench]
        modes = sorted({row.get("mapper_mode", "") for row in rows if row.get("mapper_mode", "") != ""}, key=lambda v: int(v))
        aig_ids = sorted({row.get("aig_id", "") for row in rows if row.get("aig_id", "") != ""}, key=lambda v: int(v))
        best_pressure = max(rows, key=lambda r: _float_value(r, "node_sink_pressure_ratio"), default={})
        best_cut_pressure = max(rows, key=lambda r: _float_value(r, "cut_sink_pressure_ratio"), default={})
        latest_mode = max(rows, key=lambda r: int(r.get("mapper_mode", "0")), default={})
        rows_out.append({
            "benchmark": bench,
            "selected_match_rows": len(rows),
            "watched_aigs_seen": "+".join(aig_ids),
            "mapper_modes_seen": "+".join(modes),
            "max_node_sink_pressure_ratio": best_pressure.get("node_sink_pressure_ratio", ""),
            "max_node_sink_pressure_aig": best_pressure.get("aig_id", ""),
            "max_cut_sink_pressure_ratio": best_cut_pressure.get("cut_sink_pressure_ratio", ""),
            "latest_mode": latest_mode.get("mapper_mode", ""),
            "latest_mode_gate": latest_mode.get("gate", ""),
            "latest_mode_aig": latest_mode.get("aig_id", ""),
            "latest_mode_phase": latest_mode.get("phase", ""),
        })
    write_csv(
        RESULTS / "selected_match_summary.csv",
        rows_out,
        [
            "benchmark", "selected_match_rows", "watched_aigs_seen",
            "mapper_modes_seen", "max_node_sink_pressure_ratio",
            "max_node_sink_pressure_aig", "max_cut_sink_pressure_ratio",
            "latest_mode", "latest_mode_gate", "latest_mode_aig",
            "latest_mode_phase",
        ],
    )


def write_phase_survival_summary(multi_stats_by_name):
    rows_out = []
    for bench in BENCHMARKS:
        rows = [row for row in multi_stats_by_name.get("phase_survival", []) if row["benchmark"] == bench]
        contexts = [row for row in multi_stats_by_name.get("phase_output_context", []) if row["benchmark"] == bench]
        row = rows[0] if rows else {}
        phase0_crit = _float_value(row, "phase0_best_criticality")
        phase1_crit = _float_value(row, "phase1_best_criticality")
        phase0_nodes = int(row.get("phase0_nodes", "0") or 0)
        phase1_nodes = int(row.get("phase1_nodes", "0") or 0)
        dominant_phase = ""
        if phase0_nodes or phase1_nodes:
            dominant_phase = "0" if phase0_crit >= phase1_crit else "1"
        best_context = max(contexts, key=lambda r: _float_value(r, "top_criticality"), default={})
        rows_out.append({
            "benchmark": bench,
            "aig_id": row.get("aig_id", ""),
            "final_nodes": row.get("final_nodes", ""),
            "both_phases_survived": int(phase0_nodes > 0 and phase1_nodes > 0),
            "dominant_final_phase": dominant_phase,
            "phase0_best_gate": row.get("phase0_best_gate", ""),
            "phase0_best_criticality": row.get("phase0_best_criticality", ""),
            "phase0_best_slack": row.get("phase0_best_slack", ""),
            "phase1_best_gate": row.get("phase1_best_gate", ""),
            "phase1_best_criticality": row.get("phase1_best_criticality", ""),
            "phase1_best_slack": row.get("phase1_best_slack", ""),
            "best_context_phase": best_context.get("phase", ""),
            "best_context_fanouts": best_context.get("final_fanouts", ""),
            "best_context_co_fanouts": best_context.get("co_fanouts", ""),
            "best_context_best_fanout_aig": best_context.get("best_fanout_aig_id", ""),
            "best_context_best_fanout_phase": best_context.get("best_fanout_phase", ""),
            "best_context_best_fanout_gate": best_context.get("best_fanout_gate", ""),
        })
    write_csv(
        RESULTS / "phase_survival_summary.csv",
        rows_out,
        [
            "benchmark", "aig_id", "final_nodes", "both_phases_survived",
            "dominant_final_phase", "phase0_best_gate", "phase0_best_criticality",
            "phase0_best_slack", "phase1_best_gate", "phase1_best_criticality",
            "phase1_best_slack", "best_context_phase", "best_context_fanouts",
            "best_context_co_fanouts", "best_context_best_fanout_aig",
            "best_context_best_fanout_phase", "best_context_best_fanout_gate",
        ],
    )


def main():
    RESULTS.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        futures = [pool.submit(run_flow, kind, bench) for bench in BENCHMARKS for kind in ("baseline", "candidate")]
        flow_results = [f.result() for f in concurrent.futures.as_completed(futures)]

    by_key = {(r["benchmark"], r["kind"]): r for r in flow_results}
    metrics_rows = []
    comparison_rows = []
    stats_by_name = {name: [] for name in STAT_PATTERNS}
    multi_stats_by_name = {name: [] for name in MULTI_STAT_PATTERNS}
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
        for name, parsed_rows in cand["multi_stats"].items():
            for parsed in parsed_rows:
                row = {"benchmark": bench}
                row.update(parsed)
                multi_stats_by_name[name].append(row)

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
    for name, fields in EMPTY_STAT_FIELDS.items():
        if not stats_by_name.get(name):
            write_csv(RESULTS / f"{name}.csv", [], fields)
    for name, rows in multi_stats_by_name.items():
        if rows:
            fields = ["benchmark"] + [k for k in rows[0].keys() if k != "benchmark"]
            write_csv(RESULTS / f"{name}.csv", rows, fields)
    for name, fields in EMPTY_MULTI_FIELDS.items():
        if not multi_stats_by_name.get(name):
            write_csv(RESULTS / f"{name}.csv", [], fields)
    write_path_proximity(multi_stats_by_name)
    write_final_critical_summary(multi_stats_by_name)
    write_selected_match_summary(multi_stats_by_name)
    write_phase_survival_summary(multi_stats_by_name)

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
