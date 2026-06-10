#!/usr/bin/env python3
"""
Generate 100 more patterns (numbered 100–199) into the same output folder,
without overriding the previously generated pattern_0 … pattern_99 files.

Patterns are found in parallel across NUM_WORKERS processes.
"""

import json
import os
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

import networkx as nx
from tqdm import tqdm

from find_pattern import find_pattern

INPUT_DIR  = Path(__file__).parent / "temp" / "input_color_uniform_deg_3"
OUTPUT_DIR = Path(__file__).parent / "temp" / "input_color_uniform_deg_3_patterns"
NUM_PATTERNS  = 100
START_IDX     = 100  # avoids overwriting 0-99
NUM_WORKERS   = os.cpu_count()
ALIVE_THRESHOLD = 0.01


# ── worker (module-level so it is picklable) ──────────────────────────────────

_worker_graphs: list = []
_worker_threshold: float = 0.0


def _init_worker(graphs: list, threshold: float) -> None:
    global _worker_graphs, _worker_threshold
    _worker_graphs = graphs
    _worker_threshold = threshold


def _find_one(pattern_idx: int) -> tuple:
    import random as _random
    _random.seed(pattern_idx)
    t0 = time.perf_counter()
    pattern, alive_indices = find_pattern(_worker_graphs, alive_threshold=_worker_threshold)
    duration = time.perf_counter() - t0
    return pattern_idx, pattern, alive_indices, duration


# ── helpers ───────────────────────────────────────────────────────────────────

def load_graph(path: Path) -> nx.Graph:
    with open(path, encoding="utf-8") as fh:
        data = json.load(fh)
    g = nx.Graph()
    for node in data["nodes"]:
        g.add_node(node["id"], color=node["color"])
    for link in data["links"]:
        g.add_edge(link["source"], link["target"])
    return g


def graph_to_json(g: nx.Graph) -> dict:
    return {
        "nodes": [{"id": v, "color": d["color"]} for v, d in g.nodes(data=True)],
        "links": [{"source": u, "target": v} for u, v in g.edges()],
    }


# ── main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    all_paths = sorted(INPUT_DIR.glob("*.json"))
    print(f"Found {len(all_paths)} graphs in {INPUT_DIR}")

    graphs: list[nx.Graph] = []
    for path in tqdm(all_paths, desc="loading graphs"):
        graphs.append(load_graph(path))

    log_path = OUTPUT_DIR / "durations_more.log"
    with open(log_path, "w", encoding="utf-8") as log:
        log.write("pattern_idx,duration_s,matched_graphs,pattern_nodes,pattern_edges\n")

        indices = range(START_IDX, START_IDX + NUM_PATTERNS)
        with ProcessPoolExecutor(
            max_workers=NUM_WORKERS,
            initializer=_init_worker,
            initargs=(graphs, ALIVE_THRESHOLD),
        ) as executor:
            futures = {executor.submit(_find_one, idx): idx for idx in indices}
            for future in tqdm(as_completed(futures), total=NUM_PATTERNS, desc="finding patterns"):
                pattern_idx, pattern, alive_indices, duration = future.result()

                matched_files = [all_paths[i].name for i in sorted(alive_indices)]
                out_path = OUTPUT_DIR / f"pattern_{pattern_idx}.json"
                with open(out_path, "w", encoding="utf-8") as fh:
                    json.dump(
                        {"pattern": graph_to_json(pattern), "matched_graphs": matched_files},
                        fh, indent=2,
                    )

                log.write(
                    f"{pattern_idx},{duration:.3f},{len(matched_files)},"
                    f"{pattern.number_of_nodes()},{pattern.number_of_edges()}\n"
                )
                log.flush()
                tqdm.write(
                    f"pattern {pattern_idx:3d}  {duration:6.2f}s  "
                    f"{len(matched_files)} graphs  "
                    f"{pattern.number_of_nodes()}v {pattern.number_of_edges()}e"
                )

    print(f"Done — patterns {START_IDX}–{START_IDX + NUM_PATTERNS - 1} written to {OUTPUT_DIR}")
    print(f"Durations logged to {log_path}")


if __name__ == "__main__":
    main()
