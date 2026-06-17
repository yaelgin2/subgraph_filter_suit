#pragma once

/**
 * @brief Compile-time backend selector for five-path enumeration.
 *
 * Include this header and refer to sgf::PathBackend to keep call sites
 * independent of the concrete implementation:
 *
 *   - SGF_CUDA_ENABLED (cmake -DSGF_ENABLE_CUDA=ON): aliases CudaPathBackend.
 *   - Otherwise: aliases PathProcessor (CPU thread-based implementation).
 *
 * The two backends will share a common constructor and process() interface
 * once the CUDA implementation is complete. Until then, CudaPathBackend
 * is a stub and PathProcessor remains the only functional backend.
 */

#ifdef SGF_CUDA_ENABLED
#    include "CudaPathBackend.h"

namespace sgf
{
/** GPU-backed path enumeration (selected when SGF_CUDA_ENABLED is defined). */
using PathBackend = CudaPathBackend;
}  // namespace sgf

#else
#    include "PathProcessor.h"

namespace sgf
{
/** CPU-backed path enumeration (default). */
using PathBackend = PathProcessor;
}  // namespace sgf

#endif
