#include "JsonPatternWriter.h"

#include "BoostGraph.h"
#include "IOConstants.h"
#include "SgfPathExistsException.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>

namespace sgf
{

std::string JsonPatternWriter::get_file_extension() const
{
    return "json";
}

boost::json::array JsonPatternWriter::build_nodes_array(const BoostGraph& graph)
{
    boost::json::array nodes;

    for (const auto vertex : boost::make_iterator_range(boost::vertices(graph)))
    {
        boost::json::object node;

        node[IOConstants::JSON_NODE_ID_KEY] = static_cast<int64_t>(vertex);

        node[IOConstants::JSON_COLOR_KEY] = static_cast<int64_t>(graph[vertex].m_color);

        nodes.push_back(std::move(node));
    }

    return nodes;
}

boost::json::array JsonPatternWriter::build_links_array(const BoostGraph& graph,
                                                        const bool is_directed)
{
    boost::json::array links;

    for (const auto edge : boost::make_iterator_range(boost::edges(graph)))
    {
        const auto src = boost::source(edge, graph);
        const auto tgt = boost::target(edge, graph);

        if (!is_directed && src > tgt && boost::edge(tgt, src, graph).second)
        {
            continue;
        }

        boost::json::object link;

        link[IOConstants::JSON_SOURCE_KEY] = static_cast<int64_t>(src);

        link[IOConstants::JSON_TARGET_KEY] = static_cast<int64_t>(tgt);

        link[IOConstants::JSON_COLOR_KEY] = static_cast<int64_t>(graph[edge].m_color);

        links.push_back(std::move(link));
    }

    return links;
}

void JsonPatternWriter::write_to_file(const boost::json::object& root, const std::string& path)
{
    std::ofstream file(path);

    if (!file.is_open())
    {
        throw SgfPathExistsException("Cannot open file for writing: '" + path + "'");
    }

    file << boost::json::serialize(root);
}

void JsonPatternWriter::do_write(const BoostGraph& graph, const std::string& path,
                                 const bool is_directed) const
{
    boost::json::object root;

    root[IOConstants::JSON_NODES_KEY] = build_nodes_array(graph);
    root[IOConstants::JSON_LINKS_KEY] = build_links_array(graph, is_directed);

    write_to_file(root, path);
}

}  // namespace sgf
