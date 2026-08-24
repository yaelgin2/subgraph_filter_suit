# Examples

Worked, verified example commands for every CLI tool and mode in this repo. All commands below
were actually run against a Release build (`cmake -S . -B build && cmake --build build --config
Release --parallel`, see the [main README](../README.md)) and the outputs shown are real, not
hypothetical. Run everything from the repo root.

Each example uses **one file format** — collectively the three examples below cover all three
formats the tools support (`json`, `vertex-edge`, `graphml`); no example repeats a format another
one already demonstrates.

## Example data

### `json_graph/` + `json_library/` — JSON format, used for `sgf-pattern-finder`

- `json_graph/graph.json` — one 10-vertex background graph (a cycle plus two chords, 4 colors,
  max degree 3, triangle-free).
- `json_library/` — 5 small candidate graphs (3–5 vertices):
  - `lib_1_in_path3.json` — a genuine induced subgraph of `json_graph` (vertices 0,1,2), so it
    **is** contained in it.
  - `lib_2_not_in_rare_color_edge.json` — requires an edge between two color-3 vertices; `json_graph`
    has only one color-3 vertex, so no such edge can exist.
  - `lib_3_not_in_missing_color.json` — requires a vertex colored 9, a color that never appears in
    `json_graph`.
  - `lib_4_not_in_triangle.json` — a triangle; `json_graph` is triangle-free.
  - `lib_5_not_in_high_degree.json` — a degree-4 star; `json_graph`'s max degree is 3.

### `large_graph_ve/` + `large_library_ve/` — vertex-edge format, used for `sgf-graph-enumerator`

- `large_graph_ve/graph.{node_labels,edges}` — one 100-vertex background graph (cycle + 100 random
  chords, 6 colors, max degree 8).
- `large_library_ve/` — 10 candidate graphs sized 10–40 vertices:
  - `lib_1_in_size15`, `lib_2_in_size30` — genuine induced subgraphs of `large_graph_ve` (BFS-sampled
    connected vertex subsets), so they **are** contained in it.
  - `lib_3/5/7/9_not_in_size*` (odd-numbered) — each requires a vertex colored 6, a color that never
    appears in `large_graph_ve`.
  - `lib_4/6/8/10_not_in_size*` (even-numbered) — each is a star whose center degree exceeds
    `large_graph_ve`'s max degree of 8, so no background vertex has enough neighbours to match.

### `graph_graphml/` + `in_graph_library_graphml/` + `not_in_graph_library_graphml/` — GraphML, used for `sgf-graph-searcher`

- `graph_graphml/graph.graphml` — a 12-vertex background graph (cycle + 2 chords, 4 string colors,
  triangle-free), demonstrating GraphML's string-label colors (`red`/`green`/`blue`/`yellow`, not
  raw integers — see the [GraphML color maps](../README.md#graphml-color-maps) section of the main
  README for why this is a distinct color model from JSON/vertex-edge).
- `in_graph_library_graphml/in.graphml` — a genuine 5-vertex induced subgraph of `graph_graphml`.
- `not_in_graph_library_graphml/not_in.graphml` — a triangle; `graph_graphml` is triangle-free.

### `artificial_results.json`

A hand-written filter-results file in the same format `sgf-graph-enumerator --filter` produces
(`{graph_stem: bool}`, `true` = pruned/skip, `false` = keep), used by example 6 below to drive
`--preprocess-single-graph-from-results` without needing a prior filter run. It marks
`lib_1_in_path3` as kept and the other four `json_library` graphs as pruned.

All graph data above was generated deterministically (fixed random seed); regenerate it with
whatever script you like — the important part is the "in"/"not in" guarantees are enforced
structurally (missing colors, triangle-freeness, degree bounds), not just asserted.

---

## 1. Motif enumeration (`sgf-graph-enumerator --preprocess --motifs`)

```bash
./build/sgf-graph-enumerator \
  --preprocess --motifs \
  --reader-type vertex-edge \
  --library-dir ./examples/large_library_ve \
  --cache-dir ./examples/output/motif_enum \
  --cache-type csv
```
Writes one file per candidate graph — `./examples/output/motif_enum/motif_cache_<graph>.csv` — each
holding one row per (4-vertex motif type, occurrence count) for that graph.

