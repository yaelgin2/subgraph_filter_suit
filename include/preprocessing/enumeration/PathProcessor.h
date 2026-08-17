#pragma once

#ifndef SGF_HD
#ifdef __CUDACC__
#define SGF_HD __host__ __device__
#else
#define SGF_HD
#endif
#endif

#include "ColoredGraph.h"
#include "Constants.h"
#include "CpuPathContext.h"
#include "GroupEnumerationPreprocessor.h"
#include "Int128.h"
#include "LoggerHandler.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#ifdef __CUDACC__
#include "CudaPathBackend.h"
#endif

namespace sgf
{

/**
 * @brief Ordered sequence of (vertex, traversal-direction) hops forming one half-path.
 *
 * Each pair encodes a vertex index and the edge direction used to reach it
 * (false = outgoing, true = incoming; only meaningful in directed graphs).
 * Always exactly 2 hops (depth-1 then depth-2) — a fixed-size array so this
 * type is usable from device code, unlike a heap-allocating std::vector.
 */
using PathInformation = std::array<std::pair<uint32_t, bool>, 2>;

/**
 * @class PathProcessor
 * @brief Enumerates all simple 4-edge paths (5 distinct vertices) in a colored graph.
 *
 * Extends GroupEnumerationPreprocessor to enumerate all simple paths consisting
 * of exactly 4 edges (5 distinct vertices). Each path is treated as rooted at
 * its middle vertex; two pairs of depth-1 and depth-2 neighbours are combined
 * to form the full path. Pairs sharing any vertex are discarded to ensure simplicity.
 *
 * The motif identifier encodes the vertex color sequence and, for directed graphs,
 * the bit-packed edge direction descriptor. The canonical form is chosen as the
 * lexicographically smaller of the forward and reversed representations so that
 * opposite-direction traversals of the same undirected path yield the same identifier.
 *
 * The enumeration algorithm itself (stream_groups_to_counter_for_vertex and every
 * function it calls) is implemented once as SGF_HD templates parameterized on a
 * path context type, shared between the CPU driver (stream_groups_to_counter(),
 * using CpuPathContext) and the CUDA kernel (path_kernel in CudaPathBackend.cu,
 * using GpuPathContext) — the same template + context-struct pattern
 * MotifPreprocessor uses for its Kavosh BFS (IKavoshContext / CpuKavoshContext /
 * GpuKavoshContext).
 */
class PathProcessor : public GroupEnumerationPreprocessor
{
public:
    /**
     * @brief Construct a PathProcessor for the given graph.
     * @param graph The colored graph to preprocess.
     * @param logger Logger handler for status and debug output.
     * @param thread_number Maximum number of threads to use during enumeration.
     */
    PathProcessor(const ColoredGraph& graph, LoggerHandler logger,
                  uint32_t thread_number = SgfConstants::DEFAULT_THREAD_NUMBER);

    PathProcessor() = delete;
    PathProcessor(const PathProcessor&) = delete;
    PathProcessor& operator=(const PathProcessor&) = delete;
    PathProcessor(PathProcessor&&) = delete;
    PathProcessor& operator=(PathProcessor&&) = delete;

    /**
     * @brief Default destructor.
     */
    ~PathProcessor() override = default;

    // ── SGF_HD shared enumeration algorithm ───────────────────────────────────
    //
    // Public so both the CPU driver (stream_groups_to_counter(), a PathProcessor
    // member) and the free __global__ path_kernel (CudaPathBackend.cu, not a
    // member) can call stream_groups_to_counter_for_vertex directly. Everything
    // it transitively calls stays protected/private below — those are only ever
    // reached through this entry point, which is itself always a PathProcessor
    // member call, so ordinary member-to-member access applies.

