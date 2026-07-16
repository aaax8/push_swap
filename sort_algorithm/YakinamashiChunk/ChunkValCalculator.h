// sort_algorithm/YakinamashiChunk/ChunkValCalculator.h
#ifndef UNTITLED_CHUNKVALCALCULATOR_H
#define UNTITLED_CHUNKVALCALCULATOR_H

#include "../../all_include.h"
#include "../../util.h"
#include "../../State.h"
#include "../../Legacy/lis_chank/Lis.h"
#include "../InitBeam/CircularRange.h"
#include "ChunkValState.h"

namespace YakinamashiChunk {
    using ChunkSeparator::CircularRange;
    using ChunkSeparator::add_circular;

    // Forward declarations
    template<size_t MAX_N> struct ChunkTree;

    template<size_t MAX_N>
    class ChunkValCalculator {
        private:
            int state_N;
            LISCalculator<MAX_N> lis_calc;

        public:
            //o
            explicit ChunkValCalculator(int n) : state_N(n) {
                my_assert(0 <= state_N && state_N <= MAX_N);
            }

            //x
            //全部渡す版
            ChunkValState<MAX_N> re_build_by_b_ranges(
                const ChunkValState<MAX_N> &old_st,
                my_vector<CircularRange> &new_b_ranges
            ) {
                my_assert(old_st.b_ranges.size() == new_b_ranges.size());
                
                // Step2: B側に残り値を割り当て
                my_vector<my_vector<int>> b_values;
                my_vector<my_bitset<MAX_N>> b_chunk_hases(new_b_ranges.size());
                b_values.reserve(new_b_ranges.size());
                my_bitset<MAX_N> used_in_a;
                for(auto& a_has : old_st.a_chunk_hases){
                    used_in_a |= a_has;
                }
                build_b_desc_remaining(used_in_a, new_b_ranges, b_values, b_chunk_hases);

                return ChunkValState<MAX_N>{old_st.a_values, b_values, old_st.is_a_lis, old_st.a_chunk_hases, b_chunk_hases, old_st.a_ranges,new_b_ranges};
            }

            //o A側はLIS、B側は降順で残りを割り当て
            // 戻り値: ChunkValState（値割当情報のみ）
            ChunkValState<MAX_N> build_a_lis_b_desc(
                const ChunkTree<MAX_N> &tree,
                const State &initial_state,
                const my_vector<CircularRange> &a_ranges,
                const my_vector<CircularRange> &b_ranges
            ) {
                my_assert(tree.state_N == state_N);
                my_assert(initial_state.current_N == state_N);
                my_assert(a_ranges.size() == tree.final_a_chunks.size());
                my_assert(b_ranges.size() == tree.final_b_chunks.size());

                my_vector<my_vector<int>> a_values;
                my_vector<my_bitset<MAX_N>> a_chunk_hases(a_ranges.size());
                my_bitset<MAX_N> used_in_a;
                
                a_values.reserve(a_ranges.size());
                for (size_t i = 0; i < a_ranges.size(); i++) {
                    const auto &range = a_ranges[i];
                    const auto &chunk = tree.final_a_chunks[i];
                    ChunkLabel label = chunk->label;
                    
                    // 既存のLIS計算ロジック
                    my_vector<int> lis = lis_calc.calculate_lis_in_range(
                        initial_state, range, label
                    );
#ifdef DEBUG
{
                    auto [is_lis_my_bitset, lis_swaps] = lis_calc.calculate_lis_with_swap_in_range(
                        initial_state, range, label, 0
                    );
                    my_assert(lis == lis_swaps);
                    my_assert(is_lis_my_bitset.count() == lis.size());
                }
                {
                    
                    auto [is_lis_my_bitset, lis_swaps] = lis_calc.calculate_lis_with_swap_in_range(
                        initial_state, range, label,2
                    );
                    ;
                }
#endif
                    a_values.push_back(lis);
                    
                    for (int v : lis) {
                        my_assert(0 <= v && v < state_N);
                        my_assert(!used_in_a[v]);
                        used_in_a[v] = true;
                        a_chunk_hases[i][v] = true;
                    }
                }

                // Step2: B側に残り値を割り当て
                my_vector<my_vector<int>> b_values;
                my_vector<my_bitset<MAX_N>> b_chunk_hases(b_ranges.size());
                b_values.reserve(b_ranges.size());
                
                build_b_desc_remaining(used_in_a, b_ranges, b_values, b_chunk_hases);

                return ChunkValState<MAX_N>{a_values, b_values, a_chunk_hases, b_chunk_hases, a_ranges, b_ranges};
            }

