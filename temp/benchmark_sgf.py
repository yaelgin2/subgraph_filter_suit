#!/usr/bin/env python3
"""
Run SGF MotifPreprocessor on 15 test graphs and save raw motif counts as JSON.

Input graphs are read directly from the new-graph-measures JSON files
(nodes/links format) — no conversion needed.

Output per graph (saved to sgf/):
  {
    "motifs": {"<128-bit key as decimal string>": count, ...},
    "total_subgraphs": N,
    "total_motifs": M
  }

Timing log: logs/sgf.log (CSV: graph,time_seconds,num_colored_motifs,status)
Per-graph C++ progress log: logs/<graph_name>.log  (written by FileLogger inside binary)
"""

import json
import subprocess
import sys
import time
from datetime import datetime
from itertools import product
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
GRAPHS_DIR = SCRIPT_DIR.parent.parent / "new-graph-measures" / "local_tests" / "graphs_by_density_3"
SGF_BINARY = SCRIPT_DIR.parent / "build" / "sgf_motif_counter"
OUTPUT_DIR = SCRIPT_DIR / "sgf"
LOGS_DIR   = SCRIPT_DIR / "logs"
TIMING_LOG = LOGS_DIR / "sgf.log"

DENSITIES     = [5, 8, 10, 13, 15]
DISTRIBUTIONS = ["uniform", "average", "rare"]
TOTAL_GRAPHS  = len(DENSITIES) * len(DISTRIBUTIONS)


def timestamp():
    return datetime.now().strftime("%H:%M:%S")


def check_prerequisites():
    if not SGF_BINARY.exists():
        print(f"[{timestamp()}] ERROR: binary not found: {SGF_BINARY}", flush=True)
        print("Build first: cmake --build build --config Release", flush=True)
        sys.exit(1)
    if not GRAPHS_DIR.exists():
        print(f"[{timestamp()}] ERROR: graphs directory not found: {GRAPHS_DIR}", flush=True)
        sys.exit(1)


def run_sgf(graph_path, cpp_log_path):
    """Call sgf_motif_counter; return (parsed_data, error_str)."""
    result = subprocess.run(
        [str(SGF_BINARY), str(graph_path), str(cpp_log_path)],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return None, result.stderr.strip()
    try:
        return json.loads(result.stdout), None
    except json.JSONDecodeError as exc:
        return None, str(exc)


def process_graph(den, dist, idx, timing_log):
    """Run SGF on one graph; save output and update timing log."""
    graph_name  = f"g_den_{den}_embedded_den_3_{dist}_0"
    graph_path  = GRAPHS_DIR / f"{graph_name}.json"
    output_path = OUTPUT_DIR / f"{graph_name}.json"
    cpp_log     = LOGS_DIR / f"{graph_name}.log"

    print(f"[{timestamp()}] [{idx}/{TOTAL_GRAPHS}] Starting {graph_name} ...", flush=True)

    if not graph_path.exists():
        print(f"[{timestamp()}]   SKIP — input file not found: {graph_path}", flush=True)
        timing_log.write(f"{graph_name},0,0,missing_input\n")
        timing_log.flush()
        return

    start = time.time()
    data, error = run_sgf(graph_path, cpp_log)
    elapsed = time.time() - start

    if data is None:
        print(f"[{timestamp()}]   ERROR after {elapsed:.1f}s — {error}", flush=True)
        timing_log.write(f"{graph_name},{elapsed:.1f},0,error\n")
        timing_log.flush()
        return

    with open(output_path, "w") as out:
        json.dump(data, out, indent=2)

    num_motifs = data.get("total_motifs", len(data.get("motifs", {})))
    total_sg   = data.get("total_subgraphs", "?")

    print(
        f"[{timestamp()}]   Done in {elapsed:.1f}s — "
        f"{num_motifs} colored motifs, {total_sg} total subgraphs",
        flush=True,
    )
    print(f"[{timestamp()}]   C++ log: {cpp_log}", flush=True)

    timing_log.write(f"{graph_name},{elapsed:.1f},{num_motifs},ok\n")
    timing_log.flush()


def main():
    check_prerequisites()
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    LOGS_DIR.mkdir(parents=True, exist_ok=True)

    print(f"[{timestamp()}] SGF benchmark — {TOTAL_GRAPHS} graphs", flush=True)
    print(f"[{timestamp()}] Graphs dir : {GRAPHS_DIR}", flush=True)
    print(f"[{timestamp()}] Output dir : {OUTPUT_DIR}", flush=True)
    print(f"[{timestamp()}] Timing log : {TIMING_LOG}", flush=True)
    print(flush=True)

    overall_start = time.time()

    with open(TIMING_LOG, "w") as timing_log:
        timing_log.write("graph,time_seconds,num_colored_motifs,status\n")

        for idx, (den, dist) in enumerate(product(DENSITIES, DISTRIBUTIONS), start=1):
            process_graph(den, dist, idx, timing_log)
            print(flush=True)

    overall_elapsed = time.time() - overall_start
    print(f"[{timestamp()}] Finished all {TOTAL_GRAPHS} graphs in {overall_elapsed:.1f}s total", flush=True)
    print(f"[{timestamp()}] Results  : {OUTPUT_DIR}", flush=True)
    print(f"[{timestamp()}] Timing   : {TIMING_LOG}", flush=True)


if __name__ == "__main__":
    main()
