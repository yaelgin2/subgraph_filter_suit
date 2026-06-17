#pragma once

/**
 * @brief Compile-time backend selector for subgraph isomorphism search.
 *
 * Include this header and refer to sgf::SearcherBackend to keep call sites
 * independent of the concrete implementation:
 *
 *   - SGF_CUDA_ENABLED (cmake -DSGF_ENABLE_CUDA=ON): aliases CudaSearcherBackend.
 *   - Otherwise: aliases SubgraphSearcher (CPU thread-based implementation).
 *
 * The two backends will share a common find_all() interface once the CUDA
 * implementation is complete. Until then, CudaSearcherBackend is a stub
 * and SubgraphSearcher remains the only functional backend.
 */

#ifdef SGF_CUDA_ENABLED
#    include "CudaSearcherBackend.h"

namespace sgf
{
/** GPU-backed isomorphism searcher (selected when SGF_CUDA_ENABLED is defined). */
using SearcherBackend = CudaSearcherBackend;
}  // namespace sgf

#else
#    include "SubgraphSearcher.h"

namespace sgf
{
/** CPU-backed isomorphism searcher (default). */
using SearcherBackend = SubgraphSearcher;
}  // namespace sgf

#endif
