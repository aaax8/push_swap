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
#include "YakinamashiQuery/Validation/YakinamashiStateValidator.h"
#include "util.h"

#include <algorithm>
#include <sstream>

// 責務: 採用する command 列と最終手数を保持する。
// 必要な理由: suffix QueriesState 化で元 query index に依存しない探索にするため。
struct SwapBeamResult {
    my_vector<command> commands;
    int command_count = 0;
};

template<size_t MAX_N>
class SwapBeamSearch {
    constexpr static int BEAM_WIDTH = 100;
    constexpr static size_t MAX_ALL_ORD_A = YQConstants::GET_MAX_ALL_ORD_A<MAX_N>();
    constexpr static size_t MAX_ALL_ORD_B = YQConstants::GET_MAX_ALL_ORD_B<MAX_N>();

    using AExist = my_bitset<MAX_ALL_ORD_A>;
    using BExist = my_bitset<MAX_ALL_ORD_B>;

    // 責務: 再生成 command 列を保持する。
    // 必要な理由: command の query 所属は一意に定義できないため、query 進行は Node::current_q_idx で管理する必要がある。
    struct RangeCmds {
        my_vector<command> cmds;
    };

    // 責務: 物理 stack、order、ValState、exist all_ord、suffix query state、現在 range、command、実行済み command、手数評価を保持する。
    // 必要な理由: no-swap では既存 suffix command を消費し、swap 後だけ現在 query 以降の suffix QueriesState を再生成するため。
    struct Node {
        State st;
        QueryOrder order;
        QueriesState queries_state;
        my_vector<ValState> val_a;
        my_vector<ValState> val_b;
        AExist exist_a;
        BExist exist_b;
        int range_l = 0;
        int range_r = 0;
        my_vector<command> cmds;
        int current_q_idx = 0;
        int cmd_idx = 0;
        my_vector<command> past_cmds;
        int swap_count = 0;
        bool done = false;
        int score = 0;
        bool log_enabled = false;
#ifdef DEBUG
        // 責務: 現在 range の query を論理適用した後の期待状態を保持する。
        // 必要な理由: range command 実行後の State/order/ValState/exist のズレを原因特定できるようにするため。
        bool debug_has_range_expected = false;
        State debug_range_expected_st;
        QueryOrder debug_range_expected_order;
        my_vector<ValState> debug_range_expected_val_a;
        my_vector<ValState> debug_range_expected_val_b;
        AExist debug_range_expected_exist_a;
        BExist debug_range_expected_exist_b;
        int debug_range_expected_l = 0;
        int debug_range_expected_r = 0;
        my_vector<command> debug_range_expected_cmds;
#endif

        Node(const State& init_st,
             const QueryOrder& init_order,
             const QueriesState& init_queries_state,
             bool init_log_enabled)
            : st(init_st),
              order(init_order),
              queries_state(init_queries_state),
              val_a(init_st.initial_N, ValState::NONE),
              val_b(init_st.initial_N, ValState::NONE),
              log_enabled(init_log_enabled)
#ifdef DEBUG
              , debug_range_expected_st(init_st),
              debug_range_expected_order(init_order),
              debug_range_expected_val_a(init_st.initial_N, ValState::NONE),
              debug_range_expected_val_b(init_st.initial_N, ValState::NONE)
#endif
        {}
    };

public:
    // 責務: 初期 node を作り、1手ごとに beam 展開・枝刈りし、final rotation 込み best を超えない範囲で最良 command 列を返す。
    // 必要な理由: 各 branch は past_cmds を 1 手ずつ増やすため、全 node 完了待ちではなく best final score による打ち切りができるため。
    static SwapBeamResult run(const State& init_st, const YakinamashiState<MAX_N>& yst, bool log_enabled = false) {
        cout << "start run" << endl;
        //最後手数比較で使う
        const my_vector<command> original_cmds =
                YstCommandsBuilder::build_all_commands_from_state(init_st, yst.queries_state);
        Node init_node(init_st, yst.query_order, yst.queries_state, log_enabled);
        initialize_node(init_node);
        init_node.range_l = 0;
        init_node.range_r = find_range_end(init_node.queries_state, 0);
        set_range_and_cmds_done(init_node);
        reset_score_after_rebuild(init_node);
        log_run_start(log_enabled, init_st, yst, original_cmds, init_node);

        my_vector<Node> beam;
        Node best = init_node;
        int best_score = 1<<30;
        bool has_best = false;
        auto update_best = [&](const Node& node) {
            my_assert(node.done);
            const int score = node.score;
            if (!has_best || score < best_score ||
                (score == best_score && node.swap_count > best.swap_count)) {
                best = node;
                best_score = score;
                has_best = true;
                log_best_update(log_enabled, best);
            }
        };
        if (init_node.done) {
            update_best(init_node);
        } else {
            beam.push_back(init_node);
        }
        while (!beam.empty()) {
            const int current_turn = static_cast<int>(beam.front().past_cmds.size());
        #ifdef DEBUG
            for (const Node& node: beam) {
                my_assert(!node.done);
                my_assert(static_cast<int>(node.past_cmds.size()) == current_turn);
            }
        #endif
            if (has_best && current_turn >= best_score) break;
            my_vector<Node> expanded;
            for (Node& node: beam) {
                expand_node(node, expanded);
            }
            my_vector<Node> next;
            int done_count = 0;
            for (const Node& node: expanded) {
                if (node.done) {
                    done_count++;
                    update_best(node);
                } else {
                    next.push_back(node);
                }
            }
            log_depth(log_enabled, current_turn, beam, expanded, done_count, next);
            prune(next);
            log_prune(log_enabled, current_turn, next);
            beam.swap(next);
        }
        my_assert(has_best);
        append_final_rot(best);
        validate_sorted_commands(init_st, best.past_cmds);
        my_assert(static_cast<int>(best.past_cmds.size()) <= static_cast<int>(original_cmds.size()));
        log_result(original_cmds, best);
        log_run_end(log_enabled, best);
        return SwapBeamResult{best.past_cmds, static_cast<int>(best.past_cmds.size())};
    }

private:
    static void initialize_node(Node& node) {
        my_assert(node.st.B.empty());
        for (int i = 0; i < node.st.A.size(); i++) {
            int v = node.st.A[i];
            node.val_a[v] = ValState::INIT;
            AllOrd ord = node.order.aorder.get_all_order(v, ValState::INIT);
            ExistAllOrdManager::set_exist(node.exist_a, ord);
        }
    }

