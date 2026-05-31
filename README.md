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
# Configure with CLI disabled
cmake -S . -B build -DSGF_BUILD_CLI=OFF -DCMAKE_BUILD_TYPE=Release

# Build (parallel — uses all available cores on Linux, macOS, and Windows)
cmake --build build --config Release --parallel
```

The library target is `subgraph_filter_suite`. Link against it from your own CMake project with:

```cmake
find_package(subgraph_filter_suite REQUIRED)
target_link_libraries(my_target PRIVATE subgraph_filter_suite)
```

### Library + CLI tools

```bash
# Configure (CLI is ON by default) — also generates compile_commands.json for IDE tooling
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build (parallel — uses all available cores on Linux, macOS, and Windows)
cmake --build build --config Release --parallel
```

This produces three executables in `build/`:
- `sgf-graph-enumerator` — motif/path preprocessing and filtering
- `sgf-pattern-finder` — pattern preprocessing and filtering
- `sgf-graph-searcher` — exact subgraph isomorphism

> **Note on `--config Release` vs `-DCMAKE_BUILD_TYPE=Release`:** CMake has two generator families.
> Single-config generators (Unix Makefiles, Ninja — typical on Linux/macOS) read the build type
> from `-DCMAKE_BUILD_TYPE` at configure time and ignore `--config` at build time.
> Multi-config generators (Visual Studio, Xcode — typical on Windows/macOS) ignore
> `-DCMAKE_BUILD_TYPE` and read the build type from `--config` at build time.
> Passing both, as shown above, ensures a Release build on every platform.

### Compiler/Boost ABI mismatch

If you see linker errors like `undefined reference to GLIBCXX_*`, your default compiler and your Boost installation were built against different C++ standard library ABIs. Fix by pointing CMake at the compiler that built your Boost:

```bash
# Example: Boost installed via conda
cmake -S . -B build -DCMAKE_CXX_COMPILER=$CONDA_PREFIX/bin/g++

# Example: explicit path
cmake -S . -B build -DCMAKE_CXX_COMPILER=/usr/bin/g++-13
```

Users with system Boost and system GCC typically do not need this flag.

## Run tests

```bash
# Run all tests with output on failure
ctest --test-dir build --output-on-failure --build-config Release

# Run a specific test suite by name
ctest --test-dir build -R colored_graph_tests --output-on-failure --build-config Release

