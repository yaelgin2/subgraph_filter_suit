#include "MotifDag.h"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sgf
{

// clang-format off
const DagAdjacency UNDIRECTED_MOTIF_DAG = {
    {15u, {
        {11u, {DagPermutation{0u, 1u, 2u, 3u}, DagPermutation{0u, 2u, 1u, 3u}, DagPermutation{1u, 0u, 2u, 3u}, DagPermutation{1u, 2u, 0u, 3u}, DagPermutation{2u, 0u, 1u, 3u}, DagPermutation{2u, 1u, 0u, 3u}} },
        {13u, {DagPermutation{0u, 1u, 2u, 3u}, DagPermutation{1u, 0u, 3u, 2u}, DagPermutation{0u, 2u, 1u, 3u}, DagPermutation{1u, 3u, 0u, 2u}} },
    },},
    {30u, {
        {13u, {DagPermutation{0u, 2u, 1u, 3u}, DagPermutation{1u, 3u, 0u, 2u}, DagPermutation{0u, 2u, 3u, 1u}, DagPermutation{1u, 3u, 2u, 0u}, DagPermutation{2u, 0u, 1u, 3u}, DagPermutation{3u, 1u, 0u, 2u}, DagPermutation{2u, 0u, 3u, 1u}, DagPermutation{3u, 1u, 2u, 0u}} },
    },},
    {31u, {
        {11u, {DagPermutation{0u, 1u, 2u, 3u}, DagPermutation{0u, 2u, 1u, 3u}, DagPermutation{1u, 0u, 2u, 3u}, DagPermutation{1u, 2u, 0u, 3u}, DagPermutation{2u, 0u, 1u, 3u}, DagPermutation{2u, 1u, 0u, 3u}, DagPermutation{0u, 1u, 3u, 2u}, DagPermutation{0u, 2u, 3u, 1u}, DagPermutation{1u, 0u, 3u, 2u}, DagPermutation{1u, 2u, 3u, 0u}, DagPermutation{2u, 0u, 3u, 1u}, DagPermutation{2u, 1u, 3u, 0u}} },
        {13u, {DagPermutation{0u, 1u, 2u, 3u}, DagPermutation{1u, 0u, 3u, 2u}, DagPermutation{0u, 1u, 3u, 2u}, DagPermutation{1u, 0u, 2u, 3u}, DagPermutation{0u, 2u, 1u, 3u}, DagPermutation{1u, 3u, 0u, 2u}, DagPermutation{0u, 2u, 3u, 1u}, DagPermutation{1u, 3u, 2u, 0u}, DagPermutation{2u, 0u, 1u, 3u}, DagPermutation{3u, 1u, 0u, 2u}, DagPermutation{2u, 0u, 3u, 1u}, DagPermutation{3u, 1u, 2u, 0u}} },
        {15u, {DagPermutation{0u, 1u, 2u, 3u}, DagPermutation{0u, 2u, 1u, 3u}, DagPermutation{0u, 1u, 3u, 2u}, DagPermutation{0u, 2u, 3u, 1u}, DagPermutation{1u, 0u, 2u, 3u}, DagPermutation{2u, 0u, 1u, 3u}, DagPermutation{1u, 0u, 3u, 2u}, DagPermutation{2u, 0u, 3u, 1u}} },
        {30u, {DagPermutation{0u, 1u, 2u, 3u}, DagPermutation{0u, 1u, 3u, 2u}, DagPermutation{1u, 0u, 2u, 3u}, DagPermutation{1u, 0u, 3u, 2u}, DagPermutation{2u, 3u, 0u, 1u}, DagPermutation{2u, 3u, 1u, 0u}, DagPermutation{3u, 2u, 0u, 1u}, DagPermutation{3u, 2u, 1u, 0u}} },
    },},
    {63u, {
        {11u, {DagPermutation{0u, 1u, 2u, 3u}, DagPermutation{0u, 2u, 1u, 3u}, DagPermutation{1u, 0u, 2u, 3u}, DagPermutation{1u, 2u, 0u, 3u}, DagPermutation{2u, 0u, 1u, 3u}, DagPermutation{2u, 1u, 0u, 3u}, DagPermutation{0u, 1u, 3u, 2u}, DagPermutation{0u, 2u, 3u, 1u}, DagPermutation{1u, 0u, 3u, 2u}, DagPermutation{1u, 2u, 3u, 0u}, DagPermutation{2u, 0u, 3u, 1u}, DagPermutation{2u, 1u, 3u, 0u}, DagPermutation{0u, 3u, 1u, 2u}, DagPermutation{0u, 3u, 2u, 1u}, DagPermutation{1u, 3u, 0u, 2u}, DagPermutation{1u, 3u, 2u, 0u}, DagPermutation{2u, 3u, 0u, 1u}, DagPermutation{2u, 3u, 1u, 0u}, DagPermutation{3u, 0u, 1u, 2u}, DagPermutation{3u, 0u, 2u, 1u}, DagPermutation{3u, 1u, 0u, 2u}, DagPermutation{3u, 1u, 2u, 0u}, DagPermutation{3u, 2u, 0u, 1u}, DagPermutation{3u, 2u, 1u, 0u}} },
        {13u, {DagPermutation{0u, 1u, 2u, 3u}, DagPermutation{1u, 0u, 3u, 2u}, DagPermutation{0u, 1u, 3u, 2u}, DagPermutation{1u, 0u, 2u, 3u}, DagPermutation{0u, 2u, 1u, 3u}, DagPermutation{1u, 3u, 0u, 2u}, DagPermutation{0u, 2u, 3u, 1u}, DagPermutation{1u, 3u, 2u, 0u}, DagPermutation{0u, 3u, 1u, 2u}, DagPermutation{1u, 2u, 0u, 3u}, DagPermutation{0u, 3u, 2u, 1u}, DagPermutation{1u, 2u, 3u, 0u}, DagPermutation{2u, 0u, 1u, 3u}, DagPermutation{3u, 1u, 0u, 2u}, DagPermutation{2u, 0u, 3u, 1u}, DagPermutation{3u, 1u, 2u, 0u}, DagPermutation{2u, 1u, 0u, 3u}, DagPermutation{3u, 0u, 1u, 2u}, DagPermutation{2u, 1u, 3u, 0u}, DagPermutation{3u, 0u, 2u, 1u}, DagPermutation{2u, 3u, 0u, 1u}, DagPermutation{3u, 2u, 1u, 0u}, DagPermutation{2u, 3u, 1u, 0u}, DagPermutation{3u, 2u, 0u, 1u}} },
        {15u, {DagPermutation{0u, 1u, 2u, 3u}, DagPermutation{0u, 2u, 1u, 3u}, DagPermutation{0u, 1u, 3u, 2u}, DagPermutation{0u, 2u, 3u, 1u}, DagPermutation{0u, 3u, 1u, 2u}, DagPermutation{0u, 3u, 2u, 1u}, DagPermutation{1u, 0u, 2u, 3u}, DagPermutation{2u, 0u, 1u, 3u}, DagPermutation{1u, 0u, 3u, 2u}, DagPermutation{2u, 0u, 3u, 1u}, DagPermutation{1u, 2u, 0u, 3u}, DagPermutation{2u, 1u, 0u, 3u}, DagPermutation{1u, 2u, 3u, 0u}, DagPermutation{2u, 1u, 3u, 0u}, DagPermutation{1u, 3u, 0u, 2u}, DagPermutation{2u, 3u, 0u, 1u}, DagPermutation{1u, 3u, 2u, 0u}, DagPermutation{2u, 3u, 1u, 0u}, DagPermutation{3u, 0u, 1u, 2u}, DagPermutation{3u, 0u, 2u, 1u}, DagPermutation{3u, 1u, 0u, 2u}, DagPermutation{3u, 2u, 0u, 1u}, DagPermutation{3u, 1u, 2u, 0u}, DagPermutation{3u, 2u, 1u, 0u}} },
        {30u, {DagPermutation{0u, 1u, 2u, 3u}, DagPermutation{0u, 1u, 3u, 2u}, DagPermutation{1u, 0u, 2u, 3u}, DagPermutation{1u, 0u, 3u, 2u}, DagPermutation{2u, 3u, 0u, 1u}, DagPermutation{2u, 3u, 1u, 0u}, DagPermutation{3u, 2u, 0u, 1u}, DagPermutation{3u, 2u, 1u, 0u}, DagPermutation{0u, 2u, 1u, 3u}, DagPermutation{0u, 3u, 1u, 2u}, DagPermutation{1u, 2u, 0u, 3u}, DagPermutation{1u, 3u, 0u, 2u}, DagPermutation{2u, 0u, 3u, 1u}, DagPermutation{2u, 1u, 3u, 0u}, DagPermutation{3u, 0u, 2u, 1u}, DagPermutation{3u, 1u, 2u, 0u}, DagPermutation{0u, 2u, 3u, 1u}, DagPermutation{0u, 3u, 2u, 1u}, DagPermutation{1u, 2u, 3u, 0u}, DagPermutation{1u, 3u, 2u, 0u}, DagPermutation{2u, 0u, 1u, 3u}, DagPermutation{2u, 1u, 0u, 3u}, DagPermutation{3u, 0u, 1u, 2u}, DagPermutation{3u, 1u, 0u, 2u}} },
        {31u, {DagPermutation{0u, 1u, 2u, 3u}, DagPermutation{0u, 1u, 3u, 2u}, DagPermutation{1u, 0u, 2u, 3u}, DagPermutation{1u, 0u, 3u, 2u}, DagPermutation{0u, 2u, 1u, 3u}, DagPermutation{0u, 3u, 1u, 2u}, DagPermutation{1u, 2u, 0u, 3u}, DagPermutation{1u, 3u, 0u, 2u}, DagPermutation{0u, 2u, 3u, 1u}, DagPermutation{0u, 3u, 2u, 1u}, DagPermutation{1u, 2u, 3u, 0u}, DagPermutation{1u, 3u, 2u, 0u}, DagPermutation{2u, 0u, 1u, 3u}, DagPermutation{2u, 1u, 0u, 3u}, DagPermutation{3u, 0u, 1u, 2u}, DagPermutation{3u, 1u, 0u, 2u}, DagPermutation{2u, 0u, 3u, 1u}, DagPermutation{2u, 1u, 3u, 0u}, DagPermutation{3u, 0u, 2u, 1u}, DagPermutation{3u, 1u, 2u, 0u}, DagPermutation{2u, 3u, 0u, 1u}, DagPermutation{2u, 3u, 1u, 0u}, DagPermutation{3u, 2u, 0u, 1u}, DagPermutation{3u, 2u, 1u, 0u}} },
    },},
};
// clang-format on

}  // namespace sgf