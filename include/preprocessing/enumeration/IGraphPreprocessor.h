#pragma once

#include "Int128.h"

#include <cstdint>
#include <unordered_map>

namespace sgf
{

/**
 * @brief Frequency-signature map produced by a single graph's enumeration preprocessor.
 */
using EnumerationResult = std::unordered_map<UInt128, uint32_t, UInt128Hash>;

/**
 * @class IGraphPreprocessor
 * @brief Interface for graph enumeration preprocessors.
 *
 * Defines the contract for computing a motif or path frequency signature
 * from a colored graph. Concrete implementations are created by callers via
 * a factory and driven by EnumerationPreprocessManager.
 */
class IGraphPreprocessor
{
public:
    /**
     * @brief Default destructor.
     */
    virtual ~IGraphPreprocessor() = default;

    IGraphPreprocessor() = default;
    IGraphPreprocessor(const IGraphPreprocessor&) = delete;
    IGraphPreprocessor& operator=(const IGraphPreprocessor&) = delete;
    IGraphPreprocessor(IGraphPreprocessor&&) = delete;
    IGraphPreprocessor& operator=(IGraphPreprocessor&&) = delete;

    /**
     * @brief Run the full preprocessing pipeline and return a frequency signature.
     *
     * @param use_gpu If true, offload enumeration to CUDA GPU kernels.
     *                Requires the library to be compiled with SGF_CUDA_ENABLED.
     *                Throws InvalidArgumentException if called as true without CUDA support.
     * @return Map of motif identifier to occurrence count.
     */
    virtual EnumerationResult calculate(bool use_gpu = false) = 0;
};

}  // namespace sgf
