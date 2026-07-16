#pragma once

#include "Command/Command.h"
#include "Query/QueryCommonRIFactory.h"
#include "Query/QueryStateApplyUtil.h"
#include "QueryToCommandBuilder.h"
#include "State.h"
#include "YakinamashiQuery/Command/YstCommandsBuilder.h"
#include "YakinamashiQuery/Order/ExistAllOrdManager.h"
#include "YakinamashiQuery/Order/OrdPosCalculator.h"
#include "YakinamashiQuery/State/YakinamashiState.h"
#include "YakinamashiQuery/Util/FinalRotCalculator.h"
#include "util.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>

template<size_t MAX_N>
class SwapAllOrdGreedySearch {
    constexpr static int B_ALL_ORD_RADIUS = 20;
    constexpr static size_t MAX_ALL_ORD_A = YQConstants::GET_MAX_ALL_ORD_A<MAX_N>();
    constexpr static size_t MAX_ALL_ORD_B = YQConstants::GET_MAX_ALL_ORD_B<MAX_N>();

    using AExist = my_bitset<MAX_ALL_ORD_A>;
    using BExist = my_bitset<MAX_ALL_ORD_B>;

    // 責務: 物理 stack、order、ValState、exist all_ord、suffix query、range、確定 command、score を保持する。
    // 必要な理由: 候補 all_ord 評価と current range commit の両方で同じ状態一式を使うため。
    struct Node {
        State init_st;
        State st;
        QueryOrder order;
        QueriesState queries_state;
        my_vector<ValState> val_a;
        my_vector<ValState> val_b;
        AExist exist_a;
        BExist exist_b;
        int range_l = 0;
        int range_r = 0;
        my_vector<command> range_cmds;
        int current_q_idx = 0;
        int cmd_idx = 0;
        my_vector<command> past_cmds;
        int score = 0;
        bool done = false;
        bool log_enabled = false;

        Node(const State& init_st,
             const QueryOrder& init_order,
             const QueriesState& init_queries_state,
             bool init_log_enabled)
            : init_st(init_st),
              st(init_st),
              order(init_order),
              queries_state(init_queries_state),
              val_a(init_st.initial_N, ValState::NONE),
              val_b(init_st.initial_N, ValState::NONE),
              log_enabled(init_log_enabled)
        {}
    };

    // 責務: all_ord を変更する候補と、変更しない候補を同じ評価経路へ渡す。
    // 必要な理由: 変更候補が 0 件でも no-change を suffix swap greedy まで評価するため。
    struct AllOrdCandidate {
        AllOrd all_ord;
        bool no_change = false;
    };

    // 責務: 選んだ action、出力 command、適用後 node、score をまとめる。
    // 必要な理由: 評価時に作った next_node を採用後にも再利用し、同じ計算を繰り返さないため。
    struct GreedyStep {
        command action;
        int score;
        my_vector<command> emitted_cmds;
        Node next_node;

        GreedyStep(command init_action, int init_score, my_vector<command> init_cmds, const Node& init_node)
            : action(init_action),
              score(init_score),
              emitted_cmds(std::move(init_cmds)),
              next_node(init_node)
        {}
    };

    struct CandidateEval {
        AllOrdCandidate candidate;
        int score;
        my_vector<command> commit_cmds;
        Node range_end_node;

        CandidateEval(AllOrdCandidate init_candidate,
                      int init_score,
                      my_vector<command> init_commit_cmds,
                      const Node& init_range_end_node)
            : candidate(init_candidate),
              score(init_score),
              commit_cmds(std::move(init_commit_cmds)),
              range_end_node(init_range_end_node)
        {}
    };

public:
    // 責務: SwapAllOrdGreedySearch が作った command 列と最終手数を保持する。
    // 必要な理由: 新しい探索を既存 SwapBeamSearch の型に依存せず単体で利用できるようにするため。
    struct Result {
        my_vector<command> cmds;
        int score = 1 << 30;
    };

    // 責務: range ごとに A2B/B2A の target all_ord を選び、current range command だけ確定しながら最後まで進める。
    // 必要な理由: 後続 suffix の swap 可能性を見積もりに使いながら、range 単位で all_ord を greedy に決めるため。
    static Result run(const State& init_st,
                      const YakinamashiState<MAX_N>& yst,
                      bool log_enabled = false,
                      bool write_result_file = true
                      ) {
        reset_log_file(log_enabled);
        Node node(init_st, yst.query_order, yst.queries_state, log_enabled);
        initialize_node(node);
        set_range_and_cmds_done(node);
        refresh_score(node);
        validate_node_all(node, "run_start");
        validate_suffix_replay(node, "run_start");
        log_run_start(log_enabled, init_st, yst, node);
        int loop_cnt=0;
        while (!node.done) {
            cout << "loop_cnt "<<loop_cnt << endl;
            validate_node_all(node, "run_loop_begin");
            log_range_start(node);
            if (range_has_target_ab(node)) {
                my_vector<AllOrdCandidate> candidates = build_candidates(node);
                my_assert(!candidates.empty());
                log_candidates(node, candidates);
                std::optional<CandidateEval> best_eval;
                for (const AllOrdCandidate& candidate: candidates) {
                    log_candidate_start(node, candidate);
                    CandidateEval eval = evaluate_candidate(node, candidate);
                    validate_candidate_eval(node, eval, "run_candidate_eval");
                    log_candidate(node, eval);
                    if (!best_eval.has_value() ||
                        eval.score < best_eval->score ||
                        (eval.score == best_eval->score &&
                         eval.commit_cmds.size() < best_eval->commit_cmds.size())) {
                        best_eval.emplace(eval);
                    }
                }
                my_assert(best_eval.has_value());
                log_commit(node, *best_eval);
                node = best_eval->range_end_node;
                validate_node_all(node, "after_candidate_commit");
            } else {
                log_plain_commit(node);
                commit_plain_range(node);
            }
            set_range_and_cmds_done(node);
            refresh_score(node);
            validate_node_all(node, "after_set_next_range");
            validate_suffix_replay(node, "after_set_next_range");
            loop_cnt++;
        }
        append_final_rot(node);
        validate_sorted_commands(init_st, node.past_cmds);
        if (write_result_file && !output_dir.empty()) {
            write_final_result(init_st, yst, node);
        }
        log_run_end(log_enabled, node);
        return Result{node.past_cmds, static_cast<int>(node.past_cmds.size())};
    }

private:
    // 責務: 初期 A の INIT entry を exist_a に登録し、B は空として扱う。
    // 必要な理由: OrdCalc と suffix rebuild が現在存在 all_ord を参照するため。
    static void initialize_node(Node& node) {
        my_assert(node.st.B.empty());
        for (int i = 0; i < node.st.A.size(); i++) {
            int v = node.st.A[i];
            node.val_a[v] = ValState::INIT;
            AllOrd ord = node.order.aorder.get_all_order(v, ValState::INIT);
            ExistAllOrdManager::set_exist(node.exist_a, ord);
        }
    }

    // 責務: A2A 連続と直後 AB query、または末尾 A2A 連続の半開区間終端を返す。
    // 必要な理由: [A2A, A2A, A2B/B2A] を一つの all_ord 決定単位にするため。
    static int find_range_end(const QueriesState& queries_state, int q_idx) {
        const int qn = static_cast<int>(queries_state.queries.size());
        if (q_idx >= qn) return qn;
        int r = q_idx;
        while (r < qn && queries_state.queries[r].is_a2a_type()) {
            r++;
        }
        if (r < qn) r++;
        return r;
    }

    // 責務: 空 suffix なら done、そうでなければ range と range command を node に設定する。
    // 必要な理由: 外側 loop と suffix greedy が常に current range command を参照できるようにするため。
    static void set_range_and_cmds_done(Node& node) {
        log_node_score_state(node, "set_range_begin");
        node.done = false;
        while (true) {
            if (node.queries_state.queries.empty()) {
                node.done = true;
                node.range_l = 0;
                node.range_r = 0;
                node.current_q_idx = 0;
                node.cmd_idx = 0;
                node.range_cmds.clear();
                log_node_score_state(node, "set_range_done_empty");
                return;
            }
            node.range_l = 0;
            node.current_q_idx = 0;
            node.range_r = find_range_end(node.queries_state, 0);
            log_node_score_state(node, "set_range_after_find");
            skip_noop_a2a(node);
            log_node_score_state(node, "set_range_after_skip_noop");
            if (node.current_q_idx >= node.range_r) {
                log_node_score_state(node, "set_range_before_trim_noop_range");
                trim_finished_range(node);
                log_node_score_state(node, "set_range_after_trim_noop_range");
                continue;
            }
            node.range_cmds = build_range_cmds(node);
            node.cmd_idx = 0;
            if (!node.range_cmds.empty()) {
                log_node_score_state(node, "set_range_ready");
                return;
            }
            log_node_score_state(node, "set_range_before_trim_empty_cmds");
            trim_finished_range(node);
            log_node_score_state(node, "set_range_after_trim_empty_cmds");
        }
    }

    // 責務: current range 先頭側にある ins_turn 0 の A2A を Query/ValState/exist 上で完了扱いにする。
    // 必要な理由: command が存在しない A2A で range 処理が止まらないようにするため。
    static void skip_noop_a2a(Node& node) {
        while (node.current_q_idx < node.range_r) {
            const QueryCommonRI& qri = node.queries_state.queries[node.current_q_idx];
            if (!qri.is_a2a_type() || qri.get_ins_turn() != 0) return;
            apply_query_logical_update(node, qri);
            node.current_q_idx++;
        }
    }

    static bool range_has_target_ab(const Node& node) {
        if (node.done || node.range_r <= node.current_q_idx) return false;
        const QueryCommonRI& qri = node.queries_state.queries[node.range_r - 1];
        return qri.is_a2b_type() || qri.is_b2a_type();
    }

