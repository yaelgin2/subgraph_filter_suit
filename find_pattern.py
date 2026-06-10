#!/usr/bin/env python3
"""
Pure-Python multigraph pattern finder.

Match state:  matches[graph_idx] = list of dicts {pattern_vertex: graph_vertex}.
Expansion:    for every match, look at neighbours of each matched graph vertex
              that are not already in the match; accumulate by (colour, pattern_vertex).
"""

import random
from collections import Counter
from itertools import combinations
from typing import Optional

import networkx as nx

MAX_MATCHES_PER_GRAPH = 2000  # cap to prevent memory blowup on dense graphs


def _remap_colors(graphs: list[nx.Graph]) -> dict[int, int]:
    """Remap node colors to compact 0-based IDs in-place; return the mapping."""
    colors = sorted({d["color"] for g in graphs for _, d in g.nodes(data=True)})
    color_map = {c: i for i, c in enumerate(colors)}
    for g in graphs:
        for v in g.nodes:
            g.nodes[v]["color"] = color_map[g.nodes[v]["color"]]
    return color_map


def _revert_colors(graph: nx.Graph, color_map: dict[int, int]) -> None:
    inv = {v: k for k, v in color_map.items()}
    for v in graph.nodes:
        graph.nodes[v]["color"] = inv[graph.nodes[v]["color"]]


def _select_first_color(graphs: list[nx.Graph]) -> int:
    freq: Counter = Counter(d["color"] for g in graphs for _, d in g.nodes(data=True))
    return freq.most_common(1)[0][0]


def _count_candidates(
    graphs: list[nx.Graph],
    alive: set[int],
    matches: list[list[dict[int, int]]],
) -> Counter:
    """Return Counter({(colour, pattern_vertex): total extension count})."""
    counts: Counter = Counter()
    for gi in alive:
        g = graphs[gi]
        for match in matches[gi]:
            matched_gverts = set(match.values())
            for pv, gv in match.items():
                for nb in g.neighbors(gv):
                    if nb not in matched_gverts:
                        counts[(g.nodes[nb]["color"], pv)] += 1
    return counts


def _extend_matches(
    graphs: list[nx.Graph],
    matches: list[list[dict[int, int]]],
    connect_pv: int,
    new_pv: int,
    new_color: int,
) -> list[list[dict[int, int]]]:
    extended = []
    for gi, graph_matches in enumerate(matches):
        g = graphs[gi]
        new_graph_matches: list[dict[int, int]] = []
        seen: set[frozenset] = set()
        for match in graph_matches:
            matched_gverts = set(match.values())
            for nb in g.neighbors(match[connect_pv]):
                if nb not in matched_gverts and g.nodes[nb]["color"] == new_color:
                    new_match = {**match, new_pv: nb}
                    key = frozenset(new_match.items())
                    if key not in seen:
                        seen.add(key)
                        new_graph_matches.append(new_match)
        if len(new_graph_matches) > MAX_MATCHES_PER_GRAPH:
            new_graph_matches = random.sample(new_graph_matches, MAX_MATCHES_PER_GRAPH)
        extended.append(new_graph_matches)
    return extended


def _best_candidate_edge(
    pattern: nx.Graph,
    graphs: list[nx.Graph],
    alive: set[int],
    matches: list[list[dict[int, int]]],
) -> tuple[Optional[int], Optional[int], int]:
    best_src, best_tgt, best_score = None, None, 0
    for src, tgt in combinations(sorted(pattern.nodes), 2):
        if pattern.has_edge(src, tgt):
            continue
        score = sum(
            1 for gi in alive
            if any(
                src in m and tgt in m and graphs[gi].has_edge(m[src], m[tgt])
                for m in matches[gi]
            )
        )
        if score > best_score:
            best_score, best_src, best_tgt = score, src, tgt
    return best_src, best_tgt, best_score


def find_pattern(
    input_graphs: list[nx.Graph],
    alive_threshold: float = 0.0,
) -> tuple[nx.Graph, set[int]]:
    """
    Grow a pattern common to all input graphs.

    :param input_graphs: List of NetworkX graphs with integer 'color' node attributes.
    :param alive_threshold: Fraction of graphs that must remain alive (0.0 = all).
    :return: Tuple of (pattern graph, set of input_graphs indices that contain the pattern).
    """
    graphs = [g.copy() for g in input_graphs]
    color_map = _remap_colors(graphs)
    num_graphs = len(graphs)
    min_support = max(1, int(num_graphs * alive_threshold))

    first_color = _select_first_color(graphs)
    pattern = nx.Graph()
    pattern.add_node(0, color=first_color)

    matches: list[list[dict[int, int]]] = [
        [{0: v} for v, d in g.nodes(data=True) if d["color"] == first_color]
        for g in graphs
    ]
    alive: set[int] = {i for i in range(num_graphs) if matches[i]}
    done_vertices = False
    failed_add_edge = False

    while len(alive) >= alive_threshold * num_graphs:
        # Mirrors run_one_growth_step: probability of attempting a vertex decreases
        # as the pattern grows — 1 / cbrt(num_pattern_vertices).
        vertex_add_probability = 1.0 / (len(pattern.nodes) ** (1.0 / 3.0))
        try_vertex = (random.random() < vertex_add_probability and not done_vertices) or failed_add_edge

        if try_vertex:
            counts = _count_candidates(graphs, alive, matches)
            added_vertex = False
            if counts:
                keys = list(counts.keys())
                weights = [counts[k] for k in keys]
                (new_color, connect_pv) = random.choices(keys, weights=weights, k=1)[0]
                if counts[(new_color, connect_pv)] >= min_support:
                    new_pv = len(pattern.nodes)
                    pattern.add_node(new_pv, color=new_color)
                    pattern.add_edge(connect_pv, new_pv)
                    matches = _extend_matches(graphs, matches, connect_pv, new_pv, new_color)
                    alive = {i for i in range(num_graphs) if matches[i]}
                    added_vertex = True
            if not added_vertex:
                done_vertices = True
            failed_add_edge = False  # reset regardless, same as C++
        else:
            src, tgt, score = _best_candidate_edge(pattern, graphs, alive, matches)
            if src is not None and score >= min_support:
                pattern.add_edge(src, tgt)
                matches = [
                    [
                        m for m in matches[gi]
                        if src in m and tgt in m and graphs[gi].has_edge(m[src], m[tgt])
                    ]
                    for gi in range(num_graphs)
                ]
                alive = {i for i in range(num_graphs) if matches[i]}
                failed_add_edge = False
            else:
                failed_add_edge = True

        if done_vertices and failed_add_edge:
            break

    _revert_colors(pattern, color_map)
    return pattern, alive
