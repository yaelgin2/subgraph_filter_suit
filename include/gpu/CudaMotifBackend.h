#pragma once

#ifdef SGF_CUDA_ENABLED

#include <cstdint>

namespace sgf
{

/**
 * @brief CUDA-accelerated motif enumeration backend.
 *
 * Stub — CUDA kernel implementations are pending. This header compiles cleanly
 * under both nvcc and standard C++ compilers (guarded by SGF_CUDA_ENABLED).
 *
 * When GPU kernels are added, this class will expose the same enumeration
 * interface as MotifPreprocessor so that MotifBackendDispatch.h can alias
 * either implementation transparently.
 */
class CudaMotifBackend
{
public:
    CudaMotifBackend() = default;
    ~CudaMotifBackend() = default;

    CudaMotifBackend(const CudaMotifBackend&) = delete;
    CudaMotifBackend& operator=(const CudaMotifBackend&) = delete;
    CudaMotifBackend(CudaMotifBackend&&) = delete;
    CudaMotifBackend& operator=(CudaMotifBackend&&) = delete;
};

}  // namespace sgf

#endif  // SGF_CUDA_ENABLED