    static const QueryCommonRI& get_range_target_qri(const Node& node) {
        my_assert(range_has_target_ab(node));
        return node.queries_state.queries[node.range_r - 1];
    }

    static my_vector<AllOrdCandidate> build_candidates(Node& node) {
        const QueryCommonRI& qri = get_range_target_qri(node);
        if (qri.is_a2b_type()) return build_a2b_candidates(node, qri);
        my_assert(qri.is_b2a_type());
        return build_b2a_candidates(node, qri);
    }

    static my_vector<AllOrdCandidate> build_a2b_candidates(const Node& node,
                                                           const QueryCommonRI& qri) {
        const int v = qri.get_tar_v();
        const my_vector<OrderEntry>& entries = node.order.border.get_order_entries();
        const int n = static_cast<int>(entries.size());
        my_vector<AllOrdCandidate> candidates;
        candidates.push_back({AllOrd(-1), true});
        if (n == 0) {
            candidates.push_back({AllOrd(0), false});
            return candidates;
        }
        my_bitset<MAX_ALL_ORD_B> used;
        const int base = node.order.border.get_target_order(v).get();
        for (int d = -B_ALL_ORD_RADIUS; d <= B_ALL_ORD_RADIUS; d++) {
            const int ord = mod(base + d, n);
            if (used[ord]) continue;
            used.set(ord);
            candidates.push_back({AllOrd(ord), false});
        }
        return candidates;
    }

    static my_vector<AllOrdCandidate> build_b2a_candidates(const Node& node,
                                                           const QueryCommonRI& qri) {
        Node temp = node;
        for (int qi = temp.current_q_idx; qi < node.range_r - 1; qi++) {
            apply_qri_to_rebuild_state(temp, temp.queries_state.queries, qi);
        }
        const int v = qri.get_tar_v();
        my_vector<AllOrdCandidate> candidates;
        candidates.push_back({AllOrd(-1), true});
        my_bitset<MAX_ALL_ORD_A> used;
        int duplicate_skip_count = 0;
        int invalid_skip_count = 0;
        for (int i = 0; i < temp.st.A.size(); i++) {
            const int before_v = temp.st.A[i];
            const AllOrd before_ord = temp.order.aorder.get_all_order(before_v, temp.val_a[before_v]);
            const int ord_idx = before_ord.get();
            if (used[ord_idx]) {
                duplicate_skip_count++;
                log_b2a_candidate_scan(temp, v, i, before_v, before_ord, true, false);
                continue;
            }
            const bool valid = is_valid_a_target_move(temp, v, before_ord);
            log_b2a_candidate_scan(temp, v, i, before_v, before_ord, false, valid);
            if (!valid) {
                invalid_skip_count++;
                continue;
            }
            used.set(ord_idx);
            candidates.push_back({before_ord, false});
        }
        log_b2a_candidate_summary(temp,
                                  v,
                                  duplicate_skip_count,
                                  invalid_skip_count,
                                  static_cast<int>(candidates.size()) - 1);
        return candidates;
    }

