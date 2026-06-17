#pragma once

#ifdef SGF_CUDA_ENABLED

#include <cstdint>

namespace sgf
{

/**
 * @brief CUDA-accelerated subgraph isomorphism search backend.
 *
 * Stub — CUDA kernel implementations are pending. This header compiles cleanly
 * under both nvcc and standard C++ compilers (guarded by SGF_CUDA_ENABLED).
 *
 * When GPU kernels are added, this class will expose the same search interface
 * as SubgraphSearcher so that SearcherBackendDispatch.h can alias
 * either implementation transparently.
 */
class CudaSearcherBackend
{
public:
    CudaSearcherBackend() = default;
    ~CudaSearcherBackend() = default;

    CudaSearcherBackend(const CudaSearcherBackend&) = delete;
    CudaSearcherBackend& operator=(const CudaSearcherBackend&) = delete;
    CudaSearcherBackend(CudaSearcherBackend&&) = delete;
    CudaSearcherBackend& operator=(CudaSearcherBackend&&) = delete;
};

}  // namespace sgf

#endif  // SGF_CUDA_ENABLED
