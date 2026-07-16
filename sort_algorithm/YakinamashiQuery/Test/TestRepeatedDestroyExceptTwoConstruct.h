// TestRepeatedDestroyExceptTwoConstruct.h
#pragma once

#include "all_include.h"
#include "State.h"
#include "YstFactory.h"
#include "YakinamashiState.h"
#include "Construction/GreedyQueriesConstructor.h"
#include "Validation/YakinamashiStateValidator.h"
#include "Command/YstCommandsBuilder.h"
#include "QueriesModifier.h"
#include "Query/QueryProvider.h"
#include <sstream>

template<size_t MAX_N>
class ExceptTwoQueryDestroyer {
    const State& init_st;

public:
    // : 残す v 2 種類と消す対象群を 1 つの返り値にまとめて、判定とログ出力の基準をずらさないため。
    struct DestroyPlan {
        my_vector<int> tar_idxs;
        my_vector<std::pair<int, int>> destroyed_vs;
        int keep_v1;
        int keep_cost1;
        int keep_v2;
        int keep_cost2;
    };

    // : empty を避ける暫定仕様として、最小コストの v 2 種類を残す destroyer をテスト専用で分離するため。
    explicit ExceptTwoQueryDestroyer(const State& init_st) : init_st(init_st) {}

    // : 破壊結果の keep_v1/2 をそのままテストログへ流せるように、erase 後も plan を返す。
    DestroyPlan destroy(YakinamashiState& yst) {
        DestroyPlan plan = collect_destroy_plan(yst.queries_state);
        erase_tar_idxs(yst, plan.tar_idxs);
        return plan;
    }

private:
    struct KeepVs {
        int v1;
        int v2;
    };

    // : 残す単位は query 個数でなく tar_v の種類なので、v 集約後に 2 種類選ばないと基準がぶれるため。
    DestroyPlan collect_destroy_plan(const QueriesState& qs) const {
        my_assert(!qs.queries.empty());
        my_vector<int> cost_sum(init_st.initial_N, 0);
        my_vector<int> present_vs;
        accumulate_costs(qs, cost_sum, present_vs);
        my_assert(2 <= present_vs.size());
        KeepVs keep_vs = pick_keep_vs(cost_sum, present_vs);
        return build_destroy_plan(qs, cost_sum, keep_vs);
    }

    // : 残す 2 種類の v は「その v に属する ins_turn 合計」で決めるため、先に全 query を集約する。
    void accumulate_costs(
        const QueriesState& qs, my_vector<int>& cost_sum, my_vector<int>& present_vs
    ) const {
        my_vector<int> seen(init_st.initial_N, 0);
        for (const auto& qri : qs.queries) {
            int tar_v = qri.get_tar_v();
            cost_sum[tar_v] += qri.get_ins_turn();
            if (seen[tar_v] == 0) {
                seen[tar_v] = 1;
                present_vs.push_back(tar_v);
            }
        }
    }

    // : tie 時も毎回同じ 2 種類を残せるように、最小コスト優先・同値なら小さい v を選ぶ。
    KeepVs pick_keep_vs(const my_vector<int>& cost_sum, const my_vector<int>& present_vs) const {
        int best1 = present_vs[0];
        int best2 = present_vs[1];
        sort_two_vs(cost_sum, best1, best2);
        for (int i = 2; i < static_cast<int>(present_vs.size()); i++) {
            consider_keep_v(cost_sum, present_vs[i], best1, best2);
        }
        sort_two_vs(cost_sum, best1, best2);
        return {best1, best2};
    }

    // : 残す 2 種類以外だけを erase 対象へ落として、construct 側へ戻す destroyed_vs も同じ基準で作る。
    DestroyPlan build_destroy_plan(
        const QueriesState& qs, const my_vector<int>& cost_sum, const KeepVs& keep_vs
    ) const {
        DestroyPlan plan{{}, {}, keep_vs.v1, cost_sum[keep_vs.v1], keep_vs.v2, cost_sum[keep_vs.v2]};
        my_vector<int> destroyed_vs_idxs(init_st.initial_N, -1);
        for (int i = 0; i < static_cast<int>(qs.queries.size()); i++) {
            const auto& qri = qs.queries[i];
            if (is_keep_v(qri.get_tar_v(), keep_vs)) continue;
            plan.tar_idxs.push_back(i);
            add_destroyed_v_cost(plan, destroyed_vs_idxs, qri.get_tar_v(), qri.get_ins_turn());
        }
        return plan;
    }

    // : 2 種類選抜の比較条件を 1 箇所に寄せて、コスト優先・同値なら小さい v の規則をずらさないため。
    bool is_better_keep_v(const my_vector<int>& cost_sum, int cand_v, int cur_v) const {
        if (cost_sum[cand_v] != cost_sum[cur_v]) return cost_sum[cand_v] < cost_sum[cur_v];
        return cand_v < cur_v;
    }

