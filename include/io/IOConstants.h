#pragma once

namespace sgf
{

/**
 * @brief Shared I/O constants used across multiple reader and writer classes.
 */
class IOConstants
{
public:
    /// JSON key for the nodes array.
    static constexpr const char* JSON_NODES_KEY = "nodes";
    /// JSON key for the links (edges) array.
    static constexpr const char* JSON_LINKS_KEY = "links";
    /// JSON key for a node's identifier field.
    static constexpr const char* JSON_NODE_ID_KEY = "id";
    /// JSON key for a node's or edge's color field.
    static constexpr const char* JSON_COLOR_KEY = "color";
    /// JSON key for an edge's source vertex field.
    static constexpr const char* JSON_SOURCE_KEY = "source";
    /// JSON key for an edge's target vertex field.
    static constexpr const char* JSON_TARGET_KEY = "target";
    /// Whether pattern graphs serialised as JSON are directed.
    static constexpr bool JSON_PATTERN_IS_DIRECTED = true;
};

}  // namespace sgf