## 2. Path enumeration (`sgf-graph-enumerator --preprocess --paths`)

```bash
./build/sgf-graph-enumerator \
  --preprocess --paths \
  --reader-type vertex-edge \
  --library-dir ./examples/large_library_ve \
  --cache-dir ./examples/output/path_enum \
  --cache-type csv
```
Same idea, but 5-vertex simple paths instead of 4-vertex motifs.

## 3. Motif filtering (`sgf-graph-enumerator --filter --motifs`)

`--library-dir` above holds the *candidates* (the 10 `large_library_ve` graphs); `--filter` then
checks, for a given search graph (`--graph-dir`), which candidates **could** be subgraphs of it —
that's why `--graph-dir` here is `large_graph_ve`, not the other way around.

```bash
./build/sgf-graph-enumerator \
  --filter --motifs \
  --graph-dir ./examples/large_graph_ve \
  --graph-input-type vertex-edge \
  --motif-cache-dir ./examples/output/motif_enum \
  --cache-type csv \
  --result-folder ./examples/output/motif_filter \
  --result-type json
```

**Verified result** — exactly matches the design:
```json
{
  "lib_1_in_size15": false, "lib_2_in_size30": false,
  "lib_3_not_in_size10": true, "lib_4_not_in_size14": true,
  "lib_5_not_in_size18": true, "lib_6_not_in_size22": true,
  "lib_7_not_in_size26": true, "lib_8_not_in_size30": true,
  "lib_9_not_in_size35": true, "lib_10_not_in_size40": true
}
```
(`true` = pruned/cannot be a subgraph, `false` = survives/could be one.) Both genuine subgraphs
survive; all 8 non-matching candidates are correctly pruned.

## 4. Path filtering (`sgf-graph-enumerator --filter --paths`)

```bash
./build/sgf-graph-enumerator \
  --filter --paths \
  --graph-dir ./examples/large_graph_ve \
  --graph-input-type vertex-edge \
  --path-cache-dir ./examples/output/path_enum \
  --cache-type csv \
  --result-folder ./examples/output/path_filter \
  --result-type json
```

**Verified result** — instructive: only 4 of the 8 non-matching candidates get pruned here:
```json
{
  "lib_1_in_size15": false, "lib_2_in_size30": false,
  "lib_3_not_in_size10": true, "lib_5_not_in_size18": true,
  "lib_7_not_in_size26": true, "lib_9_not_in_size35": true,
  "lib_4_not_in_size14": false, "lib_6_not_in_size22": false,
  "lib_8_not_in_size30": false, "lib_10_not_in_size40": false
}
```
The even-numbered candidates (`lib_4`/`6`/`8`/`10`) are star graphs — a star's diameter is 2, so it
has **zero** 5-vertex simple paths, meaning the path filter has nothing to compare and can't rule
them out on its own (motif filtering, example 3, already did). This is exactly why the pipeline
uses multiple filter stages: no single signature is a strong enough necessary condition alone.

> Note: a candidate with an empty enumeration result (like these stars, for paths) still gets a
> `motif_number=0, appearances=0` sentinel row in the CSV cache, so it appears in filter results as
> `false` rather than silently vanishing from the output entirely.

## 5. Single-graph pattern finder, by index (`sgf-pattern-finder --preprocess --preprocess-single-graph`)

```bash
./build/sgf-pattern-finder \
  --preprocess \
  --reader-type json \
  --library-input-folder ./examples/json_library \
  --output-folder ./examples/output/pattern_single \
  --pattern-output-type json \
  --preprocess-single-graph 0 \
  --background-graph-path ./examples/json_graph/graph.json \
  --score-threshold 0.0
```
`--preprocess-single-graph 0` selects the first library graph by index (`lib_1_in_path3.json`,
directory-listing order) and runs beam-search pattern extraction scored against `json_graph`.
Verified: writes `pattern_index_<timestamp>.csv` plus one `pattern_N_<timestamp>.json` per
extracted pattern.

