#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <ColoredGraph.h>
#include "ILogger.h"
#include "LogLevel.h"
#include "LoggerHandler.h"

using Groups=std::vector<std::pair<uint32_t, std::vector<int>>>;

namespace sgf
{

class GroupEnmerationPreprocessor
{

public:
    GroupEnmerationPreprocessor(std::shared_ptr<ColoredGraph> graph, LoggerHandler& m_logger);
    ~GroupEnmerationPreprocessor()=default;

    std::unordered_map<__uint128_t, uint32_t> calculate();

protected:
    virtual void sort_nodes()=0;
    virtual Groups find_groups()=0;
    virtual __uint128_t calculate_motif_number(std::vector<uint32_t> colors, std::vector<std::vector<bool>> edges)=0;

private:
    std::shared_ptr<ColoredGraph> m_graph;
    std::vector<uint32_t> m_node_order;
    LoggerHandler& m_logger;

    void graph_to_adjacency_matrix(std::vector<std::vector<bool>>& adjacency_matrix);
    std::vector<std::vector<bool>>> group_to_adjacency_matrix(const std::vector<uint32_t>& group);
};

}