// GCC false-positive: Boost's adjacency_list copy_impl and write_graphml trigger
// -Wmaybe-uninitialized on internal edge iterator optional members. The issue
// is in Boost's template code, not in this file.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#include "GraphmlPatternWriter.h"

#include "BoostGraph.h"
#include "GraphmlIOUtils.h"

#include <boost/graph/graphml.hpp>
#include <boost/property_map/dynamic_property_map.hpp>
#include <fstream>
#include <string>

namespace sgf
{

void GraphmlPatternWriter::write(const BoostGraph& graph, const std::string& path) const
{
    std::ofstream file = graphml_io_utils::open_output_file(path);
    // Boost's dynamic_properties::property() compiles the put() path unconditionally,
    // requiring a mutable graph even when write_graphml only reads properties.
    BoostGraph graph_copy = graph;
    boost::dynamic_properties dynamic_props;
    dynamic_props.property("color", boost::get(&VertexProperties::m_color, graph_copy));
    dynamic_props.property("color", boost::get(&EdgeProperties::m_color, graph_copy));
    boost::write_graphml(file, graph_copy, dynamic_props);
}

}  // namespace sgf

#pragma GCC diagnostic pop
