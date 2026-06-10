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

# wrong flags (no cache file needed — argument validation runs before any file I/O)
EXPECT_OUTPUT="--graph-dir"
run_test "enumerator filter: missing --graph-dir" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type json \
    --cache-type csv --motif-cache-file "/nonexistent/motif.cache"

EXPECT_OUTPUT="--result-folder"
run_test "enumerator filter: missing --result-folder" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "$TMP/lib_empty" --graph-input-type graphml \
    --result-type json \
    --cache-type csv --motif-cache-file "/nonexistent/motif.cache"

EXPECT_OUTPUT="--graph-input-type"
run_test "enumerator filter: missing --graph-input-type" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "$TMP/lib_empty" \
    --result-folder "$TMP/results" --result-type json \
    --cache-type csv --motif-cache-file "/nonexistent/motif.cache"

EXPECT_OUTPUT="--motif-cache-file"
run_test "enumerator filter: missing --motif-cache-file when --motifs" 2 \
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
    --cache-type csv --motif-cache-file "/nonexistent/motif.cache"

EXPECT_OUTPUT="--graph-cache-dir"
run_test "enumerator filter: --cache-enumeration without --graph-cache-dir" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs --cache-enumeration \
    --graph-dir "$TMP/lib_empty" --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type json \
    --cache-type csv --motif-cache-file "/nonexistent/motif.cache"

EXPECT_OUTPUT="--cache-type"
run_test "enumerator filter: missing --cache-type" 2 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "$TMP/lib_empty" --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type json \
    --motif-cache-file "/nonexistent/motif.cache"

# directories / files that don't exist (file I/O runs, no valid cache needed for exit 4)
run_test "enumerator filter: nonexistent --motif-cache-file" 4 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "$TMP/lib_empty" --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type json \
    --cache-type csv --motif-cache-file "/nonexistent/motif.cache"

run_test "enumerator filter: nonexistent --graph-dir" 4 \
    "$BUILD/sgf-graph-enumerator" \
    --filter --motifs \
    --graph-dir "/nonexistent/query" --graph-input-type graphml \
    --result-folder "$TMP/results" --result-type json \
    --cache-type csv --motif-cache-file "/nonexistent/motif.cache"

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

[ "$FAIL" -eq 0 ]
