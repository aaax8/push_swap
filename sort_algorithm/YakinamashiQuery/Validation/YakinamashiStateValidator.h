#pragma once

#include "State.h"
#include "QueriesState.h"
#include "YakinamashiState.h"
#include "YQAliases.h"
#include "QueryListOrdUtil.h"
#include "StackOrdUtil.h"
#include "QueryStateApplyUtil.h"
#include "YstCommandsBuilder.h"
#include "FinalRotCalculator.h"
#include <fstream>
#include <algorithm>

template<size_t MAX_N>
class QueriesStateValidator {
public:
    template<typename QueryRIList>
    static void validate_finished_top(int N,
                                      const QueryRIList &yst_queries,
                                      State &st,
                                      AOrder &aorder,
                                      BOrder &border,
                                      my_vector<ValState> &now_a_val_state,
                                      my_vector<ValState> &now_b_val_state) {
#ifndef DEBUG
        return;
#endif
        auto [a_ords, b_ords] = StackOrdUtil::get_stack_ord<MAX_N>(st, aorder, border, now_a_val_state, now_b_val_state);
        OrdPos my_top_ord_a = QueryListOrdUtil::get_finished_top_ord_a(N, yst_queries, st, a_ords);
        OrdPos my_top_ord_b = QueryListOrdUtil::get_finished_top_ord_b(N, yst_queries, st);
        my_assert(my_top_ord_a.get() == get_top_ord(a_ords));
        my_assert(my_top_ord_b.get() == get_top_ord(b_ords));
    }

    static void validate_query_order(const State &initial_state, const QRIList &queries) {
#ifndef DEBUG
        return;
#endif
        int N = initial_state.current_N;
        int initial_N = initial_state.initial_N;
        State now_st(initial_state);
        my_vector<ValState> now_a_val_state(initial_N, ValState::NONE);
        my_vector<ValState> now_b_val_state(initial_N, ValState::NONE);
        init_value_states(now_st, now_a_val_state, now_b_val_state);
        QueriesCalculator qs_calc(N, queries);

        for (int q_idx = 0; q_idx < queries.size(); ++q_idx) {
            const auto &qri = queries[q_idx];
            int v = qri.get_tar_v();
            my_assert(0 <= v && v < initial_N);
            validate_query_preconditions(qri, now_st, now_a_val_state, now_b_val_state);
            QueryStateApplyUtil::apply_query_to_state(now_st, qs_calc, q_idx, qri);
            apply_query_to_value_states(qri, now_a_val_state, now_b_val_state);
        }
    }

    static void validate_state(const State &initial_state, const QueriesState &state) {
#ifndef DEBUG
        return;
#endif
        validate_sum_turn(state);
        validate_query_order(initial_state, state.queries);
        validate_commands_can_sort(initial_state, state);
    }

    // 責務: YakinamashiState 全体を受け取り、既存検証に加えて sort 失敗時の yst ログ出力経路へ渡す。
    // 必要な理由: QueriesState だけでは AOrder/BOrder entries を出力できず、TARGET の線形順序を失敗時に確認できないため。
    static void validate_state(const State &initial_state, const YakinamashiState<MAX_N> &yst) {
#ifndef DEBUG
        return;
#endif
        validate_sum_turn(yst.queries_state);
        validate_query_order(initial_state, yst.queries_state.queries);
        validate_commands_can_sort(initial_state, yst);
    }

    static void validate_query_top_transfer(const State &initial_state, const QRIList &queries) {
#ifndef DEBUG
        return;
#endif
        State now_st(initial_state);
        QueriesCalculator qs_calc(initial_state.current_N, queries);
        for (int q_idx = 0; q_idx < static_cast<int>(queries.size()); ++q_idx) {
            const auto &qri = queries[q_idx];
            const int v = qri.get_tar_v();
            State before_q(now_st);
            State before_push(now_st);
            BestRotInfo bri{NO_COMMAND, NO_COMMAND, 0, 0, 0};
            if (qri.is_ab_type()) {
                bri = QueryStateApplyUtil::build_ab_best_rot_info_from_orig(qs_calc, q_idx, qri);
                QueryStateApplyUtil::apply_ab_rot_to_state(before_push, bri);
            }
            QueryStateApplyUtil::apply_query_to_state(now_st, qs_calc, q_idx, qri);
            if (qri.is_a2b_type()) {
                if (now_st.B.empty() || now_st.B[0] != v) {
                    write_top_transfer_debug(initial_state, queries, q_idx, qri, before_q, before_push, now_st, bri);
                }
                my_assert(!now_st.B.empty());
                my_assert(now_st.B[0] == v);
            } else if (qri.is_b2a_type()) {
                if (now_st.A.empty() || now_st.A[0] != v) {
                    write_top_transfer_debug(initial_state, queries, q_idx, qri, before_q, before_push, now_st, bri);
                }
                my_assert(!now_st.A.empty());
                my_assert(now_st.A[0] == v);
            }
        }
    }

