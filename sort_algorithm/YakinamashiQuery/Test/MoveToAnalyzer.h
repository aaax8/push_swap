// Test/MoveToAnalyzer.h
// 焼きなまし終了後に move_to 近傍の残余改善量を調べる分析ツール
#pragma once

#include "QueriesModifier.h"
#include "QueryProvider.h"
#include "QueriesState.h"
#include "YakinamashiState.h"
#include "State.h"
#include "Validation/QueriesModifierArgValidator.h"
#include "util.h"
#include <vector>
#include <string>
#include <limits>
#include <algorithm>

template<size_t MAX_N>
class MoveToAnalyzer {
public:
    struct Info {
        int target_idx;
        int tar_ins_turn;
        int tar_weight;
        int score_diff;
    };

    static void analyze(YakinamashiState<MAX_N> &yst, const State &init_st) {
        const QueriesState qs_snapshot = yst.queries_state;
        QueryProvider provider(yst, init_st);
        // _BEGIN: test 経路でも query mutation 後 validator を共有する。
        QueriesModifier modifier(init_st, yst.query_order.aorder, yst.queries_state, provider);
        // _END

        int q_size = static_cast<int>(qs_snapshot.queries.size());
        std::vector<int> tar_weights = compute_tar_weights(yst, qs_snapshot, modifier);

        std::vector<std::vector<Info>> from_results(q_size);
        std::vector<std::vector<Info>> to_results(q_size + 1);
        int total_valid = 0, total_improvable = 0;
        int best_from = -1, best_to = -1, best_diff = std::numeric_limits<int>::max();

        collect_all_results(yst, qs_snapshot, modifier, tar_weights,
                            from_results, to_results,
                            total_valid, total_improvable,
                            best_from, best_to, best_diff);

        print_queries(qs_snapshot.queries);
        print_from_analysis(qs_snapshot, from_results, tar_weights);
        print_to_analysis(qs_snapshot, to_results, tar_weights, q_size);
        print_summary(qs_snapshot.sum_turn, total_valid, total_improvable,
                      best_from, best_to, best_diff);
    }

private:
    // qi を取り除いたとき sum_turn が何減るかの近似値
    static std::vector<int> compute_tar_weights(
            YakinamashiState<MAX_N> &yst,
            const QueriesState &qs_snapshot,
            QueriesModifier &modifier) {
        int q_size = static_cast<int>(qs_snapshot.queries.size());
        std::vector<int> tar_weights(q_size + 1, 0);
        for (int qi = 0; qi < q_size - 1; qi++) {
            int g = qs_snapshot.queries[qi].get_ins_turn()
                  + qs_snapshot.queries[qi + 1].get_ins_turn();
            modifier.upd_swap(qi, qi + 1);
            int f = yst.queries_state.queries[qi].get_ins_turn();
            yst.queries_state = qs_snapshot;
            tar_weights[qi] = g - f;
        }
        if (q_size >= 2) {
            int last = q_size - 1;
            int g = qs_snapshot.queries[last].get_ins_turn()
                  + qs_snapshot.final_rot_info.second;
            modifier.upd_swap(last - 1, last);
            int f = yst.queries_state.final_rot_info.second;
            yst.queries_state = qs_snapshot;
            tar_weights[last] = g - f;
        }
        return tar_weights;
    }

    static void collect_all_results(
            YakinamashiState<MAX_N> &yst,
            const QueriesState &qs_snapshot,
            QueriesModifier &modifier,
            const std::vector<int> &tar_weights,
            std::vector<std::vector<Info>> &from_results,
            std::vector<std::vector<Info>> &to_results,
            int &total_valid, int &total_improvable,
            int &best_from, int &best_to, int &best_diff) {
        int q_size = static_cast<int>(qs_snapshot.queries.size());
        int prev_sum = qs_snapshot.sum_turn;
        for (int fi = 0; fi < q_size; fi++) {
            for (int ti = 0; ti <= q_size; ti++) {
                if (fi == ti || fi + 1 == ti) continue;
                if (!is_valid_move(qs_snapshot.queries, fi, ti, qs_snapshot.current_N)) continue;
                total_valid++;
                modifier.upd_move_to(fi, ti);
                int diff = yst.queries_state.sum_turn - prev_sum;
                yst.queries_state = qs_snapshot;
                if (diff >= 0) continue;
                total_improvable++;
                from_results[fi].push_back({ti, get_ins_turn_at(qs_snapshot, ti), tar_weights[ti], diff});
                to_results[ti].push_back({fi, qs_snapshot.queries[fi].get_ins_turn(), tar_weights[fi], diff});
                if (diff < best_diff) {
                    best_diff = diff; best_from = fi; best_to = ti;
                }
            }
        }
    }

    static int get_ins_turn_at(const QueriesState &qs, int ti) {
        int q_size = static_cast<int>(qs.queries.size());
        // ti == q_size は末尾挿入 → final_rot のコストで代替
        return (ti < q_size) ? qs.queries[ti].get_ins_turn() : qs.final_rot_info.second;
    }