    /**
     * @brief Enumerate all 4-edge paths rooted at @p ctx.m_root.
     *
     * Works identically on CPU (PathContext = CpuPathContext, NeighbourRangeT =
     * CpuNeighbourRange, NeighIter = vector iterator) and GPU (PathContext =
     * GpuPathContext, NeighbourRangeT = GpuNeighbourRange, NeighIter = const
     * uint32_t*). @p thread_y_offset / @p stride_y default to a full serial scan
     * (CPU); the GPU kernel supplies its y-thread index/stride to parallelize
     * the outer depth-1-neighbour loop, mirroring
     * MotifPreprocessor::emit_depth_1_1_1_groups_gpu's striding.
     *
     * @tparam PathContext     CpuPathContext or GpuPathContext.
     * @tparam NeighbourRangeT CpuNeighbourRange or GpuNeighbourRange.
     * @tparam NeighIter       Vector const_iterator (CPU) or const uint32_t* (GPU).
     * @param ctx             Shared run context for the current root.
     * @param thread_y_offset Y-dimension thread offset for striding over depth-1 neighbours.
     * @param stride_y        Total y-dimension stride across all threads.
     */
    template <typename PathContext, typename NeighbourRangeT, typename NeighIter>
    static SGF_HD void stream_groups_to_counter_for_vertex(PathContext& ctx,
                                                           uint32_t thread_y_offset = 0U,
                                                           uint32_t stride_y = 1U);

protected:
    /**
     * @brief Enumerate all simple 4-edge paths and return their counts.
     *
     * Distributes m_node_order across a std::thread pool; each thread builds one
     * CpuPathContext per root vertex and calls stream_groups_to_counter_for_vertex.
     *
     * @return Map from canonical path identifier to occurrence count.
     */
    EnumerationResult stream_groups_to_counter() const override;

#ifdef SGF_CUDA_ENABLED
    /**
     * @brief GPU override: full path enumeration via CUDA kernel.
     * @return Map from canonical path identifier to occurrence count.
     */
    EnumerationResult calculate_gpu() override;
#endif

    /**
     * @brief Enumerate paths for all pairs within the outgoing neighbour list of @p ctx.m_root.
     *
     * Iterates over all pairs of outgoing depth-1 neighbours (the outer loop strided
     * by thread_y_offset/stride_y) and delegates each pair to
     * stream_groups_to_counter_for_two_depth_one_neighbours. For directed graphs also
     * pairs each outgoing neighbour with every incoming neighbour.
     *
     * @param ctx             Shared run context for the current root.
     * @param root_range      Combined forward+reverse neighbour range of ctx.m_root.
     * @param thread_y_offset Y-dimension thread offset for striding over the outer loop.
     * @param stride_y        Total y-dimension stride across all threads.
     */
    template <typename PathContext, typename NeighbourRangeT, typename NeighIter>
    static SGF_HD void
    stream_groups_for_out_neighbours(PathContext& ctx, const NeighbourRangeT& root_range,
                                     uint32_t thread_y_offset, uint32_t stride_y);

    /**
     * @brief Enumerate paths for all pairs within the incoming neighbour list of @p ctx.m_root.
     *
     * For directed graphs only. Iterates over all pairs of incoming depth-1
     * neighbours (the outer loop strided by thread_y_offset/stride_y) and
     * delegates each to stream_groups_to_counter_for_two_depth_one_neighbours.
     *
     * @param ctx               Shared run context for the current root.
     * @param depth_one_in_start Iterator to the start of the incoming neighbour list.
     * @param depth_one_in_end   Iterator past the end of the incoming neighbour list.
     * @param thread_y_offset   Y-dimension thread offset for striding over the outer loop.
     * @param stride_y          Total y-dimension stride across all threads.
     */
    template <typename PathContext, typename NeighbourRangeT, typename NeighIter>
    static SGF_HD void stream_groups_for_in_neighbours(PathContext& ctx,
                                                       NeighIter depth_one_in_start,
                                                       NeighIter depth_one_in_end,
                                                       uint32_t thread_y_offset, uint32_t stride_y);

private:
    /**
     * @brief Returns "paths" to label the finished-enumeration log line.
     * @return The string "paths".
     */
    [[nodiscard]] std::string entity_name() const override;

