#!/usr/bin/env bash
# CLI smoke tests — run from project root: bash test_cli.sh
set -euo pipefail

BUILD=./build
GRAPHML=./tests/io/graphml
TMP=$(mktemp -d)
mkdir -p "$TMP/cache"
trap 'echo ""; echo "Results: $PASS passed, $FAIL failed"; rm -rf "$TMP"' EXIT

PASS=0
FAIL=0
EXPECT_OUTPUT=""

run_test() {
    local name="$1"
    local expected_exit="$2"
    shift 2
    local cmd=("$@")
    local outfile="$TMP/out_${name// /_}.txt"

    set +e
    "${cmd[@]}" > "$outfile" 2>&1
    local actual_exit=$?
    set -e

    local pass=true
    local fail_reason=""
    if [ "$actual_exit" -ne "$expected_exit" ]; then
        pass=false
        fail_reason="expected exit $expected_exit, got $actual_exit"
    elif [ -n "$EXPECT_OUTPUT" ] && ! grep -qF -- "$EXPECT_OUTPUT" "$outfile"; then
        pass=false
        fail_reason="output missing '$EXPECT_OUTPUT'"
    fi

    if [ "$pass" = true ]; then
        echo "PASS  $name"
        PASS=$((PASS + 1))
    else
        echo "FAIL  $name  ($fail_reason)"
        echo "      cmd: ${cmd[*]}"
        echo "      output: $(cat "$outfile")"
        FAIL=$((FAIL + 1))
    fi
    EXPECT_OUTPUT=""
}

# ── inline test data ──────────────────────────────────────────────────────────

cat > "$TMP/neg_color.graphml" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<graphml xmlns="http://graphml.graphdrawing.org/graphml">
  <key id="vcolor" for="node" attr.name="color" attr.type="string"><default>0</default></key>
  <key id="ecolor" for="edge" attr.name="color" attr.type="string"><default>0</default></key>
  <graph id="G" edgedefault="undirected">
    <node id="n0"><data key="vcolor">-1</data></node>
    <node id="n1"><data key="vcolor">0</data></node>
    <edge source="n0" target="n1"><data key="ecolor">0</data></edge>
  </graph>
</graphml>
EOF

printf -- "-1 0\n1 0\n" > "$TMP/neg_id.node_labels"
printf -- "-1 1\n"       > "$TMP/neg_id.edges"

# ── sgf-graph-searcher ────────────────────────────────────────────────────────

echo "=== sgf-graph-searcher ==="

run_test "searcher: no args prints help" 0 \
    "$BUILD/sgf-graph-searcher"

# wrong flags
EXPECT_OUTPUT="--subgraph-path"
run_test "searcher: missing --subgraph-path" 2 \
    "$BUILD/sgf-graph-searcher" \
    --background-path "$GRAPHML/triangle_all_edges_same_color_undirected.graphml" \
    --reader-type graphml --prior-policy subgraph-degree-squared

EXPECT_OUTPUT="--background-path"
run_test "searcher: missing --background-path" 2 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$GRAPHML/single_node_undirected.graphml" \
    --reader-type graphml --prior-policy subgraph-degree-squared

EXPECT_OUTPUT="--reader-type"
run_test "searcher: missing --reader-type" 2 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$GRAPHML/single_node_undirected.graphml" \
    --background-path "$GRAPHML/triangle_all_edges_same_color_undirected.graphml" \
    --prior-policy subgraph-degree-squared

EXPECT_OUTPUT="--prior-policy"
run_test "searcher: missing --prior-policy" 2 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$GRAPHML/single_node_undirected.graphml" \
    --background-path "$GRAPHML/triangle_all_edges_same_color_undirected.graphml" \
    --reader-type graphml

EXPECT_OUTPUT="bad-format"
run_test "searcher: invalid reader type" 2 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$GRAPHML/single_node_undirected.graphml" \
    --background-path "$GRAPHML/triangle_all_edges_same_color_undirected.graphml" \
    --reader-type bad-format --prior-policy subgraph-degree-squared

