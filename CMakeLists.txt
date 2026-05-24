cmake_minimum_required(VERSION 3.16)
project(subgraph_filter_suite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler version check ────────────────────────────────────────────────────
# Enforce minimums that guarantee C++17 support and a stable ABI.
# Mixing a compiler below these versions with a Boost built by a newer one
# causes GLIBCXX / MSVC CRT linker errors at link time.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "7.0")
        message(FATAL_ERROR
            "GCC 7+ required (found ${CMAKE_CXX_COMPILER_VERSION}). "
            "Using an older GCC against a newer Boost causes libstdc++ ABI mismatches.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0")
        message(FATAL_ERROR
            "Clang 5+ required (found ${CMAKE_CXX_COMPILER_VERSION}). "
            "Older Clang lacks full C++17 support and may produce ABI-incompatible objects.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # 19.14 == Visual Studio 2017 15.7, the first MSVC with complete C++17
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.14")
        message(FATAL_ERROR
            "MSVC 19.14+ (VS 2017 15.7+) required (found ${CMAKE_CXX_COMPILER_VERSION}). "
            "Earlier versions lack full C++17 and use an incompatible CRT ABI with Boost.")
    endif()
endif()

# Export compile_commands.json for clang-tidy and IDE tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Boost ─────────────────────────────────────────────────────────────────────
find_package(Boost 1.76 REQUIRED COMPONENTS graph log log_setup json program_options)

# ── Sources ───────────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/**/*.cpp)
# Exclude the CLI directory so the library remains usable without the CLI tools.
list(FILTER SOURCES EXCLUDE REGEX ".*/cli/.*")
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
    "${CMAKE_CURRENT_SOURCE_DIR}/include/io/graph"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/io/pattern"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/io/cache"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/io/filter"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/logging"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/managers"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/preprocessing"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/preprocessing/enumeration"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/preprocessing/pattern"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/filtering"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/patterns"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/patterns/histograms"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/patterns/pattern"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/patterns/tree"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/patterns/pattern_finders"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/utils"
)

target_link_libraries(subgraph_filter_suite PUBLIC
    Boost::log
    Boost::log_setup
    Boost::graph
    Boost::json
)

# Mark Boost headers as system includes so compilers suppress third-party warnings.
# This removes the need for the GCC-only -Wno-maybe-uninitialized workaround.
foreach(_boost_target Boost::graph Boost::log Boost::log_setup Boost::json Boost::program_options)
    if(TARGET ${_boost_target})
        get_target_property(_boost_incs ${_boost_target} INTERFACE_INCLUDE_DIRECTORIES)
        if(_boost_incs)
            set_target_properties(${_boost_target} PROPERTIES
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_boost_incs}")
        endif()
    endif()
endforeach()

# ── Compiler warnings ─────────────────────────────────────────────────────────
# -Wno-maybe-uninitialized: GCC 14 emits false-positive maybe-uninitialized
# warnings inside Boost 1.85 adjacency_list copy_impl / add_edge inlined paths.
# This is a known GCC Bug #105329; the project code is correct.
# clang-tidy uses -Wno-unknown-warning-option (set in .clang-tidy ExtraArgsBefore)
# to silently ignore this GCC-only flag when Clang parses compile_commands.json.
target_compile_options(subgraph_filter_suite PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -Werror -Wno-maybe-uninitialized>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc>
)

# ── CLI tools ─────────────────────────────────────────────────────────────────
# Set SGF_BUILD_CLI=OFF at configure time to build only the library.
option(SGF_BUILD_CLI "Build the sgf-graph-enumerator CLI tool" ON)
if(SGF_BUILD_CLI)
    add_executable(sgf-graph-enumerator src/cli/SgfGraphEnumerator.cpp)
    target_link_libraries(sgf-graph-enumerator PRIVATE
        subgraph_filter_suite
        Boost::program_options
    )
    target_compile_options(sgf-graph-enumerator PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -Werror -Wno-maybe-uninitialized>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic -Werror>
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /EHsc>
    )
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
