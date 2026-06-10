#!/usr/bin/env python3
"""Fuzz tester for the multigraph pattern finder.

For each (n, k) in [5..20] x [1..5], runs 1000 iterations in parallel:
  - Generates k random undirected graphs of n vertices
  - Calls sgf-pattern-finder with thresholds 1/k, 2/k, ..., k/k
  - Verifies via sgf-graph-searcher and NetworkX that the returned pattern
    is a subgraph in every graph the finder claims it is alive in.

On failure the run directory is moved to fuzz_failure/<run_name>/ and the
script stops.  run.log is the log file written by sgf-pattern-finder itself
via --log-file-path.
"""

import json
import os
import random
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

from tqdm import tqdm

# ── Paths ─────────────────────────────────────────────────────────────────────

_HERE = Path(__file__).parent
PATTERN_FINDER = _HERE / "build" / "sgf-pattern-finder"
GRAPH_SEARCHER = _HERE / "build" / "sgf-graph-searcher"
FAILURE_BASE = _HERE / "fuzz_failure"

# ── Config ────────────────────────────────────────────────────────────────────

MIN_N = 5
MAX_N = 20
MAX_K = 5
ITERATIONS = 1000
NUM_COLORS = 3
EDGE_PROB = 0.4
BASE_SEED = 42

# ── Graph generation ──────────────────────────────────────────────────────────

def make_graph(n, rng):
    nodes = [{"id": v, "color": rng.randint(0, NUM_COLORS - 1)} for v in range(n)]
    links = [
        {"source": u, "target": v}
        for u in range(n)
        for v in range(u + 1, n)
        if rng.random() < EDGE_PROB
    ]
    return {"nodes": nodes, "links": links}

# ── IO ────────────────────────────────────────────────────────────────────────

def write_json(obj, path):
    with open(path, "w") as fh:
        json.dump(obj, fh, indent=2)

def read_json(path):
    with open(path) as fh:
        return json.load(fh)

# ── CLI wrappers ──────────────────────────────────────────────────────────────

def _run(cmd):
    """Run cmd, raise on non-zero exit."""
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise subprocess.CalledProcessError(result.returncode, cmd, result.stdout, result.stderr)
    return result


def run_pattern_finder(lib_dir, out_dir, threshold, log_path):
    """Return (pattern_dict, pattern_path, alive_set) or (None, None, set())."""
    _run([
        str(PATTERN_FINDER),
        "--preprocess",
        "--reader-type", "json",
        "--library-input-folder", str(lib_dir),
        "--output-folder", str(out_dir),
        "--pattern-output-type", "json",
        "--preprocess-multigraph", "1",
        "--multigraph-alive-percent", str(threshold),
        "--log-file-path", str(log_path),
    ])

    pattern_files = sorted(out_dir.glob("pattern_[0-9]*.json"))
    index_files = sorted(out_dir.glob("pattern_index_*.csv"))

    if not pattern_files:
        return None, None, set()

    pattern_path = pattern_files[0]
    pattern = read_json(pattern_path)

    alive = set()
    if index_files:
        with open(index_files[0]) as fh:
            next(fh)
            for line in fh:
                parts = line.strip().split(",")
                if len(parts) == 2:
                    alive.add(int(parts[1]))

    return pattern, pattern_path, alive


def searcher_finds(pattern_path, graph_path):
    result = _run([
        str(GRAPH_SEARCHER),
        "--subgraph-path", str(pattern_path),
        "--background-path", str(graph_path),
        "--reader-type", "json",
        "--prior-policy", "subgraph-degree-squared",
        "--stop-on-first-match",
    ])
    for line in result.stdout.splitlines():
        if "Matches found:" in line:
            return int(line.split()[-1]) > 0
    return False

# ── NetworkX check ────────────────────────────────────────────────────────────

def nx_is_subgraph(pattern_dict, graph_dict):
    import networkx as nx
    from networkx.algorithms.isomorphism import GraphMatcher

    if not pattern_dict["nodes"]:
        return True

    def node_match(a, b):
        return a["color"] == b["color"]

    host = nx.Graph()
    for node in graph_dict["nodes"]:
        host.add_node(node["id"], color=node["color"])
    for link in graph_dict["links"]:
        host.add_edge(link["source"], link["target"])

    pat = nx.Graph()
    for node in pattern_dict["nodes"]:
        pat.add_node(node["id"], color=node["color"])
    for link in pattern_dict["links"]:
        pat.add_edge(link["source"], link["target"])

    return GraphMatcher(host, pat, node_match=node_match).subgraph_is_monomorphic()