        static void validate_original_commands_length(const QueriesState &state,
                                                      const my_vector<command> &all_cmds) {
    #ifndef DEBUG
        return;
    #endif
            my_assert(static_cast<int>(all_cmds.size()) == state.sum_turn);
        }

    static void validate_sum_turn(const QueriesState &state) {
        int sum_turn = 0;
        for (const auto &qri: state.queries) {
            sum_turn += qri.get_ins_turn();
        }
        my_assert(state.final_rot_info.second >= 0);
        sum_turn += state.final_rot_info.second;
        my_assert(sum_turn == state.sum_turn);
    }

private:
    // 責務: initial_state に含まれる値集合内で value の昇順 rank が expected_idx と一致するか返す。
    // 必要な理由: physical destroy 後は値が 0..current_N-1 とは限らないため、元値ログを保ったまま圧縮後の並びで検証するため。
    static bool is_compressed_sorted_value(const State &initial_state, int value, int expected_idx) {
        my_vector<int> values;
        values.reserve(initial_state.A.size() + initial_state.B.size());
        for (int i = 0; i < initial_state.A.size(); i++) values.push_back(initial_state.A[i]);
        for (int i = 0; i < initial_state.B.size(); i++) values.push_back(initial_state.B[i]);
        std::sort(values.begin(), values.end());
        auto it = std::lower_bound(values.begin(), values.end(), value);
        my_assert(it != values.end());
        my_assert(*it == value);
        return static_cast<int>(it - values.begin()) == expected_idx;
    }

    // 責務: validate_query_top_transfer の失敗詳細を書き出すファイル名を保持する。
    // 必要な理由: rotate ずれ調査用ログを既存の sort 失敗ログと分けて確認できるようにするため。
    static constexpr const char *TOP_TRANSFER_DEBUG_PATH = "query_top_transfer_debug.txt";

    // 責務: validate_commands_can_sort の失敗時に query 単位比較を書き出すファイル名を保持する。
    // 必要な理由: 既存の greedy_failure_debug.txt と混ぜず、実コマンド適用と仮想適用の最初の不一致を確認するため。
    static constexpr const char *QUERY_COMPARE_DEBUG_PATH = "query_compare_debug.txt";

    // 責務: stack 内で v が存在する index を返し、存在しない場合は -1 を返す。
    // 必要な理由: v は正しい stack にあるが rotate 回数がずれているケースを特定するため。
    static int find_value_pos(const RingBuffer500 &stack, int v) {
        for (int i = 0; i < stack.size(); ++i) {
            if (stack[i] == v) {
                return i;
            }
        }
        return -1;
    }

    // 責務: label 付きで RingBuffer500 の全要素を 1 行へ出力する。
    // 必要な理由: rotate 前、push 前、push 後の top と並びを直接比較するため。
    static void write_stack_debug(std::ofstream &ofs, const char *label, const RingBuffer500 &stack) {
        ofs << label << "=[";
        for (int i = 0; i < stack.size(); i++) {
            if (i != 0) ofs << ",";
            ofs << stack[i];
        }
        ofs << "]\n";
    }

    // 責務: stack 内で v が存在する index を返し、存在しない場合は -1 を返す。
    // 必要な理由: final rot の期待値と実際の A 上の 0 の位置を比較するため。
    static int find_value_index_in_stack(const RingBuffer500 &stack, int v) {
        for (int i = 0; i < stack.size(); ++i) {
            if (stack[i] == v) {
                return i;
            }
        }
        return -1;
    }

    // 責務: A を巡回昇順として見た時に隣接順序が壊れている最初の index を返す。
    // 必要な理由: どの query 後に 17,16 のような逆転が初めて発生したかを特定するため。
    static int find_first_a_order_break(const State &st) {
        if (st.A.empty()) return -1;
        const int n = st.current_N;
        for (int i = 0; i < st.A.size(); i++) {
            const int current_v = st.A[i];
            const int next_v = st.A[(i + 1) % st.A.size()];
            if ((current_v + 1) % n != next_v) {
                return i;
            }
        }
        return -1;
    }