EXPECT_OUTPUT="bad-policy"
run_test "searcher: invalid prior policy" 2 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$GRAPHML/single_node_undirected.graphml" \
    --background-path "$GRAPHML/triangle_all_edges_same_color_undirected.graphml" \
    --reader-type graphml --prior-policy bad-policy

# empty graph
run_test "searcher: empty subgraph (s empty)" 2 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$GRAPHML/empty_undirected.graphml" \
    --background-path "$GRAPHML/triangle_all_edges_same_color_undirected.graphml" \
    --reader-type graphml --prior-policy subgraph-degree-squared

run_test "searcher: empty background (g empty)" 0 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$GRAPHML/single_node_undirected.graphml" \
    --background-path "$GRAPHML/empty_undirected.graphml" \
    --reader-type graphml --prior-policy subgraph-degree-squared

run_test "searcher: both graphs empty" 2 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$GRAPHML/empty_undirected.graphml" \
    --background-path "$GRAPHML/empty_undirected.graphml" \
    --reader-type graphml --prior-policy subgraph-degree-squared

# directories / files that don't exist
run_test "searcher: nonexistent subgraph path" 4 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "/nonexistent/subgraph.graphml" \
    --background-path "$GRAPHML/triangle_all_edges_same_color_undirected.graphml" \
    --reader-type graphml --prior-policy subgraph-degree-squared

run_test "searcher: nonexistent background path" 4 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$GRAPHML/single_node_undirected.graphml" \
    --background-path "/nonexistent/background.graphml" \
    --reader-type graphml --prior-policy subgraph-degree-squared

# negative vertex
run_test "searcher: negative color vertex in graphml subgraph (s)" 0 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$TMP/neg_color.graphml" \
    --background-path "$GRAPHML/triangle_all_edges_same_color_undirected.graphml" \
    --reader-type graphml --prior-policy subgraph-degree-squared

run_test "searcher: negative color vertex in graphml background (g)" 0 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$GRAPHML/single_node_undirected.graphml" \
    --background-path "$TMP/neg_color.graphml" \
    --reader-type graphml --prior-policy subgraph-degree-squared

run_test "searcher: negative vertex ID in vertex-edge (s)" 3 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$TMP/neg_id" \
    --background-path "$TMP/neg_id" \
    --reader-type vertex-edge --prior-policy subgraph-degree-squared

# ── sgf-graph-enumerator --preprocess ────────────────────────────────────────

echo ""
echo "=== sgf-graph-enumerator --preprocess ==="

run_test "enumerator: no args prints help" 0 \
    "$BUILD/sgf-graph-enumerator"

# wrong flags
EXPECT_OUTPUT="--preprocess"
run_test "enumerator preprocess: missing mode flag" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --motifs --reader-type graphml --cache-type csv

EXPECT_OUTPUT="--graph-dir"
run_test "enumerator preprocess: --preprocess and --filter exclusive" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --filter --motifs \
    --library-dir "$GRAPHML" --reader-type graphml \
    --cache-dir "$TMP/cache" --cache-type csv

EXPECT_OUTPUT="--motifs"
run_test "enumerator preprocess: missing feature flag" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess \
    --library-dir "$GRAPHML" --reader-type graphml \
    --cache-dir "$TMP/cache" --cache-type csv

EXPECT_OUTPUT="--library-dir"
run_test "enumerator preprocess: missing --library-dir" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --reader-type graphml --cache-dir "$TMP/cache" --cache-type csv

EXPECT_OUTPUT="bad-format"
run_test "enumerator preprocess: invalid reader type" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --library-dir "$GRAPHML" --reader-type bad-format \
    --cache-dir "$TMP/cache" --cache-type csv

EXPECT_OUTPUT="bad-cache"
run_test "enumerator preprocess: invalid cache type" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --library-dir "$GRAPHML" --reader-type graphml \
    --cache-dir "$TMP/cache" --cache-type bad-cache

