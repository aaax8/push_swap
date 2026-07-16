// Minimal declarations for annealing chunk state and factory
#ifndef UNTITLED_CHUNKSTATEFACTORY_H
#define UNTITLED_CHUNKSTATEFACTORY_H

#include "../../all_include.h"
#include "../../util.h"
#include "../InitBeam/CircularRange.h"
#include "ChunkSeparator.h"
#include "YakinamashiChunkState.h"

namespace YakinamashiChunk {
    using ChunkSeparator::CircularRange;
    using ChunkSeparator::circular_ranges_overlap_nieve;

    //x 状態の生成だけを担当するファクトリ（実装は後で追加）
    class ChunkStateFactory {
    public:
        //x
        template<size_t MAX_N>
        static YakinamashiChunkState<MAX_N> build_initial_state(const State &initial_state,
                                                                const my_vector<ChunkQuery> &queries,
                                                                int state_N,
                                                                int lis_start = 0
                                                                ) {
            ChunkTree<MAX_N> tree = get_chunks<MAX_N>(queries, state_N);
            ChunkRangeDivider divider(state_N);
            int a_chunk_cnt = static_cast<int>(tree.final_a_chunks.size());
            int b_chunk_cnt = static_cast<int>(tree.final_b_chunks.size());
            my_assert(b_chunk_cnt > 0);

            my_vector<CircularRange> a_ranges = divider.divide_a_ranges(a_chunk_cnt, 0);
            my_vector<CircularRange> b_ranges = divider.divide_b_ranges(b_chunk_cnt, 0);

            ChunkValCalculator<MAX_N> chunk_val_calculator(state_N);
            ChunkValState<MAX_N> val_state = chunk_val_calculator.build_a_lis_with_swap_b_desc(
                    tree, initial_state, a_ranges, b_ranges, lis_start
            );
                                                   
            ChunkConstraintAssignor<MAX_N> assignor(state_N);
            ValueOrderConstraint<MAX_N> constraint = 
                assignor.assign_orderedA_unorderedB(tree, val_state.a_values, val_state.b_values);

            YakinamashiChunkState<MAX_N> st{state_N, queries, tree,
                                            val_state, constraint};
            st.validate_basic();
            return st;
        }
    };
}
#endif // UNTITLED_CHUNKSTATEFACTORY_H