    // 責務: label 付きで stack の中身を 1 行へ出力する。
    // 必要な理由: query 前後の A/B を grep しやすい形式で確認するため。
    static void write_stack_debug_line(std::ofstream &ofs, const char *label, const RingBuffer500 &stack) {
        ofs << label << "=";
        for (int i = 0; i < stack.size(); i++) {
            if (i != 0) ofs << ",";
            ofs << stack[i];
        }
        ofs << '\n';
    }

    // 責務: label 付きで State の N と A/B stack を出力する。
    // 必要な理由: どの段階で top がずれたかを state 単位で比較するため。
    static void write_state_debug(std::ofstream &ofs, const char *label, const State &st) {
        ofs << label << "_current_N=" << st.current_N << '\n';
        ofs << label << "_initial_N=" << st.initial_N << '\n';
        ofs << label << "_";
        write_stack_debug(ofs, "A", st.A);
        ofs << label << "_";
        write_stack_debug(ofs, "B", st.B);
    }

    // 責務: State の A/B stack の値並びが完全一致するかを返す。
    // 必要な理由: 実コマンド適用と query 仮想適用が最初に食い違う query を機械的に特定するため。
    static bool same_state_stack(const State &lhs, const State &rhs) {
        if (lhs.A.size() != rhs.A.size() || lhs.B.size() != rhs.B.size()) {
            return false;
        }
        for (int i = 0; i < lhs.A.size(); i++) {
            if (lhs.A[i] != rhs.A[i]) {
                return false;
            }
        }
        for (int i = 0; i < lhs.B.size(); i++) {
            if (lhs.B[i] != rhs.B[i]) {
                return false;
            }
        }
        return true;
    }

