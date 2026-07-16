// sort_algorithm/YakinamashiChunk/Neighborhood/BChunkRangeReAssignNeighborhood.cpp
#include "BChunkRangeReAssignNeighborhood.h"
#include "YakinamashiChunk/ChunkValState.h"

namespace YakinamashiChunk {

// //x [a, b) 範囲のランダムな整数を返す
    int rand_range(int a, int b) {
        my_assert(a < b);
        return a + (rand() % (b - a));
    }

// //x 現在のBチャンクを取得する
    template<size_t MAX_N>
    const my_vector<shared_ptr<Chunk<MAX_N>>> &BChunkRangeReAssignNeighborhood<MAX_N>::get_cur_b_chunks() const {
        my_assert(!current_state.tree.final_b_chunks.empty());
        return current_state.tree.final_b_chunks;
    }

//@ メンバ関数にする
    template<size_t MAX_N>
    int BChunkRangeReAssignNeighborhood<MAX_N>::prev_b_chunk_idx(int idx) const {
        return mod(idx - 1, this->get_cur_b_chunks().size());
    }

//@ メンバ関数にする
    template<size_t MAX_N>
    int BChunkRangeReAssignNeighborhood<MAX_N>::next_b_chunk_idx(int idx) const {
        return mod(idx + 1, this->get_cur_b_chunks().size());
    }

    //@メンバ関数にする
//[100, 200)なら、[*, 100)となるチャンクのidxをかえす
//bは全ての範囲を占めるので、これは、一つ前か1つ後ろにある
    template<size_t MAX_N>
    int BChunkRangeReAssignNeighborhood<MAX_N>::adjacent_start_idx(int idx) const {
        int cur_start = current_state.val_state.b_ranges[idx].get_start();
        int prev_idx = this->prev_b_chunk_idx(idx);
        if (current_state.val_state.b_ranges[prev_idx].get_end() == cur_start) {
            return prev_idx;
        } else {
            int next_idx = this->next_b_chunk_idx(idx);
            my_assert(current_state.val_state.b_ranges[next_idx].get_end() == cur_start);
            return next_idx;
        }
    }

