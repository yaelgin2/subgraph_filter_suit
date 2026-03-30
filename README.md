# subgraph_filter_suite

A C++ library for efficient subgraph matching. It preprocesses a library of graphs and uses a three-stage filtering pipeline (motif, path, and pattern filtering) to eliminate unlikely candidates before exact isomorphism, dramatically reducing computation time for large-scale graph queries.

## Requirements

- CMake 3.16+
- A C++17-capable compiler (GCC, Clang, or MSVC)
- Internet access on first build (GTest is fetched automatically via CMake FetchContent)

## Build

```bash
# Configure — downloads GTest and generates compile_commands.json for IDE tooling
cmake -S . -B build

# Build (Release)
cmake --build build --config Release
```

## Run tests

```bash
# Run all tests with output on failure
cd build && ctest --output-on-failure -C Release

# Run a specific test suite by name
cd build && ctest -R colored_graph_tests --output-on-failure
```

## Project structure

```
subgraph_filter_suite/
├── include/          # Public headers (one subdirectory per component)
│   ├── graph/
│   ├── exceptions/
│   ├── isomorphism/
│   ├── io/
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

## Exception handling

All errors thrown by the library are subtypes of `SgfException` (see `include/exceptions/`). CLI tools use the `return_code()` of the caught exception as the process exit status:

| Exception | Exit code |
|---|---|
| `InvalidArgumentException` | 2 |
| `GraphConstructionException` | 3 |