    // : best1/best2 の順序を正規化しておくと、走査中の更新条件を単純化できるため。
    void sort_two_vs(const my_vector<int>& cost_sum, int& best1, int& best2) const {
        if (is_better_keep_v(cost_sum, best2, best1)) std::swap(best1, best2);
    }

    // : 2 種類の最小候補を O(種類数) で更新して、不要な sort や一時配列を増やさないため。
    void consider_keep_v(const my_vector<int>& cost_sum, int cand_v, int& best1, int& best2) const {
        if (is_better_keep_v(cost_sum, cand_v, best1)) {
            best2 = best1;
            best1 = cand_v;
            return;
        }
        if (is_better_keep_v(cost_sum, cand_v, best2)) best2 = cand_v;
    }

    // : 残す単位が tar_v の種類であり、query 数ではないことを条件式に明示するため。
    bool is_keep_v(int tar_v, const KeepVs& keep_vs) const {
        return tar_v == keep_vs.v1 || tar_v == keep_vs.v2;
    }

    // : constructor へ渡す destroyed_vs.second を既存 destroyer と同じ意味に揃えるため。
    void add_destroyed_v_cost(
        DestroyPlan& plan, my_vector<int>& destroyed_vs_idxs, int v, int ins_turn
    ) const {
        if (destroyed_vs_idxs[v] < 0) {
            destroyed_vs_idxs[v] = static_cast<int>(plan.destroyed_vs.size());
            plan.destroyed_vs.push_back({v, 0});
        }
        plan.destroyed_vs[destroyed_vs_idxs[v]].second += ins_turn;
    }

    // : query 再配置と final_rot 再計算は既存の QueriesModifier に集約して、暫定テストで知識を複製しないため。
    void erase_tar_idxs(YakinamashiState& yst, const my_vector<int>& tar_idxs) {
        QueryProvider provider(yst, init_st);
        QueriesModifier modifier(init_st, yst.query_order.aorder, yst.queries_state, provider);
        modifier.erase_queries<MAX_N>(tar_idxs);
    }
};

namespace {
    // : 初期状態ログを既存テストと同じ見方に揃えて比較しやすくするため。
    std::string except_two_state_to_string(const RingBuffer500& stack) {
        std::stringstream ss;
        for (int i = 0; i < stack.size(); i++) {
            if (i != 0) ss << ", ";
            ss << stack[i];
        }
        return ss.str();
    }

    // : ランダムケースでも既存の my_rand を使い、他テストと再現性の前提を合わせるため。
    std::vector<int> build_except_two_test_perm(int n) {
        std::vector<int> perm(n);
        std::iota(perm.begin(), perm.end(), 0);
        for (int i = n - 1; i > 0; i--) {
            int j = my_rand(i + 1);
            std::swap(perm[i], perm[j]);
        }
        return perm;
    }

    // : score の定義を既存反復テストと合わせて、差分だけ比較できるようにするため。
    int get_except_two_test_score(const QueriesState& queries_state) {
        return queries_state.sum_turn;
    }

    // : iterate 後の query 変化を人手で追えるように、復元コマンド列を 1 行へ整形するため。
    std::string except_two_cmds_to_string(const my_vector<command>& cmds) {
        std::stringstream ss;
        for (int i = 0; i < static_cast<int>(cmds.size()); i++) {
            if (i != 0) ss << ", ";
            ss << cmd_str[(int)cmds[i]];
        }
        return ss.str();
    }

    // : score だけでなく実際の復元コマンド列も比較できるようにするため。
    my_vector<command> build_except_two_reconstructed_cmds(
        const State& init_st, const QueriesState& queries_state
    ) {
        return YstCommandsBuilder::build_all_commands_from_state(init_st, queries_state);
    }

    struct QueryTypeStats {
        int a2b_cnt;
        int b2a_cnt;
        int a2a_cnt;
        int block_cnt;
    };

    // : query 列の弱さを type の細かさから見たいので、A2B/B2A/A2A 件数と連続 block 数を同時に出せるようにするため。
    QueryTypeStats calc_except_two_query_type_stats(const QueriesState& queries_state) {
        QueryTypeStats stats{0, 0, 0, 0};
        CommonPushType prev_type = CommonPushType::A2B;
        bool has_prev = false;
        for (const auto& qri : queries_state.queries) {
            CommonPushType cur_type = qri.get_query().get_push_type();
            if (cur_type == CommonPushType::A2B) stats.a2b_cnt++;
            if (cur_type == CommonPushType::B2A) stats.b2a_cnt++;
            if (qri.is_a2a_type()) stats.a2a_cnt++;
            if (!has_prev || cur_type != prev_type) stats.block_cnt++;
            prev_type = cur_type;
            has_prev = true;
        }
        return stats;
    }

