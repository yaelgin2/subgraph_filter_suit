# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

`subgraph_filter_suit` is a C++ library for efficient subgraph matching. It preprocesses a library of graphs and uses multi-stage filtering (motifs, paths, and patterns) to eliminate unlikely candidates before exact isomorphism, reducing computation time for large-scale graph queries.

## Build & Test Commands

```bash
# Configure (export compile_commands.json for clang-tidy)
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
cmake --build build --config Release

# Run all tests
cd build && ctest --output-on-failure -C Release

# Run a single test by name
cd build && ctest -R <test_name> --output-on-failure

# Auto-fix formatting in place
clang-format -i $(find . -name '*.cpp' -o -name '*.h' | grep -v build)

# Check formatting without modifying (what CI runs)
find . -name '*.cpp' -o -name '*.h' | grep -v build | xargs clang-format --dry-run --Werror

# Run clang-tidy on all project .cpp files
# IMPORTANT: run from the project root, NOT from inside build/ — otherwise find
# searches the build tree and the -not -path filter has no effect.
# HeaderFilterRegex in .clang-tidy ensures project headers are also linted while
# excluding third-party headers under build/_deps/ (e.g. GTest).
# --quiet suppresses the "N warnings generated" noise (raw count before header filtering).
find . -name '*.cpp' -not -path './build/*' | xargs clang-tidy -p build/ --quiet
```

## Code Conventions

All conventions below are enforced by `.clang-format` and `.clang-tidy` in the repo root, which are auto-detected by both tools (they walk up the directory tree from each source file).

### Documentation

Every function, class, struct, and enum **must** have a Doxygen comment block:

```cpp
/**
 * @brief One-line summary.
 * @param graph The input graph.
 * @param query The query subgraph.
 * @return True if a matching subgraph exists.
 */
bool findSubgraph(const Graph& graph, const Graph& query);
```

> clang-tidy does not enforce Doxygen presence — this is a code-review requirement.
> To catch undocumented symbols automatically, run `doxygen` with `WARN_IF_UNDOCUMENTED = YES`
> and `WARN_AS_ERROR = YES` in `Doxyfile`.

### File structure

- **One newline at end of every file** — enforced by clang-format (`InsertNewlineAtEOF: true`).
- **One blank line between top-level function definitions** — clang-format enforces this via `SeparateDefinitionBlocks: Always`.
- **Curly braces on seperate lines**

### Function limits (enforced by `readability-function-size`)

| Metric | Limit |
|---|---|
| Lines | 50 |
| Branches (`if`/`else`/`switch`/`?:`) | 5 |
| Parameters | 5 |
| Local variables | 10 |
| Statements | 30 |
| Nesting depth | 3 |

### Memory management

**No raw pointers.** Use smart pointers and references exclusively:

| Use case | Type |
|---|---|
| Sole ownership | `std::unique_ptr<T>` |
| Shared ownership | `std::shared_ptr<T>` |
| Non-owning observer (uncertain lifetime) | `std::weak_ptr<T>` |
| Non-owning parameter (known lifetime) | `T&` / `const T&` |

- No `T*` anywhere — not for ownership, not for observation.
- No `new` / `delete` — always use `std::make_unique` or `std::make_shared`.
- Enforced by `cppcoreguidelines-owning-memory`, `modernize-make-unique`, `modernize-make-shared`.

### Type safety

- No C-style casts — use `static_cast`, `dynamic_cast`, or `std::bit_cast`.
- No `reinterpret_cast` except in explicitly justified low-level code.
- Prefer `nullptr` over `NULL` or `0`.
- Enforced by `cppcoreguidelines-pro-type-*` and `modernize-*` checks.

### Fixed-width integer types

Never use `int`, `long`, `short`, or `unsigned` — their bit widths are compiler- and platform-dependent. Always use types from `<cstdint>` whose widths are guaranteed:

| Instead of | Use |
|---|---|
| `int` / `short` | `int32_t` or `int64_t` |
| `unsigned` / `unsigned int` | `uint32_t` or `uint64_t` |
| `long` / `long long` | `int64_t` |
| `unsigned long` | `uint64_t` |

Use `size_t` only for container sizes and pointer arithmetic (where the standard library requires it). Enforced at code review; no clang-tidy check covers this automatically.

### Prefer unsigned types

Use the smallest unsigned type that covers the value range. Negative values should be rare — if a value can never be negative, make that explicit with an unsigned type. When mixing signed and unsigned in expressions, cast explicitly with `static_cast`.

### `const` correctness

Be as `const` as possible everywhere:

