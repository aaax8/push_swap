#pragma once
#include "BaseDestroy.h"
#include "State/YakinamashiState.h"
#include "State/QueriesState.h"
#include "QueriesModifier.h"
#include "Query/QueryProvider.h"
#include "util.h"
#include <utility>

template<size_t MAX_N>
class FullQueryDestroyer : public BaseDestroy<MAX_N> {
    const State& current_st;

    struct CollectResult {
        my_vector<int> tar_idxs;
        my_vector<std::pair<int, int>> destroyed_vs;
    };

public:
    explicit FullQueryDestroyer(const State& current_st) : current_st(current_st) {}

    // : 重さ定義を既存 destroyer と揃えるため、各 v の ins_turn 合計を destroyed_cost に使う。
    my_vector<std::pair<int, int>> destroy(YakinamashiState<MAX_N>& yst) override {
        CollectResult collected = collect_all_targets(yst.queries_state);
        erase_tar_idxs(yst, collected.tar_idxs);
        return collected.destroyed_vs;
    }

private:
    // : full destroy でも destroyed_vs.second の意味を range destroy と一致させる。
    void add_destroyed_v_cost(
        CollectResult& result, my_vector<int>& destroyed_vs_idxs, int v, int ins_turn
    ) const {
        if (destroyed_vs_idxs[v] < 0) {
            destroyed_vs_idxs[v] = static_cast<int>(result.destroyed_vs.size());
            result.destroyed_vs.push_back({v, 0});
        }
        result.destroyed_vs[destroyed_vs_idxs[v]].second += ins_turn;
    }

    // : 初回ループでは query 全体を消すので index と cost を全件収集する。
    CollectResult collect_all_targets(const QueriesState& qs) const {
        CollectResult result;
        my_vector<int> destroyed_vs_idxs(current_st.initial_N, -1);
        for (int i = 0; i < static_cast<int>(qs.queries.size()); i++) {
            const auto& qri = qs.queries[i];
            result.tar_idxs.push_back(i);
            add_destroyed_v_cost(result, destroyed_vs_idxs, qri.get_tar_v(), qri.get_ins_turn());
        }
        return result;
    }

    // : 全削除後の再計算も既存の QueriesModifier に集約して整合を崩さない。
    void erase_tar_idxs(YakinamashiState<MAX_N>& yst, const my_vector<int>& tar_idxs) {
        QueryProvider provider(yst, current_st);
        QueriesModifier modifier(current_st, yst.query_order.aorder, yst.queries_state, provider);
        modifier.erase_queries<MAX_N>(tar_idxs);
    }
};