    // 責務: 候補位置へ TARGET(v) を移した後も TARGET value が昇順かをコピー上で確認する。
    // 必要な理由: A 側 TARGET の相対順序を壊す all_ord 候補を除外するため。
    static bool is_valid_a_target_move(const Node& node,
                                       int v,
                                       AllOrd candidate) {
        const my_vector<OrderEntry>& entries = node.order.aorder.get_order_entries();
        int prev_idx = -1;
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            if (entries[i].value == v && entries[i].val_state == ValState::TARGET) {
                prev_idx = i;
                break;
            }
        }
        my_assert(prev_idx != -1);
        int target = candidate.get();
        my_assert(0 <= target && target < static_cast<int>(entries.size()));
        my_vector<OrderEntry> moved = entries;
        OrderEntry entry = moved[prev_idx];
        moved.erase(moved.begin() + prev_idx);
        if (prev_idx < target) target--;
        if (!(0 <= target && target <= static_cast<int>(moved.size()))) return false;
        moved.insert(moved.begin() + target, entry);
        int prev_target_v = -1;
        for (const OrderEntry& cur: moved) {
            if (cur.val_state != ValState::TARGET) continue;
            if (prev_target_v >= cur.value) return false;
            prev_target_v = cur.value;
        }
        return true;
    }

    static void apply_candidate_ord(Node& node,
                                    const QueryCommonRI& qri,
                                    AllOrdCandidate candidate) {
        if (candidate.no_change) return;
        const int v = qri.get_tar_v();
        if (qri.is_a2b_type()) {
            node.order.border.update_insert_ord_v(v, candidate.all_ord);
            rebuild_exist_b_from_stack(node);
            return;
        }
        my_assert(qri.is_b2a_type());
        node.order.aorder.insert_update_shift(v, candidate.all_ord);
        rebuild_exist_a_from_stack(node);
    }

    static void rebuild_exist_a_from_stack(Node& node) {
        node.exist_a.reset();
        for (int i = 0; i < node.st.A.size(); i++) {
            const int v = node.st.A[i];
            const AllOrd ord = node.order.aorder.get_all_order(v, node.val_a[v]);
            ExistAllOrdManager::set_exist(node.exist_a, ord);
        }
    }

    static void rebuild_exist_b_from_stack(Node& node) {
        node.exist_b.reset();
        for (int i = 0; i < node.st.B.size(); i++) {
            const int v = node.st.B[i];
            const AllOrd ord = node.order.border.get_all_order(v, node.val_b[v]);
            ExistAllOrdManager::set_exist(node.exist_b, ord);
        }
    }

    // 責務: A[0], A[1], A[n-1], A[2]... の順で value の位置を返す。
    // 必要な理由: A2A 途中の target は top 近辺にいる前提なので、単純な index 昇順走査を避けるため。
    static int find_top_near_pos(const RingBuffer500& stack, int value) {
        const int n = stack.size();
        my_assert(n >= 2);
        int checked = 0;
        for (int dist = 0; checked < n; dist++) {
            const int forward_pos = dist;
            if (forward_pos < n) {
                if (stack[forward_pos] == value) return forward_pos;
                checked++;
            }
            if (dist == 0) continue;
            const int backward_pos = n - dist;
            if (0 <= backward_pos && backward_pos < n && backward_pos != forward_pos) {
                if (stack[backward_pos] == value) return backward_pos;
                checked++;
            }
        }
        return -1;
    }

    // 責務: current A2A の INIT target を、物理 stack 上の円環直後 value の all_ord 直前へ rebase する。
    // 必要な理由: swap 後 rebuild で、A2A 途中の物理位置を query 再生成の開始位置として扱うため。
    static void rebase_current_a2a_init_ord_stopgap(Node& node) {
        if (node.current_q_idx >= static_cast<int>(node.queries_state.queries.size())) return;
        const QueryCommonRI& qri = node.queries_state.queries[node.current_q_idx];
        if (!qri.is_a2a_type()) return;
        const int v = qri.get_tar_v();
        if (node.val_a[v] != ValState::INIT) return;
        my_assert(node.st.A.size() >= 2);
        const int pos = find_top_near_pos(node.st.A, v);
        my_assert(pos != -1);
        const int next_pos = mod(pos + 1, node.st.A.size());
        const int next_v = node.st.A[next_pos];
        my_assert(next_v != v);
        const AllOrd before_ord = node.order.aorder.get_all_order(next_v, node.val_a[next_v]);
        node.order.aorder.rebase_init_ord_stopgap(v, before_ord);
        rebuild_exist_a_from_stack(node);
    }

    static CandidateEval evaluate_candidate(const Node& base,
                                            AllOrdCandidate candidate) {
        Node eval_node = base;
        const QueryCommonRI& target_qri = get_range_target_qri(eval_node);
        apply_candidate_ord(eval_node, target_qri, candidate);
        rebuild_suffix(eval_node);
        set_range_and_cmds_done(eval_node);
        refresh_score(eval_node);
        if (candidate.no_change) {
            debug_assert(eval_node,
                         eval_node.score == base.score,
                         "evaluate_candidate_no_change",
                         "no_change_score_changed");
            debug_assert(eval_node,
                         eval_node.queries_state.sum_turn == base.queries_state.sum_turn,
                         "evaluate_candidate_no_change",
                         "no_change_sum_turn_changed");
        }
        validate_node_all(eval_node, "evaluate_candidate_after_rebuild");
        validate_suffix_replay(eval_node, "evaluate_candidate_after_rebuild");
        my_vector<command> first_range_cmds;
        std::optional<Node> first_range_end_node;
        run_suffix_greedy(eval_node, first_range_cmds, first_range_end_node);
        my_assert(first_range_end_node.has_value());
        validate_node_all(eval_node, "evaluate_candidate_after_suffix_greedy");
        validate_node_all(*first_range_end_node, "evaluate_candidate_first_range_end");
        validate_suffix_replay(*first_range_end_node, "evaluate_candidate_first_range_end");
        return CandidateEval(candidate, eval_node.score, first_range_cmds, *first_range_end_node);
    }

    static void run_suffix_greedy(Node& eval_node,
                                  my_vector<command>& first_range_cmds,
                                  std::optional<Node>& first_range_end_node) {
        bool in_first_range = true;
        const int first_suffix_count = static_cast<int>(eval_node.queries_state.queries.size());
        while (!eval_node.done) {
            while (!eval_node.done && eval_node.cmd_idx < static_cast<int>(eval_node.range_cmds.size())) {
                const command current_cmd = eval_node.range_cmds[eval_node.cmd_idx];
                GreedyStep best_step = choose_best_step(eval_node, current_cmd);
                validate_node_all(best_step.next_node, "run_suffix_greedy_best_step");
                validate_suffix_replay(best_step.next_node, "run_suffix_greedy_best_step");
                if (in_first_range) {
                    first_range_cmds.insert(first_range_cmds.end(),
                                            best_step.emitted_cmds.begin(),
                                            best_step.emitted_cmds.end());
                }
                eval_node = best_step.next_node;
                if (in_first_range &&
                    (eval_node.done ||
                     static_cast<int>(eval_node.queries_state.queries.size()) < first_suffix_count)) {
                    first_range_end_node.emplace(eval_node);
                    in_first_range = false;
                }
            }
            if (!eval_node.done && eval_node.cmd_idx >= static_cast<int>(eval_node.range_cmds.size())) {
                debug_assert(eval_node, eval_node.current_q_idx == eval_node.range_r,
                             "run_suffix_greedy_finish_range", "range_cmds_done_before_queries");
                finish_range(eval_node);
                set_range_and_cmds_done(eval_node);
                validate_node_all(eval_node, "run_suffix_greedy_finish_range");
                if (in_first_range &&
                    static_cast<int>(eval_node.queries_state.queries.size()) < first_suffix_count) {
                    first_range_end_node.emplace(eval_node);
                    in_first_range = false;
                }
            }
        }
    }

    static GreedyStep choose_best_step(const Node& node,
                                       command current_cmd) {
        GreedyStep best = eval_no_swap(node, current_cmd);
        const int no_swap_score = best.score;
        const int no_swap_emit_count = static_cast<int>(best.emitted_cmds.size());
        std::optional<GreedyStep> replace_ss;
        std::optional<GreedyStep> insert_sa;
        std::optional<GreedyStep> insert_sb;
        std::optional<GreedyStep> insert_ss;
        auto try_update = [&](const std::optional<GreedyStep>& cand) {
            if (!cand.has_value()) return;
            if (cand->score < best.score ||
                (cand->score == best.score && cand->emitted_cmds.size() < best.emitted_cmds.size())) {
                best = *cand;
            }
        };
        if (current_cmd == SA) {
            replace_ss = eval_replace_ss(node, true);
            try_update(replace_ss);
            validate_best_step(node, current_cmd, no_swap_score, no_swap_emit_count,
                               best, replace_ss, insert_sa, insert_sb, insert_ss);
            log_greedy_step(node, current_cmd, no_swap_score, no_swap_emit_count,
                            best, replace_ss, insert_sa, insert_sb, insert_ss);
            return best;
        }
        if (current_cmd == SB) {
            replace_ss = eval_replace_ss(node, false);
            try_update(replace_ss);
            validate_best_step(node, current_cmd, no_swap_score, no_swap_emit_count,
                               best, replace_ss, insert_sa, insert_sb, insert_ss);
            log_greedy_step(node, current_cmd, no_swap_score, no_swap_emit_count,
                            best, replace_ss, insert_sa, insert_sb, insert_ss);
            return best;
        }
        insert_sa = eval_insert_swap(node, SA);
        insert_sb = eval_insert_swap(node, SB);
        insert_ss = eval_insert_swap(node, SS);
        try_update(insert_sa);
        try_update(insert_sb);
        try_update(insert_ss);
        validate_best_step(node, current_cmd, no_swap_score, no_swap_emit_count,
                           best, replace_ss, insert_sa, insert_sb, insert_ss);
        log_greedy_step(node, current_cmd, no_swap_score, no_swap_emit_count,
                        best, replace_ss, insert_sa, insert_sb, insert_ss);
        return best;
    }

    static GreedyStep eval_no_swap(const Node& node,
                                   command current_cmd) {
        Node cand = node;
        log_no_swap_state(cand, "begin", current_cmd, false);
        cand.st.do_cmd(current_cmd);
        cand.past_cmds.push_back(current_cmd);
        log_no_swap_state(cand, "after_cmd_and_past", current_cmd, false);
        bool completed = advance_query_by_cmd(cand, current_cmd);
        log_no_swap_state(cand, "after_advance", current_cmd, completed);
        mark_existing_command_done(cand);
        log_no_swap_state(cand, "after_mark_done", current_cmd, completed);
        if (completed) {
            skip_noop_a2a(cand);
            log_no_swap_state(cand, "after_skip_noop", current_cmd, completed);
            if (cand.current_q_idx == cand.range_r) {
                log_no_swap_state(cand, "before_finish_range", current_cmd, completed);
                finish_range(cand);
                log_no_swap_state(cand, "after_finish_range", current_cmd, completed);
                set_range_and_cmds_done(cand);
                log_no_swap_state(cand, "after_set_range", current_cmd, completed);
            } else {
                cand.cmd_idx++;
                log_no_swap_state(cand, "after_cmd_idx_inc_completed", current_cmd, completed);
            }
        } else {
            cand.cmd_idx++;
            log_no_swap_state(cand, "after_cmd_idx_inc_not_completed", current_cmd, completed);
        }
        log_no_swap_state(cand, "return", current_cmd, completed);
        validate_suffix_replay(cand, "eval_no_swap_return");
        return GreedyStep(NO_COMMAND, cand.score, my_vector<command>{current_cmd}, cand);
    }

    static std::optional<GreedyStep> eval_insert_swap(const Node& node,
                                                      command swap_cmd) {
        if (swap_cmd == SA && !can_add_swap_a(node)) return std::nullopt;
        if (swap_cmd == SB && !can_beam_swap_b(node)) return std::nullopt;
        if (swap_cmd == SS && (!can_add_swap_a(node) || !can_beam_swap_b(node))) return std::nullopt;
        Node cand = node;
        apply_beam_swap(cand, swap_cmd);
        cand.past_cmds.push_back(swap_cmd);
        rebuild_suffix(cand);
        set_range_and_cmds_done(cand);
        refresh_score(cand);
        validate_node_all(cand, "eval_insert_swap");
        validate_suffix_replay(cand, "eval_insert_swap");
        return GreedyStep(swap_cmd, cand.score, my_vector<command>{swap_cmd}, cand);
    }

    static std::optional<GreedyStep> eval_replace_ss(const Node& node,
                                                     bool replacing_sa) {
        if (replacing_sa && !can_beam_swap_b(node)) return std::nullopt;
        if (!replacing_sa && !can_add_swap_a(node)) return std::nullopt;
        Node cand = node;
        if (replacing_sa) {
            apply_beam_swap_b(cand);
            cand.st.ss();
        } else {
            apply_beam_swap_a(cand);
            cand.st.ss();
        }
        cand.past_cmds.push_back(SS);
        const command replaced_cmd = replacing_sa ? SA : SB;
        bool completed = advance_query_by_cmd(cand, replaced_cmd);
        if (completed) {
            skip_noop_a2a(cand);
            if (cand.current_q_idx == cand.range_r) {
                finish_range(cand);
            }
        }
        rebuild_suffix(cand);
        set_range_and_cmds_done(cand);
        refresh_score(cand);
        validate_node_all(cand, "eval_replace_ss");
        validate_suffix_replay(cand, "eval_replace_ss");
        return GreedyStep(SS, cand.score, my_vector<command>{SS}, cand);
    }

    static void commit_plain_range(Node& node) {
        while (!node.done && node.cmd_idx < static_cast<int>(node.range_cmds.size())) {
            const command cmd = node.range_cmds[node.cmd_idx];
            node.st.do_cmd(cmd);
            node.past_cmds.push_back(cmd);
            bool completed = advance_query_by_cmd(node, cmd);
            mark_existing_command_done(node);
            if (completed) {
                skip_noop_a2a(node);
                if (node.current_q_idx == node.range_r) {
                    break;
                }
            }
            node.cmd_idx++;
        }
        debug_assert(node, node.current_q_idx == node.range_r,
                     "commit_plain_range", "range_cmds_done_before_queries");
        finish_range(node);
    }

    static void finish_range(Node& node) {
        trim_finished_range(node);
        refresh_score(node);
    }

    static void trim_finished_range(Node& node) {
        log_node_score_state(node, "trim_begin");
        const int trim_count = node.range_r;
        my_assert(0 < trim_count && trim_count <= static_cast<int>(node.queries_state.queries.size()));
        const QueryCommonRI& last_qri = node.queries_state.queries[trim_count - 1];
        const QueryCommon& last_query = last_qri.get_query();
        QRIList trimmed;
        for (int qi = trim_count; qi < static_cast<int>(node.queries_state.queries.size()); qi++) {
            trimmed.push_back(node.queries_state.queries[qi]);
        }
        node.queries_state.first_top_ord_a = last_qri.get_finished_top_ord_a();
        node.queries_state.first_top_ord_b = last_query.get_finished_top_ord_b(node.queries_state.current_N);
        node.queries_state.queries = trimmed;
        node.current_q_idx = 0;
        node.range_l = 0;
        node.range_r = 0;
        node.cmd_idx = 0;
        node.range_cmds.clear();
        refresh_final_and_sum(node.queries_state);
        reset_score_from_sum(node);
        log_node_score_state(node, "trim_end");
    }

    static void rebuild_suffix(Node& node) {
        rebase_current_a2a_init_ord_stopgap(node);
        const int old_suffix_count = static_cast<int>(node.queries_state.queries.size()) - node.current_q_idx;
        QRIList old_suffix;
        for (int qi = node.current_q_idx; qi < static_cast<int>(node.queries_state.queries.size()); qi++) {
            old_suffix.push_back(node.queries_state.queries[qi]);
        }
        const OrdPos first_top_a = get_top_ord_a(node);
        const OrdPos first_top_b = get_top_ord_b(node);
        Node temp = node;
        QRIList rebuilt;
        for (const QueryCommonRI& old_qri: old_suffix) {
            QueryCommon common = build_current_common(temp, old_qri);
            QueryCommonRI qri = build_current_qri(temp, rebuilt, common);
            rebuilt.push_back(qri);
            apply_qri_to_rebuild_state(temp, rebuilt, static_cast<int>(rebuilt.size()) - 1);
        }
        node.queries_state = QueriesState(
                rebuilt,
                0,
                {NO_COMMAND, 0},
                first_top_a,
                first_top_b,
                node.st.current_N
        );
        node.current_q_idx = 0;
        node.range_l = 0;
        node.range_r = 0;
        node.cmd_idx = 0;
        node.range_cmds.clear();
        refresh_score(node);
        log_rebuild(node, old_suffix_count, static_cast<int>(rebuilt.size()));
        validate_node_all(node, "rebuild_suffix");
    }

    static QueryCommon build_current_common(Node& node, const QueryCommonRI& orig_qri) {
        const int v = orig_qri.get_tar_v();
        if (orig_qri.is_a2b_type()) {
            OrdPos ord_a = OrdCalc::get_tar_ord_posa_of_a2b<MAX_N>(
                    node.exist_a, node.order.aorder, v, node.val_a[v], node.st.A.size());
            RelOrd rel_b = OrdCalc::get_tar_rel_ordb_of_a2b<MAX_N>(
                    node.exist_b, node.order.border, v, node.st.B.size());
            return QueryCommon(CommonPushType::A2B, v, node.st.A.size(), ord_a.get(), rel_b.get());
        }
        if (orig_qri.is_b2a_type()) {
            RelOrd rel_a = OrdCalc::get_tar_rel_orda_of_b2a<MAX_N>(
                    node.exist_a, node.order.aorder, v, node.st.A.size());
            OrdPos ord_b = OrdCalc::get_tar_ord_posb_of_b2a<MAX_N>(
                    node.exist_b, node.order.border, v, node.val_b[v], node.st.B.size());
            return QueryCommon(CommonPushType::B2A, v, node.st.A.size(), rel_a.get(), ord_b.get());
        }
        my_assert(orig_qri.is_a2a_type());
        OrdPos ord_a1 = OrdCalc::get_tar_orda1_of_a2a<MAX_N>(
                node.exist_a, node.order.aorder, v, node.st.A.size(), node.val_a[v]);
        RelOrd rel_a2 = OrdCalc::get_tar_rel_orda2_of_a2a<MAX_N>(
                node.exist_a, node.order.aorder, v, node.st.A.size(), node.val_a[v]);
        OrdPos top_b = get_top_ord_b(node);
        OrdPos ord_a2 = to_ord_pos(rel_a2, node.st.A.size());
        int flip_dis_dir = A2AQueryUtil::calc_flip_dis_dir(
                ord_a1, ord_a2, node.st.A.size(), e_a2a_type::swap_rot);
        return QueryCommon(CommonPushType::A2AS, v, node.st.A.size(),
                           ord_a1.get(), rel_a2.get(), top_b.get(), flip_dis_dir);
    }

    static QueryCommonRI build_current_qri(const Node& node,
                                           const my_vector<QueryCommonRI>& prefix_qris,
                                           const QueryCommon& query) {
        QCRIFactory factory;
        QueriesState temp_state(
                prefix_qris,
                0,
                {RA, 0},
                get_top_ord_a(node),
                get_top_ord_b(node),
                node.st.current_N
        );
        return factory.build_ri(temp_state, query, static_cast<int>(prefix_qris.size()));
    }

    static void apply_qri_to_rebuild_state(Node& temp, const my_vector<QueryCommonRI>& qris, int relative_q_idx) {
        QueriesCalculator qs_calc(temp.st.current_N, qris);
        const QueryCommonRI& qri = qris[relative_q_idx];
        QueryStateApplyUtil::apply_query_to_state(temp.st, qs_calc, relative_q_idx, qri);
        apply_query_logical_update(temp, qri);
    }

    static my_vector<command> build_range_cmds(const Node& node) {
        CommandBuildState build_st(node.st);
        for (int qi = node.current_q_idx; qi < node.range_r; qi++) {
            add_qri_commands(build_st, node.queries_state.queries, qi);
        }
        return build_st.cmds;
    }

    static void add_qri_commands(CommandBuildState& build_st,
                                 const my_vector<QueryCommonRI>& qris,
                                 int relative_q_idx) {
        QueryToCommandBuilder builder(build_st);
        QueriesCalculator qs_calc(build_st.st.current_N, qris);
        const QueryCommonRI& qri = qris[relative_q_idx];
        if (qri.is_a2b_type()) {
            builder.add_a2b_b2a(e_push_type::a2b, qri.get_a_cmd_AB(), qri.get_b_cmd_AB(),
                                qs_calc.get_orig_a_cmd_cnt_A2B(relative_q_idx),
                                qs_calc.get_orig_b_cmd_cnt_A2B(relative_q_idx));
        } else if (qri.is_b2a_type()) {
            builder.add_a2b_b2a(e_push_type::b2a, qri.get_a_cmd_AB(), qri.get_b_cmd_AB(),
                                qs_calc.get_orig_a_cmd_cnt_B2A(relative_q_idx),
                                qs_calc.get_orig_b_cmd_cnt_B2A(relative_q_idx));
        } else {
            my_assert(qri.is_a2a_type());
            builder.add_a2a(e_a2a_type::swap_rot, qri.get_cmd_rot_a1_A2A(),
                            qri.get_cmd_rot_a1_cnt_A2A(), qri.get_flip_dis_dir_A2A());
        }
    }

    static OrdPos get_top_ord_a(const Node& node) {
        if (node.st.A.empty()) return OrdPos(0);
        int v = node.st.A[0];
        AllOrd all_ord = node.order.aorder.get_all_order(v, node.val_a[v]);
        return to_ord_pos(OrdCalc::get_rel_ord(node.exist_a, all_ord), node.st.A.size());
    }

    static OrdPos get_top_ord_b(const Node& node) {
        if (node.st.B.empty()) return OrdPos(0);
        int v = node.st.B[0];
        AllOrd all_ord = node.order.border.get_all_order(v, node.val_b[v]);
        return to_ord_pos(OrdCalc::get_rel_ord(node.exist_b, all_ord), node.st.B.size());
    }

    static bool can_add_swap_a(const Node& node) {
        if (node.st.A.size() < 2) return false;
        const int v0 = node.st.A[0];
        const int v1 = node.st.A[1];
        if (!allow_target_init_a(node, v0, v1)) return false;
        if (node.current_q_idx >= static_cast<int>(node.queries_state.queries.size())) return true;
        const QueryCommonRI& qri = node.queries_state.queries[node.current_q_idx];
        if (!qri.is_a2a_type()) return true;
        const int v = qri.get_tar_v();
        return node.st.A[0] != v && node.st.A[1] != v;
    }

    static bool allow_target_init_a(const Node& node, int v0, int v1) {
        const ValState s0 = node.val_a[v0];
        const ValState s1 = node.val_a[v1];
        if (s0 == s1) return s0 != ValState::TARGET;
        my_assert((s0 == ValState::TARGET && s1 == ValState::INIT) ||
                  (s0 == ValState::INIT && s1 == ValState::TARGET));
        const int ord0 = node.order.aorder.get_all_order(v0, s0).get();
        const int ord1 = node.order.aorder.get_all_order(v1, s1).get();
        const int left = std::min(ord0, ord1);
        const int right = std::max(ord0, ord1);
        const my_vector<OrderEntry>& entries = node.order.aorder.get_order_entries();
        for (int ord = left + 1; ord < right; ord++) {
            if (entries[ord].val_state == ValState::TARGET) return false;
        }
        return true;
    }

    static bool can_beam_swap_b(const Node& node) {
        return node.st.B.size() >= 2;
    }

    static void apply_beam_swap(Node& node, command swap_cmd) {
        if (swap_cmd == SA) {
            apply_beam_swap_a(node);
            node.st.sa();
            return;
        }
        if (swap_cmd == SB) {
            apply_beam_swap_b(node);
            node.st.sb();
            return;
        }
        my_assert(swap_cmd == SS);
        apply_beam_swap_a(node);
        apply_beam_swap_b(node);
        node.st.ss();
    }

    static void apply_beam_swap_a(Node& node) {
        my_assert(can_add_swap_a(node));
        int v0 = node.st.A[0];
        int v1 = node.st.A[1];
        ValState s0 = node.val_a[v0];
        ValState s1 = node.val_a[v1];
        AllOrd ord0 = node.order.aorder.get_all_order(v0, s0);
        AllOrd ord1 = node.order.aorder.get_all_order(v1, s1);
        node.order.aorder.swap_all_ord_entries(ord0, ord1);
    }

    static void apply_beam_swap_b(Node& node) {
        my_assert(can_beam_swap_b(node));
        int v0 = node.st.B[0];
        int v1 = node.st.B[1];
        ValState s0 = node.val_b[v0];
        ValState s1 = node.val_b[v1];
        my_assert(s0 == ValState::TARGET);
        my_assert(s1 == ValState::TARGET);
        AllOrd ord0 = node.order.border.get_all_order(v0, s0);
        AllOrd ord1 = node.order.border.get_all_order(v1, s1);
        node.order.border.swap_all_ord_entries(ord0, ord1);
    }

    static bool advance_query_by_cmd(Node& node, command cmd) {
        bool query_completed = update_by_existing_command(node, cmd);
        if (!query_completed && (cmd == SA || cmd == SS)) {
            query_completed = maybe_update_a2a_target(node);
            if (query_completed) node.current_q_idx++;
        }
        return query_completed;
    }

    static bool update_by_existing_command(Node& node, command cmd) {
        if (cmd == PB) {
            int v = node.st.B[0];
            const QueryCommonRI& qri = node.queries_state.queries[node.current_q_idx];
            my_assert(qri.is_a2b_type());
            my_assert(v == qri.get_tar_v());
            ExistAllOrdManager::upd_ord_a2b<MAX_N>(
                    v, node.exist_a, node.exist_b, node.order.aorder, node.order.border,
                    node.val_a, node.val_b);
            node.current_q_idx++;
            return true;
        }
        if (cmd == PA) {
            int v = node.st.A[0];
            const QueryCommonRI& qri = node.queries_state.queries[node.current_q_idx];
            my_assert(qri.is_b2a_type());
            my_assert(v == qri.get_tar_v());
            ExistAllOrdManager::upd_ord_b2a<MAX_N>(
                    v, node.exist_a, node.exist_b, node.order.aorder, node.order.border,
                    node.val_a, node.val_b);
            node.current_q_idx++;
            return true;
        }
        return false;
    }

    static bool maybe_update_a2a_target(Node& node) {
        if (node.current_q_idx >= static_cast<int>(node.queries_state.queries.size())) return false;
        const QueryCommonRI& qri = node.queries_state.queries[node.current_q_idx];
        if (!qri.is_a2a_type()) return false;
        const int v = qri.get_tar_v();
        if (node.val_a[v] != ValState::INIT) return false;
        if (!is_a2a_target_ready(node, v)) return false;
        ExistAllOrdManager::upd_ord_a2a<MAX_N>(v, node.exist_a, node.order.aorder, node.val_a);
        return true;
    }

    static bool is_a2a_target_ready(const Node& node, int v) {
        const int n = node.st.A.size();
        my_assert(n >= 2);
        int pos = -1;
        if (node.st.A[0] == v) pos = 0;
        if (node.st.A[1] == v) pos = 1;
        my_assert(pos != -1);
        const int prev_v = node.st.A[mod(pos - 1, n)];
        const int next_v = node.st.A[mod(pos + 1, n)];
        const int target = node.order.aorder.get_target_order(v).get();
        const int prev_ord = node.order.aorder.get_all_order(prev_v, node.val_a[prev_v]).get();
        const int next_ord = node.order.aorder.get_all_order(next_v, node.val_a[next_v]).get();
        if (prev_ord == next_ord) return true;
        if (prev_ord < next_ord) return prev_ord < target && target < next_ord;
        return prev_ord < target || target < next_ord;
    }

    static void apply_query_logical_update(Node& node, const QueryCommonRI& qri) {
        const int v = qri.get_tar_v();
        if (qri.is_a2b_type()) {
            ExistAllOrdManager::upd_ord_a2b<MAX_N>(
                    v, node.exist_a, node.exist_b, node.order.aorder, node.order.border,
                    node.val_a, node.val_b);
            return;
        }
        if (qri.is_b2a_type()) {
            ExistAllOrdManager::upd_ord_b2a<MAX_N>(
                    v, node.exist_a, node.exist_b, node.order.aorder, node.order.border,
                    node.val_a, node.val_b);
            return;
        }
        my_assert(qri.is_a2a_type());
        if (node.val_a[v] == ValState::TARGET) return;
        ExistAllOrdManager::upd_ord_a2a<MAX_N>(v, node.exist_a, node.order.aorder, node.val_a);
    }

    static void refresh_score(Node& node) {
        refresh_final_and_sum(node.queries_state);
        reset_score_from_sum(node);
    }

    // 責務: suffix 再計算をせず、past command 数と現在の sum_turn から評価値を設定する。
    // 必要な理由: no-swap で既存 command を 1 つ進めたときは query 再生成せず sum_turn だけを減らすため。
    static void reset_score_from_sum(Node& node) {
        node.score = static_cast<int>(node.past_cmds.size()) + node.queries_state.sum_turn;
    }

    // 責務: suffix の残り手数 sum_turn を 1 減らし、score を同期する。
    // 必要な理由: no-swap branch では query 列を再生成せず、現在 command をそのまま消費するため。
    static void mark_existing_command_done(Node& node) {
        my_assert(node.queries_state.sum_turn > 0);
        node.queries_state.sum_turn--;
        reset_score_from_sum(node);
    }

    static void refresh_final_and_sum(QueriesState& queries_state) {
        queries_state.final_rot_info =
                FinalRotCalculator::build_final_rot_info(queries_state.current_N, queries_state);
        int sum_turn = queries_state.final_rot_info.second;
        for (const QueryCommonRI& qri: queries_state.queries) {
            sum_turn += qri.get_ins_turn();
        }
        queries_state.sum_turn = sum_turn;
    }

    static void append_final_rot(Node& node) {
        my_assert(node.st.B.empty());
        const command final_cmd = node.queries_state.final_rot_info.first;
        const int final_cnt = node.queries_state.final_rot_info.second;
        if (final_cnt == 0) return;
        my_assert(final_cmd == RA || final_cmd == RRA);
        for (int i = 0; i < final_cnt; i++) {
            if (final_cmd == RA) node.st.ra();
            else node.st.rra();
            node.past_cmds.push_back(final_cmd);
        }
        node.queries_state.final_rot_info = {NO_COMMAND, 0};
        node.queries_state.sum_turn = 0;
        reset_score_from_sum(node);
    }

    static void validate_sorted_commands(const State& init_st, const my_vector<command>& cmds) {
        State st(init_st);
        for (command cmd: cmds) {
            st.do_cmd(cmd);
        }
        my_assert(st.B.empty());
        for (int i = 0; i + 1 < st.A.size(); i++) {
            my_assert(st.A[i] < st.A[i + 1]);
        }
    }

    // 責務: DEBUG build では常に true、通常 build では log_enabled に従って validate 可否を返す。
    // 必要な理由: 通常実行の性能を保ちつつ、今回の log 有効 main 経路でも原因特定用検証を有効にするため。
    static bool should_validate(const Node& node) {
#ifdef DEBUG
        (void) node;
        return true;
#else
        return node.log_enabled;
#endif
    }

    // 責務: 失敗ラベルと node の主要状態を swap_all_ord_greedy_debug.txt へ出力して停止する。
    // 必要な理由: my_assert だけでは破綻箇所の状態が不足するため、停止直前の調査情報を残すため。
    static void debug_fail(const Node& node, const char* label, const char* reason) {
        append_log_line(true,
                        "validate_fail",
                        "label", label,
                        "reason", reason,
                        "done", node.done,
                        "remaining_queries", static_cast<int>(node.queries_state.queries.size()),
                        "range_l", node.range_l,
                        "range_r", node.range_r,
                        "current_q_idx", node.current_q_idx,
                        "cmd_idx", node.cmd_idx,
                        "range_cmd_count", static_cast<int>(node.range_cmds.size()),
                        "score", node.score,
                        "sum_turn", node.queries_state.sum_turn,
                        "past_cmd_count", static_cast<int>(node.past_cmds.size()),
                        "A_size", node.st.A.size(),
                        "B_size", node.st.B.size(),
                        "current_N", node.st.current_N);
        my_assert(false);
    }

    // 責務: condition が false のときだけ debug_fail を呼ぶ。
    // 必要な理由: 各 validate 関数で同じ失敗ログ形式を使うため。
    static void debug_assert(const Node& node, bool condition, const char* label, const char* reason) {
        if (!should_validate(node)) return;
        if (condition) return;
        debug_fail(node, label, reason);
    }

    // 責務: size と各 command の一致を返す。
    // 必要な理由: range command の再生成結果が保持値と一致するか検証するため。
    static bool commands_equal(const my_vector<command>& lhs, const my_vector<command>& rhs) {
        if (lhs.size() != rhs.size()) return false;
        for (int i = 0; i < static_cast<int>(lhs.size()); i++) {
            if (lhs[i] != rhs[i]) return false;
        }
        return true;
    }

    // 責務: RingBuffer500 の size と各 value の一致を返す。
    // 必要な理由: commit command replay と range_end_node の物理 stack を比較するため。
    static bool stack_equal(const RingBuffer500& lhs, const RingBuffer500& rhs) {
        if (lhs.size() != rhs.size()) return false;
        for (int i = 0; i < lhs.size(); i++) {
            if (lhs[i] != rhs[i]) return false;
        }
        return true;
    }

    // 責務: stack サイズ、range index、cmd index、score と sum_turn の関係を assert する。
    // 必要な理由: どの段階で node の最小前提が崩れたか早期に特定するため。
    static void validate_node_basic(const Node& node, const char* label) {
        if (!should_validate(node)) return;
        debug_assert(node, node.st.A.size() + node.st.B.size() == node.st.current_N,
                     label, "stack_size_mismatch");
        debug_assert(node, node.queries_state.sum_turn >= 0, label, "negative_sum_turn");
        debug_assert(node,
                     node.score == static_cast<int>(node.past_cmds.size()) + node.queries_state.sum_turn,
                     label,
                     "score_mismatch");
        if (node.done) {
            debug_assert(node, node.queries_state.queries.empty(), label, "done_with_queries");
            debug_assert(node, node.range_l == 0 && node.range_r == 0, label, "done_range_not_zero");
            debug_assert(node, node.current_q_idx == 0 && node.cmd_idx == 0, label, "done_idx_not_zero");
            debug_assert(node, node.range_cmds.empty(), label, "done_range_cmds_not_empty");
            return;
        }
        const int qn = static_cast<int>(node.queries_state.queries.size());
        debug_assert(node, 0 < qn, label, "not_done_empty_queries");
        debug_assert(node, node.range_l == 0, label, "range_l_not_zero");
        debug_assert(node, 0 <= node.current_q_idx, label, "negative_current_q_idx");
        debug_assert(node, node.current_q_idx <= node.range_r, label, "current_q_idx_after_range");
        debug_assert(node, node.range_r <= qn, label, "range_r_after_queries");
        debug_assert(node, 0 <= node.cmd_idx, label, "negative_cmd_idx");
        debug_assert(node, node.cmd_idx <= static_cast<int>(node.range_cmds.size()), label, "cmd_idx_after_cmds");
    }

    // 責務: stack と ValState/Order から exist_a/exist_b を再構築して node の保持値と比較する。
    // 必要な理由: swap/all_ord 更新後に OrdCalc の前提となる exist がずれた瞬間を検出するため。
    static void validate_exist_matches_stack(const Node& node, const char* label) {
        if (!should_validate(node)) return;
        AExist expected_a;
        BExist expected_b;
        for (int i = 0; i < node.st.A.size(); i++) {
            const int v = node.st.A[i];
            debug_assert(node, node.val_a[v] != ValState::NONE, label, "stack_a_val_none");
            const AllOrd ord = node.order.aorder.get_all_order(v, node.val_a[v]);
            ExistAllOrdManager::set_exist(expected_a, ord);
        }
        for (int i = 0; i < node.st.B.size(); i++) {
            const int v = node.st.B[i];
            debug_assert(node, node.val_b[v] != ValState::NONE, label, "stack_b_val_none");
            const AllOrd ord = node.order.border.get_all_order(v, node.val_b[v]);
            ExistAllOrdManager::set_exist(expected_b, ord);
        }
        debug_assert(node, expected_a == node.exist_a, label, "exist_a_mismatch");
        debug_assert(node, expected_b == node.exist_b, label, "exist_b_mismatch");
    }

    // 責務: build_range_cmds(node) と node.range_cmds を比較する。
    // 必要な理由: range の query と保持 command 列がずれた状態で greedy step に入るのを検出するため。
    static void validate_range_cmds(const Node& node, const char* label) {
        if (!should_validate(node) || node.done) return;
        if (node.cmd_idx != 0) return;
        my_vector<command> rebuilt_cmds = build_range_cmds(node);
        debug_assert(node, commands_equal(rebuilt_cmds, node.range_cmds), label, "range_cmds_mismatch");
    }

    // 責務: basic、exist、range command の各 validate を同じ label で実行する。
    // 必要な理由: 呼び出し側が検証内容を漏らさず、段階名だけ指定できるようにするため。
    static void validate_node_all(const Node& node, const char* label) {
        if (!should_validate(node)) return;
        validate_node_basic(node, label);
        validate_exist_matches_stack(node, label);
        validate_range_cmds(node, label);
    }

    // 責務: 現在 range command から最後まで既存 query を実行し、sort 可能性と score 差分を assert する。
    // 必要な理由: rebuild された suffix 自体が正しい command 列を作れるか、score=sum_turn と一致するか検出するため。
    static void validate_suffix_replay(const Node& node, const char* label) {
        if (!should_validate(node)) return;
        Node sim = node;
        int replay_cmd_count = 0;
        my_vector<command> suffix_cmds;
        const auto final_rot_info = node.queries_state.final_rot_info;
        const int final_rot_count = final_rot_info.second;
        int range_start_replay_count = 0;
        auto log_replay_range = [&](const char* event, const Node& replay_node, int actual_cmd_count) {
            int expected_turn_sum = 0;
            std::ostringstream query_oss;
            for (int qi = replay_node.current_q_idx; qi < replay_node.range_r; qi++) {
                const QueryCommonRI& qri = replay_node.queries_state.queries[qi];
                if (qi > replay_node.current_q_idx) query_oss << '|';
                const int q_type = qri.is_a2b_type() ? 1 : (qri.is_b2a_type() ? 2 : 3);
                expected_turn_sum += qri.get_ins_turn();
                query_oss << "{q_idx:" << qi
                          << ",type:" << q_type
                          << ",v:" << qri.get_tar_v()
                          << ",turn:" << qri.get_ins_turn()
                          << '}';
            }
            append_log_line(node.log_enabled,
                            "suffix_replay_range",
                            "event", event,
                            "label", label,
                            "remaining_queries", static_cast<int>(replay_node.queries_state.queries.size()),
                            "current_q_idx", replay_node.current_q_idx,
                            "range_r", replay_node.range_r,
                            "cmd_idx", replay_node.cmd_idx,
                            "range_cmd_count", static_cast<int>(replay_node.range_cmds.size()),
                            "expected_turn_sum", expected_turn_sum,
                            "actual_cmd_count", actual_cmd_count,
                            "total_replay_cmd_count", replay_cmd_count,
                            "expected_suffix_cmd_count",
                            node.score - static_cast<int>(node.past_cmds.size()),
                            "final_rot_count", replay_node.queries_state.final_rot_info.second,
                            "queries", query_oss.str());
        };
        while (!sim.done) {
            if (sim.cmd_idx == 0) {
                range_start_replay_count = replay_cmd_count;
                log_replay_range("begin", sim, -1);
            }
            if (sim.cmd_idx >= static_cast<int>(sim.range_cmds.size())) {
                debug_assert(sim, sim.current_q_idx == sim.range_r,
                             label, "replay_range_cmds_done_before_queries");
                log_replay_range("end_by_cmds_done", sim, replay_cmd_count - range_start_replay_count);
                finish_range(sim);
                set_range_and_cmds_done(sim);
                continue;
            }
            debug_assert(sim, 0 <= sim.cmd_idx, label, "replay_negative_cmd_idx");
            debug_assert(sim, sim.cmd_idx < static_cast<int>(sim.range_cmds.size()),
                         label, "replay_cmd_idx_out");
            const command cmd = sim.range_cmds[sim.cmd_idx];
            sim.st.do_cmd(cmd);
            suffix_cmds.push_back(cmd);
            replay_cmd_count++;
            const bool completed = advance_query_by_cmd(sim, cmd);
            if (completed) {
                skip_noop_a2a(sim);
                if (sim.current_q_idx == sim.range_r) {
                    log_replay_range("end_by_query_done", sim, replay_cmd_count - range_start_replay_count);
                    finish_range(sim);
                    set_range_and_cmds_done(sim);
                    continue;
                }
            }
            sim.cmd_idx++;
        }
        replay_cmd_count += final_rot_count;
        sim.queries_state.final_rot_info = final_rot_info;
        for (int i = 0; i < final_rot_count; i++) {
            suffix_cmds.push_back(final_rot_info.first);
        }
        append_final_rot(sim);
        if (replay_cmd_count != node.score - static_cast<int>(node.past_cmds.size())) {
            append_log_line(true,
                            "suffix_replay_total_mismatch",
                            "label", label,
                            "actual_replay_cmd_count", replay_cmd_count,
                            "expected_suffix_cmd_count",
                            node.score - static_cast<int>(node.past_cmds.size()),
                            "score", node.score,
                            "sum_turn", node.queries_state.sum_turn,
                            "past_cmd_count", static_cast<int>(node.past_cmds.size()),
                            "final_rot_count", final_rot_count);
        }
        debug_assert(node,
                     replay_cmd_count == node.score - static_cast<int>(node.past_cmds.size()),
                     label,
                     "suffix_replay_score_mismatch");
        debug_assert(node,
                     static_cast<int>(suffix_cmds.size()) == node.queries_state.sum_turn,
                     label,
                     "suffix_cmds_sum_turn_mismatch");
        my_vector<command> full_cmds = node.past_cmds;
        full_cmds.insert(full_cmds.end(), suffix_cmds.begin(), suffix_cmds.end());
        validate_sorted_commands(node.init_st, full_cmds);
        validate_sorted_commands(node.st, suffix_cmds);
        debug_assert(sim, sim.st.B.empty(), label, "suffix_replay_b_not_empty");
        for (int i = 0; i + 1 < sim.st.A.size(); i++) {
            debug_assert(sim, sim.st.A[i] < sim.st.A[i + 1], label, "suffix_replay_a_not_sorted");
        }
    }

    // 責務: base に candidate all_ord を適用して rebuild した状態へ commit_cmds を物理適用し、range_end_node の stack と比較する。
    // 必要な理由: 評価中に採用する current range command と次 range 開始 node が物理的に対応しているか検出するため。
    static void validate_candidate_eval(const Node& base, const CandidateEval& eval, const char* label) {
        if (!should_validate(base)) return;
        Node sim = base;
        const QueryCommonRI& target_qri = get_range_target_qri(sim);
        apply_candidate_ord(sim, target_qri, eval.candidate);
        rebuild_suffix(sim);
        set_range_and_cmds_done(sim);
        refresh_score(sim);
        for (command cmd: eval.commit_cmds) {
            sim.st.do_cmd(cmd);
        }
        debug_assert(eval.range_end_node,
                     stack_equal(sim.st.A, eval.range_end_node.st.A),
                     label,
                     "candidate_commit_stack_a_mismatch");
        debug_assert(eval.range_end_node,
                     stack_equal(sim.st.B, eval.range_end_node.st.B),
                     label,
                     "candidate_commit_stack_b_mismatch");
        validate_node_all(eval.range_end_node, label);
    }

    // 責務: score 最小、同点なら emitted command 数最小の規則で best を検査する。
    // 必要な理由: ログ対象の step 比較で tie break や候補更新が壊れた場合に即検出するため。
    static void validate_best_step(const Node& node,
                                   command current_cmd,
                                   int no_swap_score,
                                   int no_swap_emit_count,
                                   const GreedyStep& best,
                                   const std::optional<GreedyStep>& replace_ss,
                                   const std::optional<GreedyStep>& insert_sa,
                                   const std::optional<GreedyStep>& insert_sb,
                                   const std::optional<GreedyStep>& insert_ss) {
        if (!should_validate(node)) return;
        (void) current_cmd;
        int expected_score = no_swap_score;
        int expected_emit = no_swap_emit_count;
        auto update_expected = [&](const std::optional<GreedyStep>& cand) {
            if (!cand.has_value()) return;
            const int cand_emit = static_cast<int>(cand->emitted_cmds.size());
            if (cand->score < expected_score ||
                (cand->score == expected_score && cand_emit < expected_emit)) {
                expected_score = cand->score;
                expected_emit = cand_emit;
            }
        };
        update_expected(replace_ss);
        update_expected(insert_sa);
        update_expected(insert_sb);
        update_expected(insert_ss);
        log_best_step_validate(node, current_cmd, expected_score, expected_emit,
                               best, replace_ss, insert_sa, insert_sb, insert_ss);
        log_node_score_state(best.next_node, "validate_best_step_best_next");
        debug_assert(node, best.score == expected_score, "validate_best_step", "best_score_mismatch");
        debug_assert(node,
                     static_cast<int>(best.emitted_cmds.size()) == expected_emit,
                     "validate_best_step",
                     "best_emit_mismatch");
        validate_node_all(best.next_node, "validate_best_step_next");
    }

    // 責務: log_enabled が true のときだけログファイルを truncate して開始行を書く。
    // 必要な理由: 1 run のログを前回実行と混ぜず、原因調査しやすくするため。
    static void reset_log_file(bool log_enabled) {
        if (!log_enabled) return;
        std::ofstream ofs("swap_all_ord_greedy_debug.txt", std::ios::trunc);
        ofs << "swap_all_ord_greedy_log_reset" << '\n';
    }

    // 責務: log_enabled が true のときだけ可変個の値を空白区切りで追記する。
    // 必要な理由: ログ出力処理をすべて関数化し、呼び出し側をログ項目の指定だけにするため。
    template<class... Args>
    static void append_log_line(bool log_enabled, const Args&... args) {
        if (!log_enabled) return;
        std::ofstream ofs("swap_all_ord_greedy_debug.txt", std::ios::app);
        ((ofs << args << ' '), ...);
        ofs << '\n';
    }

    // 責務: node の score、past command 数、sum_turn、range/done 状態を固定形式で出力する。
    // 必要な理由: score_mismatch が発生する直前に、どの遷移で score と sum_turn がずれたか断定するため。
    static void log_node_score_state(const Node& node, const char* event) {
        append_log_line(node.log_enabled || should_validate(node),
                        "node_score_state",
                        "event", event,
                        "done", node.done,
                        "remaining_queries", static_cast<int>(node.queries_state.queries.size()),
                        "range_l", node.range_l,
                        "range_r", node.range_r,
                        "current_q_idx", node.current_q_idx,
                        "cmd_idx", node.cmd_idx,
                        "range_cmd_count", static_cast<int>(node.range_cmds.size()),
                        "score", node.score,
                        "past_cmd_count", static_cast<int>(node.past_cmds.size()),
                        "sum_turn", node.queries_state.sum_turn,
                        "expected_score",
                        static_cast<int>(node.past_cmds.size()) + node.queries_state.sum_turn,
                        "final_rot", node.queries_state.final_rot_info.second);
    }

    // 責務: no-swap 評価中の event、command、completed、score/sum_turn/range index を固定形式で出力する。
    // 必要な理由: current command 1 手の評価でどの処理が score 悪化を発生させるか断定するため。
    static void log_no_swap_state(const Node& node,
                                  const char* event,
                                  command current_cmd,
                                  bool completed) {
        append_log_line(node.log_enabled || should_validate(node),
                        "no_swap_state",
                        "event", event,
                        "cmd", cmd_str[static_cast<int>(current_cmd)],
                        "completed", completed ? 1 : 0,
                        "done", node.done,
                        "remaining_queries", static_cast<int>(node.queries_state.queries.size()),
                        "range_l", node.range_l,
                        "range_r", node.range_r,
                        "current_q_idx", node.current_q_idx,
                        "cmd_idx", node.cmd_idx,
                        "range_cmd_count", static_cast<int>(node.range_cmds.size()),
                        "score", node.score,
                        "past_cmd_count", static_cast<int>(node.past_cmds.size()),
                        "sum_turn", node.queries_state.sum_turn,
                        "expected_score",
                        static_cast<int>(node.past_cmds.size()) + node.queries_state.sum_turn,
                        "final_rot", node.queries_state.final_rot_info.second);
    }

    // 責務: best と expected、および各 step 候補の score/emitted 数を固定形式で出力する。
    // 必要な理由: validate_best_step の assert が、候補選択差分か next_node 状態差分かを断定するため。
    static void log_best_step_validate(const Node& node,
                                       command current_cmd,
                                       int expected_score,
                                       int expected_emit,
                                       const GreedyStep& best,
                                       const std::optional<GreedyStep>& replace_ss,
                                       const std::optional<GreedyStep>& insert_sa,
                                       const std::optional<GreedyStep>& insert_sb,
                                       const std::optional<GreedyStep>& insert_ss) {
        append_log_line(node.log_enabled || should_validate(node),
                        "best_step_validate",
                        "current_cmd", cmd_str[static_cast<int>(current_cmd)],
                        "base_score", node.score,
                        "base_past_cmd_count", static_cast<int>(node.past_cmds.size()),
                        "base_sum_turn", node.queries_state.sum_turn,
                        "expected_score", expected_score,
                        "expected_emit", expected_emit,
                        "best_action", cmd_str[static_cast<int>(best.action)],
                        "best_score", best.score,
                        "best_emit", static_cast<int>(best.emitted_cmds.size()),
                        "best_next_score", best.next_node.score,
                        "best_next_past_cmd_count", static_cast<int>(best.next_node.past_cmds.size()),
                        "best_next_sum_turn", best.next_node.queries_state.sum_turn,
                        "best_next_done", best.next_node.done,
                        "best_next_remaining_queries",
                        static_cast<int>(best.next_node.queries_state.queries.size()),
                        "replace_ss_score", get_step_score_or_none(replace_ss),
                        "replace_ss_emit", get_step_emit_count_or_none(replace_ss),
                        "insert_sa_score", get_step_score_or_none(insert_sa),
                        "insert_sa_emit", get_step_emit_count_or_none(insert_sa),
                        "insert_sb_score", get_step_score_or_none(insert_sb),
                        "insert_sb_emit", get_step_emit_count_or_none(insert_sb),
                        "insert_ss_score", get_step_score_or_none(insert_ss),
                        "insert_ss_emit", get_step_emit_count_or_none(insert_ss));
    }

    // 責務: 候補が存在すれば score、存在しなければ -1 を返す。
    // 必要な理由: greedy step の比較候補を固定列で出力し、欠けた候補も判別できるようにするため。
    static int get_step_score_or_none(const std::optional<GreedyStep>& step) {
        if (!step.has_value()) return -1;
        return step->score;
    }

    // 責務: 候補が存在すれば emitted_cmds 数、存在しなければ -1 を返す。
    // 必要な理由: 同 score 時の tie break 材料をログで確認できるようにするため。
    static int get_step_emit_count_or_none(const std::optional<GreedyStep>& step) {
        if (!step.has_value()) return -1;
        return static_cast<int>(step->emitted_cmds.size());
    }

    // 責務: 入力規模、初期 query 数、初期 range、初期 score をログファイルへ出力する。
    // 必要な理由: run の入口状態が想定通りかを最初に確認できるようにするため。
    static void log_run_start(bool log_enabled,
                              const State& init_st,
                              const YakinamashiState<MAX_N>& yst,
                              const Node& node) {
        append_log_line(log_enabled,
                        "run_start",
                        "N", init_st.current_N,
                        "query_count", static_cast<int>(yst.queries_state.queries.size()),
                        "range_r", node.range_r,
                        "range_cmd_count", static_cast<int>(node.range_cmds.size()),
                        "score", node.score,
                        "sum_turn", node.queries_state.sum_turn,
                        "final_rot", node.queries_state.final_rot_info.second);
    }

    // 責務: 最終 command 数、score、final rot をログファイルへ出力する。
    // 必要な理由: run の返り値と内部 score の対応を確認できるようにするため。
    static void log_run_end(bool log_enabled, const Node& node) {
        append_log_line(log_enabled,
                        "run_end",
                        "cmd_count", static_cast<int>(node.past_cmds.size()),
                        "score", node.score,
                        "sum_turn", node.queries_state.sum_turn,
                        "final_rot", node.queries_state.final_rot_info.second);
    }

    // 責務: 初期 command 数、最終 command 数、差分、最終 command 列を OutPut/final_swap_all_ord_result.txt に上書き出力する。
    // 必要な理由: 調査ログとは別に、GreedySwap の入力手数と出力 command 列を毎回確認できるようにするため。
    static void write_final_result(const State& init_st,
                                   const YakinamashiState<MAX_N>& yst,
                                   const Node& node) {
        const my_vector<command> initial_cmds =
                YstCommandsBuilder::build_all_commands_from_state(init_st, yst.queries_state);
        const int initial_cmd_count = static_cast<int>(initial_cmds.size());
        const int final_cmd_count = static_cast<int>(node.past_cmds.size());
        std::filesystem::create_directories(output_dir);
        std::ofstream ofs(output_dir / "final_swap_all_ord_result.txt", std::ios::trunc);
        if (!ofs.is_open()) return;
        ofs << "initial_cmd_count " << initial_cmd_count << '\n';
        ofs << "final_cmd_count " << final_cmd_count << '\n';
        ofs << "diff " << final_cmd_count - initial_cmd_count << '\n';
        ofs << "commands";
        for (command cmd: node.past_cmds) {
            ofs << ' ' << cmd_str[static_cast<int>(cmd)];
        }
        ofs << '\n';
    }

    // 責務: range/query/cmd の位置、target query 種別、target value、score を出力する。
    // 必要な理由: どの range で候補評価または plain commit に進んだか追跡するため。
    static void log_range_start(const Node& node) {
        if (!node.log_enabled) return;
        int q_type = -1;
        int target_v = -1;
        if (!node.done && node.current_q_idx < node.range_r) {
            const QueryCommonRI& qri = node.queries_state.queries[node.range_r - 1];
            q_type = qri.is_a2b_type() ? 1 : (qri.is_b2a_type() ? 2 : 3);
            target_v = qri.get_tar_v();
        }
        append_log_line(node.log_enabled,
                        "range_start",
                        "remaining_queries", static_cast<int>(node.queries_state.queries.size()),
                        "range_l", node.range_l,
                        "range_r", node.range_r,
                        "current_q_idx", node.current_q_idx,
                        "cmd_idx", node.cmd_idx,
                        "range_cmd_count", static_cast<int>(node.range_cmds.size()),
                        "target_type", q_type,
                        "target_v", target_v,
                        "score", node.score,
                        "sum_turn", node.queries_state.sum_turn);
    }

    // 責務: current range の候補数と候補 all_ord を出力する。
    // 必要な理由: 候補列挙の過不足や重複除去の結果を確認できるようにするため。
    static void log_candidates(const Node& node, const my_vector<AllOrdCandidate>& candidates) {
        if (!node.log_enabled) return;
        std::ostringstream oss;
        std::ostringstream no_change_oss;
        for (int i = 0; i < static_cast<int>(candidates.size()); i++) {
            if (i > 0) oss << ',';
            if (i > 0) no_change_oss << ',';
            no_change_oss << (candidates[i].no_change ? 1 : 0);
            oss << candidates[i].all_ord.get();
        }
        append_log_line(node.log_enabled,
                        "candidates",
                        "range_r", node.range_r,
                        "count", static_cast<int>(candidates.size()),
                        "no_change_flags", no_change_oss.str(),
                        "all_ords", oss.str());
    }

    // 責務: before 候補ごとに重複除外か valid 判定結果かを出力する。
    // 必要な理由: 変更候補が 0 件になったとき、どの before_ord がなぜ候補から外れたか確認するため。
    static void log_b2a_candidate_scan(const Node& node,
                                       int target_v,
                                       int stack_a_idx,
                                       int before_v,
                                       AllOrd before_ord,
                                       bool duplicate_skip,
                                       bool valid) {
        append_log_line(node.log_enabled,
                        "b2a_candidate_scan",
                        "remaining_queries", static_cast<int>(node.queries_state.queries.size()),
                        "range_r", node.range_r,
                        "current_q_idx", node.current_q_idx,
                        "target_v", target_v,
                        "stack_a_size", node.st.A.size(),
                        "stack_a_idx", stack_a_idx,
                        "before_v", before_v,
                        "before_val_state", node.val_a[before_v],
                        "before_ord", before_ord.get(),
                        "duplicate_skip", duplicate_skip ? 1 : 0,
                        "valid", valid ? 1 : 0);
    }

    // 責務: 重複除外数、valid 除外数、最終候補数を出力し、0 件時は専用行も出す。
    // 必要な理由: all_ord を変える候補が 0 件になった状況を assert 直前のログで判別するため。
    static void log_b2a_candidate_summary(const Node& node,
                                          int target_v,
                                          int duplicate_skip_count,
                                          int invalid_skip_count,
                                          int change_candidate_count) {
        append_log_line(node.log_enabled,
                        "b2a_candidate_summary",
                        "remaining_queries", static_cast<int>(node.queries_state.queries.size()),
                        "range_r", node.range_r,
                        "current_q_idx", node.current_q_idx,
                        "target_v", target_v,
                        "stack_a_size", node.st.A.size(),
                        "no_change_included", 1,
                        "duplicate_skip_count", duplicate_skip_count,
                        "invalid_skip_count", invalid_skip_count,
                        "change_candidate_count", change_candidate_count,
                        "total_candidate_count", change_candidate_count + 1);
        if (change_candidate_count != 0) return;
        append_log_line(node.log_enabled,
                        "b2a_candidate_zero",
                        "target_v", target_v,
                        "stack_a_size", node.st.A.size(),
                        "no_change_included", 1,
                        "duplicate_skip_count", duplicate_skip_count,
                        "invalid_skip_count", invalid_skip_count);
    }

    // 責務: 評価対象 all_ord と評価開始時の score/range を出力する。
    // 必要な理由: candidate 評価中の rebuild/greedy step ログを候補に紐づけるため。
    static void log_candidate_start(const Node& node, const AllOrdCandidate& candidate) {
        append_log_line(node.log_enabled,
                        "candidate_start",
                        "no_change", candidate.no_change ? 1 : 0,
                        "all_ord", candidate.all_ord.get(),
                        "range_r", node.range_r,
                        "score", node.score,
                        "sum_turn", node.queries_state.sum_turn,
                        "past_cmd_count", static_cast<int>(node.past_cmds.size()));
    }

    // 責務: 候補 all_ord の評価 score と current range commit command 数を出力する。
    // 必要な理由: 候補間の score 比較と採用根拠を確認できるようにするため。
    static void log_candidate(const Node& node, const CandidateEval& eval) {
        append_log_line(node.log_enabled,
            "candidate_end",
            "no_change", eval.candidate.no_change ? 1 : 0,
            "all_ord", eval.candidate.all_ord.get(),
            "score", eval.score,
            "commit_cmd_count", static_cast<int>(eval.commit_cmds.size()),
            "range_end_score", eval.range_end_node.score,
            "range_end_sum_turn", eval.range_end_node.queries_state.sum_turn,
            "range_end_remaining_queries", static_cast<int>(eval.range_end_node.queries_state.queries.size()));
    }

    // 責務: 採用 all_ord と commit 後状態を出力する。
    // 必要な理由: greedy な all_ord 採用結果と次 range 開始状態を確認できるようにするため。
    static void log_commit(const Node& node, const CandidateEval& eval) {
        append_log_line(node.log_enabled,
            "commit",
            "no_change", eval.candidate.no_change ? 1 : 0,
            "all_ord", eval.candidate.all_ord.get(),
            "score", eval.score,
            "commit_cmd_count", static_cast<int>(eval.commit_cmds.size()),
            "next_remaining_queries", static_cast<int>(eval.range_end_node.queries_state.queries.size()),
            "next_score", eval.range_end_node.score,
            "next_sum_turn", eval.range_end_node.queries_state.sum_turn);
    }

    // 責務: all_ord 候補なしでそのまま commit する range の位置と command 数を出力する。
    // 必要な理由: 候補評価を通らない range で状態が進む箇所を追えるようにするため。
    static void log_plain_commit(const Node& node) {
        append_log_line(node.log_enabled,
                        "plain_commit",
                        "range_r", node.range_r,
                        "current_q_idx", node.current_q_idx,
                        "cmd_idx", node.cmd_idx,
                        "range_cmd_count", static_cast<int>(node.range_cmds.size()),
                        "score", node.score,
                        "sum_turn", node.queries_state.sum_turn);
    }

    // 責務: current command、各候補 score、選択 action、選択 score、tie break 材料を出力する。
    // 必要な理由: swap 挿入/ss 置換の採用または不採用の原因を command 位置ごとに特定するため。
    static void log_greedy_step(const Node& node,
                                command current_cmd,
                                int no_swap_score,
                                int no_swap_emit_count,
                                const GreedyStep& best,
                                const std::optional<GreedyStep>& replace_ss,
                                const std::optional<GreedyStep>& insert_sa,
                                const std::optional<GreedyStep>& insert_sb,
                                const std::optional<GreedyStep>& insert_ss) {
        append_log_line(node.log_enabled,
                        "greedy_step",
                        "range_r", node.range_r,
                        "current_q_idx", node.current_q_idx,
                        "cmd_idx", node.cmd_idx,
                        "current_cmd", current_cmd,
                        "base_score", node.score,
                        "no_swap_score", no_swap_score,
                        "replace_ss_score", get_step_score_or_none(replace_ss),
                        "insert_sa_score", get_step_score_or_none(insert_sa),
                        "insert_sb_score", get_step_score_or_none(insert_sb),
                        "insert_ss_score", get_step_score_or_none(insert_ss),
                        "no_swap_emit", no_swap_emit_count,
                        "replace_ss_emit", get_step_emit_count_or_none(replace_ss),
                        "insert_sa_emit", get_step_emit_count_or_none(insert_sa),
                        "insert_sb_emit", get_step_emit_count_or_none(insert_sb),
                        "insert_ss_emit", get_step_emit_count_or_none(insert_ss),
                        "selected_action", best.action,
                        "selected_score", best.score,
                        "selected_emit", static_cast<int>(best.emitted_cmds.size()),
                        "next_remaining_queries", static_cast<int>(best.next_node.queries_state.queries.size()),
                        "next_sum_turn", best.next_node.queries_state.sum_turn);
    }

    // 責務: rebuild 前後の query 数、sum_turn、final_rot、past command 数を出力する。
    // 必要な理由: swap や all_ord 候補適用後の suffix 再生成がどの規模で起きたか確認するため。
    static void log_rebuild(const Node& node, int old_suffix_count, int rebuilt_count) {
        append_log_line(node.log_enabled,
                        "rebuild",
                        "old_suffix_count", old_suffix_count,
                        "rebuilt_count", rebuilt_count,
                        "remaining_queries", static_cast<int>(node.queries_state.queries.size()),
                        "sum_turn", node.queries_state.sum_turn,
                        "final_rot", node.queries_state.final_rot_info.second,
                        "past_cmd_count", static_cast<int>(node.past_cmds.size()));
    }
};
