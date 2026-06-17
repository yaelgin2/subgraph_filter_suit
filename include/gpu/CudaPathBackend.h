#pragma once

#ifdef SGF_CUDA_ENABLED

#include <cstdint>

namespace sgf
{

/**
 * @brief CUDA-accelerated five-path enumeration backend.
 *
 * Stub — CUDA kernel implementations are pending. This header compiles cleanly
 * under both nvcc and standard C++ compilers (guarded by SGF_CUDA_ENABLED).
 *
 * When GPU kernels are added, this class will expose the same enumeration
 * interface as PathProcessor so that PathBackendDispatch.h can alias
 * either implementation transparently.
 */
class CudaPathBackend
{
public:
    CudaPathBackend() = default;
    ~CudaPathBackend() = default;

    CudaPathBackend(const CudaPathBackend&) = delete;
    CudaPathBackend& operator=(const CudaPathBackend&) = delete;
    CudaPathBackend(CudaPathBackend&&) = delete;
    CudaPathBackend& operator=(CudaPathBackend&&) = delete;
};

}  // namespace sgf

#endif  // SGF_CUDA_ENABLED