    // 責務: label 付きで OrderEntry 列を index:value:state 形式で出力する。
    // 必要な理由: TARGET(0) の位置や TARGET 線形順序を failure log だけで確認できるようにするため。
    static void write_order_entries_debug(std::ofstream &ofs,
                                          const char *label,
                                          const my_vector<OrderEntry> &entries) {
        ofs << label << "=[";
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            if (i != 0) ofs << ",";
            ofs << i << ":" << entries[i];
        }
        ofs << "]\n";
    }

    // 責務: QueriesState と AOrder/BOrder entries を greedy_failure_debug.txt へ出力する。
    // 必要な理由: sort 失敗が query/final rot 由来か、order entry の線形順序由来かを切り分けるため。
    static void write_yst_failure_debug(const YakinamashiState<MAX_N> &yst) {
        std::ofstream ofs("greedy_failure_debug.txt", std::ios::app);
        if (!ofs.is_open()) return;
        ofs << "=== yst_failure_state ===\n";
        ofs << "query_count=" << yst.queries_state.queries.size() << '\n';
        ofs << "sum_turn=" << yst.queries_state.sum_turn << '\n';
        ofs << "final_rot_cmd=" << yst.queries_state.final_rot_info.first << '\n';
        ofs << "final_rot_cnt=" << yst.queries_state.final_rot_info.second << '\n';
        write_order_entries_debug(ofs, "AOrderEntries", yst.query_order.aorder.get_all_order_entries());
        write_order_entries_debug(ofs, "BOrderEntries", yst.query_order.border.get_all_order_entries());
        ofs << '\n';
    }

    // 責務: 比較ログのヘッダとして N、初期 A、query 数、sum_turn、final_rot_info を出力する。
    // 必要な理由: 失敗ケースの前提条件を他ログなしで確認できるようにするため。
    static void write_query_compare_header(std::ofstream &ofs,
                                           const State &initial_state,
                                           const QueriesState &state) {
        ofs << "=== query_compare_debug ===\n";
        ofs << "N=" << initial_state.current_N << '\n';
        write_stack_debug_line(ofs, "initial_A", initial_state.A);
        write_stack_debug_line(ofs, "initial_B", initial_state.B);
        ofs << "query_count=" << state.queries.size() << '\n';
        ofs << "sum_turn=" << state.sum_turn << '\n';
        ofs << "final_rot_cmd=" << state.final_rot_info.first << '\n';
        ofs << "final_rot_cnt=" << state.final_rot_info.second << '\n';
    }

    // 責務: 1 query 分の qri、開始/終了 State、対応コマンド範囲、A2A 詳細を出力する。
    // 必要な理由: query 作成、コマンド生成、仮想適用のどこで食い違うかを query 単位で確認するため。
    static void write_query_compare_step(std::ofstream &ofs,
                                         int q_idx,
                                         const QueryCommonRI &qri,
                                         const State &real_before,
                                         const State &virtual_before,
                                         const State &real_after,
                                         const State &virtual_after,
                                         int cmd_begin,
                                         int cmd_count) {
        ofs << "q_idx=" << q_idx << '\n';
        ofs << "qri=" << qri << '\n';
        ofs << "cmd_begin=" << cmd_begin << '\n';
        ofs << "cmd_count=" << cmd_count << '\n';
        ofs << "same_before=" << same_state_stack(real_before, virtual_before) << '\n';
        ofs << "same_after=" << same_state_stack(real_after, virtual_after) << '\n';
        if (qri.is_a2a_type()) {
            ofs << "a2a_flip_dis_dir=" << qri.get_flip_dis_dir_A2A() << '\n';
            ofs << "a2a_rot_a1=" << qri.get_cmd_rot_a1_A2A() << '\n';
            ofs << "a2a_rot_a1_cnt=" << qri.get_cmd_rot_a1_cnt_A2A() << '\n';
            ofs << "a2a_finished_top_ord_a=" << qri.get_finished_top_ord_a().get() << '\n';
            ofs << "a2a_ins_turn=" << qri.get_ins_turn() << '\n';
        }
        write_state_debug(ofs, "real_before", real_before);
        write_state_debug(ofs, "virtual_before", virtual_before);
        write_state_debug(ofs, "real_after", real_after);
        write_state_debug(ofs, "virtual_after", virtual_after);
    }

    // 責務: 各 query の ins_turn ごとに実コマンドを適用した State と、QueryStateApplyUtil による仮想 State を比較し、最初の不一致を出力する。
    // 必要な理由: validate_query_order は通るが実コマンド列で未ソートになるケースの原因を切り分けるため。
    static void write_query_compare_debug(const State &initial_state,
                                          const QueriesState &state,
                                          const my_vector<command> &query_cmds) {
        std::ofstream ofs(QUERY_COMPARE_DEBUG_PATH, std::ios::app);
        if (!ofs.is_open()) return;
        write_query_compare_header(ofs, initial_state, state);
        State real_state(initial_state);
        State virtual_state(initial_state);
        QueriesCalculator qs_calc(initial_state.current_N, state.queries);
        int cmd_offset = 0;
        for (int q_idx = 0; q_idx < static_cast<int>(state.queries.size()); q_idx++) {
            const QueryCommonRI &qri = state.queries[q_idx];
            const int turn = qri.get_ins_turn();
            State real_before(real_state);
            State virtual_before(virtual_state);
            for (int local_i = 0; local_i < turn; local_i++) {
                const int cmd_idx = cmd_offset + local_i;
                my_assert(0 <= cmd_idx && cmd_idx < static_cast<int>(query_cmds.size()));
                command cmd = query_cmds[cmd_idx];
                my_assert(real_state.can_do(cmd));
                real_state.do_cmd(cmd);
            }
            QueryStateApplyUtil::apply_query_to_state(virtual_state, qs_calc, q_idx, qri);
            const bool same_before = same_state_stack(real_before, virtual_before);
            const bool same_after = same_state_stack(real_state, virtual_state);
            if (!same_before || !same_after) {
                ofs << "first_mismatch_q_idx=" << q_idx << '\n';
                ofs << "query_commands=";
                for (int local_i = 0; local_i < turn; local_i++) {
                    if (local_i != 0) ofs << " ";
                    ofs << query_cmds[cmd_offset + local_i];
                }
                ofs << '\n';
                write_query_compare_step(ofs, q_idx, qri, real_before, virtual_before,
                                         real_state, virtual_state, cmd_offset, turn);
                write_query_window_debug(ofs, state.queries, q_idx);
                ofs << '\n';
                return;
            }
            cmd_offset += turn;
        }
        ofs << "first_mismatch_q_idx=-1\n";
        ofs << "consumed_query_cmds=" << cmd_offset << '\n';
        ofs << "query_cmds_size=" << query_cmds.size() << '\n';
        write_state_debug(ofs, "real_final", real_state);
        write_state_debug(ofs, "virtual_final", virtual_state);
        ofs << '\n';
    }

    // 責務: center_idx の前後 query を QueryCommonRI の文字列表現で出力する。
    // 必要な理由: 直前 query や A2A の finished top が rotate ずれに影響したか確認するため。
    static void write_query_window_debug(std::ofstream &ofs, const QRIList &queries, int center_idx) {
        int begin_idx = std::max(0, center_idx - 5);
        int end_idx = std::min(static_cast<int>(queries.size()), center_idx + 6);
        ofs << "query_window_begin=" << begin_idx << '\n';
        ofs << "query_window_end=" << end_idx << '\n';
        for (int i = begin_idx; i < end_idx; i++) {
            ofs << "query_window_q[" << i << "]=" << queries[i] << '\n';
        }
    }

    // 責務: query_cmds を各 qri の ins_turn ごとに適用し、A の巡回順序・0位置・finished_top_ord_a をログ出力する。
    // 必要な理由: 最終状態だけでは entry/order 崩れがどの query 後に表面化したか分からないため。
    static void write_query_replay_debug(const State &initial_state,
                                         const QueriesState &state,
                                         const my_vector<command> &query_cmds) {
        std::ofstream ofs("greedy_failure_debug.txt", std::ios::app);
        if (!ofs.is_open()) return;
        ofs << "=== sort_failure_query_replay ===\n";
        State replay_state(initial_state);
        int cmd_offset = 0;
        int first_break_idx = -1;
        for (int q_idx = 0; q_idx < static_cast<int>(state.queries.size()); q_idx++) {
            const QueryCommonRI &qri = state.queries[q_idx];
            const int turn = qri.get_ins_turn();
            State before_q(replay_state);
            for (int local_i = 0; local_i < turn; local_i++) {
                const int cmd_idx = cmd_offset + local_i;
                my_assert(0 <= cmd_idx && cmd_idx < static_cast<int>(query_cmds.size()));
                command cmd = query_cmds[cmd_idx];
                if (!replay_state.can_do(cmd)) {
                    ofs << "replay_can_do_failed_q_idx=" << q_idx << '\n';
                    ofs << "replay_can_do_failed_cmd_idx=" << cmd_idx << '\n';
                    ofs << "replay_can_do_failed_cmd=" << cmd << '\n';
                    write_stack_debug_line(ofs, "replay_failed_A", replay_state.A);
                    write_stack_debug_line(ofs, "replay_failed_B", replay_state.B);
                    return;
                }
                replay_state.do_cmd(cmd);
            }
            const int break_idx = find_first_a_order_break(replay_state);
            ofs << "replay_q_idx=" << q_idx
                << " qri=" << qri
                << " cmd_begin=" << cmd_offset
                << " cmd_count=" << turn
                << " a_top=" << (replay_state.A.empty() ? -1 : replay_state.A[0])
                << " zero_index=" << find_value_index_in_stack(replay_state.A, 0)
                << " a_order_break=" << break_idx
                << " finished_top_ord_a=" << qri.get_finished_top_ord_a().get()
                << '\n';
            if (first_break_idx == -1 && break_idx != -1) {
                first_break_idx = q_idx;
                ofs << "first_a_order_break_q_idx=" << q_idx << '\n';
                ofs << "first_a_order_break_query=" << qri << '\n';
                write_stack_debug_line(ofs, "first_break_before_A", before_q.A);
                write_stack_debug_line(ofs, "first_break_before_B", before_q.B);
                write_stack_debug_line(ofs, "first_break_after_A", replay_state.A);
                write_stack_debug_line(ofs, "first_break_after_B", replay_state.B);
                ofs << "first_break_query_commands=";
                for (int local_i = 0; local_i < turn; local_i++) {
                    if (local_i != 0) ofs << " ";
                    ofs << query_cmds[cmd_offset + local_i];
                }
                ofs << '\n';
                write_query_window_debug(ofs, state.queries, q_idx);
            }
            cmd_offset += turn;
        }
        ofs << "replay_consumed_query_cmds=" << cmd_offset << '\n';
        ofs << "query_cmds_size=" << query_cmds.size() << '\n';
        ofs << "final_query_zero_index=" << find_value_index_in_stack(replay_state.A, 0) << '\n';
        ofs << "state_final_rot_cmd=" << state.final_rot_info.first << '\n';
        ofs << "state_final_rot_cnt=" << state.final_rot_info.second << '\n';
        write_stack_debug_line(ofs, "final_query_A", replay_state.A);
        write_stack_debug_line(ofs, "final_query_B", replay_state.B);
        ofs << '\n';
    }

    // 責務: top transfer 失敗時に q_idx、query、実 rotate、期待 rotate、前後 state を専用ログへ出力する。
    // 必要な理由: ord 作成のずれか QueriesCalculator の cmd/cnt 算出ずれかをログだけで切り分けるため。
    static void write_top_transfer_debug(const State &initial_state,
                                         const QRIList &queries,
                                         int q_idx,
                                         const QueryCommonRI &qri,
                                         const State &before_q,
                                         const State &before_push,
                                         const State &after_q,
                                         const BestRotInfo &bri) {
        std::ofstream ofs(TOP_TRANSFER_DEBUG_PATH, std::ios::app);
        if (!ofs.is_open()) return;
        const int v = qri.get_tar_v();
        const bool is_a2b = qri.is_a2b_type();
        const RingBuffer500 &source_stack = is_a2b ? before_q.A : before_q.B;
        const int source_pos = find_value_pos(source_stack, v);
        const int source_size = source_stack.size();
        const int expect_forward_cnt = source_pos < 0 ? -1 : source_pos;
        const int expect_reverse_cnt = source_pos < 0 ? -1 : (source_size - source_pos) % source_size;
        const command used_source_cmd = is_a2b ? bri.cmd_a : bri.cmd_b;
        const int used_source_cnt = is_a2b ? bri.cmd_a_cnt : bri.cmd_b_cnt;
        const int expected_used_cnt =
                (used_source_cmd == RA || used_source_cmd == RB) ? expect_forward_cnt : expect_reverse_cnt;
        ofs << "=== query_top_transfer_failure ===\n";
        ofs << "N=" << initial_state.current_N << '\n';
        ofs << "q_idx=" << q_idx << '\n';
        ofs << "qri=" << qri << '\n';
        ofs << "v=" << v << '\n';
        ofs << "source_stack=" << (is_a2b ? "A" : "B") << '\n';
        ofs << "source_pos=" << source_pos << '\n';
        ofs << "source_size=" << source_size << '\n';
        ofs << "expect_forward_cnt=" << expect_forward_cnt << '\n';
        ofs << "expect_reverse_cnt=" << expect_reverse_cnt << '\n';
        ofs << "used_source_cmd=" << used_source_cmd << '\n';
        ofs << "used_source_cnt=" << used_source_cnt << '\n';
        ofs << "expected_used_cnt=" << expected_used_cnt << '\n';
        ofs << "used_cnt_matches_expected=" << (used_source_cnt == expected_used_cnt) << '\n';
        ofs << "bri_cmd_a=" << bri.cmd_a << '\n';
        ofs << "bri_cmd_a_cnt=" << bri.cmd_a_cnt << '\n';
        ofs << "bri_cmd_b=" << bri.cmd_b << '\n';
        ofs << "bri_cmd_b_cnt=" << bri.cmd_b_cnt << '\n';
        ofs << "bri_ins_turn=" << bri.ins_turn << '\n';
        if (is_a2b) {
            ofs << "query_tar_ord_pos_a=" << qri.get_query().get_tar_ord_pos_a_A2B().get() << '\n';
            ofs << "query_tar_rel_ord_b=" << qri.get_query().get_tar_rel_ord_b_A2B().get() << '\n';
            ofs << "after_push_top_B=" << (after_q.B.empty() ? -1 : after_q.B[0]) << '\n';
        } else {
            ofs << "query_tar_rel_ord_a=" << qri.get_query().get_tar_rel_ord_a_B2A().get() << '\n';
            ofs << "query_tar_ord_pos_b=" << qri.get_query().get_tar_ord_pos_b_B2A().get() << '\n';
            ofs << "after_push_top_A=" << (after_q.A.empty() ? -1 : after_q.A[0]) << '\n';
        }
        write_state_debug(ofs, "before_q", before_q);
        write_state_debug(ofs, "before_push", before_push);
        write_state_debug(ofs, "after_q", after_q);
        write_query_window_debug(ofs, queries, q_idx);
        ofs << '\n';
    }

    // : final rot と query 実行直後のAを並べて保存し、最終回転の計算ずれか command 展開ずれかを切り分けやすくする。
    static void write_sort_failure_debug(const State &initial_state,
                                         const QueriesState &state,
                                         const my_vector<command> &cmds,
                                         const my_vector<command> &query_cmds,
                                         const my_vector<command> &prev_query_cmds,
                                         const State &before_last_query,
                                         const State &after_queries,
                                         const State &restored,
                                         const QueriesState::FinalRotInfo &rebuild_final_rot) {
        std::ofstream ofs("greedy_failure_debug.txt", std::ios::app);
        if (!ofs.is_open()) return;
        ofs << "=== sort_failure ===\n";
        ofs << "N=" << initial_state.current_N << '\n';
        ofs << "initial_A=";
        for (int i = 0; i < initial_state.A.size(); i++) {
            if (i != 0) ofs << ",";
            ofs << initial_state.A[i];
        }
        ofs << "\nafter_queries_A=";
        for (int i = 0; i < after_queries.A.size(); i++) {
            if (i != 0) ofs << ",";
            ofs << after_queries.A[i];
        }
        ofs << "\nbefore_last_query_A=";
        for (int i = 0; i < before_last_query.A.size(); i++) {
            if (i != 0) ofs << ",";
            ofs << before_last_query.A[i];
        }
        ofs << "\nrestored_A=";
        for (int i = 0; i < restored.A.size(); i++) {
            if (i != 0) ofs << ",";
            ofs << restored.A[i];
        }
        ofs << "\nstate_final_rot_cmd=" << state.final_rot_info.first;
        ofs << "\nstate_final_rot_cnt=" << state.final_rot_info.second;
        ofs << "\nrebuild_final_rot_cmd=" << rebuild_final_rot.first;
        ofs << "\nrebuild_final_rot_cnt=" << rebuild_final_rot.second;
        if (!state.queries.empty()) {
            ofs << "\nlast_query=" << state.queries.back();
            if (state.queries.size() >= 2) {
                ofs << "\nprev_query=" << state.queries[state.queries.size() - 2];
                ofs << "\nprev_query_finished_top_ord_a="
                    << state.queries[state.queries.size() - 2].get_finished_top_ord_a().get();
            }
            ofs << "\nlast_query_finished_top_ord_a="
                << state.queries.back().get_finished_top_ord_a().get();
        }
        ofs << "\nprev_query_commands=";
        for (int i = 0; i < prev_query_cmds.size(); i++) {
            if (i != 0) ofs << " ";
            ofs << prev_query_cmds[i];
        }
        ofs << "\nquery_commands=";
        for (int i = 0; i < query_cmds.size(); i++) {
            if (i != 0) ofs << " ";
            ofs << query_cmds[i];
        }
        ofs << "\ncommands=";
        for (int i = 0; i < cmds.size(); i++) {
            if (i != 0) ofs << " ";
            ofs << cmds[i];
        }
        ofs << "\nquery_count=" << state.queries.size();
        ofs << "\nsum_turn=" << state.sum_turn << "\n\n";
        ofs.close();
        write_query_replay_debug(initial_state, state, query_cmds);
    }

    static bool exists_in_stack(const RingBuffer500 &stack, int v) {
        for (int i = 0; i < stack.size(); ++i) {
            if (stack[i] == v) {
                return true;
            }
        }
        return false;
    }

    static void init_value_states(const State &st,
                                  my_vector<ValState> &now_a_val_state,
                                  my_vector<ValState> &now_b_val_state) {
        for (int i = 0; i < st.A.size(); ++i) {
            int v = st.A[i];
            now_a_val_state[v] = ValState::INIT;
        }
        for (int i = 0; i < st.B.size(); ++i) {
            int v = st.B[i];
            now_b_val_state[v] = ValState::TARGET;
        }
    }

    static void validate_query_preconditions(const QueryCommonRI &qri,
                                             const State &st,
                                             const my_vector<ValState> &now_a_val_state,
                                             const my_vector<ValState> &now_b_val_state) {
        int v = qri.get_tar_v();
        if (qri.is_a2b_type()) {
            my_assert(exists_in_stack(st.A, v));
            my_assert(!exists_in_stack(st.B, v));
            my_assert(now_a_val_state[v] == ValState::INIT );
            my_assert(now_b_val_state[v] != ValState::TARGET);
            return;
        }
        if (qri.is_b2a_type()) {
            my_assert(exists_in_stack(st.B, v));
            my_assert(!exists_in_stack(st.A, v));
            my_assert(now_b_val_state[v] == ValState::TARGET);
            return;
        }
        my_assert(qri.is_a2a_type());
        my_assert(exists_in_stack(st.A, v));
        my_assert(!exists_in_stack(st.B, v));
        my_assert(now_b_val_state[v] != ValState::TARGET);
        my_assert(now_a_val_state[v] == ValState::INIT);
    }

    static void apply_query_to_value_states(const QueryCommonRI &qri,
                                            my_vector<ValState> &now_a_val_state,
                                            my_vector<ValState> &now_b_val_state) {
        int v = qri.get_tar_v();
        if (qri.is_a2b_type()) {
            now_a_val_state[v] = ValState::NONE;
            now_b_val_state[v] = ValState::TARGET;
            return;
        }
        if (qri.is_b2a_type()) {
            now_b_val_state[v] = ValState::NONE;
            now_a_val_state[v] = ValState::TARGET;
            return;
        }
        my_assert(qri.is_a2a_type());
        now_a_val_state[v] = ValState::TARGET;
        now_b_val_state[v] = ValState::NONE;
    }

    static my_vector<command> build_all_commands_and_validate_turn_count(
        const State &initial_state,
        const QueriesState &state) {
        my_vector<command> cmds = YstCommandsBuilder::build_all_commands_from_state(initial_state, state);
        my_assert(static_cast<int>(cmds.size()) == state.sum_turn);
        return cmds;
    }

    static State apply_commands_to_state_checked(const State &initial_state, const my_vector<command> &cmds) {
        State restored(initial_state);
        for (command cmd: cmds) {
            my_assert(restored.can_do(cmd));
            restored.do_cmd(cmd);
        }
        return restored;
    }

    static void validate_commands_can_sort(const State &initial_state, const QueriesState &state) {
        #ifndef DEBUG
            return;
        #endif
        my_vector<command> cmds = build_all_commands_and_validate_turn_count(initial_state, state);
        State restored = apply_commands_to_state_checked(initial_state, cmds);
        my_assert(restored.B.empty());
        my_assert(restored.A.size() == initial_state.current_N);
        for (int i = 0; i < restored.A.size(); ++i) {
            if(!is_compressed_sorted_value(initial_state, restored.A[i], i)){
                // _BEGIN: validator の再計算も empty query 時に state の先頭基準を失わないように合わせる。
                auto rebuild_final_rot = FinalRotCalculator::build_final_rot_info(initial_state.current_N, state);
                // _END
                my_vector<command> query_cmds(cmds.begin(), cmds.end() - state.final_rot_info.second);
                const int last_query_turn = state.queries.empty() ? 0 : state.queries.back().get_ins_turn();
                my_vector<command> prev_query_cmds(query_cmds.begin(), query_cmds.end() - last_query_turn);
                State before_last_query = apply_commands_to_state_checked(initial_state, prev_query_cmds);
                State after_queries = apply_commands_to_state_checked(initial_state, query_cmds);
                // _BEGIN: final rot の計算値と query 実行直後の状態を保存し、1回転ずれの原因を観測だけで絞る。
                write_sort_failure_debug(initial_state, state, cmds, query_cmds, prev_query_cmds, before_last_query, after_queries, restored, rebuild_final_rot);
                // _END
                write_query_compare_debug(initial_state, state, query_cmds);
               my_assert(false);
            }
        }
    }

    // 責務: yst 付きでコマンド列の sort 可否を検証し、失敗時に AOrder/BOrder entries も出力する。
    // 必要な理由: QueriesState だけのログでは AOrder の TARGET 線形順序を失敗時に確認できないため。
    static void validate_commands_can_sort(const State &initial_state, const YakinamashiState<MAX_N> &yst) {
        #ifndef DEBUG
            return;
        #endif
        const QueriesState &state = yst.queries_state;
        my_vector<command> cmds = build_all_commands_and_validate_turn_count(initial_state, state);
        State restored = apply_commands_to_state_checked(initial_state, cmds);
        my_assert(restored.B.empty());
        my_assert(restored.A.size() == initial_state.current_N);
        for (int i = 0; i < restored.A.size(); ++i) {
            if(!is_compressed_sorted_value(initial_state, restored.A[i], i)){
                auto rebuild_final_rot = FinalRotCalculator::build_final_rot_info(initial_state.current_N, state);
                my_vector<command> query_cmds(cmds.begin(), cmds.end() - state.final_rot_info.second);
                const int last_query_turn = state.queries.empty() ? 0 : state.queries.back().get_ins_turn();
                my_vector<command> prev_query_cmds(query_cmds.begin(), query_cmds.end() - last_query_turn);
                State before_last_query = apply_commands_to_state_checked(initial_state, prev_query_cmds);
                State after_queries = apply_commands_to_state_checked(initial_state, query_cmds);
                write_sort_failure_debug(initial_state, state, cmds, query_cmds, prev_query_cmds, before_last_query, after_queries, restored, rebuild_final_rot);
                write_yst_failure_debug(yst);
                write_query_compare_debug(initial_state, state, query_cmds);
               my_assert(false);
            }
        }
    }

    static int get_top_ord(const my_vector<int> &ords) {
        if (ords.empty()) {
            return 0;
        }
        return ords[0];
    }
};