- **Member functions** — mark every method that does not mutate state as `const`.
- **Parameters** — mark every value parameter as `const`. Pass objects by `const&` unless the function needs to modify or take ownership.
- **Local variables** — mark every variable `const` unless it is reassigned.

```cpp
// Correct
uint32_t compute_degree(const uint32_t vertex_id) const;
bool is_edge(const uint32_t source_vertex, const uint32_t dest_vertex) const;

// Wrong — missing const on method, parameter, and local
uint32_t compute_degree(uint32_t vertex_id);
```

Enforced by `readability-make-member-function-const` and `readability-non-const-parameter` (both included in `readability-*`).

### `auto` usage

Use `auto` **only** in range-based `for` loops. Everywhere else, spell out the full type explicitly — including iterator types:

```cpp
// Allowed
for (auto& node : graph.nodes()) { ... }

// Required — do not use auto here
std::vector<int32_t>::iterator it = container.begin();
std::shared_ptr<Graph> graph = std::make_shared<Graph>();
```

### Modernize

- Prefer range-based `for` loops (`modernize-loop-convert`).
- Use `override` / `final` on virtual overrides (`modernize-use-override`).
- Use `std::make_unique` / `std::make_shared` over `new` (`modernize-make-unique`, `modernize-make-shared`).

### Namespace

All library code lives in the `sgf` namespace. Every header and source file wraps its declarations in `namespace sgf { ... }`. Tests and CLI tools are consumers — they use `using namespace sgf;` at the top of the file and are not wrapped in the namespace.

Internal-linkage helpers inside a single `.cpp` or test file go in an anonymous `namespace { ... }` — not `static`, not `sgf`.

### Naming (enforced by `readability-identifier-naming`)

| Entity | Convention |
|---|---|
| Classes / Structs / Enums | `CamelCase` |
| Functions / methods / local variables / parameters | `snake_case` |
| Members (all) | `m_snake_case` (e.g. `m_edge_count`) |
| Constants / enum values | `UPPER_CASE` |
| Namespaces | `lower_case` |

### No duplicate code

Never write the same logic twice. If a block of code appears more than once — even with minor variations (different variables, different vectors) — extract it into a named function immediately. Apply this rule at the statement level: two `if` bodies that do the same thing are already one too many.

### No magic numbers

Never use bare numeric or string literals in logic — always bind them to a named constant first:

```cpp
// Wrong
if (vertex_count > 1000) { ... }

// Correct
constexpr uint32_t MAX_VERTEX_COUNT = 1000;
if (vertex_count > MAX_VERTEX_COUNT) { ... }
```

Exception return codes must be values of `SgfReturnCode` (see `include/exceptions/SgfReturnCode.h`) — never raw integers. Enforced by `cppcoreguidelines-avoid-magic-numbers` and `readability-magic-numbers` in `.clang-tidy`.

### Name clarity

All identifiers must be fully descriptive — no single-letter or cryptic abbreviations (no `u`, `v`, `i`, `n`, `t`, etc.). Well-known, universally understood short forms are acceptable (e.g. `src`, `dest`, `idx`, `num`, `min`, `max`). When in doubt, spell it out.

### Exception handling

All exceptions thrown by this package must inherit from `SgfException` (`include/exceptions/SgfException.h`). The C++ API must **never** let a non-`SgfException` propagate to the caller.

**Catch only what you know can be thrown.** Never use `catch (const std::exception&)` as a blanket handler — it masks unexpected errors and hides bugs. Identify the exact exception types each called function can throw and catch those specifically. If a called function can only throw `FooException` and `BarException`, only those two should appear in the catch list. Re-wrap each caught exception as the appropriate `SgfException` subclass.

Each concrete exception class defines a **unique** `return_code()` used by CLI tools as the process exit status:

| Exception | Header | Return code |
|---|---|---|
| `InvalidArgumentException` | `include/exceptions/InvalidArgumentException.h` | 2 |
| `GraphConstructionException` | `include/exceptions/GraphConstructionException.h` | 3 |

When adding a new exception type, add it to this table and do not reuse an existing return code.

### Memory allocation responsibility

- **Constructor** — validates inputs, sets scalar state, and orchestrates calls to builder helpers. No vector allocation or resize.
- **Internal builder functions** (e.g. `initiate_graph`) — own all `resize` / `reserve` / `assign` for the data structures they build.

## Architecture

The library implements a three-stage filtering pipeline to efficiently answer subgraph isomorphism queries over a large graph library:

1. **Motif filtering** — eliminates candidates based on small structural motifs
2. **Path filtering** — eliminates candidates based on path signatures
3. **Pattern filtering** — eliminates candidates based on pattern features

