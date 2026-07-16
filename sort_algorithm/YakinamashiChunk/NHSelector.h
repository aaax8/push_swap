// sort_algorithm/YakinamashiChunk/NHSelector.h
#ifndef NH_SELECTOR_H
#define NH_SELECTOR_H

#include "../../all_include.h"
#include "Neighborhood/ChunkYakinamashiNeighborhood.h"
#include "Neighborhood/BChunkRangeReAssignNeighborhood.h"
#include "YakinamashiChunkState.h"

namespace YakinamashiChunk {

// //x 近傍を選択・生成するセレクタ
template <size_t MAX_N>
class NHSelector {
private:
    const YakinamashiChunkState<MAX_N> &current_state;
    size_t state_N;
    
public:
    // //x コンストラクタ
    NHSelector(
        const YakinamashiChunkState<MAX_N> &state,
        size_t n
    ) : current_state(state), state_N(n) {
        my_assert(0 < state_N && state_N <= MAX_N);
    }
    
    // //x 近傍を選択・生成（状態のみ返す。スコア計算はメインループで行う）
    YakinamashiChunkState<MAX_N> select_and_generate(double progress) {
        my_assert(current_state.tree.final_b_chunks.size() > 1);
        my_assert(0.0 <= progress && progress <= 1.0);
        
        // 簡易実装：BChunkRangeReAssignNeighborhoodで試行
        // TODO: より高度な近傍選択ロジックを実装
        size_t chunk_idx = rand() % current_state.tree.final_b_chunks.size(); 
        BChunkRangeReAssignNeighborhood<MAX_N> nh(current_state, progress);
        return nh.generate(chunk_idx);
    }
};

}  // namespace YakinamashiChunk

#endif
