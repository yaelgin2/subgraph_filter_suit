#include "GroupEnumerationPreprocessor.h"

#include "ColoredGraph.h"
#include "EnumerationOverflowException.h"
#include "IGraphPreprocessor.h"
#include "Int128.h"
#include "InvalidArgumentException.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

namespace
{

/// @brief Number of groups between progress log messages.
constexpr uint32_t LOG_INTERVAL = 1000U;

using NeighbourIteratorPair =
    std::pair<std::vector<uint32_t>::const_iterator, std::vector<uint32_t>::const_iterator>;

}  // namespace

GroupEnumerationPreprocessor::GroupEnumerationPreprocessor(const ColoredGraph& graph,
                                                           LoggerHandler logger,
                                                           const uint32_t thread_number)
    : m_graph(graph)
    , m_logger(std::move(logger))
    , m_thread_number(thread_number)
{
}

size_t GroupEnumerationPreprocessor::combined_degree(const uint32_t vertex) const
{
    const NeighbourIteratorPair out_range = m_graph.get_neighbours(vertex);
    const size_t out_degree = static_cast<size_t>(out_range.second - out_range.first);
    if (!m_graph.is_directed())
    {
        return out_degree;
    }
    const NeighbourIteratorPair in_range = m_graph.get_neighbours(vertex, true);
    return out_degree + static_cast<size_t>(in_range.second - in_range.first);
}

void GroupEnumerationPreprocessor::sort_nodes()
{
    const uint32_t vertex_count = m_graph.vertex_count();
    m_node_order.resize(vertex_count);
    std::iota(m_node_order.begin(), m_node_order.end(), 0U);
    std::sort(m_node_order.begin(), m_node_order.end(),
              [this](const uint32_t left_vertex, const uint32_t right_vertex)
              {
                  return combined_degree(left_vertex) > combined_degree(right_vertex);
              });
}

std::unordered_map<UInt128, uint32_t, UInt128Hash>
GroupEnumerationPreprocessor::calculate(const bool use_gpu)
{
    sort_nodes();

    if (use_gpu)
    {
        return calculate_gpu();
    }

    std::unordered_map<UInt128, uint32_t, UInt128Hash> motif_count;

    const EnumerationResult thread_result = stream_groups_to_counter();

    uint32_t groups_counted = 0U;
    for (const auto& [motif_id, count] : thread_result)
    {
        uint32_t& existing = motif_count[motif_id];
        if (existing > std::numeric_limits<uint32_t>::max() - count)
        {
            throw EnumerationOverflowException(
                "Motif count overflow: occurrence count exceeded uint32_t capacity.");
        }
        existing += count;
        groups_counted += count;
    }

    m_logger.log(LogLevel::INFO, "Finished enumerating " + std::to_string(groups_counted) + " " +
                                     entity_name() + ".");
    return motif_count;
}

EnumerationResult GroupEnumerationPreprocessor::calculate_gpu()
{
    throw InvalidArgumentException("GPU enumeration not implemented for this preprocessor type.");
}

std::vector<uint32_t>
GroupEnumerationPreprocessor::group_to_node_colors(const std::vector<uint32_t>& group) const
{
    std::vector<uint32_t> node_colors;
    node_colors.reserve(group.size());
    for (const auto& vertex : group)
    {
        node_colors.push_back(m_graph.get_vertex_color(vertex));
    }
    return node_colors;
}

}  // namespace sgf
