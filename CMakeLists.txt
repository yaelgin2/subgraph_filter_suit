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
find_package(Boost 1.76 REQUIRED COMPONENTS graph log log_setup)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/include/io/graph"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/logging"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/managers"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/preprocessing"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/filtering"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/patterns"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/utils"
)

target_link_libraries(subgraph_filter_suite PUBLIC
    Boost::log
    Boost::log_setup
    Boost::graph
)


# ── Compiler warnings ─────────────────────────────────────────────────────────
target_compile_options(subgraph_filter_suite PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
