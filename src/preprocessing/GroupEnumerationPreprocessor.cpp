#include "GroupEnumerationPreprocessor.h"

#include "ILogger.h"
#include "LogLevel.h"

namespace sgf
{

GroupEnmerationPreprocessor::GroupEnmerationPreprocessor(std::shared_ptr<ColoredGraph> graph,
     LoggerHandler logger)
    : m_graph(graph), m_logger(logger)
{
}

std::unordered_map<__uint128_t, uint32_t> GroupEnmerationPreprocessor::calculate()
{
    m_logger.log(LogLevel::INFO, "Starting graph enumeration calculation.");

        self._features = {}
        if self.calc_nodes:
            self._features = {node: {} for node in self._graph}
        elif self.calc_edges:
            self._features = {edge: {} for edge in self._graph.edges()}
        if self.count_motifs:
            self.features[self.MOTIF_SUM_KEY] = {}

        for i, (group, motif_num) in enumerate(self._calculate_motif()):
            if self.calc_nodes:
                self._update_nodes_group(group, motif_num)
            elif self.calc_edges:
                self._update_edges(group, motif_num)
            if self.count_motifs:
                if motif_num not in self._features[self.MOTIF_SUM_KEY]:
                    self._features[self.MOTIF_SUM_KEY][motif_num] = 0
                self._features[self.MOTIF_SUM_KEY][motif_num] += 1

            if (i + 1) % 1000 == 0 and VERBOSE:
                self._logger.debug("Groups: %d" % i)

        self._logger.info(f"Finished calculating {self._level} motifs.")

        if not self.count_motifs:
#clean features
            self._all_relevant_motifs = {motif_number for node_motifs in self._features.values()
                                         for motif_number, appearances in node_motifs.items()}
}

}  // namespace sgf