# empty graph
mkdir -p "$TMP/lib_empty" "$TMP/cache_empty"
run_test "enumerator preprocess: empty library dir (no files)" 0 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --library-dir "$TMP/lib_empty" --reader-type graphml \
    --cache-dir "$TMP/cache_empty" --cache-type csv

mkdir -p "$TMP/lib_with_empty_graph" "$TMP/cache_with_empty_graph"
cp "$GRAPHML/empty_undirected.graphml" "$TMP/lib_with_empty_graph/g.graphml"
run_test "enumerator preprocess: library containing empty graph" 0 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --library-dir "$TMP/lib_with_empty_graph" --reader-type graphml \
    --cache-dir "$TMP/cache_with_empty_graph" --cache-type csv

# directories that don't exist
run_test "enumerator preprocess: nonexistent library dir" 4 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --library-dir "/nonexistent/library" --reader-type graphml \
    --cache-dir "$TMP/cache" --cache-type csv

# negative vertex
mkdir -p "$TMP/lib_neg_color" "$TMP/cache_neg_color"
cp "$TMP/neg_color.graphml" "$TMP/lib_neg_color/g.graphml"
run_test "enumerator preprocess: negative color vertex in graphml" 0 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --library-dir "$TMP/lib_neg_color" --reader-type graphml \
    --cache-dir "$TMP/cache_neg_color" --cache-type csv

mkdir -p "$TMP/lib_neg_ve" "$TMP/cache_neg_ve"
cp "$TMP/neg_id.node_labels" "$TMP/lib_neg_ve/g.node_labels"
cp "$TMP/neg_id.edges"       "$TMP/lib_neg_ve/g.edges"
run_test "enumerator preprocess: negative vertex ID in vertex-edge" 3 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --library-dir "$TMP/lib_neg_ve" --reader-type vertex-edge \
    --cache-dir "$TMP/cache_neg_ve" --cache-type csv

# ── sgf-graph-enumerator --filter ────────────────────────────────────────────

echo ""
echo "=== sgf-graph-enumerator --filter ==="

# wrong flags (no cache dir needed — argument validation runs before any file I/O)
EXPECT_OUTPUT="--graph-dir"
run_test "enumerator filter: missing --graph-dir" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type json \
    --cache-type csv --motif-cache-dir "/nonexistent/motif_cache_dir"

EXPECT_OUTPUT="--result-folder"
run_test "enumerator filter: missing --result-folder" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "$TMP/lib_empty" --graph-input-type graphml \
    --result-type json \
    --cache-type csv --motif-cache-dir "/nonexistent/motif_cache_dir"

EXPECT_OUTPUT="--graph-input-type"
run_test "enumerator filter: missing --graph-input-type" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "$TMP/lib_empty" \
    --result-folder "$TMP/results" --result-type json \
    --cache-type csv --motif-cache-dir "/nonexistent/motif_cache_dir"

EXPECT_OUTPUT="--motif-cache-dir"
run_test "enumerator filter: missing --motif-cache-dir when --motifs" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "$TMP/lib_empty" --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type json \
    --cache-type csv

EXPECT_OUTPUT="bad-type"
run_test "enumerator filter: invalid result type" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "$TMP/lib_empty" --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type bad-type \
    --cache-type csv --motif-cache-dir "/nonexistent/motif_cache_dir"

EXPECT_OUTPUT="--graph-cache-dir"
run_test "enumerator filter: --cache-enumeration without --graph-cache-dir" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs --cache-enumeration \
    --graph-dir "$TMP/lib_empty" --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type json \
    --cache-type csv --motif-cache-dir "/nonexistent/motif_cache_dir"

EXPECT_OUTPUT="--cache-type"
run_test "enumerator filter: missing --cache-type" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "$TMP/lib_empty" --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type json \
    --motif-cache-dir "/nonexistent/motif_cache_dir"