Only candidates that pass all three filters are subjected to exact isomorphism matching. This staged approach dramatically reduces the number of expensive exact-match computations needed for large-scale queries.

### Component Map

**Graph** (`graph/`) — core data structure:
- `ColoredGraph` — adjacency structure with per-vertex color labels; the central data type used throughout the entire pipeline

**Isomorphism** (`isomorphism/`) — exact matching algorithm (VELCRO):
- `GraphSubgraphIsomorphism` — runs the VELCRO subgraph isomorphism algorithm between two `ColoredGraph` instances

**I/O** (`io/`) — reading, writing, and caching:
- `IGraphReader` → `GraphmlGraphReader`, `JsonGraphReader`, `VertexEdgeGraphReader`
- `IPatternIOManager` → `GraphmlPatternIOManager`, `JsonPatternIOManager`, `VertexEdgePatternIOManager`
- `ICacheManager` → `CSVCacheManager`, `BinaryCacheManager`

**Managers** (`managers/`) — high-level orchestration:
- `FlowManager` — top-level pipeline runner; drives enumerate, filter, pattern-preprocess, pattern-filter, and matrix runs
- `LibraryLoader` — loads a graph library from disk
- `FilterOutputManager` — collects results and writes the output index/filter file
- `EnumerationPreprocessManager` — coordinates the full enumeration preprocessing pass over a library
- `PatternPreprocessManager` — coordinates pattern preprocessing for a query graph

**Preprocessing** (`preprocessing/`) — signature computation before filtering, split into two groups:
- Enumeration preprocessors: `IGraphPreprocessor` (interface) → `GroupEnumerationPreprocessor` (implements it) ← extended by `MotifPreprocessor` and `FivePathPreprocessor`
- Pattern preprocessors: `SingleGraphPatternPreprocessor`, `MultiGraphPatternPreprocessor`

**Filtering** (`filtering/`) — candidate elimination:
- `GroupEnumerationGraphFilter` — filters library candidates using enumeration (motif/path) signatures
- `PatternGraphFilter` — filters by pattern features

**Pattern finders** (`patterns/`) — used by the pattern finder preprocessor:
- `SingleGraphPatternFinder` — finds patterns for a single query graph
- `MultiGraphPatternFinder` — finds patterns across multiple query graphs

**Utils** (`utils/`) — one shared utility file (e.g. `convert_boost_graph_to_colored_graph`)

All components compile into three CLI tools and are also exposed as a C++ API:

### CMake include paths

Every subdirectory under `include/` is listed explicitly in `target_include_directories` in `CMakeLists.txt`. This allows all headers to be included by filename alone (e.g. `#include "ColoredGraph.h"`) and ensures IDE IntelliSense resolves them without requiring `compile_commands.json`.

**When adding a new component subdirectory under `include/`, always add the corresponding path to `target_include_directories` in `CMakeLists.txt`.**

### Expected File Structure

```
subgraph_filter_suit/
├── include/
│   ├── graph/
│   ├── isomorphism/
│   ├── io/
│   ├── managers/
│   ├── preprocessing/
│   ├── filtering/
│   ├── patterns/
│   └── utils/
├── src/
│   ├── graph/
│   ├── isomorphism/
│   ├── io/
│   ├── managers/
│   ├── preprocessing/
│   ├── filtering/
│   ├── patterns/
│   ├── utils/
│   └── cli/              # main() for sgf-graph-enumerator, sgf-pattern-finder, sgf-matrix
├── tests/
│   ├── graph/
│   ├── isomorphism/
│   ├── io/
│   ├── managers/
│   ├── preprocessing/
│   ├── filtering/
│   ├── patterns/
│   └── utils/
├── .clang-format
├── .clang-tidy
├── .github/
│   └── workflows/
│       └── ci.yml
├── CMakeLists.txt
└── CLAUDE.md
```

## CI/CD

GitHub Actions (`.github/workflows/ci.yml`, on `develop` branch) runs on Linux and Windows in this order: configure → build → test → format-check → clang-tidy.

**Required CI update:** the current CI does not export `compile_commands.json` or pass `-p build/` to clang-tidy, which means many checks run without include-path context. Update the CI as follows:

```yaml
# Configure step — add flag:
run: cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# clang-tidy step — add -p:
run: find . -name '*.cpp' -not -path './build/*' | xargs clang-tidy -p build/
```

Both `.clang-format` and `.clang-tidy` are picked up automatically by the tools — no extra flags needed to point at them.

## Git

Never commit together two different changes. A commit message should be short and explain the change, if it has two changes to explain this should be two different commits.