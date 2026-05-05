#!/usr/bin/env python3
"""
Compare FANMODPlus results against SGF MotifPreprocessor results.

Keys differ between the two systems (different canonical encodings), so
comparison is done on VALUES only: the sorted list of per-motif counts must
be identical for the two implementations to agree.

Additionally checks:
  - Number of distinct colored motifs is equal.
  - Total subgraph count is equal.
"""

import json
import sys
from collections import Counter
from itertools import product
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
FANMOD_DIR = (
    SCRIPT_DIR.parent.parent
    / "new-graph-measures"
    / "local_tests"
    / "other_algorithms_tests"
    / "fanmod_plus"
)
SGF_DIR = SCRIPT_DIR / "sgf"

DENSITIES     = [5, 8, 10, 13, 15]
DISTRIBUTIONS = ["uniform", "average", "rare"]


def load_fanmod(den, dist):
    """Load FANMODPlus motif counts. Returns list of int counts, or None if missing."""
    path = FANMOD_DIR / f"g_den_{den}_embedded_den_3_{dist}_0.json"
    if not path.is_file():
        return None
    with open(path) as fh:
        raw = json.load(fh)
    motifs = raw["motifs"] if "motifs" in raw else raw
    return list(motifs.values())


def load_sgf(den, dist):
    """Load SGF motif counts. Returns (counts_list, total_subgraphs) or (None, None)."""
    path = SGF_DIR / f"g_den_{den}_embedded_den_3_{dist}_0.json"
    if not path.is_file():
        return None, None
    with open(path) as fh:
        raw = json.load(fh)
    counts = list(raw.get("motifs", {}).values())
    return counts, raw.get("total_subgraphs")


def diff_sorted(vdmc_counts, sgf_counts):
    """
    Return (only_in_vdmc, only_in_sgf) as Counter differences.
    Both inputs are lists of counts; comparison is on the multiset.
    """
    vc = Counter(vdmc_counts)
    sc = Counter(sgf_counts)
    only_vdmc = vc - sc
    only_sgf  = sc - vc
    return only_vdmc, only_sgf


def main():
    if not FANMOD_DIR.exists():
        print(f"ERROR: FANMODPlus results not found: {FANMOD_DIR}", file=sys.stderr)
        sys.exit(1)
    if not SGF_DIR.exists():
        print(f"ERROR: SGF results not found: {SGF_DIR}", file=sys.stderr)
        print("Run benchmark_sgf.py first.", file=sys.stderr)
        sys.exit(1)

    all_match = True
    for den, dist in product(DENSITIES, DISTRIBUTIONS):
        label = f"g_den_{den}_embedded_den_3_{dist}_0"
        print(f"\n{'='*60}")
        print(f"Comparing: {label}")

        fanmod_counts = load_fanmod(den, dist)
        sgf_counts, sgf_total = load_sgf(den, dist)

        if fanmod_counts is None:
            print("  SKIP: FANMODPlus output not found")
            continue
        if sgf_counts is None:
            print("  SKIP: SGF output not found")
            continue

        fanmod_total = sum(fanmod_counts)
        sgf_total_val = sgf_total if sgf_total is not None else sum(sgf_counts)

        print(f"  FANMODPlus: {len(fanmod_counts)} motifs, total subgraphs = {fanmod_total}")
        print(f"  SGF:        {len(sgf_counts)} motifs, total subgraphs = {sgf_total_val}")

        if sorted(fanmod_counts) == sorted(sgf_counts):
            print("  MATCH: all motif counts agree")
        else:
            all_match = False
            only_fanmod, only_sgf = diff_sorted(fanmod_counts, sgf_counts)
            total_diff = sum(only_fanmod.values()) + sum(only_sgf.values())
            print(f"  MISMATCH: {total_diff} count occurrences differ")
            shown = 0
            print(f"  {'Count':>10}  {'FANMOD+ copies':>15}  {'SGF copies':>10}")
            print(f"  {'-'*40}")
            for count in sorted(set(only_fanmod) | set(only_sgf)):
                fc = only_fanmod.get(count, 0)
                sc = only_sgf.get(count, 0)
                print(f"  {count:>10}  {fc:>15}  {sc:>10}")
                shown += 1
                if shown >= 20:
                    remaining = len(set(only_fanmod) | set(only_sgf)) - shown
                    if remaining > 0:
                        print(f"  ... and {remaining} more distinct count values")
                    break

    print(f"\n{'='*60}")
    print("Overall:", "ALL MATCH" if all_match else "MISMATCHES FOUND")


if __name__ == "__main__":
    main()