# directories / files that don't exist (file I/O runs, no valid cache needed for exit 4)
run_test "enumerator filter: nonexistent --motif-cache-dir" 4 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "$TMP/lib_empty" --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type json \
    --cache-type csv --motif-cache-dir "/nonexistent/motif_cache_dir"

run_test "enumerator filter: nonexistent --graph-dir" 4 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "/nonexistent/query" --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type json \
    --cache-type csv --motif-cache-dir "/nonexistent/motif_cache_dir"

# ── sgf-pattern-finder --preprocess ──────────────────────────────────────────

echo ""
echo "=== sgf-pattern-finder --preprocess ==="

run_test "pattern-finder: no args prints help" 0 \
    "$BUILD/sgf-pattern-finder"

# wrong flags
EXPECT_OUTPUT="--preprocess"
run_test "pattern-finder preprocess: missing mode flag" 2 \
    "$BUILD/sgf-pattern-finder" \
    --reader-type graphml

EXPECT_OUTPUT="--library-input-folder"
run_test "pattern-finder preprocess: missing --library-input-folder" 2 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --output-folder "$TMP/pf_out" --pattern-output-type graphml

EXPECT_OUTPUT="--output-folder"
run_test "pattern-finder preprocess: missing --output-folder" 2 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "$GRAPHML" --pattern-output-type graphml

EXPECT_OUTPUT="--pattern-output-type"
run_test "pattern-finder preprocess: missing --pattern-output-type" 2 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "$GRAPHML" --output-folder "$TMP/pf_out"

EXPECT_OUTPUT="bad-format"
run_test "pattern-finder preprocess: invalid reader type" 2 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type bad-format \
    --library-input-folder "$GRAPHML" \
    --output-folder "$TMP/pf_out" --pattern-output-type graphml

# empty graph
mkdir -p "$TMP/pf_lib_empty" "$TMP/pf_out_empty"
run_test "pattern-finder preprocess: empty library dir (no files)" 0 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "$TMP/pf_lib_empty" \
    --output-folder "$TMP/pf_out_empty" --pattern-output-type graphml

mkdir -p "$TMP/pf_lib_with_empty" "$TMP/pf_out_with_empty"
cp "$GRAPHML/empty_undirected.graphml" "$TMP/pf_lib_with_empty/g.graphml"
run_test "pattern-finder preprocess: library containing empty graph" 0 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "$TMP/pf_lib_with_empty" \
    --output-folder "$TMP/pf_out_with_empty" --pattern-output-type graphml

# directories that don't exist
run_test "pattern-finder preprocess: nonexistent library folder" 4 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "/nonexistent/library" \
    --output-folder "$TMP/pf_out" --pattern-output-type graphml

# negative vertex
mkdir -p "$TMP/pf_lib_neg_color" "$TMP/pf_out_neg_color"
cp "$TMP/neg_color.graphml" "$TMP/pf_lib_neg_color/g.graphml"
run_test "pattern-finder preprocess: negative color vertex in graphml" 0 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "$TMP/pf_lib_neg_color" \
    --output-folder "$TMP/pf_out_neg_color" --pattern-output-type graphml

mkdir -p "$TMP/pf_lib_neg_ve" "$TMP/pf_out_neg_ve"
cp "$TMP/neg_id.node_labels" "$TMP/pf_lib_neg_ve/g.node_labels"
cp "$TMP/neg_id.edges"       "$TMP/pf_lib_neg_ve/g.edges"
run_test "pattern-finder preprocess: negative vertex ID in vertex-edge" 3 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type vertex-edge \
    --library-input-folder "$TMP/pf_lib_neg_ve" \
    --output-folder "$TMP/pf_out_neg_ve" --pattern-output-type graphml

