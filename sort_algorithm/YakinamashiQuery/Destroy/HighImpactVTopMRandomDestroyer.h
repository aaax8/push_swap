#pragma once
#include "BaseDestroy.h"
#include "State/YakinamashiState.h"
#include "State/QueriesState.h"
#include "QueriesModifier.h"
#include "Query/QueryProvider.h"
#include "util.h"
#include <algorithm>

// : v を消したときの score 減少量を直接測り、寄与が大きい候補群から top-m random で destroy できるようにするため。
template<size_t MAX_N>
class HighImpactVTopMRandomDestroyer : public BaseDestroy<MAX_N> {
    struct VImpactInfo {
        int v;
        int decrease;
        int destroyed_cost;
        my_vector<int> tar_idxs;
    };

    const State& current_st;
    const int select_v_count;
    const int candidate_top_m;
    my_vector<int> last_selected_vs;
    my_vector<std::pair<int, int>> last_top_candidates;

public:
    explicit HighImpactVTopMRandomDestroyer(const State& current_st, int select_v_count, int candidate_top_m)
        : current_st(current_st), select_v_count(select_v_count), candidate_top_m(candidate_top_m) {}

    // : 各 v の leave-one-v-out 減少量を測って、top-m 候補からランダムに選んだ v 群を実際に削除するため。
    my_vector<std::pair<int, int>> destroy(YakinamashiState<MAX_N>& yst) override {
        validate_params();
        my_vector<VImpactInfo> impacts = build_v_impacts(yst);
        std::sort(impacts.begin(), impacts.end(), is_higher_impact);
        remember_top_candidates(impacts);
        my_vector<int> picked_idxs = pick_top_m_random_idxs(impacts);
        remember_selected_vs(impacts, picked_idxs);
        my_vector<int> merged_tar_idxs = build_merged_tar_idxs(impacts, picked_idxs);
        my_vector<std::pair<int, int>> destroyed_vs = build_destroyed_vs(impacts, picked_idxs);
        erase_tar_idxs(yst, merged_tar_idxs);
        return destroyed_vs;
    }

    // : ログから top-m random の実際の選択結果を追えるようにするため。
    int get_select_v_count() const { return select_v_count; }
    // : ログから top-m 候補集合の大きさを追えるようにするため。
    int get_candidate_top_m() const { return candidate_top_m; }
    // : 選ばれた v 群を output.txt に出せるようにするため。
    const my_vector<int>& get_last_selected_vs() const { return last_selected_vs; }
    // : 候補上位の decrease を output.txt に出せるようにするため。
    const my_vector<std::pair<int, int>>& get_last_top_candidates() const { return last_top_candidates; }

private:
    // : top-m random の前提を崩す入力を早期に弾いて、無意味な destroy を防ぐため。
    void validate_params() const {
        my_assert(1 <= select_v_count);
        my_assert(select_v_count <= candidate_top_m);
        my_assert(candidate_top_m <= current_st.current_N);
    }

    // : 各 v の query 群をまとめて持ち、後続の減少量計測と実削除で同じ情報源を使うため。
    my_vector<VImpactInfo> collect_impacts_seed(const QueriesState& qs) const {
        my_vector<VImpactInfo> infos;
        my_vector<int> info_idxs(current_st.initial_N, -1);
        for (int i = 0; i < (int)qs.queries.size(); i++) append_query_idx(qs.queries[i], i, info_idxs, infos);
        return infos;
    }

    // : tar_v ごとの query 群と destroyed_cost を一箇所で集約して、測定と実削除を一致させるため。
    void append_query_idx(
        const QueryCommonRI& qri, int q_idx, my_vector<int>& info_idxs, my_vector<VImpactInfo>& infos
    ) const {
        int v = qri.get_tar_v();
        if (info_idxs[v] < 0) {
            info_idxs[v] = (int)infos.size();
            infos.push_back({v, 0, 0, my_vector<int>()});
        }
        VImpactInfo& info = infos[info_idxs[v]];
        info.destroyed_cost += qri.get_ins_turn();
        info.tar_idxs.push_back(q_idx);
    }

    // : v 単位の score 減少量を元の同じ snapshot から測って、候補順位を作るため。
    my_vector<VImpactInfo> build_v_impacts(const YakinamashiState<MAX_N>& yst) const {
        my_vector<VImpactInfo> infos = collect_impacts_seed(yst.queries_state);
        int before_score = yst.queries_state.sum_turn;
        for (int i = 0; i < (int)infos.size(); i++) infos[i].decrease = measure_decrease(yst, before_score, infos[i].tar_idxs);
        return infos;
    }

