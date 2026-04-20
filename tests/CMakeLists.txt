include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# ---------------------------------------------------------------------------
# add_sgf_test(<target> <source>)
#
# Creates a test executable, links it against the library and GTest, adds the
# tests/ root to its include path (so shared headers like
# ColoredGraphTestHelpers.h are reachable without relative paths), and
# registers all discovered tests with CTest.
# ---------------------------------------------------------------------------
function(add_sgf_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target}
        PRIVATE
        subgraph_filter_suite
        GTest::gtest_main
    )
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    gtest_discover_tests(${target})
endfunction()

# ── Test suites ───────────────────────────────────────────────────────────────

add_sgf_test(colored_graph_tests  graph/ColoredGraphTest.cpp)
add_sgf_test(graph_utils_tests    utils/GraphUtilsTest.cpp)
add_sgf_test(graphml_reader_tests io/graph/GraphmlGraphReaderTest.cpp)
add_sgf_test(json_reader_tests    io/graph/JsonGraphReaderTest.cpp)

# The graphml reader tests need to locate the test-data directory at compile time.
target_compile_definitions(graphml_reader_tests PRIVATE
    GRAPHML_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/io/graphml"
)

# The json reader tests need to locate the test-data directory at compile time.
target_compile_definitions(json_reader_tests PRIVATE
    JSON_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/io/json"
)
