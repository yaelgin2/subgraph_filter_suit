#include "MotifDag.h"

namespace sgf
{

// clang-format off
const DagAdjacency UNDIRECTED_MOTIF_DAG = {
    {15U, {
        {11U, {DagPermutation{0U, 1U, 2U, 3U}, DagPermutation{0U, 2U, 1U, 3U}, DagPermutation{1U, 0U, 2U, 3U}, DagPermutation{1U, 2U, 0U, 3U}, DagPermutation{2U, 0U, 1U, 3U}, DagPermutation{2U, 1U, 0U, 3U}} },
        {13U, {DagPermutation{0U, 1U, 2U, 3U}, DagPermutation{1U, 0U, 3U, 2U}, DagPermutation{0U, 2U, 1U, 3U}, DagPermutation{1U, 3U, 0U, 2U}} },
    },},
    {30U, {
        {13U, {DagPermutation{0U, 2U, 1U, 3U}, DagPermutation{1U, 3U, 0U, 2U}, DagPermutation{0U, 2U, 3U, 1U}, DagPermutation{1U, 3U, 2U, 0U}, DagPermutation{2U, 0U, 1U, 3U}, DagPermutation{3U, 1U, 0U, 2U}, DagPermutation{2U, 0U, 3U, 1U}, DagPermutation{3U, 1U, 2U, 0U}} },
    },},
    {31U, {
        {11U, {DagPermutation{0U, 1U, 2U, 3U}, DagPermutation{0U, 2U, 1U, 3U}, DagPermutation{1U, 0U, 2U, 3U}, DagPermutation{1U, 2U, 0U, 3U}, DagPermutation{2U, 0U, 1U, 3U}, DagPermutation{2U, 1U, 0U, 3U}, DagPermutation{0U, 1U, 3U, 2U}, DagPermutation{0U, 2U, 3U, 1U}, DagPermutation{1U, 0U, 3U, 2U}, DagPermutation{1U, 2U, 3U, 0U}, DagPermutation{2U, 0U, 3U, 1U}, DagPermutation{2U, 1U, 3U, 0U}} },
        {13U, {DagPermutation{0U, 1U, 2U, 3U}, DagPermutation{1U, 0U, 3U, 2U}, DagPermutation{0U, 1U, 3U, 2U}, DagPermutation{1U, 0U, 2U, 3U}, DagPermutation{0U, 2U, 1U, 3U}, DagPermutation{1U, 3U, 0U, 2U}, DagPermutation{0U, 2U, 3U, 1U}, DagPermutation{1U, 3U, 2U, 0U}, DagPermutation{2U, 0U, 1U, 3U}, DagPermutation{3U, 1U, 0U, 2U}, DagPermutation{2U, 0U, 3U, 1U}, DagPermutation{3U, 1U, 2U, 0U}} },
        {15U, {DagPermutation{0U, 1U, 2U, 3U}, DagPermutation{0U, 2U, 1U, 3U}, DagPermutation{0U, 1U, 3U, 2U}, DagPermutation{0U, 2U, 3U, 1U}, DagPermutation{1U, 0U, 2U, 3U}, DagPermutation{2U, 0U, 1U, 3U}, DagPermutation{1U, 0U, 3U, 2U}, DagPermutation{2U, 0U, 3U, 1U}} },
        {30U, {DagPermutation{0U, 1U, 2U, 3U}, DagPermutation{0U, 1U, 3U, 2U}, DagPermutation{1U, 0U, 2U, 3U}, DagPermutation{1U, 0U, 3U, 2U}, DagPermutation{2U, 3U, 0U, 1U}, DagPermutation{2U, 3U, 1U, 0U}, DagPermutation{3U, 2U, 0U, 1U}, DagPermutation{3U, 2U, 1U, 0U}} },
    },},
    {63U, {
        {11U, {DagPermutation{0U, 1U, 2U, 3U}, DagPermutation{0U, 2U, 1U, 3U}, DagPermutation{1U, 0U, 2U, 3U}, DagPermutation{1U, 2U, 0U, 3U}, DagPermutation{2U, 0U, 1U, 3U}, DagPermutation{2U, 1U, 0U, 3U}, DagPermutation{0U, 1U, 3U, 2U}, DagPermutation{0U, 2U, 3U, 1U}, DagPermutation{1U, 0U, 3U, 2U}, DagPermutation{1U, 2U, 3U, 0U}, DagPermutation{2U, 0U, 3U, 1U}, DagPermutation{2U, 1U, 3U, 0U}, DagPermutation{0U, 3U, 1U, 2U}, DagPermutation{0U, 3U, 2U, 1U}, DagPermutation{1U, 3U, 0U, 2U}, DagPermutation{1U, 3U, 2U, 0U}, DagPermutation{2U, 3U, 0U, 1U}, DagPermutation{2U, 3U, 1U, 0U}, DagPermutation{3U, 0U, 1U, 2U}, DagPermutation{3U, 0U, 2U, 1U}, DagPermutation{3U, 1U, 0U, 2U}, DagPermutation{3U, 1U, 2U, 0U}, DagPermutation{3U, 2U, 0U, 1U}, DagPermutation{3U, 2U, 1U, 0U}} },
        {13U, {DagPermutation{0U, 1U, 2U, 3U}, DagPermutation{1U, 0U, 3U, 2U}, DagPermutation{0U, 1U, 3U, 2U}, DagPermutation{1U, 0U, 2U, 3U}, DagPermutation{0U, 2U, 1U, 3U}, DagPermutation{1U, 3U, 0U, 2U}, DagPermutation{0U, 2U, 3U, 1U}, DagPermutation{1U, 3U, 2U, 0U}, DagPermutation{0U, 3U, 1U, 2U}, DagPermutation{1U, 2U, 0U, 3U}, DagPermutation{0U, 3U, 2U, 1U}, DagPermutation{1U, 2U, 3U, 0U}, DagPermutation{2U, 0U, 1U, 3U}, DagPermutation{3U, 1U, 0U, 2U}, DagPermutation{2U, 0U, 3U, 1U}, DagPermutation{3U, 1U, 2U, 0U}, DagPermutation{2U, 1U, 0U, 3U}, DagPermutation{3U, 0U, 1U, 2U}, DagPermutation{2U, 1U, 3U, 0U}, DagPermutation{3U, 0U, 2U, 1U}, DagPermutation{2U, 3U, 0U, 1U}, DagPermutation{3U, 2U, 1U, 0U}, DagPermutation{2U, 3U, 1U, 0U}, DagPermutation{3U, 2U, 0U, 1U}} },
        {15U, {DagPermutation{0U, 1U, 2U, 3U}, DagPermutation{0U, 2U, 1U, 3U}, DagPermutation{0U, 1U, 3U, 2U}, DagPermutation{0U, 2U, 3U, 1U}, DagPermutation{0U, 3U, 1U, 2U}, DagPermutation{0U, 3U, 2U, 1U}, DagPermutation{1U, 0U, 2U, 3U}, DagPermutation{2U, 0U, 1U, 3U}, DagPermutation{1U, 0U, 3U, 2U}, DagPermutation{2U, 0U, 3U, 1U}, DagPermutation{1U, 2U, 0U, 3U}, DagPermutation{2U, 1U, 0U, 3U}, DagPermutation{1U, 2U, 3U, 0U}, DagPermutation{2U, 1U, 3U, 0U}, DagPermutation{1U, 3U, 0U, 2U}, DagPermutation{2U, 3U, 0U, 1U}, DagPermutation{1U, 3U, 2U, 0U}, DagPermutation{2U, 3U, 1U, 0U}, DagPermutation{3U, 0U, 1U, 2U}, DagPermutation{3U, 0U, 2U, 1U}, DagPermutation{3U, 1U, 0U, 2U}, DagPermutation{3U, 2U, 0U, 1U}, DagPermutation{3U, 1U, 2U, 0U}, DagPermutation{3U, 2U, 1U, 0U}} },
        {30U, {DagPermutation{0U, 1U, 2U, 3U}, DagPermutation{0U, 1U, 3U, 2U}, DagPermutation{1U, 0U, 2U, 3U}, DagPermutation{1U, 0U, 3U, 2U}, DagPermutation{2U, 3U, 0U, 1U}, DagPermutation{2U, 3U, 1U, 0U}, DagPermutation{3U, 2U, 0U, 1U}, DagPermutation{3U, 2U, 1U, 0U}, DagPermutation{0U, 2U, 1U, 3U}, DagPermutation{0U, 3U, 1U, 2U}, DagPermutation{1U, 2U, 0U, 3U}, DagPermutation{1U, 3U, 0U, 2U}, DagPermutation{2U, 0U, 3U, 1U}, DagPermutation{2U, 1U, 3U, 0U}, DagPermutation{3U, 0U, 2U, 1U}, DagPermutation{3U, 1U, 2U, 0U}, DagPermutation{0U, 2U, 3U, 1U}, DagPermutation{0U, 3U, 2U, 1U}, DagPermutation{1U, 2U, 3U, 0U}, DagPermutation{1U, 3U, 2U, 0U}, DagPermutation{2U, 0U, 1U, 3U}, DagPermutation{2U, 1U, 0U, 3U}, DagPermutation{3U, 0U, 1U, 2U}, DagPermutation{3U, 1U, 0U, 2U}} },
        {31U, {DagPermutation{0U, 1U, 2U, 3U}, DagPermutation{0U, 1U, 3U, 2U}, DagPermutation{1U, 0U, 2U, 3U}, DagPermutation{1U, 0U, 3U, 2U}, DagPermutation{0U, 2U, 1U, 3U}, DagPermutation{0U, 3U, 1U, 2U}, DagPermutation{1U, 2U, 0U, 3U}, DagPermutation{1U, 3U, 0U, 2U}, DagPermutation{0U, 2U, 3U, 1U}, DagPermutation{0U, 3U, 2U, 1U}, DagPermutation{1U, 2U, 3U, 0U}, DagPermutation{1U, 3U, 2U, 0U}, DagPermutation{2U, 0U, 1U, 3U}, DagPermutation{2U, 1U, 0U, 3U}, DagPermutation{3U, 0U, 1U, 2U}, DagPermutation{3U, 1U, 0U, 2U}, DagPermutation{2U, 0U, 3U, 1U}, DagPermutation{2U, 1U, 3U, 0U}, DagPermutation{3U, 0U, 2U, 1U}, DagPermutation{3U, 1U, 2U, 0U}, DagPermutation{2U, 3U, 0U, 1U}, DagPermutation{2U, 3U, 1U, 0U}, DagPermutation{3U, 2U, 0U, 1U}, DagPermutation{3U, 2U, 1U, 0U}} },
    },},
};
// clang-format on

}  // namespace sgf