    // : 長い query 列でも段階ごとの差を追えるように、段階名付きで index と中身をそのまま output.txt に吐くため。
    void print_except_two_queries_log(const std::string& label, const QueriesState& queries_state) {
        QueryTypeStats stats = calc_except_two_query_type_stats(queries_state);
        out(label + "_query_count=" + std::to_string(queries_state.queries.size()));
        out(label + "_query_type_stats="
            + "a2b=" + std::to_string(stats.a2b_cnt)
            + " b2a=" + std::to_string(stats.b2a_cnt)
            + " a2a=" + std::to_string(stats.a2a_cnt)
            + " blocks=" + std::to_string(stats.block_cnt));
        out(label + "_queries_begin");
        for (int i = 0; i < static_cast<int>(queries_state.queries.size()); i++) {
            std::stringstream ss;
            ss << label << "_q[" << i << "]=" << queries_state.queries[i];
            out(ss.str());
        }
        out(label + "_queries_end");
    }

    // : keep_v1/2 の選択結果を各 iterate の diff と同列で見えるようにして、残した 2 種類の影響を追いやすくするため。
    void print_except_two_score_log(
        int iter,
        int keep_v1,
        int keep_cost1,
        int keep_v2,
        int keep_cost2,
        int destroyed_cnt,
        int before_score,
        int after_destroy_score,
        int after_construct_score,
        const my_vector<command>& cmds
    ) {
        std::stringstream ss;
        ss << "iter=" << iter
           << " mode=except_two"
           << " keep_v1=" << keep_v1
           << " keep_cost1=" << keep_cost1
           << " keep_v2=" << keep_v2
           << " keep_cost2=" << keep_cost2
           << " destroyed_vs=" << destroyed_cnt
           << " before=" << before_score
           << " after_destroy=" << after_destroy_score
           << " after_construct=" << after_construct_score
           << " destroy_diff=" << (after_destroy_score - before_score)
           << " construct_diff=" << (after_construct_score - after_destroy_score)
           << " total_diff=" << (after_construct_score - before_score);
        out(ss.str());
        out(std::string("iter_cmds=") + except_two_cmds_to_string(cmds));
    }

    // : 「2種類残して他を全破壊」の 1 iterate を独立させて、destroy 方針だけを差し替えやすくするため。
    template<size_t MAX_N, int HABA, int K>
    void run_except_two_destroy_construct_once(
        const State& init_st,
        YakinamashiState& yst,
        ExceptTwoQueryDestroyer<MAX_N>& destroyer,
        GreedyQueriesConstructor<MAX_N>& constructor,
        int iter
    ) {
        int before_score = get_except_two_test_score(yst.queries_state);
        print_except_two_queries_log("iter" + std::to_string(iter) + "_before", yst.queries_state);
        auto plan = destroyer.destroy(yst);
        int after_destroy_score = get_except_two_test_score(yst.queries_state);
        print_except_two_queries_log("iter" + std::to_string(iter) + "_after_destroy", yst.queries_state);
        constructor.construct(yst, plan.destroyed_vs);
        QueriesStateValidator<MAX_N>::validate_state(init_st, yst.queries_state);
        int after_construct_score = get_except_two_test_score(yst.queries_state);
        print_except_two_queries_log("iter" + std::to_string(iter) + "_after_construct", yst.queries_state);
        auto cmds = build_except_two_reconstructed_cmds(init_st, yst.queries_state);
        print_except_two_score_log(
            iter, plan.keep_v1, plan.keep_cost1, plan.keep_v2, plan.keep_cost2,
            static_cast<int>(plan.destroyed_vs.size()), before_score, after_destroy_score, after_construct_score, cmds
        );
    }

    // : 既存 full destroy テストと同じ粒度で比較できるように、初期ログと iterate ログの形を揃えるため。
    template<size_t MAX_N, int HABA, int K>
    void run_repeated_destroy_except_two_case(const State& init_st, int repeat_n) {
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState yst = factory.build(init_st);
        ExceptTwoQueryDestroyer<MAX_N> destroyer(init_st);
        GreedyConstructorParams params{10, 24, 24, 24, 24, 1 << 30, 25};
        GreedyQueriesConstructor<MAX_N> constructor(init_st, params);
        auto initial_cmds = build_except_two_reconstructed_cmds(init_st, yst.queries_state);
        out(std::string("initial_state=") + except_two_state_to_string(init_st.A));
        out(std::string("initial_score=") + std::to_string(get_except_two_test_score(yst.queries_state)));
        out(std::string("initial_cmds=") + except_two_cmds_to_string(initial_cmds));
        print_except_two_queries_log("initial", yst.queries_state);
        for (int iter = 0; iter < repeat_n; iter++) {
            run_except_two_destroy_construct_once<MAX_N, HABA, K>(init_st, yst, destroyer, constructor, iter);
        }
    }
}

// : empty を踏まない暫定比較用に、最小コストの v 2 種類を残す反復テストを full destroy とは別名で追加するため。
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_repeated_destroy_except_two_construct() {
    constexpr int TEST_N = 100;
    constexpr int REPEAT_N = 50;
    std::vector<int> perm = build_except_two_test_perm(TEST_N);
    State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
    run_repeated_destroy_except_two_case<MAX_N, HABA, K>(init_state, REPEAT_N);
}