    /**
     * @brief CPU recording: record a pre-computed canonical path id.
     * @param ctx      CPU run context.
     * @param motif_id Canonical 128-bit path identifier.
     */
    static void cpu_add_path_to_count(CpuPathContext& ctx, UInt128 motif_id) noexcept;

    /**
     * @brief Number of edges in each enumerated path.
     */
    static constexpr uint32_t PATH_EDGE_COUNT = 4U;

    /**
     * @brief Number of vertices in a path of length PATH_EDGE_COUNT.
     */
    static constexpr uint32_t PATH_VERTEX_COUNT = PATH_EDGE_COUNT + 1U;

    /**
     * @brief Bits allocated per vertex color slot in the 128-bit identifier.
     */
    static constexpr uint32_t COLOR_BITS_PER_SLOT = 24U;

    /**
     * @brief Iterator and traversal direction for a depth-1 neighbour of the path root.
     *
     * Bundles the neighbour-list iterator with the edge direction (false = outgoing,
     * true = incoming) used to reach depth-2 neighbours from that position.
     *
     * @tparam NeighIter Vector const_iterator (CPU) or const uint32_t* (GPU).
     */
    template <typename NeighIter>
    struct DepthOneNeighbourInfo
    {
        /**
         * @brief Iterator pointing at the depth-1 neighbour vertex index.
         */
        NeighIter m_iterator;

        /**
         * @brief Edge traversal direction for depth-2 expansion (false = outgoing,
         * true = incoming).
         */
        bool m_direction{false};
    };

    /**
     * @brief Enumerate paths for a depth-1 neighbour pair across all direction combinations.
     *
     * Calls the two-DepthOneNeighbourInfo overload with every combination of direction_one and
     * direction_two ({false,false}, {false,true}, {true,false}, {true,true}) to cover
     * all edge traversal directions at depth 2.
     *
     * @param ctx Shared run context for the current root.
     * @param first_depth_one_neighbour Iterator to the first depth-1 neighbour of root.
     * @param second_depth_one_neighbour Iterator to the second depth-1 neighbour of root.
     */
    template <typename PathContext, typename NeighbourRangeT, typename NeighIter>
    static SGF_HD void
    stream_groups_to_counter_for_two_depth_one_neighbours(PathContext& ctx,
                                                          NeighIter first_depth_one_neighbour,
                                                          NeighIter second_depth_one_neighbour);

    /**
     * @brief Enumerate paths for a depth-1 neighbour pair with fixed depth-2 directions.
     *
     * Expands one hop further from each depth-1 neighbour using the given traversal
     * directions, then records every non-intersecting combination via record_path.
     *
     * @param ctx Shared run context for the current root.
     * @param first_neighbour_info Iterator and direction for first depth-1 neighbour.
     * @param second_neighbour_info Iterator and direction for second depth-1 neighbour.
     */
    template <typename PathContext, typename NeighbourRangeT, typename NeighIter>
    // NOLINTNEXTLINE(readability-function-size)
    static SGF_HD void stream_groups_to_counter_for_two_depth_one_neighbours(
        PathContext& ctx, const DepthOneNeighbourInfo<NeighIter>& first_neighbour_info,
        const DepthOneNeighbourInfo<NeighIter>& second_neighbour_info);

    /**
     * @brief Compute the canonical motif id for a completed path and record it.
     *
     * Builds the 5-vertex array via concatenate_path, looks up each vertex's
     * color via ctx.m_graph.get_vertex_color, computes the canonical 128-bit
     * identifier, then calls ctx.m_add_path_fn(ctx, motif_id) — the templated
     * equivalent of MotifPreprocessor::count_group_by_ids.
     *
     * @param ctx         Shared run context for the current root.
     * @param first_path  Left 2-hop half-path.
     * @param second_path Right 2-hop half-path.
     */
    template <typename PathContext>
    static SGF_HD void record_path(PathContext& ctx, const PathInformation& first_path,
                                   const PathInformation& second_path) noexcept;

