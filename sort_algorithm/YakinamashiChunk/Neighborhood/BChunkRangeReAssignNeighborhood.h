// sort_algorithm/YakinamashiChunk/Neighborhood/BChunkRangeReAssignNeighborhood.h
#ifndef B_CHUNK_RANGE_REASSIGN_NEIGHBORHOOD_H
#define B_CHUNK_RANGE_REASSIGN_NEIGHBORHOOD_H

#include "../../../all_include.h"
#include "../ChunkSeparator.h"
#include "../YakinamashiChunkState.h"
#include "../../../util.h"

namespace YakinamashiChunk {

// //x Bチャンク間で範囲を再割り当てする近傍生成
template <size_t MAX_N>
class BChunkRangeReAssignNeighborhood {
private:
    const YakinamashiChunkState<MAX_N> &current_state;
    double progress;

    // //x shift_limit範囲でランダムにシフト量を生成（正負ランダム）
    int get_shift_start_amount(int chunk_idx, int abs_shift_limit_progress);
    int get_shift_end_amount(int chunk_idx, int abs_shift_limit_progress);
    // //x stateのb_chunksから現在のチャンク情報を取得
    const my_vector<shared_ptr<Chunk<MAX_N>>>& get_cur_b_chunks() const;
    // //x BChunkRangeReAssignNeighborhoodのメンバ関数にする
    int prev_b_chunk_idx(int idx) const;
    int next_b_chunk_idx(int idx) const;
    // //x 隣接チャンクのインデックスを取得
    int adjacent_start_idx(int idx) const;
    int adjacent_end_idx(int idx) const;
    // //x クラスの定義にも作る
    void apply_shift_start(my_vector<CircularRange>& new_b_ranges, int chunk_idx, int shift_start);
    void apply_shift_end(my_vector<CircularRange>& new_b_ranges, int chunk_idx, int shift_end);
public:
    // //x コンストラクタ
    BChunkRangeReAssignNeighborhood(
        const YakinamashiChunkState<MAX_N> &state,
        double prog
    ) : current_state(state), progress(prog) {}

    // //x 新しい状態を生成
    // chunk_idx: 変更するBチャンクのインデックス
    YakinamashiChunkState<MAX_N> generate(size_t chunk_idx);
};

}  // namespace YakinamashiChunk

#endif
