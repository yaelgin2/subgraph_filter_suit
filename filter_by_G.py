#!/usr/bin/env python3
"""
For each pattern in PATTERNS_DIR:
  1. Run sgf-graph-searcher to check if the pattern is a subgraph of G_induced.
  2. If the pattern is NOT in G, mark every S graph that contains it as not_in_G.

Searcher calls run in parallel across NUM_WORKERS threads (subprocess work
releases the GIL, so threads give true parallelism here).

Output: OUTPUT_CSV with columns
  file_name   – S graph filename
  not_in_G    – True if any of its patterns were absent from G
  num_patterns – how many patterns list this S graph in their matched_graphs
"""

import csv
import json
import os
import subprocess
import tempfile
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from tqdm import tqdm

PATTERNS_DIR = Path("temp/input_color_rare_deg_3_patterns")
G_PATH       = Path("temp/input_color_rare_deg_3_background_graphs/G_induced.json")
S_DIR        = Path("temp/input_color_rare_deg_3")
SEARCHER     = Path("build/sgf-graph-searcher")
OUTPUT_CSV   = Path("temp/filter_results_rare.csv")
NUM_WORKERS  = os.cpu_count()

# sgf-graph-searcher exits 11 (MatchFoundException) when a match is found,
# 0 when no match is found.
EXIT_MATCH_FOUND = 11


def _search_one(args: tuple) -> tuple[str, list[str], bool, int]:
    """Run one pattern search in G. Returns (pattern_name, matched_graphs, in_G, exit_code)."""
    pf, tmpdir = args
    with open(pf, encoding="utf-8") as fh:
        data = json.load(fh)

    pattern_tmp = Path(tmpdir) / pf.name
    with open(pattern_tmp, "w", encoding="utf-8") as fh:
        json.dump(data["pattern"], fh)

    result = subprocess.run(
        [
            str(SEARCHER),
            "--subgraph-path",   str(pattern_tmp),
            "--background-path", str(G_PATH),
            "--reader-type",     "json",
            "--prior-policy",    "subgraph-degree-squared",
            "--induced",
            "--stop-on-first-match",
        ],
        capture_output=True,
    )

    in_G = result.returncode == EXIT_MATCH_FOUND
    return pf.name, data["matched_graphs"], in_G, result.returncode


def main() -> None:
    pattern_files = sorted(PATTERNS_DIR.glob("pattern_*.json"))
    print(f"Patterns: {len(pattern_files)}  |  G: {G_PATH}  |  workers: {NUM_WORKERS}")

    s_not_in_G: set[str] = set()
    s_pattern_count: defaultdict[str, int] = defaultdict(int)

    with tempfile.TemporaryDirectory() as tmpdir:
        with ThreadPoolExecutor(max_workers=NUM_WORKERS) as executor:
            futures = {
                executor.submit(_search_one, (pf, tmpdir)): pf
                for pf in pattern_files
            }
            for future in tqdm(as_completed(futures), total=len(pattern_files), desc="searching in G"):
                pf = futures[future]
                pattern_name, matched_graphs, in_G, exit_code = future.result()

                if exit_code not in (0, EXIT_MATCH_FOUND):
                    tqdm.write(f"WARNING: exit {exit_code} for {pf.name}")
                    continue

                for sg in matched_graphs:
                    s_pattern_count[sg] += 1

                if not in_G:
                    for sg in matched_graphs:
                        s_not_in_G.add(sg)

    all_s = sorted(f.name for f in S_DIR.glob("*.json"))

    OUTPUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    with open(OUTPUT_CSV, "w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["file_name", "not_in_G", "num_patterns"])
        for sg in all_s:
            writer.writerow([sg, sg in s_not_in_G, s_pattern_count[sg]])

    filtered = sum(1 for sg in all_s if sg in s_not_in_G)
    print(f"Wrote {len(all_s)} rows → {OUTPUT_CSV}")
    print(f"not_in_G: {filtered}/{len(all_s)}")


if __name__ == "__main__":
    main()