## 6. Single-graph pattern finder, from a results file (`--preprocess-single-graph-from-results`)

Same as example 5, but which library graph(s) to process is derived from a filter-results file
instead of a literal index — the realistic pipeline use is feeding in the output of a `--filter`
step (examples 3/4) so you only spend beam-search time on candidates that survived filtering.
`examples/artificial_results.json` hand-crafts that input instead of running a prior filter step:

```bash
./build/sgf-pattern-finder \
  --preprocess \
  --reader-type json \
  --library-input-folder ./examples/json_library \
  --output-folder ./examples/output/pattern_single_from_results \
  --pattern-output-type json \
  --preprocess-single-graph-from-results \
  --results-file-path ./examples/artificial_results.json \
  --results-file-type json \
  --background-graph-path ./examples/json_graph/graph.json \
  --score-threshold 0.0
```
Verified: selects the same graph (`lib_1_in_path3`, index 0) as example 5, since
`artificial_results.json` marks it `false` (kept) and everything else `true` (pruned).

## 7. Multigraph pattern finder (`sgf-pattern-finder --preprocess --preprocess-multigraph`)

```bash
./build/sgf-pattern-finder \
  --preprocess \
  --reader-type json \
  --library-input-folder ./examples/json_library \
  --output-folder ./examples/output/pattern_multigraph \
  --pattern-output-type json \
  --preprocess-multigraph 3 \
  --multigraph-alive-percent 0.2
```
Extracts patterns across the whole library at once (no `--background-graph-path` needed).
`--multigraph-alive-percent 0.2` keeps patterns appearing in at least 1 of the 5 library graphs.
Verified: found the `lib_5_not_in_high_degree` star pattern (5 nodes, center + 4 leaves).

## 8. Pattern filtering (`sgf-pattern-finder --filter`)

Filters the patterns extracted above against a background graph — here, `json_graph`:

```bash
./build/sgf-pattern-finder \
  --filter \
  --reader-type json \
  --pattern-mapping-cache ./examples/output/pattern_multigraph/pattern_index_<timestamp>.csv \
  --pattern-type json \
  --background-graph-folder ./examples/json_graph/graph.json \
  --output-path ./examples/output/pattern_filter \
  --output-type json \
  --prior-policy subgraph-degree
```
Verified: all 3 pattern files reported `false` (survive filtering — not yet ruled out; recall the
underlying pattern is a degree-4 star and `json_graph`'s max degree is 3, so `sgf-graph-searcher`
would find 0 actual matches, same as example 9 below — filtering is a necessary-condition
pre-check, not an exact match).

## 9. Subgraph isomorphism (`sgf-graph-searcher`)

Run against both the graph that **is** in the background and the one that **is not**:

```bash
# Genuine subgraph — expect a nonzero match count.
./build/sgf-graph-searcher \
  --subgraph-path ./examples/in_graph_library_graphml/in.graphml \
  --background-path ./examples/graph_graphml/graph.graphml \
  --reader-type graphml \
  --prior-policy subgraph-degree
# Verified output: Matches found: 2

# Not a subgraph — expect zero matches.
./build/sgf-graph-searcher \
  --subgraph-path ./examples/not_in_graph_library_graphml/not_in.graphml \
  --background-path ./examples/graph_graphml/graph.graphml \
  --reader-type graphml \
  --prior-policy subgraph-degree
# Verified output: Matches found: 0
```

---

## Note on a bug found while writing these examples

Example 4 above surfaced a real CSV-cache bug (now fixed, `include/io/cache/CSVCacheIOManager.h` /
`.cpp`): a graph whose enumeration result was completely empty used to write **zero rows** to the
CSV cache, making it indistinguishable from "not part of the library" when read back — it would
silently vanish from filter results instead of correctly appearing as `false` (survives). The fix
writes a single sentinel row (`motif_number=0, appearances=0`) for such graphs. The binary cache
format (`BinaryCacheIOManager`) was already correct (it's length-prefixed per graph, so "zero
entries" is representable) — only the CSV writer needed the fix.