    //@メンバ関数にする
    template<size_t MAX_N>
    int BChunkRangeReAssignNeighborhood<MAX_N>::adjacent_end_idx(int idx) const {
        int cur_end = current_state.val_state.b_ranges[idx].get_end();
        int prev_idx = this->prev_b_chunk_idx(idx);
        if (current_state.val_state.b_ranges[prev_idx].get_start() == cur_end) {
            return prev_idx;
        } else {
            int next_idx = this->next_b_chunk_idx(idx);
            my_assert(current_state.val_state.b_ranges[next_idx].get_start() == cur_end);
            return next_idx;
        }
    }


// //x shift_limit範囲でランダムにシフト量を生成（正負ランダム）
//＠クラスの定義をこの名前に変える
    template<size_t MAX_N>
    int BChunkRangeReAssignNeighborhood<MAX_N>::get_shift_start_amount(int chunk_idx, int abs_shift_limit_progress) {
        auto &b_chunks = this->get_cur_b_chunks();
        //範囲を増やす
        if (rand() % 2 == 0) {
            int adj_start_idx = this->adjacent_start_idx(chunk_idx);
            int shift_limit_inc = min(abs_shift_limit_progress, (int) b_chunks[adj_start_idx]->get_size());
            int shift_inc = rand_range(0, shift_limit_inc + 1);
            return -shift_inc;
        } else {
            int shift_limit_dec = min(abs_shift_limit_progress, (int) b_chunks[chunk_idx]->get_size());
            int shift_dec = rand_range(0, shift_limit_dec + 1);
            return shift_dec;
        }
    }

//＠クラスの定義をこの名前に変える
    template<size_t MAX_N>
    int BChunkRangeReAssignNeighborhood<MAX_N>::get_shift_end_amount(int chunk_idx, int abs_shift_limit_progress) {
        auto &b_chunks = this->get_cur_b_chunks();
        //へらす
        if (rand() % 2 == 0) {
            int shift_limit_dec = min(abs_shift_limit_progress, (int) b_chunks[chunk_idx]->get_size());
            int shift_dec = rand_range(0, shift_limit_dec + 1);
            return -shift_dec;
        } else {
            int adj_end_idx = this->adjacent_end_idx(chunk_idx);
            int shift_limit_inc = min(abs_shift_limit_progress, (int) b_chunks[adj_end_idx]->get_size());
            int shift_inc = rand_range(0, shift_limit_inc + 1);
            return shift_inc;
        }
    }


//@ CircularRangeを加工するための処理をまとめたCircularRangeProviderを作ってそこに移動する
//amountが負なら、減る
    static CircularRange get_inc_end(CircularRange &range, int amount) {
        int prev_size = range.size();
        int new_size = range.size() + amount;
        my_assert(0 <= new_size && new_size <= range.get_N());
        //同じでも作り直す
        return CircularRange(range.get_start(), new_size, range.get_N());
    }

//@ CircularRangeを加工するための処理をまとめたCircularRangeProviderを作ってそこに移動する
//amountが負なら、減る
    static CircularRange get_inc_start(CircularRange &range, int amount) {
        int prev_size = range.size();
        int new_size = range.size() - amount;//amountが負なら、増える
        my_assert(0 <= new_size && new_size <= range.get_N());
        int new_start = mod(range.get_start() + amount, range.get_N());
        //同じでも作り直す
        CircularRange ret = CircularRange(new_start, new_size, range.get_N());
        my_assert(ret.get_end() == range.get_end());
        return ret;
    }

//@ ファイル分けたい


//@ クラスの定義にも作る
//範囲が被ることは気にしない
    template<size_t MAX_N>
    void BChunkRangeReAssignNeighborhood<MAX_N>::apply_shift_start(my_vector<CircularRange> &new_b_ranges, int chunk_idx,
                                                                   int shift_start) {
        my_assert(0 <= chunk_idx && chunk_idx < current_state.tree.final_b_chunks.size());
        if (shift_start == 0) {
            return;
        }
        new_b_ranges[chunk_idx] = get_inc_start(new_b_ranges[chunk_idx], shift_start);
        int adj_start_idx = this->adjacent_start_idx(chunk_idx);
        new_b_ranges[adj_start_idx] = get_inc_end(new_b_ranges[adj_start_idx], shift_start);
    }


//@ クラスの定義にも作る
    template<size_t MAX_N>
    void BChunkRangeReAssignNeighborhood<MAX_N>::apply_shift_end(my_vector<CircularRange> &new_b_ranges, int chunk_idx,
                                                                 int shift_end) {
        my_assert(0 <= chunk_idx && chunk_idx < current_state.tree.final_b_chunks.size());
        if (shift_end == 0) {
            return;
        }
        new_b_ranges[chunk_idx] = get_inc_end(new_b_ranges[chunk_idx], shift_end);
        int adj_end_idx = this->adjacent_end_idx(chunk_idx);
        new_b_ranges[adj_end_idx] = get_inc_start(new_b_ranges[adj_end_idx], shift_end);
    }

// //x Bチャンク範囲再割り当て近傍の生成
    template<size_t MAX_N>
    YakinamashiChunkState<MAX_N>
    BChunkRangeReAssignNeighborhood<MAX_N>::generate(size_t chunk_idx) {
        my_assert(0 <= chunk_idx && chunk_idx < current_state.tree.final_b_chunks.size());
        my_assert(0.0 <= progress && progress <= 1.0);

        my_vector<CircularRange> new_b_ranges = current_state.val_state.b_ranges;

        // shift_limitの計算例：進捗に応じて修正の大きさを調整
        double avg_chunk_b_size = current_state.state_N / (double) current_state.tree.final_b_chunks.size();
        double shift_limit_start = avg_chunk_b_size * 0.3;
        double shift_limit_end = 5.0;
        int shift_limit_progress = static_cast<int>(shift_limit_start +
                                                    (shift_limit_end - shift_limit_start) * progress + 0.5);

        // shift_limitに基づいてシフト量を生成（例）
        int shift_start = get_shift_start_amount(chunk_idx, shift_limit_progress);
        apply_shift_start(new_b_ranges, chunk_idx, shift_start);
        int shift_end = get_shift_end_amount(chunk_idx, shift_limit_progress);
        apply_shift_end(new_b_ranges, chunk_idx, shift_end);
//
//        out("chunk_idx", chunk_idx);
//        out("shift_start", shift_start);
//        out("shift_end", shift_end);
        // 新しい状態を生成
        ChunkTree<MAX_N> new_tree = get_chunks<MAX_N>(current_state.queries, current_state.state_N);
        YakinamashiChunk::ChunkValCalculator<MAX_N> chunk_val_calculator(current_state.state_N);
        ChunkValState<MAX_N> new_val_state =
                chunk_val_calculator.re_build_by_b_ranges(
                        current_state.val_state,
                        new_b_ranges
                );
        YakinamashiChunk::ChunkConstraintAssignor<MAX_N> assignor(current_state.state_N);
        ValueOrderConstraint<MAX_N> constraint =
                assignor.assign_orderedA_unorderedB(new_tree, new_val_state.a_values, new_val_state.b_values);
        YakinamashiChunkState<MAX_N> st{current_state.state_N, current_state.queries, new_tree,
                                        new_val_state, constraint};
        st.validate_basic();

        return st;
    }

}  // namespace YakinamashiChunk

// テンプレート明示化
template
class YakinamashiChunk::BChunkRangeReAssignNeighborhood<500ull>;

