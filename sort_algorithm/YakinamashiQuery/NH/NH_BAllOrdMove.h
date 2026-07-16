#pragma once

#include "State.h"
#include "State/YakinamashiState.h"
#include "Construction/EraseQueriesReBuilder.h"
#include "util.h"

// 責務: 指定された v 群の BOrder TARGET entry を 1 つランダムに円環移動し、query を現在 order に合わせて再構築した候補を作る。
// 必要な理由: ALNS の destroy/construct 後に B 側 AllOrd 順序を局所的に改善するため。
template<size_t MAX_N>
class NH_BAllOrdMove {
public:
    static YakinamashiState<MAX_N> build_candidate(
            const State& init_st,
            const YakinamashiState<MAX_N>& current_yst,
            const my_vector<int>& target_vs
    );

private:
    static constexpr int MIN_ABS_DELTA = 1;
    static constexpr int MAX_ABS_DELTA = 6;

    static int pick_delta();
    static my_vector<int> build_target_entry_indices(
            const YakinamashiState<MAX_N>& candidate_yst,
            const my_vector<int>& target_vs
    );
    static int to_insert_idx(int from_idx, int final_idx);
    static void validate_moved_index(
            const YakinamashiState<MAX_N>& candidate_yst,
            int v,
            int expected_final_idx
    );
};

// 責務: current_yst をコピーし、target_vs 由来の BOrder TARGET entry を ±1..±6 の円環距離で移動して query を rebuild する。
// 必要な理由: ALNS 側が候補生成の詳細を持たずに、今回 construct で扱った v に限定した BOrder 近傍を試せるようにするため。
template<size_t MAX_N>
YakinamashiState<MAX_N> NH_BAllOrdMove<MAX_N>::build_candidate(
        const State& init_st,
        const YakinamashiState<MAX_N>& current_yst,
        const my_vector<int>& target_vs
) {
    YakinamashiState<MAX_N> candidate_yst = current_yst;
    const auto& entries = candidate_yst.query_order.border.get_order_entries();
    const int entry_size = static_cast<int>(entries.size());
    if (entry_size <= 1) return candidate_yst;

    const my_vector<int> target_entry_indices = build_target_entry_indices(candidate_yst, target_vs);
    if (target_entry_indices.empty()) return candidate_yst;
    const int from_idx = target_entry_indices[my_rand(static_cast<int>(target_entry_indices.size()))];
    const int delta = pick_delta();
    const int final_idx = mod(from_idx + delta, entry_size);
    if (final_idx == from_idx) return candidate_yst;

    const int v = entries[from_idx].value;
    const int insert_idx = to_insert_idx(from_idx, final_idx);
    candidate_yst.query_order.border.update_insert_ord_v(v, AllOrd(insert_idx));
    validate_moved_index(candidate_yst, v, final_idx);

    State candidate_st = init_st;
    EraseQueriesReBuilder<MAX_N>::rebuild_by_current_order(candidate_st, candidate_yst);
    return candidate_yst;
}

// 責務: target_vs に含まれる v のうち、candidate_yst の BOrder TARGET entry として存在する entry index を返す。
// 必要な理由: A2A-only など BOrder TARGET entry を持たない v を避けつつ、BMove 対象を今回の construct 対象へ限定するため。
template<size_t MAX_N>
my_vector<int> NH_BAllOrdMove<MAX_N>::build_target_entry_indices(
        const YakinamashiState<MAX_N>& candidate_yst,
        const my_vector<int>& target_vs
) {
    my_vector<int> target_entry_indices;
    const auto& entries = candidate_yst.query_order.border.get_order_entries();
    for (int entry_idx = 0; entry_idx < static_cast<int>(entries.size()); entry_idx++) {
        if (entries[entry_idx].val_state != ValState::TARGET) continue;
        for (int target_v : target_vs) {
            if (entries[entry_idx].value == target_v) {
                target_entry_indices.push_back(entry_idx);
                break;
            }
        }
    }
    return target_entry_indices;
}

// 責務: ±1..±6 の delta をランダムに返す。
// 必要な理由: 近傍の移動量仕様を NH_BAllOrdMove 内に閉じ込めるため。
template<size_t MAX_N>
int NH_BAllOrdMove<MAX_N>::pick_delta() {
    const int abs_delta = my_rand(MIN_ABS_DELTA, MAX_ABS_DELTA + 1);
    const int sign = (my_rand(2) == 0) ? -1 : 1;
    return sign * abs_delta;
}

// 責務: 外側の final_idx 指定を update_insert_ord_v に渡せる insert_idx に変換する。
// 必要な理由: update_insert_ord_v が既存 entry の移動時に指定位置の直前へ入れる仕様なので、移動量の 1 ずれを防ぐため。
template<size_t MAX_N>
int NH_BAllOrdMove<MAX_N>::to_insert_idx(int from_idx, int final_idx) {
    if (from_idx < final_idx) return final_idx + 1;
    return final_idx;
}

// 責務: 移動対象 v の BOrder target index が期待した final_idx と一致することを確認する。
// 必要な理由: update_insert_ord_v の直前挿入仕様による 1 ずれを、近傍生成直後に検出するため。
template<size_t MAX_N>
void NH_BAllOrdMove<MAX_N>::validate_moved_index(
        const YakinamashiState<MAX_N>& candidate_yst,
        int v,
        int expected_final_idx
) {
#ifdef DEBUG
    const int actual_final_idx = candidate_yst.query_order.border.get_target_order(v).get();
    my_assert(actual_final_idx == expected_final_idx);
#else
    (void)candidate_yst;
    (void)v;
    (void)expected_final_idx;
#endif
}
