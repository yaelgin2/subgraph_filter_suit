# Prefer the GTest already installed in the active environment (e.g. conda) so
# that the compiler, headers, and library all share the same ABI.  Fall back to
# FetchContent only when no installed copy is found.
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

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

add_sgf_test(colored_graph_tests                        graph/ColoredGraphTest.cpp)
add_sgf_test(graph_utils_tests                          utils/GraphUtilsTest.cpp)

add_sgf_test(graphml_reader_tests                       io/graph/GraphmlGraphReaderTest.cpp)
add_sgf_test(json_reader_tests                          io/graph/JsonGraphReaderTest.cpp)
add_sgf_test(vertex_edge_reader_tests                   io/graph/VertexEdgeGraphReaderTest.cpp)

add_sgf_test(json_pattern_writer_tests                  io/pattern/JsonPatternWriterTest.cpp)
add_sgf_test(vertex_edge_pattern_writer_tests           io/pattern/VertexEdgePatternWriterTest.cpp)
add_sgf_test(graphml_writer_tests                       io/pattern/GraphmlPatternWriterTest.cpp)

add_sgf_test(line_graph_searcher_tests                  isomorphism/SubgraphSearcherTest.cpp)

add_sgf_test(tree_tests                                 patterns/tree/TreeTest.cpp)

add_sgf_test(single_graph_pattern_finder_tests          patterns/pattern_finders/SingleGraphPatternFinderTest.cpp)
add_sgf_test(multi_graph_pattern_finder_tests           patterns/pattern_finders/MultiGraphPatternFinderTest.cpp)

add_sgf_test(motif_preprocessor_tests                   preprocessing/MotifPreprocessorTest.cpp)
if(MSVC)
    target_link_options(motif_preprocessor_tests PRIVATE /STACK:8388608)
elseif(MINGW)
    target_link_options(motif_preprocessor_tests PRIVATE -Wl,--stack,8388608)
endif()
if(SGF_ENABLE_CUDA)
    add_sgf_test(gpu_motif_preprocessor_tests           preprocessing/GpuMotifPreprocessorTest.cu)
    set_target_properties(gpu_motif_preprocessor_tests PROPERTIES
        CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}"
        CUDA_SEPARABLE_COMPILATION ON
        LINKER_LANGUAGE CXX)
    target_link_libraries(gpu_motif_preprocessor_tests PRIVATE cuco CUDA::cudart)
    target_compile_options(gpu_motif_preprocessor_tests PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr>)
endif()
add_sgf_test(path_processor_tests                       preprocessing/PathProcessorTest.cpp)
if(SGF_ENABLE_CUDA)
    add_sgf_test(gpu_path_processor_tests               preprocessing/GpuPathProcessorTest.cu)
    set_target_properties(gpu_path_processor_tests PROPERTIES
        CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}"
        CUDA_SEPARABLE_COMPILATION ON
        LINKER_LANGUAGE CXX)
    target_link_libraries(gpu_path_processor_tests PRIVATE cuco CUDA::cudart)
    target_compile_options(gpu_path_processor_tests PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda --expt-relaxed-constexpr>)
endif()
add_sgf_test(single_graph_pattern_preprocessor_tests    preprocessing/SingleGraphPatternPreprocessorTest.cpp)

add_sgf_test(enumeration_preprocess_manager_tests       managers/EnumerationPreprocessManagerTest.cpp)

add_sgf_test(group_enumeration_graph_filter_tests       filtering/GroupEnumerationGraphFilterTest.cpp)

add_sgf_test(csv_cache_io_manager_tests                 io/cache/CSVCacheIOManagerTest.cpp)
add_sgf_test(binary_cache_io_manager_tests              io/cache/BinaryCacheIOManagerTest.cpp)

# The graphml reader tests need to locate the test-data directory at compile time.
target_compile_definitions(graphml_reader_tests PRIVATE
    GRAPHML_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/io/graphml"
)

# The json reader tests need to locate the test-data directory at compile time.
target_compile_definitions(json_reader_tests PRIVATE
    JSON_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/io/json"
)

# The vertex-edge reader tests need to locate the test-data directory at compile time.
target_compile_definitions(vertex_edge_reader_tests PRIVATE
    VERTEX_EDGE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/io/vertex_edge"
)
