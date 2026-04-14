#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <ColoredGraph.h>

using Groups=std::pair<std::vector<uint32_t>, std::vector<std::vector<bool>>>;

namespace sgf
{

class GroupEnmerationPreprocessor
{

public:
    GroupEnmerationPreprocessor();
    ~GroupEnmerationPreprocessor()=default;

    std::unordered_map<__uint128_t, uint32_t> calculate();

protected:
    void sort_nodes();
    Groups find_groups();
    __uint128_t calculate_motif_number(std::vector<uint32_t> colors, std::vector<std::vector<bool>> edges);

private:
    std::shared_ptr<ColoredGraph> graph;
    std::vector<uint32_t> node_order;
};

}