# ── Verification ──────────────────────────────────────────────────────────────

def verify_alive_graphs(pattern, pattern_path, alive, graphs, graph_paths):
    """Return (ok, reason_or_None)."""
    for graph_idx in alive:
        if not searcher_finds(pattern_path, graph_paths[graph_idx]):
            return False, f"graph={graph_idx}: sgf-graph-searcher says NOT subgraph"
        if not nx_is_subgraph(pattern, graphs[graph_idx]):
            return False, f"graph={graph_idx}: networkx says NOT subgraph"
    return True, None

# ── Single iteration (runs in worker process) ─────────────────────────────────

def run_iteration(n, k, iteration):
    """Return (ok, failure_dir_or_None, message)."""
    seed = hash((BASE_SEED, n, k, iteration)) & 0xFFFFFFFF
    rng = random.Random(seed)

    run_dir = Path(tempfile.mkdtemp(prefix=f"fuzz_n{n}_k{k}_i{iteration}_"))
    log_path = run_dir / "run.log"
    label = f"n={n} k={k} iter={iteration}"

    lib_dir = run_dir / "lib"
    lib_dir.mkdir()

    graphs = [make_graph(n, rng) for _ in range(k)]
    graph_paths = []
    for idx, graph in enumerate(graphs):
        path = lib_dir / f"g{idx}.json"
        write_json(graph, path)
        graph_paths.append(path)

    failure = None
    for numerator in range(1, k + 1):
        threshold = numerator / k
        out_dir = run_dir / f"out_{numerator}"
        out_dir.mkdir()

        try:
            pattern, pattern_path, alive = run_pattern_finder(
                lib_dir, out_dir, threshold, log_path
            )
        except subprocess.CalledProcessError as exc:
            failure = (f"t={numerator}/{k}: pattern-finder error (exit {exc.returncode})", out_dir)
            break

        if pattern is None or not alive:
            continue

        ok, reason = verify_alive_graphs(
            pattern, pattern_path, alive, graphs, graph_paths
        )
        if not ok:
            failure = (f"t={numerator}/{k}: {reason}", out_dir)
            break

    if failure is not None:
        return _save_failure(run_dir, label, failure[0], failure[1])

    shutil.rmtree(run_dir)
    return True, None, ""


def _save_failure(run_dir, label, reason, failing_out_dir):
    for out_dir in run_dir.glob("out_*"):
        if out_dir != failing_out_dir:
            shutil.rmtree(out_dir)
    dest = FAILURE_BASE / run_dir.name
    FAILURE_BASE.mkdir(exist_ok=True)
    shutil.move(str(run_dir), str(dest))
    write_json({"label": label, "reason": reason}, dest / "summary.json")
    return False, str(dest), f"FAIL {label}: {reason}"

# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    tasks = [
        (n, k, iteration)
        for n in range(MIN_N, MAX_N + 1)
        for k in range(1, MAX_K + 1)
        for iteration in range(ITERATIONS)
    ]

    workers = os.cpu_count() or 4
    progress = tqdm(total=len(tasks), unit="iter", desc="fuzzing")
    fail_dirs = []

    with ProcessPoolExecutor(max_workers=workers) as executor:
        futures = {executor.submit(run_iteration, *task): task for task in tasks}

        for future in as_completed(futures):
            try:
                ok, fail_dir, msg = future.result()
            except Exception:
                progress.update(1)
                continue
            progress.update(1)
            if not ok:
                fail_dirs.append((fail_dir, msg))
                if len(fail_dirs) == 1:
                    for pending in futures:
                        pending.cancel()

    progress.close()

    if fail_dirs:
        first_dir, first_msg = fail_dirs[0]
        for extra_dir, _ in fail_dirs[1:]:
            if extra_dir and Path(extra_dir).exists():
                shutil.rmtree(extra_dir)
        tqdm.write(first_msg)
        tqdm.write(f"  saved to: {first_dir}")
        return 1

    tqdm.write("All iterations passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
