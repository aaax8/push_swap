// sort_algorithm/YakinamashiChunk/YakinamashiChunkState.h
#ifndef UNTITLED_YAKINAMASHICHUNKSTATE_H
#define UNTITLED_YAKINAMASHICHUNKSTATE_H

#include "../../all_include.h"
#include "../../util.h"
#include "../InitBeam/CircularRange.h"
#include "ChunkSeparator.h"

namespace YakinamashiChunk {
    using ChunkSeparator::CircularRange;
    using ChunkSeparator::circular_ranges_overlap_nieve;

    //o 焼きなまし用のチャンク状態コンテナ（責務: 状態保持のみ。実装は後で追加）
    template<size_t MAX_N>
    struct YakinamashiChunkState {
        int state_N{};
        my_vector<ChunkQuery> queries;              // 分割クエリ（毎回 tree を再構築するため保持）
        ChunkTree<MAX_N> tree;                   // assign 未適用のクリアな tree を保持
        ChunkValState<MAX_N> val_state;          // 値割当結果一式
        ValueOrderConstraint<MAX_N> constraint;  // A側チャンクの順序制約

        //o
        bool is_valid_has_unique_and_complete(
                const my_vector<my_bitset<MAX_N>> &a_sets,
                const my_vector<my_bitset<MAX_N>> &b_sets
        ) const {
            my_bitset<MAX_N> seen;
            // Aチャンクの値を集計
            for (const auto &s: a_sets) {
                if ((seen & s).any()) {
                    return false; 
                }
                seen |= s;
            }
            // Bチャンクの値を集計
            for (const auto &s: b_sets) {
                if ((seen & s).any()) {
                    return false; 
                }
                seen |= s;
            }
            if (pop_cnt(seen, 0, state_N) != state_N) {
                return false;
            }
            return true;
        }

        //x
        bool is_valid_cnt() const {
            int a_chunk_cnt = static_cast<int>(tree.final_a_chunks.size());
            int b_chunk_cnt = static_cast<int>(tree.final_b_chunks.size());
            if (!(0 <= state_N && state_N <= MAX_N)) return false;
            if (tree.state_N != state_N) return false;
            if (val_state.a_ranges.size() != static_cast<size_t>(a_chunk_cnt)) return false;
            if (val_state.b_ranges.size() != static_cast<size_t>(b_chunk_cnt)) return false;
            if (val_state.a_values.size() != static_cast<size_t>(a_chunk_cnt)) return false;
            if (val_state.a_chunk_hases.size() != static_cast<size_t>(a_chunk_cnt)) return false;
            if (val_state.b_chunk_hases.size() != static_cast<size_t>(b_chunk_cnt)) return false;
            return true;
        }
        
        //o
        my_vector<CircularRange> get_valid_ranges(const my_vector<CircularRange> & ranges) const{
            my_vector<CircularRange> valid_ranges;
            for(const auto& r : ranges){
                if(!r.is_empty()){
                    valid_ranges.push_back(r);
                }
            }
            return valid_ranges;
        }

        //o
        //rangeの範囲が円環でしょうじゅんになっているか
        //completeされる必要はない
        bool is_valid_rangeA_unique_and_increse(const my_vector<CircularRange> &ranges_) const {
            auto ranges = get_valid_ranges(ranges_);
            CircularRange watched_range = ranges[0];
            auto set_r=[&](int r){
                int l = watched_range.get_start();
                if(l < r){
                    watched_range = CircularRange(l, r - l, state_N);
                }else{//emptyを弾いているので、l==rなら、full
                    watched_range = CircularRange(l, state_N - l + r, state_N);
                }
            };
            for (size_t i = 1; i < ranges.size(); i++)
            {
                const auto &range = ranges[i];
                if (circular_ranges_overlap_nieve(watched_range, range)) return false;
                set_r(range.get_end());
            }
            return true;
        }

        //o
        bool is_valid_rangeB_unique_dec_complete(const my_vector<CircularRange> &ranges_) const {
            auto ranges = get_valid_ranges(ranges_);
            CircularRange watched_range = ranges[0];
            for (size_t i = 1; i < ranges.size(); i++)
            {
                const auto &range = ranges[i];
                if (circular_ranges_overlap_nieve(watched_range, range)) return false;
                //completeで降順かどうか
                if (watched_range.get_start() != range.get_end()) return false;
                int past_r = watched_range.get_end();
                watched_range = CircularRange(range.get_start(), range.size() + watched_range.size(), state_N);
                if (watched_range.get_end() != past_r) return false;
            }
            if (watched_range.size() != state_N) return false;//completeか
            return true;
        }