    /**
     * @brief Compute the canonical direction descriptor for the reverse traversal of a path.
     *
     * Bit-reverses and bit-inverts the 4-bit @p motif_descriptor to produce the
     * descriptor that corresponds to traversing the same path in the opposite direction.
     *
     * @param motif_descriptor Original 4-bit direction descriptor (one bit per edge).
     * @return Reversed and inverted 4-bit direction descriptor.
     */
    static SGF_HD uint32_t compute_reversed_descriptor(uint32_t motif_descriptor) noexcept;

    /**
     * @brief Check whether two 2-hop half-paths share any vertex that would create a cycle.
     *
     * Returns true if joining the two half-paths at the root would produce a
     * non-simple walk (a vertex visited more than once). The root itself is
     * excluded from this check as it is always shared by design.
     *
     * @param first_path Left 2-hop half-path: {(depth-1 vertex, dir), (depth-2 vertex, dir)}.
     * @param second_path Right 2-hop half-path: {(depth-1 vertex, dir), (depth-2 vertex, dir)}.
     * @return True if the joined path would be non-simple.
     */
    static SGF_HD bool check_path_intersection(const PathInformation& first_path,
                                               const PathInformation& second_path) noexcept;

    /**
     * @brief Concatenate two 2-hop half-paths and the shared root into a vertex sequence.
     *
     * Returns the 5-vertex list: [first[1], first[0], root, second[0], second[1]].
     *
     * @param root The middle vertex shared by both half-paths.
     * @param first_path Left 2-hop half-path.
     * @param second_path Right 2-hop half-path.
     * @return Vertex index sequence of length PATH_VERTEX_COUNT.
     */
    static SGF_HD std::array<uint32_t, PATH_VERTEX_COUNT>
    concatenate_path(uint32_t root, const PathInformation& first_path,
                     const PathInformation& second_path) noexcept;

    /**
     * @brief Compute the bit-packed edge-direction descriptor for a path.
     *
     * Encodes one bit per hop in @p first_path then @p second_path (true = 1,
     * false = 0). Returns 0 for undirected graphs, where direction is meaningless.
     *
     * @param first_path Left 2-hop half-path.
     * @param second_path Right 2-hop half-path.
     * @param is_directed Whether the source graph is directed.
     * @return Bit-packed direction descriptor (4 bits), or 0 for undirected graphs.
     */
    static SGF_HD uint32_t compute_motif_descriptor(const PathInformation& first_path,
                                                    const PathInformation& second_path,
                                                    bool is_directed) noexcept;