            //x B側の共通処理：非LIS値を降順でB-chunkに割り当て
            // used_in_a をもとに残り値を収集して降順で割り当て
            void build_b_desc_remaining(
                const my_bitset<MAX_N> &used_in_a,
                const my_vector<CircularRange> &b_ranges,
                my_vector<my_vector<int>> &b_values,
                my_vector<my_bitset<MAX_N>> &b_chunk_hases
            ) {
                my_assert(b_chunk_hases.size() == b_ranges.size());
                b_values.resize(b_ranges.size());

                for (size_t i = 0; i < b_ranges.size(); i++) {
                    const auto &range = b_ranges[i];
                    int v = range.get_start();
                    
                    for (int offset = 0; offset < range.size(); offset++) {
                        if (!used_in_a[v]) {
                            b_values[i].push_back(v);
                            b_chunk_hases[i][v] = true;
                        }
                        v = add_circular(v, 1, state_N);
                    }
                }
            }

            //x A側はLIS+swap込み、B側は降順で残りを割り当て
            // calculate_lis_in_rangeで基本LISを取得し、
            // その後に貪欲にswapで拡張してA側に割り当て
            ChunkValState<MAX_N> build_a_lis_with_swap_b_desc(
                const ChunkTree<MAX_N> &tree,
                const State &initial_state,
                const my_vector<CircularRange> &a_ranges,
                const my_vector<CircularRange> &b_ranges,
                int lis_start = 0
            ) {
                my_assert(tree.state_N == state_N);
                my_assert(initial_state.current_N == state_N);
                my_assert(a_ranges.size() == tree.final_a_chunks.size());
                my_assert(b_ranges.size() == tree.final_b_chunks.size());
                my_assert(a_ranges.size() == 1);

                // Step1: 各A-chunkで基本LISを計算+swap拡張
                my_vector<my_vector<int>> a_values;
                my_vector<my_bitset<MAX_N>> a_chunk_hases(a_ranges.size());
                my_bitset<MAX_N> is_a_lis;
                my_bitset<MAX_N> used_in_a;
                
                a_values.reserve(a_ranges.size());
                for (size_t i = 0; i < a_ranges.size(); i++) {
                    const auto &range = a_ranges[i];
                    const auto &chunk = tree.final_a_chunks[i];
                    ChunkLabel label = chunk->label;
                    
                    // swap込みでLISを取得
                    auto [is_lis_my_bitset, lis_swaps] = lis_calc.calculate_lis_with_swap_in_range(
                        initial_state, range, label, 1, lis_start
                    );
                    is_a_lis |= is_lis_my_bitset;
                    a_values.push_back(lis_swaps);
                    
                    for (int v : lis_swaps) {
                        my_assert(0 <= v && v < state_N);
                        my_assert(!used_in_a[v]);
                        used_in_a[v] = true;
                        a_chunk_hases[i][v] = true;
                    }
                }

                // Step2: B側に残り値を割り当て
                my_vector<my_vector<int>> b_values;
                my_vector<my_bitset<MAX_N>> b_chunk_hases(b_ranges.size());
                b_values.reserve(b_ranges.size());
                
                build_b_desc_remaining(used_in_a, b_ranges, b_values, b_chunk_hases);

                return ChunkValState<MAX_N>{a_values, b_values, is_a_lis, a_chunk_hases, b_chunk_hases, a_ranges, b_ranges};
            }
        };
}

#endif // UNTITLED_CHUNKVALCALCULATOR_H
