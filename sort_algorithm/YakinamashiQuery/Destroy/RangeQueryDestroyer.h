#pragma once
#include "BaseDestroy.h"
#include "State/YakinamashiState.h"
#include "State/QueriesState.h"
#include "QueriesModifier.h"
#include "Query/QueryProvider.h"
#include "util.h"
#include <cmath>
#include <utility>

template<size_t MAX_N>
class RangeQueryDestroyer : public BaseDestroy<MAX_N> {
    const State& current_st;
    const int range_size_override;

    struct CollectResult {
        my_vector<int> tar_idxs;
        my_vector<std::pair<int, int>> destroyed_vs;
    };
public:
    explicit RangeQueryDestroyer(const State& current_st, int range_size_override = -1)
        : current_st(current_st), range_size_override(range_size_override) {}

    // 削除query群の重みを後段で使うため、v別のins_turn合計も返す。
    // : ALNS arm を concrete destroyer から組む時にも size を同じ source of truth から参照できるようにする。
    int get_range_size_override() const { return range_size_override; }

    my_vector<std::pair<int, int>> destroy(YakinamashiState<MAX_N>& yst) override {
        const my_bitset<MAX_N> is_target_v = pick_target_vs(yst.queries_state);
        CollectResult collected = collect_targets(yst.queries_state, is_target_v);
        erase_tar_idxs(yst, collected.tar_idxs);
        return collected.destroyed_vs;
    }

private:
    my_bitset<MAX_N> pick_target_vs(const QueriesState& qs) const {
        const int query_count = (int)qs.queries.size();
        const int range_size = calc_range_size(query_count);
        const int range_start = my_rand(query_count - range_size + 1);
        my_bitset<MAX_N> is_target_v;
        for (int i = range_start; i < range_start + range_size; i++) {
            is_target_v.set(qs.queries[i].get_tar_v());
        }
        return is_target_v;
    }

    // : 明示指定があるときだけそれを優先し、既存の sqrt ベース挙動は既定値のまま残すため。
    int calc_range_size(int query_count) const {
        if (range_size_override > 0) return std::min(query_count, range_size_override);
        return std::max(1, (int)std::sqrt((double)query_count));
    }

    // 同じvのqueryが複数削除されるので、costを集約する。
    void add_destroyed_v_cost(
        CollectResult& result, my_vector<int>& destroyed_vs_idxs, int v, int ins_turn
    ) const {
        if (destroyed_vs_idxs[v] < 0) {
            destroyed_vs_idxs[v] = static_cast<int>(result.destroyed_vs.size());
            result.destroyed_vs.push_back({v, 0});
        }
        result.destroyed_vs[destroyed_vs_idxs[v]].second += ins_turn;
    }

    // erase対象idxと、vごとの削除ins_turn合計を同時に集めてSingle Source of Truthを保つ。
    CollectResult collect_targets(const QueriesState& qs, const my_bitset<MAX_N>& is_target_v) const {
        const int query_count = (int)qs.queries.size();
        CollectResult result;
        my_vector<int> destroyed_vs_idxs(current_st.initial_N, -1);
        for (int i = 0; i < query_count; i++) {
            const auto& qri = qs.queries[i];
            int v = qri.get_tar_v();
            if (!is_target_v[v]) continue;
            result.tar_idxs.push_back(i);
            add_destroyed_v_cost(result, destroyed_vs_idxs, v, qri.get_ins_turn());
        }
        return result;
    }

    void erase_tar_idxs(YakinamashiState<MAX_N>& yst, const my_vector<int>& tar_idxs) {
        QueryProvider provider(yst, current_st);
        QueriesModifier modifier(current_st, yst.query_order.aorder, yst.queries_state, provider);
        modifier.erase_queries<MAX_N>(tar_idxs);
    }
};