    // : 各 v を単独で消したときの減少量を直接測って、近似ではなく現状態での寄与を使うため。
    int measure_decrease(const YakinamashiState<MAX_N>& yst, int before_score, const my_vector<int>& tar_idxs) const {
        YakinamashiState<MAX_N> copied = yst;
        erase_tar_idxs(copied, tar_idxs);
        return before_score - copied.queries_state.sum_turn;
    }

    // : top-m random の候補順位を安定させるため、減少量優先・同率時に destroyed_cost を使うため。
    static bool is_higher_impact(const VImpactInfo& lhs, const VImpactInfo& rhs) {
        if (lhs.decrease != rhs.decrease) return lhs.decrease > rhs.decrease;
        if (lhs.destroyed_cost != rhs.destroyed_cost) return lhs.destroyed_cost > rhs.destroyed_cost;
        return lhs.v < rhs.v;
    }

    // : top-m 候補群の中から without replacement で k 個選び、完全貪欲で止まらないようにするため。
    my_vector<int> pick_top_m_random_idxs(const my_vector<VImpactInfo>& impacts) const {
        int top_count = std::min(candidate_top_m, (int)impacts.size());
        int picked_count = std::min(select_v_count, top_count);
        my_vector<int> pool(top_count);
        my_vector<int> picked_idxs;
        for (int i = 0; i < top_count; i++) pool[i] = i;
        for (int i = 0; i < picked_count; i++) erase_and_pick_one(pool, picked_idxs);
        return picked_idxs;
    }

    // : top-m random を without replacement で実装し、同じ v を二重に選ばないようにするため。
    void erase_and_pick_one(my_vector<int>& pool, my_vector<int>& picked_idxs) const {
        int picked_pos = my_rand((int)pool.size());
        picked_idxs.push_back(pool[picked_pos]);
        pool.erase(pool.begin() + picked_pos);
    }

    // : 実削除時に query idx が昇順で並ぶようにして、既存 erase 実装の前提と揃えるため。
    my_vector<int> build_merged_tar_idxs(const my_vector<VImpactInfo>& impacts, const my_vector<int>& picked_idxs) const {
        my_vector<int> merged_tar_idxs;
        for (int i = 0; i < (int)picked_idxs.size(); i++) append_all(merged_tar_idxs, impacts[picked_idxs[i]].tar_idxs);
        std::sort(merged_tar_idxs.begin(), merged_tar_idxs.end());
        return merged_tar_idxs;
    }

    // : 選ばれた v 群に属する query idx をまとめて、最終 erase の入力を作るため。
    void append_all(my_vector<int>& dst, const my_vector<int>& src) const {
        for (int i = 0; i < (int)src.size(); i++) dst.push_back(src[i]);
    }

    // : constructor 側が期待する destroyed_vs 形式へ戻して、既存 construct をそのまま使うため。
    my_vector<std::pair<int, int>> build_destroyed_vs(
        const my_vector<VImpactInfo>& impacts, const my_vector<int>& picked_idxs
    ) const {
        my_vector<std::pair<int, int>> destroyed_vs;
        for (int i = 0; i < (int)picked_idxs.size(); i++) append_destroyed_v(destroyed_vs, impacts[picked_idxs[i]]);
        return destroyed_vs;
    }

    // : 選ばれた v の destroyed_cost を既存 destroyer と同じ意味で返すため。
    void append_destroyed_v(my_vector<std::pair<int, int>>& destroyed_vs, const VImpactInfo& info) const {
        destroyed_vs.push_back({info.v, info.destroyed_cost});
    }

    // : top-m 候補をログへ残し、どの decrease 上位から引いたか後で確認できるようにするため。
    void remember_top_candidates(const my_vector<VImpactInfo>& impacts) {
        last_top_candidates.clear();
        int top_count = std::min(candidate_top_m, (int)impacts.size());
        for (int i = 0; i < top_count; i++) last_top_candidates.push_back({impacts[i].v, impacts[i].decrease});
    }

    // : 実際に選ばれた v 群をログへ残し、top-m random の結果を output.txt から読めるようにするため。
    void remember_selected_vs(const my_vector<VImpactInfo>& impacts, const my_vector<int>& picked_idxs) {
        last_selected_vs.clear();
        for (int i = 0; i < (int)picked_idxs.size(); i++) last_selected_vs.push_back(impacts[picked_idxs[i]].v);
    }

    // : 単独評価と本番削除で同じ erase 実装を使い、destroy 条件の解釈差をなくすため。
    void erase_tar_idxs(YakinamashiState<MAX_N>& yst, const my_vector<int>& tar_idxs) const {
        QueryProvider provider(yst, current_st);
        QueriesModifier modifier(current_st, yst.query_order.aorder, yst.queries_state, provider);
        modifier.erase_queries<MAX_N>(tar_idxs);
    }
};
