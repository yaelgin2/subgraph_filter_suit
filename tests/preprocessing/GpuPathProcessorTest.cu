#ifdef SGF_CUDA_ENABLED
#include "ColoredGraph.h"
#include "FileLogger.h"
#include "ILogger.h"
#include "LoggerHandler.h"
#include "PathProcessor.h"

#include <cstdint>
#include <cuda_runtime.h>
#include <gtest/gtest.h>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace sgf;

namespace
{

class GpuPathProcessorTest : public ::testing::Test
{
protected:
    /**
     * @brief Skip test if no CUDA-capable GPU is present on this machine.
     */
    void SetUp() override
    {
        int32_t device_count = 0;
        if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count == 0)
        {
            GTEST_SKIP() << "No CUDA-capable GPU found";
        }
    }

    /**
     * @brief Creates a no-op LoggerHandler for use in tests.
     * @return LoggerHandler backed by an expired weak_ptr (all log calls are no-ops).
     */
    static LoggerHandler null_logger()
    {
        return LoggerHandler{std::weak_ptr<ILogger>{}};
    }

    /**
     * @brief Run both the CPU and GPU path enumeration backends on @p graph and
     * assert they produce identical results.
     * @param graph Graph to enumerate paths on.
     * @return The (identical) CPU/GPU result map, for further assertions.
     */
    static std::unordered_map<UInt128, uint32_t, UInt128Hash>
    run_and_compare(const ColoredGraph& graph)
    {
        PathProcessor cpu_processor(graph, null_logger());
        PathProcessor gpu_processor(graph, null_logger());
        const std::unordered_map<UInt128, uint32_t, UInt128Hash> cpu_result =
            cpu_processor.calculate(false);
        const std::unordered_map<UInt128, uint32_t, UInt128Hash> gpu_result =
            gpu_processor.calculate(true);

        // ASSERT_* requires a void-returning function, so every check here is EXPECT_*
        // (non-fatal) — this function returns gpu_result for further assertions by the caller.
        EXPECT_EQ(cpu_result.size(), gpu_result.size());
        for (const std::pair<const UInt128, uint32_t>& entry : cpu_result)
        {
            const std::unordered_map<UInt128, uint32_t, UInt128Hash>::const_iterator gpu_it =
                gpu_result.find(entry.first);
            if (gpu_it == gpu_result.cend())
            {
                ADD_FAILURE() << "GPU result missing a CPU-found path";
                continue;
            }
            EXPECT_EQ(gpu_it->second, entry.second)
                << "GPU/CPU path count mismatch for one canonical path id";
        }
        return gpu_result;
    }
};

TEST_F(GpuPathProcessorTest, empty_graph_matches_cpu)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges;
    const ColoredGraph graph(0U, edges, {}, false);
    EXPECT_TRUE(run_and_compare(graph).empty());
}

TEST_F(GpuPathProcessorTest, four_vertices_chain_matches_cpu)
{
    // Fewer than 5 vertices: no 4-edge path can exist.
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}};
    const ColoredGraph graph(4U, edges, {1U, 2U, 3U, 4U}, false);
    EXPECT_TRUE(run_and_compare(graph).empty());
}

TEST_F(GpuPathProcessorTest, undirected_chain_matches_cpu)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}};
    const ColoredGraph graph(5U, edges, {1U, 2U, 3U, 4U, 5U}, false);
    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = run_and_compare(graph);
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.begin()->second, 1U);
}

TEST_F(GpuPathProcessorTest, directed_chain_matches_cpu)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}};
    const ColoredGraph graph(5U, edges, {1U, 2U, 3U, 4U, 5U}, true);
    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = run_and_compare(graph);
    ASSERT_EQ(result.size(), 1U);
}

TEST_F(GpuPathProcessorTest, undirected_cycle_c5_matches_cpu)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 4U}, {4U, 0U}};
    const ColoredGraph graph(5U, edges, {1U, 2U, 3U, 4U, 5U}, false);
    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = run_and_compare(graph);
    ASSERT_EQ(result.size(), 5U);
}

TEST_F(GpuPathProcessorTest, undirected_cycle_c6_matches_cpu)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {1U, 2U}, {2U, 3U},
                                                        {3U, 4U}, {4U, 5U}, {5U, 0U}};
    const ColoredGraph graph(6U, edges, {0U, 0U, 0U, 0U, 0U, 0U}, false);
    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = run_and_compare(graph);
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result.at(UInt128{}), 6U);
}

TEST_F(GpuPathProcessorTest, undirected_complete_k5_matches_cpu)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U},
                                                        {1U, 2U}, {1U, 3U}, {1U, 4U}, {2U, 3U},
                                                        {2U, 4U}, {3U, 4U}};
    const ColoredGraph graph(5U, edges, {0U, 0U, 0U, 0U, 0U}, false);
    const std::unordered_map<UInt128, uint32_t, UInt128Hash> result = run_and_compare(graph);
    ASSERT_FALSE(result.empty());
}

TEST_F(GpuPathProcessorTest, directed_complete_k5_matches_cpu)
{
    std::vector<std::pair<uint32_t, uint32_t>> edges = {{0U, 1U}, {0U, 2U}, {0U, 3U}, {0U, 4U},
                                                        {1U, 2U}, {1U, 3U}, {1U, 4U}, {2U, 3U},
                                                        {2U, 4U}, {3U, 4U}};
    const ColoredGraph graph(5U, edges, {0U, 0U, 0U, 0U, 0U}, true);
    run_and_compare(graph);
}

}  // namespace

#endif  // SGF_CUDA_ENABLED
