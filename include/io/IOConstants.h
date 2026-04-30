namespace sgf
{
/**
 * Constants for io classes. These include file suffixes and other string literals used in multiple
 * places.
 */
class IOConstants
{
public:
    /// Suffix for vertex label files.
    static constexpr const char* NODE_LABELS_SUFFIX = ".node_labels";
    /// Suffix for edge files.
    static constexpr const char* EDGE_SUFFIX = ".edge";
    /// Number of tokens on a vertex (or node-label) line: id and color.
    static constexpr uint32_t VERTEX_EDGE_TOKENS_PER_VERTEX_LINE = 2;
    /// Minimum tokens on an edge line: source and destination.
    static constexpr uint32_t VERTEX_EDGE_TOKENS_PER_UNCOLORED_EDGE_LINE = 2;
    /// Tokens on a fully-colored edge line: source, destination, and color.
    static constexpr uint32_t VERTEX_EDGE_TOKENS_PER_COLORED_EDGE_LINE = 3;
};
}  // namespace sgf