    // 責務: 指定 query の直前に連続する A2A を含めた範囲先頭を返す。
    // 必要な理由: node が元 query 列ではなく suffix QueriesState を持つため。
    static int find_range_begin(const QueriesState& queries_state, int q_idx) {
        if (q_idx >= static_cast<int>(queries_state.queries.size())) return q_idx;
        int l = q_idx;
        while (l > 0 && queries_state.queries[l - 1].is_a2a_type()) {
            l--;
        }
        return l;
    }

    // 責務: A2A 連続と直後 AB query、または末尾 A2A 連続を半開区間として返す。
    // 必要な理由: range_l を node-local で常に 0 として扱うため。
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

    // 責務: RingBuffer500 を走査して value の存在有無を返す。
    // 必要な理由: query 完了判定で対象 value が移動先 stack にいることを確認するため。
    static bool contains_value(const RingBuffer500& stack, int v) {
        for (int i = 0; i < stack.size(); i++) {
            if (stack[i] == v) return true;
        }
        return false;
    }

    static bool is_query_done(const Node& node, const QueryCommonRI& qri) {
        const int v = qri.get_tar_v();
        if (qri.is_a2b_type()) {
            return node.val_b[v] == ValState::TARGET && contains_value(node.st.B, v);
        }
        if (qri.is_b2a_type()) {
            return node.val_a[v] == ValState::TARGET && contains_value(node.st.A, v);
        }
        my_assert(qri.is_a2a_type());
        return node.val_a[v] == ValState::TARGET && contains_value(node.st.A, v);
    }

    // 責務: node の A top value/state/all_ord と exist 集合から現在 top OrdPos を計算する。
    // 必要な理由: QueryCommonRI 再生成時に開始 top ord を明示する必要があるため。
    static OrdPos get_top_ord_a(const Node& node) {
        if (node.st.A.empty()) return OrdPos(0);
        int v = node.st.A[0];
        AllOrd all_ord = node.order.aorder.get_all_order(v, node.val_a[v]);
        return to_ord_pos(OrdCalc::get_rel_ord(node.exist_a, all_ord), node.st.A.size());
    }

    // 責務: node の B top value/state/all_ord と exist 集合から現在 top OrdPos を計算する。
    // 必要な理由: QueryCommonRI 再生成時に開始 top ord を明示する必要があるため。
    static OrdPos get_top_ord_b(const Node& node) {
        if (node.st.B.empty()) return OrdPos(0);
        int v = node.st.B[0];
        AllOrd all_ord = node.order.border.get_all_order(v, node.val_b[v]);
        return to_ord_pos(OrdCalc::get_rel_ord(node.exist_b, all_ord), node.st.B.size());
    }

    // 責務: 元 query の type/tar_v と node の現在 order/ValState/exist から A2B/B2A/A2A の ord 情報を作り直す。
    // 必要な理由: swap で all_ord 割当が変わった後、古い query を使わず現在状態に合う command を生成するため。
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

    // 責務: 再生成済み prefix と現在 top ord を持つ一時 QueriesState を作り、QueryCommon を RI 化する。
    // 必要な理由: QueryToCommandBuilder が必要とする回転方向・回転数を既存 RI factory で計算するため。
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

    // 責務: QueryToCommandBuilder で command を追加する。
    // 必要な理由: command の query 所属は一意に定義できないため、所属情報を保持せず current_q_idx で進行管理するため。
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

    // 責務: qri を State に適用し、push/A2A に伴う ValState/exist の論理更新も行う。
    // 必要な理由: 後続 query を同じ [l,r) 内の現在状態から再生成するため。
    static void apply_qri_to_rebuild_state(Node& temp, const my_vector<QueryCommonRI>& qris, int relative_q_idx) {
        QueriesCalculator qs_calc(temp.st.current_N, qris);
        const QueryCommonRI& qri = qris[relative_q_idx];
        QueryStateApplyUtil::apply_query_to_state(temp.st, qs_calc, relative_q_idx, qri);
        apply_query_logical_update(temp, qri);
    }

#ifdef DEBUG
    // 責務: command 列を 1 行文字列に変換する。
    // 必要な理由: 期待 range と実行 command の対応を assert 直前に確認するため。
    static std::string debug_cmds_str(const my_vector<command>& cmds) {
        std::ostringstream oss;
        for (int i = 0; i < static_cast<int>(cmds.size()); i++) {
            if (i > 0) oss << ",";
            oss << cmds[i];
        }
        return oss.str();
    }