    /**
     * @brief Encode a 4-edge path into a canonical 128-bit motif identifier.
     *
     * Packs vertex colors into the low 120 bits (5 slots × 24 bits each).
     * For undirected graphs, returns the minimum of the forward and reversed
     * color sequences. For directed graphs, also packs the edge-direction
     * descriptor into the high bits, selecting the descriptor that corresponds
     * to the canonical (smaller) color sequence.
     *
     * @param motif_descriptor Bit-packed edge directions (one bit per edge, 4 bits total).
     * @param node_colors Color labels of the 5 vertices in traversal order.
     * @param is_directed Whether the source graph is directed.
     * @return Canonical 128-bit path identifier.
     */
    static SGF_HD UInt128 calculate_motif_number(
        uint32_t motif_descriptor, const std::array<uint32_t, PATH_VERTEX_COUNT>& node_colors,
        bool is_directed) noexcept;

public:
#ifdef __CUDACC__
    /**
     * @brief GPU recording: atomically insert a pre-computed canonical path id into the cuco maps.
     * @param ctx      GPU run context.
     * @param motif_id Canonical 128-bit path identifier.
     */
    static __device__ void gpu_add_path_to_count(GpuPathContext& ctx, UInt128 motif_id) noexcept;
#endif
};

// ============================================================================
// stream_groups_to_counter_for_vertex — template definition
// ============================================================================

template <typename PathContext, typename NeighbourRangeT, typename NeighIter>
SGF_HD void PathProcessor::stream_groups_to_counter_for_vertex(PathContext& ctx,
                                                               const uint32_t thread_y_offset,
                                                               const uint32_t stride_y)
{
    const NeighbourRangeT root_range = ctx.get_neighbour_range(ctx.m_root);

    stream_groups_for_out_neighbours<PathContext, NeighbourRangeT, NeighIter>(
        ctx, root_range, thread_y_offset, stride_y);

    if (ctx.m_graph.is_directed())
    {
        stream_groups_for_in_neighbours<PathContext, NeighbourRangeT, NeighIter>(
            ctx, root_range.m_rev_begin, root_range.m_rev_end, thread_y_offset, stride_y);
    }
}

// ============================================================================
// stream_groups_for_out_neighbours / stream_groups_for_in_neighbours
// ============================================================================

template <typename PathContext, typename NeighbourRangeT, typename NeighIter>
SGF_HD void PathProcessor::stream_groups_for_out_neighbours(PathContext& ctx,
                                                            const NeighbourRangeT& root_range,
                                                            const uint32_t thread_y_offset,
                                                            const uint32_t stride_y)
{
    for (NeighIter first_neighbour_it = root_range.m_begin + thread_y_offset;
         first_neighbour_it < root_range.m_end; first_neighbour_it += stride_y)
    {
        for (NeighIter second_neighbour_it = first_neighbour_it + 1;
             second_neighbour_it != root_range.m_end; ++second_neighbour_it)
        {
            stream_groups_to_counter_for_two_depth_one_neighbours<PathContext, NeighbourRangeT,
                                                                  NeighIter>(
                ctx, first_neighbour_it, second_neighbour_it);
        }
        if (ctx.m_graph.is_directed())
        {
            for (NeighIter second_neighbour_it = root_range.m_rev_begin;
                 second_neighbour_it != root_range.m_rev_end; ++second_neighbour_it)
            {
                stream_groups_to_counter_for_two_depth_one_neighbours<PathContext, NeighbourRangeT,
                                                                      NeighIter>(
                    ctx, first_neighbour_it, second_neighbour_it);
            }
        }
    }
}

template <typename PathContext, typename NeighbourRangeT, typename NeighIter>
SGF_HD void PathProcessor::stream_groups_for_in_neighbours(PathContext& ctx,
                                                           const NeighIter depth_one_in_start,
                                                           const NeighIter depth_one_in_end,
                                                           const uint32_t thread_y_offset,
                                                           const uint32_t stride_y)
{
    for (NeighIter first_neighbour_it = depth_one_in_start + thread_y_offset;
         first_neighbour_it < depth_one_in_end; first_neighbour_it += stride_y)
    {
        for (NeighIter second_neighbour_it = first_neighbour_it + 1;
             second_neighbour_it != depth_one_in_end; ++second_neighbour_it)
        {
            stream_groups_to_counter_for_two_depth_one_neighbours<PathContext, NeighbourRangeT,
                                                                  NeighIter>(
                ctx, first_neighbour_it, second_neighbour_it);
        }
    }
}

// ============================================================================
// stream_groups_to_counter_for_two_depth_one_neighbours — both overloads
// ============================================================================

template <typename PathContext, typename NeighbourRangeT, typename NeighIter>
SGF_HD void PathProcessor::stream_groups_to_counter_for_two_depth_one_neighbours(
    PathContext& ctx, const NeighIter first_depth_one_neighbour,
    const NeighIter second_depth_one_neighbour)
{
    stream_groups_to_counter_for_two_depth_one_neighbours<PathContext, NeighbourRangeT>(
        ctx, DepthOneNeighbourInfo<NeighIter>{first_depth_one_neighbour, false},
        DepthOneNeighbourInfo<NeighIter>{second_depth_one_neighbour, false});
    if (ctx.m_graph.is_directed())
    {
        stream_groups_to_counter_for_two_depth_one_neighbours<PathContext, NeighbourRangeT>(
            ctx, DepthOneNeighbourInfo<NeighIter>{first_depth_one_neighbour, false},
            DepthOneNeighbourInfo<NeighIter>{second_depth_one_neighbour, true});
        stream_groups_to_counter_for_two_depth_one_neighbours<PathContext, NeighbourRangeT>(
            ctx, DepthOneNeighbourInfo<NeighIter>{first_depth_one_neighbour, true},
            DepthOneNeighbourInfo<NeighIter>{second_depth_one_neighbour, false});
        stream_groups_to_counter_for_two_depth_one_neighbours<PathContext, NeighbourRangeT>(
            ctx, DepthOneNeighbourInfo<NeighIter>{first_depth_one_neighbour, true},
            DepthOneNeighbourInfo<NeighIter>{second_depth_one_neighbour, true});
    }
}

template <typename PathContext, typename NeighbourRangeT, typename NeighIter>
// NOLINTNEXTLINE(readability-function-size)
SGF_HD void PathProcessor::stream_groups_to_counter_for_two_depth_one_neighbours(
    PathContext& ctx, const DepthOneNeighbourInfo<NeighIter>& first_neighbour_info,
    const DepthOneNeighbourInfo<NeighIter>& second_neighbour_info)
{
    const NeighbourRangeT depth_two_one = ctx.get_neighbour_range(*first_neighbour_info.m_iterator);
    const NeighbourRangeT depth_two_two =
        ctx.get_neighbour_range(*second_neighbour_info.m_iterator);

    const NeighIter depth_two_one_begin =
        first_neighbour_info.m_direction ? depth_two_one.m_rev_begin : depth_two_one.m_begin;
    const NeighIter depth_two_one_end =
        first_neighbour_info.m_direction ? depth_two_one.m_rev_end : depth_two_one.m_end;
    const NeighIter depth_two_two_begin =
        second_neighbour_info.m_direction ? depth_two_two.m_rev_begin : depth_two_two.m_begin;
    const NeighIter depth_two_two_end =
        second_neighbour_info.m_direction ? depth_two_two.m_rev_end : depth_two_two.m_end;

    for (NeighIter first_depth_two = depth_two_one_begin; first_depth_two != depth_two_one_end;
         ++first_depth_two)
    {
        if (*first_depth_two == ctx.m_root)
        {
            continue;
        }
        for (NeighIter second_depth_two = depth_two_two_begin;
             second_depth_two != depth_two_two_end; ++second_depth_two)
        {
            if (*second_depth_two == ctx.m_root)
            {
                continue;
            }
            const PathInformation full_first_path = {
                std::pair<uint32_t, bool>{*first_neighbour_info.m_iterator,
                                          first_neighbour_info.m_direction},
                std::pair<uint32_t, bool>{*first_depth_two, first_neighbour_info.m_direction}};
            const PathInformation full_second_path = {
                std::pair<uint32_t, bool>{*second_neighbour_info.m_iterator,
                                          second_neighbour_info.m_direction},
                std::pair<uint32_t, bool>{*second_depth_two, second_neighbour_info.m_direction}};

            if (!check_path_intersection(full_first_path, full_second_path))
            {
                record_path<PathContext>(ctx, full_first_path, full_second_path);
            }
        }
    }
}

// ============================================================================
// record_path — template definition
// ============================================================================

template <typename PathContext>
SGF_HD void PathProcessor::record_path(PathContext& ctx, const PathInformation& first_path,
                                       const PathInformation& second_path) noexcept
{
    const std::array<uint32_t, PATH_VERTEX_COUNT> path =
        concatenate_path(ctx.m_root, first_path, second_path);

    std::array<uint32_t, PATH_VERTEX_COUNT> colors{};
    for (uint32_t vertex_index = 0U; vertex_index < PATH_VERTEX_COUNT; ++vertex_index)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        colors[vertex_index] = ctx.m_graph.get_vertex_color(path[vertex_index]);
    }

