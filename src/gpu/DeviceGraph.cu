#include "DeviceGraph.h"

#ifdef SGF_CUDA_ENABLED

#include "ColoredGraph.h"

#include <cuda_runtime.h>
#include <cstdint>
#include <cstring>

namespace sgf
{

void DeviceGraphBuilder::copy_array_to_managed_memory(uint32_t** const dest,
                                                       const uint32_t* const src,
                                                       const uint32_t count)
{
    const std::size_t byte_count = static_cast<std::size_t>(count) * sizeof(uint32_t);
    cudaMallocManaged(dest, byte_count);
    std::memcpy(*dest, src, byte_count);
}

DeviceGraph DeviceGraphBuilder::build(const ColoredGraph& graph)
{
    const uint32_t vertex_count = static_cast<uint32_t>(graph.m_colors.size());

    DeviceGraph dg{};
    dg.num_nodes = vertex_count;
    dg.m_directed = graph.m_directed;
    dg.num_fwd_edges = static_cast<uint32_t>(graph.m_neighbours.size());
    dg.num_rev_edges = 0U;

    copy_array_to_managed_memory(&dg.d_fwd_offsets, graph.m_index_of_neighbours.data(),
                                  vertex_count + 1U);
    copy_array_to_managed_memory(&dg.d_fwd_neighbors, graph.m_neighbours.data(),
                                  dg.num_fwd_edges);
    copy_array_to_managed_memory(&dg.d_colors, graph.m_colors.data(), vertex_count);

    if (graph.m_directed)
    {
        dg.num_rev_edges = static_cast<uint32_t>(graph.m_reversed_neighbours.size());
        copy_array_to_managed_memory(&dg.d_rev_offsets,
                                      graph.m_reversed_index_of_neighbours.data(),
                                      vertex_count + 1U);
        copy_array_to_managed_memory(&dg.d_rev_neighbors, graph.m_reversed_neighbours.data(),
                                      dg.num_rev_edges);
    }
    else
    {
        dg.d_rev_offsets = nullptr;
        dg.d_rev_neighbors = nullptr;
    }

    return dg;
}

void DeviceGraphBuilder::free_graph(DeviceGraph& device_graph)
{
    cudaFree(device_graph.d_fwd_offsets);
    cudaFree(device_graph.d_fwd_neighbors);
    cudaFree(device_graph.d_colors);
    if (device_graph.m_directed)
    {
        cudaFree(device_graph.d_rev_offsets);
        cudaFree(device_graph.d_rev_neighbors);
    }
    device_graph.d_fwd_offsets = nullptr;
    device_graph.d_fwd_neighbors = nullptr;
    device_graph.d_colors = nullptr;
    device_graph.d_rev_offsets = nullptr;
    device_graph.d_rev_neighbors = nullptr;
}

}  // namespace sgf

#endif  // SGF_CUDA_ENABLED
