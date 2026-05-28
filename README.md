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

This produces the `sgf-graph-enumerator` executable in the `build/` directory in addition to the library.

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

# Run single test
ctest --test-dir build -R three_vertices_triangle_returns_empty_map --output-on-failure --build-config Release

```

## Project structure

```
subgraph_filter_suite/
├── include/          # Public headers (one subdirectory per component)
│   ├── graph/
│   ├── exceptions/
│   ├── isomorphism/
│   ├── io/
|   |   ├──graph
|   |   ├──pattern
|   |   └──cache
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