    // 責務: stack の先頭から limit 個までを 1 行文字列に変換する。
    // 必要な理由: A2A target の実位置と top 周辺の差分を assert 直前に確認するため。
    static std::string debug_stack_str(const RingBuffer500& stack, int limit = 40) {
        std::ostringstream oss;
        const int n = stack.size();
        const int m = std::min(n, limit);
        oss << "[";
        for (int i = 0; i < m; i++) {
            if (i > 0) oss << ",";
            oss << stack[i];
        }
        if (m < n) oss << ",...";
        oss << "]";
        return oss.str();
    }

    // 責務: A/B/current_N/initial_N が一致するか返す。
    // 必要な理由: 期待状態は query 適用で作るため、物理 stack の一致だけを range 検証対象にするため。
    static bool debug_same_state(const State& lhs, const State& rhs) {
        return lhs.A == rhs.A &&
               lhs.B == rhs.B &&
               lhs.current_N == rhs.current_N &&
               lhs.initial_N == rhs.initial_N;
    }

    // 責務: 現在 range の query を現在状態から論理適用した期待 end snapshot を node に保持する。
    // 必要な理由: range command 実行後のズレが command 実行側か期待 range 側かを切り分けるため。
    static void debug_store_range_expected(Node& node) {
        Node expected = node;
        for (int qi = node.current_q_idx; qi < node.range_r; qi++) {
            apply_qri_to_rebuild_state(expected, expected.queries_state.queries, qi);
        }
        node.debug_has_range_expected = true;
        node.debug_range_expected_st = expected.st;
        node.debug_range_expected_order = expected.order;
        node.debug_range_expected_val_a = expected.val_a;
        node.debug_range_expected_val_b = expected.val_b;
        node.debug_range_expected_exist_a = expected.exist_a;
        node.debug_range_expected_exist_b = expected.exist_b;
        node.debug_range_expected_l = node.current_q_idx;
        node.debug_range_expected_r = node.range_r;
        node.debug_range_expected_cmds = node.cmds;
    }

    // 責務: 実状態と期待状態の主要差分、range、command、qri を出力する。
    // 必要な理由: どの range で State/order/ValState/exist がズレたか断定できるログを残すため。
    static void debug_log_range_mismatch(const Node& node, const char* site) {
        out("swap_beam_range_mismatch",
            "site", site,
            "expected_l", node.debug_range_expected_l,
            "expected_r", node.debug_range_expected_r,
            "current_q_idx", node.current_q_idx,
            "range_r", node.range_r,
            "cmd_idx", node.cmd_idx,
            "cmds", debug_cmds_str(node.debug_range_expected_cmds),
            "actual_A", debug_stack_str(node.st.A),
            "expected_A", debug_stack_str(node.debug_range_expected_st.A),
            "actual_B", debug_stack_str(node.st.B),
            "expected_B", debug_stack_str(node.debug_range_expected_st.B),
            "same_state", debug_same_state(node.st, node.debug_range_expected_st),
            "same_order", node.order == node.debug_range_expected_order,
            "same_val_a", node.val_a == node.debug_range_expected_val_a,
            "same_val_b", node.val_b == node.debug_range_expected_val_b,
            "same_exist_a", node.exist_a == node.debug_range_expected_exist_a,
            "same_exist_b", node.exist_b == node.debug_range_expected_exist_b);
        for (int qi = node.debug_range_expected_l; qi < node.debug_range_expected_r; qi++) {
            out("swap_beam_range_qri", "qi", qi, "qri", node.queries_state.queries[qi]);
        }
    }

    // 責務: State/order/ValState/exist を期待 end snapshot と比較し、不一致ならログを出して assert する。
    // 必要な理由: range command 実行後の状態ズレを発生 range の終端で検出するため。
    static void debug_validate_range_end(const Node& node, const char* site) {
        my_assert(node.debug_has_range_expected);
        const bool ok = debug_same_state(node.st, node.debug_range_expected_st) &&
                        node.order == node.debug_range_expected_order &&
                        node.val_a == node.debug_range_expected_val_a &&
                        node.val_b == node.debug_range_expected_val_b &&
                        node.exist_a == node.debug_range_expected_exist_a &&
                        node.exist_b == node.debug_range_expected_exist_b;
        if (!ok) {
            debug_log_range_mismatch(node, site);
            my_assert(false);
        }
    }
#endif

    // 責務: 残り query の ins_turn 合計と final_rot_info を QueriesState に反映する。
    // 必要な理由: score を「実行済み手数 + 残り query 手数 + final rot」として扱うため。
    static void refresh_final_and_sum(QueriesState& queries_state) {
        queries_state.final_rot_info =
                FinalRotCalculator::build_final_rot_info(queries_state.current_N, queries_state);
        int sum_turn = queries_state.final_rot_info.second;
        for (const QueryCommonRI& qri: queries_state.queries) {
            sum_turn += qri.get_ins_turn();
        }
        queries_state.sum_turn = sum_turn;
    }

    // 責務: node-local suffix QueriesState の [current_q_idx, range_r) を command 列へ変換する。
    // 必要な理由: no-swap branch では query を再生成せず、既に持っている QRI から次 range の command だけを作るため。
    static RangeCmds build_range_cmds(const Node& node) {
        CommandBuildState build_st(node.st);
        for (int qi = node.current_q_idx; qi < node.range_r; qi++) {
            add_qri_commands(build_st, node.queries_state.queries, qi);
        }
        return RangeCmds{build_st.cmds};
    }

    // 責務: [0, trim_count) を取り除き、first top / sum_turn / final_rot_info を suffix 用に更新する。
    // 必要な理由: node が常に「現在以降の query 列」だけを持ち、range_l を 0 として扱うため。
    static void trim_queries(Node& node, int trim_count) {
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
        node.cmd_idx = 0;
        refresh_final_and_sum(node.queries_state);
    }

