#pragma once

/**
 * @brief Compile-time backend selector for motif enumeration.
 *
 * Include this header and refer to sgf::MotifBackend to keep call sites
 * independent of the concrete implementation:
 *
 *   - SGF_CUDA_ENABLED (cmake -DSGF_ENABLE_CUDA=ON): aliases CudaMotifBackend.
 *   - Otherwise: aliases MotifPreprocessor (CPU thread-based implementation).
 *
 * The two backends will share a common constructor and process() interface
 * once the CUDA implementation is complete. Until then, CudaMotifBackend
 * is a stub and MotifPreprocessor remains the only functional backend.
 */

#ifdef SGF_CUDA_ENABLED
#include "CudaMotifBackend.h"

namespace sgf
{
/** GPU-backed motif enumeration (selected when SGF_CUDA_ENABLED is defined). */
using MotifBackend = CudaMotifBackend;
}  // namespace sgf

#else
#include "MotifPreprocessor.h"

namespace sgf
{
/** CPU-backed motif enumeration (default). */
using MotifBackend = MotifPreprocessor;
}  // namespace sgf

#endif
