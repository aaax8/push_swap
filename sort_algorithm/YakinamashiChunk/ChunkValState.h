// Shared value allocation state for chunk operations
#ifndef UNTITLED_CHUNKVALSTATE_H
#define UNTITLED_CHUNKVALSTATE_H

#include "../../all_include.h"
#include "../InitBeam/CircularRange.h"

namespace YakinamashiChunk {
    using ChunkSeparator::CircularRange;

    //x 値割当結果の共通保持構造体
    template <size_t MAX_N>
    struct ChunkValState {
        my_vector<my_vector<int>> a_values;
        my_vector<my_vector<int>> b_values;
        my_bitset<MAX_N> is_a_lis;
        my_vector<my_bitset<MAX_N>> a_chunk_hases;
        my_vector<my_bitset<MAX_N>> b_chunk_hases;
        my_vector<CircularRange> a_ranges;
        my_vector<CircularRange> b_ranges;
    };
}

#endif // UNTITLED_CHUNKVALSTATE_H
