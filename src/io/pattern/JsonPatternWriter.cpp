#include "JsonPatternIOManager.h"

#include "BoostGraph.h"
#include "ColoredGraph.h"
#include "IOConstants.h"
#include "ILogger.h"
#include "JsonGraphReader.h"
#include "LoggerHandler.h"
#include "SgfPathDoesntExistException.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

namespace sgf
{

boost::json::array JsonPatternIOManager::build_nodes_array(const BoostGraph& graph)
{
    boost::json::array nodes;
    const std::pair<boost::graph_traits<BoostGraph>::vertex_iterator,
                    boost::graph_traits<BoostGraph>::vertex_iterator>
        vertex_range = boost::vertices(graph);
    for (boost::graph_traits<BoostGraph>::vertex_iterator vertex_iter = vertex_range.first;
         vertex_iter != vertex_range.second; ++vertex_iter)
    {
        boost::json::object node;
        node[IOConstants::JSON_NODE_ID_KEY] = static_cast<int64_t>(*vertex_iter);
        node[IOConstants::JSON_COLOR_KEY] = static_cast<int64_t>(graph[*vertex_iter].m_color);
        nodes.push_back(std::move(node));
    }
    return nodes;
}

boost::json::array JsonPatternIOManager::build_links_array(const BoostGraph& graph)
{
    boost::json::array links;
    const std::pair<boost::graph_traits<BoostGraph>::edge_iterator,
                    boost::graph_traits<BoostGraph>::edge_iterator>
        edge_range = boost::edges(graph);
    for (boost::graph_traits<BoostGraph>::edge_iterator edge_iter = edge_range.first;
         edge_iter != edge_range.second; ++edge_iter)
    {
        boost::json::object link;
        link[IOConstants::JSON_SOURCE_KEY] = static_cast<int64_t>(boost::source(*edge_iter, graph));
        link[IOConstants::JSON_TARGET_KEY] = static_cast<int64_t>(boost::target(*edge_iter, graph));
        link[IOConstants::JSON_COLOR_KEY] = static_cast<int64_t>(graph[*edge_iter].m_color);
        links.push_back(std::move(link));
    }
    return links;
}

void JsonPatternIOManager::write_to_file(const boost::json::object& root, const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
    {
        throw SgfPathDoesntExistException("Cannot open file for writing: '" + path + "'");
    }
    file << boost::json::serialize(root);
}

void JsonPatternIOManager::write(const BoostGraph& graph, const std::string& path) const
{
    boost::json::object root;
    root[IOConstants::JSON_NODES_KEY] = build_nodes_array(graph);
    root[IOConstants::JSON_LINKS_KEY] = build_links_array(graph);
    write_to_file(root, path);
}

}  // namespace sgf
