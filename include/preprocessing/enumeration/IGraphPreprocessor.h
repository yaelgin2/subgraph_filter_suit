#pragma once

#include "Int128.h"

#include <cstdint>
#include <unordered_map>

namespace sgf
{

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

    /**
     * @brief Run the full preprocessing pipeline and return a frequency signature.
     *
     * @return Map of motif identifier to occurrence count.
     */
    virtual std::unordered_map<UInt128, uint32_t, UInt128Hash> calculate() = 0;
};

}  // namespace sgf
