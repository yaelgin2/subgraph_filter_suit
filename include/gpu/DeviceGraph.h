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
    uint32_t  num_nodes;        ///< Number of vertices.
    uint32_t  num_fwd_edges;    ///< Number of forward CSR entries.
    uint32_t  num_rev_edges;    ///< Number of reverse CSR entries (0 for undirected).
    bool      m_directed;       ///< True for directed graphs.

    /**
     * @brief Return true if graph is directed.
     */
    __host__ __device__ bool is_directed() const noexcept { return m_directed; }

    /**
     * @brief Return the color label of @p vertex.
     * @param vertex Vertex id to query.
     */
    __host__ __device__ uint32_t get_vertex_color(const uint32_t vertex) const noexcept
    {
        return d_colors[vertex];
    }
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
     * @return Populated DeviceGraph with all arrays in managed memory.
     */
    static DeviceGraph build(const ColoredGraph& graph);

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

    /**
     * @brief Allocate managed memory for a CSR offset array with a sentinel.
     *
     * ColoredGraph stores only @p num_vertices offsets (no sentinel). This
     * function allocates num_vertices+1 entries, copies the source, then
     * appends @p total_edges as the CSR sentinel so the kernel can compute
     * the last vertex's degree as d_offsets[num_vertices] - d_offsets[num_vertices-1].
     * @param dest         Output pointer set to the newly allocated managed block.
     * @param src          Source host offset array (length num_vertices).
     * @param num_vertices Number of vertices; source array length.
     * @param total_edges  Total edge count written as the sentinel at index num_vertices.
     */
    static void copy_csr_offsets_to_managed_memory(uint32_t** dest, const uint32_t* src,
                                                    uint32_t num_vertices,
                                                    uint32_t total_edges);
};

}  // namespace sgf

#endif  // SGF_CUDA_ENABLED