# multigraph preprocess smoke test
mkdir -p "$TMP/pf_lib_mg" "$TMP/pf_out_mg"
cp "$GRAPHML/triangle_same_vertex_color_undirected.graphml" "$TMP/pf_lib_mg/g0.graphml"
cp "$GRAPHML/triangle_diff_vertex_colors_undirected.graphml" "$TMP/pf_lib_mg/g1.graphml"
run_test "pattern-finder preprocess: multigraph mode succeeds" 0 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "$TMP/pf_lib_mg" \
    --output-folder "$TMP/pf_out_mg" --pattern-output-type graphml \
    --preprocess-multigraph 1 --multigraph-alive-percent 0.5

EXPECT_OUTPUT="multigraph-alive-percent"
run_test "pattern-finder preprocess: multigraph alive-percent out of range" 2 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "$TMP/pf_lib_mg" \
    --output-folder "$TMP/pf_out_mg" --pattern-output-type graphml \
    --preprocess-multigraph 1 --multigraph-alive-percent 0.0

# ── sgf-pattern-finder --filter ──────────────────────────────────────────────

echo ""
echo "=== sgf-pattern-finder --filter ==="

# wrong flags (argument validation runs before any file I/O)
EXPECT_OUTPUT="--pattern-mapping-cache"
run_test "pattern-finder filter: missing --pattern-mapping-cache" 2 \
    "$BUILD/sgf-pattern-finder" \
    --filter --reader-type graphml \
    --pattern-type graphml \
    --background-graph-folder "$TMP/pf_lib_empty" \
    --output-path "$TMP/pf_filter_out" --output-type json \
    --prior-policy subgraph-degree-squared

EXPECT_OUTPUT="--pattern-type"
run_test "pattern-finder filter: missing --pattern-type" 2 \
    "$BUILD/sgf-pattern-finder" \
    --filter --reader-type graphml \
    --pattern-mapping-cache "/nonexistent/patterns.cache" \
    --background-graph-folder "$TMP/pf_lib_empty" \
    --output-path "$TMP/pf_filter_out" --output-type json \
    --prior-policy subgraph-degree-squared

EXPECT_OUTPUT="--background-graph-folder"
run_test "pattern-finder filter: missing --background-graph-folder" 2 \
    "$BUILD/sgf-pattern-finder" \
    --filter --reader-type graphml \
    --pattern-mapping-cache "/nonexistent/patterns.cache" \
    --pattern-type graphml \
    --output-path "$TMP/pf_filter_out" --output-type json \
    --prior-policy subgraph-degree-squared

EXPECT_OUTPUT="--output-path"
run_test "pattern-finder filter: missing --output-path" 2 \
    "$BUILD/sgf-pattern-finder" \
    --filter --reader-type graphml \
    --pattern-mapping-cache "/nonexistent/patterns.cache" \
    --pattern-type graphml \
    --background-graph-folder "$TMP/pf_lib_empty" \
    --output-type json --prior-policy subgraph-degree-squared

EXPECT_OUTPUT="--prior-policy"
run_test "pattern-finder filter: missing --prior-policy" 2 \
    "$BUILD/sgf-pattern-finder" \
    --filter --reader-type graphml \
    --pattern-mapping-cache "/nonexistent/patterns.cache" \
    --pattern-type graphml \
    --background-graph-folder "$TMP/pf_lib_empty" \
    --output-path "$TMP/pf_filter_out" --output-type json

EXPECT_OUTPUT="bad-policy"
run_test "pattern-finder filter: invalid prior policy" 2 \
    "$BUILD/sgf-pattern-finder" \
    --filter --reader-type graphml \
    --pattern-mapping-cache "/nonexistent/patterns.cache" \
    --pattern-type graphml \
    --background-graph-folder "$TMP/pf_lib_empty" \
    --output-path "$TMP/pf_filter_out" --output-type json \
    --prior-policy bad-policy

# directories / files that don't exist
run_test "pattern-finder filter: nonexistent --pattern-mapping-cache" 4 \
    "$BUILD/sgf-pattern-finder" \
    --filter --reader-type graphml \
    --pattern-mapping-cache "/nonexistent/patterns.cache" \
    --pattern-type graphml \
    --background-graph-folder "$TMP/pf_lib_empty" \
    --output-path "$TMP/pf_filter_out" --output-type json \
    --prior-policy subgraph-degree-squared

