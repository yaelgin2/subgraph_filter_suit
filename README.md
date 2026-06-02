# subgraph_filter_suite

A C++ library for efficient subgraph matching. It preprocesses a library of graphs and uses a three-stage filtering pipeline (motif, path, and pattern filtering) to eliminate unlikely candidates before exact isomorphism, dramatically reducing computation time for large-scale graph queries.

## Requirements

- CMake 3.16+
- A C++17-capable compiler (GCC 7+, Clang 5+, or MSVC 19.14+) **built against the same C++ standard library ABI as your Boost installation** — mixing compiler versions (e.g. system GCC and a conda-provided Boost) causes linker errors; see [Compiler/Boost ABI mismatch](#compilerboost-abi-mismatch) below
- Boost 1.76+ (`graph`, `log`, `log_setup`, `json` components; `program_options` is required only for the CLI tools)
- Internet access on first build (GTest is fetched automatically via CMake FetchContent)

## Build

### Library only (no CLI tools)

```bash
cmake -S . -B build -DSGF_BUILD_CLI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The library target is `subgraph_filter_suite`. Link against it from your own CMake project with:

```cmake
find_package(subgraph_filter_suite REQUIRED)
target_link_libraries(my_target PRIVATE subgraph_filter_suite)
```

### Library + CLI tools

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

This produces three executables in `build/`:
- `sgf-graph-enumerator` — motif/path preprocessing and filtering
- `sgf-pattern-finder` — pattern preprocessing and filtering
- `sgf-graph-searcher` — exact subgraph isomorphism

> **Note on `--config Release` vs `-DCMAKE_BUILD_TYPE=Release`:** Single-config generators (Unix Makefiles, Ninja) read the build type from `-DCMAKE_BUILD_TYPE` at configure time. Multi-config generators (Visual Studio, Xcode) read it from `--config` at build time. Passing both ensures a Release build on every platform.

### Compiler/Boost ABI mismatch

If you see linker errors like `undefined reference to GLIBCXX_*`, your compiler and Boost installation were built against different C++ standard library ABIs. Fix by pointing CMake at the compiler that built Boost:

```bash
# Example: Boost installed via conda
cmake -S . -B build -DCMAKE_CXX_COMPILER=$CONDA_PREFIX/bin/g++

# Example: explicit path
cmake -S . -B build -DCMAKE_CXX_COMPILER=/usr/bin/g++-13
```

Users with system Boost and system GCC typically do not need this flag.

## Running tests

```bash
# Run all tests
ctest --test-dir build --output-on-failure --build-config Release

# Run a specific test suite by name
ctest --test-dir build -R colored_graph_tests --output-on-failure --build-config Release

# Run a single test by name
ctest --test-dir build -R three_vertices_triangle_returns_empty_map --output-on-failure --build-config Release
```

---

## CLI tools

### Graph file formats

All three tools accept the same `--reader-type` values:

| Value | Extension(s) | Description |
|---|---|---|
| `graphml` | `.graphml` | Standard GraphML XML; vertex and edge `color` attributes are mapped to sequential integer IDs. String color values are supported. |
| `json` | `.json` | JSON object with `"nodes"` (array of `{id, color}`) and `"links"` (array of `{source, target[, color]}`). Colors must be non-negative integers. |
| `vertex-edge` | `.node_labels` + `.edges` | Two plain-text files sharing the same base path. Each vertex line is `<id> <color>`; each edge line is `<src> <dst> [color]`. |

**Color handling:** GraphML colors are strings mapped to consecutive integer IDs (the mapping is logged at INFO level). JSON and vertex-edge colors are integer values used directly — no remapping occurs.

---

### `sgf-graph-enumerator`

Preprocesses a graph library into motif/path enumeration caches, then filters query graphs against those caches.

**Modes** (exactly one required, mutually exclusive):

| Flag | Description |
|---|---|
| `--preprocess` | Enumerate the graph library and write caches. |
| `--filter` | Filter query graphs against existing library caches. |

**Feature flags** (at least one required with both modes):

| Flag | Description |
|---|---|
| `--motifs` | Enable motif-based processing. |
| `--paths` | Enable path-based processing. |

**Common flags:**

| Flag | Type | Description |
|---|---|---|
| `--cache-type` | `binary` \| `csv` | Cache file format. |
| `--is-directed` | bool switch | Treat all graphs as directed. |
| `--non-induced` | bool switch | Expand induced motif counts via inclusion DAG before filtering (non-induced subgraph search). |
| `--log-file-path` | string | *(optional)* Path to log file. |

**Preprocess flags** (required with `--preprocess`):

| Flag | Type | Description |
|---|---|---|
| `--library-dir` | string | Directory containing the graph library. |
| `--reader-type` | `graphml` \| `json` \| `vertex-edge` | Graph file format. |
| `--cache-dir` | string | Directory where caches are written. |

**Filter flags** (required with `--filter`):

| Flag | Type | Description |
|---|---|---|
| `--graph-dir` | string | Directory containing query graphs. |
| `--graph-input-type` | `graphml` \| `json` \| `vertex-edge` | Query graph file format. |
| `--result-folder` | string | Directory for filter result output. |
| `--result-type` | `json` \| `csv` | Result file format. |
| `--motif-cache-file` | string | Full path to motif cache (required with `--motifs`). |
| `--path-cache-file` | string | Full path to path cache (required with `--paths`). |
| `--cache-enumeration` | bool switch | Cache query graph enumeration after computing. Mutually exclusive with `--load-motif-graph-cache` and `--load-path-graph-cache`. |
| `--graph-cache-dir` | string | Directory for query enumeration cache (required with `--cache-enumeration`). |
| `--load-motif-graph-cache` | string | Existing motif enumeration cache to load instead of computing. Mutually exclusive with `--cache-enumeration`. |
| `--load-path-graph-cache` | string | Existing path enumeration cache to load instead of computing. Mutually exclusive with `--cache-enumeration`. |

**Example — preprocess a library:**

```bash
./build/sgf-graph-enumerator \
  --preprocess --motifs --paths \
  --reader-type json \
  --library-dir ./graphs/library \
  --cache-dir ./cache \
  --cache-type binary
```

**Example — filter query graphs:**

```bash
./build/sgf-graph-enumerator \
  --filter --motifs --paths \
  --graph-dir ./graphs/queries \
  --graph-input-type json \
  --motif-cache-file ./cache/motif_cache_2024-01-01_12-00-00 \
  --path-cache-file ./cache/path_cache_2024-01-01_12-00-00 \
  --cache-type binary \
  --result-folder ./results \
  --result-type json
```

---

### `sgf-pattern-finder`

Extracts pattern subgraphs from a graph library and filters query graphs against them.

**Modes** (at least one required):

| Flag | Description |
|---|---|
| `--preprocess` | Extract patterns from the library and write pattern caches. |
| `--filter` | Filter query graphs against an existing pattern cache. |

**Common flags:**

| Flag | Type | Description |
|---|---|---|
| `--reader-type` | `graphml` \| `json` \| `vertex-edge` | Graph file format (required). |
| `--is-directed` | bool switch | Treat all graphs as directed. |
| `--log-file-path` | string | *(optional)* Path to log file. |

**Preprocess flags** (required with `--preprocess`):

| Flag | Type | Default | Description |
|---|---|---|---|
| `--library-input-folder` | string | — | Directory containing the graph library. |
| `--output-folder` | string | — | Directory where pattern files are written. |
| `--pattern-output-type` | `graphml` \| `json` \| `vertex-edge` | — | Pattern file format. |
| `--preprocess-single-graph` | integer | `-1` | Index of a single library graph to process; `-1` disables. |
| `--preprocess-single-graph-from-results` | bool switch | — | Derive the graph index from a results file instead of `--preprocess-single-graph`. |
| `--results-file-path` | string | — | Path to results file (required with `--preprocess-single-graph-from-results`). |
| `--results-file-type` | `json` \| `csv` | `json` | Format of the results file. |
| `--background-graph-path` | string | — | *(optional)* Path to background graph for pattern scoring. Required when `--preprocess-single-graph` is set. |
| `--score-threshold` | float | `0.0` | Pattern score cutoff; beam search stops below this value. |

**SingleGraphFinder config flags** (optional, used with `--preprocess`):

| Flag | Type | Default | Description |
|---|---|---|---|
| `--max-active-patterns` | integer | `500` | Maximum number of simultaneously active beam states. |
| `--alpha-0` | float | `1.0` | Initial weight for the outside-neighbour score term. |
| `--alpha-decay` | float | `0.9` | Per-depth multiplicative decay applied to alpha-0. |

**Filter flags** (required with `--filter`):

| Flag | Type | Description |
|---|---|---|
| `--pattern-mapping-cache` | string | Full path to the pattern cache produced by `--preprocess`. |
| `--pattern-type` | `graphml` \| `json` \| `vertex-edge` | Pattern cache file format. |
| `--background-graph-folder` | string | Directory containing background graphs to filter against. |
| `--output-path` | string | Directory for filter result output. |
| `--output-type` | `json` \| `csv` | Filter result file format. |
| `--prior-policy` | string | Vertex ordering heuristic (see values below). |
| `--is-induced` | bool switch | Search for induced subgraph matches. |

**Prior policy values:**

| Value | Description |
|---|---|
| `subgraph-degree` | Order by query-graph vertex degree. |
| `subgraph-degree-squared` | Order by query-graph vertex degree squared. |
| `graph-degree-squared` | Order by background-graph vertex degree squared. |
| `combined` | Combined degree heuristic. |
| `constant` | Fixed ordering (no reordering). |
| `random` | Random ordering. |

---

### `sgf-graph-searcher`

Runs exact subgraph isomorphism between a query subgraph and a background graph and prints the match count.

> **Important:** Only **connected** subgraphs are supported. A disconnected query graph returns 0. Ensure the subgraph file contains a single connected component.

| Flag | Type | Description |
|---|---|---|
| `--subgraph-path` | string | Path to the query subgraph file (required). |
| `--background-path` | string | Path to the background graph file (required). |
| `--reader-type` | `graphml` \| `json` \| `vertex-edge` | Graph file format (required). |
| `--prior-policy` | string | Vertex ordering heuristic (same values as `sgf-pattern-finder`, required). |
| `--is-directed` | bool switch | Treat graphs as directed. |
| `--is-induced` | bool switch | Search for induced subgraph matches only. |
| `--stop-on-first-match` | bool switch | Stop after the first match is found (returns 1). |
| `--output-path` | string | *(optional)* File to write match vertex mappings to. |
| `--log-file-path` | string | *(optional)* Path to log file. |

**Example:**

```bash
./build/sgf-graph-searcher \
  --subgraph-path ./query.json \
  --background-path ./background.json \
  --reader-type json \
  --prior-policy subgraph-degree
```

---

## C++ API

The library exposes five free functions in `include/api/SgfApi.h` that mirror the CLI tools but use typed C++ structs. Each function validates its inputs and throws `InvalidArgumentException` on missing or conflicting fields; all other `SgfException` subtypes propagate unchanged from the underlying pipeline.

### Quick example

```cpp
#include "SgfApi.h"

// 1. Preprocess a library into motif caches
sgf::EnumerateLibraryParams ep;
ep.m_library_path      = "./graphs/library";
ep.m_reader_type       = sgf::GraphReaderType::JSON;
ep.m_output_path       = "./cache";
ep.m_cache_type        = sgf::CacheManagerType::BINARY;
ep.m_preprocess_motifs = true;
ep.m_preprocess_paths  = true;
sgf::enumerate_library(ep);

// 2. Filter query graphs against those caches
sgf::FilterWithEnumerationParams fp;
fp.m_query_graph_path = "./graphs/queries";
fp.m_reader_type      = sgf::GraphReaderType::JSON;
fp.m_output_folder    = "./results";
fp.m_result_type      = sgf::ResultOutputType::JSON;
fp.m_cache_type       = sgf::CacheManagerType::BINARY;
fp.m_filter_motifs    = true;
fp.m_filter_paths     = true;
fp.m_motif_cache_file = "./cache/motif_cache_2024-01-01_12-00-00";
fp.m_path_cache_file  = "./cache/path_cache_2024-01-01_12-00-00";
auto results = sgf::filter_with_enumeration(fp);

// 3. Exact subgraph search
sgf::FindSubgraphParams sp;
sp.m_subgraph_path   = "./query.json";
sp.m_background_path = "./background.json";
sp.m_reader_type     = sgf::GraphReaderType::JSON;
sp.m_prior_policy    = sgf::PriorPolicy::SUBGRAPH_DEGREE;
const uint64_t count = sgf::find_subgraph(sp);
```

---

### `enumerate_library` — `EnumerateLibraryParams`

| Field | Type | Required | Description |
|---|---|---|---|
| `m_library_path` | `string` | yes | Directory containing the graph library. |
| `m_reader_type` | `GraphReaderType` | yes | Graph file format. |
| `m_output_path` | `string` | yes | Directory where caches are written. |
| `m_cache_type` | `CacheManagerType` | yes | Cache file format (`BINARY` or `CSV`). |
| `m_preprocess_motifs` | `bool` | at least one | Enumerate motif signatures. |
| `m_preprocess_paths` | `bool` | at least one | Enumerate path signatures. |
| `m_is_directed` | `bool` | — | Treat graphs as directed (default `false`). |
| `m_log_file` | `optional<string>` | — | Log file path. |

Returns `vector<EnumerationResultVector>`, one element per enabled feature.

---

### `filter_with_enumeration` — `FilterWithEnumerationParams`

| Field | Type | Required | Description |
|---|---|---|---|
| `m_query_graph_path` | `string` | yes | Directory containing query graphs. |
| `m_reader_type` | `GraphReaderType` | yes | Query graph file format. |
| `m_output_folder` | `string` | yes | Directory for filter result output. |
| `m_result_type` | `ResultOutputType` | yes | Filter result format (`JSON` or `CSV`). |
| `m_cache_type` | `CacheManagerType` | yes | Cache file format. |
| `m_filter_motifs` | `bool` | at least one | Filter by motif signatures. |
| `m_filter_paths` | `bool` | at least one | Filter by path signatures. |
| `m_motif_cache_file` | `optional<string>` | if motifs | Full path to the motif cache file. |
| `m_path_cache_file` | `optional<string>` | if paths | Full path to the path cache file. |
| `m_is_directed` | `bool` | — | Treat graphs as directed (default `false`). |
| `m_non_induced` | `bool` | — | Expand motif counts via inclusion DAG (default `false`). |
| `m_cache_config` | `GraphEnumerationCacheConfig` | — | Query graph enumeration caching options. |
| `m_log_file` | `optional<string>` | — | Log file path. |

Returns `vector<unordered_map<string, FilterResult>>`, one map per feature.

---

### `preprocess_patterns` — `PreprocessPatternsParams`

| Field | Type | Required | Description |
|---|---|---|---|
| `m_library_path` | `string` | yes | Directory containing the graph library. |
| `m_reader_type` | `GraphReaderType` | yes | Graph file format. |
| `m_output_path` | `string` | yes | Directory where pattern files are written. |
| `m_pattern_type` | `PatternWriterType` | yes | Pattern file format. |
| `m_is_directed` | `bool` | — | Treat graphs as directed (default `false`). |
| `m_single_graph_index` | `optional<int64_t>` | mutually exclusive with `m_results_file_path` | Process one library graph by index. |
| `m_results_file_path` | `optional<string>` | mutually exclusive with `m_single_graph_index` | Derive graph index from a results file. |
| `m_results_file_type` | `ResultOutputType` | — | Results file format (default `JSON`). |
| `m_background_graph_path` | `optional<string>` | required in single-graph mode | Background graph for pattern scoring. |
| `m_score_threshold` | `double` | — | Pattern score cutoff (default `0.0`). |
| `m_finder_config` | `SingleGraphFinderConfig` | — | Beam search tuning (max patterns, alpha, decay). |
| `m_log_file` | `optional<string>` | — | Log file path. |

Returns `vector<PatternPreprocessorResult>`.

---

### `filter_with_patterns` — `FilterWithPatternsParams`

| Field | Type | Required | Description |
|---|---|---|---|
| `m_pattern_cache_path` | `string` | yes | Full path to the pattern cache from `preprocess_patterns`. |
| `m_pattern_type` | `PatternWriterType` | yes | Pattern cache file format. |
| `m_background_graph_folder` | `string` | yes | Directory containing background graphs. |
| `m_reader_type` | `GraphReaderType` | yes | Graph file format. |
| `m_output_path` | `string` | yes | Directory for filter result output. |
| `m_result_type` | `ResultOutputType` | yes | Filter result format. |
| `m_prior_policy` | `PriorPolicy` | yes | Vertex ordering heuristic. |
| `m_is_directed` | `bool` | — | Treat graphs as directed (default `false`). |
| `m_is_induced` | `bool` | — | Search for induced matches (default `false`). |
| `m_log_file` | `optional<string>` | — | Log file path. |

Returns `vector<unordered_map<string, FilterResult>>`.

---

### `find_subgraph` — `FindSubgraphParams`

| Field | Type | Required | Description |
|---|---|---|---|
| `m_subgraph_path` | `string` | yes | Path to the query subgraph file. |
| `m_background_path` | `string` | yes | Path to the background graph file. |
| `m_reader_type` | `GraphReaderType` | yes | Graph file format. |
| `m_prior_policy` | `PriorPolicy` | yes | Vertex ordering heuristic. |
| `m_is_directed` | `bool` | — | Treat graphs as directed (default `false`). |
| `m_is_induced` | `bool` | — | Search for induced matches (default `false`). |
| `m_stop_on_first_match` | `bool` | — | Stop after the first match (default `false`). |
| `m_output_path` | `optional<string>` | — | File to write match vertex mappings to. |
| `m_log_file` | `optional<string>` | — | Log file path. |

Returns `uint64_t` — the number of matching instances found.

> **Note:** Only **connected** subgraphs are supported. A disconnected query graph always returns 0.

---

## Error codes

All errors thrown by the library are subtypes of `SgfException` (see `include/exceptions/`). CLI tools use the exception's `return_code()` as the process exit status. The C++ API throws these exceptions directly; callers should catch `SgfException` or its subtypes.

| Exception | Exit code | When it is thrown |
|---|---|---|
| `SgfInvalidArgumentException` | 2 | A required CLI flag is missing or has an invalid value. |
| `InvalidArgumentException` | 2 | A library API call received an invalid argument (e.g. conflicting edge colors, no feature flag set). |
| `GraphConstructionException` | 3 | A graph file is malformed, references unknown vertices, or has too many distinct colors. |
| `SgfPathExistsException` | 4 | A required file or directory could not be opened. |
| `SgfDirectoryCreationException` | 5 | A required output directory could not be created. |
| `EnumerationOverflowException` | 6 | A motif/path enumeration count exceeded the representable range. |
| `HistogramOverflowException` | 7 | An internal histogram counter overflowed. |
| `PatternException` | 8 | An error occurred during pattern expansion (e.g. histogram invariant violated). |
| `AddNodeException` | 9 | An error occurred while adding a node to the pattern tree. |
| `DeleteNodeException` | 10 | An attempt was made to delete a non-leaf node from the pattern tree. |
| `MatchFoundException` | 11 | Internal early-exit signal used by `--stop-on-first-match`; not an error, never propagates to the CLI. |

Exit code 0 means success.

---

## Project structure

```
subgraph_filter_suite/
├── include/          # Public headers (one subdirectory per component)
│   ├── api/
│   ├── graph/
│   ├── exceptions/
│   ├── isomorphism/
│   ├── io/
│   │   ├── graph/
│   │   ├── pattern/
│   │   └── cache/
│   ├── managers/
│   ├── preprocessing/
│   ├── filtering/
│   ├── patterns/
│   └── utils/
├── src/              # Implementation files (mirrors include/ layout)
│   └── cli/          # main() for each CLI tool and their argument parsers
├── tests/            # Unit tests (mirrors src/ layout, uses GTest)
├── CMakeLists.txt
└── CLAUDE.md         # Coding conventions and architecture guide
```