    const bool is_directed = ctx.m_graph.is_directed();
    const uint32_t motif_descriptor =
        compute_motif_descriptor(first_path, second_path, is_directed);
    const UInt128 motif_id = calculate_motif_number(motif_descriptor, colors, is_directed);
    ctx.m_add_path_fn(ctx, motif_id);
}

// ============================================================================
// check_path_intersection / concatenate_path / compute_motif_descriptor /
// compute_reversed_descriptor / calculate_motif_number — inline SGF_HD bodies
// ============================================================================
//
// These are plain (non-template) static member functions but must still be
// defined here rather than in PathProcessor.cpp: PathProcessor.cpp is compiled
// by the host compiler only, so it never emits device code. record_path (above)
// is instantiated with GpuPathContext inside CudaPathBackend.cu, compiled by
// nvcc, and needs an actual __device__ body for each of these to call — hence
// `inline` header definitions, exactly like MotifPreprocessor's
// calculate_motif_number_from_arrays.

inline SGF_HD bool
PathProcessor::check_path_intersection(const PathInformation& first_path,
                                       const PathInformation& second_path) noexcept
{
    return first_path[0].first == second_path[1].first ||
           first_path[1].first == second_path[0].first ||
           first_path[1].first == second_path[1].first;
}

inline SGF_HD std::array<uint32_t, PathProcessor::PATH_VERTEX_COUNT>
PathProcessor::concatenate_path(const uint32_t root, const PathInformation& first_path,
                                const PathInformation& second_path) noexcept
{
    return {first_path[1].first, first_path[0].first, root, second_path[0].first,
            second_path[1].first};
}