    // 責務: range 先頭から ins_turn 0 の A2A を完了扱いにし、ValState/exist を更新する。
    // 必要な理由: no-op A2A だけの区間や no-op A2A 先頭 range を command 展開対象から外すため。
    static void skip_noop_a2a(Node& node) {
        while (node.current_q_idx < node.range_r) {
            const QueryCommonRI& qri = node.queries_state.queries[node.current_q_idx];
            if (!qri.is_a2a_type() || qri.get_ins_turn() != 0) return;
            apply_query_logical_update(node, qri);
            node.current_q_idx++;
        }
    }

    // 責務: no-op A2A を飛ばし、空 range は trim し、次に見る command 列を node に設定する。
    // 必要な理由: no-swap では QRI 再生成を避け、range 終了時だけ suffix を縮めるため。
    static void set_range_and_cmds_done(Node& node) {
        node.done = false;
        while (!node.queries_state.queries.empty()) {
            node.range_l = 0;
            node.range_r = find_range_end(node.queries_state, 0);
            node.current_q_idx = 0;
            skip_noop_a2a(node);
            if (node.current_q_idx == node.range_r) {
                trim_queries(node, node.range_r);
                continue;
            }
            RangeCmds range_cmds = build_range_cmds(node);
            node.cmds = range_cmds.cmds;
            node.cmd_idx = 0;
        #ifdef DEBUG
            debug_store_range_expected(node);
        #endif
            if (node.cmds.empty()) {
                trim_queries(node, node.range_r);
                continue;
            }
            return;
        }
        node.done = true;
        node.cmds.clear();
        node.cmd_idx = 0;
        node.current_q_idx = 0;
        node.range_l = 0;
        node.range_r = 0;
        refresh_final_and_sum(node.queries_state);
    }

    // 責務: node の score を「実行済み手数 + 残り query 手数 + final rot」として保持する。
    // 必要な理由: no-swap や range trim では score が変わらないため、都度再計算を避けるため。
    static void reset_score_after_rebuild(Node& node) {
        node.score = static_cast<int>(node.past_cmds.size()) + node.queries_state.sum_turn;
    #ifdef DEBUG
        validate_score_by_replay(node);
    #endif
    }