run_test "pattern-finder filter: nonexistent --background-graph-folder" 4 \
    "$BUILD/sgf-pattern-finder" \
    --filter --reader-type graphml \
    --pattern-mapping-cache "/nonexistent/patterns.cache" \
    --pattern-type graphml \
    --background-graph-folder "/nonexistent/graphs" \
    --output-path "$TMP/pf_filter_out" --output-type json \
    --prior-policy subgraph-degree-squared

# ── --thread-number flag ──────────────────────────────────────────────────────

echo ""
echo "=== --thread-number flag ==="

# invalid values rejected before any I/O
EXPECT_OUTPUT="thread-number"
run_test "enumerator: --thread-number 0 rejected" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --library-dir "$TMP/lib_empty" --reader-type graphml \
    --cache-dir "$TMP/cache" --cache-type csv \
    --thread-number 0

EXPECT_OUTPUT="thread-number"
run_test "enumerator: --thread-number non-numeric rejected" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --library-dir "$TMP/lib_empty" --reader-type graphml \
    --cache-dir "$TMP/cache" --cache-type csv \
    --thread-number abc

EXPECT_OUTPUT="thread-number"
run_test "pattern-finder: --thread-number 0 rejected" 2 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "$TMP/pf_lib_empty" \
    --output-folder "$TMP/pf_out_empty" --pattern-output-type graphml \
    --thread-number 0

EXPECT_OUTPUT="thread-number"
run_test "pattern-finder: --thread-number non-numeric rejected" 2 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "$TMP/pf_lib_empty" \
    --output-folder "$TMP/pf_out_empty" --pattern-output-type graphml \
    --thread-number abc

# correctness: single-thread vs multi-thread output must match
mkdir -p "$TMP/lib_threads"
cp "$GRAPHML/triangle_same_vertex_color_undirected.graphml" "$TMP/lib_threads/g0.graphml"
cp "$GRAPHML/triangle_diff_vertex_colors_undirected.graphml" "$TMP/lib_threads/g1.graphml"
cp "$GRAPHML/two_nodes_bidirectional_same_edge_color_undirected.graphml" "$TMP/lib_threads/g2.graphml"

mkdir -p "$TMP/cache_t1_motif" "$TMP/cache_t4_motif"
run_test "enumerator preprocess --motifs --thread-number 1" 0 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --library-dir "$TMP/lib_threads" --reader-type graphml \
    --cache-dir "$TMP/cache_t1_motif" --cache-type csv \
    --thread-number 1

run_test "enumerator preprocess --motifs --thread-number 4" 0 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs \
    --library-dir "$TMP/lib_threads" --reader-type graphml \
    --cache-dir "$TMP/cache_t4_motif" --cache-type csv \
    --thread-number 4