inline SGF_HD uint32_t PathProcessor::compute_motif_descriptor(const PathInformation& first_path,
                                                               const PathInformation& second_path,
                                                               const bool is_directed) noexcept
{
    uint32_t motif_descriptor = 0U;
    if (is_directed)
    {
        for (const std::pair<uint32_t, bool>& node : first_path)
        {
            motif_descriptor <<= 1U;
            motif_descriptor |= node.second ? 1U : 0U;
        }
        for (const std::pair<uint32_t, bool>& node : second_path)
        {
            motif_descriptor <<= 1U;
            motif_descriptor |= node.second ? 0U : 1U;
        }
    }
    return motif_descriptor;
}

inline SGF_HD uint32_t
PathProcessor::compute_reversed_descriptor(const uint32_t motif_descriptor) noexcept
{
    uint32_t reversed = 0U;
    for (uint32_t bit_index = 0U; bit_index < PATH_EDGE_COUNT; ++bit_index)
    {
        reversed |= (((motif_descriptor >> bit_index) & 1U) ^ 1U)
                    << (PATH_EDGE_COUNT - 1U - bit_index);
    }
    return reversed;
}

// NOLINTNEXTLINE(readability-function-size)
inline SGF_HD UInt128 PathProcessor::calculate_motif_number(
    const uint32_t motif_descriptor, const std::array<uint32_t, PATH_VERTEX_COUNT>& node_colors,
    const bool is_directed) noexcept
{
    UInt128 forward_color_sequence{};
    for (const uint32_t vertex_color : node_colors)
    {
        forward_color_sequence <<= COLOR_BITS_PER_SLOT;
        forward_color_sequence |= vertex_color;
    }
    UInt128 reversed_color_sequence{};
    for (uint32_t index = PATH_VERTEX_COUNT; index > 0U; --index)
    {
        reversed_color_sequence <<= COLOR_BITS_PER_SLOT;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        reversed_color_sequence |= node_colors[index - 1U];
    }
    if (!is_directed)
    {
        return UInt128{std::min(forward_color_sequence, reversed_color_sequence)};
    }
    UInt128 motif_number{};
    if (forward_color_sequence <= reversed_color_sequence)
    {
        motif_number = forward_color_sequence;
        motif_number |= UInt128{static_cast<uint64_t>(motif_descriptor)}
                        << (PATH_VERTEX_COUNT * COLOR_BITS_PER_SLOT);
    }
    else
    {
        motif_number = reversed_color_sequence;
        motif_number |=
            UInt128{static_cast<uint64_t>(compute_reversed_descriptor(motif_descriptor))}
            << (PATH_VERTEX_COUNT * COLOR_BITS_PER_SLOT);
    }
    return UInt128{motif_number};
}

}  // namespace sgf