        //o
        bool is_valid_has_range(const my_vector<my_bitset<MAX_N>>& chunk_hases, const my_vector<CircularRange>& ranges) const {
            int chunk_cnt = static_cast<int>(chunk_hases.size()); 
            if (chunk_cnt != static_cast<int>(ranges.size())) return false;
            for (size_t i = 0; i < chunk_cnt; i++)
            {
                auto& chunk_has = chunk_hases[i];
                auto& range = ranges[i];
                for(int v = 0 ; v < state_N ; v++){
                    if(chunk_has[v]){
                        if (!range.contains(v)) return false;
                    }
                }
            }
            return true;
        }

        //x
        bool is_valid_lis() const {
            int a_chunk_cnt = static_cast<int>(val_state.a_values.size());
            if (a_chunk_cnt != static_cast<int>(val_state.a_chunk_hases.size())) return false;
            for (int i = 0; i < a_chunk_cnt; i++) {
                const auto &lis = val_state.a_values[i];
                const auto &has_set = val_state.a_chunk_hases[i];
                if(lis.size() != pop_cnt(has_set, 0, state_N)){
                    return false;
                }
                for(auto&v : lis){
                    if(!has_set[v]){
                        return false;
                    }
                }
            }
            return true;
        } 

        // 責務: 状態整合性検証（サイズ/範囲/重複なし）
        //x
        void validate_basic() const {
            my_assert(is_valid_cnt());
            my_assert(is_valid_has_unique_and_complete(val_state.a_chunk_hases, val_state.b_chunk_hases));
            my_assert(is_valid_rangeA_unique_and_increse(val_state.a_ranges));
            my_assert(is_valid_rangeB_unique_dec_complete(val_state.b_ranges));
            my_assert(is_valid_has_range(val_state.a_chunk_hases, val_state.a_ranges));
            my_assert(is_valid_has_range(val_state.b_chunk_hases, val_state.b_ranges));
            my_assert(is_valid_lis());
        }
    };

    template<size_t MAX_N>
    void print_chunk_a(const my_vector<std::shared_ptr<Chunk<MAX_N>>> &final_chunks,
                       const my_vector<my_vector<int>> &vals,
                       const my_vector<CircularRange> &ranges,
                       const my_bitset<MAX_N>&is_a_lis
                       ) {
        const int chunk_cnt = static_cast<int>(final_chunks.size());
        my_assert(chunk_cnt == static_cast<int>(vals.size()));
        my_assert(chunk_cnt == static_cast<int>(ranges.size()));

        for (int i = 0; i < chunk_cnt; i++) {
            const std::string label = final_chunks[i]->label.to_string();
            const auto &range = ranges[i];
            out("chunk " + std::to_string(i));
            out("label = " + label + ", range = " + ChunkSeparator::to_string(range));

            std::stringstream ss;
            ss << "vals : ";
            for (const auto &v: vals[i]) {
                ss << v;
                if(is_a_lis[v]){
                    ss<<"(l)";
                }
                ss<< ", ";
            }
            out(ss.str());
        }
    }


    //x チャンク配列＋値＋範囲をまとめて出力
    template<size_t MAX_N>
    void print_chunk_b(const my_vector<std::shared_ptr<Chunk<MAX_N>>> &final_chunks,
                        const my_vector<my_vector<int>> &vals,
                        const my_vector<CircularRange> &ranges) {
        const int chunk_cnt = static_cast<int>(final_chunks.size());
        my_assert(chunk_cnt == static_cast<int>(vals.size()));
        my_assert(chunk_cnt == static_cast<int>(ranges.size()));

        for (int i = 0; i < chunk_cnt; i++) {
            const std::string label = final_chunks[i]->label.to_string();
            const auto &range = ranges[i];
            out("chunk " + std::to_string(i));
            out("label = " + label + ", range = " + ChunkSeparator::to_string(range));

            std::stringstream ss;
            ss << "vals : ";
            for (const auto &v: vals[i]) {
                ss << v << ", ";
            }
            out(ss.str());
        }
    }

    //x YakinamashiChunkState の内容を出力
    template<size_t MAX_N>
    void print_st(const YakinamashiChunkState<MAX_N> &chunk_st) {
        const auto &val_st = chunk_st.val_state;
        const auto &a_values = val_st.a_values;
        const auto &b_values = val_st.b_values;
        const auto &a_chunk_hases = val_st.a_chunk_hases;
        const auto &b_chunk_hases = val_st.b_chunk_hases;
        const auto &a_ranges = val_st.a_ranges;
        const auto &b_ranges = val_st.b_ranges;

        out("chunk_as");
        print_chunk_a<MAX_N>(chunk_st.tree.final_a_chunks, a_values, a_ranges, val_st.is_a_lis);
        out("");
        out("chunk_bs");
        print_chunk_b<MAX_N>(chunk_st.tree.final_b_chunks, b_values, b_ranges);
    }
}

#endif // UNTITLED_YAKINAMASHICHUNKSTATE_H