# compare cache contents: same files, same content (sort for determinism)
T1_MOTIF_SORTED=$(sort "$TMP/cache_t1_motif"/*.csv 2>/dev/null | md5sum)
T4_MOTIF_SORTED=$(sort "$TMP/cache_t4_motif"/*.csv 2>/dev/null | md5sum)
if [ "$T1_MOTIF_SORTED" = "$T4_MOTIF_SORTED" ]; then
    echo "PASS  enumerator: motif cache identical for thread-number 1 vs 4"
    PASS=$((PASS + 1))
else
    echo "FAIL  enumerator: motif cache differs between thread-number 1 and 4"
    FAIL=$((FAIL + 1))
fi

mkdir -p "$TMP/cache_t1_path" "$TMP/cache_t4_path"
run_test "enumerator preprocess --paths --thread-number 1" 0 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --paths \
    --library-dir "$TMP/lib_threads" --reader-type graphml \
    --cache-dir "$TMP/cache_t1_path" --cache-type csv \
    --thread-number 1

run_test "enumerator preprocess --paths --thread-number 4" 0 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --paths \
    --library-dir "$TMP/lib_threads" --reader-type graphml \
    --cache-dir "$TMP/cache_t4_path" --cache-type csv \
    --thread-number 4

T1_PATH_SORTED=$(sort "$TMP/cache_t1_path"/*.csv 2>/dev/null | md5sum)
T4_PATH_SORTED=$(sort "$TMP/cache_t4_path"/*.csv 2>/dev/null | md5sum)
if [ "$T1_PATH_SORTED" = "$T4_PATH_SORTED" ]; then
    echo "PASS  enumerator: path cache identical for thread-number 1 vs 4"
    PASS=$((PASS + 1))
else
    echo "FAIL  enumerator: path cache differs between thread-number 1 and 4"
    FAIL=$((FAIL + 1))
fi

# ── full pipeline integration (3 graphml graphs) ─────────────────────────────

echo ""
echo "=== full pipeline integration (3 graphml graphs) ==="

mkdir -p "$TMP/pipe_lib" "$TMP/pipe_lib_cache" "$TMP/pipe_enum_result" \
         "$TMP/pipe_query" "$TMP/pipe_pf_out" "$TMP/pipe_pf_result" \
         "$TMP/pipe_lib_cache_with_map" "$TMP/pipe_pf_out_with_map"

# large: 4-cycle n0(alpha)-n1(beta)-n2(alpha)-n3(beta)-n0
cat > "$TMP/pipe_lib/large.graphml" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<graphml xmlns="http://graphml.graphdrawing.org/graphml">
  <key id="vcolor" for="node" attr.name="color" attr.type="string"><default>0</default></key>
  <key id="ecolor" for="edge" attr.name="color" attr.type="string"><default>0</default></key>
  <graph id="G" edgedefault="undirected">
    <node id="n0"><data key="vcolor">alpha</data></node>
    <node id="n1"><data key="vcolor">beta</data></node>
    <node id="n2"><data key="vcolor">alpha</data></node>
    <node id="n3"><data key="vcolor">beta</data></node>
    <edge source="n0" target="n1"><data key="ecolor">0</data></edge>
    <edge source="n1" target="n2"><data key="ecolor">0</data></edge>
    <edge source="n2" target="n3"><data key="ecolor">0</data></edge>
    <edge source="n3" target="n0"><data key="ecolor">0</data></edge>
  </graph>
</graphml>
EOF

# small_in_large: single edge alpha-beta (subgraph of large)
cat > "$TMP/pipe_query/small_in_large.graphml" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<graphml xmlns="http://graphml.graphdrawing.org/graphml">
  <key id="vcolor" for="node" attr.name="color" attr.type="string"><default>0</default></key>
  <key id="ecolor" for="edge" attr.name="color" attr.type="string"><default>0</default></key>
  <graph id="G" edgedefault="undirected">
    <node id="n0"><data key="vcolor">alpha</data></node>
    <node id="n1"><data key="vcolor">beta</data></node>
    <edge source="n0" target="n1"><data key="ecolor">0</data></edge>
  </graph>
</graphml>
EOF

# small_not_in_large: triangle of gamma (color absent from large)
cat > "$TMP/pipe_query/small_not_in_large.graphml" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<graphml xmlns="http://graphml.graphdrawing.org/graphml">
  <key id="vcolor" for="node" attr.name="color" attr.type="string"><default>0</default></key>
  <key id="ecolor" for="edge" attr.name="color" attr.type="string"><default>0</default></key>
  <graph id="G" edgedefault="undirected">
    <node id="n0"><data key="vcolor">gamma</data></node>
    <node id="n1"><data key="vcolor">gamma</data></node>
    <node id="n2"><data key="vcolor">gamma</data></node>
    <edge source="n0" target="n1"><data key="ecolor">0</data></edge>
    <edge source="n1" target="n2"><data key="ecolor">0</data></edge>
    <edge source="n2" target="n0"><data key="ecolor">0</data></edge>
  </graph>
</graphml>
EOF

# graph searcher: small_in_large in large (4 undirected edge-maps)
EXPECT_OUTPUT="Matches found: 4"
run_test "pipeline: small_in_large found in large (4 matches)" 0 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$TMP/pipe_query/small_in_large.graphml" \
    --background-path "$TMP/pipe_lib/large.graphml" \
    --reader-type graphml --prior-policy subgraph-degree

# graph searcher: small_not_in_large in large (no gamma in large → 0)
EXPECT_OUTPUT="Matches found: 0"
run_test "pipeline: small_not_in_large not found in large" 0 \
    "$BUILD/sgf-graph-searcher" \
    --subgraph-path "$TMP/pipe_query/small_not_in_large.graphml" \
    --background-path "$TMP/pipe_lib/large.graphml" \
    --reader-type graphml --prior-policy subgraph-degree

# enumerator: preprocess library into motif cache
run_test "pipeline: enumerator preprocess motifs" 0 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs --reader-type graphml \
    --library-dir "$TMP/pipe_lib" \
    --cache-dir "$TMP/pipe_lib_cache" --cache-type binary

# enumerator: filter query graphs against motif cache (the whole cache directory,
# which now holds one file per library graph)
run_test "pipeline: enumerator filter motifs (query dir)" 0 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs --graph-input-type graphml \
    --graph-dir "$TMP/pipe_query" \
    --motif-cache-dir "$TMP/pipe_lib_cache" \
    --cache-type binary \
    --result-folder "$TMP/pipe_enum_result" --result-type json

# pattern-finder: multigraph preprocess on library
run_test "pipeline: pattern-finder preprocess multigraph" 0 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "$TMP/pipe_lib" \
    --output-folder "$TMP/pipe_pf_out" --pattern-output-type graphml \
    --preprocess-multigraph 1 --multigraph-alive-percent 0.5

PIPE_PATTERN_CACHE=$(find "$TMP/pipe_pf_out" -name "*.csv" 2>/dev/null | head -1)

# pattern-finder: filter a single background graph against pattern cache
run_test "pipeline: pattern-finder filter (small_in_large as background)" 0 \
    "$BUILD/sgf-pattern-finder" \
    --filter --reader-type graphml \
    --pattern-mapping-cache "$PIPE_PATTERN_CACHE" \
    --pattern-type graphml \
    --background-graph-folder "$TMP/pipe_query/small_in_large.graphml" \
    --output-path "$TMP/pipe_pf_result" --output-type json \
    --prior-policy subgraph-degree

# color map: preprocess with initial map → verify saved map
printf "color_label,color_id\nalpha,0\nbeta,1\n" > "$TMP/pipe_color_map.csv"

run_test "pipeline: enumerator preprocess with --graphml-color-map-path" 0 \
    "$BUILD/sgf-graph-enumerator" \
    --preprocess --motifs --reader-type graphml \
    --library-dir "$TMP/pipe_lib" \
    --cache-dir "$TMP/pipe_lib_cache_with_map" --cache-type binary \
    --graphml-color-map-path "$TMP/pipe_color_map.csv"

if find "$TMP/pipe_lib_cache_with_map" -name "color_map_*.csv" 2>/dev/null | grep -q .; then
    echo "PASS  pipeline: color map CSV saved after preprocess"
    PASS=$((PASS + 1))
else
    echo "FAIL  pipeline: color map CSV not found in cache dir after preprocess"
    FAIL=$((FAIL + 1))
fi

run_test "pipeline: pattern-finder preprocess with --graphml-color-map-path" 0 \
    "$BUILD/sgf-pattern-finder" \
    --preprocess --reader-type graphml \
    --library-input-folder "$TMP/pipe_lib" \
    --output-folder "$TMP/pipe_pf_out_with_map" --pattern-output-type graphml \
    --preprocess-multigraph 1 --multigraph-alive-percent 0.5 \
    --graphml-color-map-path "$TMP/pipe_color_map.csv"

[ "$FAIL" -eq 0 ]
