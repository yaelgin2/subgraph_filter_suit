# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

`subgraph_filter_suit` is a C++ library for efficient subgraph matching. It preprocesses a library of graphs and uses multi-stage filtering (motifs, paths, and patterns) to eliminate unlikely candidates before exact isomorphism, reducing computation time for large-scale graph queries.

## Build & Test Commands

```bash
# Configure and build
cmake -S . -B build
cmake --build build --config Release

# Run all tests
cd build && ctest --output-on-failure -C Release

# Run a single test by name
cd build && ctest -R <test_name> --output-on-failure

# Check formatting (must pass with no errors)
find . -name '*.cpp' -o -name '*.h' | xargs clang-format --dry-run --Werror

# Lint
find . -name '*.cpp' | xargs clang-tidy
```

## Architecture

The library implements a three-stage filtering pipeline to efficiently answer subgraph isomorphism queries over a large graph library:

1. **Motif filtering** — eliminates candidates based on small structural motifs
2. **Path filtering** — eliminates candidates based on path signatures
3. **Pattern filtering** — eliminates candidates based on pattern features

Only candidates that pass all three filters are subjected to exact isomorphism matching. This staged approach dramatically reduces the number of expensive exact-match computations needed for large-scale queries.

## CI/CD

GitHub Actions runs on Linux and Windows, executing build → test → format check → lint in that order. The pipeline is defined in `.github/workflows/ci.yml`.
