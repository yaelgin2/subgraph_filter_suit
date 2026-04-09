cmake_minimum_required(VERSION 3.16)
project(subgraph_filter_suite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clang-tidy and IDE tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Sources ───────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/**/*.cpp)

add_library(subgraph_filter_suite ${SOURCES})

# ── Include paths ─────────────────────────────────────────────────────────────
# Add include/ and every subdirectory under it so headers can be included by
# filename alone (e.g. #include "ColoredGraph.h") without a path prefix.
target_include_directories(subgraph_filter_suite PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/graph"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/exceptions"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/isomorphism"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/io"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/managers"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/preprocessing"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/filtering"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/patterns"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/utils"
)

# ── Compiler warnings ─────────────────────────────────────────────────────────
target_compile_options(subgraph_filter_suite PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
