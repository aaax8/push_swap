#pragma once
#include "BaseDestroy.h"
#include "State/YakinamashiState.h"
#include "State/QueriesState.h"
#include "QueriesModifier.h"
#include "Query/QueryProvider.h"
#include "util.h"

template<size_t MAX_N>
class RandomTarVRangeDestroyer : public BaseDestroy<MAX_N> {
    const State& current_st;
    const int target_v_range_size;
    int last_start_v;
    int last_end_v_exclusive;

    struct CollectResult {
        my_vector<int> tar_idxs;
        my_vector<std::pair<int, int>> destroyed_vs;
    };
public:
    explicit RandomTarVRangeDestroyer(const State& current_st, int target_v_range_size)
        : current_st(current_st), target_v_range_size(target_v_range_size),
          last_start_v(-1), last_end_v_exclusive(-1) {}

    my_vector<std::pair<int, int>> destroy(YakinamashiState<MAX_N>& yst) override {
        my_assert(1 <= target_v_range_size && target_v_range_size <= current_st.initial_N);
        last_start_v = my_rand(current_st.initial_N);
        last_end_v_exclusive = (last_start_v + target_v_range_size) % current_st.initial_N;
        CollectResult collected = collect_targets(yst.queries_state);
        erase_tar_idxs(yst, collected.tar_idxs);
        return collected.destroyed_vs;
    }

    int get_target_v_range_size() const { return target_v_range_size; }
    int get_last_start_v() const { return last_start_v; }
    int get_last_end_v_exclusive() const { return last_end_v_exclusive; }

private:
    bool is_in_target_range(int v) const {
        int raw_end = last_start_v + target_v_range_size;
        if (raw_end <= current_st.initial_N) return last_start_v <= v && v < raw_end;
        return last_start_v <= v || v < last_end_v_exclusive;
    }

    void add_destroyed_v_cost(
        CollectResult& result, my_vector<int>& destroyed_vs_idxs, int v, int ins_turn
    ) const {
        if (destroyed_vs_idxs[v] < 0) {
            destroyed_vs_idxs[v] = static_cast<int>(result.destroyed_vs.size());
            result.destroyed_vs.push_back({v, 0});
        }
        result.destroyed_vs[destroyed_vs_idxs[v]].second += ins_turn;
    }

    CollectResult collect_targets(const QueriesState& qs) const {
        CollectResult result;
        my_vector<int> destroyed_vs_idxs(current_st.initial_N, -1);
        for (int i = 0; i < (int)qs.queries.size(); i++) {
            const auto& qri = qs.queries[i];
            int v = qri.get_tar_v();
            if (!is_in_target_range(v)) continue;
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
