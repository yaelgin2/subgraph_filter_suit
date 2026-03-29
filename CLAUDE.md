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

# Run clang-tidy (requires build/ with compile_commands.json)
find . -name '*.cpp' -not -path './build/*' | xargs clang-tidy -p build/
```

## Code Conventions

All conventions below are enforced by `.clang-format` and `.clang-tidy` in the repo root, which are auto-detected by both tools (they walk up the directory tree from each source file).

### Documentation

Every function, class, struct, and enum **must** have a Doxygen comment block:

```cpp
/// @brief One-line summary.
/// @param graph The input graph.
/// @param query The query subgraph.
/// @return True if a matching subgraph exists.
bool findSubgraph(const Graph& graph, const Graph& query);
```

> clang-tidy does not enforce Doxygen presence — this is a code-review requirement.
> To catch undocumented symbols automatically, run `doxygen` with `WARN_IF_UNDOCUMENTED = YES`
> and `WARN_AS_ERROR = YES` in `Doxyfile`.

### File structure

- **One newline at end of every file** — enforced by clang-format (`InsertNewlineAtEOF: true`).
- **Two blank lines between top-level function definitions** — clang-format enforces a minimum of one blank line (`SeparateDefinitionBlocks: Always`) and preserves a second one (`MaxEmptyLinesToKeep: 2`). Always write two blank lines; clang-format will not add the second automatically.

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

### `auto` usage

Use `auto` **only** in range-based `for` loops. Everywhere else, spell out the full type explicitly — including iterator types:

```cpp
// Allowed
for (auto& node : graph.nodes()) { ... }

// Required — do not use auto here
std::vector<int>::iterator it = container.begin();
std::shared_ptr<Graph> graph = std::make_shared<Graph>();
```

### Modernize

- Prefer range-based `for` loops (`modernize-loop-convert`).
- Use `override` / `final` on virtual overrides (`modernize-use-override`).
- Use `std::make_unique` / `std::make_shared` over `new` (`modernize-make-unique`, `modernize-make-shared`).

### Naming (enforced by `readability-identifier-naming`)

| Entity | Convention |
|---|---|
| Classes / Structs / Enums | `CamelCase` |
| Functions / variables / parameters / members | `camelCase` |
| Private members | `camelCase_` (trailing underscore) |
| Constants / enum values | `UPPER_CASE` |
| Namespaces | `lower_case` |

## Architecture

The library implements a three-stage filtering pipeline to efficiently answer subgraph isomorphism queries over a large graph library:

1. **Motif filtering** — eliminates candidates based on small structural motifs
2. **Path filtering** — eliminates candidates based on path signatures
3. **Pattern filtering** — eliminates candidates based on pattern features

Only candidates that pass all three filters are subjected to exact isomorphism matching. This staged approach dramatically reduces the number of expensive exact-match computations needed for large-scale queries.

## CI/CD

GitHub Actions (`.github/workflows/ci.yml`) runs on Linux and Windows in this order: configure → build → test → format-check → clang-tidy.

- Configure exports `compile_commands.json` (`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`) so clang-tidy has full include-path context.
- Both `.clang-format` and `.clang-tidy` are picked up automatically by the tools — no extra flags needed to point at them.

## Git

Never commit together two different changes. A commit message should be short and explain the change, if it has two changes to explain this should be two different commits.