    static bool is_valid_move(const QRIList &queries, int fi, int ti, int current_N) {
        int q_size = static_cast<int>(queries.size());
        if (fi < ti) {
            if (!QueriesModifierArgValidator::is_move_down_arg_valid(queries, fi, ti)) return false;
        } else {
            // move_up は ti < q_size が前提
            if (ti >= q_size) return false;
            if (!QueriesModifierArgValidator::is_move_up_arg_valid(queries, fi, ti)) return false;
        }
        return is_stack_cnt_safe(queries, fi, ti, current_N);
    }

    // NH_QueryMoveTo::is_stack_cnt_safe と同一ロジック
    static bool is_stack_cnt_safe(const QRIList &queries, int fi, int ti, int current_N) {
        const CommonPushType push_type = queries[fi].get_push_type();
        if (push_type != CommonPushType::A2B && push_type != CommonPushType::B2A) return true;
        int lo = (fi < ti) ? fi + 1 : ti;
        int hi = (fi < ti) ? ti : fi;
        bool adds_one = (fi < ti) ? (push_type == CommonPushType::A2B) : (push_type == CommonPushType::B2A);
        for (int qi = lo; qi < hi; qi++) {
            int cnt = queries[qi].get_stack_a_cnt();
            if (adds_one && cnt + 1 > current_N) return false;
            if (!adds_one && cnt - 1 < 0) return false;
        }
        return true;
    }

    static void print_queries(const QRIList &queries) {
        out("[queries]");
        for (int i = 0; i < (int)queries.size(); i++) {
            out("q" + std::to_string(i) + ":", queries[i]);
        }
    }

    enum class SortBy { PartnerCount, Idx };

    static void print_analysis(
            const std::string &header,
            const std::string &self_label,
            const std::string &entry_label,
            const std::vector<std::vector<Info>> &results,
            const std::vector<int> &self_ins_turns,
            const std::vector<int> &self_tar_weights,
            SortBy sort_by) {
        out(header);
        std::vector<int> order;
        order.reserve(results.size());
        for (int i = 0; i < (int)results.size(); i++) order.push_back(i);
        if (sort_by == SortBy::PartnerCount) {
            std::sort(order.begin(), order.end(), [&](int a, int b) {
                return results[a].size() > results[b].size();
            });
        }
        for (int i : order) {
            std::string line = self_label + std::to_string(i) +
                               "(ins_turn:" + std::to_string(self_ins_turns[i]) +
                               ", tar_weight:" + std::to_string(self_tar_weights[i]) + "):";
            for (const auto &m : results[i]) {
                line += " " + entry_label + std::to_string(m.target_idx) +
                        "(ins_turn:" + std::to_string(m.tar_ins_turn) +
                        ", tar_weight:" + std::to_string(m.tar_weight) +
                        ", diff:" + std::to_string(m.score_diff) + ")";
            }
            out(line);
        }
    }

    static void print_from_analysis(const QueriesState &qs,
                                    const std::vector<std::vector<Info>> &from_results,
                                    const std::vector<int> &tar_weights) {
        int q_size = static_cast<int>(qs.queries.size());
        std::vector<int> self_ins_turns(q_size);
        for (int i = 0; i < q_size; i++) self_ins_turns[i] = qs.queries[i].get_ins_turn();
        print_analysis("[from analysis sort by to cnt]", "from", "to",
                       from_results, self_ins_turns, tar_weights, SortBy::PartnerCount);
        print_analysis("[from analysis sort by from idx]", "from", "to",
                       from_results, self_ins_turns, tar_weights, SortBy::Idx);
    }

    static void print_to_analysis(const QueriesState &qs,
                                  const std::vector<std::vector<Info>> &to_results,
                                  const std::vector<int> &tar_weights,
                                  int q_size) {
        std::vector<int> self_ins_turns(q_size + 1);
        for (int i = 0; i <= q_size; i++) self_ins_turns[i] = get_ins_turn_at(qs, i);
        print_analysis("[to analysis sort by to cnt]", "to", "from",
                       to_results, self_ins_turns, tar_weights, SortBy::PartnerCount);
        print_analysis("[to analysis sort by to idx]", "to", "from",
                       to_results, self_ins_turns, tar_weights, SortBy::Idx);
    }

    static void print_summary(int sum_turn, int total_valid, int total_improvable,
                               int best_from, int best_to, int best_diff) {
        out("[summary]");
        out("sum_turn: " + std::to_string(sum_turn));
        out("valid_pairs: " + std::to_string(total_valid) +
            " / improvable_pairs: " + std::to_string(total_improvable));
        if (best_from >= 0) {
            out("best_move: from=" + std::to_string(best_from) +
                ", to=" + std::to_string(best_to) +
                ", diff=" + std::to_string(best_diff));
        } else {
            out("best_move: (none)");
        }
    }
};
