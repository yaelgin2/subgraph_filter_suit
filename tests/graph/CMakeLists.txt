add_executable(colored_graph_tests ColoredGraphTest.cpp)

target_link_libraries(colored_graph_tests
    PRIVATE
    subgraph_filter_suite
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(colored_graph_tests)
