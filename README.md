# subgraph_filter_suite

A C++ library for efficient subgraph matching. It preprocesses a library of graphs and uses a three-stage filtering pipeline (motif, path, and pattern filtering) to eliminate unlikely candidates before exact isomorphism, dramatically reducing computation time for large-scale graph queries.

## Requirements

- CMake 3.16+
- A C++17-capable compiler (GCC, Clang, or MSVC)
- Boost 1.76+ (`graph`, `log`, `log_setup` components)
- Internet access on first build (GTest is fetched automatically via CMake FetchContent)

## Build

```bash
# Configure — downloads GTest and generates compile_commands.json for IDE tooling
cmake -S . -B build

# Build (Release)
cmake --build build --config Release
```

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