    // 責務: node.st.A と node.val_a に基づいて node.exist_a を再構築する。
    // 必要な理由: INIT entry の rebase で all_ord index が動くため、差分更新ではなく現在状態から同期するため。
    static void rebuild_exist_a_from_stack(Node& node) {
        node.exist_a.reset();
        for (int i = 0; i < node.st.A.size(); i++) {
            const int v = node.st.A[i];
            const AllOrd ord = node.order.aorder.get_all_order(v, node.val_a[v]);
            ExistAllOrdManager::set_exist(node.exist_a, ord);
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

    // 責務: node.current_q_idx 以降の query type/tar_v を保ち、State/order/ValState/exist から suffix QueriesState を作り直す。
    // 必要な理由: swap で all_ord 割当と物理 stack が変わった後、古い QRI の ord/rel_ord/turn を使わないため。
    static void rebuild_current_suffix_after_swap(Node& node) {
        rebase_current_a2a_init_ord_stopgap(node);
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
        refresh_final_and_sum(node.queries_state);
        log_rebuild(node, static_cast<int>(old_suffix.size()), static_cast<int>(rebuilt.size()));
        node.current_q_idx = 0;
        node.range_l = 0;
        node.range_r = 0;
        node.cmd_idx = 0;
        node.cmds.clear();
        node.done = false;
    #ifdef DEBUG
        validate_rebuilt_suffix(node);
    #endif
    }

    // 責務: A2B/B2A/A2A の完了に応じて exist all_ord と A/B ValState を更新する。
    // 必要な理由: command 実行と query 再生成の両方で同じ論理状態更新を使うため。
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
        ExistAllOrdManager::upd_ord_a2a<MAX_N>(
                v, node.exist_a, node.order.aorder, node.val_a);
    }

    // 責務: swap なし、追加 swap、既存 sa/sb の ss 化の各 branch を現在 command に応じて生成する。
    // 必要な理由: command 判断ごとに beam 幅 100 へ枝刈りする探索単位を作るため。
    static void expand_node(Node& node,
                            my_vector<Node>& next) {
        my_assert(!node.done);
        my_assert(0 <= node.cmd_idx && node.cmd_idx < static_cast<int>(node.cmds.size()));
        const int base_score = node.score;
        add_no_swap_branch(node, next);
        const command current_cmd = node.cmds[node.cmd_idx];
        if (current_cmd == SA) {
            add_replace_with_ss_branch(node, next, true, base_score);
            return;
        }
        if (current_cmd == SB) {
            add_replace_with_ss_branch(node, next, false, base_score);
            return;
        }
        add_insert_swap_branch(node, next, SA, base_score);
        add_insert_swap_branch(node, next, SB, base_score);
        add_insert_swap_branch(node, next, SS, base_score);
    }

    // 責務: 現在 command をそのまま実行し、range 完了時だけ suffix trim と次 range 構築を行う。
    // 必要な理由: swap なしでは既存 QRI/command を維持し、current_q_idx と cmd_idx の更新だけで進めるため。
    static void add_no_swap_branch(const Node& node,
                                   my_vector<Node>& next) {
        Node cand = node;
        const command cmd = cand.cmds[cand.cmd_idx];
        cand.st.do_cmd(cmd);
        cand.past_cmds.push_back(cmd);
        bool query_completed = advance_query_by_cmd(cand, cmd);
        if (query_completed) {
            skip_noop_a2a(cand);
            if (cand.current_q_idx == cand.range_r) {
            #ifdef DEBUG
                debug_validate_range_end(cand, "add_no_swap_branch");
            #endif
                trim_queries(cand, cand.range_r);
                set_range_and_cmds_done(cand);
            } else {
                cand.cmd_idx++;
                my_assert(cand.cmd_idx < static_cast<int>(cand.cmds.size()));
            }
        #ifdef DEBUG
            validate_score_by_replay(cand);
        #endif
            next.push_back(cand);
            return;
        }
        cand.cmd_idx++;
        my_assert(cand.cmd_idx < static_cast<int>(cand.cmds.size()));
    #ifdef DEBUG
        validate_score_by_replay(cand);
    #endif
        next.push_back(cand);
    }

    // 責務: PA/PB 実行後に current query の tar_v だけが push されたことを検証し、ValState/exist と current_q_idx を更新する。
    // 必要な理由: 元 query index ではなく node が持つ suffix query index で進行管理するため。
    static bool update_by_existing_command(Node& node, command cmd) {
        if (cmd == PB) {
            int v = node.st.B[0];
            my_assert(node.current_q_idx < static_cast<int>(node.queries_state.queries.size()));
            const QueryCommonRI& qri = node.queries_state.queries[node.current_q_idx];
            my_assert(qri.is_a2b_type());
            my_assert(v == qri.get_tar_v());
            my_assert(node.val_a[v] == ValState::INIT);
            my_assert(node.val_b[v] != ValState::TARGET);
            ExistAllOrdManager::upd_ord_a2b<MAX_N>(
                    v, node.exist_a, node.exist_b, node.order.aorder, node.order.border,
                    node.val_a, node.val_b);
            node.current_q_idx++;
            return true;
        }
        if (cmd == PA) {
            int v = node.st.A[0];
            my_assert(node.current_q_idx < static_cast<int>(node.queries_state.queries.size()));
            const QueryCommonRI& qri = node.queries_state.queries[node.current_q_idx];
            my_assert(qri.is_b2a_type());
            my_assert(v == qri.get_tar_v());
            my_assert(node.val_b[v] == ValState::TARGET);
            my_assert(node.val_a[v] == ValState::INIT);
            ExistAllOrdManager::upd_ord_b2a<MAX_N>(
                    v, node.exist_a, node.exist_b, node.order.aorder, node.order.border,
                    node.val_a, node.val_b);
            node.current_q_idx++;
            return true;
        }
        return false;
    }

    // 責務: push command の完了更新と、sa/ss による A2A target 化チェックを行い、完了時は current_q_idx を進める。
    // 必要な理由: update_by_existing_command と maybe_update_a2a_target_after_a_swap はセットで扱う前提なので、呼び出し側の重複と更新漏れを避けるため。
    static bool advance_query_by_cmd(Node& node, command cmd) {
        bool query_completed = update_by_existing_command(node, cmd);
        if (!query_completed && (cmd == SA || cmd == SS)) {
            query_completed = maybe_update_a2a_target_after_a_swap(node);
            if (query_completed) node.current_q_idx++;
        }
        return query_completed;
    }

    // 責務: A 側 TARGET/INIT top2 の all_ord 間に別 TARGET entry がないか確認する。
    // 必要な理由: 中間に TARGET があると TARGET entry の線形順序が変わり、AOrder の不変条件が壊れるため。
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

    // 責務: A size、TARGET 同士禁止、TARGET/INIT 間の TARGET 不在ルールに基づき sa/ss の A 側可否を返す。
    // 必要な理由: A において TARGET はソート済みを表すため、TARGET 順序を壊す swap 候補を外すため。
    static bool can_beam_swap_a(const Node& node) {
        if (node.st.A.size() < 2) return false;
        const int v0 = node.st.A[0];
        const int v1 = node.st.A[1];
        return allow_target_init_a(node, v0, v1);
    }

    // 責務: A 側追加 swap が、TARGET 同士禁止と current A2A tar_v 禁止の両方を満たすか返す。
    // 必要な理由: A2A で移動中の tar_v を追加 swap で戻す候補を作らないため。
    static bool can_add_swap_a(const Node& node) {
        if (!can_beam_swap_a(node)) return false;
        if (node.current_q_idx >= static_cast<int>(node.queries_state.queries.size())) return true;
        const QueryCommonRI& qri = node.queries_state.queries[node.current_q_idx];
        if (!qri.is_a2a_type()) return true;
        const int v = qri.get_tar_v();
        return node.st.A[0] != v && node.st.A[1] != v;
    }

    // 責務: B size のみから sb/ss の B 側可否を返す。
    // 必要な理由: B では TARGET 同士 swap 禁止がない仕様に合わせるため。
    static bool can_beam_swap_b(const Node& node) {
        return node.st.B.size() >= 2;
    }

    // 責務: sa/sb/ss を実行・集計し、query 範囲を先頭から再生成して command index 0 へ戻す。
    // 必要な理由: swap によって all_ord/state が変わった場合は現在 query だけでなく [l,r) を見直す仕様のため。
    static void add_insert_swap_branch(const Node& node,
                                       my_vector<Node>& next,
                                       command swap_cmd,
                                       int base_score) {
        if (swap_cmd == SA && !can_add_swap_a(node)) return;
        if (swap_cmd == SB && !can_beam_swap_b(node)) return;
        if (swap_cmd == SS && (!can_add_swap_a(node) || !can_beam_swap_b(node))) return;
        Node cand = node;
        apply_beam_swap(cand, swap_cmd);
        cand.swap_count++;
        cand.past_cmds.push_back(swap_cmd);
        rebuild_current_suffix_after_swap(cand);
        set_range_and_cmds_done(cand);
        reset_score_after_rebuild(cand);
        if (cand.score > base_score) {
            log_branch(cand, "insert_swap", swap_cmd, base_score, false,
                       static_cast<int>(node.past_cmds.size()), node.queries_state.sum_turn);
            return;
        }
        log_branch(cand, "insert_swap", swap_cmd, base_score, true,
                   static_cast<int>(node.past_cmds.size()), node.queries_state.sum_turn);
        next.push_back(cand);
    }

    // 責務: 反対側 swap 可否を見て ss を実行・集計し、query 範囲を先頭から再生成して command index 0 へ戻す。
    // 必要な理由: 既存 swap に片側 swap を合成して 1 手の ss にする候補を探索するため。
    static void add_replace_with_ss_branch(const Node& node,
                                           my_vector<Node>& next,
                                           bool replacing_sa,
                                           int base_score) {
        if (replacing_sa && !can_beam_swap_b(node)) return;
        if (!replacing_sa && !can_add_swap_a(node)) return;
        Node cand = node;
        if (replacing_sa) {
            apply_beam_swap_b(cand);
            cand.st.ss();
        } else {
            apply_beam_swap_a(cand);
            cand.st.ss();
        }
        cand.swap_count++;
        cand.past_cmds.push_back(SS);
        const command replaced_cmd = replacing_sa ? SA : SB;
        bool query_completed = advance_query_by_cmd(cand, replaced_cmd);
        if (query_completed) {
            skip_noop_a2a(cand);
        }
        rebuild_current_suffix_after_swap(cand);
        set_range_and_cmds_done(cand);
        reset_score_after_rebuild(cand);
        if (cand.score > base_score) {
            log_branch(cand, "replace_ss", SS, base_score, false,
                       static_cast<int>(node.past_cmds.size()), node.queries_state.sum_turn);
            return;
        }
        log_branch(cand, "replace_ss", SS, base_score, true,
                   static_cast<int>(node.past_cmds.size()), node.queries_state.sum_turn);
        next.push_back(cand);
    }

     // 責務: sa/sb/ss に応じて order all_ord 交換と物理 State 実行を行う。
    // 必要な理由: swap branch の後続 query 再生成が変更後の stack/order/ValState を参照できるようにするため。
    static void apply_beam_swap(Node& node,
                                command swap_cmd) {
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

    // 責務: A top2 の value/state から all_ord を取得し、AOrder の O(1) swap API で入れ替えて検証する。
    // 必要な理由: 物理 sa/ss と all_ord 解釈の swap を一致させるため。
    static void apply_beam_swap_a(Node& node) {
        my_assert(can_beam_swap_a(node));
        int v0 = node.st.A[0];
        int v1 = node.st.A[1];
        ValState s0 = node.val_a[v0];
        ValState s1 = node.val_a[v1];
        AllOrd ord0 = node.order.aorder.get_all_order(v0, s0);
        AllOrd ord1 = node.order.aorder.get_all_order(v1, s1);
        node.order.aorder.swap_all_ord_entries(ord0, ord1);
        my_assert(node.order.aorder.get_all_order(v0, s0).get() == ord1.get());
        my_assert(node.order.aorder.get_all_order(v1, s1).get() == ord0.get());
    }

    // 責務: B top2 の TARGET all_ord を取得し、BOrder の O(1) swap API で入れ替えて検証する。
    // 必要な理由: 物理 sb/ss と B all_ord 解釈の swap を一致させるため。
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
        my_assert(node.order.border.get_all_order(v0, s0).get() == ord1.get());
        my_assert(node.order.border.get_all_order(v1, s1).get() == ord0.get());
    }

    // 責務: current_q_idx の query が未完了 A2A なら、target all_ord が現在 A の円環区間に入ったかを確認して TARGET 化する。
    // 必要な理由: swap 後再生成や trim 後も元 query index に依存せず current query を判定するため。
    static bool maybe_update_a2a_target_after_a_swap(Node& node) {
        my_assert (!node.done && node.current_q_idx < static_cast<int>(node.queries_state.queries.size()));
        const int q_idx = node.current_q_idx;
        const QueryCommonRI& qri = node.queries_state.queries[q_idx];
        my_assert(qri.is_a2a_type());
        const int v = qri.get_tar_v();
        my_assert (node.val_a[v] == ValState::INIT) ;
        if (is_a2a_target_ready(node, v)) {
            ExistAllOrdManager::upd_ord_a2a<MAX_N>(
                    v, node.exist_a, node.order.aorder, node.val_a);
            return true;
        }
        return false;
    }

    // 責務: target v が A[0]/A[1] にいることを検証し、前後 all_ord の円環区間に target all_ord が含まれるか返す。
    // 必要な理由: A 側 swap 直後の A2A target 化は top2 だけを見て判定する仕様のため。
    static bool is_a2a_target_ready(const Node& node, int v) {
        const int n = node.st.A.size();
        my_assert(n >= 2);
        int pos = -1;
        if (node.st.A[0] == v) pos = 0;
        if (node.st.A[1] == v) pos = 1;
    #ifdef DEBUG
        if (pos == -1) {
            int actual_pos = -1;
            for (int i = 0; i < node.st.A.size(); i++) {
                if (node.st.A[i] == v) {
                    actual_pos = i;
                    break;
                }
            }
            out("swap_beam_a2a_target_miss",
                "tar_v", v,
                "actual_pos", actual_pos,
                "current_q_idx", node.current_q_idx,
                "range_r", node.range_r,
                "cmd_idx", node.cmd_idx,
                "cmds", debug_cmds_str(node.cmds),
                "A", debug_stack_str(node.st.A),
                "B", debug_stack_str(node.st.B),
                "qri", node.queries_state.queries[node.current_q_idx]);
        }
    #endif
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

#ifdef DEBUG
    // 責務: 再生成直後の node をコピーし、range command を構築して最後まで replay できることを assert する。
    // 必要な理由: rebuild_current_suffix_after_swap は current query 以降の QRI を作り直す中核処理なので、score 設定前に suffix 自体の妥当性を検出するため。
    static void validate_rebuilt_suffix(const Node& node) {
        Node sim = node;
        set_range_and_cmds_done(sim);
        while (!sim.done) {
            my_assert(0 <= sim.cmd_idx && sim.cmd_idx < static_cast<int>(sim.cmds.size()));
            const command cmd = sim.cmds[sim.cmd_idx];
            sim.st.do_cmd(cmd);
            bool query_completed = advance_query_by_cmd(sim, cmd);
            if (query_completed) {
                skip_noop_a2a(sim);
                if (sim.current_q_idx == sim.range_r) {
                    debug_validate_range_end(sim, "validate_rebuilt_suffix");
                    trim_queries(sim, sim.range_r);
                    set_range_and_cmds_done(sim);
                } else {
                    sim.cmd_idx++;
                    my_assert(sim.cmd_idx < static_cast<int>(sim.cmds.size()));
                }
                continue;
            }
            sim.cmd_idx++;
            my_assert(sim.cmd_idx < static_cast<int>(sim.cmds.size()));
        }
        append_final_rot(sim);
        my_assert(sim.st.B.empty());
        for (int i = 0; i + 1 < sim.st.A.size(); i++) {
            my_assert(sim.st.A[i] < sim.st.A[i + 1]);
        }
    }

    // 責務: node の現在 command 位置から最後まで replay し、追加手数と sort 完了状態を assert する。
    // 必要な理由: score は no-swap/range trim で更新しない保持値なので、不変条件の崩れを DEBUG で検出するため。
    static void validate_score_by_replay(const Node& node) {
        Node sim = node;
        int replay_cmd_count = 0;
        while (!sim.done) {
            my_assert(0 <= sim.cmd_idx && sim.cmd_idx < static_cast<int>(sim.cmds.size()));
            const command cmd = sim.cmds[sim.cmd_idx];
            sim.st.do_cmd(cmd);
            replay_cmd_count++;
            bool query_completed = advance_query_by_cmd(sim, cmd);
            if (query_completed) {
                skip_noop_a2a(sim);
                if (sim.current_q_idx == sim.range_r) {
                    debug_validate_range_end(sim, "validate_score_by_replay");
                    trim_queries(sim, sim.range_r);
                    set_range_and_cmds_done(sim);
                } else {
                    sim.cmd_idx++;
                    my_assert(sim.cmd_idx < static_cast<int>(sim.cmds.size()));
                }
                continue;
            }
            sim.cmd_idx++;
            my_assert(sim.cmd_idx < static_cast<int>(sim.cmds.size()));
        }
        replay_cmd_count += sim.queries_state.final_rot_info.second;
        append_final_rot(sim);
        my_assert(replay_cmd_count == node.score - static_cast<int>(node.past_cmds.size()));
        my_assert(sim.st.B.empty());
        for (int i = 0; i + 1 < sim.st.A.size(); i++) {
            my_assert(sim.st.A[i] < sim.st.A[i + 1]);
        }
    }
#endif

    // 責務: node に保持済みの score 昇順、同点なら swap 数降順で上位 BEAM_WIDTH 件だけ残す。
    // 必要な理由: score は no-swap/range trim では変わらず、swap 後再生成時だけ更新される保持値として扱うため。
    static void prune(my_vector<Node>& nodes) {
        std::stable_sort(nodes.begin(), nodes.end(), [](const Node& lhs, const Node& rhs) {
            const int lhs_score = lhs.score;
            const int rhs_score = rhs.score;
            if (lhs_score != rhs_score) return lhs_score < rhs_score;
            return lhs.swap_count > rhs.swap_count;
        });
        if (static_cast<int>(nodes.size()) > BEAM_WIDTH) {
            nodes.erase(nodes.begin() + BEAM_WIDTH, nodes.end());
        }
    }

    // 責務: 入力規模、元 command 数、初期 node の score/range/done を出力する。
    // 必要な理由: SwapBeamSearch::run が呼ばれ、探索開始状態が期待通りか確認できるようにするため。
    static void log_run_start(bool log_enabled,
                              const State& init_st,
                              const YakinamashiState<MAX_N>& yst,
                              const my_vector<command>& original_cmds,
                              const Node& init_node) {
        if (!log_enabled) return;
        out("swap_beam_log_run_start",
            "N", init_st.current_N,
            "query_count", static_cast<int>(yst.queries_state.queries.size()),
            "original_cmd_count", static_cast<int>(original_cmds.size()),
            "init_score", init_node.score,
            "init_done", init_node.done,
            "init_range_r", init_node.range_r,
            "init_cmd_count", static_cast<int>(init_node.cmds.size()));
    }

    // 責務: beam 数、expanded 数、完了数、次候補数を出力する。
    // 必要な理由: 探索が進んでいるか、完了 node が best 更新経路へ渡っているか確認できるようにするため。
    static void log_depth(bool log_enabled,
                          int current_turn,
                          const my_vector<Node>& beam,
                          const my_vector<Node>& expanded,
                          int done_count,
                          const my_vector<Node>& next) {
        if (!log_enabled) return;
        out("swap_beam_log_depth",
            "turn", current_turn,
            "beam", static_cast<int>(beam.size()),
            "expanded", static_cast<int>(expanded.size()),
            "done", done_count,
            "next_before_prune", static_cast<int>(next.size()));
    }

    // 責務: prune 後の候補数と先頭 node の score/swap 数を出力する。
    // 必要な理由: beam 幅制限と score 順ソートが期待通り動いているか確認できるようにするため。
    static void log_prune(bool log_enabled, int current_turn, const my_vector<Node>& next) {
        if (!log_enabled) return;
        if (next.empty()) {
            out("swap_beam_log_prune", "turn", current_turn, "next_after_prune", 0);
            return;
        }
        out("swap_beam_log_prune",
            "turn", current_turn,
            "next_after_prune", static_cast<int>(next.size()),
            "front_score", next.front().score,
            "front_swap_count", next.front().swap_count,
            "front_past", static_cast<int>(next.front().past_cmds.size()));
    }

    // 責務: best 更新時の score、swap 数、過去 command 数を出力する。
    // 必要な理由: done node が採用されているかと tie break の結果を確認できるようにするため。
    static void log_best_update(bool log_enabled, const Node& best) {
        if (!log_enabled) return;
        out("swap_beam_log_best_update",
            "score", best.score,
            "swap_count", best.swap_count,
            "past_cmd_count", static_cast<int>(best.past_cmds.size()),
            "final_rot", best.queries_state.final_rot_info.second);
    }

    // 責務: branch 種別、swap command、base score、新 score、採用可否を出力する。
    // 必要な理由: 追加 swap/ss 化が候補として残っているか、score filter で落ちているか確認できるようにするため。
    static void log_branch(const Node& node,
                           const char* branch_name,
                           command swap_cmd,
                           int base_score,
                           bool accepted,
                           int base_past_count,
                           int base_sum_turn) {
        if (!node.log_enabled) return;
        out("swap_beam_log_branch",
            "branch", branch_name,
            "cmd", swap_cmd,
            "accepted", accepted,
            "base_score", base_score,
            "score", node.score,
            "score_delta", node.score - base_score,
            "base_sum_turn", base_sum_turn,
            "sum_turn", node.queries_state.sum_turn,
            "sum_delta", node.queries_state.sum_turn - base_sum_turn,
            "base_past_count", base_past_count,
            "past_cmd_count", static_cast<int>(node.past_cmds.size()),
            "past_delta", static_cast<int>(node.past_cmds.size()) - base_past_count,
            "swap_count", node.swap_count,
            "remaining_queries", static_cast<int>(node.queries_state.queries.size()));
    }

    // 責務: 再生成対象 query 数、再生成後 query 数、sum_turn/final_rot を出力する。
    // 必要な理由: swap branch で current 以降だけ再生成されているか確認できるようにするため。
    static void log_rebuild(const Node& node, int old_suffix_count, int rebuilt_count) {
        if (!node.log_enabled) return;
        out("swap_beam_log_rebuild",
            "old_suffix_count", old_suffix_count,
            "rebuilt_count", rebuilt_count,
            "sum_turn", node.queries_state.sum_turn,
            "final_rot", node.queries_state.final_rot_info.second,
            "past_cmd_count", static_cast<int>(node.past_cmds.size()));
    }

    // 責務: 最終 best の score、swap 数、command 数を出力する。
    // 必要な理由: run 終了時に採用結果が期待通りか確認できるようにするため。
    static void log_run_end(bool log_enabled, const Node& best) {
        if (!log_enabled) return;
        out("swap_beam_log_run_end",
            "score", best.score,
            "swap_count", best.swap_count,
            "cmd_count", static_cast<int>(best.past_cmds.size()));
    }

    // 責務: suffix QueriesState の final_rot_info に従って State と command 列を進める。
    // 必要な理由: 物理 A を走査せず、再生成済み top OrdPos 由来の final rot を使うため。
    static void append_final_rot(Node& node) {
        my_assert(node.st.B.empty());
        const command final_cmd = node.queries_state.final_rot_info.first;
        const int final_cnt = node.queries_state.final_rot_info.second;
        my_assert(final_cnt >= 0);
        if (final_cnt == 0) return;
        my_assert(final_cmd == RA || final_cmd == RRA);
        for (int i = 0; i < final_cnt; i++) {
            if (final_cmd == RA) {
                node.st.ra();
            } else {
                node.st.rra();
            }
            node.past_cmds.push_back(final_cmd);
        }
    }

    // 責務: init state に command 列を適用し、B 空かつ A 昇順を assert する。
    // 必要な理由: query state を更新しない後処理なので、最終的な正しさは実 command の replay で保証するため。
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

    // 責務: 元手数、beam best 手数、最終 command 列を出力する。
    // 必要な理由: query_stats 廃止後も後処理の効果と最終列を確認できるようにするため。
    static void log_result(const my_vector<command>& original_cmds,
                           const Node& best) {
        out("swap_beam_original_len", static_cast<int>(original_cmds.size()));
        out("swap_beam_best_len", static_cast<int>(best.past_cmds.size()));
        out("swap_beam_cmds", best.past_cmds);
    }
};
