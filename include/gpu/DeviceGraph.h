#pragma once

#ifdef SGF_CUDA_ENABLED

#include <cstdint>
#include <vector>

namespace sgf
{

class ColoredGraph;

/**
 * @brief GPU-accessible flat representation of a ColoredGraph.
 *
 * All pointer fields are allocated with cudaMallocManaged and accessible from
 * both host and device code. Forward CSR mirrors m_index_of_neighbours /
 * m_neighbours; reverse CSR (directed only) mirrors the reversed variants.
 * For undirected graphs d_rev_offsets and d_rev_neighbors are nullptr.
 */
struct DeviceGraph
{
    uint32_t* d_fwd_offsets;    ///< CSR row offsets for forward edges, length num_nodes + 1.
    uint32_t* d_fwd_neighbors;  ///< CSR column indices for forward edges, length num_fwd_edges.
    uint32_t* d_rev_offsets;    ///< CSR row offsets for reverse edges (directed only), or nullptr.
    uint32_t* d_rev_neighbors;  ///< CSR column indices for reverse edges (directed only), or nullptr.
    uint32_t* d_colors;         ///< Vertex color array, length num_nodes.
    uint32_t* d_order_index;    ///< Position of each vertex in the degree-sorted order, length num_nodes.
    uint32_t* d_sorted_nodes;   ///< Vertices listed in degree-sorted order, length num_nodes.
    uint32_t  num_nodes;        ///< Number of vertices.
    uint32_t  num_fwd_edges;    ///< Number of forward CSR entries.
    uint32_t  num_rev_edges;    ///< Number of reverse CSR entries (0 for undirected).
    bool      is_directed;      ///< True for directed graphs.
};

/**
 * @brief Builds and releases GPU-accessible DeviceGraph instances.
 *
 * Declared as a friend of ColoredGraph so it can copy the private CSR arrays
 * directly — no intermediate host-side reconstruction needed.
 */
class DeviceGraphBuilder
{
public:
    /**
     * @brief Allocate managed memory and copy graph data to device.
     *
     * Accesses ColoredGraph's private CSR arrays directly via friendship.
     *
     * @param graph Source colored graph.
     * @param order_index Mapping from vertex id to its position in degree-sorted order.
     * @param sorted_nodes Vertices in degree-sorted order (m_node_order).
     * @return Populated DeviceGraph with all arrays in managed memory.
     */
    static DeviceGraph build(const ColoredGraph& graph,
                              const std::vector<uint32_t>& order_index,
                              const std::vector<uint32_t>& sorted_nodes);

    /**
     * @brief Free all managed memory arrays in a DeviceGraph.
     * @param device_graph Graph to free; all pointer fields are set to nullptr after the call.
     */
    static void free_graph(DeviceGraph& device_graph);

private:
    /**
     * @brief Allocate managed memory for @p count uint32_t elements and copy from @p src.
     * @param dest Output pointer set to the newly allocated managed block.
     * @param src Source host data.
     * @param count Number of elements.
     */
    static void copy_array_to_managed_memory(uint32_t** dest, const uint32_t* src,
                                              uint32_t count);
};

}  // namespace sgf

#endif  // SGF_CUDA_ENABLED