# Run a single test by name
ctest --test-dir build -R three_vertices_triangle_returns_empty_map --output-on-failure --build-config Release
```

## Graph file formats

All three tools accept the same `--reader-type` values:

| Value | Extension(s) | Description |
|---|---|---|
| `graphml` | `.graphml` | Standard GraphML XML; vertex and edge `color` attributes are mapped to sequential integer IDs. String color values are supported. |
| `json` | `.json` | JSON object with `"nodes"` (array of `{id, color}`) and `"links"` (array of `{source, target[, color]}`). Colors must be non-negative integers. |
| `vertex-edge` | `.vertex_indices` + `.edges` | Two plain-text files sharing the same base path. Each vertex line is `<id> <color>`; each edge line is `<src> <dst> [color]`. |

**Color handling:** GraphML colors are strings mapped to consecutive integer IDs (the mapping is logged at INFO level). JSON and vertex-edge colors are integer values used directly — no remapping occurs.

## Project structure

```
subgraph_filter_suite/
├── include/          # Public headers (one subdirectory per component)
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
├── tests/            # Unit tests (mirrors src/ layout, uses GTest)
├── CMakeLists.txt
└── CLAUDE.md         # Coding conventions and architecture guide
```

## CLI tools

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
| `--is-directed` | bool switch | Treat all graphs as directed. |
| `--non-induced` | bool switch | Expand induced motif counts via inclusion DAG before filtering (non-induced subgraph search). |
| `--cache-type` | `binary` \| `csv` | Cache file format. |
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
| `--graph-cache-dir` | string | Directory for query graph enumeration cache (required with `--cache-enumeration`). |
| `--load-motif-graph-cache` | string | Full path to existing motif enumeration cache to load instead of computing. Mutually exclusive with `--cache-enumeration`. |
| `--load-path-graph-cache` | string | Full path to existing path enumeration cache to load instead of computing. Mutually exclusive with `--cache-enumeration`. |

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
| `--is-directed` | bool switch | Treat all graphs as directed. |
| `--reader-type` | `graphml` \| `json` \| `vertex-edge` | Graph file format. |
| `--log-file-path` | string | *(optional)* Path to log file. |

**Preprocess flags** (required with `--preprocess`):

| Flag | Type | Default | Description |
|---|---|---|---|
| `--library-input-folder` | string | — | Directory containing the graph library. |
| `--output-folder` | string | — | Directory where pattern files are written. |
| `--pattern-output-type` | `graphml` \| `json` \| `vertex-edge` | — | Pattern file format. |
| `--preprocess-single-graph` | integer | `0` | Index of single graph to process in single-graph mode. |
| `--preprocess-single-graph-from-results` | bool switch | — | Derive the graph index from a results file instead of `--preprocess-single-graph`. |
| `--results-file-path` | string | — | Path to results file (required with `--preprocess-single-graph-from-results`). |
| `--results-file-type` | `json` \| `csv` | `json` | Format of the results file. |
| `--background-graph-path` | string | — | *(optional)* Path to background graph for pattern scoring. |
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
| `--prior-policy` | string | Vertex ordering heuristic (see below). |
| `--is-induced` | bool switch | Search for induced subgraph matches. |

**Prior policy values** for `--prior-policy`:

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

Runs exact subgraph isomorphism between a query subgraph and a background graph and prints the count of matches.

> **Important:** `sgf-graph-searcher` only finds **connected** subgraph instances. If the query subgraph is disconnected (contains two or more separate connected components), the search returns 0. Ensure the subgraph file contains a single connected component.

| Flag | Type | Description |
|---|---|---|
| `--subgraph-path` | string | Path to the query subgraph file (required). |
| `--background-path` | string | Path to the background graph file (required). |
| `--reader-type` | `graphml` \| `json` \| `vertex-edge` | Graph file format (required). |
| `--is-directed` | bool switch | Treat graphs as directed. |
| `--is-induced` | bool switch | Search for induced subgraph matches only. |
| `--prior-policy` | string | Vertex ordering heuristic (same values as `sgf-pattern-finder`). |
| `--stop-on-first-match` | bool switch | Stop after the first match is found (returns 1). |
| `--output-path` | string | *(optional)* Path to file where match vertex mappings are written. |
| `--log-file-path` | string | *(optional)* Path to log file. |

**Example:**

```bash
./build/sgf-graph-searcher \
  --subgraph-path ./query.json \
  --background-path ./background.json \
  --reader-type json \
  --prior-policy subgraph-degree
```

## Exception handling

All errors thrown by the library are subtypes of `SgfException` (see `include/exceptions/`). CLI tools use the `return_code()` of the caught exception as the process exit status.

| Exception | Exit code | Meaning |
|---|---|---|
| `SgfInvalidArgumentException` | 2 | A required CLI flag is missing or has an invalid value. |
| `InvalidArgumentException` | 2 | A library API call received an invalid argument (e.g. conflicting edge colors). |
| `GraphConstructionException` | 3 | A graph file is malformed, references unknown vertices, or has too many distinct colors. |
| `SgfPathExistsException` | 4 | A required file or directory could not be opened. |
| `SgfDirectoryCreationException` | 5 | A required output directory could not be created. |
| `EnumerationOverflowException` | 6 | The motif/path enumeration count exceeded the representable range. |
| `HistogramOverflowException` | 7 | An internal histogram counter overflowed. |
| `PatternException` | 8 | An error occurred during pattern expansion (e.g. histogram invariant violated). |
| `AddNodeException` | 9 | An error occurred while adding a node to the pattern tree (e.g. invalid parent ordering). |
| `DeleteNodeException` | 10 | An attempt was made to delete a non-leaf node from the pattern tree. |
| `MatchFoundException` | 11 | Internal signal used when `--stop-on-first-match` terminates the search early; not an error. |

Exit code 0 means success. All non-zero codes above indicate a fatal error except `MatchFoundException` (11), which is an internal early-exit signal and does not propagate to the CLI.
