//› GreedyQueriesConstructor_doc.md
#include "Construction/GreedyQueriesConstructor.h"

#include "BOrder.h"
#include "BestRotInfo.h"
#include "FinalRotCalculator.h"
#include "Order/ExistAllOrdManager.h"
#include "Order/OrdPosCalculator.h"
#include "PrevA2ARotSum.h"
#include "QueriesModifier.h"
#include "Query/QueriesCalculator.h"
#include "Query/A2AQueryUtil.h"
#include "Query/QueryCommonRIFactory.h"
#include "Query/QueryStateApplyUtil.h"
#include "QueryCommonRI.h"
#include "RingBuffer500.h"
#include "YakinamashiQueryConstants.h"
#include "YakinamashiState.h"
#include "sort_algorithm/InitBeam/CircularRange.h"
#include "sort_algorithm/YakinamashiQuery/Construction/A2BB2ARangeSetLazySegTree.h"
#include "sort_algorithm/YakinamashiQuery/Validation/YakinamashiStateValidator.h"
#include "QueryProvider.h"
#include "AllOrdCalculator.h"
#include "Command/YstCommandsBuilder.h"
#include "Construction/EraseQueriesReBuilder.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include "QueryOrdEffect.h"
#include "util.h"

#ifndef DEBUG
#define out(...) ((void)0)
#endif

// 責務: timing log 用 ofstream を static に保持して返す。
// 必要な理由: ALNS 側と construct 側のログを同じ専用ファイルへ揃えるため。
static std::ofstream &construct_timing_log_stream() {
    static std::ofstream log_file;
    return log_file;
}

template<size_t MAX_N>
struct GreedyQueriesConstructor<MAX_N>::Helper {
    using G = GreedyQueriesConstructor<MAX_N>;
    using YakinamashiState = ::YakinamashiState<MAX_N>;
    using CircularRange = ChunkSeparator::CircularRange;

    static Helper &instance() {
        static Helper helper;
        return helper;
    }

    static constexpr size_t MAX_ALL_ORD_A = G::MAX_ALL_ORD_A;
    static constexpr size_t MAX_ALL_ORD_B = G::MAX_ALL_ORD_B;
    static constexpr size_t MAX_INSERT_SLOT_B = G::MAX_INSERT_SLOT_B;
    static constexpr size_t MAX_INSERT_EVENT_B = G::MAX_INSERT_EVENT_B;
    static constexpr int MAX_QUERY_CNT = G::MAX_QUERY_CNT;
    // 責務: qcnt/qcnt+1 個までの query index 列と prefix sum を保持する共通型を定義する。
    // 必要な理由: InsertPairPrecalc 周辺の qcnt 系配列を vec_array 化して heap allocation を減らすため。
    using QueryIntArray = vec_array<int, MAX_QUERY_CNT>;
    using QueryBoundaryArray = vec_array<int, MAX_QUERY_CNT + 1>;
    using QueryPrefixArray = vec_array<int, MAX_QUERY_CNT + 1>;
    using QueryExistAArray = vec_array<my_bitset<MAX_ALL_ORD_A>, MAX_QUERY_CNT + 1>;
    using QueryExistBArray = vec_array<my_bitset<MAX_ALL_ORD_B>, MAX_QUERY_CNT + 1>;
    using QueryCommonArray = vec_array<QueryCommon, MAX_QUERY_CNT>;
    using QueryOrdPosArray = vec_array<OrdPos, MAX_QUERY_CNT>;

    static constexpr const char *GREEDY_TRACE_PATH = "greedy_constructor_trace.txt";
    static constexpr const char *LEGACY_CHECK_TRACE_PATH = "legacy_check_trace.txt";
    static constexpr const char *GREEDY_RANGE_TRACE_PATH = "greedy_a2b_b_range_trace.txt";

    // #define LOG_PHYSICAL
    
    // #define LOG_A2A_CANDIDATE
    static constexpr const char *PHYSICAL_TRACE_PATH = "physical_destroy_trace.txt";

    bool g_trace_enabled = false;
    my_vector<std::string> g_last_construct_output_lines;
    CollectorLogMode current_collector_log_mode = CollectorLogMode::disabled;

    struct BandRangeInfo {
        bool has_band = false;
        CircularRange band_range = CircularRange(0, 0, 0);
    };

    using QueryBandRangeArray = vec_array<BandRangeInfo, MAX_QUERY_CNT>;

    static bool is_collector_log_enabled() {
        return instance().current_collector_log_mode == CollectorLogMode::enabled;
    }

    void apply_collector_log_mode(CollectorLogMode mode) {
        current_collector_log_mode = mode;
    }
    
    static void out2() {
        if (!is_collector_log_enabled()) return;
        if (output_file.is_open()) {
            output_file << "" << endl;
        } else {
            cout << "" << endl;
        }
    }

    template<class... Args>
    static void outaaa(const Args&... args) {
        if (!is_collector_log_enabled()) return;
        if (output_file.is_open()) {
            ((output_file << args << " "), ...);
            output_file << endl;
        } else {
            ((cout << args << " "), ...);
            cout << endl;
        }
    }

    // 責務: LOG_A2A_CANDIDATE 定義時にだけ A2A 候補確認ログを出力する。
    // 必要な理由: 通常時のログ量を増やさず、採用判断の確認だけを任意で見られるようにするため。
    template<class... Args>
    static void out_a2a_candidate_log(const Args&... args) {
#ifdef LOG_A2A_CANDIDATE
        if (output_file.is_open()) {
            ((output_file << args << " "), ...);
            output_file << endl;
        } else {
            ((cout << args << " "), ...);
            cout << endl;
        }
#else
        (void) sizeof...(args);
#endif
    }
    // 責務: DEBUG ビルド時だけ legacy check 用ログを legacy_check_trace.txt へ append する。
    // 必要な理由: assert で停止した時に、原因特定用ログをファイルから確実に読めるようにするため。
    template<class... Args>
    static void out_legacy_check_log(const Args&... args) {
#ifdef DEBUG
        std::ofstream log_file(LEGACY_CHECK_TRACE_PATH, std::ios::app);
        if (!log_file.is_open()) {
            cerr << "failed to open " << LEGACY_CHECK_TRACE_PATH << endl;
            return;
        }
        ((log_file << args << " "), ...);
        log_file << endl;
#else
        (void) sizeof...(args);
#endif
    }

    // 責務: DEBUG ビルド時だけ legacy_check_trace.txt を空にする。
    // 必要な理由: 古い iteration の decision ログと混ざらないよう、new 候補収集前にクリアするため。
    static void clear_legacy_check_trace() {
#ifdef DEBUG
        std::ofstream log_file(LEGACY_CHECK_TRACE_PATH, std::ios::trunc);
        if (!log_file.is_open()) {
            cerr << "failed to clear " << LEGACY_CHECK_TRACE_PATH << endl;
        }
#endif
    }
    void append_physical_trace_line(const std::string &line) {
#ifdef LOG_PHYSICAL
        std::ofstream log_file(PHYSICAL_TRACE_PATH, std::ios::app);
        if (!log_file.is_open()) return;
        log_file << line << '\n';
#else
        (void) line;
#endif
    }

    bool is_b_all_ord_inside_band(const BandRangeInfo &info, int pushed_b_all_ord) {
        if (!info.has_band) return false;
        return info.band_range.contains(pushed_b_all_ord);
    }

    int pick_probe_b_all_ord(const BandRangeInfo &info, int insert_slot_size, bool want_inside) {
        my_assert(0 < insert_slot_size);
        if (want_inside) {
            my_assert(info.has_band);
            return info.band_range.get_start();
        }
        for (int pushed_b_all_ord = 0; pushed_b_all_ord < insert_slot_size; pushed_b_all_ord++) {
            if (!is_b_all_ord_inside_band(info, pushed_b_all_ord)) return pushed_b_all_ord;
        }
        my_assert(false);
        return 0;
    }

    template<class T>
    string indirect_debug_to_string(const T &value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

    struct IndirectMeasureResult {
        int before = 0;
        int after = 0;
        int diff = 0;
        YakinamashiState yst;
    };

    vector<int> build_indirect_b_target_orders(const BOrder &border, int n) {
        vector<int> ret(n, -1);
        for (int ord = 0; ord < border.get_all_order_entries_size(); ord++)
            ret[border.get_all_order_entries()[ord].value] = ord;
        return ret;
    }

    struct QueryScanState {
        State st;
        my_bitset<MAX_ALL_ORD_A> exist_a_ords;
        my_bitset<MAX_ALL_ORD_B> exist_b_ords;
        my_vector<ValState> now_a_val_state;
        my_vector<ValState> now_b_val_state;
    };

    enum class BestInsertPlanKind {
        insert_pair,
        single_a2a
    };

    struct BestInsertPlan {
        bool found = false;
        BestInsertPlanKind kind = BestInsertPlanKind::insert_pair;
        int best_score = INT_MAX;
        int best_a2b_idx = -1;
        int best_b2a_idx = -1;
        int best_a2a_idx = -1;
        int best_a2a_init_rank = -1;
        int best_a2a_target_rank = -1;
        AllOrd best_pushed_b_all_ord = AllOrd(0);
        std::optional<QueryCommon> best_a2b_query;
        std::optional<QueryCommon> best_b2a_query;
        std::optional<QueryCommon> best_a2a_query;
    };

//  snapshot 上の best plan から v の処理順だけを作れるよう、先頭 query の位置とコストを保持する。
    struct VOrderPlanInfo {
        int v = -1;
        int destroyed_cost = 0;
        BestInsertPlanKind kind = BestInsertPlanKind::insert_pair;
        int best_score = INT_MAX;
        int front_idx = -1;
        int front_cost = INT_MAX;
    };

    struct NaiveEvalResult {
        int score = INT_MAX;
        std::optional<QueryCommon> a2b_query;
        std::optional<QueryCommon> b2a_query;
#ifdef DEBUG
        QRIList queries;
        int shifted_b2a_idx = -1;
#endif 
    };

    // 責務: 再 destroy 候補の v、元 destroyed_cost、現在 query 手数合計、A2B/B2A query 手数合計を保持する。
    // 必要な理由: A2B/B2A を持つ v だけにソート用の下駄を履かせ、既存 construct 用の (v,cost) へ戻すため。
    struct VQueryTurnInfo {
        int v = -1;
        int destroyed_cost = 0;
        int query_turn_sum = 0;
        int ab_query_turn_sum = 0;
    };

//  0手 A2A 候補の発見位置と相対 ord を保持する。
    struct NoMoveA2AInfo {
        int q_idx = -1;
        int init_rank = -1;
        int target_rank = -1;
    };

//  v ごとの候補 pair と common B range を後から trace で見返せるようにする。
    struct PairProfile {
        int a2b_idx = -1;
        int b2a_idx = -1;
        int common_b_all_ord_count = 0;
        int common_b_range_count = 0;
        std::array<CircularRange, 2> common_b_ranges = {
                CircularRange(0, 0, 0),
                CircularRange(0, 0, 0)
        };
        int naive_eval_count = 0;
    };

//  pair 数だけでなく具体的な組一覧も v 単位で残し、重いケースを trace から特定しやすくする。
    struct VProfile {
        int v = -1;
        int destroyed_cost = 0;
        int candidate_pair_count = 0;
        my_vector<std::pair<int, int>> candidate_pairs;
        int naive_eval_count = 0;
        int best_a2b_idx = -1;
        int best_b2a_idx = -1;
        int best_pushed_b_all_ord = -1;
        bool best_is_single_a2a = false;
        my_vector<PairProfile> pair_profiles;
    };


    bool is_trace_enabled() {
        return g_trace_enabled;
    }

    static constexpr int AB_QUERY_TURN_BONUS = 10000;

    void append_trace_line(const std::string &line) {
#ifndef DEBUG
        return;
#endif
        if (!is_trace_enabled()) return;
        std::ofstream log_file(GREEDY_TRACE_PATH, std::ios::app);
        if (log_file.is_open()) log_file << line << '\n';
    }

//  output.txt 向けの construct 詳細を前回実行ぶんだけ保持し、trace を切った実験でも pair 情報を見えるようにする。
    void clear_last_construct_output_lines() {
#ifndef DEBUG
        return;
#endif
        g_last_construct_output_lines.clear();
    }

//  output.txt にも trace と同じ v 単位の詳細を流し、pair 数だけでなく best pair も同じ書式で追えるようにする。
    std::string to_construct_profile_v_line(const VProfile &v_profile) {
        std::stringstream ss;
        ss << "construct_profile_v v=" << v_profile.v
           << " destroyed_cost=" << v_profile.destroyed_cost
           << " candidate_pair_count=" << v_profile.candidate_pair_count
           << " naive_eval_count=" << v_profile.naive_eval_count
           << " best_kind=" << (v_profile.best_is_single_a2a ? "single_a2a" : "pair")
           << " best_a2b_idx=" << v_profile.best_a2b_idx
           << " best_b2a_idx=" << v_profile.best_b2a_idx
           << " best_pushed_b_all_ord=" << v_profile.best_pushed_b_all_ord;
        return ss.str();
    }

//  candidate pair の全列挙を output.txt 側にも残し、trace を開かずに組の全件を確認できるようにする。
    std::string to_construct_profile_candidate_pairs_line(const VProfile &v_profile) {
        return
                "construct_profile_candidate_pairs v=" + std::to_string(v_profile.v) +
                " pairs=" + to_candidate_pairs_string(v_profile.candidate_pairs);
    }

//  pair ごとの common B range と評価回数を output.txt へも流し、重い pair を実験ログから直接読めるようにする。
    std::string to_construct_profile_pair_line(int v, const PairProfile &pair_profile) {
        std::stringstream ss;
        ss << "construct_profile_pair v=" << v
           << " a2b_idx=" << pair_profile.a2b_idx
           << " b2a_idx=" << pair_profile.b2a_idx
           << " common_b_range_count=" << pair_profile.common_b_range_count
           << " common_b_ranges=" << to_common_b_ranges_string(pair_profile)
           << " common_b_all_ord_count=" << pair_profile.common_b_all_ord_count
           << " naive_eval_count=" << pair_profile.naive_eval_count;
        return ss.str();
    }

//  construct 全体の探索量も 1 行に残し、詳細行の末尾で iter ごとの重さを比較できるようにする。
    std::string to_construct_profile_total_line(
            int destroyed_v_count, int total_candidate_pair_count, int total_naive_eval_count
    ) {
        std::stringstream ss;
        ss << "construct_profile_total destroyed_v_count=" << destroyed_v_count
           << " total_candidate_pair_count=" << total_candidate_pair_count
           << " total_naive_eval_count=" << total_naive_eval_count;
        return ss.str();
    }

//  output.txt にも v 単位の詳細をまとめて出せるよう、trace と同じ粒度の行列を保持する。
    void append_last_construct_output_lines(const VProfile &v_profile) {
#ifndef DEBUG
        return;
#endif
        g_last_construct_output_lines.push_back(to_construct_profile_v_line(v_profile));
        g_last_construct_output_lines.push_back(to_construct_profile_candidate_pairs_line(v_profile));
        for (const PairProfile &pair_profile: v_profile.pair_profiles) {
            g_last_construct_output_lines.push_back(to_construct_profile_pair_line(v_profile.v, pair_profile));
        }
    }

//  v ごとの profile 集計を探索本体から切り離し、更新箇所を減らす。
    VProfile init_v_profile(int v, int destroyed_cost) {
        VProfile profile;
        profile.v = v;
        profile.destroyed_cost = destroyed_cost;
        return profile;
    }

//  common B の区間情報を 1 行へ潰し、pair 詳細ログを読みやすくする。
    std::string to_common_b_ranges_string(const PairProfile &pair_profile) {
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < pair_profile.common_b_range_count; i++) {
            if (i != 0) ss << ", ";
            const CircularRange &range = pair_profile.common_b_ranges[i];
            ss << "(start=" << range.get_start() << ",size=" << range.size() << ")";
        }
        ss << "]";
        return ss.str();
    }

//  候補 pair の全列挙を 1 行で出し、件数と実体を同時に照合できるようにする。
    std::string to_candidate_pairs_string(const my_vector<std::pair<int, int>> &pairs) {
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < static_cast<int>(pairs.size()); i++) {
            if (i != 0) ss << ", ";
            ss << "(a2b=" << pairs[i].first << ",b2a=" << pairs[i].second << ")";
        }
        ss << "]";
        return ss.str();
    }

//  pair ごとの range と評価回数を v ログの直下へ並べ、重い組を追跡しやすくする。
    void append_trace_pair_profile(int v, const PairProfile &pair_profile) {
        append_trace_line(to_construct_profile_pair_line(v, pair_profile));
    }

//  best plan と候補 pair 一覧をまとめて出し、v 単位で探索量を見返せるようにする。
    void append_trace_v_profile(const VProfile &v_profile) {
        append_trace_line(to_construct_profile_v_line(v_profile));
        append_trace_line(to_construct_profile_candidate_pairs_line(v_profile));
        for (const PairProfile &pair_profile: v_profile.pair_profiles)
            append_trace_pair_profile(v_profile.v, pair_profile);
    }

//  best plan の種類を trace 用 profile へ移し替え、single_a2a を pair と混同しないようにする。
    void finalize_v_profile_from_best_plan(VProfile &v_profile, const BestInsertPlan &best_plan) {
        v_profile.best_is_single_a2a = (best_plan.kind == BestInsertPlanKind::single_a2a);
        if (v_profile.best_is_single_a2a) return;
        v_profile.best_a2b_idx = best_plan.best_a2b_idx;
        v_profile.best_b2a_idx = best_plan.best_b2a_idx;
        v_profile.best_pushed_b_all_ord = best_plan.best_pushed_b_all_ord.get();
    }

//  再構築途中の type 分布と block 数を 1 つの集計値で扱えるようにする。
    struct QueryTypeStats {
        int a2b_cnt = 0;
        int b2a_cnt = 0;
        int a2a_cnt = 0;
        int block_cnt = 0;
    };

//  挿入途中の query 構造がどの時点で細切れ化したかを score と同時に残す。
    QueryTypeStats calc_query_type_stats(const QueriesState &queries_state) {
        QueryTypeStats stats;
        CommonPushType prev_type = CommonPushType::A2B;
        bool has_prev = false;
        for (const auto &qri: queries_state.queries) {
            CommonPushType cur_type = qri.get_push_type();
            if (cur_type == CommonPushType::A2B) stats.a2b_cnt++;
            if (cur_type == CommonPushType::B2A) stats.b2a_cnt++;
            if (qri.is_a2a_type()) stats.a2a_cnt++;
            if (!has_prev || cur_type != prev_type) stats.block_cnt++;
            prev_type = cur_type;
            has_prev = true;
        }
        return stats;
    }

//  trace 1 行で block 増加を比較できるようにする。
    std::string to_query_type_stats_string(const QueryTypeStats &stats) {
        std::stringstream ss;
        ss << "a2b=" << stats.a2b_cnt
           << " b2a=" << stats.b2a_cnt
           << " a2a=" << stats.a2a_cnt
           << " blocks=" << stats.block_cnt;
        return ss.str();
    }

//  再構築順と destroyed_cost を 1 行で見返せるようにする。
    std::string to_destroyed_vs_string(const my_vector<std::pair<int, int>> &destroyed_vs) {
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < static_cast<int>(destroyed_vs.size()); i++) {
            if (i != 0) ss << ", ";
            ss << "(" << destroyed_vs[i].first << ":" << destroyed_vs[i].second << ")";
        }
        ss << "]";
        return ss.str();
    }

    std::string to_query_string(const QueryCommonRI &qri) {
        std::stringstream ss;
        ss << qri;
        return ss.str();
    }

    std::string to_query_common_string(const QueryCommon &q) {
        std::stringstream ss;
        ss << q;
        return ss.str();
    }

// _REVIEW: construct_v の選択結果を、挿入後 query 列の局所窓で確認するため。
    void out_construct_v_insert_window(
            const QueriesState &queries_state,
            const std::string &label,
            int center_idx,
            int inserted_a2b_idx,
            int inserted_b2a_idx
    ) {
        if (!is_collector_log_enabled()) return;
        int query_count = static_cast<int>(queries_state.queries.size());
        int begin_idx = std::max(0, center_idx - 5);
        int end_idx = std::min(query_count, center_idx + 6);
        outaaa("construct_v_insert_window_begin", label);
        for (int q_idx = begin_idx; q_idx < end_idx; q_idx++) {
            std::string mark = "";
            if (q_idx == inserted_a2b_idx || q_idx == inserted_b2a_idx) mark = "@";
            outaaa(mark + "q_idx", q_idx, to_query_string(queries_state.queries[q_idx]));
        }
        outaaa("construct_v_insert_window_end", label);
    }

//  挿入後の query 列そのものを見返せるように、trace 出力を helper 化する。
    void append_trace_queries(const QueriesState &queries_state, const std::string &label) {
        append_trace_line(label + "_queries_begin");
        for (int i = 0; i < static_cast<int>(queries_state.queries.size()); i++) {
            append_trace_line(label + "_q[" + std::to_string(i) + "]=" + to_query_string(queries_state.queries[i]));
        }
        append_trace_line(label + "_queries_end");
    }

    template<class Stack>
    std::string to_stack_string(const Stack &stack) {
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < static_cast<int>(stack.size()); i++) {
            if (i != 0) ss << ", ";
            ss << stack[i];
        }
        ss << "]";
        return ss.str();
    }

    std::string physical_state_to_string(const State &st) {
        std::stringstream ss;
        ss << "current_N=" << st.current_N
           << " initial_N=" << st.initial_N
           << " A=" << to_stack_string(st.A)
           << " B=" << to_stack_string(st.B);
        return ss.str();
    }

    std::string physical_order_summary_to_string(const YakinamashiState &yst) {
        std::stringstream ss;
        ss << "a_entries=" << yst.query_order.aorder.get_all_order_entries_size()
           << " b_entries=" << yst.query_order.border.get_all_order_entries_size();
        return ss.str();
    }

    std::string physical_order_window_to_string(
            const my_vector<OrderEntry> &entries,
            int center_pos,
            int radius = 4
    ) {
        if (center_pos < 0) return "[]";
        int begin_pos = std::max(0, center_pos - radius);
        int end_pos = std::min(static_cast<int>(entries.size()), center_pos + radius + 1);
        std::stringstream ss;
        ss << "[";
        for (int i = begin_pos; i < end_pos; i++) {
            if (i != begin_pos) ss << ", ";
            ss << i << ":" << entries[i];
        }
        ss << "]";
        return ss.str();
    }

    std::string physical_query_summary_to_string(const YakinamashiState &yst) {
        std::stringstream ss;
        ss << "current_N=" << yst.queries_state.current_N
           << " query_count=" << yst.queries_state.queries.size()
           << " sum_turn=" << yst.queries_state.sum_turn
           << " final_rot_cmd=" << yst.queries_state.final_rot_info.first
           << " final_rot_cnt=" << yst.queries_state.final_rot_info.second;
        return ss.str();
    }

    std::string physical_erased_values_to_string(const EraseOrderSnapshot<MAX_N> &snapshot) {
        std::stringstream ss;
        ss << "[";
        bool first = true;
        for (int v = 0; v < static_cast<int>(MAX_N); v++) {
            if (!snapshot.is_erased_v[v]) continue;
            if (!first) ss << ",";
            first = false;
            ss << v;
        }
        ss << "]";
        return ss.str();
    }

    void log_construct_physical_begin(
            const PhysicalDestroyContext<MAX_N> &context,
            const my_vector<std::pair<int, int>> &sorted_vs
    ) {

#ifndef LOG_PHYSICAL
        return;
#endif
        append_physical_trace_line("=== physical_construct_begin ===");
        append_physical_trace_line("construct_sorted_vs " + to_destroyed_vs_string(sorted_vs));
        append_physical_trace_line("construct_snapshot_erased_vs " +
                                   physical_erased_values_to_string(context.erase_order_snapshot));
        append_physical_trace_line("construct_begin_state " + physical_state_to_string(context.current_st));
        append_physical_trace_line("construct_begin_query " + physical_query_summary_to_string(context.yst));
        append_physical_trace_line("construct_begin_order " + physical_order_summary_to_string(context.yst));
    }

    void log_construct_physical_end(const State &init_st, const PhysicalDestroyContext<MAX_N> &context) {

#ifndef LOG_PHYSICAL
        return;
#endif
        append_physical_trace_line("construct_end_state " + physical_state_to_string(context.current_st));
        append_physical_trace_line("construct_end_query " + physical_query_summary_to_string(context.yst));
        append_physical_trace_line("construct_end_order " + physical_order_summary_to_string(context.yst));
        my_vector<command> cmds = YstCommandsBuilder::build_all_commands_from_state(
                init_st, context.yst.queries_state
        );
        append_physical_trace_line("construct_end_commands cmd_count=" + std::to_string(cmds.size())
                                   + " sum_turn=" + std::to_string(context.yst.queries_state.sum_turn)
                                   + " count_matches_sum_turn=" +
                                   std::to_string(static_cast<int>(cmds.size()) == context.yst.queries_state.sum_turn));
        append_physical_trace_line("=== physical_construct_end ===");
    }

    void log_construct_v_begin(
            const PhysicalDestroyContext<MAX_N> &context,
            int v
    ) {

#ifndef LOG_PHYSICAL
        return;
#endif
        append_physical_trace_line("--- physical_construct_v_begin v=" + std::to_string(v) + " ---");
        append_physical_trace_line("construct_v_begin_state " + physical_state_to_string(context.current_st));
        append_physical_trace_line("construct_v_begin_query " + physical_query_summary_to_string(context.yst));
        append_physical_trace_line("construct_v_begin_order " + physical_order_summary_to_string(context.yst));
    }

    void log_restore_order_entry(
            const PhysicalDestroyContext<MAX_N> &context,
            int v,
            bool is_erased,
            int init_pos,
            int target_pos
    ) {

#ifndef LOG_PHYSICAL
        return;
#endif
        append_physical_trace_line("restore_order_entry v=" + std::to_string(v)
                                   + " is_erased=" + std::to_string(is_erased)
                                   + " init_pos=" + std::to_string(init_pos)
                                   + " target_pos=" + std::to_string(target_pos)
                                   + " order=" + physical_order_summary_to_string(context.yst));
        const auto& a_entries = context.yst.query_order.aorder.get_all_order_entries();
        append_physical_trace_line("restore_order_entry_init_window v=" + std::to_string(v)
                                   + " " + physical_order_window_to_string(a_entries, init_pos));
        append_physical_trace_line("restore_order_entry_target_window v=" + std::to_string(v)
                                   + " " + physical_order_window_to_string(a_entries, target_pos));
    }

    void log_add_v_stack(
            const State &init_st,
            const State &current_st,
            int v,
            const RingBuffer500 &add_v_stack
    ) {

#ifndef LOG_PHYSICAL
        return;
#endif
        append_physical_trace_line("build_add_v_stack v=" + std::to_string(v)
                                   + " init_A=" + to_stack_string(init_st.A)
                                   + " before_A=" + to_stack_string(current_st.A)
                                   + " after_A=" + to_stack_string(add_v_stack));
    }

    void log_restore_state_value(
            const State &before_st,
            const State &after_st,
            int v
    ) {

#ifndef LOG_PHYSICAL
        return;
#endif
        append_physical_trace_line("restore_state_value v=" + std::to_string(v));
        append_physical_trace_line("restore_state_before " + physical_state_to_string(before_st));
        append_physical_trace_line("restore_state_after " + physical_state_to_string(after_st));
    }

    void log_rebuild_queries_for_state_begin(
            const State &current_st,
            const YakinamashiState &yst
    ) {

#ifndef LOG_PHYSICAL
        return;
#endif
        append_physical_trace_line("rebuild_queries_for_state_begin state=" + physical_state_to_string(current_st));
        append_physical_trace_line("rebuild_queries_for_state_begin query=" + physical_query_summary_to_string(yst));
    }

    void log_rebuild_queries_for_state_step(
            int q_idx,
            const QueryCommonRI &old_qri,
            const QueryCommon &new_q,
            const QueryCommonRI &new_ri
    ) {
#ifndef LOG_PHYSICAL
        return;
#endif
        append_physical_trace_line("rebuild_queries_for_state_step q_idx=" + std::to_string(q_idx)
                                   + " old_qri=" + to_query_string(old_qri)
                                   + " new_q=" + to_query_common_string(new_q)
                                   + " new_ri=" + to_query_string(new_ri));
    }

    struct InsertRangeSet {
        my_vector<std::pair<int, int>> ranges;
    };

    struct InitNeighborVs {
        int prev_v = -1;
        int next_v = -1;
    };

    struct TargetNeighborVs {
        int prev_v = -1;
        int next_v = -1;
    };

    void log_rebuild_queries_for_state_end(
            const State &current_st,
            const YakinamashiState &yst
    ) {

#ifndef LOG_PHYSICAL
        return;
#endif
        append_physical_trace_line("rebuild_queries_for_state_end state=" + physical_state_to_string(current_st));
        append_physical_trace_line("rebuild_queries_for_state_end query=" + physical_query_summary_to_string(yst));
    }

    void log_construct_v_end(
            const PhysicalDestroyContext<MAX_N> &context,
            int v,
            int before_sum_turn,
            int before_query_count
    ) {

#ifndef LOG_PHYSICAL
        return;
#endif
        append_physical_trace_line("construct_v_end v=" + std::to_string(v)
                                   + " before_sum_turn=" + std::to_string(before_sum_turn)
                                   + " after_sum_turn=" + std::to_string(context.yst.queries_state.sum_turn)
                                   + " before_query_count=" + std::to_string(before_query_count)
                                   + " after_query_count=" +
                                   std::to_string(context.yst.queries_state.queries.size()));
        append_physical_trace_line("construct_v_end_state " + physical_state_to_string(context.current_st));
        append_physical_trace_line("construct_v_end_query " + physical_query_summary_to_string(context.yst));
        append_physical_trace_line("construct_v_end_order " + physical_order_summary_to_string(context.yst));
        append_physical_trace_line("--- physical_construct_v_end v=" + std::to_string(v) + " ---");
    }

    std::string to_order_entries_string(const my_vector<OrderEntry> &entries) {
        // 責務: debug 出力のためだけに呼ばれる文字列化 helper を、trace 有効/無効に関係なく利用可能にする。
        // 必要な理由: エラー状況を出力しようとした時に、状況説明ではなく helper 冒頭の assert で停止しないようにするため。
        if (entries.empty()) return "[]";
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            if (i != 0) ss << ", ";
            ss << i << ":" << entries[i];
        }
        ss << "]";
        return ss.str();
    }

    template<class Stack>
    std::string to_stack_value_state_string(
            const Stack &stack, const my_vector<ValState> &val_states
    ) {
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < static_cast<int>(stack.size()); i++) {
            if (i != 0) ss << ", ";
            int v = stack[i];
            ss << v << "(" << val_states[v] << ")";
        }
        ss << "]";
        return ss.str();
    }

//  失敗ログで idx 群を 1 行比較できるようにする。
    std::string join_debug_ints(const my_vector<int> &values) {
        if (values.empty()) return "none";
        std::stringstream ss;
        for (int i = 0; i < static_cast<int>(values.size()); i++) {
            if (i != 0) ss << ",";
            ss << values[i];
        }
        return ss.str();
    }

    void append_trace_b2a_candidates(
            int b2a_idx, const my_vector<int> &raw_candidates, const my_vector<int> &ordered_candidates
    ) {
        append_trace_line(
                "  b2a_candidates q" + std::to_string(b2a_idx) +
                ": raw=[" + join_debug_ints(raw_candidates) + "], usable=[" + join_debug_ints(ordered_candidates) + "]"
        );
    }

    void append_trace_best_update(
            int a2b_idx, int b2a_idx, AllOrd pushed_b_all_ord, int score
    ) {
        std::stringstream ss;
        ss << "  best_update: a2b=q" << a2b_idx
           << ", b2a=q" << b2a_idx
           << ", pushed_b_all_ord=" << pushed_b_all_ord.get()
           << ", score=" << score;
        append_trace_line(ss.str());
    }

    void append_trace_selected_pair(const BestInsertPlan &best_plan) {
        if (!best_plan.found) return;
        std::stringstream ss;
        ss << "selected_pair: a2b=q" << best_plan.best_a2b_idx
           << ", b2a=q" << best_plan.best_b2a_idx
           << ", pushed_b_all_ord=" << best_plan.best_pushed_b_all_ord.get()
           << ", score=" << best_plan.best_score;
        append_trace_line(ss.str());
    }

//  snapshot 順序と実際の再探索結果のずれを 1 行で見返せるようにする。
    void append_trace_ordered_step(
            const State &init_st,
            const QueriesState &queries_state,
            int step_idx,
            const VOrderPlanInfo &ordered_info,
            const BestInsertPlan &actual_best_plan
    ) {
        std::stringstream ss;
        ss << "ordered_step step=" << step_idx
           << " v=" << ordered_info.v
           << " snapshot_best_score=" << ordered_info.best_score
           << " snapshot_front_idx=" << ordered_info.front_idx
           << " snapshot_front_cost=" << ordered_info.front_cost
           << " actual_front_idx=" << get_front_idx(actual_best_plan)
           << " actual_front_cost=" << get_front_cost(init_st, queries_state, actual_best_plan)
           << " actual_kind=" << (actual_best_plan.kind == BestInsertPlanKind::single_a2a ? "a2a" : "pair");
        append_trace_line(ss.str());
    }

//  pair と a2a only で先頭 query の位置が違うので、順序付け側から同じ API で取れるようにする。
    int get_front_idx(const BestInsertPlan &best_plan) {
        my_assert(best_plan.found);
        if (best_plan.kind == BestInsertPlanKind::single_a2a) return best_plan.best_a2a_idx;
        return best_plan.best_a2b_idx;
    }

//  tie-break は先頭 query の実手数だけを見るので、snapshot の QueriesState から RI 化して取る。
    int get_front_cost(const State &init_st, const QueriesState &queries_state, const BestInsertPlan &best_plan) {
        my_assert(best_plan.found);
        QCRIFactory qri_factory;
        if (best_plan.kind == BestInsertPlanKind::single_a2a) {
            my_assert(best_plan.best_a2a_query.has_value());
            return qri_factory.build_ri(queries_state, *best_plan.best_a2a_query,
                                        best_plan.best_a2a_idx).get_ins_turn();
        }
        my_assert(best_plan.best_a2b_query.has_value());
        return qri_factory.build_ri(queries_state, *best_plan.best_a2b_query, best_plan.best_a2b_idx).get_ins_turn();
    }

//  次段階で snapshot から ordered_vs を作る時に、plan 依存の key 生成を一箇所へ寄せる。
    VOrderPlanInfo build_v_order_plan_info(
            const State &init_st, const QueriesState &queries_state, int v, int destroyed_cost,
            const BestInsertPlan &best_plan
    ) {
        VOrderPlanInfo info;
        info.v = v;
        info.destroyed_cost = destroyed_cost;
        info.kind = best_plan.kind;
        info.best_score = best_plan.best_score;
        info.front_idx = get_front_idx(best_plan);
        info.front_cost = get_front_cost(init_st, queries_state, best_plan);
        return info;
    }

//  順序変更の本体を入れる前に、snapshot 上の規則が安定しているか trace だけで確認できるようにする。
    std::string to_v_order_plan_infos_string(const my_vector<VOrderPlanInfo> &ordered_infos) {
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < static_cast<int>(ordered_infos.size()); i++) {
            if (i != 0) ss << ", ";
            const VOrderPlanInfo &info = ordered_infos[i];
            ss << "(v=" << info.v
               << ", best_score=" << info.best_score
               << ", front_idx=" << info.front_idx
               << ", front_cost=" << info.front_cost
               << ", kind=" << (info.kind == BestInsertPlanKind::single_a2a ? "a2a" : "pair")
               << ")";
        }
        ss << "]";
        return ss.str();
    }

//  順序付けを destroyed_cost のみへ絞り、他の key は見ないことで比較を軽くする。
// _REVIEW_BEGIN: std::sort から this なしで呼ぶため、状態を読まない比較関数に限定して static 化する。
    static bool is_v_order_plan_info_prior(const VOrderPlanInfo &lhs, const VOrderPlanInfo &rhs) {
        return lhs.destroyed_cost > rhs.destroyed_cost;
    }
// _REVIEW_END

//  順序決定では探索せず、destroyed_vs の情報だけで v 順を作る。
    my_vector<VOrderPlanInfo> build_ordered_v_plan_infos_from_snapshot(
            const YakinamashiState &yst, const State &init_st, const my_vector<std::pair<int, int>> &destroyed_vs
    ) {
        my_vector<VOrderPlanInfo> ordered_infos;
        ordered_infos.reserve(destroyed_vs.size());
        for (const auto &[v, destroyed_cost]: destroyed_vs) {
            VOrderPlanInfo info;
            info.v = v;
            info.destroyed_cost = destroyed_cost;
            ordered_infos.push_back(info);
        }
        std::sort(ordered_infos.begin(), ordered_infos.end(), is_v_order_plan_info_prior);
        return ordered_infos;
    }

//  construct 単位の開始状態を残し、どの順で壊れ始めたかを case/iter と対応付けやすくする。
    void append_trace_construct_begin(
            const YakinamashiState &yst, const my_vector<std::pair<int, int>> &sorted_vs
    ) {
        QueryTypeStats stats = calc_query_type_stats(yst.queries_state);
        append_trace_line("construct_begin score=" + std::to_string(yst.queries_state.sum_turn));
        append_trace_line("construct_begin_query_count=" + std::to_string(yst.queries_state.queries.size()));
        append_trace_line("construct_begin_destroyed_vs=" + to_destroyed_vs_string(sorted_vs));
        append_trace_line("construct_begin_query_type_stats=" + to_query_type_stats_string(stats));
        append_trace_queries(yst.queries_state, "construct_begin");
    }

//  各 v 挿入直後の score と block 数を残し、後段挿入で悪化した点を実例で追えるようにする。
    void append_trace_after_insert(
            const YakinamashiState &yst, int step_idx, int v, int destroyed_cost
    ) {
        QueryTypeStats stats = calc_query_type_stats(yst.queries_state);
        append_trace_line(
                "after_insert step=" + std::to_string(step_idx) +
                " v=" + std::to_string(v) +
                " destroyed_cost=" + std::to_string(destroyed_cost) +
                " score=" + std::to_string(yst.queries_state.sum_turn) +
                " query_count=" + std::to_string(yst.queries_state.queries.size())
        );
        append_trace_line("after_insert_query_type_stats=" + to_query_type_stats_string(stats));
        append_trace_queries(yst.queries_state, "after_insert");
    }

//  0手 A2A 候補の発見位置を trace に残す。
    void append_trace_no_move_a2a(const NoMoveA2AInfo &info) {
        std::stringstream ss;
        ss << "  no_move_a2a q" << info.q_idx
           << ": init_rank=" << info.init_rank
           << ", target_rank=" << info.target_rank;
        append_trace_line(ss.str());
    }

    my_vector<std::pair<int, int>> sort_destroyed_vs_by_cost_desc(
            const my_vector<std::pair<int, int>> &destroyed_vs
    ) {
        my_vector<std::pair<int, int>> sorted_vs = destroyed_vs;
        std::sort(sorted_vs.begin(), sorted_vs.end(), [](const auto &lhs, const auto &rhs) {
            if (lhs.second != rhs.second) return lhs.second > rhs.second;
            return lhs.first < rhs.first;
        });
        return sorted_vs;
    }

    // 責務: v の AOrder INIT all_ord と TARGET all_ord の 0..entry_size-1 円環上の最短距離を返す。
    // 必要な理由: gap 優先 construct で、単純な差ではなく AllOrd 円環上で離れた v を先に処理するため。
    int calc_a_order_gap(
            const YakinamashiState &yst,
            int v
    ) {
        const int entry_size = yst.query_order.aorder.get_all_order_entries_size();
        my_assert(0 < entry_size);
        const int init_all_ord = yst.query_order.aorder.get_init_all_order(v).get();
        const int target_all_ord = yst.query_order.aorder.get_target_order(v).get();
        const int diff = target_all_ord < init_all_ord
                         ? init_all_ord - target_all_ord
                         : target_all_ord - init_all_ord;
        return std::min(diff, entry_size - diff);
    }

    // 責務: destroyed_vs を AOrder 円環 gap 降順、destroyed_cost 降順、v 昇順で並べる。
    // 必要な理由: 既存の destroyed_cost 順とは別に、AOrder の INIT/TARGET 差が大きい v を先に construct できるようにするため。
    my_vector<std::pair<int, int>> sort_destroyed_vs_by_a_gap_desc(
            const YakinamashiState &yst,
            const my_vector<std::pair<int, int>> &destroyed_vs
    ) {
        my_vector<std::pair<int, int>> sorted_vs = destroyed_vs;
        std::sort(sorted_vs.begin(), sorted_vs.end(), [&](const auto &lhs, const auto &rhs) {
            const int lhs_gap = calc_a_order_gap(yst, lhs.first);
            const int rhs_gap = calc_a_order_gap(yst, rhs.first);
            if (lhs_gap != rhs_gap) return lhs_gap > rhs_gap;
            if (lhs.second != rhs.second) return lhs.second > rhs.second;
            return lhs.first < rhs.first;
        });
        return sorted_vs;
    }

    // 責務: construct 順ソートでだけ使う一時 priority を返す。
    // 必要な理由: 下駄つき合計を保存せず、後続処理で重み以外の意味に使われないようにするため。
    int calc_construct_turn_priority(const VQueryTurnInfo &info) {
        int priority = info.query_turn_sum;
        if (0 < info.ab_query_turn_sum) priority += AB_QUERY_TURN_BONUS;
        return priority;
    }

    // 責務: destroyed_vs を現在 query 列の手数 priority 降順、destroyed_cost 降順、v 昇順で並べる。
    // 必要な理由: sort_destroyed_vs_for_construct の通常経路を、誰が読んでも query cost 順だと分かる関数にするため。
    my_vector<std::pair<int, int>> sort_destroyed_vs_by_query_turn_desc(
            const YakinamashiState &yst,
            const my_vector<std::pair<int, int>> &destroyed_vs,
            int initial_n
    ) {
        my_vector<VQueryTurnInfo> infos = collect_v_query_turn_infos(yst, destroyed_vs, initial_n);
        std::sort(infos.begin(), infos.end(), [&](const auto &lhs, const auto &rhs) {
            const int lhs_priority = calc_construct_turn_priority(lhs);
            const int rhs_priority = calc_construct_turn_priority(rhs);
            if (lhs_priority != rhs_priority) return lhs_priority > rhs_priority;
            if (lhs.query_turn_sum != rhs.query_turn_sum) return lhs.query_turn_sum > rhs.query_turn_sum;
            if (lhs.ab_query_turn_sum != rhs.ab_query_turn_sum) return lhs.ab_query_turn_sum > rhs.ab_query_turn_sum;
            if (lhs.destroyed_cost != rhs.destroyed_cost) return lhs.destroyed_cost > rhs.destroyed_cost;
            return lhs.v < rhs.v;
        });
        my_vector<std::pair<int, int>> sorted_vs;
        sorted_vs.reserve(infos.size());
        for (const VQueryTurnInfo &info: infos) {
            sorted_vs.push_back({info.v, info.destroyed_cost});
        }
        return sorted_vs;
    }

    // 責務: use_a_order_gap_priority に応じて AOrder gap 順または v の query 手数 priority 順の ordered_vs を返す。
    // 必要な理由: only-query と physical の両経路で同じ順序切替を使うため。
    my_vector<std::pair<int, int>> sort_destroyed_vs_for_construct(
            const YakinamashiState &yst,
            const my_vector<std::pair<int, int>> &destroyed_vs,
            int initial_n
    ) {
        if (G::is_a_order_gap_priority_enabled()) {
            return sort_destroyed_vs_by_a_gap_desc(yst, destroyed_vs);
        }
        return sort_destroyed_vs_by_query_turn_desc(yst, destroyed_vs, initial_n);
    }

    // 責務: destroyed_vs の各 v について、現在 query 列でその v を target にする query の ins_turn 合計を集計する。
    // 必要な理由: construct 順と二段目 rebuild 順の両方で、現在 query 上の v ごとの手数を比較できるようにするため。
    my_vector<VQueryTurnInfo> collect_v_query_turn_infos(
            const YakinamashiState &yst,
            const my_vector<std::pair<int, int>> &destroyed_vs,
            int initial_n
    ) {
        my_vector<VQueryTurnInfo> infos;
        infos.reserve(destroyed_vs.size());
        my_vector<int> info_idx_by_v(initial_n, -1);
        for (const auto &[v, destroyed_cost]: destroyed_vs) {
            my_assert(0 <= v && v < initial_n);
            info_idx_by_v[v] = static_cast<int>(infos.size());
            infos.push_back(VQueryTurnInfo{v, destroyed_cost, 0, 0});
        }
        for (const QueryCommonRI &qri: yst.queries_state.queries) {
            const int v = qri.get_tar_v();
            if (0 <= v && v < static_cast<int>(info_idx_by_v.size()) && info_idx_by_v[v] != -1) {
                infos[info_idx_by_v[v]].query_turn_sum += qri.get_ins_turn();
                if (qri.is_a2b_type() || qri.is_b2a_type()) {
                    infos[info_idx_by_v[v]].ab_query_turn_sum += qri.get_ins_turn();
                }
            }
        }
        return infos;
    }

    // 責務: rebuild_vs の v 集合を bitset 化する。
    // 必要な理由: only-query の query index 収集で target v 判定を O(1) にするため。
    my_bitset<MAX_N> build_rebuild_v_set(
            const my_vector<std::pair<int, int>> &rebuild_vs
    ) {
        my_bitset<MAX_N> rebuild_v_set;
        for (const auto &[v, destroyed_cost]: rebuild_vs) {
            (void) destroyed_cost;
            my_assert(0 <= v && v < static_cast<int>(MAX_N));
            rebuild_v_set.set(v);
        }
        return rebuild_v_set;
    }

    // 責務: rebuild 対象 v を target にする query index を現在 query 列から集める。
    // 必要な理由: QueriesModifier::erase_queries に渡す index 群を作るため。
    my_vector<int> collect_rebuild_query_idxs(
            const YakinamashiState &yst,
            const my_bitset<MAX_N> &rebuild_v_set
    ) {
        my_vector<int> query_idxs;
        for (int q_idx = 0; q_idx < static_cast<int>(yst.queries_state.queries.size()); q_idx++) {
            const int v = yst.queries_state.queries[q_idx].get_tar_v();
            if (rebuild_v_set[v]) query_idxs.push_back(q_idx);
        }
        return query_idxs;
    }

    void validate_destroyed_v(int v, int n) {
        my_assert(0 <= v && v < n);
    }

    my_vector<int> build_init_pos_by_value(const State &init_st) {
        my_vector<int> pos_by_value(init_st.initial_N, -1);
        for (int i = 0; i < init_st.A.size(); i++) {
            pos_by_value[init_st.A[i]] = i;
        }
        return pos_by_value;
    }

    my_bitset<MAX_N> build_current_a_values(const State &current_st) {
        my_bitset<MAX_N> values;
        for (int i = 0; i < current_st.A.size(); i++) {
            values.set(current_st.A[i]);
        }
        return values;
    }

    InitNeighborVs find_init_neighbors(
            const State &init_st,
            const my_vector<int> &init_pos_by_value,
            const my_bitset<MAX_N> &current_a_values,
            int v
    ) {
        InitNeighborVs result;
        const int n = init_st.A.size();
        const int start_pos = init_pos_by_value[v];
        my_assert(0 <= start_pos && start_pos < n);
        for (int step = 1; step < n; step++) {
            const int prev_pos = (start_pos - step + n) % n;
            const int next_pos = (start_pos + step) % n;
            if (result.prev_v == -1 && current_a_values[init_st.A[prev_pos]]) {
                result.prev_v = init_st.A[prev_pos];
            }
            if (result.next_v == -1 && current_a_values[init_st.A[next_pos]]) {
                result.next_v = init_st.A[next_pos];
            }
            if (result.prev_v != -1 && result.next_v != -1) break;
        }
        return result;
    }

    int find_entry_pos_by_value_state(
            const my_vector<OrderEntry> &entries,
            int v,
            ValState val_state
    ) {
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            if (entries[i].value == v && entries[i].val_state == val_state) return i;
        }
        my_assert(false);
        return 0;
    }

    int find_optional_entry_pos_by_value_state(
            const my_vector<OrderEntry> &entries,
            int v,
            ValState val_state
    ) {
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            if (entries[i].value == v && entries[i].val_state == val_state) return i;
        }
        return -1;
    }


    InsertRangeSet intersect_insert_ranges(
            const InsertRangeSet &lhs,
            const InsertRangeSet &rhs
    ) {
        InsertRangeSet result;
        for (const auto &[l_begin, l_end]: lhs.ranges) {
            for (const auto &[r_begin, r_end]: rhs.ranges) {
                const int begin = std::max(l_begin, r_begin);
                const int end = std::min(l_end, r_end);
                if (begin < end) result.ranges.push_back({begin, end});
            }
        }
        return result;
    }

    InsertRangeSet build_circular_insert_ranges(
            int entry_count,
            int prev_pos,
            int next_pos
    ) {
        InsertRangeSet result;
        if (prev_pos == -1 || next_pos == -1 || prev_pos == next_pos) {
            result.ranges.push_back({0, entry_count + 1});
            return result;
        }
        if (prev_pos < next_pos) {
            result.ranges.push_back({prev_pos + 1, next_pos + 1});
            return result;
        }
        if (prev_pos + 1 <= entry_count) {
            result.ranges.push_back({prev_pos + 1, entry_count + 1});
        }
        result.ranges.push_back({0, next_pos + 1});
        return result;
    }

    InsertRangeSet build_init_insert_ranges(
            const State &init_st,
            const State &current_st,
            const my_vector<OrderEntry> &entries,
            int v
    ) {
        const my_vector<int> init_pos_by_value = build_init_pos_by_value(init_st);
        const my_bitset<MAX_N> current_a_values = build_current_a_values(current_st);
        const InitNeighborVs neighbors = find_init_neighbors(init_st, init_pos_by_value, current_a_values, v);
        const int prev_pos = neighbors.prev_v == -1
                             ? -1
                             : find_entry_pos_by_value_state(entries, neighbors.prev_v, ValState::INIT);
        const int next_pos = neighbors.next_v == -1
                             ? -1
                             : find_entry_pos_by_value_state(entries, neighbors.next_v, ValState::INIT);
        return build_circular_insert_ranges(static_cast<int>(entries.size()), prev_pos, next_pos);
    }

    int circular_value_forward_dist(int from, int to, int n) {
        my_assert(0 <= from && from < n);
        my_assert(0 <= to && to < n);
        int dist = to - from;
        if (dist < 0) dist += n;
        return dist;
    }

    TargetNeighborVs find_target_neighbors(
            const my_vector<OrderEntry> &entries,
            int v,
            int n
    ) {
        TargetNeighborVs result;
        int best_prev_dist = INT_MAX;
        int best_next_dist = INT_MAX;
        for (const OrderEntry &entry: entries) {
            if (entry.val_state != ValState::TARGET) continue;
            if (entry.value == v) continue;
            const int prev_dist = circular_value_forward_dist(entry.value, v, n);
            my_assert(prev_dist != 0);
            if (prev_dist < best_prev_dist) {
                best_prev_dist = prev_dist;
                result.prev_v = entry.value;
            }
            const int next_dist = circular_value_forward_dist(v, entry.value, n);
            my_assert(next_dist != 0);
            if (next_dist < best_next_dist) {
                best_next_dist = next_dist;
                result.next_v = entry.value;
            }
        }
        return result;
    }

    InsertRangeSet build_target_insert_ranges(
            const my_vector<OrderEntry> &entries,
            int v,
            int n
    ) {
        const TargetNeighborVs neighbors = find_target_neighbors(entries, v, n);
        const int prev_pos = neighbors.prev_v == -1
                             ? -1
                             : find_entry_pos_by_value_state(entries, neighbors.prev_v, ValState::TARGET);
        const int next_pos = neighbors.next_v == -1
                             ? -1
                             : find_entry_pos_by_value_state(entries, neighbors.next_v, ValState::TARGET);
        InsertRangeSet ranges = build_circular_insert_ranges(static_cast<int>(entries.size()), prev_pos, next_pos);
        return ranges;
    }

    int count_insert_positions(const InsertRangeSet &range_set) {
        int count = 0;
        for (const auto &[begin, end]: range_set.ranges) {
            my_assert(begin <= end);
            count += end - begin;
        }
        return count;
    }

    int pick_random_insert_pos(const InsertRangeSet &range_set) {
        int idx = my_rand(count_insert_positions(range_set));
        for (const auto &[begin, end]: range_set.ranges) {
            const int size = end - begin;
            if (idx < size) return begin + idx;
            idx -= size;
        }
        my_assert(false);
        return 0;
    }

    // 責務: InsertRangeSet を円環順に連結した候補列として扱い、その中央 slot を返す。
    // 必要な理由: 段階復元実験で同じ入力から同じ AOrder 復元位置を選べるようにするため。
    int pick_middle_insert_pos(const InsertRangeSet &range_set) {
        int idx = count_insert_positions(range_set) / 2;
        for (const auto &[begin, end]: range_set.ranges) {
            const int size = end - begin;
            if (idx < size) return begin + idx;
            idx -= size;
        }
        my_assert(false);
        return 0;
    }

    QueryScanState init_scan_state(const YakinamashiState &yst, const State &init_st) {
        QueryScanState scan_state{
                State(init_st),
                my_bitset<MAX_ALL_ORD_A>(),
                my_bitset<MAX_ALL_ORD_B>(),
                yst.query_order.aorder.initial_value_type(init_st),
                my_vector<ValState>(init_st.initial_N, ValState::NONE)
        };
        ExistAllOrdManager::init_exist_a_ords_from_state<MAX_N>(
                init_st, yst.query_order.aorder, scan_state.now_a_val_state, scan_state.exist_a_ords
        );
        for (int i = 0; i < init_st.B.size(); i++) {
            int v = init_st.B[i];
            scan_state.now_b_val_state[v] = ValState::TARGET;
            ExistAllOrdManager::set_exist(
                    scan_state.exist_b_ords,
                    yst.query_order.border.get_all_order(v, scan_state.now_b_val_state[v])
            );
        }
        return scan_state;
    }

    void restore_order_entry(const State &init_st, PhysicalDestroyContext<MAX_N> &context, int v) {
        YakinamashiState& yst = context.yst;
        const EraseOrderSnapshot<MAX_N>& snapshot = context.erase_order_snapshot;
        if (!snapshot.is_erased_v[v]) {
            log_restore_order_entry(context, v, false, -1, -1);
            return;
        }
        const auto& before_entries = snapshot.before_erased_order.aorder.get_order_entries();
        const auto& after_entries = snapshot.after_erased_order.aorder.get_order_entries();
        const auto& current_entries = yst.query_order.aorder.get_order_entries();
        const OrderEntry init_entry(v, ValState::INIT);
        (void)before_entries;
        (void)after_entries;
        const InsertRangeSet init_ranges = build_init_insert_ranges(init_st, context.current_st, current_entries, v);
        const int init_pos = pick_random_insert_pos(init_ranges);
        yst.query_order.aorder.insert_entry(init_pos, init_entry);

        const auto& current_entries_after_init = yst.query_order.aorder.get_order_entries();
        const OrderEntry target_entry(v, ValState::TARGET);
        const InsertRangeSet target_ranges =
                build_target_insert_ranges(current_entries_after_init, v, init_st.initial_N);
        const int target_pos = pick_random_insert_pos(target_ranges);
        yst.query_order.aorder.insert_entry(target_pos, target_entry);
        yst.query_order.aorder.normalize_min_front();
        yst.query_order.aorder.validate_target_entry_order();
        log_restore_order_entry(context, v, true, init_pos, target_pos);
    }

    // 責務: erased v の INIT/TARGET entry を既存の制約 range 内の中央 slot へ復元する。
    // 必要な理由: random restore を避け、初期 A idx と target 値順の前後制約を保った deterministic 復元を行うため。
    void restore_order_entry_middle(const State &init_st, PhysicalDestroyContext<MAX_N> &context, int v) {
        YakinamashiState& yst = context.yst;
        const EraseOrderSnapshot<MAX_N>& snapshot = context.erase_order_snapshot;
        if (!snapshot.is_erased_v[v]) {
            log_restore_order_entry(context, v, false, -1, -1);
            return;
        }
        const auto& current_entries = yst.query_order.aorder.get_order_entries();
        const OrderEntry init_entry(v, ValState::INIT);
        const InsertRangeSet init_ranges = build_init_insert_ranges(init_st, context.current_st, current_entries, v);
        const int init_pos = pick_middle_insert_pos(init_ranges);
        yst.query_order.aorder.insert_entry(init_pos, init_entry);

        const auto& current_entries_after_init = yst.query_order.aorder.get_order_entries();
        const OrderEntry target_entry(v, ValState::TARGET);
        const InsertRangeSet target_ranges =
                build_target_insert_ranges(current_entries_after_init, v, init_st.initial_N);
        const int target_pos = pick_middle_insert_pos(target_ranges);
        yst.query_order.aorder.insert_entry(target_pos, target_entry);
        yst.query_order.aorder.normalize_min_front();
        yst.query_order.aorder.validate_target_entry_order();
        log_restore_order_entry(context, v, true, init_pos, target_pos);
    }

    RingBuffer500 build_add_v_stack(
            const State &init_st,
            const State &current_st,
            int v
    ) {
        my_bitset<MAX_N> should_keep_a;
        for (int i = 0; i < current_st.A.size(); i++) should_keep_a.set(current_st.A[i]);
        should_keep_a.set(v);
        RingBuffer500 add_v_stack;
        for (int i = 0; i < init_st.A.size(); i++) {
            int init_v = init_st.A[i];
            if (should_keep_a[init_v]) add_v_stack.push_back(init_v);
        }
        log_add_v_stack(init_st, current_st, v, add_v_stack);
        my_assert(add_v_stack.size() == current_st.A.size() + 1);
        return add_v_stack;
    }

    void restore_state_value(
            const State &init_st,
            State &current_st,
            int v
    ) {
        State before_st = current_st;
#ifdef DEBUG
        for (int i = 0; i < current_st.A.size(); i++) my_assert(current_st.A[i] != v);
        for (int i = 0; i < current_st.B.size(); i++) my_assert(current_st.B[i] != v);
#endif
        current_st.A = build_add_v_stack(init_st, current_st, v);
        current_st.current_N++;
        my_assert(current_st.current_N == current_st.A.size() + current_st.B.size());
        log_restore_state_value(before_st, current_st, v);
    }

    void reset_first_top_ords(
            const State &current_st,
            YakinamashiState &yst,
            const QueryScanState &scan_state
    ) {
        if (current_st.A.empty()) {
            yst.queries_state.first_top_ord_a = OrdPos(0);
        } else {
            const int v = current_st.A[0];
            const AllOrd all_ord = yst.query_order.aorder.get_all_order(v, scan_state.now_a_val_state[v]);
            yst.queries_state.first_top_ord_a =
                    to_ord_pos(OrdCalc::get_rel_ord(scan_state.exist_a_ords, all_ord), current_st.A.size());
        }
        if (current_st.B.empty()) {
            yst.queries_state.first_top_ord_b = OrdPos(0);
        } else {
            const int v = current_st.B[0];
            const AllOrd all_ord = yst.query_order.border.get_all_order(v, scan_state.now_b_val_state[v]);
            yst.queries_state.first_top_ord_b =
                    to_ord_pos(OrdCalc::get_rel_ord(scan_state.exist_b_ords, all_ord), current_st.B.size());
        }
    }

    QueryCommon rebuild_query_for_state(
            const QueryCommon &old_q,
            YakinamashiState &yst,
            QueryScanState &scan_state,
            int q_idx,
            int current_N
    ) {
        const int v = old_q.get_tar_v();
        if (old_q.is_a2b_type()) return build_eval_a2b_query(yst, scan_state, v);
        if (old_q.is_b2a_type()) return build_eval_b2a_query(yst, scan_state, v);
        my_assert(old_q.is_a2a_type());
        return build_eval_a2a_query(yst, scan_state, v, q_idx, e_a2a_type::swap_rot, current_N);
    }

    void rebuild_queries_for_state(
            const State &current_st,
            YakinamashiState &yst
    ) {
        log_rebuild_queries_for_state_begin(current_st, yst);
        QRIList old_queries = yst.queries_state.queries;
        yst.queries_state.current_N = current_st.current_N;
        yst.queries_state.queries.clear();
        yst.queries_state.sum_turn = 0;
        QueryScanState scan_state = init_scan_state(yst, current_st);
        reset_first_top_ords(current_st, yst, scan_state);
        QCRIFactory cri_factory;
        for (int q_idx = 0; q_idx < static_cast<int>(old_queries.size()); q_idx++) {
            const QueryCommon new_q =
                    rebuild_query_for_state(old_queries[q_idx].get_query(), yst, scan_state, q_idx, current_st.current_N);
            const QueryCommonRI new_ri = cri_factory.build_ri(yst.queries_state, new_q, q_idx);
            log_rebuild_queries_for_state_step(q_idx, old_queries[q_idx], new_q, new_ri);
            yst.queries_state.sum_turn += new_ri.get_ins_turn();
            yst.queries_state.queries.push_back(new_ri);
            advance_scan_state(yst, scan_state, QueriesCalculator(current_st.current_N, yst.queries_state.queries), q_idx, new_ri);
            // 責務: ここまで再構築した prefix query が current_st から順に実行可能か検証する。
            QueriesStateValidator<MAX_N>::validate_query_order(current_st, yst.queries_state.queries);
            QueriesStateValidator<MAX_N>::validate_query_top_transfer(current_st, yst.queries_state.queries);
        }
        yst.queries_state.final_rot_info =
                FinalRotCalculator::build_final_rot_info(yst.queries_state.current_N, yst.queries_state);
        yst.queries_state.sum_turn += yst.queries_state.final_rot_info.second;
        QueriesStateValidator<MAX_N>::validate_sum_turn(yst.queries_state);
        QueriesStateValidator<MAX_N>::validate_query_order(current_st, yst.queries_state.queries);
        QueriesStateValidator<MAX_N>::validate_query_top_transfer(current_st, yst.queries_state.queries);
        log_rebuild_queries_for_state_end(current_st, yst);
    }

    int decide_insert_pos(
            const my_vector<OrderEntry>& before_entries,
            const my_vector<OrderEntry>& after_entries,
            const my_vector<OrderEntry>& current_entries,
            const my_bitset<MAX_N>& is_erased_v,
            const OrderEntry& entry,
            int lower_pos,
            int upper_pos
    ) {
        my_assert(0 <= lower_pos && lower_pos <= upper_pos);
        my_assert(upper_pos <= static_cast<int>(current_entries.size()));
        const int ideal_pos = find_before_prev_pos(before_entries, after_entries, current_entries, is_erased_v, entry);
        if (ideal_pos < lower_pos) return lower_pos;
        if (upper_pos < ideal_pos) return upper_pos;
        return ideal_pos;
    }

    int find_before_prev_pos(
            const my_vector<OrderEntry>& before_entries,
            const my_vector<OrderEntry>& after_entries,
            const my_vector<OrderEntry>& current_entries,
            const my_bitset<MAX_N>& is_erased_v,
            const OrderEntry& entry
    ) {
        const int before_pos = find_entry_pos(before_entries, entry);
        for (int i = before_pos - 1; i >= 0; i--) {
            const OrderEntry& prev_entry = before_entries[i];
            if (is_erased_v[prev_entry.value]) continue;
            (void)find_entry_pos(after_entries, prev_entry);
            return find_entry_pos(current_entries, prev_entry) + 1;
        }
        return 0;
    }

    int find_entry_pos(const my_vector<OrderEntry>& entries, const OrderEntry& entry) {
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            if (entries[i] == entry) return i;
        }
        my_assert(false);
        return 0;
    }

    std::pair<int, int> find_target_range(const my_vector<OrderEntry>& entries, int v) {
        int lower_pos = 0;
        int upper_pos = static_cast<int>(entries.size());
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            const OrderEntry& entry = entries[i];
            if (entry.val_state != ValState::TARGET) continue;
            if (entry.value < v) lower_pos = i + 1;
            if (v < entry.value) {
                upper_pos = i;
                break;
            }
        }
        my_assert(lower_pos <= upper_pos);
        return {lower_pos, upper_pos};
    }

    std::pair<int, int> full_insert_range(const my_vector<OrderEntry>& entries) {
        return {0, static_cast<int>(entries.size())};
    }

    OrdPos get_query_start_ord_a(const QueriesState &queries_state, int q_idx, int n) {
        my_assert(0 <= q_idx && q_idx < queries_state.queries.size());
        if (q_idx == 0) return queries_state.first_top_ord_a;
        return queries_state.queries[q_idx - 1].get_finished_top_ord_a();
    }

    OrdPos get_query_start_ord_b(const QueriesState &queries_state, int q_idx, int n) {
        my_assert(0 <= q_idx && q_idx - 1 < (int)queries_state.queries.size());
        if (q_idx == 0) return queries_state.first_top_ord_b;
        return queries_state.queries[q_idx - 1].get_query().get_finished_top_ord_b(n);
    }

    OrdPos get_query_tar_ord_a(const QueryCommonRI &qri) {
        const QueryCommon &q = qri.get_query();
        if (qri.is_a2b_type()) return q.get_tar_ord_pos_a_A2B();
        my_assert(qri.is_b2a_type());
        return to_ord_pos(q.get_tar_rel_ord_a_B2A(), q.get_stack_a_cnt());
    }

    OrdPos get_query_tar_ord_b(const QueryCommonRI &qri, int n) {
        const QueryCommon &q = qri.get_query();
        if (qri.is_a2b_type()) return to_ord_pos(q.get_tar_rel_ord_b_A2B(), q.get_stack_b_cnt(n));
        my_assert(qri.is_b2a_type());
        return q.get_tar_ord_pos_b_B2A();
    }

//以降にa2bが無い場合は-1
//    my_vector<int> build_next_ab_query_idxs(const QueriesState &queries_state) {
//        int query_count = static_cast<int>(queries_state.queries.size());
//        my_vector<int> next_ab_query_idxs(query_count, -1);
//        int next_ab_idx = -1;
//        for (int q_idx = query_count - 1; q_idx >= 0; q_idx--) {
//            if (queries_state.queries[q_idx].is_ab_type()) next_ab_idx = q_idx;
//            next_ab_query_idxs[q_idx] = next_ab_idx;
//        }
//        return next_ab_query_idxs;
//    }

    OrdPos get_ord_pos_b_by_pos(const QueryScanState &scan_state, const YakinamashiState &yst, int idx) {
        if (scan_state.st.B.size() == 0) return OrdPos(0);
        my_assert(0 <= idx && idx < scan_state.st.B.size());
        int v = scan_state.st.B[idx];
        AllOrd all_ord = yst.query_order.border.get_all_order(v, scan_state.now_b_val_state[v]);
        return to_ord_pos(OrdCalc::get_rel_ord(scan_state.exist_b_ords, all_ord), scan_state.st.B.size());
    }

    AllOrd get_all_ord_a_by_pos(const QueryScanState &scan_state, const YakinamashiState &yst, int idx) {
        my_assert(0 <= idx && idx < scan_state.st.A.size());
        int v = scan_state.st.A[idx];
        return yst.query_order.aorder.get_all_order(v, scan_state.now_a_val_state[v]);
    }

    AllOrd get_all_ord_b_by_pos(const QueryScanState &scan_state, const YakinamashiState &yst, int idx) {
        my_assert(0 <= idx && idx < scan_state.st.B.size());
        int v = scan_state.st.B[idx];
        if (scan_state.now_b_val_state[v] != ValState::TARGET) {
            out("scan_b_val_state_mismatch",
                "idx", idx,
                "v", v,
                "b_val_state", scan_state.now_b_val_state[v],
                "state_A", to_stack_string(scan_state.st.A),
                "state_B", to_stack_string(scan_state.st.B),
                "state_B_val_state", to_stack_value_state_string(scan_state.st.B, scan_state.now_b_val_state),
                "BOrderEntries", to_order_entries_string(yst.query_order.border.get_all_order_entries()));
        }
        return yst.query_order.border.get_all_order(v, scan_state.now_b_val_state[v]);
    }

    std::string to_stack_all_ord_string_a(const QueryScanState &scan_state, const YakinamashiState &yst) {
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < static_cast<int>(scan_state.st.A.size()); i++) {
            if (i != 0) ss << ", ";
            ss << get_all_ord_a_by_pos(scan_state, yst, i).get();
        }
        ss << "]";
        return ss.str();
    }

    std::string to_stack_all_ord_string_b(const QueryScanState &scan_state, const YakinamashiState &yst) {
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < static_cast<int>(scan_state.st.B.size()); i++) {
            if (i != 0) ss << ", ";
            ss << get_all_ord_b_by_pos(scan_state, yst, i).get();
        }
        ss << "]";
        return ss.str();
    }

    void
    append_trace_state_lines(const YakinamashiState &yst, const QueryScanState &scan_state, const std::string &indent) {
        append_trace_line(indent + "State.A=" + to_stack_string(scan_state.st.A));
        append_trace_line(indent + "State.B=" + to_stack_string(scan_state.st.B));
        append_trace_line(indent + "State.A(value,val_state)=" +
                          to_stack_value_state_string(scan_state.st.A, scan_state.now_a_val_state));
        append_trace_line(indent + "State.B(value,val_state)=" +
                          to_stack_value_state_string(scan_state.st.B, scan_state.now_b_val_state));
        append_trace_line(indent + "State.A(all_ord)=" + to_stack_all_ord_string_a(scan_state, yst));
        append_trace_line(indent + "State.B(all_ord)=" + to_stack_all_ord_string_b(scan_state, yst));
    }

    void append_trace_query_lines(const YakinamashiState &yst) {
        append_trace_line("Queries:");
        for (int q_idx = 0; q_idx < static_cast<int>(yst.queries_state.queries.size()); q_idx++) {
            append_trace_line("  q" + std::to_string(q_idx) + ": " + to_query_string(yst.queries_state.queries[q_idx]));
        }
    }

    void append_trace_order_lines(const YakinamashiState &yst) {
        append_trace_line("AOrderEntries=" + to_order_entries_string(yst.query_order.aorder.get_all_order_entries()));
        append_trace_line("BOrderEntries=" + to_order_entries_string(yst.query_order.border.get_all_order_entries()));
    }

    void append_trace_v_header(
            const YakinamashiState &yst, const QueryScanState &scan_state, int v, int destroyed_cost
    ) {
        append_trace_line("");
        append_trace_line(
                "==== v=" + std::to_string(v) + " destroyed_cost=" + std::to_string(destroyed_cost) + " ====");
        append_trace_query_lines(yst);
        append_trace_state_lines(yst, scan_state, "  ");
        append_trace_order_lines(yst);
    }

    int calc_half_open_all_ord_range_size(int start, int end_exclusive, int all_ord_size) {
        int size = end_exclusive - start;
        if (size <= 0) size += all_ord_size;
        my_assert(size <= all_ord_size);
        return size;
    }

    void update_scan_order(const YakinamashiState &yst, QueryScanState &scan_state, const QueryCommon &q) {
        int tar_v = q.get_tar_v();
        if (q.is_a2b_type()) {
            ExistAllOrdManager::upd_ord_a2b<MAX_N>(
                    tar_v, scan_state.exist_a_ords, scan_state.exist_b_ords,
                    yst.query_order.aorder, yst.query_order.border,
                    scan_state.now_a_val_state, scan_state.now_b_val_state
            );
            return;
        }
        if (q.is_b2a_type()) {
            ExistAllOrdManager::upd_ord_b2a<MAX_N>(
                    tar_v, scan_state.exist_a_ords, scan_state.exist_b_ords,
                    yst.query_order.aorder, yst.query_order.border,
                    scan_state.now_a_val_state, scan_state.now_b_val_state
            );
            return;
        }
        ExistAllOrdManager::upd_ord_a2a<MAX_N>(
                tar_v, scan_state.exist_a_ords, yst.query_order.aorder,
                scan_state.now_a_val_state
        );
    }

    // 責務: 各 query 境界について、その直前に連続して存在する A2A query 数を返す。
    // 必要な理由: InsertPairPrecalc から参照する qcnt+1 配列を vec_array 化して heap allocation を避けるため。
    //自分を含まない
    QueryBoundaryArray build_prev_a2a_ranges(const YakinamashiState &yst) {
        auto &queries = yst.queries_state.queries;
        int qcnt = queries.size();
        static QueryBoundaryArray prev_a2a_ranges;
        prev_a2a_ranges.clear();
        prev_a2a_ranges.push_back(0);
        for (int q_idx = 0; q_idx < qcnt; q_idx++) {
            const int next_range = queries[q_idx].is_a2a_type() ? prev_a2a_ranges[q_idx] + 1 : 0;
            prev_a2a_ranges.push_back(next_range);
        }
        return prev_a2a_ranges;
    }

    // 責務: 各 query 境界から見た最初の AB query index を qcnt sentinel 付きで返す。
    // 必要な理由: InsertPairPrecalc から参照する qcnt+1 配列を vec_array 化して heap allocation を避けるため。
    QueryBoundaryArray build_next_ab_idxs(const YakinamashiState &yst) {
        auto &queries = yst.queries_state.queries;
        int qcnt = queries.size();
        static QueryBoundaryArray next_ab_idxs;
        next_ab_idxs.clear();
        for (int qi = 0; qi <= qcnt; qi++) {
            next_ab_idxs.push_back(qcnt);
        }
        int next_ab_idx = qcnt;
        for (int qi = qcnt - 1; qi >= 0; qi--) {
            if (queries[qi].get_query().is_ab_type()) {
                next_ab_idx = qi;
            }
            next_ab_idxs[qi] = next_ab_idx;
        }
        return next_ab_idxs;
    }

    void advance_scan_state(
            const YakinamashiState &yst,
            QueryScanState &scan_state,
            const QueriesCalculator &qs_calc,
            int q_idx,
            const QueryCommonRI &qri
    ) {
        my_assert(0 <= q_idx && q_idx < static_cast<int>(yst.queries_state.queries.size()));
        update_scan_order(yst, scan_state, qri.get_query());
        QueryStateApplyUtil::apply_query_to_state(scan_state.st, qs_calc, q_idx, qri);
    }

    YakinamashiState build_eval_state(
            const YakinamashiState &yst,
            int v,
            AllOrd new_pushed_b_all_ord
    ) {
        YakinamashiState eval_yst = yst;
        eval_yst.query_order.border.update_insert_ord_v(v, new_pushed_b_all_ord);
        out("measure_border_after_update",
            "v", v,
            "new_pushed_b_all_ord", new_pushed_b_all_ord.get(),
            "BOrderEntries", to_order_entries_string(eval_yst.query_order.border.get_all_order_entries()));
        return eval_yst;
    }


    //しかたないやつはこれをよぶ, いこうのため
    QueryScanState allow_build_scan_state_before_q_idx(const YakinamashiState &yst, const State &init_st, int q_idx) {
        //これは酷く重い関数で使うべきではない
        //現状使ってるか所があるが、それらは使われない関数のため放置する
        my_assert(0 <= q_idx && q_idx <= static_cast<int>(yst.queries_state.queries.size()));
        QueryScanState scan_state = init_scan_state(yst, init_st);
        QueriesCalculator qs_calc(init_st.current_N, yst.queries_state.queries);
        for (int i = 0; i < q_idx; i++) {
            const QueryCommonRI &qri = yst.queries_state.queries[i];
            advance_scan_state(yst, scan_state, qs_calc, i, qri);
        }
        return scan_state;
    }

    QueryScanState build_scan_state_before_q_idx(const YakinamashiState &yst, const State &init_st, int q_idx) {
        //これは酷く重い関数で使うべきではない
        //現状使ってるか所があるが、それらは使われない関数のため放置する
        my_assert(false);
        assert(false);
        my_assert(0 <= q_idx && q_idx <= static_cast<int>(yst.queries_state.queries.size()));
        QueryScanState scan_state = init_scan_state(yst, init_st);
        QueriesCalculator qs_calc(init_st.current_N, yst.queries_state.queries);
        for (int i = 0; i < q_idx; i++) {
            const QueryCommonRI &qri = yst.queries_state.queries[i];
            advance_scan_state(yst, scan_state, qs_calc, i, qri);
        }
        return scan_state;
    }

    // 責務: 各 query 直前の A/B all_ord 存在集合を q_idx 境界ごとに作る。
    // 必要な理由: 大きい vec_array の返り値代入を避け、InsertPairPrecalc のメンバへ直接構築するため。
    void build_exist_all_ords_before_q_idx(
            const YakinamashiState &yst,
            const State &init_st,
            QueryExistAArray &exist_a_ords_list,
            QueryExistBArray &exist_b_ords_list
    ) {
        const int qcnt = static_cast<int>(yst.queries_state.queries.size());
        exist_a_ords_list.clear();
        exist_b_ords_list.clear();
        QueryScanState scan_state = init_scan_state(yst, init_st);
        QueriesCalculator qs_calc(init_st.current_N, yst.queries_state.queries);
        exist_a_ords_list.push_back(scan_state.exist_a_ords);
        exist_b_ords_list.push_back(scan_state.exist_b_ords);
        for (int q_idx = 0; q_idx < qcnt; q_idx++) {
            const QueryCommonRI &qri = yst.queries_state.queries[q_idx];
            advance_scan_state(yst, scan_state, qs_calc, q_idx, qri);
            exist_a_ords_list.push_back(scan_state.exist_a_ords);
            exist_b_ords_list.push_back(scan_state.exist_b_ords);
        }
    }


    QueryCommon build_eval_a2b_query(YakinamashiState &eval_yst, QueryScanState &scan_state, int v) {
        OrdPos ord_pos_a = OrdCalc::get_tar_ord_posa_of_a2b<MAX_N>(
                scan_state.exist_a_ords, eval_yst.query_order.aorder, v, scan_state.now_a_val_state[v],
                scan_state.st.A.size()
        );
        RelOrd rel_ord_b = OrdCalc::get_tar_rel_ordb_of_a2b<MAX_N>(
                scan_state.exist_b_ords, eval_yst.query_order.border, v, scan_state.st.B.size()
        );
        return QueryCommon(CommonPushType::A2B, v, scan_state.st.A.size(), ord_pos_a.get(), rel_ord_b.get());
    }

    QueryCommon build_eval_b2a_query(YakinamashiState &eval_yst, QueryScanState &scan_state, int v) {
        RelOrd rel_ord_a = OrdCalc::get_tar_rel_orda_of_b2a<MAX_N>(
                scan_state.exist_a_ords, eval_yst.query_order.aorder, v, scan_state.st.A.size()
        );
        OrdPos ord_pos_b = OrdCalc::get_tar_ord_posb_of_b2a<MAX_N>(
                scan_state.exist_b_ords, eval_yst.query_order.border, v, scan_state.now_b_val_state[v],
                scan_state.st.B.size()
        );
        // _BEGIN: b2a も同様に QueryCommon 生成だけで足りるため、scan state を factory の init 前提へ流さない。
        return QueryCommon(CommonPushType::B2A, v, scan_state.st.A.size(), rel_ord_a.get(), ord_pos_b.get());
        // _END
    }

    void
    insert_query_common(YakinamashiState &eval_yst, const State &init_st, int q_idx, const QueryCommon &query_common) {
        QueryProvider provider(eval_yst, init_st);
        QueriesModifier modifier(init_st, eval_yst.query_order.aorder, eval_yst.queries_state,
                                 provider);
        modifier.insert_queries<MAX_N>(my_vector<int>{q_idx}, my_vector<QueryCommon>{query_common});
    }

    QueryCommon build_lis_noop_a2a_query(const YakinamashiState &yst, const State &init_st, int v, int q_idx) {
        QueryScanState scan_state = build_scan_state_before_q_idx(yst, init_st, q_idx);
        my_assert(scan_state.now_a_val_state[v] == ValState::INIT);
        const AllOrd init_all_ord = yst.query_order.aorder.get_all_order(v, ValState::INIT);
        const AllOrd target_all_ord = yst.query_order.aorder.get_all_order(v, ValState::TARGET);
        OrdPos tar_orda1 = to_ord_pos(
                OrdCalc::get_rel_ord(scan_state.exist_a_ords, init_all_ord),
                scan_state.st.A.size()
        );
        RelOrd tar_rel_orda2 = OrdCalc::get_rel_ord(scan_state.exist_a_ords, target_all_ord);
        OrdPos top_ord_b = q_idx == 0 ? yst.queries_state.first_top_ord_b
                                      : yst.queries_state.queries[q_idx - 1].get_query().get_finished_top_ord_b(
                        init_st.current_N);
        QueryCommon res = QueryCommon(CommonPushType::A2AS, v, scan_state.st.A.size(),
                                      tar_orda1.get(), tar_rel_orda2.get(), top_ord_b.get(), 0);
        my_assert(res.get_flip_dis_dir() == 0);
        return res;
    }

//    YakinamashiState build_debug_state_with_random_lis_noops(const YakinamashiState &yst, const State &init_st) {
//        YakinamashiState eval_yst = yst;
//        for (int v = 0; v < init_st.current_N; v++) {
//            if (!eval_yst.is_lis[v]) continue;
//            const int q_idx = my_rand(static_cast<int>(eval_yst.queries_state.queries.size()) + 1);
//            QueryCommon noop_query = build_lis_noop_a2a_query(eval_yst, init_st, v, q_idx);
//            insert_query_common(eval_yst, init_st, q_idx, noop_query);
//        }
//        return eval_yst;
//    }

    QueryCommon build_test_front_a2b_query(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            int pushed_b_all_ord
    ) {
        YakinamashiState eval_yst = build_eval_state(yst, v, AllOrd(pushed_b_all_ord));
        QueryScanState scan_state = build_scan_state_before_q_idx(eval_yst, init_st, 0);
        return build_eval_a2b_query(eval_yst, scan_state, v);
    }

    IndirectMeasureResult measure_q_idx_diff_after_front_a2b_insert(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            int q_idx,
            int pushed_b_all_ord
    ) {
        YakinamashiState eval_yst = build_eval_state(yst, v, AllOrd(pushed_b_all_ord));
        IndirectMeasureResult result = {
                0,
                0,
                0,
                eval_yst
        };
        result.before = yst.queries_state.queries[q_idx].get_ins_turn();
        QueryCommon front_a2b_query = build_test_front_a2b_query(yst, init_st, v, pushed_b_all_ord);
        insert_query_common(eval_yst, init_st, 0, front_a2b_query);
        result.after = eval_yst.queries_state.queries[q_idx + 1].get_ins_turn();
        result.diff = result.after - result.before;
        result.yst = eval_yst;
        return result;
    }

    IndirectMeasureResult measure_q_idx_diff_after_front_a2b_b2a_insert(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            int q_idx
    ) {
        YakinamashiState eval_yst = build_eval_state(yst, v, AllOrd(0));
        IndirectMeasureResult result = {0, 0, 0, eval_yst};
        result.before = yst.queries_state.queries[q_idx].get_ins_turn();
        QueryScanState a2b_scan_state = build_scan_state_before_q_idx(eval_yst, init_st, 0);
        QueryCommon front_a2b_query = build_eval_a2b_query(eval_yst, a2b_scan_state, v);
        insert_query_common(eval_yst, init_st, 0, front_a2b_query);
        QueryScanState b2a_scan_state = build_scan_state_before_q_idx(eval_yst, init_st, 1);
        QueryCommon front_b2a_query = build_eval_b2a_query(eval_yst, b2a_scan_state, v);
        insert_query_common(eval_yst, init_st, 1, front_b2a_query);
        result.after = eval_yst.queries_state.queries[q_idx + 2].get_ins_turn();
        result.diff = result.after - result.before;
        result.yst = eval_yst;
        return result;
    }

    int calc_q_idx_indirect_diff(
            int fixed_indirect_diff,
            const BandRangeInfo &info,
            int pushed_b_all_ord
    ) {
        return fixed_indirect_diff + (is_b_all_ord_inside_band(info, pushed_b_all_ord) ? 1 : 0);
    }

    void dump_a2b_b2a_indirect_mismatch(
            int v,
            int q_idx,
            int pushed_b_all_ord,
            int calc_diff,
            const IndirectMeasureResult &measured
    ) {
        out("=== INDIRECT_MISMATCH_AFTER_FRONT_A2B_B2A ===");
        out("mismatch_v", v, "q_idx", q_idx, "pushed_b_all_ord", pushed_b_all_ord,
            "before", measured.before, "after", measured.after,
            "measured_diff", measured.diff, "calc_diff", calc_diff);
        out("after_front_a2b_b2a_sum_turn", measured.yst.queries_state.sum_turn);
        for (int measured_q_idx = 0;
             measured_q_idx < static_cast<int>(measured.yst.queries_state.queries.size());
             measured_q_idx++) {
            out("after_front_a2b_b2a_q", measured_q_idx,
                indirect_debug_to_string(measured.yst.queries_state.queries[measured_q_idx]));
        }
    }

    template<typename QueryIntContainer, typename BandRangeContainer>
    void run_a2b_b2a_indirect_q_idx_checks(
            const QueryIntContainer &is_only_direct,
            int v,
            const YakinamashiState &yst,
            const State &init_st,
            const QueryIntContainer &fixed_indirect_a2b_b2a_range_diff,
            const BandRangeContainer &q_idx_band_range_info
    ) {
        const int qcnt = static_cast<int>(yst.queries_state.queries.size());
        const YakinamashiState pushed_yst = build_eval_state(yst, v, AllOrd(0));
        const int pushed_b_all_ord = pushed_yst.query_order.border.get_target_order(v).get();
        out("a2b_b2a_is_only_direct");
        out(is_only_direct);
        for (int q_idx = 0; q_idx < qcnt; q_idx++) {
            if (is_only_direct[q_idx]) continue;
            const BandRangeInfo &info = q_idx_band_range_info[q_idx];
            const IndirectMeasureResult measured = measure_q_idx_diff_after_front_a2b_b2a_insert(
                    yst, init_st, v, q_idx);
            const int calc_diff = fixed_indirect_a2b_b2a_range_diff[q_idx];
            out("a2b_b2a_q_idx_diff", "v", v, "q_idx", q_idx, "pushed_b_all_ord", pushed_b_all_ord,
                "before", measured.before, "after", measured.after, "measured_diff", measured.diff,
                "fixed_diff", fixed_indirect_a2b_b2a_range_diff[q_idx],
                "band_hit", is_b_all_ord_inside_band(info, pushed_b_all_ord) ? 1 : 0,
                "calc_diff", calc_diff, "target_qri",
                indirect_debug_to_string(yst.queries_state.queries[q_idx]));
            if (measured.diff != calc_diff)
                dump_a2b_b2a_indirect_mismatch(v, q_idx, pushed_b_all_ord, calc_diff, measured);
            my_assert(measured.diff == calc_diff);
        }
    }

    template<typename QueryIntContainer, typename BandRangeContainer>
    void run_indirect_q_idx_checks(
            const QueryIntContainer &is_only_direct,
            int v,
            const YakinamashiState &yst,
            const State &init_st,
            const QueryIntContainer &fixed_indirect_range_diff,
            const BandRangeContainer &q_idx_band_range_info
    ) {
        const int qcnt = static_cast<int>(yst.queries_state.queries.size());
        const int insert_slot_size = yst.query_order.border.get_all_order_entries_size() + 1;
        my_assert(static_cast<int>(fixed_indirect_range_diff.size()) == qcnt);
        my_assert(static_cast<int>(q_idx_band_range_info.size()) == qcnt);
        out("is_only_direct");
        out(is_only_direct);
        out("=== q_idx差分一覧 ===");
        for (int q_idx = 0; q_idx < qcnt; q_idx++) {
            if (is_only_direct[q_idx]) continue;
            const BandRangeInfo &info = q_idx_band_range_info[q_idx];
            const int probe_count = info.has_band ? 2 : 1;
            for (int probe_idx = 0; probe_idx < probe_count; probe_idx++) {
                const bool want_inside = (probe_idx == 0 && info.has_band);
                const int pushed_b_all_ord = pick_probe_b_all_ord(info, insert_slot_size, want_inside);
                const IndirectMeasureResult measured = measure_q_idx_diff_after_front_a2b_insert(
                        yst, init_st, v, q_idx, pushed_b_all_ord);
                const int calc_diff = calc_q_idx_indirect_diff(
                        fixed_indirect_range_diff[q_idx], info, pushed_b_all_ord);
                out("q_idx_diff",
                    "v", v,
                    "q_idx", q_idx,
                    "probe_idx", probe_idx,
                    "pushed_b_all_ord", pushed_b_all_ord,
                    "before", measured.before,
                    "after", measured.after,
                    "measured_diff", measured.diff,
                    "fixed_diff", fixed_indirect_range_diff[q_idx],
                    "band_hit", is_b_all_ord_inside_band(info, pushed_b_all_ord) ? 1 : 0,
                    "calc_diff", calc_diff,
                    "target_qri", indirect_debug_to_string(yst.queries_state.queries[q_idx]));
                if (measured.diff != calc_diff) {
                    out("=== INDIRECT_MISMATCH_AFTER_FRONT_A2B ===");
                    out("mismatch_v", v,
                        "q_idx", q_idx,
                        "probe_idx", probe_idx,
                        "pushed_b_all_ord", pushed_b_all_ord,
                        "before", measured.before,
                        "after", measured.after,
                        "measured_diff", measured.diff,
                        "calc_diff", calc_diff);
                    out("after_front_a2b_sum_turn", measured.yst.queries_state.sum_turn);
                    for (int measured_q_idx = 0;
                         measured_q_idx < static_cast<int>(measured.yst.queries_state.queries.size());
                         measured_q_idx++) {
                        out("after_front_a2b_q",
                            measured_q_idx,
                            indirect_debug_to_string(measured.yst.queries_state.queries[measured_q_idx]));
                    }
                }
                my_assert(measured.diff == calc_diff);
            }
        }
    }

    QueryCommon insert_eval_a2b(
            YakinamashiState &eval_yst,
            const State &init_st,
            int v,
            int a2b_idx
    ) {
        QueryScanState scan_state = allow_build_scan_state_before_q_idx(eval_yst, init_st, a2b_idx);
        QueryCommon a2b_query = build_eval_a2b_query(eval_yst, scan_state, v);
        insert_query_common(eval_yst, init_st, a2b_idx, a2b_query);
        return a2b_query;
    }

    QueryCommon insert_eval_b2a(
            YakinamashiState &eval_yst,
            const State &init_st,
            int v,
            int b2a_idx
    ) {
        QueryScanState scan_state = allow_build_scan_state_before_q_idx(eval_yst, init_st, b2a_idx);
        QueryCommon b2a_query = build_eval_b2a_query(eval_yst, scan_state, v);
        insert_query_common(eval_yst, init_st, b2a_idx, b2a_query);
        return b2a_query;
    }

//  0手A2A でも後続 query の再計算影響は見る必要があるので、現在の scan state から単独 A2A query を組み立てる。
    QueryCommon build_eval_a2a_query(
            const YakinamashiState &yst,
            const QueryScanState &scan_state,
            int v,
            int a2a_idx,
            e_a2a_type a2a_type,
            int n
    ) {
        // _BEGIN: A2AP 廃止後の 0手 A2A 評価は swap_rot のみを直接 QueryCommon 化する。
        my_assert(a2a_type == e_a2a_type::swap_rot);
        auto exist_a_ords = scan_state.exist_a_ords;
        auto aorder = yst.query_order.aorder;
        OrdPos tar_ord_a1 = OrdCalc::get_tar_orda1_of_a2a<MAX_N>(
                exist_a_ords, aorder, v, scan_state.st.A.size(),
                scan_state.now_a_val_state[v]
        );
        RelOrd tar_rel_ord_a2 = OrdCalc::get_tar_rel_orda2_of_a2a<MAX_N>(
                exist_a_ords, aorder, v, scan_state.st.A.size(),
                scan_state.now_a_val_state[v]
        );
        OrdPos tar_ord_a2 = to_ord_pos(tar_rel_ord_a2, scan_state.st.A.size());
        int flip_dis_dir = A2AQueryUtil::calc_flip_dis_dir(
                tar_ord_a1, tar_ord_a2, scan_state.st.A.size(), a2a_type
        );
        OrdPos top_ord_b = get_query_start_ord_b(yst.queries_state, a2a_idx, n);
        return QueryCommon(
                CommonPushType::A2AS, v, scan_state.st.A.size(),
                tar_ord_a1.get(), tar_rel_ord_a2.get(), top_ord_b.get(), flip_dis_dir
        );
        // _END
    }

// 責務: 現在の eval_yst と a2a_idx 直前状態から A2A QueryCommon を作り、query 列へ挿入する。
// 必要な理由: fast 評価用に作った QueryCommon を使い回さず、AOrder 更新後の実状態に基づいて A2A を適用するため。
    QueryCommon insert_eval_a2a(
            YakinamashiState &eval_yst,
            const State &init_st,
            int v,
            int a2a_idx
    ) {
        QueryScanState scan_state = allow_build_scan_state_before_q_idx(eval_yst, init_st, a2a_idx);
        QueryCommon a2a_query = build_eval_a2a_query(
                eval_yst, scan_state, v, a2a_idx, e_a2a_type::swap_rot, init_st.current_N);
        insert_query_common(eval_yst, init_st, a2a_idx, a2a_query);
        return a2a_query;
    }

//  0手A2A は今は0手でも、挿入後の後続 query 再計算で総手数が変わるので、実際に挿入した状態で score を取る。
    std::pair<int, QueryCommon> evaluate_naive_no_move_a2a_plan(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            int a2a_idx,
            e_a2a_type a2a_type
    ) {
        YakinamashiState eval_yst = yst;
        QueryScanState scan_state = build_scan_state_before_q_idx(eval_yst, init_st, a2a_idx);
        QueryCommon a2a_query = build_eval_a2a_query(eval_yst, scan_state, v, a2a_idx, a2a_type, init_st.current_N);
        insert_query_common(eval_yst, init_st, a2a_idx, a2a_query);
        return {eval_yst.queries_state.sum_turn, a2a_query};
    }

    NaiveEvalResult evaluate_naive_insert_plan(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            int a2b_idx,
            int b2a_idx,
            AllOrd new_pushed_b_all_ord
    ) {
        NaiveEvalResult result;
        my_assert(0 <= a2b_idx && a2b_idx <= static_cast<int>(yst.queries_state.queries.size()));
        my_assert(0 <= b2a_idx && b2a_idx <= static_cast<int>(yst.queries_state.queries.size()));
        YakinamashiState eval_yst = build_eval_state(yst, v, new_pushed_b_all_ord);
        result.a2b_query = insert_eval_a2b(eval_yst, init_st, v, a2b_idx);
        int shifted_b2a_idx = b2a_idx;
        if (a2b_idx <= b2a_idx) shifted_b2a_idx++;
        result.b2a_query = insert_eval_b2a(eval_yst, init_st, v, shifted_b2a_idx);
        result.score = eval_yst.queries_state.sum_turn;
#ifdef DEBUG
        // _REVIEW_BEGIN: 高速差分と naive 差分がずれた時だけ対応 query を比較するため。
        result.queries = eval_yst.queries_state.queries;
        result.shifted_b2a_idx = shifted_b2a_idx;
        // _REVIEW_END
#endif
        return result;
    }

    void update_best_insert_plan(
            BestInsertPlan &best_plan,
            const NaiveEvalResult &result,
            int a2b_idx,
            int b2a_idx,
            AllOrd pushed_b_all_ord
    ) {
        my_assert(result.a2b_query.has_value() && result.b2a_query.has_value());
        if (best_plan.found && best_plan.best_score <= result.score) return;
        best_plan.found = true;
        best_plan.kind = BestInsertPlanKind::insert_pair;
        best_plan.best_score = result.score;
        best_plan.best_a2b_idx = a2b_idx;
        best_plan.best_b2a_idx = b2a_idx;
        best_plan.best_pushed_b_all_ord = pushed_b_all_ord;
        best_plan.best_a2b_query = result.a2b_query;
        best_plan.best_b2a_query = result.b2a_query;
    }

//  0手 A2A も best_plan に載せられるように、pair と同じ器で保持する。
    void update_best_no_move_a2a_plan(
            BestInsertPlan &best_plan, const NoMoveA2AInfo &info, int score, const QueryCommon &a2a_query
    ) {
        if (best_plan.found && best_plan.best_score <= score) return;
        best_plan.found = true;
        best_plan.kind = BestInsertPlanKind::single_a2a;
        best_plan.best_score = score;
        best_plan.best_a2a_idx = info.q_idx;
        best_plan.best_a2a_init_rank = info.init_rank;
        best_plan.best_a2a_target_rank = info.target_rank;
        best_plan.best_a2b_query.reset();
        best_plan.best_b2a_query.reset();
        best_plan.best_a2a_query = a2a_query;
    }

    void apply_best_insert_plan(YakinamashiState &yst, const State &init_st, int v, const BestInsertPlan &best_plan) {
        my_assert(best_plan.found);
        if (best_plan.kind == BestInsertPlanKind::single_a2a) {
            my_assert(best_plan.best_a2a_query.has_value());
            insert_query_common(yst, init_st, best_plan.best_a2a_idx, *best_plan.best_a2a_query);
            return;
        }
        my_assert(best_plan.best_a2b_query.has_value() && best_plan.best_b2a_query.has_value());
        yst.query_order.border.update_insert_ord_v(v, best_plan.best_pushed_b_all_ord);
        insert_query_common(yst, init_st, best_plan.best_a2b_idx, *best_plan.best_a2b_query);
        int shifted_b2a_idx = best_plan.best_b2a_idx;
        if (best_plan.best_a2b_idx <= best_plan.best_b2a_idx) shifted_b2a_idx++;
        insert_query_common(yst, init_st, shifted_b2a_idx, *best_plan.best_b2a_query);
    }

bool is_in_forward_arc(AllOrd from, AllOrd to, AllOrd val) {
    my_assert(val != from);
    my_assert(val != to);
    my_assert(from != to);
    int f = from.get(), t = to.get(), v = val.get();
    if (f < t) return (f < v && v < t);
    return (f < v || v < t);
}

AllOrd get_a_all_ord_of_tar_v(const YakinamashiState &yst, const QueryCommonRI &qri) {
    int tar_v = qri.get_query().get_tar_v();
    if (qri.is_a2b_type()) return yst.query_order.aorder.get_init_all_order(tar_v);
    if (qri.is_b2a_type()) return yst.query_order.aorder.get_target_order(tar_v);
    return yst.query_order.aorder.get_target_order(tar_v);
}

void log_indirect_v_all_ord_table(const YakinamashiState &yst, int n) {

#ifndef LOG_PHYSICAL
    return;
#endif
    const vector<int> b_target_orders = build_indirect_b_target_orders(yst.query_order.border, n);
    out("=== 値ごとのall_ord ===");
    for (int v = 0; v < n; v++) {
        out("v", v,
            "init_a", yst.query_order.aorder.get_init_all_order(v).get(),
            "target_a",
            yst.query_order.aorder.has_target_order(v) ? yst.query_order.aorder.get_target_order(v).get() : -1,
            "target_b", b_target_orders[v]);
    }
}

void log_indirect_query_snapshot_table(const YakinamashiState &yst) {

#ifndef LOG_PHYSICAL
    return;
#endif
    out("=== 基準query一覧 ===");
    for (int q_idx = 0; q_idx < static_cast<int>(yst.queries_state.queries.size()); q_idx++) {
        const QueryCommonRI &qri = yst.queries_state.queries[q_idx];
        out("q_idx", q_idx,
            "tar_v", qri.get_tar_v(),
            "a_all_ord", get_a_all_ord_of_tar_v(yst, qri).get(),
            "b_all_ord", qri.is_ab_type() ? yst.query_order.border.get_target_order(qri.get_tar_v()).get() : -1,
            "ins_turn", qri.get_ins_turn(),
            "query", indirect_debug_to_string(qri));
    }
}

void log_indirect_order_entries_lines(const char *section_title,
                                      const char *row_label,
                                      const my_vector<OrderEntry> &entries) {

#ifndef LOG_PHYSICAL
    return;
#endif
    out(section_title);
    for (int ord = 0; ord < static_cast<int>(entries.size()); ord++) {
        out(row_label,
            "ord", ord,
            "value", entries[ord].value,
            "val_state", indirect_debug_to_string(entries[ord].val_state));
    }
}

void log_indirect_case_snapshot(
        const State &init_st,
        const YakinamashiState &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {

#ifndef LOG_PHYSICAL
    return;
#endif
    out("");
    out("=== 間接検証の基準状態 ===");
    out("init_A_after_destroy", to_stack_string(init_st.A));
    out("destroyed_vs", to_destroyed_vs_string(destroyed_vs));
    log_indirect_order_entries_lines("=== AOrderEntries ===", "a_entry",
                                     yst.query_order.aorder.get_all_order_entries());
    log_indirect_order_entries_lines("=== BOrderEntries ===", "b_entry",
                                     yst.query_order.border.get_all_order_entries());
    log_indirect_v_all_ord_table(yst, init_st.current_N);
    log_indirect_query_snapshot_table(yst);
}

//  init 側削除後の終点を AllOrd のまま更新し、次の区間判定が ord 用 helper に依存しないようにする。
AllOrd apply_single_side_all_ord(AllOrd prev_all_ord, const SingleSideOrdEffect &side) {
    int ord = prev_all_ord.get();
    if (side.has_add()) {
        my_assert(prev_all_ord != side.get_add_ord());
        if (prev_all_ord > side.get_add_ord()) ord++;
    }
    if (side.has_del()) {
        my_assert(prev_all_ord != side.get_del_ord());
        if (prev_all_ord > side.get_del_ord()) ord--;
    }
    return AllOrd(ord);
}

// why: 4組合せの最小回転コストを計算（正確版で必須）
int compute_min_ab_cost(int ra, int rra, int rb, int rrb) {
    return min({
                       RotCalculator::calc_ra_rb_ins_turn(ra, rb),
                       RotCalculator::calc_ra_rrb_ins_turn(ra, rrb),
                       RotCalculator::calc_rra_rb_ins_turn(rra, rb),
                       RotCalculator::calc_rra_rrb_ins_turn(rra, rrb)
               });
}

// why: 弧上判定でRA/RRA距離変化を算出。端点一致やfrom==toは安全に0を返す。
std::pair<int, int> compute_arc_delta(AllOrd from, AllOrd to, AllOrd v) {
    if (from == v || to == v || from == to) return {0, 0};
    int d_ra = is_in_forward_arc(from, to, v) ? -1 : 0;
    int d_rra = is_in_forward_arc(to, from, v) ? -1 : 0;
    return {d_ra, d_rra};
}

// why: bitsetのk番目(0-indexed)のset bitの位置をAllOrdとして返す
template<size_t M>
AllOrd bitset_select_as_allord(const my_bitset<M> &bs, int k) {
    my_assert(k >= 0);
    int count = 0;
    for (size_t i = 0; i < M; i++) {
        if (bs[i]) {
            if (count == k) return AllOrd(static_cast<int>(i));
            count++;
        }
    }
    my_assert(false);
    return AllOrd(0);
}

struct PerQueryCostInfo {
    int base_diff = 0;
    int band_extra = 0;
    AllOrd band_start = AllOrd(0);
    AllOrd band_end = AllOrd(0);
    bool has_band = false;
};

// why: AB queryのA側orig RA/RRAを復元
std::pair<int, int> get_ab_ra_rra(const QueryCommonRI &qri) {
    qri.validate_is_ab_type();
    int stack_a = qri.get_stack_a_cnt();
    int a_cost = qri.get_a_cmd_cost_AB();
    if (qri.get_a_cmd_AB() == RA) return {a_cost, stack_a - a_cost};
    return {stack_a - a_cost, a_cost};
}

// why: AB queryのB側orig RB/RRBをOrdPosから計算
std::pair<int, int> get_ab_orig_rb_rrb(
        const QueryCommonRI &qri, OrdPos start_b, int N
) {
    qri.validate_is_ab_type();
    int stack_b = qri.get_stack_b_cnt(N);
    if (stack_b == 0) return {0, 0};
    OrdPos tar_b = qri.is_a2b_type()
                   ? to_ord_pos(qri.get_query().get_tar_rel_ord_b_A2B(), stack_b)
                   : qri.get_query().get_tar_ord_pos_b_B2A();
    int rb = RotCalculator::get_rb_dis(start_b, tar_b, stack_b);
    return {rb, stack_b - rb};
}

// why: AB queryのbase_diff/band_extraを正確版4組合せで計算
PerQueryCostInfo compute_ab_cost_info(
        int orig_cost, int ra, int rra, int orig_rb, int orig_rrb,
        int d_ra, int d_rra,
        int pa2a_ra, int pa2a_rra, int pa2a_d_ra, int pa2a_d_rra
) {
    PerQueryCostInfo info;
    int new_ra = ra + d_ra;
    int new_rra = rra + d_rra;
    int new_pa2a_ra = max(0, pa2a_ra + pa2a_d_ra);
    int new_pa2a_rra = max(0, pa2a_rra + pa2a_d_rra);
    int new_rb = max(0, orig_rb - new_pa2a_ra);
    int new_rrb = max(0, orig_rrb - new_pa2a_rra);

    int cost_rb_inc = compute_min_ab_cost(new_ra, new_rra, new_rb + 1, new_rrb);
    int cost_rrb_inc = compute_min_ab_cost(new_ra, new_rra, new_rb, new_rrb + 1);

    info.base_diff = min(cost_rb_inc, cost_rrb_inc) - orig_cost;
    info.band_extra = abs(cost_rb_inc - cost_rrb_inc);
    // why: 差が高々1でないとbitset集計が成り立たない
    my_assert(info.band_extra <= 1);
    return info;
}

// why: band区間のAllOrd端点を決定。worse側のarcをband区間とする。
void set_band_endpoints(
        PerQueryCostInfo &info, int diff_rb, int diff_rrb,
        OrdPos start_b, OrdPos tar_b, const my_bitset<MAX_ALL_ORD_B> &exist_b
) {
    AllOrd from_b = bitset_select_as_allord(exist_b, start_b.get());
    AllOrd to_b = bitset_select_as_allord(exist_b, tar_b.get());
    if (from_b == to_b) return;
    info.has_band = true;
    bool rb_worse = (diff_rb > diff_rrb);
    // why: RB arc = from→to forward, RRB arc = to→from forward
    info.band_start = rb_worse ? from_b : to_b;
    info.band_end = rb_worse ? to_b : from_b;
}

// why: AB queryのband端点を計算しpqに設定する
void compute_and_set_band(
        PerQueryCostInfo &pq, const QueryCommonRI &qri,
        int orig_cost, int ra, int rra, int orig_rb, int orig_rrb,
        int d_ra, int d_rra,
        int pa2a_ra, int pa2a_rra, int pa2a_d_ra, int pa2a_d_rra,
        OrdPos start_b, int N, const my_bitset<MAX_ALL_ORD_B> &exist_b
) {
    int stack_b = qri.get_stack_b_cnt(N);
    if (pq.band_extra == 0 || stack_b < 2) return;

    int npa_ra = max(0, pa2a_ra + pa2a_d_ra);
    int npa_rra = max(0, pa2a_rra + pa2a_d_rra);
    int new_rb = max(0, orig_rb - npa_ra);
    int new_rrb = max(0, orig_rrb - npa_rra);
    int nra = ra + d_ra, nrra = rra + d_rra;

    int diff_rb = compute_min_ab_cost(nra, nrra, new_rb + 1, new_rrb) - orig_cost;
    int diff_rrb = compute_min_ab_cost(nra, nrra, new_rb, new_rrb + 1) - orig_cost;

    OrdPos tar_b = qri.is_a2b_type()
                   ? to_ord_pos(qri.get_query().get_tar_rel_ord_b_A2B(), stack_b)
                   : qri.get_query().get_tar_ord_pos_b_B2A();
    set_band_endpoints(pq, diff_rb, diff_rrb, start_b, tar_b, exist_b);
}

bool is_in_segment_a(AllOrd from_ord, AllOrd to_ord, AllOrd tar_all_ord_a, command a_rot_cmd) {
    if (a_rot_cmd == RA) {
        return is_in_forward_arc(from_ord, to_ord, tar_all_ord_a);
    } else {
        my_assert(a_rot_cmd == RRA);
        return is_in_forward_arc(to_ord, from_ord, tar_all_ord_a);
    }
}


//[begin, end)の範囲に対してins_updateするといういみ
CircularRange make_circular_range_of_insert(int ins_upd_begin, int ins_upd_end, int all_ord_size) {
    my_assert(0 < all_ord_size);
    const int insert_slot_size = all_ord_size + 1;//一番後ろのall_ordとしてinsertできるように+1
    const int size = calc_half_open_all_ord_range_size(ins_upd_begin, ins_upd_end, insert_slot_size);
    return CircularRange(ins_upd_begin, size, insert_slot_size);
}

void register_band_segment(
        const CircularRange &all_ord_b_range,
        int q_idx,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &start_q_b_segment,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &end_q_b_segment,
        QueryBandRangeArray &q_idx_band_range_info
) {
    my_assert(!all_ord_b_range.is_empty() && !all_ord_b_range.is_full());
    q_idx_band_range_info[q_idx].has_band = true;
    q_idx_band_range_info[q_idx].band_range = all_ord_b_range;
    const int start = all_ord_b_range.get_start();
    int end = all_ord_b_range.get_end();
    if (end == 0) {
        end = all_ord_b_range.get_N();
    }
    my_assert(start != end);
    if (start < end) {
        start_q_b_segment[start].push_back(q_idx);
        end_q_b_segment[end].push_back(q_idx);
    } else {
        start_q_b_segment[start].push_back(q_idx);
        end_q_b_segment[all_ord_b_range.get_N()].push_back(q_idx);
        start_q_b_segment[0].push_back(q_idx);
        end_q_b_segment[end].push_back(q_idx);
    }
}

//  B 側候補を insert 区間と effect 後 start/target ord で保持する。
struct AppliedABVariant {
    CircularRange valid_all_ord_range_b = CircularRange(0, 0, 0);
    OrdPos effected_start_ord_b = OrdPos(0);
    OrdPos effected_tar_ord_b = OrdPos(0);
};

//  AB の差分集計に必要な最良回転結果と A 側終了 top をまとめる。
struct AppliedABInfo {
    BestRotInfo bri;
    CircularRange valid_all_ord_range_b = CircularRange(0, 0, 0);
    OrdPos finished_top_ord_a = OrdPos(0);
};

//  construct_v の direct 差分メモを helper 間で共有できる位置へ出す。
struct DirectDiff {
    int target_q_idx = -1;
    int diff = 0;
};

struct a2a_info {
    int ra_cnt;
    int rra_cnt;
    int ins_turn;
};

//  indirect 評価を q_idx bitset で進めるため、all_ord ごとの pair を task 化する。
struct IndirectTask {
    int a2b_idx = -1;
    int b2a_idx = -1;
};

// 責務: A2B/B2A 挿入差分評価に必要な band 情報、effect 後 query、prefix、境界 index をまとめて保持する。
// 必要な理由: qcnt / qcnt+1 サイズの配列を InsertPairPrecalc に集約しつつ、段階的に vec_array へ移行するため。
struct InsertPairPrecalc {
    std::array<vector<int>, MAX_INSERT_EVENT_B> b_band_start_qidxs;
    std::array<vector<int>, MAX_INSERT_EVENT_B> b_band_end_qidxs;
    // 責務: B all_ord ごとに、その all_ord が band に入る q_idx の bitset を保持する。
    // 必要な理由: 過去の a2b_idx owner と新 owner を同じ all_ord で比較するとき、band を巻き戻し計算できるようにするため。
    std::array<my_bitset<MAX_QUERY_CNT>, MAX_INSERT_EVENT_B> band_bits;
    QueryExistAArray exist_stack_a_ords;
    QueryExistBArray exist_stack_b_ords;
    QueryBandRangeArray a2b_band_ranges;
    QueryIntArray indirect_a2b_ra_A2A;
    QueryIntArray indirect_a2b_rra_A2A;
    QueryIntArray indirect_a2b_b2a_ra_A2A;
    QueryIntArray indirect_a2b_b2a_rra_A2A;
    QueryCommonArray effected_a2b_b2a_queries;
    QueryIntArray next_a2b_effected_valid_q_idx;
    QueryBoundaryArray prev_a2b_effected_valid_q_idx;
    // 責務: A2B effect だけを適用した replay で、各 query 実行後の A 側 finished_top_ord_a を保持する。
    // 必要な理由: finished_top_ord_a 単体へ AllOrd 仮定で effect を当てる危険な計算を避けるため。
    QueryOrdPosArray a2b_effected_finished_top_ord_a;
    QueryBoundaryArray next_a2b_b2a_effected_valid_q_idx;
    QueryBoundaryArray prev_a2b_b2a_effected_valid_q_idx;
    QueryPrefixArray fixed_indirect_a2b_prefix_sum;
    QueryPrefixArray fixed_indirect_a2b_b2a_prefix_sum;
    QueryPrefixArray base_ra_prefix_sum_A2A;
    QueryPrefixArray base_rra_prefix_sum_A2A;
    QueryPrefixArray indirect_a2b_ra_prefix_sum_A2A;
    QueryPrefixArray indirect_a2b_rra_prefix_sum_A2A;
    QueryPrefixArray indirect_a2b_b2a_ra_prefix_sum_A2A;
    QueryPrefixArray indirect_a2b_b2a_rra_prefix_sum_A2A;
    QueryBoundaryArray next_ab_idxs;
    QueryBoundaryArray prev_a2a_ranges;
    int direct_final_rot_diff = 0;
    int indirect_final_rot_diff = 0;
    // 責務: A2B+B2A effect を全 query に適用した後の A 側最終 top OrdPos を保持する。
    // 必要な理由: last valid query だけでは command no-cmd A2A の finished top 変化を拾えないため。
    OrdPos indirect_final_top_ord_a = OrdPos(0);
    // 責務: InsertPairPrecalc の再構築前に、保持コンテナと scalar を初期状態へ戻す。
    // 必要な理由: build_construct_v_precalc が static workspace を返しても前回 v の状態が混ざらないようにするため。
    void clear() {
        for (auto &qidxs: b_band_start_qidxs) qidxs.clear();
        for (auto &qidxs: b_band_end_qidxs) qidxs.clear();
        for (auto &bits: band_bits) bits = my_bitset<MAX_QUERY_CNT>();
        exist_stack_a_ords.clear();
        exist_stack_b_ords.clear();
        a2b_band_ranges.clear();
        indirect_a2b_ra_A2A.clear();
        indirect_a2b_rra_A2A.clear();
        indirect_a2b_b2a_ra_A2A.clear();
        indirect_a2b_b2a_rra_A2A.clear();
        effected_a2b_b2a_queries.clear();
        next_a2b_effected_valid_q_idx.clear();
        prev_a2b_effected_valid_q_idx.clear();
        a2b_effected_finished_top_ord_a.clear();
        next_a2b_b2a_effected_valid_q_idx.clear();
        prev_a2b_b2a_effected_valid_q_idx.clear();
        fixed_indirect_a2b_prefix_sum.clear();
        fixed_indirect_a2b_b2a_prefix_sum.clear();
        base_ra_prefix_sum_A2A.clear();
        base_rra_prefix_sum_A2A.clear();
        indirect_a2b_ra_prefix_sum_A2A.clear();
        indirect_a2b_rra_prefix_sum_A2A.clear();
        indirect_a2b_b2a_ra_prefix_sum_A2A.clear();
        indirect_a2b_b2a_rra_prefix_sum_A2A.clear();
        next_ab_idxs.clear();
        prev_a2a_ranges.clear();
        direct_final_rot_diff = 0;
        indirect_final_rot_diff = 0;
        indirect_final_top_ord_a = OrdPos(0);
    }
};

#define outaaa(...) do { if (is_collector_log_enabled()) outaaa(__VA_ARGS__); } while (false)
#include "sort_algorithm/YakinamashiQuery/Construction/GreedyQueriesCollector.h"
#include "sort_algorithm/YakinamashiQuery/Construction/GreedyQueriesCollectorOwnerRange.h"
#undef outaaa

    struct ScoredPair {
        PairQidx pair_qidx;
        AllOrd all_ord = AllOrd(0);
        int diff = INT_MAX;
        // 責務: A2B/B2A pair 採用時に適用する TARGET(v) の AOrder 挿入位置を保持する。
        // 必要な理由: B 側 all_ord だけでなく、target A all_ord 全探索で選んだ位置も実適用するため。
        AllOrd target_a_all_ord = AllOrd(0);
        bool has_target_a = false;
    };

    // 責務: best pair が候補成立するために必要だった limit と、retry 同幅 ext の必要量を保持する。
    // 必要な理由: retry が大きくなる原因が cost limit か range ext かを best pair 基準で切り分けるため。
    struct BestPairNeed {
        int required_a2b_lim = -1;
        int required_b2a_lim = -1;
        int required_from_ext_a = -1;
        int required_to_ext_a = -1;
        int required_from_ext_b = -1;
        int required_to_ext_b = -1;
        int required_a2b_a_ext = -1;
        int required_a2b_b_ext = -1;
        int required_b2a_a_ext = -1;
        int required_b2a_b_ext = -1;
    };

    // 責務: target A all_ord 全探索で試した候補、同点 best 候補、元 all_ord に最も近い選択候補、前後 diff を保持する。
    // 必要な理由: 同点 best から元の AOrder 構造に近い 1 つを選び、collector log へ探索内容を出すため。
    struct PairTargetAResult {
        bool found = false;
        int before_diff = INT_MAX;
        int best_diff = INT_MAX;
        // 責務: 全探索前の TARGET(v) の AOrder all_ord と、選択候補までの abs 距離を保持する。
        // 必要な理由: 同点時に元の AOrder 構造に最も近い候補を採用するため。
        AllOrd original_a_all_ord = AllOrd(0);
        int selected_a_distance = INT_MAX;
        vector<AllOrd> tried_a_all_ords;
        vector<AllOrd> best_a_all_ords;
        AllOrd selected_a_all_ord = AllOrd(0);
    };

    // 責務: fixed B all_ord 全探索で見つけた best ScoredPair と、その有無を保持する。
    // 必要な理由: fixed B 経路を既存 ScoredPair/apply_insert_pair_plan と接続するため。
    struct FixedBPairResult {
        bool found = false;
        ScoredPair pair;
    };

    // 責務: fixed B all_ord 探索で見た候補数と重い処理の呼び出し回数を保持する。
    // 必要な理由: 実行が遅い原因を qcnt、target A all_ord 数、評価回数から切り分けるため。
    struct FixedBLogCounter {
        int qcnt = 0;
        int a_all_ord_count = 0;
        long long pair_count = 0;
        long long eval_count = 0;
        long long rebuild_count = 0;
        long long insert_a2b_count = 0;
        long long insert_b2a_count = 0;
        int best_diff = INT_MAX;
    };

    // 責務: 採用する plan 種別、pair 候補、A2A 候補、採用差分を保持する。
    struct ChosenInsertPlan {
        BestInsertPlanKind kind = BestInsertPlanKind::insert_pair;
        ScoredPair pair;
        A2ACandidate a2a;
        int diff = INT_MAX;
    };
    
    // 責務: base 評価で選ばれた diff 昇順の上位 pair を保持する。
    struct TopKPlanResult {
        vector<ScoredPair> topk_pairs;
    };

    // 責務: top pair refine で使う top 数と q_idx/all_ord の近傍幅を保持する。
    // 必要な理由: refine の実験パラメータを 1 箇所で渡し、既存 top_k と混同しないようにするため。
    struct PairRefineParams {
        int top_k = 6;
        int a2b_idx_range = 3;
        int b2a_idx_range = 3;
        int all_ord_range = 3;
    };

    // 責務: refine で直接評価する a2b_idx/b2a_idx/all_ord_b の組を保持する。
    // 必要な理由: collector の range/limit 外の点も Mo 順に並べて評価できるようにするため。
    struct RefinePoint {
        int a2b_idx = -1;
        int b2a_idx = -1;
        int all_ord_b = -1;
    };

    struct BestPlanResult {
        bool found = false;
        ScoredPair best_pair;
        my_vector<ScoredPair> top_pairs;
    };

    // 責務: 旧候補 best、旧候補数、新 range への包含漏れ数、diff 不一致数を保持する。
    // 必要な理由: 新 PairRangeTask 経路が旧候補より悪い候補を選ぶ原因をログから切り分けるため。
    struct LegacyCheckResult {
        BestPlanResult legacy_best;
        int legacy_count = 0;
        int missing_count = 0;
        int diff_mismatch_count = 0;
    };

    // 責務: 指定 all_ord の B2A plan で、手数に関係する座標・PrevA2A・選択コマンドを保持する。
    // 必要な理由: 同一 OrdRange 内で b2a_insert_turn が変わった時に、tar/start/PrevA2A のどれが原因か比較するため。
    struct B2APlanProbeLog {
        int all_ord = -1;
        int stack_a_cnt = 0;
        int stack_b_cnt = 0;
        int tar_rel_ord_a = 0;
        int tar_ord_pos_a = 0;
        int tar_rel_ord_b = 0;
        int tar_ord_pos_b = 0;
        int start_ord_pos_a = 0;
        int start_ord_pos_b = 0;
        int prev_a2a_ra_sum = 0;
        int prev_a2a_rra_sum = 0;
        int cmd_a = 0;
        int cmd_a_cnt = 0;
        int cmd_b = 0;
        int cmd_b_cnt = 0;
        int ins_turn = 0;
        int finished_ord_pos_a = 0;
        int finished_ord_pos_b = 0;
    };

    struct ConstructVScanCtx;

    // 責務: 1 つの A2A target AllOrd 候補に対する effect 後 query、最初の AB 探索表、suffix 固定差分を保持する。
    // 必要な理由: AOrder を候補ごとに実更新せず、pair 評価と同じく effect と prefix sum で A2A 候補を採点するため。
    struct A2AFastPrecalc {
        QueryOrdEffect effect;
        QueryCommonArray effected_queries;
        QueryBoundaryArray next_ab_q_idx;
        QueryPrefixArray fixed_diff_prefix_sum;
        int final_rot_diff = 0;
        OrdPos final_top_ord_a = OrdPos(0);
        // 責務: A2AFastPrecalc の再構築前に、保持コンテナと scalar を初期状態へ戻す。
        // 必要な理由: build_a2a_fast_precalc が static workspace を返しても前回 target_all_ord の状態が混ざらないようにするため。
        void clear() {
            effected_queries.clear();
            next_ab_q_idx.clear();
            fixed_diff_prefix_sum.clear();
            final_rot_diff = 0;
            final_top_ord_a = OrdPos(0);
        }
    };

    // 責務: A2A chain と最初の AB の再計算差分、最終 A top、最初の AB index を保持する。
    // 必要な理由: A2A 挿入直後の chain は prefix 固定差分に混ぜず、PrevA2ARot を含めて正確評価するため。
    struct A2AReplayResult {
        int first_ab_idx = 0;
        int diff = 0;
        OrdPos finished_top_a = OrdPos(0);
        bool has_first_ab = false;
    };

    static constexpr size_t PAIR_SCORE_TABLE_WIDTH = MAX_QUERY_CNT + 1;
    static constexpr size_t PAIR_SCORE_TABLE_SIZE = PAIR_SCORE_TABLE_WIDTH * PAIR_SCORE_TABLE_WIDTH;

    // 責務: pair score table の 2 次元 pair index を flat 配列 index へ変換する。
    // 必要な理由: table 容量と pair の意味を変えずに、MSVC が扱う型を単純な 1 次元配列にするため。
    static size_t pair_score_index(int a2b_idx, int b2a_idx) {
        my_assert(0 <= a2b_idx && a2b_idx <= MAX_QUERY_CNT);
        my_assert(0 <= b2a_idx && b2a_idx <= MAX_QUERY_CNT);
        return static_cast<size_t>(a2b_idx) * PAIR_SCORE_TABLE_WIDTH + static_cast<size_t>(b2a_idx);
    }

    // 責務: a2b_idx/b2a_idx pair ごとの最良 ScoredPair と有効世代を保持する。
    // 必要な理由: 同じ pair の評価済み best score を世代 stamp 付きで再利用するため。
    struct PairBestScoreTable {
        std::array<ScoredPair, PAIR_SCORE_TABLE_SIZE> best_scores{};
        std::array<int, PAIR_SCORE_TABLE_SIZE> stamps{};
        int current_stamp = 0;
    };

    // _REVIEW: construct_v の候補差分計算を Helper 内の型・関数だけで完結させるため。
    int calc_diff_without_band(
            const YakinamashiState &case_yst,
            const State &init_st,
            int v,
            int a2b_idx,
            int b2a_idx,
            AllOrd all_ord_b,
            const InsertPairPrecalc &prec
    );

    ScoredPair eval_insert_pair_qidx(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            const PairQidx &pair_qidx,
            int all_ord_b,
            const my_bitset<MAX_QUERY_CNT> &is_band,
            InsertPairPrecalc &prec
    );

    ScoredPair eval_pair_range_task(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            const BasePairCandidates &candidates,
            const PairRangeTask &task,
            BandMinSeg &seg,
            InsertPairPrecalc &prec
    );
    BestPairNeed calc_best_pair_need(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            const ScoredPair &best_pair,
            const InsertPairPrecalc &prec,
            const my_bitset<MAX_N> &is_never_construct_v
    );

    void write_best_pair_need(
            int v,
            const ScoredPair &best_pair,
            const BestPairNeed &need
    );
    PairCostLog build_no_band_probe_cost_log(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            int a2b_idx,
            int b2a_idx,
            AllOrd pushed_b_all_ord,
            InsertPairPrecalc &prec
    );
    B2APlanProbeLog build_b2a_plan_probe_log(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            int a2b_idx,
            int b2a_idx,
            AllOrd pushed_b_all_ord,
            InsertPairPrecalc &prec
    );
    void log_b2a_plan_probe(
            const char *label,
            const B2APlanProbeLog &log
    );
    void log_b2a_exist_b_ords_probe(
            int b2a_idx,
            InsertPairPrecalc &prec
    );
    void log_b2a_range_points_probe(
            const BasePairCandidates &candidates,
            const PairRangeTask &task
    );
    void log_no_band_range_diff_probe(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            const BasePairCandidates &candidates,
            const PairRangeTask &task,
            InsertPairPrecalc &prec
    );
    BestPlanResult solve_pair_range_tasks(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            const BasePairCandidates &candidates,
            InsertPairPrecalc &prec
    );
    TopKPlanResult calc_base_topk_plan(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            BasePairCandidates &base_candidates,
            InsertPairPrecalc &prec,
            int top_k,
            int &cand_cnt1
    );

    BestPlanResult calc_topk_expanded_best(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            InsertPairCandidatesByAllOrd &topk_expanded_candidates,
            InsertPairPrecalc &prec,
            int &cand_cnt2
    );

    void start_pair_best_scores(PairBestScoreTable &table);
    bool has_pair_score(const PairBestScoreTable &table, const PairQidx &pair_qidx);
    const ScoredPair &get_pair_best_score(const PairBestScoreTable &table, const PairQidx &pair_qidx);
    void update_pair_best_score(PairBestScoreTable &table, const ScoredPair &scored_pair);
    void insert_topk_pair(TopKPlanResult &result, const ScoredPair &scored_pair, int top_k);
    TopKPlanResult build_topk_pairs(
            const BasePairCandidates &base_candidates,
            const PairBestScoreTable &table,
            int top_k
    );
    bool contains_ord_range(const BasePairCandidates &candidates, const PairRangeTask &task, int all_ord);
    const PairRangeTask *find_pair_task(
            const BasePairCandidates &candidates,
            const PairQidx &pair_qidx
    );
    int check_legacy_coverage(
            const LegacyBasePairCandidates &legacy_candidates,
            const BasePairCandidates &new_candidates
    );
    int check_legacy_diffs(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            const LegacyBasePairCandidates &legacy_candidates,
            InsertPairPrecalc &prec
    );
    bool band_contains_all_ord(
            const BandRangeInfo &band,
            int all_ord_b
    );
    bool is_direct_excluded_q_idx(
            int q_idx,
            int a2b_idx,
            int b2a_idx,
            InsertPairPrecalc &prec,
            int qcnt
    );
    int eval_pair_fixed_all_ord_diff(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            const PairQidx &pair_qidx,
            int all_ord_b,
            InsertPairPrecalc &prec
    );
    LegacyCheckResult run_legacy_check(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            const BasePairCandidates &new_candidates,
            const BestPlanResult &new_result,
            InsertPairPrecalc &prec,
            const GreedyConstructorParams &params,
            const my_bitset<MAX_N> &is_never_construct_v
    );
    void abort_if_legacy_best_pair_differs(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            const BestPlanResult &new_result,
            InsertPairPrecalc &prec,
            const GreedyConstructorParams &params,
            const my_bitset<MAX_N> &is_never_construct_v
    );
    void log_legacy_check(
            int v,
            const BestPlanResult &new_result,
            const LegacyCheckResult &check_result
    );
    ScoredPair choose_best_plan(
            const TopKPlanResult &base_result,
            const BestPlanResult &topk_expanded_result
    );
    ChosenInsertPlan choose_a2a_or_pair_plan(
            const ScoredPair &best_pair,
            const A2ACandidate &best_a2a
    );
    A2ACandidate collect_best_a2a_fixed_a(
            const YakinamashiState &yst,
            const State &current_st,
            int v
    );
    ScoredPair collect_best_pair_fixed_a(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            const GreedyConstructorParams &params,
            const my_bitset<MAX_N> &is_never_construct_v
    );
    ChosenInsertPlan choose_fixed_a_plan(
            const ScoredPair &best_pair,
            const A2ACandidate &best_a2a
    );
    void apply_pair_plan_fixed_a(
            YakinamashiState &yst,
            const State &current_st,
            int v,
            const ScoredPair &best_pair
    );
    void apply_a2a_plan_fixed_a(
            YakinamashiState &yst,
            const State &current_st,
            int v,
            const A2ACandidate &best_a2a
    );
    void restore_entry_by_plan(
            PhysicalDestroyContext<MAX_N> &context,
            const AOrderRangePlan<MAX_N> &plan,
            int v,
            const my_bitset<MAX_N> &missing_after_restore
    );
    static my_vector<OrderEntry> normalize_entries_for_compare(
            my_vector<OrderEntry> entries
    );
    static bool entries_equal_normalized_debug(
            my_vector<OrderEntry> lhs,
            my_vector<OrderEntry> rhs
    );
    static int find_optional_entry_pos(
            const my_vector<OrderEntry>& entries,
            const OrderEntry& target_entry
    );
    static int find_plan_insert_pos(
            const my_vector<OrderEntry>& plan_entries,
            const my_vector<OrderEntry>& current_entries,
            int plan_pos
    );
    void validate_plan_entries_debug(
            const YakinamashiState &yst,
            const AOrderRangePlan<MAX_N> &plan
    );
    void validate_plan_subset_debug(
            const YakinamashiState &yst,
            const AOrderRangePlan<MAX_N> &plan,
            const my_bitset<MAX_N> &missing_v
    );
    void validate_destroyed_absent_debug(
            const PhysicalDestroyContext<MAX_N> &context,
            const AOrderRangePlan<MAX_N> &plan
    );
    void validate_fixed_construct_debug(
            const State &current_st,
            const YakinamashiState &yst
    );
    void apply_insert_pair_plan(
            YakinamashiState &yst,
            const State &current_st,
            int v,
            const ScoredPair &best_pair
    );
    void apply_a2a_plan(
            YakinamashiState &yst,
            const State &current_st,
            int v,
            const A2ACandidate &best_a2a
    );
    void apply_chosen_insert_plan(
            YakinamashiState &yst,
            const State &current_st,
            int v,
            const ChosenInsertPlan &chosen
    );
    int scan_candidates_for_scores(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            InsertPairCandidatesByAllOrd &candidates,
            InsertPairPrecalc &prec,
            PairBestScoreTable &pair_scores
    );
    void update_best_result(BestPlanResult &result, const ScoredPair &scored_pair);
    void insert_top_pair_result(BestPlanResult &result, const ScoredPair &scored_pair, int top_k);
    void validate_top_pairs_debug(const BestPlanResult &result, int top_k);
    void validate_refine_point_debug(const RefinePoint &point, int qcnt, int b_insert_slot_count);
    void validate_refine_points_debug(const my_vector<RefinePoint> &points, int qcnt, int b_insert_slot_count);
    void validate_refine_score_debug(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            const RefinePoint &point,
            const ScoredPair &scored_pair
    );
    my_vector<RefinePoint> build_refine_points(
            const BestPlanResult &base_result,
            const PairRefineParams &refine_params,
            int qcnt,
            int b_insert_slot_count
    );
    ScoredPair eval_refine_point(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            const RefinePoint &point,
            BandMinSeg &seg,
            InsertPairPrecalc &prec
    );
    BestPlanResult solve_refine_points_mo(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            const my_vector<RefinePoint> &points,
            InsertPairPrecalc &prec
    );
    BestPlanResult refine_top_pairs(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            const BestPlanResult &base_result,
            InsertPairPrecalc &prec
    );
    BestPlanResult scan_candidates_for_best(
            const YakinamashiState &yst,
            const State &init_st,
            int v,
            InsertPairCandidatesByAllOrd &candidates,
            InsertPairPrecalc &prec,
            int &evaluated_count
    );
    const BasePairInfo &find_base_pair_info(
            const BasePairCandidates &base_candidates,
            const PairQidx &pair_qidx
    );
    void append_expanded_pair_qidx(
            InsertPairCandidatesByAllOrd &topk_expanded_candidates,
            const PairQidx &pair_qidx,
            int all_ord
    );
    int append_common_range_except_base_ords(
            InsertPairCandidatesByAllOrd &topk_expanded_candidates,
            const BasePairInfo &pair_info
    );
    InsertPairCandidatesByAllOrd build_topk_expanded_candidates(
            const BasePairCandidates &base_candidates,
            const TopKPlanResult &base_result
    );
    QueryBoundaryArray build_next_ab_q_idx(const QRIList &queries);
    AllOrd resolve_moved_target_ord(
            const YakinamashiState &yst,
            int v,
            AllOrd target_all_ord
    );
    QueryOrdEffect build_a2a_effect(
            const YakinamashiState &yst,
            int v,
            AllOrd target_all_ord
    );
    QueryCommon build_fast_a2a_query(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            int q_idx,
            AllOrd target_all_ord,
            const QueryScanState& scan_state
    );
    A2AFastPrecalc &build_a2a_fast_precalc(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            AllOrd target_all_ord
    );
    int calc_fixed_a2a_insert_diff_ab(
            const YakinamashiState &yst,
            int q_idx,
            const QueryCommonRI &qri,
            ConstructVScanCtx &ctx
    );
    A2AReplayResult calc_a2a_replay_diff(
            const YakinamashiState &yst,
            const State &current_st,
            int q_idx,
            const QueryCommonRI &inserted_a2a_qri,
            const A2AFastPrecalc &precalc
    );
    PrevA2ARotSum build_a2a_replay_initial_rot_sum(
            const QRIList &queries,
            int q_idx,
            const QueryCommonRI &inserted_a2a_qri
    );
    A2ACandidate eval_a2a_candidate_fast(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            int q_idx,
            AllOrd target_all_ord,
            const A2AFastPrecalc &precalc,
            const QueryScanState& scan_state
    );
    InsertRangeSet build_aorder_v_ranges(
            const YakinamashiState &yst,
            const State &current_st,
            int v
    );
    void for_each_all_ord_in_ranges(
            const InsertRangeSet &ranges,
            const function<void(AllOrd)> &func
    );
    bool pick_fixed_b_mode();
    void append_fixed_b_log_line(
            const std::string &line
    );
    void out_fixed_b_log(
            int v,
            AllOrd fixed_b_all_ord,
            const FixedBLogCounter &counter
    );
    bool has_b_target_order(
            const YakinamashiState &yst,
            int v
    );
    bool snapshot_has_b_target_order(
            const PhysicalDestroyContext<MAX_N> &context,
            int v
    );
    int eval_fixed_b_pair(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            int a2b_idx,
            int b2a_idx,
            AllOrd fixed_b_all_ord,
            AllOrd target_a_all_ord
    );
    FixedBPairResult collect_fixed_b_pair(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            AllOrd fixed_b_all_ord
    );
    void apply_fixed_b_pair(
            YakinamashiState &yst,
            const State &current_st,
            int v,
            const FixedBPairResult &result
    );
    int find_restore_b_pos(
            const PhysicalDestroyContext<MAX_N> &context,
            int v
    );
    AllOrd restore_fixed_b_order(
            PhysicalDestroyContext<MAX_N> &context,
            int v
    );
    int eval_pair_target_a(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            const ScoredPair &base_pair,
            AllOrd target_a_all_ord
    );
    AllOrd select_pair_target_a(
            const vector<AllOrd> &best_a_all_ords,
            AllOrd original_a_all_ord,
            int &selected_distance
    );
    ScoredPair optimize_pair_target_a(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            const ScoredPair &base_pair
    );
    A2ACandidate collect_best_a2a_fast(
            const YakinamashiState &yst,
            const State &current_st,
            int v
    );
    void assert_a2a_fast_matches_naive(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            const A2ACandidate &fast,
            const QueryScanState& scan_state
    );
    A2ACandidate eval_a2a_candidate_naive_no_shift(
            const YakinamashiState &yst,
            const State &current_st,
            int v,
            int q_idx,
            AllOrd target_all_ord,
            const QueryScanState& scan_state
    );
    void log_a2a_order_snapshot(
            const char *tag,
            const YakinamashiState &yst,
            int v,
            AllOrd target_all_ord
    );
//    void log_a2a_eval_breakdown(
//            const YakinamashiState &yst,
//            const State &current_st,
//            int v,
//            const A2ACandidate &candidate
//    );
    void log_a2a_actual_breakdown(
            const YakinamashiState &before_yst,
            const YakinamashiState &after_yst,
            const State &current_st,
            int v,
            const A2ACandidate &candidate,
            int before_sum_turn
    );

//  construct_v の applied query メモは static 配列へ戻し、毎回の確保を避ける。
//static std::array<QueryCommon, MAX_BUFF * 10> g_memo_applied_queries;

//  construct_v の前計算状態を 1 つにまとめ、helper 間の引数ばらつきを抑える。
struct ConstructVScanCtx {
    QueryOrdEffect a_only_a2b_effect;
    QueryOrdEffect a_only_b2a_effect;
    QueryProvider provider;
    QCRIFactory cri_factory;
    //todo effectedとはa2bなのか、どっちでもつかうのか
    OrdPos effected_prev_finished_top_ord_a = OrdPos(0);
    OrdPos base_prev_finished_top_ord_b = OrdPos(0);
    PrevA2ARotSum effected_prev_a2a_rot_sum = PrevA2ARotSum(0, 0);
    bool was_ab_type = false;
    CommonPushType prev_ab_type = CommonPushType::A2AS;//dummy
    int prev_ab_idx = -1;//dummy
    AllOrd prev_tar_all_ord_b = AllOrd(0);
    bool was_valid_a2a = false;
};

//  band の start/end event から現在有効な q_idx bitset を更新する。
void update_active_band_q_idxs(
        int all_ord_b,
        const std::array<vector<int>, MAX_INSERT_EVENT_B> &start_q_b_segment,
        const std::array<vector<int>, MAX_INSERT_EVENT_B> &end_q_b_segment,
        my_bitset<MAX_QUERY_CNT> &active_band_q_idxs
) {
    for (int q_idx: start_q_b_segment[all_ord_b]) {
        my_assert(active_band_q_idxs[q_idx] == 0);
        active_band_q_idxs.set(q_idx);
    }
    for (int q_idx: end_q_b_segment[all_ord_b]) {
        my_assert(active_band_q_idxs[q_idx] == 1);
        active_band_q_idxs.reset(q_idx);
    }
}

//  区間内の有効 band 数は既存 pop_cnt を使って半開区間で取る。
int count_active_band_q_idxs_in_range(
        const my_bitset<MAX_QUERY_CNT> &active_band_q_idxs,
        int begin_idx,
        int end_idx
) {
    my_assert(0 <= begin_idx && begin_idx <= end_idx);
    my_assert(end_idx <= MAX_QUERY_CNT);
    return pop_cnt(active_band_q_idxs, begin_idx, end_idx);
}

//  indirect base は累積和の差だけで取れるので helper に切り出す。
int get_fixed_indirect_range_sum(
        const QueryPrefixArray &fixed_indirect_prefix_sum,
        int a2b_idx,
        int b2a_idx
) {
    my_assert(0 <= a2b_idx && a2b_idx <= b2a_idx);
    my_assert(b2a_idx + 1 <= fixed_indirect_prefix_sum.size());
    return fixed_indirect_prefix_sum[b2a_idx] - fixed_indirect_prefix_sum[a2b_idx];
}

//  direct 差し引き前の indirect 合計を base+band でまとめて取る。
int get_tmp_indirect_sum(
        const QueryPrefixArray &fixed_indirect_prefix_sum,
        int a2b_idx,
        int b2a_idx,
        const my_bitset<MAX_QUERY_CNT> &active_band_q_idxs
) {
    int base_sum = get_fixed_indirect_range_sum(fixed_indirect_prefix_sum, a2b_idx, b2a_idx);
    int band_count = count_active_band_q_idxs_in_range(active_band_q_idxs, a2b_idx, b2a_idx);
    return base_sum + band_count;
}

//  construct_v だけ prev_a2a 同期量つきの AB 最良回転が必要なので、既存 BestRotInfo API の前段だけここで補う。
BestRotInfo build_brp_by_ord_with_prev_a2a(
        int stack_a_size,
        int stack_b_size,
        OrdPos from_orda,
        OrdPos from_ordb,
        OrdPos to_orda,
        OrdPos to_ordb,
        const PrevA2ARotSum &prev_a2a_rot_sum
) {
    if (!(0 <= from_ordb.get() && from_ordb.get() < stack_b_size) ||
        !(0 <= to_ordb.get() && to_ordb.get() < stack_b_size)) {
        out("construct_build_brp_invalid_ord",
            "stack_a_size", stack_a_size,
            "stack_b_size", stack_b_size,
            "from_orda", from_orda.get(),
            "from_ordb", from_ordb.get(),
            "to_orda", to_orda.get(),
            "to_ordb", to_ordb.get(),
            "prev_ra_sum", prev_a2a_rot_sum.ra_sum,
            "prev_rra_sum", prev_a2a_rot_sum.rra_sum);
    }
    const int ra_dis = RotCalculator::get_ra_dis(from_orda, to_orda, stack_a_size);
    const int rra_dis = RotCalculator::get_rra_dis(from_orda, to_orda, stack_a_size);
    const int orig_rb_dis = RotCalculator::get_rb_dis(from_ordb, to_ordb, stack_b_size);
    const int orig_rrb_dis = RotCalculator::get_rrb_dis(from_ordb, to_ordb, stack_b_size);
    auto [sync_rb_dis, sync_rrb_dis] = PrevA2ARotSumCalc::get_prev_a2a_syncd_rot_b(
            orig_rb_dis, orig_rrb_dis, prev_a2a_rot_sum);
    return build_brp_by_rot(ra_dis, rra_dis, sync_rb_dis, sync_rrb_dis);
}

std::array<AppliedABVariant, 2> build_ab_b_ranges(
        const QueryProvider &qprov,
        const YakinamashiState &yst,
        int q_idx,
        const QueryCommon &applied_a_effect_query,
        const BOrder &border,
        AllOrd prev_query_tar_all_ord_b,
        OrdPos start_ord_b,
        int stack_b_cnt_after_insert,
        const ConstructVScanCtx &ctx
) {
    const int tar_v = applied_a_effect_query.get_tar_v();
    const AllOrd tar_all_ord_b = border.get_target_order(tar_v);
    const int stack_b_cnt_before_insert = stack_b_cnt_after_insert - 1;
    const OrdPos base_tar_ord_b = applied_a_effect_query.is_a2b_type()
                                  ? to_ord_pos(applied_a_effect_query.get_tar_rel_ord_b_A2B(),
                                               stack_b_cnt_before_insert)
                                  : applied_a_effect_query.get_tar_ord_pos_b_B2A();
    const int all_ord_size = border.get_all_order_entries_size();
    my_assert(prev_query_tar_all_ord_b.get() != tar_all_ord_b.get());
    //midはstart-toの間
    //after_toはtoの後に挿入する場合
    AppliedABVariant mid, after_to;
    int N = yst.queries_state.current_N;
    if (stack_b_cnt_before_insert == 0) {
        mid.effected_start_ord_b = OrdPos(0);
        mid.effected_tar_ord_b = OrdPos(0);
        after_to.effected_start_ord_b = OrdPos(0);
        after_to.effected_tar_ord_b = OrdPos(0);
    } else {
        auto get_effected_start_ord_b = [&](const SingleSideOrdEffect &add_b_effect) -> OrdPos {
            my_assert(ctx.prev_ab_idx >= 0);
            const QueryCommon &last_ab_q = yst.queries_state.queries[ctx.prev_ab_idx].get_query();
            if (last_ab_q.is_a2b_type()) {
                RelOrd effceted_start_rel = qprov.apply_single_side_ord(last_ab_q.get_tar_rel_ord_b_A2B(),
                                                                        prev_query_tar_all_ord_b, add_b_effect);
                return OrdCalc::get_inserted_ord(effceted_start_rel);
            } else {
                my_assert(last_ab_q.is_b2a_type());
                OrdPos effected_erase_ord_pos = qprov.apply_single_side_ord(last_ab_q.get_tar_ord_pos_b_B2A(),
                                                                            prev_query_tar_all_ord_b, add_b_effect);
                return OrdCalc::get_erased_next_ord(effected_erase_ord_pos, last_ab_q.get_stack_b_cnt(N) + 1);
            }
        };
        auto get_effected_to_ord_b = [&](const SingleSideOrdEffect &add_b_effect) -> OrdPos {
            const QueryCommon &q = yst.queries_state.queries[q_idx].get_query();
            if (q.is_a2b_type()) {
                RelOrd effected_to_rel = qprov.apply_single_side_ord(q.get_tar_rel_ord_b_A2B(), tar_all_ord_b,
                                                                     add_b_effect);
                return to_ord_pos(effected_to_rel, stack_b_cnt_after_insert);
            } else {
                my_assert(q.is_b2a_type());
                OrdPos effected_to_ord_pos = qprov.apply_single_side_ord(q.get_tar_ord_pos_b_B2A(), tar_all_ord_b,
                                                                         add_b_effect);
                return effected_to_ord_pos;
            }
        };
        CircularRange mid_range = make_circular_range_of_insert(prev_query_tar_all_ord_b.get() + 1,
                                                                tar_all_ord_b.get() + 1,
                                                                all_ord_size);
        SingleSideOrdEffect add_mid_effect;
        add_mid_effect.set_add(AllOrd(mid_range.get_start()));
        mid = AppliedABVariant{
                mid_range,
                get_effected_start_ord_b(add_mid_effect),
                get_effected_to_ord_b(add_mid_effect)
        };
        CircularRange after_to_range = make_circular_range_of_insert(tar_all_ord_b.get() + 1,
                                                                     prev_query_tar_all_ord_b.get() + 1,
                                                                     all_ord_size);
        SingleSideOrdEffect add_after_to_effect;
        add_after_to_effect.set_add(AllOrd(after_to_range.get_start()));
        after_to = AppliedABVariant{
                after_to_range,
                get_effected_start_ord_b(add_after_to_effect),
                get_effected_to_ord_b(add_after_to_effect),
        };
    }
    my_assert(0 <= mid.effected_start_ord_b.get() && mid.effected_start_ord_b.get() < stack_b_cnt_after_insert);
    my_assert(0 <= mid.effected_tar_ord_b.get() && mid.effected_tar_ord_b.get() < stack_b_cnt_after_insert);
    my_assert(
            0 <= after_to.effected_start_ord_b.get() && after_to.effected_start_ord_b.get() < stack_b_cnt_after_insert);
    my_assert(0 <= after_to.effected_tar_ord_b.get() && after_to.effected_tar_ord_b.get() < stack_b_cnt_after_insert);
    if (!(0 <= mid.effected_start_ord_b.get() && mid.effected_start_ord_b.get() < stack_b_cnt_after_insert) ||
        !(0 <= mid.effected_tar_ord_b.get() && mid.effected_tar_ord_b.get() < stack_b_cnt_after_insert) ||
        !(0 <= after_to.effected_start_ord_b.get() && after_to.effected_start_ord_b.get() < stack_b_cnt_after_insert) ||
        !(0 <= after_to.effected_tar_ord_b.get() && after_to.effected_tar_ord_b.get() < stack_b_cnt_after_insert)) {
        out("construct_ab_b_ranges_invalid",
            "query", indirect_debug_to_string(applied_a_effect_query),
            "prev_query_tar_all_ord_b", prev_query_tar_all_ord_b.get(),
            "tar_all_ord_b", tar_all_ord_b.get(),
            "start_ord_b", start_ord_b.get(),
            "base_tar_ord_b", base_tar_ord_b.get(),
            "stack_b_cnt_after_insert", stack_b_cnt_after_insert,
            "mid_start_ord_b", mid.effected_start_ord_b.get(),
            "mid_tar_ord_b", mid.effected_tar_ord_b.get(),
            "after_start_ord_b", after_to.effected_start_ord_b.get(),
            "after_tar_ord_b", after_to.effected_tar_ord_b.get());
    }
    return {mid, after_to};
}

//  AB 候補ごとの差分は A 側適用済み query と B 区間候補から最良回転を再計算する。
AppliedABInfo build_applied_ab_info(
        const QueryCommon &applied_a_effect_query,
        const AppliedABVariant &variant,
        OrdPos applied_tar_ord_pos_a,
        const PrevA2ARotSum &prev_a2a_rot_sum,
        OrdPos effected_start_top_ord_a,
        int stack_b_cnt_after_insert,
        int n
) {
    const int stack_a_cnt = applied_a_effect_query.get_stack_a_cnt();
    if (!(0 <= variant.effected_start_ord_b.get() && variant.effected_start_ord_b.get() < stack_b_cnt_after_insert) ||
        !(0 <= variant.effected_tar_ord_b.get() && variant.effected_tar_ord_b.get() < stack_b_cnt_after_insert)) {
        out("construct_applied_ab_info_invalid",
            "query", indirect_debug_to_string(applied_a_effect_query),
            "stack_b_cnt_after_insert", stack_b_cnt_after_insert,
            "variant_start_ord_b", variant.effected_start_ord_b.get(),
            "variant_tar_ord_b", variant.effected_tar_ord_b.get(),
            "variant_range_start", variant.valid_all_ord_range_b.get_start(),
            "variant_range_end", variant.valid_all_ord_range_b.get_end(),
            "variant_range_n", variant.valid_all_ord_range_b.get_N(),
            "applied_tar_ord_pos_a", applied_tar_ord_pos_a.get(),
            "effected_start_top_ord_a", effected_start_top_ord_a.get(),
            "prev_ra_sum", prev_a2a_rot_sum.ra_sum,
            "prev_rra_sum", prev_a2a_rot_sum.rra_sum);
    }
    BestRotInfo bri = build_brp_by_ord_with_prev_a2a(
            stack_a_cnt,
            stack_b_cnt_after_insert,
            effected_start_top_ord_a,
            variant.effected_start_ord_b,
            applied_tar_ord_pos_a,
            variant.effected_tar_ord_b,
            prev_a2a_rot_sum
    );
    return {
            bri,
            variant.valid_all_ord_range_b,
            applied_a_effect_query.get_finished_top_ord_a(n)
    };
}

void clear_band_buffers(
        int insert_event_size,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &start_q_b_segment,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &end_q_b_segment,
        std::array<vector<IndirectTask>, MAX_INSERT_EVENT_B> &indirect_tasks_by_b_all_ord
) {
    for (int all_ord_b = 0; all_ord_b < insert_event_size; all_ord_b++) {
        start_q_b_segment[all_ord_b].clear();
        end_q_b_segment[all_ord_b].clear();
        indirect_tasks_by_b_all_ord[all_ord_b].clear();
    }
}

void clear_direct_buffers(
        int qcnt,
        std::array<vector<DirectDiff>, MAX_QUERY_CNT> &a2b_direct_diff,
        std::array<vector<DirectDiff>, MAX_QUERY_CNT> &b2a_direct_diff
) {
    for (int q_idx = 0; q_idx < qcnt; q_idx++) {
        a2b_direct_diff[q_idx].clear();
        b2a_direct_diff[q_idx].clear();
    }
}

//[q_idx] := q_idxに挿入した際に、directに影響をあたえるidx達
//final_rotに影響を与える場合は、qcntをもつ
vector<vector<int>> make_direct_idxs(const QueriesState &queries_state) {
    const int qcnt = static_cast<int>(queries_state.queries.size());
    vector<vector<int>> direct_idxs(qcnt + 1);
    direct_idxs[qcnt].push_back(qcnt);//final_rotへの影響
    int next_ab_idx = -1;
    for (int insert_q_idx = qcnt - 1; insert_q_idx >= 0; insert_q_idx--) {
        const QueryCommonRI &qri = queries_state.queries[insert_q_idx];
        // _BEGIN: command no-cmd A2A は direct 境界にせず、後続 AB だけを直接再計算対象にする。
        if (qri.is_a2a_type() && qri.get_flip_dis_dir_A2A() == 0) {
            if (next_ab_idx != -1) {
                direct_idxs[insert_q_idx].push_back(next_ab_idx);
            }
            continue;
        }
        // _END
        direct_idxs[insert_q_idx].push_back(insert_q_idx);
        if (qri.is_a2a_type() && next_ab_idx != -1) {
            direct_idxs[insert_q_idx].push_back(next_ab_idx);
        }
        if (qri.is_ab_type()) {
            next_ab_idx = insert_q_idx;
        }
    }
    return direct_idxs;
}

ConstructVScanCtx init_scan_ctx(
        const vector<int> &init_a_pos,
        int v,
        const YakinamashiState &yst,
        const State &init_st,
        AllOrd deleted_init_all_ord_a,
        AllOrd add_tar_all_ord_a,
        int n
) {
    ConstructVScanCtx ctx{
            {},
            {},
            QueryProvider(yst, init_st),
            QCRIFactory(),
            yst.queries_state.first_top_ord_a,
            yst.queries_state.first_top_ord_b
    };
    //vのa2bをやった場合のord_a
    ctx.effected_prev_finished_top_ord_a = OrdCalc::get_erased_next_ord(OrdPos(init_a_pos[v]), n);
    ctx.a_only_a2b_effect.a_effect.set_del(deleted_init_all_ord_a);
    ctx.a_only_a2b_effect.diff_stack_a_cnt = -1;

    ctx.a_only_b2a_effect.a_effect.set_add(add_tar_all_ord_a);
    ctx.a_only_b2a_effect.diff_stack_a_cnt = 1;

    return ctx;
}

bool is_prev_tar_same(
        const YakinamashiState &yst,
        const QueryCommon &applied_query,
        const ConstructVScanCtx &ctx
) {
    if (!applied_query.is_b2a_type()) return false;
    return ctx.prev_tar_all_ord_b == AllOrdCalc::get_tar_all_ord_b_B2A(applied_query, yst);
}

//q_idxは、前に今考えてる挿入（a2bか、a2bとb2a）について、directでしかぶつかる事が無いかどうか
//ctxにいままでのeffect後の常態を入れていきながら呼ぶもの
//a2bを前に入れても、0_a2aがvalidなa2aになることはないので、a2bの時だけ使ってるなら正しい
bool is_only_direct(int q_idx, const QueryCommon &q, ConstructVScanCtx &ctx) {
    if (q.is_a2a_type()) {
        // command no-cmd A2A は境界を作らないが、effect 適用で command 有りになったら境界として扱う。
        if (q.get_flip_dis_dir() == 0) {
            return false;
        }
        return !ctx.was_ab_type && !ctx.was_valid_a2a;
    } else {
        my_assert(q.is_ab_type());
        return !ctx.was_ab_type;
    }
}

template<typename QueryIntContainer>
int calc_fixed_diff_a2a(
        int q_idx,
        int v,
        const YakinamashiState &yst,
        const QueryCommonRI &qri,
        ConstructVScanCtx &ctx,
        QueryIntContainer &indirect_a2b_ra_A2A,
        QueryIntContainer &indirect_a2b_rra_A2A,
        QueryIntContainer &a2b_b2a_effected_is_valid_query
) {
    const QueryCommon &query = qri.get_query();
    QueryCommon applied_query = ctx.provider.get_applied_common(
            ctx.base_prev_finished_top_ord_b/*dummyのようなもの*/, query, ctx.a_only_a2b_effect);
    a2b_b2a_effected_is_valid_query[q_idx] = applied_query.get_flip_dis_dir() != 0;
    //A/B 個数変化で command no-cmd A2A が command 有りになった場合は direct 境界として扱う。
    if (is_only_direct(q_idx, applied_query, ctx)) {
        int n = yst.queries_state.current_N;
        ctx.effected_prev_finished_top_ord_a = applied_query.get_finished_top_ord_a_a2a(
                ctx.effected_prev_finished_top_ord_a);
        if (applied_query.get_flip_dis_dir() != 0) {
            ctx.was_valid_a2a = true;
        }
        return 0;
    }
    QueryCommonRI applied_qri = ctx.cri_factory.build_a2a_ri_from_start_top(
            ctx.effected_prev_finished_top_ord_a, applied_query);
    auto [ra_sum, rra_sum] = applied_qri.get_can_sync_ra_rra_A2A();
    if (applied_query.get_flip_dis_dir() == 0) {
        ctx.effected_prev_finished_top_ord_a = applied_qri.get_finished_top_ord_a();
        //was_valid_a2aはもとがfalseならfalseのままなので何もしない
        return applied_qri.get_ins_turn() - qri.get_ins_turn();
    }
    //既存のabがないと、今見てるクエリがindirectになる事が無いから
    if (ctx.was_ab_type) {
        ctx.effected_prev_a2a_rot_sum.ra_sum += ra_sum;
        ctx.effected_prev_a2a_rot_sum.rra_sum += rra_sum;
    }
    ctx.effected_prev_finished_top_ord_a = applied_qri.get_finished_top_ord_a();
    my_assert(applied_query.get_flip_dis_dir() != 0);
    ctx.was_valid_a2a = true;
    indirect_a2b_ra_A2A[q_idx] = ra_sum;
    indirect_a2b_rra_A2A[q_idx] = rra_sum;
    return applied_qri.get_ins_turn() - qri.get_ins_turn();
}

template<typename QueryIntContainer>
int calc_fixed_a2b_b2a_diff_a2a(
        int q_idx,
        int v,
        const YakinamashiState &yst,
        const QueryCommonRI &qri,
        ConstructVScanCtx &ctx,
        QueryIntContainer &indirect_a2b_b2a_ra_A2A,
        QueryIntContainer &indirect_a2b_b2a_rra_A2A
) {
    const QueryCommon &query = qri.get_query();
    QueryCommon applied_query1 = ctx.provider.get_applied_common(
            ctx.base_prev_finished_top_ord_b, query, ctx.a_only_a2b_effect);
    QueryCommon applied_query2 = ctx.provider.get_applied_common(
            ctx.base_prev_finished_top_ord_b, applied_query1, ctx.a_only_b2a_effect);
    if (is_only_direct(q_idx, applied_query2, ctx)) {
        int n = yst.queries_state.current_N;
        ctx.effected_prev_finished_top_ord_a = applied_query2.get_finished_top_ord_a_a2a(
                ctx.effected_prev_finished_top_ord_a);
        if (applied_query2.get_flip_dis_dir() != 0) {
            ctx.was_valid_a2a = true;
        }
        return 0;
    }
    QueryCommonRI applied_qri = ctx.cri_factory.build_a2a_ri_from_start_top(
            ctx.effected_prev_finished_top_ord_a, applied_query2);
    auto [ra_sum, rra_sum] = applied_qri.get_can_sync_ra_rra_A2A();
    if (applied_query2.get_flip_dis_dir() == 0) {
        ctx.effected_prev_finished_top_ord_a = applied_qri.get_finished_top_ord_a();
        return applied_qri.get_ins_turn() - qri.get_ins_turn();
    }
    if (ctx.was_ab_type) {
        ctx.effected_prev_a2a_rot_sum.ra_sum += ra_sum;
        ctx.effected_prev_a2a_rot_sum.rra_sum += rra_sum;
    }
    indirect_a2b_b2a_ra_A2A[q_idx] = ra_sum;
    indirect_a2b_b2a_rra_A2A[q_idx] = rra_sum;
    ctx.effected_prev_finished_top_ord_a = applied_qri.get_finished_top_ord_a();
    my_assert(applied_query2.get_flip_dis_dir() != 0);
    ctx.was_valid_a2a = true;
    return applied_qri.get_ins_turn() - qri.get_ins_turn();
}

int calc_ab_diff_with_band(
        const QueryProvider &qprov,
        const YakinamashiState &yst,
        int q_idx,
        int n,
        const BOrder &border,
        const QueryCommonRI &qri,
        const QueryCommon &applied_query,
        OrdPos applied_tar_ord_pos_a,
        ConstructVScanCtx &ctx,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &start_q_b_segment,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &end_q_b_segment,
        QueryBandRangeArray &q_idx_band_range_info
) {
    const int stack_b_cnt_after_insert = qri.get_query().get_stack_b_cnt(n) + 1;
    auto b_range_infos = build_ab_b_ranges(
            qprov,
            yst, q_idx,
            applied_query, border, ctx.prev_tar_all_ord_b, ctx.base_prev_finished_top_ord_b, stack_b_cnt_after_insert,
            ctx
    );
    std::array<AppliedABInfo, 2> infos = {
            build_applied_ab_info(applied_query,
                                  b_range_infos[0],
                                  applied_tar_ord_pos_a,
                                  ctx.effected_prev_a2a_rot_sum,
                                  ctx.effected_prev_finished_top_ord_a,
                                  stack_b_cnt_after_insert, n),
            build_applied_ab_info(applied_query,
                                  b_range_infos[1],
                                  applied_tar_ord_pos_a,
                                  ctx.effected_prev_a2a_rot_sum,
                                  ctx.effected_prev_finished_top_ord_a,
                                  stack_b_cnt_after_insert, n)
    };
    int best_idx = (infos[1].bri.ins_turn < infos[0].bri.ins_turn) ? 1 : 0;
    if (infos[0].bri.ins_turn != infos[1].bri.ins_turn)
        register_band_segment(
                infos[best_idx ^ 1].valid_all_ord_range_b,
                q_idx,
                start_q_b_segment,
                end_q_b_segment,
                q_idx_band_range_info
        );
    ctx.effected_prev_finished_top_ord_a = infos[best_idx].finished_top_ord_a;
    my_assert(std::abs(infos[0].bri.ins_turn - infos[1].bri.ins_turn) <= 1);
    return (infos[best_idx].bri.ins_turn) - qri.get_ins_turn();
}

int calc_fixed_diff_same_tar_b2a(
        const QueryCommonRI &qri,
        const QueryCommon &applied_query,
        OrdPos start_ord_pos_a,
        ConstructVScanCtx &ctx
) {
    applied_query.validate_is_b2a_type();
    const int a_size = applied_query.get_stack_a_cnt();
    OrdPos tar_ord_pos_a = to_ord_pos(applied_query.get_tar_rel_ord_a_B2A(), a_size);
    int ra_dis = RotCalculator::get_ra_dis(start_ord_pos_a, tar_ord_pos_a, a_size);
    int rra_dis = RotCalculator::get_rra_dis(start_ord_pos_a, tar_ord_pos_a, a_size);
    ctx.effected_prev_finished_top_ord_a = OrdCalc::get_inserted_ord(applied_query.get_tar_rel_ord_a_B2A());
    return std::min(ra_dis, rra_dis) + 1 - qri.get_ins_turn();
}

int calc_fixed_a2b_b2a_diff_ab(
        const QueryProvider &qprov,
        int q_idx,
        int n,
        const YakinamashiState &yst,
        const QueryCommonRI &qri,
        ConstructVScanCtx &ctx,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &start_q_b_segment,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &end_q_b_segment
) {
    auto &query = qri.get_query();
    QueryCommon applied_query1 = ctx.provider.get_applied_common(
            ctx.base_prev_finished_top_ord_b, query, ctx.a_only_a2b_effect);
    QueryCommon applied_query2 = ctx.provider.get_applied_common(
            ctx.base_prev_finished_top_ord_b, applied_query1, ctx.a_only_b2a_effect);
    if (is_only_direct(q_idx, query, ctx)) {
        ctx.effected_prev_finished_top_ord_a = applied_query2.get_finished_top_ord_a(n);
        return 0;
    }
    OrdPos applied_tar_ord_pos_a = qri.is_a2b_type()
                                   ? applied_query2.get_tar_ord_pos_a_A2B()
                                   : to_ord_pos(applied_query2.get_tar_rel_ord_a_B2A(),
                                                applied_query2.get_stack_a_cnt());
    const int stack_b_cnt = applied_query2.get_stack_b_cnt(n);
    const OrdPos start_ord_b = ctx.base_prev_finished_top_ord_b;
    const OrdPos tar_ord_pos_b = qri.is_a2b_type()
                                 ? to_ord_pos(applied_query2.get_tar_rel_ord_b_A2B(), stack_b_cnt)
                                 : applied_query2.get_tar_ord_pos_b_B2A();
    BestRotInfo bri = build_brp_by_ord_with_prev_a2a(
            applied_query2.get_stack_a_cnt(),
            stack_b_cnt,
            ctx.effected_prev_finished_top_ord_a,
            start_ord_b,
            applied_tar_ord_pos_a,
            tar_ord_pos_b,
            ctx.effected_prev_a2a_rot_sum
    );
    ctx.effected_prev_finished_top_ord_a = applied_query2.get_finished_top_ord_a(n);
    return bri.ins_turn - qri.get_ins_turn();
}

//ctx.finished_top_ord_aだけこの中でupdateされる
int calc_fixed_diff_ab(
        const QueryProvider &qprov,
        int q_idx,
        int n,
        const YakinamashiState &yst,
        const QueryCommonRI &qri,
        ConstructVScanCtx &ctx,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &start_q_b_segment,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &end_q_b_segment,
        QueryBandRangeArray &q_idx_band_range_info
) {
    const QueryCommon &query = qri.get_query();
    QueryCommon applied_query = ctx.provider.get_applied_common(
            ctx.base_prev_finished_top_ord_b, query, ctx.a_only_a2b_effect);
    if (is_only_direct(q_idx, query, ctx)) {
        ctx.effected_prev_finished_top_ord_a = applied_query.get_finished_top_ord_a(n);
        return 0;
    }
    int a_cnt = applied_query.get_stack_a_cnt();
    OrdPos applied_tar_ord_pos_a = qri.is_a2b_type()
                                   ? applied_query.get_tar_ord_pos_a_A2B()
                                   : to_ord_pos(applied_query.get_tar_rel_ord_a_B2A(), a_cnt);
    if (is_prev_tar_same(yst, applied_query, ctx)) {
        return calc_fixed_diff_same_tar_b2a(
                qri, applied_query, ctx.effected_prev_finished_top_ord_a, ctx);
    }
    return calc_ab_diff_with_band(qprov,
                                  yst,
                                  q_idx, n, yst.query_order.border, qri, applied_query, applied_tar_ord_pos_a,
                                  ctx, start_q_b_segment, end_q_b_segment, q_idx_band_range_info);
}

//  AB 後の replay 状態更新を helper に分け、固定差分計算と後処理を分離する。
void update_scan_ctx_after_ab(
        int n,
        const YakinamashiState &yst,
        const QueryCommonRI &qri,
        ConstructVScanCtx &ctx,
        int q_idx
) {
    const QueryCommon &query = qri.get_query();
    ctx.was_ab_type = true;
    ctx.prev_ab_type = qri.get_push_type();
    ctx.prev_ab_idx = q_idx;
    ctx.effected_prev_a2a_rot_sum = PrevA2ARotSum(0, 0);
    ctx.prev_tar_all_ord_b = qri.is_a2b_type()
                             ? AllOrdCalc::get_tar_all_ord_b_A2B(query, yst)
                             : AllOrdCalc::get_tar_all_ord_b_B2A(query, yst);
    ctx.base_prev_finished_top_ord_b = query.get_finished_top_ord_b(n);
}

// 責務: qcnt 個の int を同じ初期値で埋めた QueryIntArray を作る。
// 必要な理由: vector<int>(qcnt, value) の未代入要素の意味を保ったまま heap allocation をなくすため。
QueryIntArray make_query_int_array(int qcnt, int value = 0) {
    QueryIntArray result;
    result.clear();
    for (int i = 0; i < qcnt; i++) {
        result.push_back(value);
    }
    return result;
}

// 責務: 指定個数の int を同じ初期値で埋めた QueryBoundaryArray を作る。
// 必要な理由: vector<int>(qcnt + 1, value) の未代入要素の意味を保ったまま heap allocation をなくすため。
QueryBoundaryArray make_query_boundary_array(int size, int value = 0) {
    QueryBoundaryArray result;
    result.clear();
    for (int i = 0; i < size; i++) {
        result.push_back(value);
    }
    return result;
}

// 責務: qcnt 個の BandRangeInfo 初期値を vec_array に詰める。
// 必要な理由: 大きい vec_array の返り値代入を避けつつ、a2b_band_ranges を置換するため。
void make_query_band_range_array(QueryBandRangeArray &result, int qcnt) {
    result.clear();
    for (int i = 0; i < qcnt; i++) result.push_back(BandRangeInfo());
}

// 責務: qcnt 個の QueryCommon 初期値を vec_array に詰める。
// 必要な理由: 大きい vec_array の返り値代入を避けつつ、effected_a2b_b2a_queries を置換するため。
void make_query_common_array(QueryCommonArray &result, int qcnt) {
    result.clear();
    for (int i = 0; i < qcnt; i++) result.push_back(QueryCommon());
}

// 責務: qcnt 個の OrdPos(0) 初期値を vec_array に詰める。
// 必要な理由: 大きい vec_array の返り値代入を避けつつ、a2b_effected_finished_top_ord_a を置換するため。
void make_query_ord_pos_array(QueryOrdPosArray &result, int qcnt) {
    result.clear();
    for (int i = 0; i < qcnt; i++) result.push_back(OrdPos(0));
}


//a2b, b2aを前に挿入された結果のindirectの差分前計算
template<typename ExistAList, typename ExistBList>
tuple<QueryIntArray, QueryIntArray> build_fixed_a2b_b2a_indirect_diffs(
        const vector<int> &init_a_pos,
        int v,
        int n,
        const State &init_st,
        const YakinamashiState &yst,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &start_q_b_segment,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &end_q_b_segment,
        QueryIntArray &indirect_a2b_b2a_ra_A2A,
        QueryIntArray &indirect_a2b_b2a_rra_A2A,
        QueryCommonArray &effected_a2b_b2a_queries,
        OrdPos &indirect_final_top_ord_a,
        const ExistAList &exist_a_ords_before_q_idx,
        const ExistBList &exist_b_ords_before_q_idx
) {
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const int insert_event_size = yst.query_order.border.get_all_order_entries_size() + 2;
    for (int all_ord_b = 0; all_ord_b < insert_event_size; all_ord_b++) {
        start_q_b_segment[all_ord_b].clear();
        end_q_b_segment[all_ord_b].clear();
    }
    ConstructVScanCtx ctx = init_scan_ctx(
            init_a_pos,
            v,
            yst, init_st,
            yst.query_order.aorder.get_init_all_order(v),
            yst.query_order.aorder.get_target_order(v),
            n);
    QueryIntArray is_only_direct_ = make_query_int_array(qcnt);
    QueryIntArray fixed_a2b_b2a_indirect_range_diff = make_query_int_array(qcnt, 0);
    make_query_common_array(effected_a2b_b2a_queries, qcnt);
    QueryProvider qprov(yst, init_st);
    for (int q_idx = 0; q_idx < qcnt; q_idx++) {
        const QueryCommonRI &qri = yst.queries_state.queries[q_idx];
        is_only_direct_[q_idx] = is_only_direct(q_idx, qri.get_query(), ctx);
        if (qri.is_a2a_type()) {
            fixed_a2b_b2a_indirect_range_diff[q_idx] = calc_fixed_a2b_b2a_diff_a2a(q_idx, qri.get_tar_v(), yst, qri,
                                                                                   ctx, indirect_a2b_b2a_ra_A2A,
                                                                                   indirect_a2b_b2a_rra_A2A);
        } else {
            fixed_a2b_b2a_indirect_range_diff[q_idx] =
                    calc_fixed_a2b_b2a_diff_ab(qprov, q_idx, n, yst, qri, ctx, start_q_b_segment, end_q_b_segment);
        }
        QueryCommon applied_query1 = ctx.provider.get_applied_common(
                ctx.base_prev_finished_top_ord_b, qri.get_query(), ctx.a_only_a2b_effect);
        effected_a2b_b2a_queries[q_idx] = ctx.provider.get_applied_common(
                ctx.base_prev_finished_top_ord_b, applied_query1, ctx.a_only_b2a_effect);
        if (qri.is_ab_type()) update_scan_ctx_after_ab(n, yst, qri, ctx, q_idx);
    }
    indirect_final_top_ord_a = ctx.effected_prev_finished_top_ord_a;
    return {is_only_direct_, fixed_a2b_b2a_indirect_range_diff};
}


//<is_only_direct, , >
//is_only_directを返すのはテスト用だった気がする（のちに消すかも）
template<typename ExistAList, typename ExistBList>
tuple<QueryIntArray, QueryIntArray> build_fixed_a2b_indirect_diffs(
        const vector<int> &init_a_pos,
        int v,
        int n,
        const State &init_st,
        const YakinamashiState &yst,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &start_q_b_segment,
        std::array<vector<int>, MAX_INSERT_EVENT_B> &end_q_b_segment,
        QueryBandRangeArray &q_idx_band_range_info,
        QueryIntArray &indirect_a2b_ra_A2A,
        QueryIntArray &indirect_a2b_rra_A2A,
        const ExistAList &exist_a_ords_before_q_idx,
        const ExistBList &exist_b_ords_before_q_idx,
        QueryBoundaryArray &prev_a2b_effected_valid_q_idx,
        QueryIntArray &next_a2b_effected_valid_q_idx,
        QueryOrdPosArray &a2b_effected_finished_top_ord_a
) {
    my_assert(indirect_a2b_ra_A2A.size() == yst.queries_state.queries.size());
    my_assert(indirect_a2b_rra_A2A.size() == yst.queries_state.queries.size());
    int qcnt = static_cast<int>(yst.queries_state.queries.size());
    make_query_ord_pos_array(a2b_effected_finished_top_ord_a, qcnt);
    ConstructVScanCtx ctx = init_scan_ctx(
            init_a_pos,
            v,
            yst, init_st,
            yst.query_order.aorder.get_init_all_order(v),
            yst.query_order.aorder.get_target_order(v),
            n);
    QueryIntArray fixed_a2b_indirect_range_diff = make_query_int_array(qcnt, 0);
    QueryIntArray fixed_a2b_b2a_indirect_range_diff = make_query_int_array(qcnt, 0);

    QueryIntArray is_only_direct_ = make_query_int_array(qcnt);
    QueryProvider qprov(yst, init_st);
    prev_a2b_effected_valid_q_idx = make_query_boundary_array(qcnt + 1, -1);
    next_a2b_effected_valid_q_idx = make_query_int_array(qcnt);
    QueryIntArray a2b_b2a_effected_is_valid_query = make_query_int_array(qcnt);
    int prev_valid_idx = -1;
    for (int q_idx = 0; q_idx < qcnt; q_idx++) {
        const QueryCommonRI &qri = yst.queries_state.queries[q_idx];
        is_only_direct_[q_idx] = is_only_direct(q_idx, qri.get_query(), ctx);
        //prev_finished_top_ord_aはこの中でupdateする
        if (qri.is_a2a_type()) {
            fixed_a2b_indirect_range_diff[q_idx] = calc_fixed_diff_a2a(q_idx, qri.get_tar_v(), yst, qri, ctx,
                                                                       indirect_a2b_ra_A2A, indirect_a2b_rra_A2A, a2b_b2a_effected_is_valid_query);
        } else {
            fixed_a2b_indirect_range_diff[q_idx] =
                    calc_fixed_diff_ab(
                            qprov,
                            q_idx, n, yst, qri, ctx,
                            start_q_b_segment, end_q_b_segment, q_idx_band_range_info
                    );
            a2b_b2a_effected_is_valid_query[q_idx] = true;
        }
        a2b_effected_finished_top_ord_a[q_idx] = ctx.effected_prev_finished_top_ord_a;
        if (qri.is_ab_type()) update_scan_ctx_after_ab(n, yst, qri, ctx, q_idx);
        prev_a2b_effected_valid_q_idx[q_idx] = prev_valid_idx;
        if(a2b_b2a_effected_is_valid_query[q_idx]){
            prev_valid_idx = q_idx;
        }
    }
    prev_a2b_effected_valid_q_idx[qcnt] = prev_valid_idx;
    int next_valid_idx = qcnt;
    for (int q_idx = qcnt-1; q_idx >= 0; q_idx--){
        if(a2b_b2a_effected_is_valid_query[q_idx]){
            next_valid_idx = q_idx;
        }
        next_a2b_effected_valid_q_idx[q_idx] = next_valid_idx;
    }
    return {is_only_direct_, fixed_a2b_indirect_range_diff};
}

vector<int> build_prefix_sum(
        const vector<int> &fixed_indirect_range_diff
) {
    vector<int> prefix_sum(fixed_indirect_range_diff.size() + 1, 0);
    for (int q_idx = 0; q_idx < static_cast<int>(fixed_indirect_range_diff.size()); q_idx++)
        prefix_sum[q_idx + 1] = prefix_sum[q_idx] + fixed_indirect_range_diff[q_idx];
    return prefix_sum;
}

// 責務: q_idx diff 配列から qcnt+1 個の prefix sum を QueryPrefixArray として返す。
// 必要な理由: InsertPairPrecalc の prefix sum 系 vector 確保をなくしつつ、prefix_sum[0]=0 の意味を維持するため。
template<typename QueryIntContainer>
QueryPrefixArray build_query_prefix_sum(
        const QueryIntContainer &fixed_indirect_range_diff
) {
    QueryPrefixArray prefix_sum;
    prefix_sum.clear();
    prefix_sum.push_back(0);
    for (int q_idx = 0; q_idx < static_cast<int>(fixed_indirect_range_diff.size()); q_idx++) {
        prefix_sum.push_back(prefix_sum[q_idx] + fixed_indirect_range_diff[q_idx]);
    }
    return prefix_sum;
}

//  DEBUG 時の band 件数検証を helper に分け、評価ループ本体を短くする。
void assert_tmp_indirect_sum(
        const QueryPrefixArray &fixed_indirect_prefix_sum,
        const IndirectTask &task,
        const my_bitset<MAX_QUERY_CNT> &active_band_q_idxs,
        int tmp_indirect_sum
) {
#ifdef DEBUG
    int naive_band_count = 0;
    for (int q_idx = task.a2b_idx; q_idx < task.b2a_idx; q_idx++)
        if (active_band_q_idxs[q_idx]) naive_band_count++;
    my_assert(count_active_band_q_idxs_in_range(active_band_q_idxs, task.a2b_idx, task.b2a_idx) == naive_band_count);
    my_assert(tmp_indirect_sum ==
              get_fixed_indirect_range_sum(fixed_indirect_prefix_sum, task.a2b_idx, task.b2a_idx) + naive_band_count);
#else
    (void) fixed_indirect_prefix_sum;
    (void) task;
    (void) active_band_q_idxs;
    (void) tmp_indirect_sum;
#endif
}

//  indirect task 評価を helper 化し、construct_v 本体では前計算結果の配線だけを見る。
void evaluate_indirect_tasks(
        const BOrder &border,
        const QueryPrefixArray &fixed_indirect_prefix_sum,
        const std::array<vector<int>, MAX_INSERT_EVENT_B> &start_q_b_segment,
        const std::array<vector<int>, MAX_INSERT_EVENT_B> &end_q_b_segment,
        const std::array<vector<IndirectTask>, MAX_INSERT_EVENT_B> &indirect_tasks_by_b_all_ord
) {
    my_bitset<MAX_QUERY_CNT> active_band_q_idxs;
    active_band_q_idxs.reset();
    for (int all_ord_b = 0; all_ord_b <= border.get_all_order_entries_size() + 1; all_ord_b++) {
        update_active_band_q_idxs(all_ord_b, start_q_b_segment, end_q_b_segment, active_band_q_idxs);
        for (const IndirectTask &task: indirect_tasks_by_b_all_ord[all_ord_b]) {
            int tmp_indirect_sum = get_tmp_indirect_sum(
                    fixed_indirect_prefix_sum, task.a2b_idx, task.b2a_idx, active_band_q_idxs);
            assert_tmp_indirect_sum(fixed_indirect_prefix_sum, task, active_band_q_idxs, tmp_indirect_sum);
            (void) tmp_indirect_sum;
        }
    }
}

static vector<int> build_init_a_pos(const State &init_st) {
    vector<int> init_a_pos(init_st.initial_N);
    for (int i = 0; i < init_st.A.size(); i++) {
        init_a_pos[init_st.A[i]] = i;
    }
    return init_a_pos;
}

int calc_prefix_sum(const vector<int> &prefix_sum, int begin, int end) {
    my_assert(0 <= begin && begin <= end && end < prefix_sum.size());
    return prefix_sum[end] - prefix_sum[begin];
}

// 責務: 固定容量化した prefix sum から半開区間 [begin, end) の合計を返す。
// 必要な理由: InsertPairPrecalc の prefix sum 系を QueryPrefixArray に置換しても既存計算式を維持するため。
int calc_prefix_sum(const QueryPrefixArray &prefix_sum, int begin, int end) {
    my_assert(0 <= begin && begin <= end && end < static_cast<int>(prefix_sum.size()));
    return prefix_sum[end] - prefix_sum[begin];
}

template<typename ExistAList, typename ExistBList>
int build_a2b_b2a_direct_final_rot(int v, const YakinamashiState &yst,
                                   const ExistAList &exist_all_ord_as,
                                   const ExistBList &exist_all_ord_bs) {
    const AOrder &aorder = yst.query_order.aorder;
    AllOrd a2b_all_ord_a = aorder.get_init_all_order(v);
    AllOrd b2a_all_ord_a = aorder.get_target_order(v);
    int qcnt = yst.queries_state.queries.size();
    int b2a_ord_pos_a_v = pop_cnt(exist_all_ord_as[qcnt], b2a_all_ord_a.get());
    if (a2b_all_ord_a.get() < b2a_all_ord_a.get()) {
        b2a_ord_pos_a_v--;
    }
    auto [cmd, cnt] = FinalRotCalculator::build_final_rot(yst.queries_state.current_N, OrdPos(b2a_ord_pos_a_v));
    return cnt - yst.queries_state.final_rot_info.second;
}

//int get_prev_valid_q_idx(const YakinamashiState &yst, int q_idx);

//int get_first_valid_q_idx(const YakinamashiState &yst, int q_idx);

//// : QueryCommon だけから A 側 target AllOrd を決めるための overload。
//AllOrd get_tar_all_ord_a(const YakinamashiState &yst, const QueryCommon &query) {
//    const AOrder &aorder = yst.query_order.aorder;
//    if (query.is_a2b_type()) {
//        return aorder.get_init_order(query.get_tar_v(), yst.is_lis);
//    }
//    if (query.is_b2a_type()) {
//        return aorder.get_target_order(query.get_tar_v());
//    }
//    my_assert(query.is_a2a_type());
//    return aorder.get_target_order(query.get_tar_v());
//}

// : final rot 用に QueryCommon 単体から終了 top を取得する。
OrdPos get_finished_top_ord_a(const QueryCommon &query, int n) {
    if (query.is_ab_type()) {
        return query.get_finished_top_ord_a(n);
    }
    my_assert(query.is_a2a_type());
    my_assert(query.get_flip_dis_dir() != 0);
    return query.get_finished_top_ord_a_a2a(OrdPos(0));
}

// : net effect 後に scan した最終 top を使って final rot 差分を見積もる。
template<typename ExistAList, typename ExistBList>
int build_a2b_b2a_indirect_final_rot(int v, const YakinamashiState &yst,
                                     const ExistAList &exist_all_ord_as,
                                     const ExistBList &exist_all_ord_bs,
                                     const InsertPairPrecalc &precalc) {
    (void) v;
    (void) exist_all_ord_as;
    (void) exist_all_ord_bs;
    int current_N = yst.queries_state.current_N;
    auto [cmd, cnt] = FinalRotCalculator::build_final_rot(current_N, precalc.indirect_final_top_ord_a);
    return cnt - yst.queries_state.final_rot_info.second;
}

AllOrd get_tar_all_ord_a(const YakinamashiState &yst, const QueryCommonRI &qri) {
    const AOrder &aorder = yst.query_order.aorder;
    if (qri.is_a2b_type()) {
        return aorder.get_init_order(qri.get_tar_v());
    } else if (qri.is_b2a_type()) {
        return aorder.get_target_order(qri.get_tar_v());
    } else {
        my_assert(qri.is_a2a_type());
        return aorder.get_target_order(qri.get_tar_v());
    }
}

AllOrd get_tar_all_ord_b(const YakinamashiState &yst, const QueryCommonRI &qri) {
    const BOrder &border = yst.query_order.border;
    return border.get_target_order(qri.get_tar_v());
}

// 責務: A2A command が発生しない query かどうかを返す。
// 必要な理由: flip_dis_dir==0 でも境界ケースでは OrdPos が変わり得るため、no-op と呼ぶと誤読されるため。
bool is_no_cmd_a2a_query(const QueryCommon &query) {
    return query.is_a2a_type() && query.get_flip_dis_dir() == 0;
}

// : a2b+b2a net effect 後に有効な query 境界を一度だけ前計算する。
//res[q_idx] := q_idx以降で最初の有効なクエリのidx
//次が無い場合、queries.size()を持つ
template<typename QueryContainer>
QueryBoundaryArray build_next_a2b_b2a_effected_valid_q_idx(const QueryContainer &effected_queries) {
    const int qcnt = static_cast<int>(effected_queries.size());
    QueryBoundaryArray next_valid = make_query_boundary_array(qcnt + 1, qcnt);
    int next_idx = qcnt;
    for (int q_idx = qcnt - 1; q_idx >= 0; q_idx--) {
        if (!is_no_cmd_a2a_query(effected_queries[q_idx])) {
            next_idx = q_idx;
        }
        next_valid[q_idx] = next_idx;
    }
    return next_valid;
}

// : final rot や直前境界で使うため、net effect 後の直前有効 query を保持する。
template<typename QueryContainer>
QueryBoundaryArray build_prev_a2b_b2a_effected_valid_q_idx(const QueryContainer &effected_queries) {
    const int qcnt = static_cast<int>(effected_queries.size());
    QueryBoundaryArray prev_valid = make_query_boundary_array(qcnt + 1, -1);
    int prev_idx = -1;
    for (int q_idx = 0; q_idx < qcnt; q_idx++) {
        prev_valid[q_idx] = prev_idx;
        if (!is_no_cmd_a2a_query(effected_queries[q_idx])) {
            prev_idx = q_idx;
        }
    }
    prev_valid[qcnt] = prev_idx;
    return prev_valid;
}

// 責務: base A2A の ra/rra 差分 prefix sum を QueryPrefixArray の pair として作る。
// 必要な理由: InsertPairPrecalc に保持する prefix sum の container を QueryPrefixArray に揃えるため。
std::pair<QueryPrefixArray, QueryPrefixArray> build_debug_ins2_base_a2a_prefix_sums(const QueriesState &queries_state) {
    const int qcnt = static_cast<int>(queries_state.queries.size());
    QueryIntArray base_ra_A2A = make_query_int_array(qcnt);
    QueryIntArray base_rra_A2A = make_query_int_array(qcnt);
    for (int q_idx = 0; q_idx < qcnt; q_idx++) {
        if (!queries_state.queries[q_idx].is_a2a_type()) {
            continue;
        }
        auto [ra_cnt, rra_cnt] = queries_state.queries[q_idx].get_can_sync_ra_rra_A2A();
        base_ra_A2A[q_idx] = ra_cnt;
        base_rra_A2A[q_idx] = rra_cnt;
    }
    return {build_query_prefix_sum(base_ra_A2A), build_query_prefix_sum(base_rra_A2A)};
}

// 責務: band start/end event を all_ord 昇順に走査し、各 all_ord で有効な q_idx bitset を保存する。
// 必要な理由: owner range 更新時に古い a2b_idx の band_count も O(popcount) で取得できるようにするため。
void build_band_bits(
        InsertPairPrecalc &ctx,
        int insert_slot_count
) {
    my_assert(0 <= insert_slot_count && insert_slot_count < MAX_INSERT_EVENT_B);
    my_bitset<MAX_QUERY_CNT> active;
    active.reset();
    for (int all_ord = 0; all_ord < MAX_INSERT_EVENT_B; all_ord++) {
        ctx.band_bits[all_ord].reset();
    }
    for (int all_ord = 0; all_ord < insert_slot_count; all_ord++) {
        for (int q_idx: ctx.b_band_end_qidxs[all_ord]) {
            my_assert(active[q_idx] == 1);
            active.reset(q_idx);
        }
        for (int q_idx: ctx.b_band_start_qidxs[all_ord]) {
            my_assert(active[q_idx] == 0);
            active.set(q_idx);
        }
        ctx.band_bits[all_ord] = active;
    }
}

// 責務: construct v 用の前計算 workspace を初期化して参照で返す。
// 必要な理由: InsertPairPrecalc に大きな vec_array メンバを持たせても stack 使用量を増やさないため。
InsertPairPrecalc &build_construct_v_precalc(const State &init_st, const YakinamashiState &yst, int v) {
    const int n = yst.queries_state.current_N;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const auto &border = yst.query_order.border;
    my_assert(MAX_QUERY_CNT >= qcnt);
    my_assert(MAX_INSERT_EVENT_B >= border.get_all_order_entries_size() + 2);

    static InsertPairPrecalc ctx;
    ctx.clear();
    make_query_band_range_array(ctx.a2b_band_ranges, qcnt);
    ctx.indirect_a2b_ra_A2A = make_query_int_array(qcnt);
    ctx.indirect_a2b_rra_A2A = make_query_int_array(qcnt);
    ctx.indirect_a2b_b2a_ra_A2A = make_query_int_array(qcnt);
    ctx.indirect_a2b_b2a_rra_A2A = make_query_int_array(qcnt);
    build_exist_all_ords_before_q_idx(yst, init_st, ctx.exist_stack_a_ords, ctx.exist_stack_b_ords);

    vector<int> init_a_pos = build_init_a_pos(init_st);
    auto [a2b_effected_is_only_direct, fixed_indirect_a2b_range_diff] =
            build_fixed_a2b_indirect_diffs(
                    init_a_pos,
                    v, n, init_st, yst, ctx.b_band_start_qidxs, ctx.b_band_end_qidxs, ctx.a2b_band_ranges,
                    ctx.indirect_a2b_ra_A2A, ctx.indirect_a2b_rra_A2A,
                    ctx.exist_stack_a_ords, ctx.exist_stack_b_ords,
                    ctx.prev_a2b_effected_valid_q_idx, ctx.next_a2b_effected_valid_q_idx,
                    ctx.a2b_effected_finished_top_ord_a
            );
    (void) a2b_effected_is_only_direct;
    build_band_bits(ctx, border.get_all_order_entries_size() + 1);
    // _REVIEW_BEGIN: A2B後用の band event を calc_best_plan まで保持するため、後続前計算の clear 対象を分ける。
    std::array<vector<int>, MAX_INSERT_EVENT_B> a2b_b2a_start_q_b_segment;
    std::array<vector<int>, MAX_INSERT_EVENT_B> a2b_b2a_end_q_b_segment;
    auto [a2b_b2a_effected_is_only_direct, fixed_indirect_a2b_b2a_range_diff] =
            build_fixed_a2b_b2a_indirect_diffs(
                    init_a_pos,
                    v, n, init_st, yst, a2b_b2a_start_q_b_segment, a2b_b2a_end_q_b_segment,
                    ctx.indirect_a2b_b2a_ra_A2A, ctx.indirect_a2b_b2a_rra_A2A,
                    ctx.effected_a2b_b2a_queries,
                    ctx.indirect_final_top_ord_a,
                    ctx.exist_stack_a_ords, ctx.exist_stack_b_ords
            );
    // _REVIEW_END
    (void) a2b_b2a_effected_is_only_direct;
    ctx.next_a2b_b2a_effected_valid_q_idx =
            build_next_a2b_b2a_effected_valid_q_idx(ctx.effected_a2b_b2a_queries);
    ctx.prev_a2b_b2a_effected_valid_q_idx =
            build_prev_a2b_b2a_effected_valid_q_idx(ctx.effected_a2b_b2a_queries);
    ctx.fixed_indirect_a2b_prefix_sum = build_query_prefix_sum(fixed_indirect_a2b_range_diff);
    ctx.fixed_indirect_a2b_b2a_prefix_sum = build_query_prefix_sum(fixed_indirect_a2b_b2a_range_diff);
    const auto &exist_a_ords_before_q_idx = ctx.exist_stack_a_ords;
    const auto &exist_b_ords_before_q_idx = ctx.exist_stack_b_ords;
    std::tie(ctx.base_ra_prefix_sum_A2A, ctx.base_rra_prefix_sum_A2A) =
            build_debug_ins2_base_a2a_prefix_sums(yst.queries_state);
    ctx.indirect_a2b_ra_prefix_sum_A2A = build_query_prefix_sum(ctx.indirect_a2b_ra_A2A);
    ctx.indirect_a2b_rra_prefix_sum_A2A = build_query_prefix_sum(ctx.indirect_a2b_rra_A2A);
    ctx.indirect_a2b_b2a_ra_prefix_sum_A2A = build_query_prefix_sum(ctx.indirect_a2b_b2a_ra_A2A);
    ctx.indirect_a2b_b2a_rra_prefix_sum_A2A = build_query_prefix_sum(ctx.indirect_a2b_b2a_rra_A2A);
    ctx.next_ab_idxs = build_next_ab_idxs(yst);
    ctx.prev_a2a_ranges = build_prev_a2a_ranges(yst);
    ctx.direct_final_rot_diff = build_a2b_b2a_direct_final_rot(
            v, yst, exist_a_ords_before_q_idx, exist_b_ords_before_q_idx);
    ctx.indirect_final_rot_diff = build_a2b_b2a_indirect_final_rot(
            v, yst, exist_a_ords_before_q_idx, exist_b_ords_before_q_idx, ctx);
    return ctx;
}

struct DebugIns2Case {
    int a2b_idx = -1;
    int b2a_idx = -1;
    int v_all_ord_b = -1;
};

struct DebugIns2NaiveResult {
    YakinamashiState yst;
    int shifted_b2a_idx = -1;
    int total_diff = 0;
    int final_rot_diff = 0;
};

DebugIns2NaiveResult build_debug_ins2_naive_result(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const DebugIns2Case &case_info
) {
    YakinamashiState eval_yst = build_eval_state(yst, v, AllOrd(case_info.v_all_ord_b));
    insert_eval_a2b(eval_yst, init_st, v, case_info.a2b_idx);
    DebugIns2NaiveResult result{eval_yst};
    result.shifted_b2a_idx = case_info.b2a_idx + 1;
    insert_eval_b2a(result.yst, init_st, v, result.shifted_b2a_idx);
    result.total_diff = result.yst.queries_state.sum_turn - yst.queries_state.sum_turn;
    result.final_rot_diff =
            result.yst.queries_state.final_rot_info.second - yst.queries_state.final_rot_info.second;
    return result;
}

void assert_debug_ins2_total_diff(
        int v,
        const DebugIns2Case &case_info,
        int estimated_diff,
        int naive_diff
) {
    if (estimated_diff == naive_diff) return;
    out("debug_ins2_total_mismatch",
        "v", v,
        "a2b_idx", case_info.a2b_idx,
        "b2a_idx", case_info.b2a_idx,
        "v_all_ord_b", case_info.v_all_ord_b,
        "estimated_diff", estimated_diff,
        "naive_diff", naive_diff);
    my_assert(false);
}

void assert_debug_ins2_final_rot_diff(
        int v,
        const DebugIns2Case &case_info,
        int qcnt,
        bool use_direct_final_rot,
        int direct_final_rot_diff,
        int indirect_final_rot_diff,
        int naive_diff,
        const InsertPairPrecalc &precalc,
        const QRIList &queries
) {
    int estimated_diff = indirect_final_rot_diff;
    if (use_direct_final_rot) estimated_diff = direct_final_rot_diff;
    if (estimated_diff == naive_diff) return;
    // _BEGIN: command no-cmd A2A 追加後に direct/indirect どちらの final_rot 推定がずれたか切り分ける。
    int next_effected_valid_idx = precalc.next_a2b_b2a_effected_valid_q_idx[case_info.b2a_idx];
    int last_effected_valid_idx = precalc.prev_a2b_b2a_effected_valid_q_idx[qcnt];
    std::string base_b2a_q = "end";
    std::string effected_b2a_q = "end";
    if (case_info.b2a_idx < qcnt) {
        base_b2a_q = indirect_debug_to_string(queries[case_info.b2a_idx]);
        effected_b2a_q = indirect_debug_to_string(precalc.effected_a2b_b2a_queries[case_info.b2a_idx]);
    }
    std::string next_effected_q = "end";
    if (next_effected_valid_idx < qcnt) {
        next_effected_q = indirect_debug_to_string(precalc.effected_a2b_b2a_queries[next_effected_valid_idx]);
    }
    std::string last_effected_q = "none";
    if (last_effected_valid_idx >= 0) {
        last_effected_q = indirect_debug_to_string(precalc.effected_a2b_b2a_queries[last_effected_valid_idx]);
    }
    out("debug_ins2_final_rot_mismatch",
        "v", v,
        "a2b_idx", case_info.a2b_idx,
        "b2a_idx", case_info.b2a_idx,
        "qcnt", qcnt,
        "v_all_ord_b", case_info.v_all_ord_b,
        "next_effected_valid_idx", next_effected_valid_idx,
        "last_effected_valid_idx", last_effected_valid_idx,
        "direct_final_rot_diff", direct_final_rot_diff,
        "indirect_final_rot_diff", indirect_final_rot_diff,
        "estimated_diff", estimated_diff,
        "naive_diff", naive_diff,
        "base_b2a_q", base_b2a_q,
        "effected_b2a_q", effected_b2a_q,
        "next_effected_q", next_effected_q,
        "last_effected_q", last_effected_q);
    // _END
    my_assert(false);
}

struct DebugIns2A2BPlan {
    AllOrd tar_all_ord_a = AllOrd(0);
    AllOrd tar_all_ord_b = AllOrd(0);
    RelOrd tar_rel_ord_a = RelOrd(0);
    RelOrd tar_rel_ord_b = RelOrd(0);
    int stack_a_cnt = 0;
    int stack_b_cnt = 0;
    BestRotInfo bri = {NO_COMMAND, NO_COMMAND, 0, 0, 0};
    OrdPos finished_ord_pos_a = OrdPos(0);
    OrdPos finished_ord_pos_b = OrdPos(0);
};

const QueryCommonRI &get_debug_ins2_naive_a2b_qri(
        const DebugIns2NaiveResult &naive,
        const DebugIns2Case &case_info
) {
    my_assert(0 <= case_info.a2b_idx);
    my_assert(case_info.a2b_idx < static_cast<int>(naive.yst.queries_state.queries.size()));
    const QueryCommonRI &qri = naive.yst.queries_state.queries[case_info.a2b_idx];
    qri.validate_is_a2b_type();
    return qri;
}

const QueryCommonRI &get_debug_ins2_naive_b2a_qri(
        const DebugIns2NaiveResult &naive
) {
    my_assert(0 <= naive.shifted_b2a_idx);
    my_assert(naive.shifted_b2a_idx < static_cast<int>(naive.yst.queries_state.queries.size()));
    const QueryCommonRI &qri = naive.yst.queries_state.queries[naive.shifted_b2a_idx];
    qri.validate_is_b2a_type();
    return qri;
}

void assert_debug_ins2_a2b_insert_turn(
        int v,
        const DebugIns2Case &case_info,
        const DebugIns2A2BPlan &a2b_plan,
        const DebugIns2NaiveResult &naive
) {
    const QueryCommonRI &naive_qri = get_debug_ins2_naive_a2b_qri(naive, case_info);
    if (a2b_plan.bri.ins_turn == naive_qri.get_ins_turn()) return;
    out("debug_ins2_a2b_insert_mismatch",
        "v", v,
        "a2b_idx", case_info.a2b_idx,
        "b2a_idx", case_info.b2a_idx,
        "v_all_ord_b", case_info.v_all_ord_b,
        "estimated_turn", a2b_plan.bri.ins_turn,
        "naive_turn", naive_qri.get_ins_turn(),
        "naive_qri", indirect_debug_to_string(naive_qri));
    my_assert(false);
}

struct DebugIns2B2APlan {
    AllOrd tar_all_ord_a = AllOrd(0);
    AllOrd tar_all_ord_b = AllOrd(0);
    int stack_a_cnt = 0;
    int stack_b_cnt = 0;
    RelOrd tar_rel_ord_a = RelOrd(0);
    OrdPos tar_ord_pos_a = OrdPos(0);
    OrdPos tar_ord_pos_b = OrdPos(0);
    OrdPos start_ord_pos_a = OrdPos(0);
    OrdPos start_ord_pos_b = OrdPos(0);
    int a2a_start_idx = -1;
    PrevA2ARotSum prev_a2a_rot_sum = PrevA2ARotSum(0, 0);
    BestRotInfo bri = {NO_COMMAND, NO_COMMAND, 0, 0, 0};
    OrdPos finished_ord_pos_a = OrdPos(0);
    OrdPos finished_ord_pos_b = OrdPos(0);
};

void assert_debug_ins2_b2a_insert_turn(
        int v,
        const DebugIns2Case &case_info,
        const DebugIns2B2APlan &b2a_plan,
        const DebugIns2NaiveResult &naive,
        int N
) {
    const QueryCommonRI &naive_qri = get_debug_ins2_naive_b2a_qri(naive);
    if (b2a_plan.bri.ins_turn == naive_qri.get_ins_turn()) return;
    const QueryCommon &naive_query = naive_qri.get_query();
    int naive_prev_idx = naive.shifted_b2a_idx - 1;
    std::string naive_prev_qri = "none";
    OrdPos naive_start_ord_pos_a = naive.yst.queries_state.first_top_ord_a;
    OrdPos naive_start_ord_pos_b = naive.yst.queries_state.first_top_ord_b;
    if (naive_prev_idx >= 0) {
        naive_prev_qri = indirect_debug_to_string(naive.yst.queries_state.queries[naive_prev_idx]);
        naive_start_ord_pos_a = naive.yst.queries_state.queries[naive_prev_idx].get_finished_top_ord_a();
        naive_start_ord_pos_b = naive.yst.queries_state.queries[naive_prev_idx].get_query().get_finished_top_ord_b(N);
    }
    int est_ra_dis = RotCalculator::get_ra_dis(
            b2a_plan.start_ord_pos_a, b2a_plan.tar_ord_pos_a, b2a_plan.stack_a_cnt - 1);
    int est_rra_dis = RotCalculator::get_rra_dis(
            b2a_plan.start_ord_pos_a, b2a_plan.tar_ord_pos_a, b2a_plan.stack_a_cnt - 1);
    int est_rb_dis = RotCalculator::get_rb_dis(
            b2a_plan.start_ord_pos_b, b2a_plan.tar_ord_pos_b, b2a_plan.stack_b_cnt + 1);
    int est_rrb_dis = RotCalculator::get_rrb_dis(
            b2a_plan.start_ord_pos_b, b2a_plan.tar_ord_pos_b, b2a_plan.stack_b_cnt + 1);
    auto [est_sync_rb_dis, est_sync_rrb_dis] = PrevA2ARotSumCalc::get_prev_a2a_syncd_rot_b(
            est_rb_dis, est_rrb_dis, b2a_plan.prev_a2a_rot_sum);
    OrdPos naive_tar_ord_pos_a = to_ord_pos(naive_query.get_tar_rel_ord_a_B2A(), naive_query.get_stack_a_cnt());
    OrdPos naive_tar_ord_pos_b = naive_query.get_tar_ord_pos_b_B2A();
    int naive_ra_dis = RotCalculator::get_ra_dis(
            naive_start_ord_pos_a, naive_tar_ord_pos_a, naive_query.get_stack_a_cnt());
    int naive_rra_dis = RotCalculator::get_rra_dis(
            naive_start_ord_pos_a, naive_tar_ord_pos_a, naive_query.get_stack_a_cnt());
    int naive_rb_dis = RotCalculator::get_rb_dis(
            naive_start_ord_pos_b, naive_tar_ord_pos_b, naive_qri.get_stack_b_cnt(N));
    int naive_rrb_dis = RotCalculator::get_rrb_dis(
            naive_start_ord_pos_b, naive_tar_ord_pos_b, naive_qri.get_stack_b_cnt(N));
    out("debug_ins2_b2a_insert_mismatch",
        "v", v,
        "a2b_idx", case_info.a2b_idx,
        "b2a_idx", case_info.b2a_idx,
        "shifted_b2a_idx", naive.shifted_b2a_idx,
        "v_all_ord_b", case_info.v_all_ord_b,
        "estimated_turn", b2a_plan.bri.ins_turn,
        "est_stack_a_cnt_before", b2a_plan.stack_a_cnt,
        "est_stack_b_cnt_before", b2a_plan.stack_b_cnt,
        "est_stack_a_cnt_effected", b2a_plan.stack_a_cnt - 1,
        "est_stack_b_cnt_effected", b2a_plan.stack_b_cnt + 1,
        "est_tar_all_ord_a", b2a_plan.tar_all_ord_a.get(),
        "est_tar_all_ord_b", b2a_plan.tar_all_ord_b.get(),
        "est_tar_rel_ord_a", b2a_plan.tar_rel_ord_a.get(),
        "est_tar_ord_pos_a", b2a_plan.tar_ord_pos_a.get(),
        "est_tar_ord_pos_b", b2a_plan.tar_ord_pos_b.get(),
        "est_start_ord_pos_a", b2a_plan.start_ord_pos_a.get(),
        "est_start_ord_pos_b", b2a_plan.start_ord_pos_b.get(),
        "est_ra_dis", est_ra_dis,
        "est_rra_dis", est_rra_dis,
        "est_rb_dis", est_rb_dis,
        "est_rrb_dis", est_rrb_dis,
        "est_sync_rb_dis", est_sync_rb_dis,
        "est_sync_rrb_dis", est_sync_rrb_dis,
        "est_cmd_a", b2a_plan.bri.cmd_a,
        "est_cmd_a_cnt", b2a_plan.bri.cmd_a_cnt,
        "est_cmd_b", b2a_plan.bri.cmd_b,
        "est_cmd_b_cnt", b2a_plan.bri.cmd_b_cnt,
        "est_a2a_start_idx", b2a_plan.a2a_start_idx,
        "est_prev_a2a_ra_sum", b2a_plan.prev_a2a_rot_sum.ra_sum,
        "est_prev_a2a_rra_sum", b2a_plan.prev_a2a_rot_sum.rra_sum,
        "est_finished_ord_pos_a", b2a_plan.finished_ord_pos_a.get(),
        "est_finished_ord_pos_b", b2a_plan.finished_ord_pos_b.get(),
        "naive_turn", naive_qri.get_ins_turn(),
        "naive_stack_a_cnt", naive_query.get_stack_a_cnt(),
        "naive_stack_b_cnt", naive_qri.get_stack_b_cnt(N),
        "naive_tar_rel_ord_a", naive_query.get_tar_rel_ord_a_B2A().get(),
        "naive_tar_ord_pos_b", naive_query.get_tar_ord_pos_b_B2A().get(),
        "naive_start_ord_pos_a", naive_start_ord_pos_a.get(),
        "naive_start_ord_pos_b", naive_start_ord_pos_b.get(),
        "naive_tar_ord_pos_a", naive_tar_ord_pos_a.get(),
        "naive_tar_ord_pos_b", naive_tar_ord_pos_b.get(),
        "naive_ra_dis", naive_ra_dis,
        "naive_rra_dis", naive_rra_dis,
        "naive_rb_dis", naive_rb_dis,
        "naive_rrb_dis", naive_rrb_dis,
        "naive_cmd_a", naive_qri.ab_cmd_a(),
        "naive_cmd_a_cnt", naive_qri.get_a_cmd_cost_AB(),
        "naive_cmd_b", naive_qri.ab_cmd_b(),
        "naive_cmd_b_cnt", naive_qri.get_b_cmd_cost_AB(),
        "naive_finished_ord_pos_a", naive_qri.get_finished_top_ord_a().get(),
        "naive_prev_qri", naive_prev_qri,
        "naive_qri", indirect_debug_to_string(naive_qri));
    my_assert(false);
}

//a2b, b2aが挿入されることによる、q_idxの変化差分
void assert_debug_ins2_q_idx_diff(
        int v,
        const DebugIns2Case &case_info,
        const DebugIns2NaiveResult &naive,
        const QRIList &queries,
        int q_idx,
        int estimated_diff,
        const std::string &item_name
) {
    int naive_q_idx = q_idx;
    if (case_info.a2b_idx <= q_idx) naive_q_idx++;
    if (case_info.b2a_idx <= q_idx) naive_q_idx++;
    const int naive_diff = naive.yst.queries_state.queries[naive_q_idx].get_ins_turn()
                           - queries[q_idx].get_ins_turn();
    if (estimated_diff == naive_diff) return;
    // _REVIEW_BEGIN: mismatch 本体だけを出し、詳細な計算入力は direct diff 側へ寄せる。
    const int N = naive.yst.queries_state.current_N;
    const int before_prev_idx = q_idx - 1;
    const int naive_prev_idx = naive_q_idx - 1;
    OrdPos before_start_b = naive.yst.queries_state.first_top_ord_b;
    OrdPos naive_start_b = naive.yst.queries_state.first_top_ord_b;
    std::string before_prev_qri = "none";
    std::string naive_prev_qri = "none";
    if (before_prev_idx >= 0) {
        before_start_b = queries[before_prev_idx].get_query().get_finished_top_ord_b(N);
        before_prev_qri = indirect_debug_to_string(queries[before_prev_idx]);
    }
    if (naive_prev_idx >= 0) {
        naive_start_b = naive.yst.queries_state.queries[naive_prev_idx].get_query().get_finished_top_ord_b(N);
        naive_prev_qri = indirect_debug_to_string(naive.yst.queries_state.queries[naive_prev_idx]);
    }
    out("debug_ins2_q_idx_diff_mismatch",
        "v", v,
        "a2b_idx", case_info.a2b_idx,
        "b2a_idx", case_info.b2a_idx,
        "v_all_ord_b", case_info.v_all_ord_b,
        "item", item_name,
        "q_idx", q_idx,
        "naive_q_idx", naive_q_idx,
        "estimated_diff", estimated_diff,
        "naive_diff", naive_diff,
        "before_prev_idx", before_prev_idx,
        "naive_prev_idx", naive_prev_idx,
        "before_start_b", before_start_b.get(),
        "naive_start_b", naive_start_b.get(),
        "before_prev_qri", before_prev_qri,
        "naive_prev_qri", naive_prev_qri,
        "before_qri", indirect_debug_to_string(queries[q_idx]),
        "after_qri", indirect_debug_to_string(naive.yst.queries_state.queries[naive_q_idx]));
    // _REVIEW_END
    my_assert(false);
}

DebugIns2Case sample_debug_ins2_case(int qcnt, const BOrder &border) {
    DebugIns2Case ret;
    my_assert(qcnt > 0);
    ret.a2b_idx = my_rand(qcnt + 1);
    ret.b2a_idx = my_rand(ret.a2b_idx, qcnt + 1);
    ret.v_all_ord_b = my_rand(border.get_all_order_entries_size() + 1);
    return ret;
}

int to_debug_ins2_naive_q_idx(int q_idx, const DebugIns2Case &case_info) {
    int ret = q_idx;
    if (case_info.a2b_idx <= q_idx) ret++;
    if (case_info.b2a_idx <= q_idx) ret++;
    return ret;
}

template<class IntValues>
bool is_debug_excluded_q_idx(const IntValues &exclude_q_idxs, int q_idx) {
    for (int exclude_q_idx: exclude_q_idxs) {
        if (q_idx == exclude_q_idx) return true;
    }
    return false;
}

template<class IntValues>
int calc_debug_fixed_range_sum(
        const QueryPrefixArray &fixed_prefix_sum,
        const IntValues &exclude_q_idxs,
        int begin_idx,
        int end_idx
) {
    int sum = calc_prefix_sum(fixed_prefix_sum, begin_idx, end_idx);
    for (int q_idx: exclude_q_idxs) {
        if (begin_idx <= q_idx && q_idx < end_idx) {
            sum -= calc_prefix_sum(fixed_prefix_sum, q_idx, q_idx + 1);
        }
    }
    return sum;
}

//てすとようのこーど
template<class IntValues, class BandRangeContainer>
int calc_debug_band_range_sum(
        const BandRangeContainer &band_infos,
        const DebugIns2Case &case_info,
        const IntValues &exclude_q_idxs,
        int begin_idx,
        int end_idx
) {
    int sum = 0;
    for (int q_idx = begin_idx; q_idx < end_idx; q_idx++) {
        if (is_debug_excluded_q_idx(exclude_q_idxs, q_idx)) continue;
        sum += is_b_all_ord_inside_band(band_infos[q_idx], case_info.v_all_ord_b);
    }
    return sum;
}

template<class IntValues, class BandRangeContainer>
void log_debug_indirect_range_sum_items(
        const DebugIns2Case &case_info,
        const DebugIns2NaiveResult &naive,
        const QRIList &queries,
        const QueryPrefixArray &fixed_prefix_sum,
        const BandRangeContainer &band_infos,
        const IntValues &exclude_q_idxs,
        int begin_idx,
        int end_idx
) {

#ifndef LOG_PHYSICAL
    return;
#endif
    for (int q_idx = begin_idx; q_idx < end_idx; q_idx++) {
        int naive_q_idx = to_debug_ins2_naive_q_idx(q_idx, case_info);
        int fixed_piece = calc_prefix_sum(fixed_prefix_sum, q_idx, q_idx + 1);
        int band_hit = is_b_all_ord_inside_band(band_infos[q_idx], case_info.v_all_ord_b);
        int naive_piece = naive.yst.queries_state.queries[naive_q_idx].get_ins_turn() - queries[q_idx].get_ins_turn();
        out("debug_ins2_range_sum_item", "q_idx", q_idx, "naive_q_idx", naive_q_idx,
            "excluded", is_debug_excluded_q_idx(exclude_q_idxs, q_idx), "fixed_piece", fixed_piece,
            "band_hit", band_hit, "estimated_piece", fixed_piece + band_hit, "naive_piece", naive_piece,
            "before_qri", indirect_debug_to_string(queries[q_idx]),
            "after_qri", indirect_debug_to_string(naive.yst.queries_state.queries[naive_q_idx]));
    }
}

template<class IntValues, class BandRangeContainer>
void debug_indirect_range_sum(int v, const DebugIns2Case &case_info, const DebugIns2NaiveResult &naive,
                              const QRIList &queries, const QueryPrefixArray &fixed_prefix_sum,
                              const BandRangeContainer &band_infos, int begin_idx, int end_idx,
                              const IntValues &exclude_q_idxs, int estimated_diff,
                              const std::string &item_name) {
    int naive_diff = 0;
    for (int q_idx = begin_idx; q_idx < end_idx; q_idx++) {
        if (is_debug_excluded_q_idx(exclude_q_idxs, q_idx)) continue;
        int naive_q_idx = to_debug_ins2_naive_q_idx(q_idx, case_info);
        naive_diff += naive.yst.queries_state.queries[naive_q_idx].get_ins_turn();
        naive_diff -= queries[q_idx].get_ins_turn();
    }
    if (estimated_diff == naive_diff) return;
    out("debug_ins2_range_sum_mismatch", "v", v, "a2b_idx", case_info.a2b_idx,
        "b2a_idx", case_info.b2a_idx, "v_all_ord_b", case_info.v_all_ord_b, "item", item_name,
        "begin_idx", begin_idx, "end_idx", end_idx, "estimated_diff", estimated_diff, "naive_diff", naive_diff);
    log_debug_indirect_range_sum_items(
            case_info, naive, queries, fixed_prefix_sum, band_infos, exclude_q_idxs, begin_idx, end_idx);
    my_assert(false);
}

template<class IntValues>
void debug_fixed_range_sum(int v, const DebugIns2Case &case_info, const DebugIns2NaiveResult &naive,
                           const QRIList &queries, int begin_idx, int end_idx,
                           const IntValues &exclude_q_idxs, int estimated_diff,
                           const std::string &item_name) {
    int naive_diff = 0;
    for (int q_idx = begin_idx; q_idx < end_idx; q_idx++) {
        if (is_debug_excluded_q_idx(exclude_q_idxs, q_idx)) continue;
        int naive_q_idx = to_debug_ins2_naive_q_idx(q_idx, case_info);
        naive_diff += naive.yst.queries_state.queries[naive_q_idx].get_ins_turn();
        naive_diff -= queries[q_idx].get_ins_turn();
    }
    if (estimated_diff == naive_diff) return;
    out("debug_ins2_fixed_range_sum_mismatch", "v", v, "a2b_idx", case_info.a2b_idx,
        "b2a_idx", case_info.b2a_idx, "v_all_ord_b", case_info.v_all_ord_b, "item", item_name,
        "begin_idx", begin_idx, "end_idx", end_idx, "estimated_diff", estimated_diff, "naive_diff", naive_diff);
    my_assert(false);
}

PrevA2ARotSum calc_debug_a2a_rot_sum(
        const QueryPrefixArray &ra_prefix_sum,
        const QueryPrefixArray &rra_prefix_sum,
        int start_idx,
        int end_idx
) {
    return {
            calc_prefix_sum(ra_prefix_sum, start_idx, end_idx),
            calc_prefix_sum(rra_prefix_sum, start_idx, end_idx)
    };
}

PrevA2ARotSum build_effected_a2a_rot_sum(
        const YakinamashiState &yst,
        const State &init_st,
        const QueryCommonRI &qri,
        const QueryOrdEffect &effect,
        OrdPos start_top_a,
        OrdPos start_top_b,
        int N
) {
    my_assert(qri.is_a2a_type());
    QueryProvider qprov(yst, init_st);
    QCRIFactory qfac;
    QueryCommon effected_q = qprov.get_applied_common(start_top_b, qri.get_query(), effect);
    QueryCommonRI effected_qri = qfac.build_a2a_ri_from_start_top(start_top_a, effected_q);
    auto [ra_cnt, rra_cnt] = effected_qri.get_can_sync_ra_rra_A2A();
    return PrevA2ARotSum(ra_cnt, rra_cnt);
}

// _REVIEW: direct2 直前の effect 後 A2A 列から start_a と同期回転量を同じ根拠で得るため。
struct EffectedA2AChainEval {
    OrdPos finished_top_a;
    PrevA2ARotSum rot_sum;
};

// _REVIEW: direct2 の開始位置を effect 後 A2A 実行結果から確定するため。
EffectedA2AChainEval build_effected_a2a_chain_eval(
        const YakinamashiState &yst,
        const State &init_st,
        const QRIList &queries,
        const QueryOrdEffect &effect,
        OrdPos start_top_a,
        OrdPos start_top_b,
        int begin_idx,
        int end_idx,
        int N
) {
    QueryProvider qprov(yst, init_st);
    QCRIFactory qfac;
    OrdPos current_top_a = start_top_a;
    PrevA2ARotSum sum(0, 0);
    for (int q_idx = begin_idx; q_idx < end_idx; q_idx++) {
        const QueryCommonRI &qri = queries[q_idx];
        my_assert(qri.is_a2a_type());
        QueryCommon effected_q = qprov.get_applied_common(start_top_b, qri.get_query(), effect);
        QueryCommonRI effected_qri = qfac.build_a2a_ri_from_start_top(current_top_a, effected_q);
        auto [ra_cnt, rra_cnt] = effected_qri.get_can_sync_ra_rra_A2A();
        sum.ra_sum += ra_cnt;
        sum.rra_sum += rra_cnt;
        current_top_a = effected_qri.get_finished_top_ord_a();
    }
    return {current_top_a, sum};
}

PrevA2ARotSum calc_debug_b2a_prev_a2a_rot_sum(
        const YakinamashiState &yst,
        const State &init_st,
        const DebugIns2Case &case_info,
        const QRIList &queries,
        const DebugIns2A2BPlan &a2b_plan,
        const QueryPrefixArray &indirect_a2b_ra_prefix_sum_A2A,
        const QueryPrefixArray &indirect_a2b_rra_prefix_sum_A2A,
        int start_idx,
        int end_idx,
        const InsertPairPrecalc& prec,
        int N
) {
    PrevA2ARotSum sum(0, 0);
    const int direct1_idx = prec.next_a2b_effected_valid_q_idx[case_info.a2b_idx];

    if (start_idx == direct1_idx && direct1_idx < end_idx) {
        QueryOrdEffect a2b_effect;
        a2b_effect.a_effect.set_del(a2b_plan.tar_all_ord_a);
        a2b_effect.b_effect.set_add(a2b_plan.tar_all_ord_b);
        a2b_effect.diff_stack_a_cnt = -1;
        PrevA2ARotSum direct1_sum = build_effected_a2a_rot_sum(
                yst, init_st, queries[direct1_idx], a2b_effect,
                a2b_plan.finished_ord_pos_a, a2b_plan.finished_ord_pos_b, N);
        sum.ra_sum += direct1_sum.ra_sum;
        sum.rra_sum += direct1_sum.rra_sum;
        start_idx++;
    }
    sum.ra_sum += calc_prefix_sum(indirect_a2b_ra_prefix_sum_A2A, start_idx, end_idx);
    sum.rra_sum += calc_prefix_sum(indirect_a2b_rra_prefix_sum_A2A, start_idx, end_idx);
    return sum;
}

int calc_debug_mid_indirect_sum(
        const QueryPrefixArray &fixed_prefix_sum,
        const QueryBandRangeArray &band_infos,
        const DebugIns2Case &case_info,
        int direct1_idx,
        int direct2_idx
) {
    if (case_info.a2b_idx == case_info.b2a_idx) {
        return 0;
    }
    vec_array<int, 2> exclude_q_idxs;
    if (case_info.a2b_idx <= direct1_idx && direct1_idx < case_info.b2a_idx) {
        exclude_q_idxs.push_back(direct1_idx);
    }
    if (case_info.a2b_idx <= direct2_idx && direct2_idx < case_info.b2a_idx) {
        exclude_q_idxs.push_back(direct2_idx);
    }
    int fixed_sum = calc_debug_fixed_range_sum(
            fixed_prefix_sum, exclude_q_idxs, case_info.a2b_idx, case_info.b2a_idx);
    int band_sum = calc_debug_band_range_sum(
            band_infos, case_info, exclude_q_idxs, case_info.a2b_idx, case_info.b2a_idx);
    return fixed_sum + band_sum;
}

int calc_debug_mid_indirect_sum_without_band(
        const QueryPrefixArray &fixed_prefix_sum,
        const QueryBandRangeArray &band_infos,
        const DebugIns2Case &case_info,
        int direct1_idx,
        int direct2_idx
) {
    if (case_info.a2b_idx == case_info.b2a_idx) {
        return 0;
    }
    vec_array<int, 2> exclude_q_idxs;
    if (case_info.a2b_idx <= direct1_idx && direct1_idx < case_info.b2a_idx) {
        exclude_q_idxs.push_back(direct1_idx);
    }
    if (case_info.a2b_idx <= direct2_idx && direct2_idx < case_info.b2a_idx) {
        exclude_q_idxs.push_back(direct2_idx);
    }
    int fixed_sum = calc_debug_fixed_range_sum(
            fixed_prefix_sum, exclude_q_idxs, case_info.a2b_idx, case_info.b2a_idx);
    return fixed_sum;
}

int calc_tail_indirect_sum(
        const QueryPrefixArray &fixed_prefix_sum,
        const DebugIns2Case &case_info,
        int direct1_idx,
        int direct2_idx,
        int qcnt
) {
    my_assert(case_info.b2a_idx <= qcnt);
    if (case_info.b2a_idx == qcnt) {
        return 0;
    }
    vec_array<int, 2> exclude_q_idxs;
    if (case_info.b2a_idx <= direct1_idx && direct1_idx < qcnt) {
        exclude_q_idxs.push_back(direct1_idx);
    }
    if (case_info.b2a_idx <= direct2_idx && direct2_idx < qcnt) {
        exclude_q_idxs.push_back(direct2_idx);
    }
    return calc_debug_fixed_range_sum(fixed_prefix_sum, exclude_q_idxs, case_info.b2a_idx, qcnt);
}

int get_debug_stack_a_cnt_before_q_idx(const YakinamashiState &yst, int q_idx, int N) {
    const auto &queries = yst.queries_state.queries;
    my_assert(0 <= q_idx && q_idx <= static_cast<int>(queries.size()));
    if (q_idx == static_cast<int>(queries.size())) return N;
    return queries[q_idx].get_stack_a_cnt();
}

int get_debug_stack_b_cnt_before_q_idx(const YakinamashiState &yst, int q_idx, int N) {
    return N - get_debug_stack_a_cnt_before_q_idx(yst, q_idx, N);
}

OrdPos get_debug_start_ord_a_before_q_idx(const YakinamashiState &yst, int q_idx) {
    const auto &queries = yst.queries_state.queries;
    my_assert(0 <= q_idx && q_idx <= static_cast<int>(queries.size()));
    if (q_idx == 0) return yst.queries_state.first_top_ord_a;
    //effectの影響がないため、ひとつまえから取って問題ない
    return queries[q_idx - 1].get_finished_top_ord_a();
}

OrdPos get_debug_start_ord_b_before_q_idx(const YakinamashiState &yst, int q_idx, int N) {
    const auto &queries = yst.queries_state.queries;
    my_assert(0 <= q_idx && q_idx <= static_cast<int>(queries.size()));
    if (q_idx == 0) return yst.queries_state.first_top_ord_b;
    //effectの影響がないため、ひとつまえから取って問題ない
    return queries[q_idx - 1].get_query().get_finished_top_ord_b(N);
}

// 責務: A2B 挿入候補の採点に必要な plan を、effect 前の境界 prefix 情報から組み立てる。
// 必要な理由: prev_a2a_ranges を vec_array 化した後も既存の direct diff 計算をそのまま使うため。
template<typename ExistAList, typename ExistBList>
DebugIns2A2BPlan build_a2b_plan(int v, const YakinamashiState &yst,
                                AllOrd new_pushed_b_all_ord,
                                const ExistAList &exist_a_ords_before_q_idx,
                                const ExistBList &exist_b_ords_before_q_idx,
                                const QueryBoundaryArray &prev_a2a_ranges,
                                const QueryPrefixArray &ra_prefix_sum,
                                const QueryPrefixArray &rra_prefix_sum,
                                int a2b_idx, int N) {
    const auto &queries = yst.queries_state.queries;
    const auto &aorder = yst.query_order.aorder;
    my_assert(a2b_idx <= static_cast<int>(queries.size()));
    DebugIns2A2BPlan plan;
    plan.tar_all_ord_a = aorder.get_init_order(v);
    plan.tar_all_ord_b = new_pushed_b_all_ord;
    plan.tar_rel_ord_a = OrdCalc::get_rel_ord(exist_a_ords_before_q_idx[a2b_idx], plan.tar_all_ord_a);
    plan.tar_rel_ord_b = OrdCalc::get_rel_ord(exist_b_ords_before_q_idx[a2b_idx], plan.tar_all_ord_b);
    plan.stack_a_cnt = get_debug_stack_a_cnt_before_q_idx(yst, a2b_idx, N);
    plan.stack_b_cnt = get_debug_stack_b_cnt_before_q_idx(yst, a2b_idx, N);
    OrdPos start_a = get_debug_start_ord_a_before_q_idx(yst, a2b_idx);
    OrdPos start_b = get_debug_start_ord_b_before_q_idx(yst, a2b_idx, N);
    int start_idx = a2b_idx - prev_a2a_ranges[a2b_idx];
    PrevA2ARotSum prev_sum = calc_debug_a2a_rot_sum(ra_prefix_sum, rra_prefix_sum, start_idx, a2b_idx);
    plan.bri = build_brp_by_ord_with_prev_a2a(
            plan.stack_a_cnt, plan.stack_b_cnt, start_a, start_b,
            to_ord_pos(plan.tar_rel_ord_a, plan.stack_a_cnt), to_ord_pos(plan.tar_rel_ord_b, plan.stack_b_cnt),
            prev_sum);
    plan.finished_ord_pos_a = OrdCalc::get_erased_next_ord(to_ord_pos(plan.tar_rel_ord_a, plan.stack_a_cnt),
                                                           plan.stack_a_cnt);
    plan.finished_ord_pos_b = OrdCalc::get_inserted_ord(plan.tar_rel_ord_b);
    return plan;
}

//ない場合は-1
//int get_prev_valid_q_idx(const YakinamashiState &yst, int q_idx) {
//    const auto &queries = yst.queries_state.queries;
//    for (int prev_q_idx = q_idx - 1; prev_q_idx >= 0; prev_q_idx--) {
//        auto &qri = queries[prev_q_idx];
//        if (qri.is_a2a_type() && qri.get_flip_dis_dir_A2A() == 0) {
//            continue;
//        }
//        return prev_q_idx;
//    }
//    return -1;
//}

//ない場合は、queries.size()
//int get_first_valid_q_idx(const YakinamashiState &yst, int q_idx) {
//    const auto &queries = yst.queries_state.queries;
//    for (int tar_q_idx = q_idx; tar_q_idx < queries.size(); tar_q_idx++) {
//        auto &qri = queries[tar_q_idx];
//        if (qri.is_a2a_type() && qri.get_flip_dis_dir_A2A() == 0) {
//            continue;
//        }
//        return tar_q_idx;
//    }
//    return yst.queries_state.queries.size();
//}


// : A2B削除後topは円環wrapを持つため、top自体ではなく補正後queryから再計算する。
OrdPos calc_effected_a2b_finished_top_ord_a(
        const YakinamashiState &yst,
        const QueryCommonRI &prev_qri,
        AllOrd deleted_all_ord_a
) {
    prev_qri.validate_is_a2b_type();
    const QueryCommon &prev_q = prev_qri.get_query();
    SingleSideOrdEffect effect;
    effect.set_del(deleted_all_ord_a);
    const AllOrd prev_tar_all_ord_a = get_tar_all_ord_a(yst, prev_qri);
    const OrdPos new_tar_ord_pos_a = QueryProvider::apply_single_side_ord(
            prev_q.get_tar_ord_pos_a_A2B(), prev_tar_all_ord_a, effect);
    const int new_stack_a_cnt = prev_q.get_stack_a_cnt() - 1;
    my_assert(new_stack_a_cnt >= 0);
    const QueryCommon effected_prev_q(
            CommonPushType::A2B,
            prev_q.get_tar_v(),
            new_stack_a_cnt,
            new_tar_ord_pos_a.get(),
            prev_q.get_tar_rel_ord_b_A2B().get()
    );
    return effected_prev_q.get_finished_top_ord_a(yst.queries_state.current_N);
}

//ok
OrdPos calc_debug_b2a_start_ord_pos_a(
        const YakinamashiState &yst,
        const State &init_st,
        const DebugIns2Case &case_info,
        const QRIList &queries,
        OrdPos start_ord_pos_b,
        const DebugIns2A2BPlan &a2b_plan,
        const InsertPairPrecalc& prec,
        int N
) {
    // 責務: B2A 挿入直前の A 側 start top OrdPos を、挿入 A2B 後から見た連続 A2A 範囲の replay 結果として求める。
    // 必要な理由: valid query 起点では no-cmd A2A の top 変化を取り落とし、B2A start A がずれるため。
    QueryOrdEffect a2b_effect;
    a2b_effect.a_effect.set_del(a2b_plan.tar_all_ord_a);
    a2b_effect.b_effect.set_add(a2b_plan.tar_all_ord_b);
    a2b_effect.diff_stack_a_cnt = -1;
    int a2a_begin_idx = case_info.b2a_idx - prec.prev_a2a_ranges[case_info.b2a_idx];
    a2a_begin_idx = std::max(a2a_begin_idx, case_info.a2b_idx);
    OrdPos start_a = a2b_plan.finished_ord_pos_a;
    if (a2a_begin_idx > case_info.a2b_idx) {
        my_assert(a2a_begin_idx - 1 < static_cast<int>(prec.a2b_effected_finished_top_ord_a.size()));
        start_a = prec.a2b_effected_finished_top_ord_a[a2a_begin_idx - 1];
    }
    EffectedA2AChainEval chain_eval = build_effected_a2a_chain_eval(
            yst, init_st, queries, a2b_effect,
            start_a, start_ord_pos_b,
            a2a_begin_idx, case_info.b2a_idx, N);
    return chain_eval.finished_top_a;
}

OrdPos get_added_ord_ins(OrdPos tar_ord, AllOrd tar_all_ord, AllOrd added_all_ord) {
    //BOrderのins_updateの仕様に基づいて、同じ値の場合その前に挿入する
    if (tar_all_ord.get() >= added_all_ord.get()) {
        return OrdPos(tar_ord.get() + 1);
    } else {
        return tar_ord;
    }
}

// 責務: B2A 挿入直前の B 側 start top と、その再計算開始 query index を返す。
// 必要な理由: prev_a2a_ranges を vec_array 化した後も A2A chain 切り出し位置の意味を変えずに使うため。
std::pair<OrdPos, int> calc_debug_b2a_start_ord_pos_b(
        const YakinamashiState &yst,
        const State &init_st,
        const DebugIns2Case &case_info,
        const DebugIns2A2BPlan &a2b_plan,
        const QueryBoundaryArray &prev_a2a_ranges,
        int N
) {
    const auto &queries = yst.queries_state.queries;
    int prev_a2a_range = prev_a2a_ranges[case_info.b2a_idx];
    int prev_ab_idx = case_info.b2a_idx - prev_a2a_range - 1;
    if (prev_ab_idx < case_info.a2b_idx) return {OrdCalc::get_inserted_ord(a2b_plan.tar_rel_ord_b), case_info.a2b_idx};
    const auto &prev_ab_qri = queries[prev_ab_idx];
    OrdPos start_b(0);
    if (case_info.b2a_idx < queries.size() && queries[case_info.b2a_idx].get_stack_b_cnt(N) > 0) {
        QueryOrdEffect a2b_effect;
        a2b_effect.a_effect.set_del(a2b_plan.tar_all_ord_a);
        a2b_effect.b_effect.set_add(a2b_plan.tar_all_ord_b);
        a2b_effect.diff_stack_a_cnt = -1;
        QueryProvider qprov(yst, init_st);
        QueryCommon effected_prev_q =
                qprov.get_applied_common(a2b_plan.finished_ord_pos_b, prev_ab_qri.get_query(), a2b_effect);
        start_b = effected_prev_q.get_finished_top_ord_b(N);
    }
    return {start_b, case_info.b2a_idx - prev_a2a_range};
}

// 責務: B2A 挿入候補の採点に必要な plan を、A2B 後 effect と境界 prefix 情報から組み立てる。
// 必要な理由: prev_a2a_ranges を vec_array 化した後も direct diff 計算の入力形を揃えるため。
template<typename ExistAList, typename ExistBList>
DebugIns2B2APlan build_b2a_plan(int v, const YakinamashiState &yst, const State &init_st,
                                const DebugIns2Case &case_info,
                                const DebugIns2A2BPlan &a2b_plan,
                                const ExistAList &exist_a_ords_before_q_idx,
                                const ExistBList &exist_b_ords_before_q_idx,
                                const QueryBoundaryArray &prev_a2a_ranges,
                                const QueryPrefixArray &indirect_a2b_ra_prefix_sum_A2A,
                                const QueryPrefixArray &indirect_a2b_rra_prefix_sum_A2A,
                                int N,
                                const InsertPairPrecalc &prec
                                      ) {
    const auto &queries = yst.queries_state.queries;
    const auto &aorder = yst.query_order.aorder;
    my_assert(case_info.b2a_idx <= static_cast<int>(queries.size()));
    DebugIns2B2APlan plan;
    plan.tar_all_ord_a = aorder.get_target_order(v);
    plan.tar_all_ord_b = a2b_plan.tar_all_ord_b;
    plan.stack_a_cnt = get_debug_stack_a_cnt_before_q_idx(yst, case_info.b2a_idx, N);
    plan.stack_b_cnt = get_debug_stack_b_cnt_before_q_idx(yst, case_info.b2a_idx, N);
    RelOrd base_rel_ord_a = OrdCalc::get_rel_ord(exist_a_ords_before_q_idx[case_info.b2a_idx], plan.tar_all_ord_a);
    plan.tar_rel_ord_a = OrdCalc::get_erased_ord(base_rel_ord_a, plan.tar_all_ord_a, a2b_plan.tar_all_ord_a);
    plan.tar_ord_pos_a = to_ord_pos(plan.tar_rel_ord_a, plan.stack_a_cnt - 1);
    plan.tar_ord_pos_b = to_ord_pos(
            OrdCalc::get_rel_ord(exist_b_ords_before_q_idx[case_info.b2a_idx], a2b_plan.tar_all_ord_b),
            plan.stack_b_cnt + 1);
    auto [start_b, a2a_start_idx] =
            calc_debug_b2a_start_ord_pos_b(yst, init_st, case_info, a2b_plan, prev_a2a_ranges, N);
    plan.start_ord_pos_b = start_b;
    plan.a2a_start_idx = a2a_start_idx;
    plan.start_ord_pos_a = calc_debug_b2a_start_ord_pos_a(
            yst, init_st, case_info, queries, plan.start_ord_pos_b, a2b_plan, prec, N);
    plan.prev_a2a_rot_sum = calc_debug_b2a_prev_a2a_rot_sum(
            yst, init_st, case_info, queries, a2b_plan,
            indirect_a2b_ra_prefix_sum_A2A, indirect_a2b_rra_prefix_sum_A2A,
            a2a_start_idx, case_info.b2a_idx,
            prec, N);
    plan.bri = build_brp_by_ord_with_prev_a2a(plan.stack_a_cnt - 1, plan.stack_b_cnt + 1,
                                              plan.start_ord_pos_a, plan.start_ord_pos_b,
                                              plan.tar_ord_pos_a, plan.tar_ord_pos_b, plan.prev_a2a_rot_sum);
    plan.finished_ord_pos_a = OrdCalc::get_inserted_ord(plan.tar_rel_ord_a);
    plan.finished_ord_pos_b = OrdCalc::get_erased_next_ord(plan.tar_ord_pos_b, plan.stack_b_cnt + 1);
    return plan;
}

//a2bを挿入した直後のins_turn(元のa2b_idxのins_turnの変化を返す)
int calc_debug_next_ins_turn_after_a2b(
        const QueryCommonRI &qri,
        const QueryCommon &effected_q,
        OrdPos finished_a,
        OrdPos finished_b,
        int stack_a_cnt,
        int stack_b_cnt,
        int N,
        QCRIFactory &qfac
) {
    if (!qri.is_ab_type()) return qfac.build_a2a_ri_from_start_top(finished_a, effected_q).get_ins_turn();
    int effected_stack_a_cnt = stack_a_cnt - 1;
    int effected_stack_b_cnt = stack_b_cnt + 1;
    OrdPos tar_a = qri.is_a2b_type() ? effected_q.get_tar_ord_pos_a_A2B()
                                     : to_ord_pos(effected_q.get_tar_rel_ord_a_B2A(), effected_stack_a_cnt);
    OrdPos tar_b = qri.is_a2b_type() ? to_ord_pos(effected_q.get_tar_rel_ord_b_A2B(), effected_stack_b_cnt)
                                     : effected_q.get_tar_ord_pos_b_B2A();
    return build_brp_by_ord_with_prev_a2a(effected_stack_a_cnt, effected_stack_b_cnt, finished_a, finished_b,
                                          tar_a, tar_b, PrevA2ARotSum(0, 0)).ins_turn;
}

QueryOrdEffect build_a2b_b2a_net_exist_effect(
        const DebugIns2A2BPlan &a2b_plan,
        const DebugIns2B2APlan &b2a_plan
) {
    QueryOrdEffect effect;
    effect.diff_stack_a_cnt = 0;
    if (a2b_plan.tar_all_ord_a != b2a_plan.tar_all_ord_a) {
        effect.a_effect.set_del(a2b_plan.tar_all_ord_a);
        effect.a_effect.set_add(b2a_plan.tar_all_ord_a);
    }
    return effect;
}

OrdPos get_ab_tar_ord_pos_a(const QueryCommonRI &qri, const QueryCommon &query) {
    return qri.is_a2b_type()
           ? query.get_tar_ord_pos_a_A2B()
           : to_ord_pos(query.get_tar_rel_ord_a_B2A(), query.get_stack_a_cnt());
}

OrdPos get_ab_tar_ord_pos_b(const QueryCommonRI &qri, const QueryCommon &query, int N) {
    return qri.is_a2b_type()
           ? to_ord_pos(query.get_tar_rel_ord_b_A2B(), query.get_stack_b_cnt(N))
           : query.get_tar_ord_pos_b_B2A();
}

int calc_debug_next_ins_turn_after_b2a(
        const QueryCommonRI &qri,
        const QueryCommon &effected_q,
        OrdPos finished_a,
        OrdPos finished_b,
        int N,
        QCRIFactory &qfac
) {
    my_assert (qri.is_ab_type());
    OrdPos tar_a = get_ab_tar_ord_pos_a(qri, effected_q);
    OrdPos tar_b = get_ab_tar_ord_pos_b(qri, effected_q, N);
    return build_brp_by_ord_with_prev_a2a(
            effected_q.get_stack_a_cnt(), effected_q.get_stack_b_cnt(N),
            finished_a, finished_b, tar_a, tar_b, PrevA2ARotSum(0, 0)).ins_turn;
}

int calc_a2b_direct1_diff(
        const int direct1_idx,
        const YakinamashiState &yst,
        const State &init_st,
        const QRIList &queries,
        const DebugIns2Case &case_info,
        const DebugIns2A2BPlan &a2b_plan,
        int N
) {
    if (case_info.a2b_idx >= case_info.b2a_idx) return 0;
    if (direct1_idx == -1 || direct1_idx >= case_info.b2a_idx) return 0;
    const auto &qri = queries[direct1_idx];
    QueryOrdEffect a2b_effect;
    a2b_effect.a_effect.set_del(a2b_plan.tar_all_ord_a);
    a2b_effect.b_effect.set_add(a2b_plan.tar_all_ord_b);
    a2b_effect.diff_stack_a_cnt = -1;
    QueryProvider qprov(yst, init_st);
    QCRIFactory qfac;
    QueryCommon effected_q = qprov.get_applied_common(a2b_plan.finished_ord_pos_b, qri.get_query(), a2b_effect);
    // 責務: inserted A2B の後、direct1_idx 直前の A2A 列を順に評価して direct1 の開始 top を求める。
    // 必要な理由: direct1 の手数は直前 A2A の finished_top_a に依存し、A2B 直後の top をそのまま使うと turn 差がずれるため。
    const EffectedA2AChainEval direct1_chain_eval = build_effected_a2a_chain_eval(
            yst, init_st, queries, a2b_effect,
            a2b_plan.finished_ord_pos_a, a2b_plan.finished_ord_pos_b,
            case_info.a2b_idx, direct1_idx, N);
    const OrdPos direct1_start_a = direct1_chain_eval.finished_top_a;
    int prev_ins_turn = qri.get_ins_turn();
    int new_ins_turn = calc_debug_next_ins_turn_after_a2b(qri, effected_q, direct1_start_a,
                                                           a2b_plan.finished_ord_pos_b, a2b_plan.stack_a_cnt,
                                                           a2b_plan.stack_b_cnt, N, qfac);
#ifdef DEBUG
    int effected_stack_a_cnt = a2b_plan.stack_a_cnt - 1;
    int effected_stack_b_cnt = a2b_plan.stack_b_cnt + 1;
    OrdPos debug_start_a = direct1_start_a;
    if (qri.is_ab_type()) {
        OrdPos tar_a = qri.is_a2b_type() ? effected_q.get_tar_ord_pos_a_A2B()
                                         : to_ord_pos(effected_q.get_tar_rel_ord_a_B2A(), effected_stack_a_cnt);
        OrdPos tar_b = qri.is_a2b_type() ? to_ord_pos(effected_q.get_tar_rel_ord_b_A2B(), effected_stack_b_cnt)
                                         : effected_q.get_tar_ord_pos_b_B2A();
        int ra_dis = RotCalculator::get_ra_dis(debug_start_a, tar_a, effected_stack_a_cnt);
        int rra_dis = RotCalculator::get_rra_dis(debug_start_a, tar_a, effected_stack_a_cnt);
        int rb_dis = RotCalculator::get_rb_dis(a2b_plan.finished_ord_pos_b, tar_b, effected_stack_b_cnt);
        int rrb_dis = RotCalculator::get_rrb_dis(a2b_plan.finished_ord_pos_b, tar_b, effected_stack_b_cnt);
        out("debug_a2b_direct1_calc",
            "a2b_idx", case_info.a2b_idx,
            "b2a_idx", case_info.b2a_idx,
            "direct1_idx", direct1_idx,
            "finished_a", a2b_plan.finished_ord_pos_a.get(),
            "direct1_start_a", debug_start_a.get(),
            "finished_b", a2b_plan.finished_ord_pos_b.get(),
            "a2b_stack_a_cnt", a2b_plan.stack_a_cnt,
            "a2b_stack_b_cnt", a2b_plan.stack_b_cnt,
            "effected_stack_a_cnt", effected_stack_a_cnt,
            "effected_stack_b_cnt", effected_stack_b_cnt,
            "tar_a", tar_a.get(),
            "tar_b", tar_b.get(),
            "ra_dis", ra_dis,
            "rra_dis", rra_dis,
            "rb_dis", rb_dis,
            "rrb_dis", rrb_dis,
            "prev_turn", prev_ins_turn,
            "new_turn", new_ins_turn,
            "diff", new_ins_turn - prev_ins_turn,
            "a2b_tar_all_ord_a", a2b_plan.tar_all_ord_a.get(),
            "a2b_tar_all_ord_b", a2b_plan.tar_all_ord_b.get(),
            "base_qri", indirect_debug_to_string(qri),
            "effected_q", effected_q);
    } else {
        out("debug_a2b_direct1_calc_a2a",
            "a2b_idx", case_info.a2b_idx,
            "b2a_idx", case_info.b2a_idx,
            "direct1_idx", direct1_idx,
            "finished_a", a2b_plan.finished_ord_pos_a.get(),
            "direct1_start_a", debug_start_a.get(),
            "finished_b", a2b_plan.finished_ord_pos_b.get(),
            "a2b_stack_a_cnt", a2b_plan.stack_a_cnt,
            "a2b_stack_b_cnt", a2b_plan.stack_b_cnt,
            "effected_stack_a_cnt", effected_stack_a_cnt,
            "effected_stack_b_cnt", effected_stack_b_cnt,
            "tar_a1", effected_q.get_tar_ord_pos_a1_A2A().get(),
            "tar_a2", effected_q.get_tar_ord_pos_a2_A2A(effected_stack_a_cnt).get(),
            "flip_dis_dir", effected_q.get_flip_dis_dir(),
            "prev_turn", prev_ins_turn,
            "new_turn", new_ins_turn,
            "diff", new_ins_turn - prev_ins_turn,
            "a2b_tar_all_ord_a", a2b_plan.tar_all_ord_a.get(),
            "a2b_tar_all_ord_b", a2b_plan.tar_all_ord_b.get(),
            "base_qri", indirect_debug_to_string(qri),
            "effected_q", effected_q);
    }
#endif
    return new_ins_turn - prev_ins_turn;
}

// 責務: 挿入 A2B 後に direct2 で再計算される query の差分 turn を返す。
// 必要な理由: next_ab_idxs を vec_array 化した後も direct2 判定の index 意味を変えずに使うため。
//ok
int calc_a2b_direct2_diff(
        const int a2b_direct1_idx,
        const int a2b_direct2_idx,
        const YakinamashiState &yst,
        const State &init_st,
        const QRIList &queries,
        const DebugIns2Case &case_info,
        const DebugIns2A2BPlan &a2b_plan,
        int N,
        const QueryBoundaryArray &next_ab_idx,
        const QueryOrdEffect &a2b_effect,
        const QueryPrefixArray &indirect_ra_prefix_sum_A2A,
        const QueryPrefixArray &indirect_rra_prefix_sum_A2A,
        const InsertPairPrecalc &prec
) {
    if (a2b_direct2_idx == -1 || a2b_direct2_idx >= case_info.b2a_idx) {
        return 0;
    }
    QueryProvider qprov(yst, init_st);
    //a2bのエフェクトがかかった状態で、validなdirect2の前のidx
    int prev_valid_a_q_idx = prec.prev_a2b_effected_valid_q_idx[a2b_direct2_idx];

    
    //a側も、挿入するa2b_idxがdirectに影響するケース
    //それはdirect2==direct1ということになるので、おかしい(そのような場合、direct2=-1に設定されるため)
    my_assert(prev_valid_a_q_idx >= case_info.a2b_idx);

    const QueryCommonRI &prev_valid_qri = queries[prev_valid_a_q_idx];
    const QueryCommonRI &tar_qri = queries[a2b_direct2_idx];
    QueryCommon effected_q = qprov.get_applied_common(a2b_plan.finished_ord_pos_b, tar_qri.get_query(), a2b_effect);
    //向き変わらない前提のコードになってる気がする、だいたいあってるからいいか

    // 責務: A2B 挿入後から direct2 直前までの A2A 列を同じ start top から評価し、direct2 の開始 top と同期可能回転量を得る。
    // 必要な理由: direct1 A2A を単体で評価すると開始 top がずれ、存在しない rra 同期を direct2 に乗せてしまうため。
    EffectedA2AChainEval chain_eval = build_effected_a2a_chain_eval(
            yst, init_st, queries, a2b_effect,
            a2b_plan.finished_ord_pos_a, a2b_plan.finished_ord_pos_b,
            case_info.a2b_idx, a2b_direct2_idx, N);
    OrdPos start_a = chain_eval.finished_top_a;
    PrevA2ARotSum prev_sum = chain_eval.rot_sum;
    OrdPos tar_a = tar_qri.is_a2b_type() ? effected_q.get_tar_ord_pos_a_A2B()
                                         : to_ord_pos(effected_q.get_tar_rel_ord_a_B2A(), effected_q.get_stack_a_cnt());
    OrdPos tar_b = tar_qri.is_a2b_type() ? to_ord_pos(effected_q.get_tar_rel_ord_b_A2B(), effected_q.get_stack_b_cnt(N))
                                         : effected_q.get_tar_ord_pos_b_B2A();
    my_assert(queries[a2b_direct1_idx].is_a2a_type());
    BestRotInfo bri = build_brp_by_ord_with_prev_a2a(
            effected_q.get_stack_a_cnt(), effected_q.get_stack_b_cnt(N),
            start_a, a2b_plan.finished_ord_pos_b, tar_a, tar_b, prev_sum);
    out("debug_a2b_direct2_calc",
        "a2b_idx", case_info.a2b_idx,
        "b2a_idx", case_info.b2a_idx,
        "direct1_idx", a2b_direct1_idx,
        "direct2_idx", a2b_direct2_idx,
        "prev_valid_idx", prev_valid_a_q_idx,
        "start_a", start_a.get(),
        "finished_b", a2b_plan.finished_ord_pos_b.get(),
        "tar_a", tar_a.get(),
        "tar_b", tar_b.get(),
        "prev_sum_ra", prev_sum.ra_sum,
        "prev_sum_rra", prev_sum.rra_sum,
        "new_turn", bri.ins_turn,
        "old_turn", tar_qri.get_ins_turn(),
        "prev_qri", indirect_debug_to_string(prev_valid_qri),
        "tar_qri", indirect_debug_to_string(tar_qri),
        "effected_q", effected_q);
    return bri.ins_turn - tar_qri.get_ins_turn();
}

int normalize_a2b_direct1_idx(int a2b_idx, const InsertPairPrecalc &precalc, int qcnt) {
    int direct1_idx = precalc.next_a2b_effected_valid_q_idx[a2b_idx];
    if (direct1_idx == qcnt) return -1;
    return direct1_idx;
}

int normalize_a2b_direct2_idx(int direct1_idx, int a2b_idx, const InsertPairPrecalc &precalc, int qcnt) {
    if (direct1_idx == -1) return -1;
    int direct2_idx = precalc.next_ab_idxs[a2b_idx];
    if (direct2_idx == direct1_idx || direct2_idx == qcnt) return -1;
    return direct2_idx;
}

bool is_a2b_directly_related_to_q_idx(int a2b_idx, int q_idx, const InsertPairPrecalc &precalc, int qcnt) {
    int direct1_idx = normalize_a2b_direct1_idx(a2b_idx, precalc, qcnt);
    int direct2_idx = normalize_a2b_direct2_idx(direct1_idx, a2b_idx, precalc, qcnt);
    return direct1_idx == q_idx || direct2_idx == q_idx;
}

int calc_a2b_insert_and_direct_diff(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        AllOrd pushed_b_all_ord,
        const InsertPairPrecalc &precalc
) {
    const int N = yst.queries_state.current_N;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    DebugIns2Case case_info{a2b_idx, qcnt, pushed_b_all_ord.get()};
    DebugIns2A2BPlan a2b_plan = build_a2b_plan(
            v, yst, pushed_b_all_ord,
            precalc.exist_stack_a_ords,
            precalc.exist_stack_b_ords,
            precalc.prev_a2a_ranges,
            precalc.base_ra_prefix_sum_A2A,
            precalc.base_rra_prefix_sum_A2A,
            a2b_idx,
            N
    );

    int result = a2b_plan.bri.ins_turn;
    int direct1_idx = normalize_a2b_direct1_idx(a2b_idx, precalc, qcnt);
    int direct2_idx = normalize_a2b_direct2_idx(direct1_idx, a2b_idx, precalc, qcnt);

    result += calc_a2b_direct1_diff(
            direct1_idx, yst, init_st, yst.queries_state.queries, case_info, a2b_plan, N);

    QueryOrdEffect a2b_effect;
    a2b_effect.a_effect.set_del(a2b_plan.tar_all_ord_a);
    a2b_effect.b_effect.set_add(a2b_plan.tar_all_ord_b);
    a2b_effect.diff_stack_a_cnt = -1;
    result += calc_a2b_direct2_diff(
            direct1_idx, direct2_idx, yst, init_st, yst.queries_state.queries, case_info, a2b_plan,
            N, precalc.next_ab_idxs, a2b_effect,
            precalc.indirect_a2b_ra_prefix_sum_A2A,
            precalc.indirect_a2b_rra_prefix_sum_A2A,
            precalc
    );
    return result;
}

int calc_b2a_direct1_diff(
        const int b2a_direct1_idx,
        const YakinamashiState &yst, const State &init_st, const QRIList &queries,
        const DebugIns2Case &case_info, const DebugIns2B2APlan &b2a_plan,
        const QueryOrdEffect &net_effect, int N
) {
    const int qcnt = static_cast<int>(queries.size());
    if(b2a_direct1_idx == -1){
        return 0;
    }
    my_assert (b2a_direct1_idx < qcnt);
    const QueryCommonRI &qri = queries[b2a_direct1_idx];
    QueryProvider qprov(yst, init_st);
    QCRIFactory qfac;
    QueryCommon effected_q = qprov.get_applied_common(b2a_plan.finished_ord_pos_b, qri.get_query(), net_effect);
    // 責務: direct1 query の A 側開始 top OrdPos を、effect 後 A2A chain の replay 結果として求める。
    // 必要な理由: command no-cmd A2A でも finished_top_ord_a が変わる場合があり、B2A 直後 top 直指定では turn がずれるため。
    EffectedA2AChainEval chain_eval = build_effected_a2a_chain_eval(
            yst, init_st, queries, net_effect,
            b2a_plan.finished_ord_pos_a, b2a_plan.finished_ord_pos_b,
            case_info.b2a_idx, b2a_direct1_idx, N);
    OrdPos start_a = chain_eval.finished_top_a;
    if (qri.is_a2a_type()) {
        QueryCommonRI effected_qri = qfac.build_a2a_ri_from_start_top(
                start_a,
                effected_q);
        // _REVIEW_BEGIN: b2a direct1 の A2A 分岐だけが +1 ずれているかを出力から切り分けるため。
#ifdef DEBUG
        out("debug_b2a_direct1_calc",
            "a2b_idx", case_info.a2b_idx,
            "b2a_idx", case_info.b2a_idx,
            "direct1_idx", b2a_direct1_idx,
            "direct1_type", "A2A",
            "start_a", start_a.get(),
            "finished_b", b2a_plan.finished_ord_pos_b.get(),
            "new_turn", effected_qri.get_ins_turn(),
            "old_turn", qri.get_ins_turn(),
            "diff", effected_qri.get_ins_turn() - qri.get_ins_turn(),
            "tar_qri", indirect_debug_to_string(qri),
            "effected_q", effected_q);
#endif
        // _REVIEW_END
        return effected_qri.get_ins_turn() - qri.get_ins_turn();
    }
    int new_ins_turn = calc_debug_next_ins_turn_after_b2a(
            qri, effected_q, start_a, b2a_plan.finished_ord_pos_b, N, qfac);
    // _REVIEW_BEGIN: b2a direct1 の AB 分岐で新旧 turn と対象 query を確認するため。
#ifdef DEBUG
    out("debug_b2a_direct1_calc",
        "a2b_idx", case_info.a2b_idx,
        "b2a_idx", case_info.b2a_idx,
        "direct1_idx", b2a_direct1_idx,
        "direct1_type", "AB",
        "start_a", start_a.get(),
        "finished_b", b2a_plan.finished_ord_pos_b.get(),
        "new_turn", new_ins_turn,
        "old_turn", qri.get_ins_turn(),
        "diff", new_ins_turn - qri.get_ins_turn(),
        "tar_qri", indirect_debug_to_string(qri),
        "effected_q", effected_q);
#endif
    // _REVIEW_END
    return new_ins_turn - qri.get_ins_turn();
}


// 責務: 挿入 B2A 後に direct2 で再計算される query の差分 turn を返す。
// 必要な理由: next_ab_idxs を vec_array 化した後も B2A direct2 の境界探索意味を維持するため。
int calc_b2a_direct2_diff(
        const InsertPairPrecalc &prec,
        const int b2a_direct1_idx,
        const int b2a_direct2_idx,
        const YakinamashiState &yst, const State &init_st, const QRIList &queries,
        const DebugIns2Case &case_info, const DebugIns2B2APlan &b2a_plan, int N,
        const QueryBoundaryArray &next_ab_idx, const QueryOrdEffect &net_effect,
        const QueryPrefixArray &indirect_ra_prefix_sum_A2A,
        const QueryPrefixArray &indirect_rra_prefix_sum_A2A
) {
    const int qcnt = static_cast<int>(queries.size());
    if(b2a_direct2_idx == -1){
        return 0;
    }
    my_assert(case_info.b2a_idx < qcnt);

    QueryProvider qprov(yst, init_st);
    const int prev_valid_a_q_idx =  prec.prev_a2b_b2a_effected_valid_q_idx[b2a_direct2_idx];
    
    my_assert(prev_valid_a_q_idx >= case_info.b2a_idx);

    const QueryCommonRI &prev_qri = queries[prev_valid_a_q_idx];
    // _REVIEW_END
    const QueryCommonRI &tar_qri = queries[b2a_direct2_idx];
    QueryCommon effected_q = qprov.get_applied_common(b2a_plan.finished_ord_pos_b, tar_qri.get_query(), net_effect);
    OrdPos tar_a = get_ab_tar_ord_pos_a(tar_qri, effected_q);
    OrdPos tar_b = get_ab_tar_ord_pos_b(tar_qri, effected_q, N);
    // _REVIEW_BEGIN: effect 後 A2A の finished_top_a を direct2 の start_a として使うため。
    my_assert(queries[b2a_direct1_idx].is_a2a_type());
    //todo 懸念、O(1)で出来るのでは（ネックにならなそうなのでいったんほうち）
    // 責務: 挿入 B2A 後から direct2 直前までの連続 A2A 全体を replay し、direct2 の開始 top と同期可能回転量を求める。
    // 必要な理由: direct1 起点では direct1 より前の no-cmd A2A を取り落とし、prev_sum_ra/rra がずれるため。
    int a2a_begin_idx = b2a_direct2_idx - prec.prev_a2a_ranges[b2a_direct2_idx];
    a2a_begin_idx = std::max(a2a_begin_idx, case_info.b2a_idx);
    EffectedA2AChainEval chain_eval = build_effected_a2a_chain_eval(
            yst, init_st, queries, net_effect,
            b2a_plan.finished_ord_pos_a, b2a_plan.finished_ord_pos_b,
            a2a_begin_idx, b2a_direct2_idx, N);
    OrdPos start_a = chain_eval.finished_top_a;
    PrevA2ARotSum prev_sum = chain_eval.rot_sum;
    // _REVIEW_END
    BestRotInfo bri = build_brp_by_ord_with_prev_a2a(
            effected_q.get_stack_a_cnt(), effected_q.get_stack_b_cnt(N),
            start_a, b2a_plan.finished_ord_pos_b, tar_a, tar_b, prev_sum);
    // _REVIEW_BEGIN: b2a direct2 の +1 ずれ候補を prev_sum と target 座標込みで追うため。
#ifdef DEBUG
    // _REVIEW_BEGIN: 推定側の回転距離と選択 command を naive_qri の command 表示と機械的に比較するため。
    int stack_a_cnt = effected_q.get_stack_a_cnt();
    int stack_b_cnt = effected_q.get_stack_b_cnt(N);
    int ra_dis = RotCalculator::get_ra_dis(start_a, tar_a, stack_a_cnt);
    int rra_dis = RotCalculator::get_rra_dis(start_a, tar_a, stack_a_cnt);
    int rb_dis = RotCalculator::get_rb_dis(b2a_plan.finished_ord_pos_b, tar_b, stack_b_cnt);
    int rrb_dis = RotCalculator::get_rrb_dis(b2a_plan.finished_ord_pos_b, tar_b, stack_b_cnt);
    int rb_dis_after_prev_sum = std::max(0, rb_dis - prev_sum.ra_sum);
    int rrb_dis_after_prev_sum = std::max(0, rrb_dis - prev_sum.rra_sum);
    // _REVIEW_END
    out("debug_b2a_direct2_calc",
        "a2b_idx", case_info.a2b_idx,
        "b2a_idx", case_info.b2a_idx,
        "direct1_idx", b2a_direct1_idx,
        "direct2_idx", b2a_direct2_idx,
        "prev_valid_idx", prev_valid_a_q_idx,
        "start_a", start_a.get(),
        "finished_b", b2a_plan.finished_ord_pos_b.get(),
        "b2a_start_b", b2a_plan.start_ord_pos_b.get(),
        "b2a_tar_b", b2a_plan.tar_ord_pos_b.get(),
        "b2a_finished_b", b2a_plan.finished_ord_pos_b.get(),
        "tar_a", tar_a.get(),
        "tar_b", tar_b.get(),
        "stack_a_cnt", stack_a_cnt,
        "stack_b_cnt", stack_b_cnt,
        "target_stack_b_cnt", stack_b_cnt,
        "target_tar_b", tar_b.get(),
        "ra_dis", ra_dis,
        "rra_dis", rra_dis,
        "rb_dis", rb_dis,
        "rrb_dis", rrb_dis,
        "target_rb_from_finished_b", rb_dis,
        "target_rrb_from_finished_b", rrb_dis,
        "rb_dis_after_prev_sum", rb_dis_after_prev_sum,
        "rrb_dis_after_prev_sum", rrb_dis_after_prev_sum,
        "prev_sum_ra", prev_sum.ra_sum,
        "prev_sum_rra", prev_sum.rra_sum,
        "est_cmd_a", bri.cmd_a,
        "est_cmd_b", bri.cmd_b,
        "est_cmd_a_cnt", bri.cmd_a_cnt,
        "est_cmd_b_cnt", bri.cmd_b_cnt,
        "new_turn", bri.ins_turn,
        "old_turn", tar_qri.get_ins_turn(),
        "diff", bri.ins_turn - tar_qri.get_ins_turn(),
        "prev_qri", indirect_debug_to_string(prev_qri),
        "tar_qri", indirect_debug_to_string(tar_qri),
        "effected_q", effected_q);
#endif
    // _REVIEW_END
    return bri.ins_turn - tar_qri.get_ins_turn();
}

int calc_b2a_insert_and_direct_cost(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int b2a_idx,
        AllOrd pushed_b_all_ord,
        const InsertPairPrecalc &precalc
) {
    constexpr int DEBUG_A2B_IDX_FOR_B2A_COST = 0;
    const int fast_cost = calc_b2a_insert_and_direct_cost(
            yst, init_st, v, DEBUG_A2B_IDX_FOR_B2A_COST, b2a_idx, pushed_b_all_ord, precalc);

#ifdef DEBUG
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    int direct1_idx = normalize_b2a_direct1_idx(b2a_idx, precalc, qcnt);
    int direct2_idx = normalize_b2a_direct2_idx(direct1_idx, b2a_idx, precalc, qcnt);
    const bool can_check_with_a2b0 =
            !is_a2b_directly_related_to_q_idx(DEBUG_A2B_IDX_FOR_B2A_COST, b2a_idx, precalc, qcnt);

    if (can_check_with_a2b0) {
        YakinamashiState eval_yst = build_eval_state(yst, v, pushed_b_all_ord);
        insert_eval_a2b(eval_yst, init_st, v, DEBUG_A2B_IDX_FOR_B2A_COST);
        int shifted_b2a_idx = b2a_idx;
        if (DEBUG_A2B_IDX_FOR_B2A_COST <= b2a_idx) shifted_b2a_idx++;
        insert_eval_b2a(eval_yst, init_st, v, shifted_b2a_idx);

        int naive_cost = eval_yst.queries_state.queries[shifted_b2a_idx].get_ins_turn();
        auto add_direct_diff = [&](int direct_idx) {
            if (direct_idx == -1) return;
            my_assert(0 <= direct_idx && direct_idx < qcnt);
            int shifted_idx = direct_idx;
            if (DEBUG_A2B_IDX_FOR_B2A_COST <= direct_idx) shifted_idx++;
            if (b2a_idx <= direct_idx) shifted_idx++;
            my_assert(0 <= shifted_idx && shifted_idx < static_cast<int>(eval_yst.queries_state.queries.size()));
            naive_cost += eval_yst.queries_state.queries[shifted_idx].get_ins_turn()
                          - yst.queries_state.queries[direct_idx].get_ins_turn();
        };
        add_direct_diff(direct1_idx);
        add_direct_diff(direct2_idx);
        if (fast_cost != naive_cost) {
            out("b2a_cost_limit_fast_naive_mismatch",
                "v", v,
                "a2b_idx", DEBUG_A2B_IDX_FOR_B2A_COST,
                "b2a_idx", b2a_idx,
                "shifted_b2a_idx", shifted_b2a_idx,
                "pushed_b_all_ord", pushed_b_all_ord.get(),
                "direct1_idx", direct1_idx,
                "direct2_idx", direct2_idx,
                "fast_cost", fast_cost,
                "naive_cost", naive_cost);
            my_assert(false);
        }
    }
#endif
    return fast_cost;
}

int calc_b2a_insert_and_direct_cost(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        int b2a_idx,
        AllOrd pushed_b_all_ord,
        const InsertPairPrecalc &precalc
) {
    const int N = yst.queries_state.current_N;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    DebugIns2Case case_info{a2b_idx, b2a_idx, pushed_b_all_ord.get()};
    DebugIns2A2BPlan a2b_plan = build_a2b_plan(
            v, yst, pushed_b_all_ord,
            precalc.exist_stack_a_ords,
            precalc.exist_stack_b_ords,
            precalc.prev_a2a_ranges,
            precalc.base_ra_prefix_sum_A2A,
            precalc.base_rra_prefix_sum_A2A,
            a2b_idx,
            N
    );
    DebugIns2B2APlan b2a_plan = build_b2a_plan(
            v, yst, init_st, case_info, a2b_plan,
            precalc.exist_stack_a_ords,
            precalc.exist_stack_b_ords,
            precalc.prev_a2a_ranges,
            precalc.indirect_a2b_ra_prefix_sum_A2A,
            precalc.indirect_a2b_rra_prefix_sum_A2A,
            N,
            precalc
    );
    QueryOrdEffect net_effect = build_a2b_b2a_net_exist_effect(a2b_plan, b2a_plan);
    int direct1_idx = normalize_b2a_direct1_idx(b2a_idx, precalc, qcnt);
    int direct2_idx = normalize_b2a_direct2_idx(direct1_idx, b2a_idx, precalc, qcnt);

    int result = b2a_plan.bri.ins_turn;
    result += calc_b2a_direct1_diff(
            direct1_idx, yst, init_st, yst.queries_state.queries, case_info, b2a_plan, net_effect, N);
    result += calc_b2a_direct2_diff(
            precalc, direct1_idx, direct2_idx, yst, init_st, yst.queries_state.queries, case_info, b2a_plan,
            N, precalc.next_ab_idxs, net_effect,
            precalc.indirect_a2b_b2a_ra_prefix_sum_A2A,
            precalc.indirect_a2b_b2a_rra_prefix_sum_A2A
    );
    return result;
}

// _REVIEW_BEGIN: debug 本体を Helper 内へ移し、外側メンバは末尾で委譲だけを行う。
    void debug_ins2_impl(const State &init_st,
                                          int case_n, const YakinamashiState &yst,
                                          const my_vector<std::pair<int, int>> &destroyed_vs
) {
    // _REVIEW_BEGIN: debug 関数のファイル初期化とログ生成を DEBUG ビルドだけに限定するため。
#ifndef DEBUG
    return;
#endif
    // _REVIEW_END
    log_indirect_case_snapshot(init_st, yst, destroyed_vs);
    auto sorted_vs = sort_destroyed_vs_by_cost_desc(destroyed_vs);
    for (const auto &[v, destroyed_cost]: sorted_vs) {
        for (int case_i = 0; case_i < case_n; case_i++) {
            clear_file();
            YakinamashiState case_yst = yst;
            const int qcnt = static_cast<int>(case_yst.queries_state.queries.size());
            auto &border = case_yst.query_order.border;
            const InsertPairPrecalc &prec = build_construct_v_precalc(init_st, case_yst, v);
            const auto &exist_a_ords_before_q_idx = prec.exist_stack_a_ords;
            const auto &exist_b_ords_before_q_idx = prec.exist_stack_b_ords;
            const auto &a2b_band_ranges = prec.a2b_band_ranges;
            const auto &fixed_indirect_a2b_prefix_sum = prec.fixed_indirect_a2b_prefix_sum;
            const auto &fixed_indirect_a2b_b2a_prefix_sum = prec.fixed_indirect_a2b_b2a_prefix_sum;
            const auto &base_ra_prefix_sum_A2A = prec.base_ra_prefix_sum_A2A;
            const auto &base_rra_prefix_sum_A2A = prec.base_rra_prefix_sum_A2A;
            const auto &indirect_a2b_ra_prefix_sum_A2A = prec.indirect_a2b_ra_prefix_sum_A2A;
            const auto &indirect_a2b_rra_prefix_sum_A2A = prec.indirect_a2b_rra_prefix_sum_A2A;
            const auto &indirect_a2b_b2a_ra_prefix_sum_A2A = prec.indirect_a2b_b2a_ra_prefix_sum_A2A;
            const auto &indirect_a2b_b2a_rra_prefix_sum_A2A = prec.indirect_a2b_b2a_rra_prefix_sum_A2A;
            int N = case_yst.queries_state.current_N;
            const auto &next_ab_idxs = prec.next_ab_idxs;
            const auto &prev_a2a_ranges = prec.prev_a2a_ranges;
            int direct_final_rot_diff = prec.direct_final_rot_diff;
            int indirect_final_rot_diff = prec.indirect_final_rot_diff;
            const QRIList &queries = case_yst.queries_state.queries;
            DebugIns2Case case_info = sample_debug_ins2_case(qcnt, border);
            bool use_direct_final_rot;
            if(case_info.b2a_idx == qcnt){
                use_direct_final_rot = true;
            }else{
                my_assert(case_info.b2a_idx < prec.next_a2b_b2a_effected_valid_q_idx.size());
                use_direct_final_rot = prec.next_a2b_b2a_effected_valid_q_idx[case_info.b2a_idx] == qcnt;
            }
            int res_diff = indirect_final_rot_diff;
            if (use_direct_final_rot) {
                res_diff = direct_final_rot_diff;
            }
            int a2b_direct1_idx = prec.next_a2b_effected_valid_q_idx[case_info.a2b_idx];
            int a2b_direct2_idx = next_ab_idxs[case_info.a2b_idx];
            //a2bがb2aに当たる場合は、 b2aの手数自体の計算の時にやるので、 ぶつかるケースをdirect1と考えなくていい
            if(a2b_direct1_idx == qcnt){
                a2b_direct1_idx = -1;
                a2b_direct2_idx = -1;
            }else if (a2b_direct2_idx == a2b_direct1_idx) {
                a2b_direct2_idx = -1;
            }else if(a2b_direct2_idx == qcnt){
                a2b_direct2_idx = -1;
            }
            int b2a_direct1_idx = prec.next_a2b_b2a_effected_valid_q_idx[case_info.b2a_idx];
            int b2a_direct2_idx = next_ab_idxs[case_info.b2a_idx];
            if(b2a_direct1_idx == qcnt){
                b2a_direct1_idx = -1;
                b2a_direct2_idx = -1;
            }else if (b2a_direct2_idx == b2a_direct1_idx) {
                b2a_direct2_idx = -1;
            }else if(b2a_direct2_idx == qcnt){
                b2a_direct2_idx = -1;
            }
            int mid_indirect_diff = calc_debug_mid_indirect_sum(
                    fixed_indirect_a2b_prefix_sum, a2b_band_ranges, case_info,
                    a2b_direct1_idx, a2b_direct2_idx);
            int tail_indirect_diff = calc_tail_indirect_sum(
                    fixed_indirect_a2b_b2a_prefix_sum, case_info, b2a_direct1_idx, b2a_direct2_idx, qcnt);
            res_diff += mid_indirect_diff;
            res_diff += tail_indirect_diff;
            DebugIns2A2BPlan a2b_plan = build_a2b_plan(
                    v, case_yst, AllOrd(case_info.v_all_ord_b), exist_a_ords_before_q_idx, exist_b_ords_before_q_idx,
                    prev_a2a_ranges, base_ra_prefix_sum_A2A, base_rra_prefix_sum_A2A, case_info.a2b_idx, N);
            res_diff += a2b_plan.bri.ins_turn;
            DebugIns2B2APlan b2a_plan = build_b2a_plan(
                    v, case_yst, init_st, case_info, a2b_plan, exist_a_ords_before_q_idx, exist_b_ords_before_q_idx,
                    prev_a2a_ranges,
                    indirect_a2b_ra_prefix_sum_A2A, indirect_a2b_rra_prefix_sum_A2A, N, prec);
            res_diff += b2a_plan.bri.ins_turn;


            QueryOrdEffect a2b_effect;
            a2b_effect.a_effect.set_del(a2b_plan.tar_all_ord_a);
            a2b_effect.b_effect.set_add(a2b_plan.tar_all_ord_b);
            a2b_effect.diff_stack_a_cnt = -1;

            QueryOrdEffect net_effect = build_a2b_b2a_net_exist_effect(a2b_plan, b2a_plan);

            int a2b_direct1_diff = calc_a2b_direct1_diff(
                    a2b_direct1_idx,
                    case_yst, init_st, queries, case_info, a2b_plan, N);
            res_diff += a2b_direct1_diff;
            int a2b_direct2_diff = calc_a2b_direct2_diff(
                a2b_direct1_idx,
                    a2b_direct2_idx,
                    case_yst, init_st, queries, case_info, a2b_plan, N, next_ab_idxs, a2b_effect,
                    indirect_a2b_ra_prefix_sum_A2A, indirect_a2b_rra_prefix_sum_A2A, prec);
            res_diff += a2b_direct2_diff;
            int b2a_direct1_diff = calc_b2a_direct1_diff(
                    b2a_direct1_idx,
                    case_yst, init_st, queries, case_info, b2a_plan, net_effect, N);
            res_diff += b2a_direct1_diff;
            int b2a_direct2_diff = calc_b2a_direct2_diff(
                    prec,
                    b2a_direct1_idx,
                    b2a_direct2_idx,
                    case_yst, init_st, queries, case_info, b2a_plan, N, next_ab_idxs, net_effect,
                    indirect_a2b_b2a_ra_prefix_sum_A2A, indirect_a2b_b2a_rra_prefix_sum_A2A);
            res_diff += b2a_direct2_diff;
            DebugIns2NaiveResult naive = build_debug_ins2_naive_result(case_yst, init_st, v, case_info);
            assert_debug_ins2_final_rot_diff(
                    v, case_info, qcnt, use_direct_final_rot,
                    direct_final_rot_diff, indirect_final_rot_diff, naive.final_rot_diff,
                    prec, queries);
            assert_debug_ins2_a2b_insert_turn(v, case_info, a2b_plan, naive);
            assert_debug_ins2_b2a_insert_turn(v, case_info, b2a_plan, naive, N);
            // _REVIEW_BEGIN: q_idx diff assert も direct 実対象へ合わせる。
            if (case_info.a2b_idx <= a2b_direct1_idx && a2b_direct1_idx < case_info.b2a_idx) {
                assert_debug_ins2_q_idx_diff(
                        v, case_info, naive, queries, a2b_direct1_idx,
                        a2b_direct1_diff, "a2b_direct1");
            }
            if (case_info.a2b_idx < a2b_direct2_idx && a2b_direct2_idx < case_info.b2a_idx) {
                assert_debug_ins2_q_idx_diff(
                        v, case_info, naive, queries, a2b_direct2_idx,
                        a2b_direct2_diff, "a2b_direct2");
            }
            if (case_info.b2a_idx <= b2a_direct1_idx && b2a_direct1_idx < qcnt) {
                assert_debug_ins2_q_idx_diff(
                        v, case_info, naive, queries, b2a_direct1_idx,
                        b2a_direct1_diff, "b2a_direct1");
            }
            if (case_info.b2a_idx < b2a_direct2_idx && b2a_direct2_idx < qcnt) {
                assert_debug_ins2_q_idx_diff(
                        v, case_info, naive, queries, b2a_direct2_idx,
                        b2a_direct2_diff, "b2a_direct2");
            }
            vec_array<int, 2> mid_exclude_q_idxs;
            if (case_info.a2b_idx <= a2b_direct1_idx && a2b_direct1_idx < case_info.b2a_idx) {
                mid_exclude_q_idxs.push_back(a2b_direct1_idx);
            }
            if (case_info.a2b_idx <= a2b_direct2_idx && a2b_direct2_idx < case_info.b2a_idx) {
                mid_exclude_q_idxs.push_back(a2b_direct2_idx);
            }
            debug_indirect_range_sum(
                    v, case_info, naive, queries, fixed_indirect_a2b_prefix_sum, a2b_band_ranges,
                    case_info.a2b_idx, case_info.b2a_idx,
                    mid_exclude_q_idxs, mid_indirect_diff, "mid_indirect");
            vec_array<int, 2> tail_exclude_q_idxs;
            if (case_info.b2a_idx <= b2a_direct1_idx && b2a_direct1_idx < qcnt) {
                tail_exclude_q_idxs.push_back(b2a_direct1_idx);
            }
            if (case_info.b2a_idx <= b2a_direct2_idx && b2a_direct2_idx < qcnt) {
                tail_exclude_q_idxs.push_back(b2a_direct2_idx);
            }
            debug_fixed_range_sum(
                    v, case_info, naive, queries,
                    case_info.b2a_idx, qcnt,
                    tail_exclude_q_idxs, tail_indirect_diff, "tail_indirect");
            assert_debug_ins2_total_diff(v, case_info, res_diff, naive.total_diff);
        }
    }
    }
// _REVIEW_END

//------------

void debug_run_a2b_indirect_q_idx_checks_impl(
        const State &init_st,
        const YakinamashiState &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    // _REVIEW_BEGIN: debug 検証入口のログ生成と検証計算を DEBUG ビルドだけに限定するため。
#ifndef DEBUG
    return;
#endif
    // _REVIEW_END
    const int n = yst.queries_state.current_N;
    log_indirect_case_snapshot(init_st, yst, destroyed_vs);
    auto sorted_vs = sort_destroyed_vs_by_cost_desc(destroyed_vs);
    for (const auto &[v, destroyed_cost]: sorted_vs) {
        (void) destroyed_cost;
        validate_destroyed_v(v, init_st.initial_N);
        out("");
        out("=== 値vの検証開始 ===");
        out("v", v,
            "destroyed_cost", destroyed_cost,
            "deleted_init_all_ord_a", yst.query_order.aorder.get_init_all_order(v).get());
        const int qcnt = static_cast<int>(yst.queries_state.queries.size());
        const auto &border = yst.query_order.border;
        my_assert(MAX_QUERY_CNT >= qcnt);
        my_assert(MAX_INSERT_EVENT_B >= border.get_all_order_entries_size() + 2);
        static array<vector<int>, MAX_INSERT_EVENT_B> start_q_b_segment;
        static array<vector<int>, MAX_INSERT_EVENT_B> end_q_b_segment;
        static array<vector<IndirectTask>, MAX_INSERT_EVENT_B> indirect_tasks_by_b_all_ord;
        clear_band_buffers(border.get_all_order_entries_size() + 2, start_q_b_segment, end_q_b_segment,
                           indirect_tasks_by_b_all_ord);
        QueryBandRangeArray a2b_band_ranges;
        make_query_band_range_array(a2b_band_ranges, qcnt);
        vector<int> init_a_pos = build_init_a_pos(init_st);
        QueryIntArray indirect_a2b_ra_A2A = make_query_int_array(qcnt);
        QueryIntArray indirect_a2b_rra_A2A = make_query_int_array(qcnt);
        static QueryExistAArray exist_a_ords_before_q_idx;
        static QueryExistBArray exist_b_ords_before_q_idx;
        build_exist_all_ords_before_q_idx(yst, init_st, exist_a_ords_before_q_idx, exist_b_ords_before_q_idx);
        // _REVIEW_BEGIN: build_fixed_a2b_indirect_diffs が command no-cmd 透明化用の有効境界配列を返すようになったため、debug 呼び出し側でも受け皿を用意する。
        QueryBoundaryArray prev_a2b_effected_valid_q_idx;
        QueryIntArray next_a2b_effected_valid_q_idx;
        // _REVIEW_END
        QueryOrdPosArray a2b_effected_finished_top_ord_a;
        auto [is_only_direct, fixed_indirect_range_diff] = build_fixed_a2b_indirect_diffs(
                init_a_pos,
                v, n, init_st, yst, start_q_b_segment, end_q_b_segment, a2b_band_ranges,
                indirect_a2b_ra_A2A,
                indirect_a2b_rra_A2A,
                exist_a_ords_before_q_idx,
                exist_b_ords_before_q_idx,
                prev_a2b_effected_valid_q_idx,
                next_a2b_effected_valid_q_idx,
                a2b_effected_finished_top_ord_a
        );
        run_indirect_q_idx_checks(is_only_direct, v, yst, init_st, fixed_indirect_range_diff, a2b_band_ranges);
        out("=== 値vの検証終了 ===", "v", v);
    }
    }
// _REVIEW_END

// _REVIEW_BEGIN: debug 本体を Helper 内へ移し、外側メンバは末尾で委譲だけを行う。
    void debug_dump_a2b_b2a_indirect_diffs_impl(
        const State &init_st,
        const YakinamashiState &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    // _REVIEW_BEGIN: debug dump 入口のログ生成と検証計算を DEBUG ビルドだけに限定するため。
#ifndef DEBUG
    return;
#endif
    // _REVIEW_END
    const int n = yst.queries_state.current_N;
    log_indirect_case_snapshot(init_st, yst, destroyed_vs);
    auto sorted_vs = sort_destroyed_vs_by_cost_desc(destroyed_vs);
    for (const auto &[v, destroyed_cost]: sorted_vs) {
        (void) destroyed_cost;
        validate_destroyed_v(v, init_st.initial_N);
        const int qcnt = static_cast<int>(yst.queries_state.queries.size());
        static array<vector<int>, MAX_INSERT_EVENT_B> start_q_b_segment;
        static array<vector<int>, MAX_INSERT_EVENT_B> end_q_b_segment;
        QueryBandRangeArray a2b_band_ranges;
        make_query_band_range_array(a2b_band_ranges, qcnt);
        vector<int> init_a_pos = build_init_a_pos(init_st);
        QueryIntArray indirect_a2b_b2a_ra_A2A = make_query_int_array(qcnt);
        QueryIntArray indirect_a2b_b2a_rra_A2A = make_query_int_array(qcnt);
        QueryIntArray indirect_a2b_ra_A2A = make_query_int_array(qcnt);
        QueryIntArray indirect_a2b_rra_A2A = make_query_int_array(qcnt);
        QueryCommonArray effected_a2b_b2a_queries;
        OrdPos indirect_final_top_ord_a = OrdPos(0);
        static QueryExistAArray exist_a_ords_before_q_idx;
        static QueryExistBArray exist_b_ords_before_q_idx;
        build_exist_all_ords_before_q_idx(yst, init_st, exist_a_ords_before_q_idx, exist_b_ords_before_q_idx);
        // _REVIEW_BEGIN: build_fixed_a2b_indirect_diffs の有効境界前計算を debug_dump 側でも同期させる。
        QueryBoundaryArray prev_a2b_effected_valid_q_idx;
        QueryIntArray next_a2b_effected_valid_q_idx;
        // _REVIEW_END
        QueryOrdPosArray a2b_effected_finished_top_ord_a;
        auto [is_only_direct, fixed_indirect_a2b_range_diff] = build_fixed_a2b_indirect_diffs(
                init_a_pos,
                v, n, init_st, yst, start_q_b_segment, end_q_b_segment, a2b_band_ranges,
                indirect_a2b_ra_A2A, indirect_a2b_rra_A2A,
                exist_a_ords_before_q_idx, exist_b_ords_before_q_idx,
                prev_a2b_effected_valid_q_idx, next_a2b_effected_valid_q_idx,
                a2b_effected_finished_top_ord_a
        );

        auto [_, fixed_indirect_a2b_b2a_range_diff] = build_fixed_a2b_b2a_indirect_diffs(
                init_a_pos, v, n, init_st, yst, start_q_b_segment, end_q_b_segment,
                indirect_a2b_b2a_ra_A2A, indirect_a2b_b2a_rra_A2A,
                effected_a2b_b2a_queries,
                indirect_final_top_ord_a,
                exist_a_ords_before_q_idx, exist_b_ords_before_q_idx

        );
        QueryBandRangeArray no_band_ranges;
        make_query_band_range_array(no_band_ranges, qcnt);
        run_a2b_b2a_indirect_q_idx_checks(
                is_only_direct, v, yst, init_st, fixed_indirect_a2b_b2a_range_diff, no_band_ranges);
    }
    }
    void construct_impl(
        const State &init_st,
        YakinamashiState &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs,
        const GreedyConstructorParams &params
) {
    my_bitset<MAX_N> is_never_construct_v;
    for (const auto &[v, destroyed_cost]: destroyed_vs) {
        (void) destroyed_cost;
        is_never_construct_v.set(v);
    }
    for (const auto &[v, destroyed_cost]: destroyed_vs) {
        (void) destroyed_cost;
        InsertPairPrecalc &prec = build_construct_v_precalc(init_st, yst, v);
//        BasePairCandidates candidates = collect_candidates(yst, init_st, v, prec, params);
        BasePairCandidates &candidates = base_pair_candidates_workspace();
        candidates.clear();
        collect_candidates_owner_range(yst, init_st, v, prec, params, is_never_construct_v, candidates);
    }
    }
};

template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::use_a_order_gap_priority = false;

template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::use_owner_collector = false;

template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::use_owner_check = true;

template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::use_owner_unit_range = true;

template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::use_owner_even_unit = false;

template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::use_pair_refine = false;

template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::pair_refine_top_k = 6;

template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::pair_refine_a2b_idx_range = 3;

template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::pair_refine_b2a_idx_range = 3;

template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::pair_refine_all_ord_range = 3;

template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::use_owner_array_range = false;

template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::use_timing_log = false;

template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::timing_iter = -1;

template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::timing_target_iter = -1;

template<size_t MAX_N>
GreedyQueriesConstructor<MAX_N>::GreedyQueriesConstructor(
        const State &init_st,
        const GreedyConstructorParams &params,
        CollectorLogMode collector_log_mode
)
        : init_st(init_st),
          params(params),
          collector_log_mode(collector_log_mode)
{
}

// 責務: 今回の construct でまだ構築されていない v を O(1) 判定できる形で保持する。
// 必要な理由: A 側 from/to ext の実歩き中に、未構築 destroyed v を 0 消費扱いで判定するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::reset_never_construct_vs(
        const my_vector<std::pair<int, int>> &target_vs
) {
    is_never_construct_v.reset();
    for (const auto &[v, destroyed_cost]: target_vs) {
        (void) destroyed_cost;
        is_never_construct_v.set(v);
    }
}

// 責務: construct 済みになった v を未構築 destroyed v 集合から外す。
// 必要な理由: 後続 v の A 側 from/to ext では、構築済み v を 0 消費扱いしないため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::mark_constructed_v(int v) {
    is_never_construct_v.reset(v);
}

// 責務: use_a_order_gap_priority を enabled に更新する。
// 必要な理由: 実験入口から destroyed_cost 順と AOrder gap 順を切り替えられるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_a_order_gap_priority_enabled(bool enabled) {
    use_a_order_gap_priority = enabled;
}

// 責務: use_a_order_gap_priority の現在値を返す。
// 必要な理由: debug/test 側で実行時の順序モードを確認できるようにするため。
template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::is_a_order_gap_priority_enabled() {
    return use_a_order_gap_priority;
}

// 責務: 通常 query/physical construct で使う collector mode を更新する。
// 必要な理由: 同じ init_state と seed で旧 collector と owner collector の score を比較するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(bool enabled) {
    use_owner_collector = enabled;
}

// 責務: owner collector 実行時に旧 collector best と違えば abort するかを更新する。
// 必要な理由: 100 case 平均比較では差分があっても停止せず score を集計する必要があるため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(bool enabled) {
    use_owner_check = enabled;
}

// 責務: owner range 登録を all_ord 1 点単位に分解するかを更新する。
// 必要な理由: 同じ init_state と seed で unit 登録あり/なしの最終 score 差を比較するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(bool enabled) {
    use_owner_unit_range = enabled;
}

// 責務: owner unit 登録時に奇数 all_ord を登録しない実験フラグを更新する。
// 必要な理由: even unit sampling と top pair refine の速度・score 影響を比較できるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_owner_even_unit_enabled(bool enabled) {
    use_owner_even_unit = enabled;
}

// 責務: Mo 評価後の top pair 周辺探索を有効化する。
// 必要な理由: even unit sampling で落とした奇数 all_ord 近傍を後段で補えるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_pair_refine_enabled(bool enabled) {
    use_pair_refine = enabled;
}

// 責務: refine で見る top 数と a2b_idx/b2a_idx/all_ord の範囲を保持する。
// 必要な理由: 実験入口から探索量を調整できるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_pair_refine_params(
        int top_k,
        int a2b_idx_range,
        int b2a_idx_range,
        int all_ord_range
) {
    my_assert(0 <= top_k);
    my_assert(0 <= a2b_idx_range);
    my_assert(0 <= b2a_idx_range);
    my_assert(0 <= all_ord_range);
    pair_refine_top_k = top_k;
    pair_refine_a2b_idx_range = a2b_idx_range;
    pair_refine_b2a_idx_range = b2a_idx_range;
    pair_refine_all_ord_range = all_ord_range;
}

// 責務: owner collector の overlap 検索に array 経路を使うかを更新する。
// 必要な理由: 同じ init_state と seed で map 経路と array 経路の速度・score を比較するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_owner_array_enabled(bool enabled) {
    use_owner_array_range = enabled;
}

// 責務: construct timing 専用ログの有効/無効を切り替え、有効化時にファイルを初期化する。
// 必要な理由: 重い case だけ全段階 timing を後から見返せるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_timing_log_enabled(bool enabled) {
    use_timing_log = enabled;
    std::ofstream &log_file = construct_timing_log_stream();
    if (log_file.is_open()) log_file.close();
    if (!enabled) return;
    if (!output_dir.empty()) {
        std::filesystem::create_directories(output_dir);
        log_file.open(output_dir / "construct_timing_log.txt", std::ios::out | std::ios::trunc);
    } else {
        log_file.open("construct_timing_log.txt", std::ios::out | std::ios::trunc);
    }
    use_timing_log = log_file.is_open();
    if (is_timing_log_enabled()) {
        log_file << "construct_timing_log_begin" << std::endl;
    }
}

// 責務: construct timing 専用ログが有効か返す。
// 必要な理由: timing 無効時にログ文字列構築コストも避けるため。
template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::is_timing_log_enabled() {
    return use_timing_log && (timing_target_iter < 0 || timing_iter == timing_target_iter);
}

// 責務: 現在の ALNS iter index を timing log の共通 prefix 用に保持する。
// 必要な理由: construct 内部ログにも外側の iter を付けて見返せるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_timing_iter(int iter) {
    timing_iter = iter;
}

// 責務: timing log を実際に出す ALNS iter index を保持する。
// 必要な理由: 重い iter だけ詳細ログを出し、ログ出力自体の負荷を抑えるため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_timing_target_iter(int iter) {
    timing_target_iter = iter;
}

// 責務: timing log の 1 行を専用ファイルへ flush 付きで出力する。
// 必要な理由: 重い処理中に停止しても最後に到達した stage をファイルから確認できるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::write_timing_log(const std::string &line) {
    if (!is_timing_log_enabled()) return;
    std::ofstream &log_file = construct_timing_log_stream();
    if (!log_file.is_open()) return;
    log_file << "iter=" << timing_iter << ' ' << line << std::endl;
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::set_trace_enabled(bool enabled) {
    Helper::instance().g_trace_enabled = enabled;
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::clear_trace_log() {
    // _REVIEW_BEGIN: trace ファイル削除も DEBUG ビルドだけに限定するため。
#ifndef DEBUG
    return;
#endif
    // _REVIEW_END
    std::remove(Helper::GREEDY_TRACE_PATH);
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::append_trace_marker(const std::string &line) {
    Helper::instance().append_trace_line(line);
}

template<size_t MAX_N>
const my_vector<std::string> &GreedyQueriesConstructor<MAX_N>::get_last_construct_output_lines() {
    return Helper::instance().g_last_construct_output_lines;
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::nieve_construct(
        YakinamashiState<MAX_N> &yst, const my_vector<std::pair<int, int>> &destroyed_vs
) {
    (void) yst;
    (void) destroyed_vs;
    my_assert(false);
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct(
        YakinamashiState<MAX_N> &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    construct_only_query_destroyed(yst, destroyed_vs);
}

// 責務: ordered_vs の順に fixed B mode または通常の only-query 1-v construct を呼ぶ。
// 必要な理由: 順序決定と 1-v construct 呼び出しを分離し、cost 順/gap 順の切替を入口側だけで扱うため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_ordered_vs(
        YakinamashiState<MAX_N> &yst,
        const my_vector<std::pair<int, int>> &ordered_vs
) {
    Helper &helper = Helper::instance();
    const bool use_fixed_b_mode = helper.pick_fixed_b_mode();
    const auto timing_start = std::chrono::steady_clock::now();
    if (is_timing_log_enabled()) {
        std::ostringstream ss;
        ss << "stage=construct_ordered_begin"
           << " v_count=" << ordered_vs.size()
           << " qcnt=" << yst.queries_state.queries.size()
           << " sum_turn=" << yst.queries_state.sum_turn
           << " fixed_b=" << use_fixed_b_mode;
        write_timing_log(ss.str());
    }
    reset_never_construct_vs(ordered_vs);
    for (const auto &[v, destroyed_cost]: ordered_vs) {
        (void) destroyed_cost;
        if (use_fixed_b_mode) {
            construct_v_fixed_b(yst, init_st, v);
            mark_constructed_v(v);
            continue;
        }
        construct_v_only_query_destroyed(init_st, yst, v);
        mark_constructed_v(v);
    }
    if (is_timing_log_enabled()) {
        const auto timing_end = std::chrono::steady_clock::now();
        const auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(timing_end - timing_start).count();
        std::ostringstream ss;
        ss << "stage=construct_ordered_end"
           << " v_count=" << ordered_vs.size()
           << " qcnt=" << yst.queries_state.queries.size()
           << " sum_turn=" << yst.queries_state.sum_turn
           << " elapsed_ms=" << elapsed_ms;
        write_timing_log(ss.str());
    }
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_only_query_destroyed(
        YakinamashiState<MAX_N> &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    Helper &helper = Helper::instance();
    helper.apply_collector_log_mode(collector_log_mode);
    //Optuna helper.log_indirect_case_snapshot(init_st, yst, destroyed_vs);
    my_vector<std::pair<int, int>> ordered_vs = helper.sort_destroyed_vs_for_construct(
            yst, destroyed_vs, init_st.initial_N);
    construct_ordered_vs(yst, ordered_vs);
}

// 責務: ordered_vs の順に fixed B mode または通常の physical 1-v construct を呼ぶ。
// 必要な理由: physical 経路でも順序決定と 1-v construct 呼び出しを分離し、only-query と同じ順序切替を使うため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_physical_ordered_vs(
        PhysicalDestroyContext<MAX_N> &context,
        const my_vector<std::pair<int, int>> &ordered_vs
) {
    Helper &helper = Helper::instance();
    const bool use_fixed_b_mode = helper.pick_fixed_b_mode();
    const auto timing_start = std::chrono::steady_clock::now();
    if (is_timing_log_enabled()) {
        std::ostringstream ss;
        ss << "stage=construct_physical_ordered_begin"
           << " v_count=" << ordered_vs.size()
           << " qcnt=" << context.yst.queries_state.queries.size()
           << " sum_turn=" << context.yst.queries_state.sum_turn
           << " fixed_b=" << use_fixed_b_mode;
        write_timing_log(ss.str());
    }
    reset_never_construct_vs(ordered_vs);
    for (const auto &[v, destroyed_cost]: ordered_vs) {
        (void) destroyed_cost;
        if (use_fixed_b_mode) {
            construct_v_physical_fixed_b(context, v);
            mark_constructed_v(v);
            continue;
        }
        construct_v_physical_destroyed(context, v);
        mark_constructed_v(v);
    }
    if (is_timing_log_enabled()) {
        const auto timing_end = std::chrono::steady_clock::now();
        const auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(timing_end - timing_start).count();
        std::ostringstream ss;
        ss << "stage=construct_physical_ordered_end"
           << " v_count=" << ordered_vs.size()
           << " qcnt=" << context.yst.queries_state.queries.size()
           << " sum_turn=" << context.yst.queries_state.sum_turn
           << " elapsed_ms=" << elapsed_ms;
        write_timing_log(ss.str());
    }
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_physical_destroyed(
        PhysicalDestroyContext<MAX_N> &context,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    Helper &helper = Helper::instance();
    helper.apply_collector_log_mode(collector_log_mode);
    my_vector<std::pair<int, int>> ordered_vs = helper.sort_destroyed_vs_for_construct(
            context.yst, destroyed_vs, init_st.initial_N);
    helper.log_construct_physical_begin(context, ordered_vs);
    construct_physical_ordered_vs(context, ordered_vs);
    helper.log_construct_physical_end(init_st, context);
}

// 責務: ordered_vs の順に、physical 割当 v は State 復元込み、query 割当 v は query のみを construct する。
// 必要な理由: physical 側と query 側を二段階に分けず、同じ cost 順の 1 本の construct として扱うため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_phy_q_ordered_vs(
        PhysicalDestroyContext<MAX_N> &context,
        const PhyQDestroyPlan<MAX_N> &plan,
        const my_vector<std::pair<int, int>> &ordered_vs
) {
    const auto timing_start = std::chrono::steady_clock::now();
    if (is_timing_log_enabled()) {
        std::ostringstream ss;
        ss << "stage=construct_phy_q_ordered_begin"
           << " v_count=" << ordered_vs.size()
           << " qcnt=" << context.yst.queries_state.queries.size()
           << " sum_turn=" << context.yst.queries_state.sum_turn;
        write_timing_log(ss.str());
    }
    reset_never_construct_vs(ordered_vs);
    for (const auto &[v, destroyed_cost]: ordered_vs) {
        (void) destroyed_cost;
        if (plan.is_physical_v[v]) {
            construct_v_physical_destroyed(context, v);
        } else {
            construct_v_only_query_destroyed(context.current_st, context.yst, v);
        }
        mark_constructed_v(v);
    }
    if (is_timing_log_enabled()) {
        const auto timing_end = std::chrono::steady_clock::now();
        const auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(timing_end - timing_start).count();
        std::ostringstream ss;
        ss << "stage=construct_phy_q_ordered_end"
           << " v_count=" << ordered_vs.size()
           << " qcnt=" << context.yst.queries_state.queries.size()
           << " sum_turn=" << context.yst.queries_state.sum_turn
           << " elapsed_ms=" << elapsed_ms;
        write_timing_log(ss.str());
    }
}

// 責務: PHY_Q destroy plan の全対象 v を cost 降順に並べ、construct_phy_q_ordered_vs へ渡す。
// 必要な理由: query destroy と physical destroy を混ぜても、重い v から再構築する仕様を守るため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_phy_q_destroyed(
        PhysicalDestroyContext<MAX_N> &context,
        const PhyQDestroyPlan<MAX_N> &plan
) {
    Helper &helper = Helper::instance();
    helper.apply_collector_log_mode(collector_log_mode);
    my_vector<std::pair<int, int>> ordered_vs = helper.sort_destroyed_vs_by_cost_desc(plan.destroyed_vs);
    helper.log_construct_physical_begin(context, ordered_vs);
    construct_phy_q_ordered_vs(context, plan, ordered_vs);
    helper.log_construct_physical_end(init_st, context);
}

// 責務: destroyed_vs の順に、INIT/TARGET entry を中央復元してから通常の 1-v construct を行う。
// 必要な理由: 段階復元で random AOrder 復元を使わず、実験結果を再現しやすくするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_physical_middle(
        PhysicalDestroyContext<MAX_N> &context,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    Helper &helper = Helper::instance();
    helper.apply_collector_log_mode(collector_log_mode);
    helper.log_construct_physical_begin(context, destroyed_vs);
    reset_never_construct_vs(destroyed_vs);
    for (const auto &[v, destroyed_cost]: destroyed_vs) {
        (void) destroyed_cost;
        construct_v_physical_middle(context, v);
        mark_constructed_v(v);
    }
    helper.log_construct_physical_end(context.current_st, context);
}

// 責務: 既存 only-query construct 後、手数合計が悪い k/2 個を query だけ再 destroy して再構築する。
// 必要な理由: 既存入口を保ったまま、二段再構築 constructor を実験できるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_worst_half_rebuild(
        YakinamashiState<MAX_N> &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    construct_only_query_destroyed(yst, destroyed_vs);
    // 責務: 一段目 construct 後の状態を基準に、二段目 trial が改善した場合だけ yst へ反映する。
    // 必要な理由: worst half 再構築が悪化した場合に、一段目 construct の結果を壊さないようにするため。
    YakinamashiState<MAX_N> base_yst = yst;
    const int base_score = yst.queries_state.sum_turn;
    my_assert(base_yst == yst);
    my_assert(base_score == yst.queries_state.sum_turn);
    my_vector<std::pair<int, int>> rebuild_vs = pick_worst_half_rebuild_vs(yst, destroyed_vs);
    if (rebuild_vs.empty()) return;
    YakinamashiState<MAX_N> trial_yst = yst;
    rebuild_worst_half_only_query(trial_yst, rebuild_vs);
    if (trial_yst.queries_state.sum_turn < base_score) {
        yst = trial_yst;
        my_assert(yst == trial_yst);
        my_assert(yst.queries_state.sum_turn < base_score);
    } else {
        yst = base_yst;
        my_assert(yst == base_yst);
        my_assert(yst.queries_state.sum_turn == base_score);
    }
}

// 責務: 既存 physical construct 後、手数合計が悪い k/2 個を physical 再 destroy して再構築する。
// 必要な理由: physical 経路でも既存入口を保ったまま、二段再構築 constructor を実験できるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_physical_worst_half_rebuild(
        PhysicalDestroyContext<MAX_N> &context,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    construct_physical_destroyed(context, destroyed_vs);
    // 責務: 一段目 physical construct 後の current_st/yst を基準に、二段目 trial が改善した場合だけ context へ反映する。
    // 必要な理由: worst half physical 再構築が悪化した場合に、一段目 construct の結果を壊さないようにするため。
    State base_current_st = context.current_st;
    YakinamashiState<MAX_N> base_yst = context.yst;
    // 責務: 一段目 physical construct 後の erase_order_snapshot を保持する。
    // 必要な理由: current_st/yst だけを戻して snapshot が trial 後のまま残る不整合を防ぐため。
    EraseOrderSnapshot<MAX_N> base_erase_order_snapshot = context.erase_order_snapshot;
    const int base_score = context.yst.queries_state.sum_turn;
    my_assert(context.yst == base_yst);
    my_assert(base_score == context.yst.queries_state.sum_turn);
    my_vector<std::pair<int, int>> rebuild_vs = pick_worst_half_rebuild_vs(context.yst, destroyed_vs);
    if (rebuild_vs.empty()) return;
    State trial_current_st = context.current_st;
    YakinamashiState<MAX_N> trial_yst = context.yst;
    PhysicalDestroyContext<MAX_N> trial_context(trial_current_st, trial_yst);
    trial_context.erase_order_snapshot = context.erase_order_snapshot;
    rebuild_worst_half_physical(trial_context, rebuild_vs);
    if (trial_context.yst.queries_state.sum_turn < base_score) {
        context.current_st = trial_current_st;
        context.yst = trial_yst;
        context.erase_order_snapshot = trial_context.erase_order_snapshot;
        my_assert(context.yst == trial_yst);
        my_assert(context.yst.queries_state.sum_turn < base_score);
        my_assert(context.erase_order_snapshot.before_erased_order ==
                  trial_context.erase_order_snapshot.before_erased_order);
        my_assert(context.erase_order_snapshot.after_erased_order ==
                  trial_context.erase_order_snapshot.after_erased_order);
        my_assert(context.erase_order_snapshot.is_erased_v ==
                  trial_context.erase_order_snapshot.is_erased_v);
    } else {
        context.current_st = base_current_st;
        context.yst = base_yst;
        context.erase_order_snapshot = base_erase_order_snapshot;
        my_assert(context.yst == base_yst);
        my_assert(context.yst.queries_state.sum_turn == base_score);
        my_assert(context.erase_order_snapshot.before_erased_order ==
                  base_erase_order_snapshot.before_erased_order);
        my_assert(context.erase_order_snapshot.after_erased_order ==
                  base_erase_order_snapshot.after_erased_order);
        my_assert(context.erase_order_snapshot.is_erased_v ==
                  base_erase_order_snapshot.is_erased_v);
    }
}

// 責務: destroyed_vs 内の v を現在 query 手数合計の降順に並べ、上位 k/2 個を返す。
// 必要な理由: 1回目 construct 後に悪い v だけを二段目で再構築するため。
template<size_t MAX_N>
my_vector<std::pair<int, int>> GreedyQueriesConstructor<MAX_N>::pick_worst_half_rebuild_vs(
        const YakinamashiState<MAX_N> &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    Helper &helper = Helper::instance();
    const int rebuild_count = static_cast<int>(destroyed_vs.size()) / 2;
    my_vector<std::pair<int, int>> rebuild_vs;
    if (rebuild_count <= 0) return rebuild_vs;
    my_vector<typename Helper::VQueryTurnInfo> infos =
            helper.collect_v_query_turn_infos(yst, destroyed_vs, init_st.initial_N);
    std::sort(infos.begin(), infos.end(), [](const auto &lhs, const auto &rhs) {
        if (lhs.query_turn_sum != rhs.query_turn_sum) return lhs.query_turn_sum > rhs.query_turn_sum;
        if (lhs.destroyed_cost != rhs.destroyed_cost) return lhs.destroyed_cost > rhs.destroyed_cost;
        return lhs.v < rhs.v;
    });
    for (int i = 0; i < rebuild_count && i < static_cast<int>(infos.size()); i++) {
        rebuild_vs.push_back({infos[i].v, infos[i].destroyed_cost});
    }
    return rebuild_vs;
}

// 責務: rebuild_vs の v を target にする query を QueriesModifier で消し、only-query construct で再構築する。
// 必要な理由: physical state を変更せず、query だけを壊す既存 destroyer と同じ前提で二段目を実行するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::rebuild_worst_half_only_query(
        YakinamashiState<MAX_N> &yst,
        const my_vector<std::pair<int, int>> &rebuild_vs
) {
    if (rebuild_vs.empty()) return;
    Helper &helper = Helper::instance();
    my_bitset<MAX_N> rebuild_v_set = helper.build_rebuild_v_set(rebuild_vs);
    my_vector<int> query_idxs = helper.collect_rebuild_query_idxs(yst, rebuild_v_set);
    if (query_idxs.empty()) return;
    QueryProvider provider(yst, init_st);
    QueriesModifier modifier(init_st, yst.query_order.aorder, yst.queries_state, provider);
    modifier.erase_queries<MAX_N>(query_idxs);
    construct_only_query_destroyed(yst, rebuild_vs);
}

// 責務: rebuild_vs を EraseQueriesReBuilder で physical erase し、physical construct で再構築する。
// 必要な理由: current_st と query/order を一貫して更新する既存 physical 経路で二段目を実行するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::rebuild_worst_half_physical(
        PhysicalDestroyContext<MAX_N> &context,
        const my_vector<std::pair<int, int>> &rebuild_vs
) {
    if (rebuild_vs.empty()) return;
    EraseQueriesReBuilder<MAX_N>::rebuild_by_order(context, rebuild_vs);
    construct_physical_destroyed(context, rebuild_vs);
}

// 責務: only-query destroy 経路で、各 step の現在 yst から全 remaining v を 1 個ずつ trial construct して最良状態を採用する。
// 必要な理由: 既存の 1-v construct 実装を変更せず、再挿入順だけを現在状態の実スコアに基づく greedy にするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_greedy_order(
        YakinamashiState<MAX_N> &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    Helper &helper = Helper::instance();
    helper.apply_collector_log_mode(collector_log_mode);
    my_vector<std::pair<int, int>> sorted_vs = helper.sort_destroyed_vs_by_cost_desc(destroyed_vs);
    my_vector<std::pair<int, int>> remaining_vs = sorted_vs;
    while (!remaining_vs.empty()) {
        reset_never_construct_vs(remaining_vs);
        bool found_best = false;
        int best_idx = -1;
        int best_score = INT_MAX;
        int best_destroyed_cost = INT_MIN;
        int best_v = INT_MAX;
        YakinamashiState<MAX_N> best_yst = yst;
        for (int idx = 0; idx < static_cast<int>(remaining_vs.size()); idx++) {
            const int v = remaining_vs[idx].first;
            const int destroyed_cost = remaining_vs[idx].second;
            YakinamashiState<MAX_N> trial_yst = yst;
            construct_v_only_query_destroyed(init_st, trial_yst, v);
            const int trial_score = trial_yst.queries_state.sum_turn;
            const bool better =
                    !found_best
                    || trial_score < best_score
                    || (trial_score == best_score && destroyed_cost > best_destroyed_cost)
                    || (trial_score == best_score && destroyed_cost == best_destroyed_cost && v < best_v);
            if (!better) continue;
            found_best = true;
            best_idx = idx;
            best_score = trial_score;
            best_destroyed_cost = destroyed_cost;
            best_v = v;
            best_yst = trial_yst;
        }
        my_assert(found_best);
        yst = best_yst;
        remaining_vs.erase(remaining_vs.begin() + best_idx);
    }
}

// 責務: physical destroy 経路で、current_st と yst を含む trial context を作り、最良 trial 状態を本物の context へ反映する。
// 必要な理由: 参照メンバを持つ PhysicalDestroyContext を直接コピーせず、既存の 1-v physical construct を使って greedy 順を実現するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_physical_greedy(
        PhysicalDestroyContext<MAX_N> &context,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    Helper &helper = Helper::instance();
    helper.apply_collector_log_mode(collector_log_mode);
    my_vector<std::pair<int, int>> sorted_vs = helper.sort_destroyed_vs_by_cost_desc(destroyed_vs);
    helper.log_construct_physical_begin(context, sorted_vs);
    my_vector<std::pair<int, int>> remaining_vs = sorted_vs;
    while (!remaining_vs.empty()) {
        reset_never_construct_vs(remaining_vs);
        bool found_best = false;
        int best_idx = -1;
        int best_score = INT_MAX;
        int best_destroyed_cost = INT_MIN;
        int best_v = INT_MAX;
        State best_current_st = context.current_st;
        YakinamashiState<MAX_N> best_yst = context.yst;
        for (int idx = 0; idx < static_cast<int>(remaining_vs.size()); idx++) {
            const int v = remaining_vs[idx].first;
            const int destroyed_cost = remaining_vs[idx].second;
            State trial_current_st = context.current_st;
            YakinamashiState<MAX_N> trial_yst = context.yst;
            PhysicalDestroyContext<MAX_N> trial_context(trial_current_st, trial_yst);
            trial_context.erase_order_snapshot = context.erase_order_snapshot;
            construct_v_physical_destroyed(trial_context, v);
            const int trial_score = trial_context.yst.queries_state.sum_turn;
            const bool better =
                    !found_best
                    || trial_score < best_score
                    || (trial_score == best_score && destroyed_cost > best_destroyed_cost)
                    || (trial_score == best_score && destroyed_cost == best_destroyed_cost && v < best_v);
            if (!better) continue;
            found_best = true;
            best_idx = idx;
            best_score = trial_score;
            best_destroyed_cost = destroyed_cost;
            best_v = v;
            best_current_st = trial_context.current_st;
            best_yst = trial_context.yst;
        }
        my_assert(found_best);
        context.current_st = best_current_st;
        context.yst = best_yst;
        remaining_vs.erase(remaining_vs.begin() + best_idx);
    }
    helper.log_construct_physical_end(init_st, context);
}

template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::Helper::calc_diff_without_band(
        const YakinamashiState &case_yst,
        const State &init_st,
        int v,
        int a2b_idx,
        int b2a_idx,
        AllOrd all_ord_b,
        const InsertPairPrecalc &prec
) {
            int N = case_yst.queries_state.current_N;
            const int qcnt = static_cast<int>(case_yst.queries_state.queries.size());
            const QRIList &queries = case_yst.queries_state.queries;
            DebugIns2Case case_info{a2b_idx, b2a_idx, all_ord_b.get()};
            const auto &exist_a_ords_before_q_idx = prec.exist_stack_a_ords;
            const auto &exist_b_ords_before_q_idx = prec.exist_stack_b_ords;
            const auto &a2b_band_ranges = prec.a2b_band_ranges;
            const auto &fixed_indirect_a2b_prefix_sum = prec.fixed_indirect_a2b_prefix_sum;
            const auto &fixed_indirect_a2b_b2a_prefix_sum = prec.fixed_indirect_a2b_b2a_prefix_sum;
            const auto &base_ra_prefix_sum_A2A = prec.base_ra_prefix_sum_A2A;
            const auto &base_rra_prefix_sum_A2A = prec.base_rra_prefix_sum_A2A;
            const auto &indirect_a2b_ra_prefix_sum_A2A = prec.indirect_a2b_ra_prefix_sum_A2A;
            const auto &indirect_a2b_rra_prefix_sum_A2A = prec.indirect_a2b_rra_prefix_sum_A2A;
            const auto &indirect_a2b_b2a_ra_prefix_sum_A2A = prec.indirect_a2b_b2a_ra_prefix_sum_A2A;
            const auto &indirect_a2b_b2a_rra_prefix_sum_A2A = prec.indirect_a2b_b2a_rra_prefix_sum_A2A;
            const auto &next_ab_idxs = prec.next_ab_idxs;
            const auto &prev_a2a_ranges = prec.prev_a2a_ranges;
            int direct_final_rot_diff = prec.direct_final_rot_diff;
            int indirect_final_rot_diff = prec.indirect_final_rot_diff;
            bool use_direct_final_rot = prec.next_a2b_b2a_effected_valid_q_idx[case_info.b2a_idx] == qcnt;
            int res_diff;
            if (use_direct_final_rot) {
                res_diff = direct_final_rot_diff;
            }else{
                res_diff = indirect_final_rot_diff;
            }
            int a2b_direct1_idx = prec.next_a2b_effected_valid_q_idx[case_info.a2b_idx];
            int a2b_direct2_idx = next_ab_idxs[case_info.a2b_idx];
    //         //a2bがb2aに当たる場合は、 b2aの手数自体の計算の時にやるので、 ぶつかるケースをdirect1と考えなくていい
            if(a2b_direct1_idx == qcnt){
                a2b_direct1_idx = -1;
                a2b_direct2_idx = -1;
            }else if (a2b_direct2_idx == a2b_direct1_idx) {
                a2b_direct2_idx = -1;
            }else if(a2b_direct2_idx == qcnt){
                a2b_direct2_idx = -1;
            }
            int b2a_direct1_idx = prec.next_a2b_b2a_effected_valid_q_idx[case_info.b2a_idx];
            int b2a_direct2_idx = next_ab_idxs[case_info.b2a_idx];
            if(b2a_direct1_idx == qcnt){
                b2a_direct1_idx = -1;
                b2a_direct2_idx = -1;
            }else if (b2a_direct2_idx == b2a_direct1_idx) {
                b2a_direct2_idx = -1;
            }else if(b2a_direct2_idx == qcnt){
                b2a_direct2_idx = -1;
            }
            int mid_indirect_diff_without_band = calc_debug_mid_indirect_sum_without_band(
                    fixed_indirect_a2b_prefix_sum, a2b_band_ranges, case_info,
                    a2b_direct1_idx, a2b_direct2_idx);
            int tail_indirect_diff = calc_tail_indirect_sum(
                    fixed_indirect_a2b_b2a_prefix_sum, case_info, b2a_direct1_idx, b2a_direct2_idx, qcnt);
            res_diff += mid_indirect_diff_without_band;
            res_diff += tail_indirect_diff;
            DebugIns2A2BPlan a2b_plan = build_a2b_plan(
                    v, case_yst, AllOrd(case_info.v_all_ord_b), exist_a_ords_before_q_idx, exist_b_ords_before_q_idx,
                    prev_a2a_ranges, base_ra_prefix_sum_A2A, base_rra_prefix_sum_A2A, case_info.a2b_idx, N);
            res_diff += a2b_plan.bri.ins_turn;
            DebugIns2B2APlan b2a_plan = build_b2a_plan(
                    v, case_yst, init_st, case_info, a2b_plan, exist_a_ords_before_q_idx, exist_b_ords_before_q_idx,
                    prev_a2a_ranges,
                    indirect_a2b_ra_prefix_sum_A2A, indirect_a2b_rra_prefix_sum_A2A, N, prec);
            res_diff += b2a_plan.bri.ins_turn;


            QueryOrdEffect a2b_effect;
            a2b_effect.a_effect.set_del(a2b_plan.tar_all_ord_a);
            a2b_effect.b_effect.set_add(a2b_plan.tar_all_ord_b);
            a2b_effect.diff_stack_a_cnt = -1;

            QueryOrdEffect net_effect = build_a2b_b2a_net_exist_effect(a2b_plan, b2a_plan);

            int a2b_direct1_diff = calc_a2b_direct1_diff(
                    a2b_direct1_idx,
                    case_yst, init_st, queries, case_info, a2b_plan, N);
            res_diff += a2b_direct1_diff;
            int a2b_direct2_diff = calc_a2b_direct2_diff(
                a2b_direct1_idx,
                    a2b_direct2_idx,
                    case_yst, init_st, queries, case_info, a2b_plan, N, next_ab_idxs, a2b_effect,
                    indirect_a2b_ra_prefix_sum_A2A, indirect_a2b_rra_prefix_sum_A2A, prec);
            res_diff += a2b_direct2_diff;
            int b2a_direct1_diff = calc_b2a_direct1_diff(
                    b2a_direct1_idx,
                    case_yst, init_st, queries, case_info, b2a_plan, net_effect, N);
            res_diff += b2a_direct1_diff;
            int b2a_direct2_diff = calc_b2a_direct2_diff(
                    prec,
                    b2a_direct1_idx,
                    b2a_direct2_idx,
                    case_yst, init_st, queries, case_info, b2a_plan, N, next_ab_idxs, net_effect,
                    indirect_a2b_b2a_ra_prefix_sum_A2A, indirect_a2b_b2a_rra_prefix_sum_A2A);
            res_diff += b2a_direct2_diff;
#ifdef DEBUG
            out("calc_diff_without_band_parts",
                "v", v,
                "a2b_idx", a2b_idx,
                "b2a_idx", b2a_idx,
                "all_ord_b", all_ord_b.get(),
                "use_direct_final_rot", use_direct_final_rot,
                "direct_final_rot_diff", direct_final_rot_diff,
                "indirect_final_rot_diff", indirect_final_rot_diff,
                "selected_final_rot_diff", use_direct_final_rot ? direct_final_rot_diff : indirect_final_rot_diff,
                "mid_indirect_diff_without_band", mid_indirect_diff_without_band,
                "tail_indirect_diff", tail_indirect_diff,
                "a2b_insert_turn", a2b_plan.bri.ins_turn,
                "b2a_insert_turn", b2a_plan.bri.ins_turn,
                "a2b_direct1_idx", a2b_direct1_idx,
                "a2b_direct1_diff", a2b_direct1_diff,
                "a2b_direct2_idx", a2b_direct2_idx,
                "a2b_direct2_diff", a2b_direct2_diff,
                "b2a_direct1_idx", b2a_direct1_idx,
                "b2a_direct1_diff", b2a_direct1_diff,
                "b2a_direct2_idx", b2a_direct2_idx,
                "b2a_direct2_diff", b2a_direct2_diff,
                "total_without_band_diff", res_diff);
#endif
            return res_diff;
        }

template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::ScoredPair
GreedyQueriesConstructor<MAX_N>::Helper::eval_insert_pair_qidx(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const PairQidx &pair_qidx,
        int all_ord_b,
        const my_bitset<MAX_QUERY_CNT> &is_band,
        InsertPairPrecalc &prec
) {
            auto[a2b_idx, b2a_idx] = pair_qidx;
            int res_diff = 0;
            int res_band = pop_cnt(is_band, a2b_idx, b2a_idx);
            const int qcnt = static_cast<int>(yst.queries_state.queries.size());
            int a2b_direct1_idx = prec.next_a2b_effected_valid_q_idx[a2b_idx];
            int a2b_direct2_idx = prec.next_ab_idxs[a2b_idx];
            if(a2b_direct1_idx == qcnt){
                a2b_direct1_idx = -1;
                a2b_direct2_idx = -1;
            }else if (a2b_direct2_idx == a2b_direct1_idx) {
                a2b_direct2_idx = -1;
            }else if(a2b_direct2_idx == qcnt){
                a2b_direct2_idx = -1;
            }
            if (a2b_idx <= a2b_direct1_idx && a2b_direct1_idx < b2a_idx && is_band[a2b_direct1_idx]) {
                res_band--;
            }
            if (a2b_idx <= a2b_direct2_idx && a2b_direct2_idx < b2a_idx && is_band[a2b_direct2_idx]) {
                res_band--;
            }
            res_diff += res_band;
            int without_band_diff = calc_diff_without_band(
                    yst, init_st, v, a2b_idx, b2a_idx, AllOrd(all_ord_b), prec);
            res_diff += without_band_diff;
#ifdef DEBUG
            auto naive = Helper::instance().evaluate_naive_insert_plan(
                    yst, init_st, v, a2b_idx, b2a_idx, AllOrd(all_ord_b));
            int naive_diff = naive.score - yst.queries_state.sum_turn;
            if (res_diff != naive_diff) {
                int active_band_scan = 0;
                int range_band_scan = 0;
                std::ostringstream active_band_q_idxs;
                std::ostringstream range_band_q_idxs;
                for (int q_idx = a2b_idx; q_idx < b2a_idx; q_idx++) {
                    if (is_band[q_idx]) {
                        active_band_scan++;
                        active_band_q_idxs << q_idx << ",";
                    }
                    if (is_b_all_ord_inside_band(prec.a2b_band_ranges[q_idx], all_ord_b)) {
                        range_band_scan++;
                        range_band_q_idxs << q_idx << ",";
                    }
                }
                out("calc_best_plan_band_check",
                    "v", v,
                    "a2b_idx", a2b_idx,
                    "b2a_idx", b2a_idx,
                    "all_ord_b", all_ord_b,
                    "res_band", res_band,
                    "active_band_scan", active_band_scan,
                    "range_band_scan", range_band_scan,
                    "active_band_q_idxs", active_band_q_idxs.str(),
                    "range_band_q_idxs", range_band_q_idxs.str(),
                    "without_band_diff", without_band_diff,
                    "res_diff", res_diff,
                    "naive_diff", naive_diff);
                // _REVIEW_END
                out("calc_best_plan_diff_mismatch",
                    "v", v,
                    "a2b_idx", a2b_idx,
                    "b2a_idx", b2a_idx,
                    "all_ord_b", all_ord_b,
                    "res_diff", res_diff,
                    "naive_diff", naive_diff,
                    "naive_score", naive.score,
                    "base_score", yst.queries_state.sum_turn);
                // _REVIEW_BEGIN: direct1/direct2 の推定対象と naive 再構築後の対応 query を照合するため。
                out("calc_best_plan_naive_inserted",
                    "v", v,
                    "a2b_idx", a2b_idx,
                    "b2a_idx", b2a_idx,
                    "shifted_b2a_idx", naive.shifted_b2a_idx,
                    "a2b_query", naive.a2b_query.value(),
                    "b2a_query", naive.b2a_query.value());
                DebugIns2Case case_info{a2b_idx, b2a_idx, all_ord_b};
                DebugIns2A2BPlan debug_a2b_plan = build_a2b_plan(
                        v, yst, AllOrd(all_ord_b),
                        prec.exist_stack_a_ords, prec.exist_stack_b_ords,
                        prec.prev_a2a_ranges, prec.base_ra_prefix_sum_A2A, prec.base_rra_prefix_sum_A2A,
                        a2b_idx, yst.queries_state.current_N);
                DebugIns2B2APlan debug_b2a_plan = build_b2a_plan(
                        v, yst, init_st, case_info, debug_a2b_plan,
                        prec.exist_stack_a_ords, prec.exist_stack_b_ords,
                        prec.prev_a2a_ranges,
                        prec.indirect_a2b_ra_prefix_sum_A2A,
                        prec.indirect_a2b_rra_prefix_sum_A2A,
                        yst.queries_state.current_N, prec);
                const QueryCommonRI &naive_b2a_qri = naive.queries[naive.shifted_b2a_idx];
                const QueryCommon &naive_b2a_q = naive_b2a_qri.get_query();
                int naive_b2a_prev_idx = naive.shifted_b2a_idx - 1;
                OrdPos naive_b2a_start_a = yst.queries_state.first_top_ord_a;
                OrdPos naive_b2a_start_b = yst.queries_state.first_top_ord_b;
                std::string naive_b2a_prev_qri = "none";
                if (naive_b2a_prev_idx >= 0) {
                    naive_b2a_start_a = naive.queries[naive_b2a_prev_idx].get_finished_top_ord_a();
                    naive_b2a_start_b = naive.queries[naive_b2a_prev_idx].get_query().get_finished_top_ord_b(
                            yst.queries_state.current_N);
                    naive_b2a_prev_qri = indirect_debug_to_string(naive.queries[naive_b2a_prev_idx]);
                }
                int est_ra_dis = RotCalculator::get_ra_dis(
                        debug_b2a_plan.start_ord_pos_a,
                        debug_b2a_plan.tar_ord_pos_a,
                        debug_b2a_plan.stack_a_cnt - 1);
                int est_rra_dis = RotCalculator::get_rra_dis(
                        debug_b2a_plan.start_ord_pos_a,
                        debug_b2a_plan.tar_ord_pos_a,
                        debug_b2a_plan.stack_a_cnt - 1);
                int est_rb_dis = RotCalculator::get_rb_dis(
                        debug_b2a_plan.start_ord_pos_b,
                        debug_b2a_plan.tar_ord_pos_b,
                        debug_b2a_plan.stack_b_cnt + 1);
                int est_rrb_dis = RotCalculator::get_rrb_dis(
                        debug_b2a_plan.start_ord_pos_b,
                        debug_b2a_plan.tar_ord_pos_b,
                        debug_b2a_plan.stack_b_cnt + 1);
                auto [est_sync_rb_dis, est_sync_rrb_dis] = PrevA2ARotSumCalc::get_prev_a2a_syncd_rot_b(
                        est_rb_dis, est_rrb_dis, debug_b2a_plan.prev_a2a_rot_sum);
                OrdPos naive_tar_ord_pos_a = to_ord_pos(
                        naive_b2a_q.get_tar_rel_ord_a_B2A(), naive_b2a_q.get_stack_a_cnt());
                OrdPos naive_tar_ord_pos_b = naive_b2a_q.get_tar_ord_pos_b_B2A();
                int naive_ra_dis = RotCalculator::get_ra_dis(
                        naive_b2a_start_a, naive_tar_ord_pos_a, naive_b2a_q.get_stack_a_cnt());
                int naive_rra_dis = RotCalculator::get_rra_dis(
                        naive_b2a_start_a, naive_tar_ord_pos_a, naive_b2a_q.get_stack_a_cnt());
                int naive_rb_dis = RotCalculator::get_rb_dis(
                        naive_b2a_start_b, naive_tar_ord_pos_b, naive_b2a_qri.get_stack_b_cnt(
                                yst.queries_state.current_N));
                int naive_rrb_dis = RotCalculator::get_rrb_dis(
                        naive_b2a_start_b, naive_tar_ord_pos_b, naive_b2a_qri.get_stack_b_cnt(
                                yst.queries_state.current_N));
                out("calc_best_plan_b2a_insert_detail",
                    "v", v,
                    "a2b_idx", a2b_idx,
                    "b2a_idx", b2a_idx,
                    "all_ord_b", all_ord_b,
                    "est_turn", debug_b2a_plan.bri.ins_turn,
                    "est_stack_a_cnt_before", debug_b2a_plan.stack_a_cnt,
                    "est_stack_b_cnt_before", debug_b2a_plan.stack_b_cnt,
                    "est_stack_a_cnt_effected", debug_b2a_plan.stack_a_cnt - 1,
                    "est_stack_b_cnt_effected", debug_b2a_plan.stack_b_cnt + 1,
                    "est_start_a", debug_b2a_plan.start_ord_pos_a.get(),
                    "est_start_b", debug_b2a_plan.start_ord_pos_b.get(),
                    "est_tar_a", debug_b2a_plan.tar_ord_pos_a.get(),
                    "est_tar_b", debug_b2a_plan.tar_ord_pos_b.get(),
                    "est_ra_dis", est_ra_dis,
                    "est_rra_dis", est_rra_dis,
                    "est_rb_dis", est_rb_dis,
                    "est_rrb_dis", est_rrb_dis,
                    "est_sync_rb_dis", est_sync_rb_dis,
                    "est_sync_rrb_dis", est_sync_rrb_dis,
                    "est_prev_a2a_ra_sum", debug_b2a_plan.prev_a2a_rot_sum.ra_sum,
                    "est_prev_a2a_rra_sum", debug_b2a_plan.prev_a2a_rot_sum.rra_sum,
                    "est_cmd_a", debug_b2a_plan.bri.cmd_a,
                    "est_cmd_a_cnt", debug_b2a_plan.bri.cmd_a_cnt,
                    "est_cmd_b", debug_b2a_plan.bri.cmd_b,
                    "est_cmd_b_cnt", debug_b2a_plan.bri.cmd_b_cnt,
                    "naive_turn", naive_b2a_qri.get_ins_turn(),
                    "naive_stack_a_cnt", naive_b2a_q.get_stack_a_cnt(),
                    "naive_stack_b_cnt", naive_b2a_qri.get_stack_b_cnt(yst.queries_state.current_N),
                    "naive_start_a", naive_b2a_start_a.get(),
                    "naive_start_b", naive_b2a_start_b.get(),
                    "naive_tar_a", naive_tar_ord_pos_a.get(),
                    "naive_tar_b", naive_tar_ord_pos_b.get(),
                    "naive_ra_dis", naive_ra_dis,
                    "naive_rra_dis", naive_rra_dis,
                    "naive_rb_dis", naive_rb_dis,
                    "naive_rrb_dis", naive_rrb_dis,
                    "naive_cmd_a", naive_b2a_qri.ab_cmd_a(),
                    "naive_cmd_a_cnt", naive_b2a_qri.get_a_cmd_cost_AB(),
                    "naive_cmd_b", naive_b2a_qri.ab_cmd_b(),
                    "naive_cmd_b_cnt", naive_b2a_qri.get_b_cmd_cost_AB(),
                    "naive_prev_idx", naive_b2a_prev_idx,
                    "naive_prev_qri", naive_b2a_prev_qri,
                    "naive_qri", indirect_debug_to_string(naive_b2a_qri));
                // _REVIEW_BEGIN: base window ログに出す b2a 側 direct 対象を calc_diff_without_band と同じ規則で特定するため。
                int b2a_direct1_idx = prec.next_a2b_b2a_effected_valid_q_idx[b2a_idx];
                int b2a_direct2_idx = prec.next_ab_idxs[b2a_idx];
                if (b2a_direct1_idx == qcnt) {
                    b2a_direct1_idx = -1;
                    b2a_direct2_idx = -1;
                } else if (b2a_direct2_idx == b2a_direct1_idx) {
                    b2a_direct2_idx = -1;
                } else if (b2a_direct2_idx == qcnt) {
                    b2a_direct2_idx = -1;
                }
                // _REVIEW_END
                // _REVIEW_BEGIN: direct2 差分を読む時に必要な関係 query を一箇所に集めて出すため。
                out("calc_best_plan_related_insert_a2b",
                    "base_insert_idx", a2b_idx,
                    "naive_idx", a2b_idx,
                    "query", naive.a2b_query.value());
                out("calc_best_plan_related_insert_b2a",
                    "base_insert_idx", b2a_idx,
                    "naive_idx", naive.shifted_b2a_idx,
                    "query", naive.b2a_query.value());
                if (b2a_direct1_idx != -1) {
                    int naive_direct1_idx = b2a_direct1_idx;
                    if (a2b_idx <= b2a_direct1_idx) naive_direct1_idx++;
                    if (b2a_idx <= b2a_direct1_idx) naive_direct1_idx++;
                    out("calc_best_plan_related_direct1",
                        "base_idx", b2a_direct1_idx,
                        "naive_idx", naive_direct1_idx,
                        "base_qri", indirect_debug_to_string(yst.queries_state.queries[b2a_direct1_idx]),
                        "naive_qri", indirect_debug_to_string(naive.queries[naive_direct1_idx]));
                }
                if (b2a_direct2_idx != -1) {
                    int naive_direct2_idx = b2a_direct2_idx;
                    if (a2b_idx <= b2a_direct2_idx) naive_direct2_idx++;
                    if (b2a_idx <= b2a_direct2_idx) naive_direct2_idx++;
                    out("calc_best_plan_related_direct2",
                        "base_idx", b2a_direct2_idx,
                        "naive_idx", naive_direct2_idx,
                        "base_qri", indirect_debug_to_string(yst.queries_state.queries[b2a_direct2_idx]),
                        "naive_qri", indirect_debug_to_string(naive.queries[naive_direct2_idx]));
                }
                // _REVIEW_END
                int base_begin = std::max(0, b2a_idx - 3);
                int base_end = std::min(qcnt, b2a_idx + 4);
                for (int q_idx = base_begin; q_idx < base_end; q_idx++) {
                    out("calc_best_plan_base_window",
                        "q_idx", q_idx,
                        "direct1_idx", b2a_direct1_idx,
                        "direct2_idx", b2a_direct2_idx,
                        "qri", indirect_debug_to_string(yst.queries_state.queries[q_idx]));
                }
                int naive_qcnt = static_cast<int>(naive.queries.size());
                int naive_begin = std::max(0, naive.shifted_b2a_idx - 3);
                int naive_end = std::min(naive_qcnt, naive.shifted_b2a_idx + 5);
                for (int q_idx = naive_begin; q_idx < naive_end; q_idx++) {
                    out("calc_best_plan_naive_window",
                        "q_idx", q_idx,
                        "shifted_b2a_idx", naive.shifted_b2a_idx,
                        "qri", indirect_debug_to_string(naive.queries[q_idx]));
                }
                // _REVIEW_BEGIN: base の各 query が naive で何手変化したかを直接対応付け、ずれた成分を確定するため。
                int mapped_begin = a2b_idx;
                int mapped_end = std::min(qcnt, b2a_idx + 4);
                for (int q_idx = mapped_begin; q_idx < mapped_end; q_idx++) {
                    int naive_q_idx = q_idx;
                    if (a2b_idx <= q_idx) naive_q_idx++;
                    if (b2a_idx <= q_idx) naive_q_idx++;
                    if (naive_q_idx < 0 || naive_q_idx >= naive_qcnt) continue;
                    bool is_a2b_direct1 = q_idx == a2b_direct1_idx;
                    bool is_a2b_direct2 = q_idx == a2b_direct2_idx;
                    bool is_b2a_direct1 = q_idx == b2a_direct1_idx;
                    bool is_b2a_direct2 = q_idx == b2a_direct2_idx;
                    bool band_contribution = a2b_idx <= q_idx && q_idx < b2a_idx && is_band[q_idx];
                    if (is_a2b_direct1 || is_a2b_direct2) band_contribution = false;
                    int base_turn = yst.queries_state.queries[q_idx].get_ins_turn();
                    int naive_turn = naive.queries[naive_q_idx].get_ins_turn();
                    out("calc_best_plan_mapped_turn_diff",
                        "base_q_idx", q_idx,
                        "naive_q_idx", naive_q_idx,
                        "is_band", is_band[q_idx],
                        "band_contribution", band_contribution,
                        "is_a2b_direct1", is_a2b_direct1,
                        "is_a2b_direct2", is_a2b_direct2,
                        "is_b2a_direct1", is_b2a_direct1,
                        "is_b2a_direct2", is_b2a_direct2,
                        "base_turn", base_turn,
                        "naive_turn", naive_turn,
                        "turn_diff", naive_turn - base_turn,
                        "base_qri", indirect_debug_to_string(yst.queries_state.queries[q_idx]),
                        "naive_qri", indirect_debug_to_string(naive.queries[naive_q_idx]));
                }
                // _REVIEW_END
                // _REVIEW_END
                my_assert(false);
            }
#endif
            return ScoredPair{pair_qidx, AllOrd(all_ord_b), res_diff};
}

// 責務: 指定 all_ord の no-band 診断用内訳を既存 PairCostLog 形式で作る。
// 必要な理由: no-band 差分が変わった時に、どの direct/insert 成分が変化したかをログで確認するため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::PairCostLog
GreedyQueriesConstructor<MAX_N>::Helper::build_no_band_probe_cost_log(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        int b2a_idx,
        AllOrd pushed_b_all_ord,
        InsertPairPrecalc &prec
) {
    return build_pair_cost_log(yst, init_st, v, a2b_idx, b2a_idx, pushed_b_all_ord, prec);
}

// 責務: 指定 all_ord で A2B/B2A plan を作り、B2A 手数に関係する値を B2APlanProbeLog に詰める。
// 必要な理由: no-band 差分変化時に、B2A 自身の tar/start/PrevA2A/command の違いを直接比較するため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::B2APlanProbeLog
GreedyQueriesConstructor<MAX_N>::Helper::build_b2a_plan_probe_log(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        int b2a_idx,
        AllOrd pushed_b_all_ord,
        InsertPairPrecalc &prec
) {
    const int n = yst.queries_state.current_N;
    DebugIns2Case case_info{a2b_idx, b2a_idx, pushed_b_all_ord.get()};
    DebugIns2A2BPlan a2b_plan = build_a2b_plan(
            v, yst, pushed_b_all_ord,
            prec.exist_stack_a_ords,
            prec.exist_stack_b_ords,
            prec.prev_a2a_ranges,
            prec.base_ra_prefix_sum_A2A,
            prec.base_rra_prefix_sum_A2A,
            a2b_idx,
            n);
    DebugIns2B2APlan b2a_plan = build_b2a_plan(
            v, yst, init_st, case_info, a2b_plan,
            prec.exist_stack_a_ords,
            prec.exist_stack_b_ords,
            prec.prev_a2a_ranges,
            prec.indirect_a2b_ra_prefix_sum_A2A,
            prec.indirect_a2b_rra_prefix_sum_A2A,
            n,
            prec);
    B2APlanProbeLog log;
    log.all_ord = pushed_b_all_ord.get();
    log.stack_a_cnt = b2a_plan.stack_a_cnt;
    log.stack_b_cnt = b2a_plan.stack_b_cnt;
    log.tar_rel_ord_a = b2a_plan.tar_rel_ord_a.get();
    log.tar_ord_pos_a = b2a_plan.tar_ord_pos_a.get();
    log.tar_rel_ord_b =
            OrdCalc::get_rel_ord(prec.exist_stack_b_ords[b2a_idx], a2b_plan.tar_all_ord_b).get();
    log.tar_ord_pos_b = b2a_plan.tar_ord_pos_b.get();
    log.start_ord_pos_a = b2a_plan.start_ord_pos_a.get();
    log.start_ord_pos_b = b2a_plan.start_ord_pos_b.get();
    log.prev_a2a_ra_sum = b2a_plan.prev_a2a_rot_sum.ra_sum;
    log.prev_a2a_rra_sum = b2a_plan.prev_a2a_rot_sum.rra_sum;
    log.cmd_a = b2a_plan.bri.cmd_a;
    log.cmd_a_cnt = b2a_plan.bri.cmd_a_cnt;
    log.cmd_b = b2a_plan.bri.cmd_b;
    log.cmd_b_cnt = b2a_plan.bri.cmd_b_cnt;
    log.ins_turn = b2a_plan.bri.ins_turn;
    log.finished_ord_pos_a = b2a_plan.finished_ord_pos_a.get();
    log.finished_ord_pos_b = b2a_plan.finished_ord_pos_b.get();
    return log;
}

// 責務: B2APlanProbeLog を legacy_check_trace に出力する。
// 必要な理由: mismatch した 2 点の B2A plan 詳細を同じ形式で比較できるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::log_b2a_plan_probe(
        const char *label,
        const B2APlanProbeLog &log
) {
    out_legacy_check_log(label,
                         "all_ord =", log.all_ord,
                         ", stack_a_cnt =", log.stack_a_cnt,
                         ", stack_b_cnt =", log.stack_b_cnt,
                         ", tar_rel_ord_a =", log.tar_rel_ord_a,
                         ", tar_ord_pos_a =", log.tar_ord_pos_a,
                         ", tar_rel_ord_b =", log.tar_rel_ord_b,
                         ", tar_ord_pos_b =", log.tar_ord_pos_b,
                         ", start_ord_pos_a =", log.start_ord_pos_a,
                         ", start_ord_pos_b =", log.start_ord_pos_b,
                         ", prev_a2a_ra_sum =", log.prev_a2a_ra_sum,
                         ", prev_a2a_rra_sum =", log.prev_a2a_rra_sum,
                         ", cmd_a =", log.cmd_a,
                         ", cmd_a_cnt =", log.cmd_a_cnt,
                         ", cmd_b =", log.cmd_b,
                         ", cmd_b_cnt =", log.cmd_b_cnt,
                         ", ins_turn =", log.ins_turn,
                         ", finished_ord_pos_a =", log.finished_ord_pos_a,
                         ", finished_ord_pos_b =", log.finished_ord_pos_b);
}

// 責務: B2A 直前の B 全体に存在する all_ord 集合を出力する。
// 必要な理由: provisional range の split 点に無い all_ord が、B 全体側で ord/rank へ影響していないか確認するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::log_b2a_exist_b_ords_probe(
        int b2a_idx,
        InsertPairPrecalc &prec
) {
    vec_array<int, MAX_ALL_ORD_B> exist_b_ords;
    my_assert(0 <= b2a_idx && b2a_idx < static_cast<int>(prec.exist_stack_b_ords.size()));
    for (int all_ord = 0; all_ord < static_cast<int>(MAX_ALL_ORD_B); all_ord++) {
        if (prec.exist_stack_b_ords[b2a_idx][all_ord]) exist_b_ords.push_back(all_ord);
    }
    out_legacy_check_log("no_band_range_probe_exist_b_ords",
                         "b2a_idx =", b2a_idx,
                         ", exist_b_ords =", int_vector_to_debug_string(exist_b_ords));
}

// 責務: mismatch した PairRangeTask の range 列を出力する。
// 必要な理由: 問題の all_ord がどの粗い OrdRange にまとめられていたかをログ上で確認するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::log_b2a_range_points_probe(
        const BasePairCandidates &candidates,
        const PairRangeTask &task
) {
    out_legacy_check_log("no_band_range_probe_ord_ranges",
                         "a2b_idx =", task.a2b_idx,
                         ", b2a_idx =", task.b2a_idx,
                         ", ord_ranges =", task_ord_ranges_to_debug_string(candidates, task));
}

// 責務: PairRangeTask の各 OrdRange を全点走査し、calc_diff_without_band が range 内で変わればログして停止する。
// 必要な理由: range 評価が no-band 差分一定を仮定できるか、壊れるならどの成分が原因かを特定するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::log_no_band_range_diff_probe(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const BasePairCandidates &candidates,
        const PairRangeTask &task,
        InsertPairPrecalc &prec
) {
#ifdef DEBUG
    const int a2b_idx = task.a2b_idx;
    const int b2a_idx = task.b2a_idx;
    for (int range_idx = 0; range_idx < task.range_count; range_idx++) {
        const OrdRange &range = candidates.pair_ranges[task.range_begin + range_idx];
        int first_all_ord = -1;
        int first_no_band_diff = 0;
        for (int all_ord = range.begin; all_ord < range.end; all_ord++) {
            const int no_band_diff = calc_diff_without_band(
                    yst, init_st, v, a2b_idx, b2a_idx, AllOrd(all_ord), prec);
            out_legacy_check_log("no_band_range_probe",
                                 "v =", v,
                                 ", a2b_idx =", a2b_idx,
                                 ", b2a_idx =", b2a_idx,
                                 ", range_begin =", range.begin,
                                 ", range_end =", range.end,
                                 ", all_ord =", all_ord,
                                 ", no_band_diff =", no_band_diff);
            if (first_all_ord == -1) {
                first_all_ord = all_ord;
                first_no_band_diff = no_band_diff;
                continue;
            }
            if (first_no_band_diff == no_band_diff) continue;
            PairCostLog base_log = build_no_band_probe_cost_log(
                    yst, init_st, v, a2b_idx, b2a_idx, AllOrd(first_all_ord), prec);
            PairCostLog diff_log = build_no_band_probe_cost_log(
                    yst, init_st, v, a2b_idx, b2a_idx, AllOrd(all_ord), prec);
            B2APlanProbeLog base_b2a_plan_log = build_b2a_plan_probe_log(
                    yst, init_st, v, a2b_idx, b2a_idx, AllOrd(first_all_ord), prec);
            B2APlanProbeLog diff_b2a_plan_log = build_b2a_plan_probe_log(
                    yst, init_st, v, a2b_idx, b2a_idx, AllOrd(all_ord), prec);
            out_legacy_check_log("no_band_range_mismatch",
                                 "v =", v,
                                 ", a2b_idx =", a2b_idx,
                                 ", b2a_idx =", b2a_idx,
                                 ", range_begin =", range.begin,
                                 ", range_end =", range.end,
                                 ", first_all_ord =", first_all_ord,
                                 ", first_no_band_diff =", first_no_band_diff,
                                 ", diff_all_ord =", all_ord,
                                 ", diff_no_band_diff =", no_band_diff);
            out_legacy_check_log("no_band_range_mismatch_cost_base",
                                 "all_ord =", first_all_ord,
                                 ", a2b_insert_turn =", base_log.a2b_insert_turn,
                                 ", a2b_direct1_idx =", base_log.a2b_direct1_idx,
                                 ", a2b_direct1_diff =", base_log.a2b_direct1_diff,
                                 ", a2b_direct2_idx =", base_log.a2b_direct2_idx,
                                 ", a2b_direct2_diff =", base_log.a2b_direct2_diff,
                                 ", a2b_total =", base_log.a2b_total,
                                 ", b2a_insert_turn =", base_log.b2a_insert_turn,
                                 ", b2a_direct1_idx =", base_log.b2a_direct1_idx,
                                 ", b2a_direct1_diff =", base_log.b2a_direct1_diff,
                                 ", b2a_direct2_idx =", base_log.b2a_direct2_idx,
                                 ", b2a_direct2_diff =", base_log.b2a_direct2_diff,
                                 ", b2a_total =", base_log.b2a_total);
            out_legacy_check_log("no_band_range_mismatch_cost_diff",
                                 "all_ord =", all_ord,
                                 ", a2b_insert_turn =", diff_log.a2b_insert_turn,
                                 ", a2b_direct1_idx =", diff_log.a2b_direct1_idx,
                                 ", a2b_direct1_diff =", diff_log.a2b_direct1_diff,
                                 ", a2b_direct2_idx =", diff_log.a2b_direct2_idx,
                                 ", a2b_direct2_diff =", diff_log.a2b_direct2_diff,
                                 ", a2b_total =", diff_log.a2b_total,
                                 ", b2a_insert_turn =", diff_log.b2a_insert_turn,
                                 ", b2a_direct1_idx =", diff_log.b2a_direct1_idx,
                                 ", b2a_direct1_diff =", diff_log.b2a_direct1_diff,
                                 ", b2a_direct2_idx =", diff_log.b2a_direct2_idx,
                                 ", b2a_direct2_diff =", diff_log.b2a_direct2_diff,
                                 ", b2a_total =", diff_log.b2a_total);
            log_b2a_range_points_probe(candidates, task);
            log_b2a_exist_b_ords_probe(b2a_idx, prec);
            log_b2a_plan_probe("no_band_range_probe_b2a_plan_base", base_b2a_plan_log);
            log_b2a_plan_probe("no_band_range_probe_b2a_plan_diff", diff_b2a_plan_log);
            my_assert(false);
        }
    }
#else
    (void) yst;
    (void) init_st;
    (void) v;
    (void) task;
    (void) prec;
#endif
}

// 責務: 1 つの PairRangeTask について、direct band を一時除外し、各 OrdRange の最小 band all_ord へ no-band diff を足した最小 ScoredPair を返す。
// 必要な理由: OrdRange ごとに no-band diff が変わるため、band だけで全 range の all_ord を 1 点に決めると最小手数を取り逃すため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::ScoredPair
GreedyQueriesConstructor<MAX_N>::Helper::eval_pair_range_task(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const BasePairCandidates &candidates,
        const PairRangeTask &task,
        BandMinSeg &seg,
        InsertPairPrecalc &prec
) {
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const int a2b_idx = task.a2b_idx;
    const int b2a_idx = task.b2a_idx;
    my_assert(0 <= a2b_idx && a2b_idx <= b2a_idx && b2a_idx <= qcnt);
#ifdef DEBUG
    log_no_band_range_diff_probe(yst, init_st, v, candidates, task, prec);
#endif
    vec_array<int, 2> direct_band_q_idxs;
    int direct1_idx = normalize_a2b_direct1_idx(a2b_idx, prec, qcnt);
    int direct2_idx = normalize_a2b_direct2_idx(direct1_idx, a2b_idx, prec, qcnt);
    if (a2b_idx <= direct1_idx && direct1_idx < b2a_idx) direct_band_q_idxs.push_back(direct1_idx);
    if (a2b_idx <= direct2_idx && direct2_idx < b2a_idx && direct2_idx != direct1_idx) {
        direct_band_q_idxs.push_back(direct2_idx);
    }
    for (int q_idx: direct_band_q_idxs) add_band_range_to_min_seg(seg, prec.a2b_band_ranges[q_idx], -1);
    bool found_best_range = false;
    int best_all_ord = -1;
    int best_min_band_diff = 0;
    int best_without_band_diff = 0;
    int best_score = INT_MAX;
    for (int range_idx = 0; range_idx < task.range_count; range_idx++) {
        const OrdRange &range = candidates.pair_ranges[task.range_begin + range_idx];
        BandMinNode node = seg.query_range(range.begin, range.end);
        my_assert(node.min_all_ord != -1);
        const int without_band_diff = calc_diff_without_band(
                yst, init_st, v, a2b_idx, b2a_idx, AllOrd(node.min_all_ord), prec);
        const int range_score = without_band_diff + node.min_band_diff;
        if (!found_best_range || range_score < best_score) {
            found_best_range = true;
            best_all_ord = node.min_all_ord;
            best_min_band_diff = node.min_band_diff;
            best_without_band_diff = without_band_diff;
            best_score = range_score;
        }
    }
    for (int q_idx: direct_band_q_idxs) add_band_range_to_min_seg(seg, prec.a2b_band_ranges[q_idx], +1);
    my_assert(found_best_range);
    if (is_collector_log_enabled()) {
        outaaa("mo_pair_range_score",
               "a2b_idx =", a2b_idx,
               ", b2a_idx =", b2a_idx,
               ", ord_ranges =", task_ord_ranges_to_debug_string(candidates, task),
               ", direct_band_q_idxs =", int_vector_to_debug_string(direct_band_q_idxs),
               ", selected_all_ord =", best_all_ord,
               ", min_band_diff =", best_min_band_diff,
               ", without_band_diff =", best_without_band_diff,
               ", score =", best_score);
        ::out2("pair_range_score",
               "a2b_idx =", a2b_idx,
               ", b2a_idx =", b2a_idx,
               ", selected_all_ord =", best_all_ord,
               ", min_band_diff =", best_min_band_diff,
               ", without_band_diff =", best_without_band_diff,
               ", score =", best_score);
    }
    return ScoredPair{PairQidx{a2b_idx, b2a_idx}, AllOrd(best_all_ord), best_score};
}

// 責務: pair_tasks の index 列を Mo order で sort し、BandMinSeg の active q_idx 範囲を更新しながら best pair を返す。
// 必要な理由: all_ord range 列を持つ重い task 本体の sort と、pair ごとの band 再構築を避けるため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::BestPlanResult
GreedyQueriesConstructor<MAX_N>::Helper::solve_pair_range_tasks(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const BasePairCandidates &candidates,
        InsertPairPrecalc &prec
) {
    BestPlanResult result;
    const auto &pair_tasks = candidates.pair_tasks;
    if (pair_tasks.empty()) return result;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    int block_size = 1;
    while (block_size * block_size < qcnt + 1) block_size++;
    static vec_array<int, BASE_PAIR_TASK_CAPACITY> task_order;
    task_order.clear();
    for (int idx = 0; idx < static_cast<int>(pair_tasks.size()); idx++) task_order.push_back(idx);
    std::sort(task_order.begin(), task_order.end(), [&](int lhs_idx, int rhs_idx) {
        const PairRangeTask &lhs = pair_tasks[lhs_idx];
        const PairRangeTask &rhs = pair_tasks[rhs_idx];
        int lhs_block = lhs.a2b_idx / block_size;
        int rhs_block = rhs.a2b_idx / block_size;
        if (lhs_block != rhs_block) return lhs_block < rhs_block;
        if (lhs_block & 1) return lhs.b2a_idx > rhs.b2a_idx;
        return lhs.b2a_idx < rhs.b2a_idx;
    });
    const int b_insert_slot_count =
            calc_insert_slot_count(yst.query_order.border.get_all_order_entries_size());
    BandMinSeg seg;
    seg.init(b_insert_slot_count);
    int cur_l = 0;
    int cur_r = 0;
    auto add_q_idx = [&](int q_idx, int delta) {
        my_assert(0 <= q_idx && q_idx < qcnt);
        add_band_range_to_min_seg(seg, prec.a2b_band_ranges[q_idx], delta);
    };
    for (int task_idx: task_order) {
        const PairRangeTask &task = pair_tasks[task_idx];
        while (cur_l > task.a2b_idx) add_q_idx(--cur_l, +1);
        while (cur_r < task.b2a_idx) add_q_idx(cur_r++, +1);
        while (cur_l < task.a2b_idx) add_q_idx(cur_l++, -1);
        while (cur_r > task.b2a_idx) add_q_idx(--cur_r, -1);
        ScoredPair scored_pair = eval_pair_range_task(yst, init_st, v, candidates, task, seg, prec);
        update_best_result(result, scored_pair);
        if (G::use_pair_refine) insert_top_pair_result(result, scored_pair, G::pair_refine_top_k);
    }
    if (G::use_pair_refine) validate_top_pairs_debug(result, G::pair_refine_top_k);
    return result;
}

// 責務: best pair が候補化されるために必要だった limit と retry 同幅 ext を、既存 range 判定で求める。
// 必要な理由: retry 増大の原因が cost limit か range ext かを best pair 基準で特定するため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::BestPairNeed
GreedyQueriesConstructor<MAX_N>::Helper::calc_best_pair_need(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const ScoredPair &best_pair,
        const InsertPairPrecalc &prec,
        const my_bitset<MAX_N> &is_never_construct_v
) {
    BestPairNeed need;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const int state_n = init_st.current_N;
    const int a2b_idx = best_pair.pair_qidx.a2b_idx;
    const int b2a_idx = best_pair.pair_qidx.b2a_idx;
    const int all_ord_b = best_pair.all_ord.get();
    const OrdRange single_range{all_ord_b, all_ord_b + 1};

    OwnedA2BRange owner = build_owned_a2b_range(yst, init_st, v, a2b_idx, single_range, prec);
    need.required_a2b_lim = owner.a2b_insert_turn + owner.direct1_diff + owner.direct2_diff;
    B2ARangeCost b2a_cost = build_b2a_range_cost(yst, init_st, v, b2a_idx, single_range, prec);
    need.required_b2a_lim = calc_b2a_range_total(b2a_cost, qcnt, prec);

    auto scan_before = [&](int target_q_idx) {
        QueriesCalculator qs_calc(init_st.current_N, yst.queries_state.queries);
        QueryScanState scan_state = init_scan_state(yst, init_st);
        for (int q_idx = 0; q_idx < target_q_idx; q_idx++) {
            const QueryCommonRI &qri = yst.queries_state.queries[q_idx];
            advance_scan_state(yst, scan_state, qs_calc, q_idx, qri);
        }
        return scan_state;
    };
    QueryScanState a2b_scan = scan_before(a2b_idx);
    QueryScanState b2a_scan = scan_before(b2a_idx);

    auto make_ab_range_at = [&](int q_idx, const QueryScanState &scan_state, const RangeExt &range_ext) {
        if (q_idx < qcnt) {
            const QueryCommonRI &qri = yst.queries_state.queries[q_idx];
            return make_ab_idx_range(
                    yst, scan_state, prec.next_ab_idxs, q_idx, qri,
                    range_ext, is_never_construct_v, init_st.current_N);
        }
        return make_final_ab_idx_range(
                yst.queries_state, scan_state, range_ext, is_never_construct_v, init_st.current_N);
    };
    const int b_all_ord_size = yst.query_order.border.get_all_order_entries_size();
    auto b_range_contains = [&](const QueryScanState &scan_state, const AbIdxRange &ab_range) {
        BAllOrdRange b_range =
                make_previsional_b_ord_range(yst, scan_state, ab_range.b_idx_range, b_all_ord_size);
        return !b_range.range.is_empty() && b_range.range.contains(all_ord_b);
    };
    auto find_min_uniform_ext = [&](const auto &passes) {
        for (int ext = 0; ext <= state_n; ext++) {
            RangeExt range_ext{ext, ext, ext, ext};
            if (passes(range_ext)) return ext;
        }
        return -1;
    };
    need.required_a2b_a_ext = find_min_uniform_ext([&](const RangeExt &range_ext) {
        AbIdxRange ab_range = make_ab_range_at(a2b_idx, a2b_scan, range_ext);
        return contains_init_v_in_a_range(yst, a2b_scan, v, ab_range);
    });
    need.required_a2b_b_ext = find_min_uniform_ext([&](const RangeExt &range_ext) {
        AbIdxRange ab_range = make_ab_range_at(a2b_idx, a2b_scan, range_ext);
        return b_range_contains(a2b_scan, ab_range);
    });
    need.required_b2a_a_ext = find_min_uniform_ext([&](const RangeExt &range_ext) {
        AbIdxRange ab_range = make_ab_range_at(b2a_idx, b2a_scan, range_ext);
        return contains_pushed_v_in_a_range(yst, b2a_scan, v, ab_range);
    });
    need.required_b2a_b_ext = find_min_uniform_ext([&](const RangeExt &range_ext) {
        AbIdxRange ab_range = make_ab_range_at(b2a_idx, b2a_scan, range_ext);
        return b_range_contains(b2a_scan, ab_range);
    });
    auto max_required = [](int lhs, int rhs) {
        if (lhs < 0 || rhs < 0) return -1;
        return std::max(lhs, rhs);
    };
    need.required_from_ext_a = max_required(need.required_a2b_a_ext, need.required_b2a_a_ext);
    need.required_to_ext_a = need.required_from_ext_a;
    need.required_from_ext_b = max_required(need.required_a2b_b_ext, need.required_b2a_b_ext);
    need.required_to_ext_b = need.required_from_ext_b;
    return need;
}

// 責務: calc_best_pair_need の結果を 1 行の timing log として出す。
// 必要な理由: ログ末尾から retry 増大の直接要因を読み取れるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::write_best_pair_need(
        int v,
        const ScoredPair &best_pair,
        const BestPairNeed &need
) {
    std::ostringstream ss;
    ss << "stage=best_pair_need"
       << " v=" << v
       << " a2b_idx=" << best_pair.pair_qidx.a2b_idx
       << " b2a_idx=" << best_pair.pair_qidx.b2a_idx
       << " all_ord=" << best_pair.all_ord.get()
       << " diff=" << best_pair.diff
       << " required_a2b_lim=" << need.required_a2b_lim
       << " required_b2a_lim=" << need.required_b2a_lim
       << " required_from_ext_a=" << need.required_from_ext_a
       << " required_to_ext_a=" << need.required_to_ext_a
       << " required_from_ext_b=" << need.required_from_ext_b
       << " required_to_ext_b=" << need.required_to_ext_b
       << " required_a2b_a_ext=" << need.required_a2b_a_ext
       << " required_a2b_b_ext=" << need.required_a2b_b_ext
       << " required_b2a_a_ext=" << need.required_b2a_a_ext
       << " required_b2a_b_ext=" << need.required_b2a_b_ext
       << " ext_mode=uniform_retry";
    G::write_timing_log(ss.str());
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::start_pair_best_scores(PairBestScoreTable &table) {
    if (table.current_stamp == INT_MAX) {
        table.stamps.fill(0);
        table.current_stamp = 1;
        return;
    }
    table.current_stamp++;
}

// 責務: 指定 pair の score が現在世代で登録済みか判定する。
template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::Helper::has_pair_score(
        const PairBestScoreTable &table,
        const PairQidx &pair_qidx
) {
    return table.stamps[pair_score_index(pair_qidx.a2b_idx, pair_qidx.b2a_idx)] == table.current_stamp;
}

// 責務: 指定 pair の現在世代の best score を返す。
template<size_t MAX_N>
const typename GreedyQueriesConstructor<MAX_N>::Helper::ScoredPair &
GreedyQueriesConstructor<MAX_N>::Helper::get_pair_best_score(
        const PairBestScoreTable &table,
        const PairQidx &pair_qidx
) {
    my_assert(has_pair_score(table, pair_qidx));
    return table.best_scores[pair_score_index(pair_qidx.a2b_idx, pair_qidx.b2a_idx)];
}

// 責務: ScoredPair が同じ pair の既存 score より良ければ table を更新する。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::update_pair_best_score(
        PairBestScoreTable &table,
        const ScoredPair &scored_pair
) {
    const int a2b_idx = scored_pair.pair_qidx.a2b_idx;
    const int b2a_idx = scored_pair.pair_qidx.b2a_idx;
    const size_t score_idx = pair_score_index(a2b_idx, b2a_idx);
    if (table.stamps[score_idx] == table.current_stamp &&
        table.best_scores[score_idx].diff <= scored_pair.diff) {
        return;
    }
    table.stamps[score_idx] = table.current_stamp;
    table.best_scores[score_idx] = scored_pair;
}

// 責務: pairについてのbestなScoredPair を diff 昇順の top_k 配列へ挿入する。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::insert_topk_pair(
        TopKPlanResult &result,
        const ScoredPair &scored_pair,
        int top_k
) {
    int insert_idx = 0;
    while (insert_idx < static_cast<int>(result.topk_pairs.size()) &&
           result.topk_pairs[insert_idx].diff <= scored_pair.diff) {
        insert_idx++;
    }
    if (insert_idx >= top_k) return;
    result.topk_pairs.insert(result.topk_pairs.begin() + insert_idx, scored_pair);
    if (static_cast<int>(result.topk_pairs.size()) > top_k) {
        result.topk_pairs.pop_back();
    }
}

// 責務: collect_result の base_pair_infos を走査し、pair score table の best から TopKPlanResult を作る。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::TopKPlanResult
GreedyQueriesConstructor<MAX_N>::Helper::build_topk_pairs(
        const BasePairCandidates &base_candidates,
        const PairBestScoreTable &table,
        int top_k
) {
    TopKPlanResult result;
    for (const BasePairInfo &info: base_candidates.base_pair_infos) {
        const PairQidx &pair_qidx = info.pair_qidx;
        my_assert(has_pair_score(table, pair_qidx));
        insert_topk_pair(result, get_pair_best_score(table, pair_qidx), top_k);
    }
    return result;
}

// 責務: 半開区間 [begin, end) の OrdRange 群に all_ord が含まれるか返す。
// 必要な理由: 旧候補が新 PairRangeTask の評価範囲から漏れていないか検査するため。
template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::Helper::contains_ord_range(
        const BasePairCandidates &candidates,
        const PairRangeTask &task,
        int all_ord
) {
    for (int range_idx = 0; range_idx < task.range_count; range_idx++) {
        const OrdRange &range = candidates.pair_ranges[task.range_begin + range_idx];
        if (range.begin <= all_ord && all_ord < range.end) return true;
    }
    return false;
}

// 責務: pair_tasks から a2b_idx/b2a_idx が一致する task を返し、なければ nullptr を返す。
// 必要な理由: 旧候補の pair 自体が新候補集合に存在するか検査するため。
template<size_t MAX_N>
const typename GreedyQueriesConstructor<MAX_N>::Helper::PairRangeTask *
GreedyQueriesConstructor<MAX_N>::Helper::find_pair_task(
        const BasePairCandidates &candidates,
        const PairQidx &pair_qidx
) {
    for (const PairRangeTask &task: candidates.pair_tasks) {
        if (task.a2b_idx == pair_qidx.a2b_idx && task.b2a_idx == pair_qidx.b2a_idx) {
            return &task;
        }
    }
    return nullptr;
}

// 責務: 旧 base_cands_by_allord の各候補について、同じ pair の新 task と all_ord を含む range の有無を確認する。
// 必要な理由: 新方式が旧方式の候補集合を包含できているかを問題発生時に切り分けるため。
template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::Helper::check_legacy_coverage(
        const LegacyBasePairCandidates &legacy_candidates,
        const BasePairCandidates &new_candidates
) {
    int missing_count = 0;
    for (int all_ord = 0; all_ord < MAX_INSERT_SLOT_B; all_ord++) {
        for (const PairQidx &pair_qidx: legacy_candidates.base_cands_by_allord[all_ord]) {
            const PairRangeTask *task = find_pair_task(new_candidates, pair_qidx);
            if (task != nullptr && contains_ord_range(new_candidates, *task, all_ord)) continue;
            missing_count++;
            out_legacy_check_log("legacy_candidate_missing",
                                 "a2b_idx =", pair_qidx.a2b_idx,
                                 ", b2a_idx =", pair_qidx.b2a_idx,
                                 ", all_ord =", all_ord,
                                 ", pair_task_exists =", task != nullptr ? 1 : 0);
            my_assert(false);
        }
    }
    return missing_count;
}

// 責務: BandRangeInfo が有効で、band_range が all_ord_b を含む場合に true を返す。
// 必要な理由: 旧候補 triple の手数一致検査で band contribution を愚直に数えるため。
template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::Helper::band_contains_all_ord(
        const BandRangeInfo &band,
        int all_ord_b
) {
    if (!band.has_band) return false;
    return band.band_range.contains(all_ord_b);
}

// 責務: eval_pair_range_task と同じ A2B direct1/direct2 条件で q_idx を除外するか返す。
// 必要な理由: 旧候補 triple の固定評価でも range 評価と同じ direct band 除外を適用するため。
template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::Helper::is_direct_excluded_q_idx(
        int q_idx,
        int a2b_idx,
        int b2a_idx,
        InsertPairPrecalc &prec,
        int qcnt
) {
    if (q_idx < a2b_idx || b2a_idx <= q_idx) return false;
    int direct1_idx = normalize_a2b_direct1_idx(a2b_idx, prec, qcnt);
    int direct2_idx = normalize_a2b_direct2_idx(direct1_idx, a2b_idx, prec, qcnt);
    if (q_idx == direct1_idx) return true;
    return q_idx == direct2_idx && direct2_idx != direct1_idx;
}

// 責務: calc_diff_without_band と direct 除外後 band scan を合算し、指定 all_ord の新方式 diff を返す。
// 必要な理由: 旧評価と新評価の手数ずれを候補単位で即 assert できるようにするため。
template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::Helper::eval_pair_fixed_all_ord_diff(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const PairQidx &pair_qidx,
        int all_ord_b,
        InsertPairPrecalc &prec
) {
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const int a2b_idx = pair_qidx.a2b_idx;
    const int b2a_idx = pair_qidx.b2a_idx;
    my_assert(0 <= a2b_idx && a2b_idx <= b2a_idx && b2a_idx <= qcnt);
    int band_diff = 0;
    for (int q_idx = a2b_idx; q_idx < b2a_idx; q_idx++) {
        if (is_direct_excluded_q_idx(q_idx, a2b_idx, b2a_idx, prec, qcnt)) continue;
        if (band_contains_all_ord(prec.a2b_band_ranges[q_idx], all_ord_b)) band_diff++;
    }
    const int without_band_diff = calc_diff_without_band(
            yst, init_st, v, a2b_idx, b2a_idx, AllOrd(all_ord_b), prec);
    return without_band_diff + band_diff;
}

// 責務: 旧候補 triple を旧評価と新方式固定評価で比べ、差が出た瞬間にログして assert する。
// 必要な理由: 手数ずれを集計後ではなく、問題候補を見つけたその場で止めるため。
template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::Helper::check_legacy_diffs(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const LegacyBasePairCandidates &legacy_candidates,
        InsertPairPrecalc &prec
) {
    int mismatch_count = 0;
    my_bitset<MAX_QUERY_CNT> is_band;
    for (int all_ord_b = 0; all_ord_b < MAX_INSERT_SLOT_B; all_ord_b++) {
        for (auto in_band_q_idx: prec.b_band_start_qidxs[all_ord_b]) {
            is_band[in_band_q_idx] = true;
        }
        for (auto out_band_q_idx: prec.b_band_end_qidxs[all_ord_b]) {
            is_band[out_band_q_idx] = false;
        }
        for (const PairQidx &pair_qidx: legacy_candidates.base_cands_by_allord[all_ord_b]) {
            ScoredPair legacy_scored =
                    eval_insert_pair_qidx(yst, init_st, v, pair_qidx, all_ord_b, is_band, prec);
            const int fixed_diff =
                    eval_pair_fixed_all_ord_diff(yst, init_st, v, pair_qidx, all_ord_b, prec);
            if (legacy_scored.diff == fixed_diff) continue;
            mismatch_count++;
            out_legacy_check_log("legacy_diff_mismatch",
                                 "v =", v,
                                 ", a2b_idx =", pair_qidx.a2b_idx,
                                 ", b2a_idx =", pair_qidx.b2a_idx,
                                 ", all_ord =", all_ord_b,
                                 ", legacy_diff =", legacy_scored.diff,
                                 ", fixed_new_diff =", fixed_diff);
            my_assert(false);
        }
    }
    return mismatch_count;
}

// 責務: 比較用旧候補を作り、旧 best と新候補への包含漏れを LegacyCheckResult に集約する。
// 必要な理由: construct 本線の採用結果を変えずに、悪化原因をログから追えるようにするため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::LegacyCheckResult
GreedyQueriesConstructor<MAX_N>::Helper::run_legacy_check(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        const BasePairCandidates &new_candidates,
        const BestPlanResult &new_result,
        InsertPairPrecalc &prec,
        const GreedyConstructorParams &params,
        const my_bitset<MAX_N> &is_never_construct_v
) {
    LegacyCheckResult result;
    LegacyBasePairCandidates &legacy_candidates = legacy_pair_candidates_workspace();
    legacy_candidates.clear();
    collect_candidates_legacy_check(
            yst, current_st, v, prec, params, is_never_construct_v, legacy_candidates);
    result.legacy_best =
            scan_candidates_for_best(yst, current_st, v, legacy_candidates.base_cands_by_allord,
                                     prec, result.legacy_count);
    result.missing_count =
            check_legacy_coverage(legacy_candidates, new_candidates);
    result.diff_mismatch_count =
            check_legacy_diffs(yst, current_st, v, legacy_candidates, prec);
    log_legacy_check(v, new_result, result);
    return result;
}

// 責務: 旧候補 best を作り、新 owner range best と found/pair/all_ord/diff が違えば詳細を標準エラーへ出して abort する。
// 必要な理由: my_assert が無効な実行でも、A2B owner range 化で best pair 選択が変わった最初の地点を特定するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::abort_if_legacy_best_pair_differs(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        const BestPlanResult &new_result,
        InsertPairPrecalc &prec,
        const GreedyConstructorParams &params,
        const my_bitset<MAX_N> &is_never_construct_v
) {
    LegacyBasePairCandidates &legacy_candidates = legacy_pair_candidates_workspace();
    legacy_candidates.clear();
    collect_candidates_legacy_check(
            yst, current_st, v, prec, params, is_never_construct_v, legacy_candidates);
    int legacy_count = 0;
    BestPlanResult legacy_result =
            scan_candidates_for_best(yst, current_st, v, legacy_candidates.base_cands_by_allord,
                                     prec, legacy_count);
    const bool differs =
            legacy_result.found != new_result.found ||
            (legacy_result.found && new_result.found &&
             (legacy_result.best_pair.pair_qidx.a2b_idx != new_result.best_pair.pair_qidx.a2b_idx ||
              legacy_result.best_pair.pair_qidx.b2a_idx != new_result.best_pair.pair_qidx.b2a_idx ||
              legacy_result.best_pair.all_ord.get() != new_result.best_pair.all_ord.get() ||
              legacy_result.best_pair.diff != new_result.best_pair.diff));
    if (!differs) return;
    std::cerr << "[LEGACY BEST PAIR DIFF] v=" << v
              << " legacy_count=" << legacy_count
              << " new_found=" << (new_result.found ? 1 : 0)
              << " legacy_found=" << (legacy_result.found ? 1 : 0);
    if (new_result.found) {
        std::cerr << " new_diff=" << new_result.best_pair.diff
                  << " new_a2b_idx=" << new_result.best_pair.pair_qidx.a2b_idx
                  << " new_b2a_idx=" << new_result.best_pair.pair_qidx.b2a_idx
                  << " new_all_ord=" << new_result.best_pair.all_ord.get();
    }
    if (legacy_result.found) {
        std::cerr << " legacy_diff=" << legacy_result.best_pair.diff
                  << " legacy_a2b_idx=" << legacy_result.best_pair.pair_qidx.a2b_idx
                  << " legacy_b2a_idx=" << legacy_result.best_pair.pair_qidx.b2a_idx
                  << " legacy_all_ord=" << legacy_result.best_pair.all_ord.get();
    }
    std::cerr << std::endl;
    std::abort();
}

// 責務: 旧候補数、包含漏れ数、diff 不一致数、新旧 best を summary と悪化ログとして出す。
// 必要な理由: 新経路が旧 best より悪い時に、候補漏れか評価差かをログから判断できるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::log_legacy_check(
        int v,
        const BestPlanResult &new_result,
        const LegacyCheckResult &check_result
) {
    const bool legacy_found = check_result.legacy_best.found;
    out_legacy_check_log("legacy_check_summary",
                         "v =", v,
                         ", legacy_count =", check_result.legacy_count,
                         ", missing_count =", check_result.missing_count,
                         ", diff_mismatch_count =", check_result.diff_mismatch_count,
                         ", new_found =", new_result.found ? 1 : 0,
                         ", new_diff =", new_result.found ? new_result.best_pair.diff : 0,
                         ", new_a2b_idx =", new_result.found ? new_result.best_pair.pair_qidx.a2b_idx : -1,
                         ", new_b2a_idx =", new_result.found ? new_result.best_pair.pair_qidx.b2a_idx : -1,
                         ", new_all_ord =", new_result.found ? new_result.best_pair.all_ord.get() : -1,
                         ", legacy_found =", legacy_found ? 1 : 0,
                         ", legacy_diff =", legacy_found ? check_result.legacy_best.best_pair.diff : 0,
                         ", legacy_a2b_idx =",
                         legacy_found ? check_result.legacy_best.best_pair.pair_qidx.a2b_idx : -1,
                         ", legacy_b2a_idx =",
                         legacy_found ? check_result.legacy_best.best_pair.pair_qidx.b2a_idx : -1,
                         ", legacy_all_ord =",
                         legacy_found ? check_result.legacy_best.best_pair.all_ord.get() : -1);
    if (new_result.found && legacy_found &&
        check_result.legacy_best.best_pair.diff < new_result.best_pair.diff) {
        out_legacy_check_log("legacy_new_worse",
                             "v =", v,
                             ", new_diff =", new_result.best_pair.diff,
                             ", legacy_diff =", check_result.legacy_best.best_pair.diff,
                             ", new_a2b_idx =", new_result.best_pair.pair_qidx.a2b_idx,
                             ", new_b2a_idx =", new_result.best_pair.pair_qidx.b2a_idx,
                             ", new_all_ord =", new_result.best_pair.all_ord.get(),
                             ", legacy_a2b_idx =", check_result.legacy_best.best_pair.pair_qidx.a2b_idx,
                             ", legacy_b2a_idx =", check_result.legacy_best.best_pair.pair_qidx.b2a_idx,
                             ", legacy_all_ord =", check_result.legacy_best.best_pair.all_ord.get());
        my_assert(false);
    }
}

// 責務: base_result の topk_pairs[0] と topk_expanded_result の best のうち diff が良い方を返す。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::ScoredPair
GreedyQueriesConstructor<MAX_N>::Helper::choose_best_plan(
        const TopKPlanResult &base_result,
        const BestPlanResult &topk_expanded_result
) {
    my_assert(!base_result.topk_pairs.empty() || topk_expanded_result.found);
    if (!topk_expanded_result.found) return base_result.topk_pairs[0];
    if (base_result.topk_pairs.empty()) return topk_expanded_result.best_pair;
    if (topk_expanded_result.best_pair.diff < base_result.topk_pairs[0].diff) {
        return topk_expanded_result.best_pair;
    }
    return base_result.topk_pairs[0];
}

// 責務: 各 q_idx 以降で最初に現れる A2B/B2A query の index を返す。
// 必要な理由: 挿入 A2A から最初の AB までは PrevA2ARot を含めて再計算し、それ以降だけ prefix sum を使うため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::QueryBoundaryArray
GreedyQueriesConstructor<MAX_N>::Helper::build_next_ab_q_idx(const QRIList &queries) {
    const int qcnt = static_cast<int>(queries.size());
    QueryBoundaryArray next_ab = make_query_boundary_array(qcnt + 1, qcnt);
    int next_idx = qcnt;
    for (int q_idx = qcnt - 1; q_idx >= 0; q_idx--) {
        if (queries[q_idx].is_ab_type()) {
            next_idx = q_idx;
        }
        next_ab[q_idx] = next_idx;
    }
    return next_ab;
}

// 責務: 既存 TARGET(v) を移動する候補 slot を、AOrder::insert_update_no_shift 後の実 AllOrd へ変換する。
// 必要な理由: 既存 TARGET を erase してから insert する実適用側の target-- 仕様と、fast 評価側の effect 座標を一致させるため。
template<size_t MAX_N>
AllOrd GreedyQueriesConstructor<MAX_N>::Helper::resolve_moved_target_ord(
        const YakinamashiState &yst,
        int v,
        AllOrd target_all_ord
) {
    const my_vector<OrderEntry> &entries = yst.query_order.aorder.get_order_entries();
    int target = target_all_ord.get();
    my_assert(0 <= target && target <= static_cast<int>(entries.size()));
    int prev_idx = -1;
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        const OrderEntry &entry = entries[i];
        if (entry.value == v && entry.val_state == ValState::TARGET) {
            prev_idx = i;
            break;
        }
    }
    my_assert(prev_idx != -1);
    if (prev_idx < target) target--;
    return AllOrd(target);
}

// 責務: A2A 挿入後に後続 query へ掛かる A 側 net-effect を作る。
// 必要な理由: A2B+B2A の net-effect と同じ形で、INIT(v) 削除と TARGET(v) 追加による OrdPos/RelOrd 差分を評価するため。
template<size_t MAX_N>
QueryOrdEffect GreedyQueriesConstructor<MAX_N>::Helper::build_a2a_effect(
        const YakinamashiState &yst,
        int v,
        AllOrd target_all_ord
) {
    QueryOrdEffect effect;
    effect.a_effect.set_del(yst.query_order.aorder.get_init_order(v));
    effect.a_effect.set_add(target_all_ord);
    effect.diff_stack_a_cnt = 0;
    return effect;
}

// 責務: q_idx 直前の存在集合と candidate target AllOrd から、仮 A2A QueryCommon を作る。
// 必要な理由: 候補評価ごとに AOrder を実更新せず、target AllOrd だけを差し替えて A2A 自身の手数を計算するため。
template<size_t MAX_N>
QueryCommon GreedyQueriesConstructor<MAX_N>::Helper::build_fast_a2a_query(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        int q_idx,
        AllOrd target_all_ord,
        const QueryScanState& scan_state
) {
    my_assert(scan_state.now_a_val_state[v] == ValState::INIT);
    AllOrd init_all_ord = yst.query_order.aorder.get_init_order(v);
    OrdPos tar_ord_a1 = to_ord_pos(
            OrdCalc::get_rel_ord(scan_state.exist_a_ords, init_all_ord),
            scan_state.st.A.size());
    RelOrd tar_rel_ord_a2 = OrdCalc::get_rel_ord(scan_state.exist_a_ords, target_all_ord);
    validate_RelOrd(tar_rel_ord_a2, scan_state.st.A.size());
    OrdPos tar_ord_a2 = to_ord_pos(tar_rel_ord_a2, scan_state.st.A.size());
    int flip_dis_dir = A2AQueryUtil::calc_flip_dis_dir(
            tar_ord_a1, tar_ord_a2, scan_state.st.A.size(), e_a2a_type::swap_rot);
    OrdPos top_ord_b = get_query_start_ord_b(yst.queries_state, q_idx, current_st.current_N);
    return QueryCommon(
            CommonPushType::A2AS,
            v,
            scan_state.st.A.size(),
            tar_ord_a1.get(),
            tar_rel_ord_a2.get(),
            top_ord_b.get(),
            flip_dis_dir
    );
}

// 責務: 既存 AB query に A2A effect を適用した時の固定 turn 差分を返し、scan ctx の A top 状態を進める。
// 必要な理由: A2A 候補の最初の AB より後ろの suffix 差分を、候補 AllOrd ごとの prefix sum で評価するため。
template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::Helper::calc_fixed_a2a_insert_diff_ab(
        const YakinamashiState &yst,
        int q_idx,
        const QueryCommonRI &qri,
        ConstructVScanCtx &ctx
) {
    const int n = yst.queries_state.current_N;
    QueryCommon applied_query = ctx.provider.get_applied_common(
            ctx.base_prev_finished_top_ord_b, qri.get_query(), ctx.a_only_a2b_effect);
    if (is_only_direct(q_idx, applied_query, ctx)) {
        ctx.effected_prev_finished_top_ord_a = applied_query.get_finished_top_ord_a(n);
        return 0;
    }
    const int stack_a_cnt = applied_query.get_stack_a_cnt();
    const int stack_b_cnt = applied_query.get_stack_b_cnt(n);
    OrdPos tar_a = qri.is_a2b_type()
                   ? applied_query.get_tar_ord_pos_a_A2B()
                   : to_ord_pos(applied_query.get_tar_rel_ord_a_B2A(), stack_a_cnt);
    OrdPos tar_b = qri.is_a2b_type()
                   ? to_ord_pos(applied_query.get_tar_rel_ord_b_A2B(), stack_b_cnt)
                   : applied_query.get_tar_ord_pos_b_B2A();
    BestRotInfo bri = build_brp_by_ord_with_prev_a2a(
            stack_a_cnt,
            stack_b_cnt,
            ctx.effected_prev_finished_top_ord_a,
            ctx.base_prev_finished_top_ord_b,
            tar_a,
            tar_b,
            ctx.effected_prev_a2a_rot_sum
    );
    ctx.effected_prev_finished_top_ord_a = applied_query.get_finished_top_ord_a(n);
    return bri.ins_turn - qri.get_ins_turn();
}

// 責務: candidate target AllOrd による A2A effect を全既存 query に適用し、fixed diff prefix と最初の AB 探索表を作る。
// 必要な理由: 各 q_idx の A2A 候補評価で、direct1 以外の後続差分を O(1) の prefix sum として使うため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::A2AFastPrecalc &
GreedyQueriesConstructor<MAX_N>::Helper::build_a2a_fast_precalc(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        AllOrd target_all_ord
)
{
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const int n = yst.queries_state.current_N;
    static A2AFastPrecalc precalc;
    precalc.clear();
    precalc.effect = build_a2a_effect(yst, v, target_all_ord);
    make_query_common_array(precalc.effected_queries, qcnt);
    QueryIntArray fixed_diff = make_query_int_array(qcnt, 0);
    QueryIntArray indirect_ra = make_query_int_array(qcnt, 0);
    QueryIntArray indirect_rra = make_query_int_array(qcnt, 0);
    QueryIntArray effected_valid = make_query_int_array(qcnt, 0);
    ConstructVScanCtx ctx{
            precalc.effect,
            {},
            QueryProvider(yst, current_st),
            QCRIFactory(),
            yst.queries_state.first_top_ord_a,
            yst.queries_state.first_top_ord_b
    };
    for (int q_idx = 0; q_idx < qcnt; q_idx++) {
        const QueryCommonRI &qri = yst.queries_state.queries[q_idx];
        precalc.effected_queries[q_idx] = ctx.provider.get_applied_common(
                ctx.base_prev_finished_top_ord_b, qri.get_query(), precalc.effect);
        if (qri.is_a2a_type()) {
            fixed_diff[q_idx] = calc_fixed_diff_a2a(
                    q_idx, qri.get_tar_v(), yst, qri, ctx, indirect_ra, indirect_rra, effected_valid);
        } else {
            effected_valid[q_idx] = 1;
            fixed_diff[q_idx] = calc_fixed_a2a_insert_diff_ab(yst, q_idx, qri, ctx);
        }
        if (qri.is_ab_type()) update_scan_ctx_after_ab(n, yst, qri, ctx, q_idx);
    }
    precalc.next_ab_q_idx = build_next_ab_q_idx(yst.queries_state.queries);
    precalc.fixed_diff_prefix_sum = build_query_prefix_sum(fixed_diff);
    precalc.final_top_ord_a = ctx.effected_prev_finished_top_ord_a;
    auto [cmd, cnt] = FinalRotCalculator::build_final_rot(n, precalc.final_top_ord_a);
    (void) cmd;
    precalc.final_rot_diff = cnt - yst.queries_state.final_rot_info.second;
    return precalc;
}

// 責務: 挿入位置直前の既存 A2A chain と挿入 A2A 自身の同期回転量を合計して返す。
// 必要な理由: 実適用側の QCRIFactory は AB 直前の連続 A2A 全体を calc_prev_range_P で集計するため。
template<size_t MAX_N>
PrevA2ARotSum GreedyQueriesConstructor<MAX_N>::Helper::build_a2a_replay_initial_rot_sum(
        const QRIList &queries,
        int q_idx,
        const QueryCommonRI &inserted_a2a_qri
) {
    PrevA2ARotSum existing_rot_sum(0, 0);
    if (q_idx > 0) {
        existing_rot_sum = PrevA2ARotSumCalc::calc_prev_range_P(queries, q_idx - 1);
    }
    auto [inserted_ra_sum, inserted_rra_sum] = inserted_a2a_qri.get_can_sync_ra_rra_A2A();
    return PrevA2ARotSum(
            existing_rot_sum.ra_sum + inserted_ra_sum,
            existing_rot_sum.rra_sum + inserted_rra_sum);
}

// 責務: 挿入 A2A 直後の A2A chain と最初の AB query を順に再計算し、その差分を返す。
// 必要な理由: 最初の AB は挿入 A2A と途中 A2A の PrevA2ARotSum に依存し、固定 prefix では start top と同期回転量がずれるため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::A2AReplayResult
GreedyQueriesConstructor<MAX_N>::Helper::calc_a2a_replay_diff(
        const YakinamashiState &yst,
        const State &current_st,
        int q_idx,
        const QueryCommonRI &inserted_a2a_qri,
        const A2AFastPrecalc &precalc
) {
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const int n = yst.queries_state.current_N;
    const int first_ab_idx = precalc.next_ab_q_idx[q_idx];
    QueryProvider provider(yst, current_st);
    QCRIFactory qfac;
    A2AReplayResult result;
    result.first_ab_idx = first_ab_idx;
    result.finished_top_a = inserted_a2a_qri.get_finished_top_ord_a();
    OrdPos current_top_b = get_debug_start_ord_b_before_q_idx(yst, q_idx, n);
    PrevA2ARotSum rot_sum = build_a2a_replay_initial_rot_sum(
            yst.queries_state.queries, q_idx, inserted_a2a_qri);
    for (int replay_idx = q_idx; replay_idx < first_ab_idx; replay_idx++) {
        const QueryCommonRI &base_qri = yst.queries_state.queries[replay_idx];
        my_assert(base_qri.is_a2a_type());
        QueryCommon effected_q = provider.get_applied_common(
                current_top_b, base_qri.get_query(), precalc.effect);
        QueryCommonRI effected_qri = qfac.build_a2a_ri_from_start_top(result.finished_top_a, effected_q);
        auto [ra_sum, rra_sum] = effected_qri.get_can_sync_ra_rra_A2A();
        rot_sum.ra_sum += ra_sum;
        rot_sum.rra_sum += rra_sum;
        result.diff += effected_qri.get_ins_turn() - base_qri.get_ins_turn();
        result.finished_top_a = effected_qri.get_finished_top_ord_a();
    }
    if (first_ab_idx == qcnt) {
        result.has_first_ab = false;
        return result;
    }
    result.has_first_ab = true;
    const QueryCommonRI &base_qri = yst.queries_state.queries[first_ab_idx];
    const QueryCommon &effected_q = precalc.effected_queries[first_ab_idx];
    const int stack_a_cnt = effected_q.get_stack_a_cnt();
    const int stack_b_cnt = effected_q.get_stack_b_cnt(n);
    OrdPos tar_a = base_qri.is_a2b_type()
                   ? effected_q.get_tar_ord_pos_a_A2B()
                   : to_ord_pos(effected_q.get_tar_rel_ord_a_B2A(), stack_a_cnt);
    OrdPos tar_b = base_qri.is_a2b_type()
                   ? to_ord_pos(effected_q.get_tar_rel_ord_b_A2B(), stack_b_cnt)
                   : effected_q.get_tar_ord_pos_b_B2A();
    BestRotInfo bri = build_brp_by_ord_with_prev_a2a(
            stack_a_cnt,
            stack_b_cnt,
            result.finished_top_a,
            current_top_b,
            tar_a,
            tar_b,
            rot_sum);
    result.diff += bri.ins_turn - base_qri.get_ins_turn();
    result.finished_top_a = effected_q.get_finished_top_ord_a(n);
    return result;
}

// 責務: target AllOrd と q_idx の A2A 候補について、最初の AB まで正確再計算した fast diff を返す。
// 必要な理由: 挿入 A2A 直後の PrevA2ARot 依存を保ちつつ、最初の AB より後ろは prefix sum で評価するため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::A2ACandidate
GreedyQueriesConstructor<MAX_N>::Helper::eval_a2a_candidate_fast(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        int q_idx,
        AllOrd target_all_ord,
        const A2AFastPrecalc &precalc,
        const QueryScanState& scan_state
) {
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const int n = yst.queries_state.current_N;
    QueryCommon a2a_query = build_fast_a2a_query(yst, current_st, v, q_idx, target_all_ord, scan_state);
    QCRIFactory qfac;
    OrdPos start_a = get_debug_start_ord_a_before_q_idx(yst, q_idx);
    QueryCommonRI inserted_a2a_qri = qfac.build_a2a_ri_from_start_top(start_a, a2a_query);
    int diff = inserted_a2a_qri.get_ins_turn();
    A2AReplayResult replay = calc_a2a_replay_diff(yst, current_st, q_idx, inserted_a2a_qri, precalc);
    diff += replay.diff;
    if (replay.has_first_ab) {
        diff += calc_prefix_sum(precalc.fixed_diff_prefix_sum, replay.first_ab_idx + 1, qcnt);
        diff += precalc.final_rot_diff;
    } else {
        auto [cmd, cnt] = FinalRotCalculator::build_final_rot(n, replay.finished_top_a);
        (void) cmd;
        diff += cnt - yst.queries_state.final_rot_info.second;
    }
    A2ACandidate result;
    result.found = true;
    result.q_idx = q_idx;
    result.a_all_ord = target_all_ord;
    result.diff = diff;
    return result;
}

// 責務: 既存 TARGET(v) の値順に基づき、v が min なら min 側、max なら max 側、それ以外は prev/next 間の slot range を返す。
// 必要な理由: 既存 TARGET(v) を no-shift で移動した後も、TARGET 最小 value が先頭基準である前提を保つため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::InsertRangeSet
GreedyQueriesConstructor<MAX_N>::Helper::build_aorder_v_ranges(
        const YakinamashiState &yst,
        const State &current_st,
        int v
) {
    const my_vector<OrderEntry> &entries = yst.query_order.aorder.get_order_entries();
    const int entry_count = static_cast<int>(entries.size());
    InsertRangeSet result;
    int target_count = 0;
    int min_target_v = numeric_limits<int>::max();
    int max_target_v = numeric_limits<int>::min();
    int min_target_pos = -1;
    int max_target_pos = -1;
    bool has_target_v = false;
    for (int pos = 0; pos < entry_count; pos++) {
        const OrderEntry &entry = entries[pos];
        if (entry.val_state != ValState::TARGET) continue;
        target_count++;
        if (entry.value == v) has_target_v = true;
        if (chmi(min_target_v, entry.value)) min_target_pos = pos;
        if (chma(max_target_v, entry.value)) max_target_pos = pos;
    }
    my_assert(has_target_v);
    if (v == min_target_v) {
        my_assert(0 <= min_target_pos && min_target_pos < entry_count);
        result.ranges.push_back({0, min_target_pos + 1});
        return result;
    }
    if (v == max_target_v) {
        my_assert(0 <= max_target_pos && max_target_pos < entry_count);
        result.ranges.push_back({max_target_pos + 1, entry_count + 1});
        return result;
    }
    const TargetNeighborVs neighbors = find_target_neighbors(entries, v, current_st.initial_N);
    const int prev_pos = neighbors.prev_v == -1
                         ? -1
                         : find_entry_pos_by_value_state(entries, neighbors.prev_v, ValState::TARGET);
    const int next_pos = neighbors.next_v == -1
                         ? -1
                         : find_entry_pos_by_value_state(entries, neighbors.next_v, ValState::TARGET);
    return build_circular_insert_ranges(entry_count, prev_pos, next_pos);
}

// 責務: InsertRangeSet の各 [begin, end) slot を AllOrd として callback へ渡す。
// 必要な理由: A2A fast の min/max range は circular range では表せない線形 slot 範囲として扱うため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::for_each_all_ord_in_ranges(
        const InsertRangeSet &ranges,
        const function<void(AllOrd)> &func
) {
    for (const auto &[begin, end]: ranges.ranges) {
        my_assert(0 <= begin && begin <= end);
        for (int all_ord = begin; all_ord < end; all_ord++) {
            func(AllOrd(all_ord));
        }
    }
}

#include "sort_algorithm/YakinamashiQuery/Construction/GreedyFixedBAllOrdConstructor.h"

// 責務: AOrder::normalize_min_front と同じ基準で DEBUG 比較用 entry 列を rotate する。
// 必要な理由: planned_entries は index 0 固定ではなく、normalize 後に同じなら同じ plan と扱うため。
template<size_t MAX_N>
my_vector<OrderEntry> GreedyQueriesConstructor<MAX_N>::Helper::normalize_entries_for_compare(
        my_vector<OrderEntry> entries
) {
    if (entries.empty()) return entries;
    int base_v = entries[0].value;
    for (const OrderEntry &entry: entries) chmi(base_v, entry.value);
    int base_init_idx = -1;
    int base_target_idx = -1;
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        if (entries[i].value != base_v) continue;
        if (entries[i].val_state == ValState::INIT) base_init_idx = i;
        if (entries[i].val_state == ValState::TARGET) base_target_idx = i;
    }
    const int start_idx = base_target_idx != -1 ? base_target_idx : base_init_idx;
    my_assert(0 <= start_idx && start_idx < static_cast<int>(entries.size()));
    if (start_idx != 0) {
        std::rotate(entries.begin(), entries.begin() + start_idx, entries.end());
    }
    return entries;
}

// 責務: 2 つの entry 列を同じ normalize 規則へ通してから比較する。
// 必要な理由: fixed-A DEBUG 検証を plan 作成側と同じ円環同値基準へ揃えるため。
template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::Helper::entries_equal_normalized_debug(
        my_vector<OrderEntry> lhs,
        my_vector<OrderEntry> rhs
) {
    return normalize_entries_for_compare(std::move(lhs)) == normalize_entries_for_compare(std::move(rhs));
}

// 責務: entry 列から target_entry と完全一致する位置を返す。
// 必要な理由: physical restore でも value と ValState の両方が一致する exact entry を anchor として使うため。
template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::Helper::find_optional_entry_pos(
        const my_vector<OrderEntry>& entries,
        const OrderEntry& target_entry
) {
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        if (entries[i] == target_entry) return i;
    }
    return -1;
}

// 責務: plan_pos の entry を現在 AOrder に入れる位置を、plan 上の直前 existing exact entry の直後として返す。
// 必要な理由: physical restore 中も planned_entries の線形 index ではなく円環上の相対順序で復元するため。
template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::Helper::find_plan_insert_pos(
        const my_vector<OrderEntry>& plan_entries,
        const my_vector<OrderEntry>& current_entries,
        int plan_pos
) {
    my_assert(!plan_entries.empty());
    my_assert(0 <= plan_pos && plan_pos < static_cast<int>(plan_entries.size()));
    int prev_pos = plan_pos;
    for (int step = 0; step < static_cast<int>(plan_entries.size()) - 1; step++) {
        prev_pos--;
        if (prev_pos < 0) prev_pos = static_cast<int>(plan_entries.size()) - 1;
        const int current_pos = find_optional_entry_pos(current_entries, plan_entries[prev_pos]);
        if (current_pos != -1) return current_pos + 1;
    }
    my_assert(false);
    return 0;
}

// 責務: 現在の AOrder entry 列が plan.planned_entries と normalize 後に一致することを確認する。
// 必要な理由: fixed-A construct 中に AOrder が動いた場合、即座に検出するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::validate_plan_entries_debug(
        const YakinamashiState &yst,
        const AOrderRangePlan<MAX_N> &plan
) {
#ifdef DEBUG
    my_assert(entries_equal_normalized_debug(yst.query_order.aorder.get_order_entries(), plan.planned_entries));
#else
    (void) yst;
    (void) plan;
#endif
}

// 責務: 現在 AOrder が planned_entries から missing_v の entry を除いた列と normalize 後に一致することを確認する。
// 必要な理由: physical destroy 後から各 v 復元までの段階的 AOrder 復元を検証するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::validate_plan_subset_debug(
        const YakinamashiState &yst,
        const AOrderRangePlan<MAX_N> &plan,
        const my_bitset<MAX_N> &missing_v
) {
#ifdef DEBUG
    my_vector<OrderEntry> expected;
    for (const OrderEntry &entry: plan.planned_entries) {
        if (missing_v[entry.value]) continue;
        expected.push_back(entry);
    }
    my_assert(entries_equal_normalized_debug(yst.query_order.aorder.get_order_entries(), expected));
#else
    (void) yst;
    (void) plan;
    (void) missing_v;
#endif
}

// 責務: stack A/B と AOrder/BOrder に destroyed v が残っていないことを確認する。
// 必要な理由: planned subset 復元の前提となる physical destroy の成功を検出するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::validate_destroyed_absent_debug(
        const PhysicalDestroyContext<MAX_N> &context,
        const AOrderRangePlan<MAX_N> &plan
) {
#ifdef DEBUG
    for (int v: plan.target_vs) {
        for (int i = 0; i < context.current_st.A.size(); i++) my_assert(context.current_st.A[i] != v);
        for (int i = 0; i < context.current_st.B.size(); i++) my_assert(context.current_st.B[i] != v);
        for (const OrderEntry &entry: context.yst.query_order.aorder.get_order_entries()) {
            my_assert(entry.value != v);
        }
        for (const OrderEntry &entry: context.yst.query_order.border.get_order_entries()) {
            my_assert(entry.value != v);
        }
    }
#else
    (void) context;
    (void) plan;
#endif
}

// 責務: sum_turn と query replay/top transfer の既存 validator を実行する。
// 必要な理由: AOrder 固定の専用 construct が query 列を壊していないことを確認するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::validate_fixed_construct_debug(
        const State &current_st,
        const YakinamashiState &yst
) {
#ifdef DEBUG
    QueriesStateValidator<MAX_N>::validate_sum_turn(yst.queries_state);
    QueriesStateValidator<MAX_N>::validate_query_order(current_st, yst.queries_state.queries);
    QueriesStateValidator<MAX_N>::validate_query_top_transfer(current_st, yst.queries_state.queries);
#else
    (void) current_st;
    (void) yst;
#endif
}

// 責務: planned_entries にある v の INIT/TARGET entry を、plan 上の直前 existing exact entry の直後へ挿入する。
// 必要な理由: 既存 restore_order_entry のランダム復元を使わず、planned AOrder の subset を復元するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::restore_entry_by_plan(
        PhysicalDestroyContext<MAX_N> &context,
        const AOrderRangePlan<MAX_N> &plan,
        int v,
        const my_bitset<MAX_N> &missing_after_restore
) {
    if (!plan.is_destroyed_v[v]) return;
    (void) missing_after_restore;
    const my_vector<OrderEntry> &current_entries = context.yst.query_order.aorder.get_order_entries();
    my_assert(find_optional_entry_pos_by_value_state(current_entries, v, ValState::INIT) == -1);
    my_assert(find_optional_entry_pos_by_value_state(current_entries, v, ValState::TARGET) == -1);
    int inserted_count = 0;
    for (int i = 0; i < static_cast<int>(plan.planned_entries.size()); i++) {
        const OrderEntry &entry = plan.planned_entries[i];
        if (entry.value != v) continue;
        const int restore_pos = find_plan_insert_pos(
                plan.planned_entries,
                context.yst.query_order.aorder.get_order_entries(),
                i);
        context.yst.query_order.aorder.insert_entry(restore_pos, entry);
        inserted_count++;
    }
    my_assert(inserted_count == 2);
    context.yst.query_order.aorder.normalize_min_front();
    context.yst.query_order.aorder.validate_target_entry_order();
}

// 責務: yst コピーへ AOrder/BOrder 更新と A2B/B2A 挿入を適用し、sum_turn 差分を返す。
// 必要な理由: target A all_ord 候補の score を、実際の query rebuild/insert 経路で評価するため。
template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::Helper::eval_pair_target_a(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        const ScoredPair &base_pair,
        AllOrd target_a_all_ord
) {
    const PairQidx pair_qidx = base_pair.pair_qidx;
    const int a2b_idx = pair_qidx.a2b_idx;
    const int b2a_idx = pair_qidx.b2a_idx;
    my_assert(0 <= a2b_idx && a2b_idx <= b2a_idx);
    YakinamashiState eval_yst = yst;
    eval_yst.query_order.aorder.insert_update_shift(v, target_a_all_ord);
    eval_yst.query_order.border.update_insert_ord_v(v, base_pair.all_ord);
    // 責務: A/B order を仮更新した eval_yst の既存 query 列を、更新後 order 前提で作り直す。
    // 必要な理由: 古い QueryCommonRI をそのまま replay すると OrdPos が更新後 order と矛盾するため。
    rebuild_queries_for_state(current_st, eval_yst);
    insert_eval_a2b(eval_yst, current_st, v, a2b_idx);
    int shifted_b2a_idx = b2a_idx;
    if (a2b_idx <= b2a_idx) shifted_b2a_idx++;
    insert_eval_b2a(eval_yst, current_st, v, shifted_b2a_idx);
    return eval_yst.queries_state.sum_turn - yst.queries_state.sum_turn;
}

// 責務: best_a_all_ords の中から original_a_all_ord との abs 距離が最小の AllOrd を返す。
// 必要な理由: 同点時に元の AOrder 構造に最も近い target A all_ord を採用し、後続 query への悪化を抑えるため。
template<size_t MAX_N>
AllOrd GreedyQueriesConstructor<MAX_N>::Helper::select_pair_target_a(
        const vector<AllOrd> &best_a_all_ords,
        AllOrd original_a_all_ord,
        int &selected_distance
) {
    my_assert(!best_a_all_ords.empty());
    AllOrd selected = best_a_all_ords[0];
    selected_distance = abs(selected.get() - original_a_all_ord.get());
    for (AllOrd candidate: best_a_all_ords) {
        const int distance = abs(candidate.get() - original_a_all_ord.get());
        if (distance < selected_distance) {
            selected = candidate;
            selected_distance = distance;
        }
    }
    return selected;
}

// 責務: AOrder の有効 target all_ord を全探索し、best diff と同点 all_ord 群から選択した結果を ScoredPair へ反映する。
// 必要な理由: A2B/B2A pair の score と適用 AOrder を target A all_ord 最適化後のものにするため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::ScoredPair
GreedyQueriesConstructor<MAX_N>::Helper::optimize_pair_target_a(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        const ScoredPair &base_pair
) {
    PairTargetAResult result;
    result.before_diff = base_pair.diff;
    // 責務: target A all_ord 全探索前の AOrder 上の TARGET(v) 位置を保持する。
    // 必要な理由: best diff 同点候補から、元 all_ord に最も近い候補を選ぶため。
    result.original_a_all_ord = yst.query_order.aorder.get_target_order(v);
    InsertRangeSet a_all_ord_ranges = build_aorder_v_ranges(yst, current_st, v);
    for_each_all_ord_in_ranges(a_all_ord_ranges, [&](AllOrd target_a_all_ord) {
        result.tried_a_all_ords.push_back(target_a_all_ord);
        const int diff = eval_pair_target_a(yst, current_st, v, base_pair, target_a_all_ord);
        if (!result.found || diff < result.best_diff) {
            result.found = true;
            result.best_diff = diff;
            result.best_a_all_ords.clear();
            result.best_a_all_ords.push_back(target_a_all_ord);
            return;
        }
        if (diff == result.best_diff) {
            result.best_a_all_ords.push_back(target_a_all_ord);
        }
    });
    my_assert(result.found);
    result.selected_a_all_ord = select_pair_target_a(
            result.best_a_all_ords, result.original_a_all_ord, result.selected_a_distance);
    ScoredPair optimized = base_pair;
    optimized.diff = result.best_diff;
    optimized.target_a_all_ord = result.selected_a_all_ord;
    optimized.has_target_a = true;
    if (is_collector_log_enabled()) {
        outaaa("pair_target_a_try",
               "v", v,
               "a2b_idx", base_pair.pair_qidx.a2b_idx,
               "b2a_idx", base_pair.pair_qidx.b2a_idx,
               "b_all_ord", base_pair.all_ord.get(),
               "tried_a_all_ords", all_ord_vector_to_debug_string(result.tried_a_all_ords),
               "best_a_all_ords", all_ord_vector_to_debug_string(result.best_a_all_ords),
               "original_a_all_ord", result.original_a_all_ord.get(),
               "selected_a_all_ord", result.selected_a_all_ord.get(),
               "selected_a_distance", result.selected_a_distance,
               "before_diff", result.before_diff,
               "after_diff", result.best_diff);
    }
    return optimized;
}

// 責務: AOrder を実更新せず、挿入 A2A 後の全 suffix query を逐次再計算して差分を返す。
// 必要な理由: 旧 naive の insert_update_shift は normalize_min_front を含み、fast の target AllOrd effect と比較座標がずれるため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::A2ACandidate
GreedyQueriesConstructor<MAX_N>::Helper::eval_a2a_candidate_naive_no_shift(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        int q_idx,
        AllOrd target_all_ord,
        const QueryScanState& scan_state
) {
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const int n = yst.queries_state.current_N;
    QueryOrdEffect effect = build_a2a_effect(yst, v, target_all_ord);
    QueryProvider provider(yst, current_st);
    QCRIFactory qfac;
    QueryCommon a2a_query = build_fast_a2a_query(yst, current_st, v, q_idx, target_all_ord, scan_state);
    OrdPos start_a = get_debug_start_ord_a_before_q_idx(yst, q_idx);
    OrdPos current_top_b = get_debug_start_ord_b_before_q_idx(yst, q_idx, n);
    QueryCommonRI inserted_a2a_qri = qfac.build_a2a_ri_from_start_top(start_a, a2a_query);
    int diff = inserted_a2a_qri.get_ins_turn();
    OrdPos current_top_a = inserted_a2a_qri.get_finished_top_ord_a();
    PrevA2ARotSum rot_sum = build_a2a_replay_initial_rot_sum(
            yst.queries_state.queries, q_idx, inserted_a2a_qri);
    for (int replay_idx = q_idx; replay_idx < qcnt; replay_idx++) {
        const QueryCommonRI &base_qri = yst.queries_state.queries[replay_idx];
        QueryCommon effected_q = provider.get_applied_common(
                current_top_b, base_qri.get_query(), effect);
        if (base_qri.is_a2a_type()) {
            QueryCommonRI effected_qri = qfac.build_a2a_ri_from_start_top(current_top_a, effected_q);
            auto [ra_sum, rra_sum] = effected_qri.get_can_sync_ra_rra_A2A();
            rot_sum.ra_sum += ra_sum;
            rot_sum.rra_sum += rra_sum;
            diff += effected_qri.get_ins_turn() - base_qri.get_ins_turn();
            current_top_a = effected_qri.get_finished_top_ord_a();
            continue;
        }
        const int stack_a_cnt = effected_q.get_stack_a_cnt();
        const int stack_b_cnt = effected_q.get_stack_b_cnt(n);
        OrdPos tar_a = base_qri.is_a2b_type()
                       ? effected_q.get_tar_ord_pos_a_A2B()
                       : to_ord_pos(effected_q.get_tar_rel_ord_a_B2A(), stack_a_cnt);
        OrdPos tar_b = base_qri.is_a2b_type()
                       ? to_ord_pos(effected_q.get_tar_rel_ord_b_A2B(), stack_b_cnt)
                       : effected_q.get_tar_ord_pos_b_B2A();
        BestRotInfo bri = build_brp_by_ord_with_prev_a2a(
                stack_a_cnt,
                stack_b_cnt,
                current_top_a,
                current_top_b,
                tar_a,
                tar_b,
                rot_sum);
        diff += bri.ins_turn - base_qri.get_ins_turn();
        current_top_a = effected_q.get_finished_top_ord_a(n);
        current_top_b = effected_q.get_finished_top_ord_b(n);
        rot_sum = PrevA2ARotSum(0, 0);
    }
    auto [cmd, cnt] = FinalRotCalculator::build_final_rot(n, current_top_a);
    (void) cmd;
    diff += cnt - yst.queries_state.final_rot_info.second;
    A2ACandidate result;
    result.found = true;
    result.q_idx = q_idx;
    result.a_all_ord = target_all_ord;
    result.diff = diff;
    return result;
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::log_a2a_order_snapshot(
        const char *tag,
        const YakinamashiState &yst,
        int v,
        AllOrd target_all_ord
) {
    const my_vector<OrderEntry> &entries = yst.query_order.aorder.get_order_entries();
    std::ostringstream entries_oss;
    int target_pos = -1;
    int init_pos = -1;
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        if (i > 0) entries_oss << " ";
        entries_oss << i << ":" << entries[i];
        if (entries[i].value == v && entries[i].val_state == ValState::TARGET) target_pos = i;
        if (entries[i].value == v && entries[i].val_state == ValState::INIT) init_pos = i;
    }
    AllOrd resolved_target_all_ord = resolve_moved_target_ord(yst, v, target_all_ord);
    out("a2a_order_snapshot",
        "tag", tag,
        "v", v,
        "target_all_ord_arg", target_all_ord.get(),
        "resolved_target_all_ord", resolved_target_all_ord.get(),
        "init_all_ord", yst.query_order.aorder.get_init_order(v).get(),
        "current_target_all_ord", yst.query_order.aorder.get_target_order(v).get(),
        "init_pos", init_pos,
        "target_pos", target_pos,
        "entry_count", static_cast<int>(entries.size()),
        "entries", entries_oss.str());
}
//
//template<size_t MAX_N>
//void GreedyQueriesConstructor<MAX_N>::Helper::log_a2a_eval_breakdown(
//        const YakinamashiState &yst,
//        const State &current_st,
//        int v,
//        const A2ACandidate &candidate
//) {
//    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
//    const int n = yst.queries_state.current_N;
//    A2AFastPrecalc precalc = build_a2a_fast_precalc(yst, current_st, v, candidate.a_all_ord);
//    QueryCommon a2a_query = build_fast_a2a_query(yst, current_st, v, candidate.q_idx, candidate.a_all_ord);
//    QCRIFactory qfac;
//    OrdPos start_a = get_debug_start_ord_a_before_q_idx(yst, candidate.q_idx);
//    QueryCommonRI inserted_a2a_qri = qfac.build_a2a_ri_from_start_top(start_a, a2a_query);
//    PrevA2ARotSum existing_rot_sum(0, 0);
//    if (candidate.q_idx > 0) {
//        existing_rot_sum = PrevA2ARotSumCalc::calc_prev_range_P(
//                yst.queries_state.queries, candidate.q_idx - 1);
//    }
//    auto [inserted_ra_sum, inserted_rra_sum] = inserted_a2a_qri.get_can_sync_ra_rra_A2A();
//    PrevA2ARotSum initial_rot_sum = build_a2a_replay_initial_rot_sum(
//            yst.queries_state.queries, candidate.q_idx, inserted_a2a_qri);
//    A2AReplayResult replay = calc_a2a_replay_diff(yst, current_st, candidate.q_idx, inserted_a2a_qri, precalc);
//    int prefix_suffix_diff = 0;
//    int selected_final_rot_diff = 0;
//    if (replay.has_first_ab) {
//        prefix_suffix_diff = calc_prefix_sum(precalc.fixed_diff_prefix_sum, replay.first_ab_idx + 1, qcnt);
//        selected_final_rot_diff = precalc.final_rot_diff;
//    } else {
//        auto [cmd, cnt] = FinalRotCalculator::build_final_rot(n, replay.finished_top_a);
//        (void) cmd;
//        selected_final_rot_diff = cnt - yst.queries_state.final_rot_info.second;
//    }
//    int total_fast_diff = inserted_a2a_qri.get_ins_turn()
//                          + replay.diff
//                          + prefix_suffix_diff
//                          + selected_final_rot_diff;
//    A2ACandidate naive = eval_a2a_candidate_naive_no_shift(
//            yst, current_st, v, candidate.q_idx, candidate.a_all_ord);
//    out("a2a_eval_breakdown",
//        "v", v,
//        "q_idx", candidate.q_idx,
//        "a_all_ord", candidate.a_all_ord.get(),
//        "resolved_target_all_ord", resolve_moved_target_ord(yst, v, candidate.a_all_ord).get(),
//        "candidate_diff", candidate.diff,
//        "inserted_turn", inserted_a2a_qri.get_ins_turn(),
//        "inserted_qri", indirect_debug_to_string(inserted_a2a_qri),
//        "replay_first_ab_idx", replay.first_ab_idx,
//        "replay_has_first_ab", replay.has_first_ab ? 1 : 0,
//        "replay_diff", replay.diff,
//        "replay_finished_top_a", replay.finished_top_a.get(),
//        "prefix_suffix_diff", prefix_suffix_diff,
//        "final_rot_diff", selected_final_rot_diff,
//        "precalc_final_top_a", precalc.final_top_ord_a.get(),
//        "before_final_rot", yst.queries_state.final_rot_info.second,
//        "prev_existing_rot_sum_ra", existing_rot_sum.ra_sum,
//        "prev_existing_rot_sum_rra", existing_rot_sum.rra_sum,
//        "inserted_rot_sum_ra", inserted_ra_sum,
//        "inserted_rot_sum_rra", inserted_rra_sum,
//        "initial_rot_sum_ra", initial_rot_sum.ra_sum,
//        "initial_rot_sum_rra", initial_rot_sum.rra_sum,
//        "total_fast_diff", total_fast_diff,
//        "naive_no_shift_diff", naive.diff);
//
//    out("a2a_fast_inserted_diff",
//        "v", v,
//        "q_idx", candidate.q_idx,
//        "turn_diff", inserted_a2a_qri.get_ins_turn(),
//        "qri", indirect_debug_to_string(inserted_a2a_qri));
//    QueryProvider provider(yst, current_st);
//    OrdPos replay_top_a = inserted_a2a_qri.get_finished_top_ord_a();
//    OrdPos replay_top_b = get_debug_start_ord_b_before_q_idx(yst, candidate.q_idx, n);
//    PrevA2ARotSum replay_rot_sum = initial_rot_sum;
//    for (int replay_idx = candidate.q_idx; replay_idx < replay.first_ab_idx; replay_idx++) {
//        const QueryCommonRI &base_qri = yst.queries_state.queries[replay_idx];
//        QueryCommon effected_q = provider.get_applied_common(
//                replay_top_b, base_qri.get_query(), precalc.effect);
//        QueryCommonRI effected_qri = qfac.build_a2a_ri_from_start_top(replay_top_a, effected_q);
//        auto [ra_sum, rra_sum] = effected_qri.get_can_sync_ra_rra_A2A();
//        replay_rot_sum.ra_sum += ra_sum;
//        replay_rot_sum.rra_sum += rra_sum;
//        out("a2a_fast_replay_query_diff",
//            "v", v,
//            "base_idx", replay_idx,
//            "base_turn", base_qri.get_ins_turn(),
//            "fast_turn", effected_qri.get_ins_turn(),
//            "turn_diff", effected_qri.get_ins_turn() - base_qri.get_ins_turn(),
//            "base_qri", indirect_debug_to_string(base_qri),
//            "fast_qri", indirect_debug_to_string(effected_qri),
//            "rot_sum_ra_after", replay_rot_sum.ra_sum,
//            "rot_sum_rra_after", replay_rot_sum.rra_sum);
//        replay_top_a = effected_qri.get_finished_top_ord_a();
//    }
//    if (replay.has_first_ab) {
//        const int first_ab_idx = replay.first_ab_idx;
//        const QueryCommonRI &base_qri = yst.queries_state.queries[first_ab_idx];
//        const QueryCommon &effected_q = precalc.effected_queries[first_ab_idx];
//        const int stack_a_cnt = effected_q.get_stack_a_cnt();
//        const int stack_b_cnt = effected_q.get_stack_b_cnt(n);
//        OrdPos tar_a = base_qri.is_a2b_type()
//                       ? effected_q.get_tar_ord_pos_a_A2B()
//                       : to_ord_pos(effected_q.get_tar_rel_ord_a_B2A(), stack_a_cnt);
//        OrdPos tar_b = base_qri.is_a2b_type()
//                       ? to_ord_pos(effected_q.get_tar_rel_ord_b_A2B(), stack_b_cnt)
//                       : effected_q.get_tar_ord_pos_b_B2A();
//        BestRotInfo bri = build_brp_by_ord_with_prev_a2a(
//                stack_a_cnt,
//                stack_b_cnt,
//                replay_top_a,
//                replay_top_b,
//                tar_a,
//                tar_b,
//                replay_rot_sum);
//        out("a2a_fast_first_ab_diff",
//            "v", v,
//            "base_idx", first_ab_idx,
//            "base_turn", base_qri.get_ins_turn(),
//            "fast_turn", bri.ins_turn,
//            "turn_diff", bri.ins_turn - base_qri.get_ins_turn(),
//            "base_qri", indirect_debug_to_string(base_qri),
//            "effected_q", effected_q,
//            "start_a", replay_top_a.get(),
//            "start_b", replay_top_b.get(),
//            "tar_a", tar_a.get(),
//            "tar_b", tar_b.get(),
//            "rot_sum_ra", replay_rot_sum.ra_sum,
//            "rot_sum_rra", replay_rot_sum.rra_sum);
//        for (int suffix_idx = first_ab_idx + 1; suffix_idx < qcnt; suffix_idx++) {
//            const int turn_diff = precalc.fixed_diff_prefix_sum[suffix_idx + 1]
//                                  - precalc.fixed_diff_prefix_sum[suffix_idx];
//            out("a2a_fast_suffix_query_diff",
//                "v", v,
//                "base_idx", suffix_idx,
//                "turn_diff", turn_diff,
//                "base_turn", yst.queries_state.queries[suffix_idx].get_ins_turn(),
//                "base_qri", indirect_debug_to_string(yst.queries_state.queries[suffix_idx]),
//                "effected_q", precalc.effected_queries[suffix_idx]);
//        }
//    }
//}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::log_a2a_actual_breakdown(
        const YakinamashiState &before_yst,
        const YakinamashiState &after_yst,
        const State &current_st,
        int v,
        const A2ACandidate &candidate,
        int before_sum_turn
) {
    (void) current_st;
    const int before_qcnt = static_cast<int>(before_yst.queries_state.queries.size());
    const int after_qcnt = static_cast<int>(after_yst.queries_state.queries.size());
    my_assert(after_qcnt == before_qcnt + 1);
    const QueryCommonRI &inserted_qri = after_yst.queries_state.queries[candidate.q_idx];
    int mapped_turn_diff_sum = inserted_qri.get_ins_turn();
    out("a2a_actual_breakdown_begin",
        "v", v,
        "q_idx", candidate.q_idx,
        "a_all_ord", candidate.a_all_ord.get(),
        "expected_diff", candidate.diff,
        "before_sum_turn", before_sum_turn,
        "after_sum_turn", after_yst.queries_state.sum_turn,
        "actual_total_diff", after_yst.queries_state.sum_turn - before_sum_turn,
        "before_final_rot", before_yst.queries_state.final_rot_info.second,
        "after_final_rot", after_yst.queries_state.final_rot_info.second,
        "final_rot_diff",
        after_yst.queries_state.final_rot_info.second - before_yst.queries_state.final_rot_info.second,
        "inserted_turn", inserted_qri.get_ins_turn(),
        "inserted_qri", indirect_debug_to_string(inserted_qri));
    for (int base_idx = 0; base_idx < before_qcnt; base_idx++) {
        int after_idx = base_idx;
        if (candidate.q_idx <= base_idx) after_idx++;
        const QueryCommonRI &base_qri = before_yst.queries_state.queries[base_idx];
        const QueryCommonRI &after_qri = after_yst.queries_state.queries[after_idx];
        const int turn_diff = after_qri.get_ins_turn() - base_qri.get_ins_turn();
        mapped_turn_diff_sum += turn_diff;
        out("a2a_actual_query_diff",
            "v", v,
            "base_idx", base_idx,
            "after_idx", after_idx,
            "base_turn", base_qri.get_ins_turn(),
            "after_turn", after_qri.get_ins_turn(),
            "turn_diff", turn_diff,
            "base_qri", indirect_debug_to_string(base_qri),
            "after_qri", indirect_debug_to_string(after_qri));
    }
    const int final_rot_diff =
            after_yst.queries_state.final_rot_info.second - before_yst.queries_state.final_rot_info.second;
    out("a2a_actual_breakdown_end",
        "v", v,
        "q_idx", candidate.q_idx,
        "mapped_query_diff_sum", mapped_turn_diff_sum,
        "final_rot_diff", final_rot_diff,
        "mapped_plus_final", mapped_turn_diff_sum + final_rot_diff,
        "actual_total_diff", after_yst.queries_state.sum_turn - before_sum_turn);
}

// 責務: DEBUG 時に同じ q_idx / target AllOrd effect 座標の naive diff と fast diff が一致することを検証する。
// 必要な理由: prefix sum 化した tail indirect と direct1 再評価の分解が既存挙動とずれていないか機械的に検出するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::assert_a2a_fast_matches_naive(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        const A2ACandidate &fast,
        const QueryScanState& scan_state
) {
#ifdef DEBUG
    A2ACandidate naive = eval_a2a_candidate_naive_no_shift(
            yst, current_st, v, fast.q_idx, fast.a_all_ord, scan_state);
    if (naive.diff == fast.diff) return;
    out("a2a_fast_naive_mismatch",
        "v", v,
        "q_idx", fast.q_idx,
        "a_all_ord", fast.a_all_ord.get(),
        "fast_diff", fast.diff,
        "naive_diff", naive.diff);
    my_assert(false);
#else
    (void) yst;
    (void) current_st;
    (void) v;
    (void) fast;
    (void) scan_state;
#endif
}

// 責務: 現在の AOrder から得た target AllOrd 候補範囲と全 q_idx を走査し、最良 A2A 候補を返す。
// 必要な理由: A2A を pair の比較対象に戻しつつ、候補ごとの AOrder 実更新を避けて評価するため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::A2ACandidate
GreedyQueriesConstructor<MAX_N>::Helper::collect_best_a2a_fast(
        const YakinamashiState &yst,
        const State &current_st,
        int v
) {
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    InsertRangeSet a_all_ord_ranges = build_aorder_v_ranges(yst, current_st, v);
    A2ACandidate best;
    for_each_all_ord_in_ranges(a_all_ord_ranges, [&](AllOrd target_all_ord) {
        const A2AFastPrecalc &precalc = build_a2a_fast_precalc(yst, current_st, v, target_all_ord);
        QueryScanState scan_state = init_scan_state(yst, current_st);
        QueriesCalculator qs_calc(current_st.current_N, yst.queries_state.queries);
        for (int q_idx = 0; q_idx <= qcnt; q_idx++) {
            A2ACandidate candidate = eval_a2a_candidate_fast(
                    yst, current_st, v, q_idx, target_all_ord, precalc, scan_state);
            assert_a2a_fast_matches_naive(yst, current_st, v, candidate, scan_state);
            if (!best.found || candidate.diff < best.diff) best = candidate;
            if (q_idx < qcnt) {
                auto& qri = yst.queries_state.queries[q_idx];
                advance_scan_state(yst, scan_state, qs_calc, q_idx, qri);
            }
        }
    });
    out_a2a_candidate_log(
            "a2a_fast_collect_best",
            "v", v,
            "found", best.found ? 1 : 0,
            "q_idx", best.q_idx,
            "a_all_ord", best.found ? best.a_all_ord.get() : -1,
            "diff", best.found ? best.diff : 0);
    return best;
}

// 責務: pair best と A2A best を比較し、同点なら A2A を採用する。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::ChosenInsertPlan
GreedyQueriesConstructor<MAX_N>::Helper::choose_a2a_or_pair_plan(
        const ScoredPair &best_pair,
        const A2ACandidate &best_a2a
) {
    ChosenInsertPlan chosen;
    if (best_a2a.found && best_a2a.diff <= best_pair.diff) {
        chosen.kind = BestInsertPlanKind::single_a2a;
        chosen.a2a = best_a2a;
        chosen.diff = best_a2a.diff;
        out_a2a_candidate_log(
                "a2a_pair_choose",
                "pair_diff", best_pair.diff,
                "a2a_found", best_a2a.found ? 1 : 0,
                "a2a_diff", best_a2a.diff,
                "chosen", "a2a");
        return chosen;
    }
    chosen.kind = BestInsertPlanKind::insert_pair;
    chosen.pair = best_pair;
    chosen.diff = best_pair.diff;
    out_a2a_candidate_log(
            "a2a_pair_choose",
            "pair_diff", best_pair.diff,
            "a2a_found", best_a2a.found ? 1 : 0,
            "a2a_diff", best_a2a.found ? best_a2a.diff : 0,
            "chosen", "pair");
    return chosen;
}

// 責務: 現在 AOrder の TARGET(v) だけを使い、全 q_idx から最良 A2A 候補を返す。
// 必要な理由: planned AOrder を固定したまま A2A query を選び、construct 中に A target を動かさないようにするため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::A2ACandidate
GreedyQueriesConstructor<MAX_N>::Helper::collect_best_a2a_fixed_a(
        const YakinamashiState &yst,
        const State &current_st,
        int v
) {
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const AllOrd fixed_target_all_ord = yst.query_order.aorder.get_target_order(v);
    const A2AFastPrecalc &precalc = build_a2a_fast_precalc(yst, current_st, v, fixed_target_all_ord);
    A2ACandidate best;
    QueryScanState scan_state = init_scan_state(yst, current_st);
    QueriesCalculator qs_calc(current_st.current_N, yst.queries_state.queries);
    for (int q_idx = 0; q_idx <= qcnt; q_idx++) {
        A2ACandidate candidate = eval_a2a_candidate_fast(
                yst, current_st, v, q_idx, fixed_target_all_ord, precalc, scan_state);
        assert_a2a_fast_matches_naive(yst, current_st, v, candidate, scan_state);
        if (!best.found || candidate.diff < best.diff) best = candidate;
        if (q_idx < qcnt) {
            auto& qri = yst.queries_state.queries[q_idx];
            advance_scan_state(yst, scan_state, qs_calc, q_idx, qri);
        }
    }
    return best;
}

// 責務: pair range task の best だけを使い、target A all_ord は現在 AOrder の TARGET(v) に固定する。
// 必要な理由: 既存 optimize_pair_target_a を呼ばず、planned AOrder を固定した pair 候補を得るため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::ScoredPair
GreedyQueriesConstructor<MAX_N>::Helper::collect_best_pair_fixed_a(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        const GreedyConstructorParams &params,
        const my_bitset<MAX_N> &is_never_construct_v
) {
    InsertPairPrecalc &prec = build_construct_v_precalc(current_st, yst, v);
    // この経路は現時点では新 owner range collector へ切り替えない。
    BasePairCandidates &base_candidates = base_pair_candidates_workspace();
    base_candidates.clear();
    collect_candidates(yst, current_st, v, prec, params, is_never_construct_v, base_candidates);
    BestPlanResult pair_result = solve_pair_range_tasks(yst, current_st, v, base_candidates, prec);
    my_assert(pair_result.found);
    ScoredPair fixed_pair = pair_result.best_pair;
    fixed_pair.target_a_all_ord = yst.query_order.aorder.get_target_order(v);
    fixed_pair.has_target_a = true;
    return fixed_pair;
}

// 責務: fixed-A pair と fixed-A A2A を比較し、同点なら A2A を選ぶ。
// 必要な理由: フラグなしの専用経路として採用 plan を作るため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::ChosenInsertPlan
GreedyQueriesConstructor<MAX_N>::Helper::choose_fixed_a_plan(
        const ScoredPair &best_pair,
        const A2ACandidate &best_a2a
) {
    return choose_a2a_or_pair_plan(best_pair, best_a2a);
}

// 責務: BOrder だけを更新し、fixed AOrder に対して query rebuild と A2B/B2A 挿入を行う。
// 必要な理由: 既存 apply_insert_pair_plan は AOrder::insert_update_shift を呼ぶため、このモードでは使えないため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::apply_pair_plan_fixed_a(
        YakinamashiState &yst,
        const State &current_st,
        int v,
        const ScoredPair &best_pair
) {
    const PairQidx best_pair_qidx = best_pair.pair_qidx;
    const int best_a2b_idx = best_pair_qidx.a2b_idx;
    const int best_b2a_idx = best_pair_qidx.b2a_idx;
    my_assert(0 <= best_a2b_idx && best_a2b_idx <= best_b2a_idx);
    yst.query_order.border.update_insert_ord_v(v, best_pair.all_ord);
    rebuild_queries_for_state(current_st, yst);
    insert_eval_a2b(yst, current_st, v, best_a2b_idx);
    int shifted_b2a_idx = best_b2a_idx;
    if (best_a2b_idx <= best_b2a_idx) shifted_b2a_idx++;
    insert_eval_b2a(yst, current_st, v, shifted_b2a_idx);
    validate_fixed_construct_debug(current_st, yst);
}

// 責務: 現在 AOrder の TARGET(v) に向かう A2A query だけを挿入する。
// 必要な理由: 既存 apply_a2a_plan は AOrder::insert_update_no_shift を呼ぶため、このモードでは使えないため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::apply_a2a_plan_fixed_a(
        YakinamashiState &yst,
        const State &current_st,
        int v,
        const A2ACandidate &best_a2a
) {
    my_assert(best_a2a.found);
    my_assert(best_a2a.a_all_ord == yst.query_order.aorder.get_target_order(v));
    insert_eval_a2a(yst, current_st, v, best_a2a.q_idx);
    validate_fixed_construct_debug(current_st, yst);
}

// 責務: AOrder と BOrder を更新し、既存と同じ手順で A2B と B2A query を挿入する。
// 必要な理由: target A all_ord 全探索で選んだ AOrder 位置を pair 採用時に反映するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::apply_insert_pair_plan(
        YakinamashiState &yst,
        const State &current_st,
        int v,
        const ScoredPair &best_pair
) {
    const PairQidx best_pair_qidx = best_pair.pair_qidx;
    const int best_a2b_idx = best_pair_qidx.a2b_idx;
    const int best_b2a_idx = best_pair_qidx.b2a_idx;
    my_assert(0 <= best_a2b_idx && best_a2b_idx <= best_b2a_idx);
    // 責務: pair score に保持された TARGET(v) の AOrder 挿入位置を insert_update_shift で実状態へ反映する。
    // 必要な理由: A2B/B2A 採用後の AOrder が、評価時に選ばれた target A all_ord と一致するようにするため。
    my_assert(best_pair.has_target_a);
    yst.query_order.aorder.insert_update_shift(v, best_pair.target_a_all_ord);
    yst.query_order.border.update_insert_ord_v(v, best_pair.all_ord);
    // 責務: 更新後 A/B order に対して、既存 query 列を current_st から再構築する。
    // 必要な理由: 古い QueryCommonRI のまま A2B/B2A を挿入すると OrdPos が矛盾するため。
    rebuild_queries_for_state(current_st, yst);
    insert_eval_a2b(yst, current_st, v, best_a2b_idx);
    // 責務: A2B だけを挿入した中間 query 列が current_st から順に実行可能か検証する。
    QueriesStateValidator<MAX_N>::validate_query_order(current_st, yst.queries_state.queries);
    QueriesStateValidator<MAX_N>::validate_query_top_transfer(current_st, yst.queries_state.queries);
    int shifted_b2a_idx = best_b2a_idx;
    if (best_a2b_idx <= best_b2a_idx) {
        shifted_b2a_idx++;
    }
    insert_eval_b2a(yst, current_st, v, shifted_b2a_idx);
    // 責務: A2B/B2A 挿入後の query 列が current_st から順に実行可能か検証する。
    QueriesStateValidator<MAX_N>::validate_query_order(current_st, yst.queries_state.queries);
    QueriesStateValidator<MAX_N>::validate_query_top_transfer(current_st, yst.queries_state.queries);
    out_construct_v_insert_window(
            yst.queries_state, "a2b", best_a2b_idx, best_a2b_idx, shifted_b2a_idx);
    out_construct_v_insert_window(
            yst.queries_state, "b2a", shifted_b2a_idx, best_a2b_idx, shifted_b2a_idx);
}

// 責務: AOrder を normalize なしで更新し、更新後の実状態から A2A query を作って挿入する。
// 必要な理由: fast 評価用 QueryCommon を使い回さず、採用した q_idx / all_ord の組だけを実適用へ渡すため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::apply_a2a_plan(
        YakinamashiState &yst,
        const State &current_st,
        int v,
        const A2ACandidate &best_a2a
) {
    my_assert(best_a2a.found);
    YakinamashiState before_yst = yst;
    const int before_sum_turn = yst.queries_state.sum_turn;
    out_a2a_candidate_log(
            "a2a_apply_begin",
            "v", v,
            "q_idx", best_a2a.q_idx,
            "a_all_ord", best_a2a.a_all_ord.get(),
            "before_sum", yst.queries_state.sum_turn);
    log_a2a_order_snapshot("before_order_update", yst, v, best_a2a.a_all_ord);
//    log_a2a_eval_breakdown(yst, current_st, v, best_a2a);
    yst.query_order.aorder.insert_update_no_shift(v, best_a2a.a_all_ord);
    log_a2a_order_snapshot("after_order_update", yst, v, best_a2a.a_all_ord);
    // 責務: AOrder 更新後、A2A 挿入前の query 列が current_st から順に実行可能か検証する。
    QueriesStateValidator<MAX_N>::validate_query_order(current_st, yst.queries_state.queries);
    QueriesStateValidator<MAX_N>::validate_query_top_transfer(current_st, yst.queries_state.queries);
    out_a2a_candidate_log(
            "a2a_apply_after_order_update",
            "query_count", static_cast<int>(yst.queries_state.queries.size()),
            "sum", yst.queries_state.sum_turn);
    insert_eval_a2a(yst, current_st, v, best_a2a.q_idx);
    log_a2a_actual_breakdown(before_yst, yst, current_st, v, best_a2a, before_sum_turn);
    // 責務: A2A 挿入後の query 列が current_st から順に実行可能か検証する。
    QueriesStateValidator<MAX_N>::validate_query_order(current_st, yst.queries_state.queries);
    QueriesStateValidator<MAX_N>::validate_query_top_transfer(current_st, yst.queries_state.queries);
    out_a2a_candidate_log(
            "a2a_apply_end",
            "after_sum", yst.queries_state.sum_turn);
}

// 責務: ChosenInsertPlan の kind に従い pair または A2A の適用関数を呼ぶ。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::apply_chosen_insert_plan(
        YakinamashiState &yst,
        const State &current_st,
        int v,
        const ChosenInsertPlan &chosen
) {
    if (chosen.kind == BestInsertPlanKind::single_a2a) {
        apply_a2a_plan(yst, current_st, v, chosen.a2a);
        return;
    }
    my_assert(chosen.kind == BestInsertPlanKind::insert_pair);
    apply_insert_pair_plan(yst, current_st, v, chosen.pair);
}

// 責務: 各候補を評価し、pair ごとの best score を更新する。
template<size_t MAX_N>
// 責務: 各候補を評価し、pair ごとの best score を更新し、評価回数を返す。
// 必要な理由: 1周目の実評価候補数を cand_cnt1 として cout へ出すため。
int GreedyQueriesConstructor<MAX_N>::Helper::scan_candidates_for_scores(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        InsertPairCandidatesByAllOrd &candidates,
        InsertPairPrecalc &prec,
        PairBestScoreTable &pair_scores
) {
    int evaluated_count = 0;
    my_bitset<MAX_QUERY_CNT> is_band;
    for (int all_ord_b = 0; all_ord_b < MAX_INSERT_SLOT_B; all_ord_b++) {
        for (auto in_band_q_idx: prec.b_band_start_qidxs[all_ord_b]) {
            is_band[in_band_q_idx] = true;
        }
        for (auto out_band_q_idx: prec.b_band_end_qidxs[all_ord_b]) {
            is_band[out_band_q_idx] = false;
        }
        for (PairQidx &pair_qidx: candidates[all_ord_b]) {
            ScoredPair scored_pair =
                    eval_insert_pair_qidx(yst, init_st, v, pair_qidx, all_ord_b, is_band, prec);
            evaluated_count++;
            update_pair_best_score(pair_scores, scored_pair);
        }
    }
    return evaluated_count;
}

//was_ai_rev calc_base_topk_planにかえる、ほかのbestってつかってるがtopkにするのが自然な物も名前を変える
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::TopKPlanResult
GreedyQueriesConstructor<MAX_N>::Helper::calc_base_topk_plan(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        BasePairCandidates &base_candidates,
        InsertPairPrecalc &prec,
        int top_k,
        int &cand_cnt1
) {
    static PairBestScoreTable pair_scores;
    start_pair_best_scores(pair_scores);
    cand_cnt1 = scan_candidates_for_scores(yst, init_st, v, base_candidates.base_cands_by_allord, prec, pair_scores);
    return build_topk_pairs(base_candidates, pair_scores, top_k);
}

// 責務: base 評価済み all_ord 除外後の候補を all_ord bucket に追加する。
// 必要な理由: topk expanded base_cands_by_allord の bucket 要素が q_idx pair だけであることを明確にするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::append_expanded_pair_qidx(
        InsertPairCandidatesByAllOrd &topk_expanded_candidates,
        const PairQidx &pair_qidx,
        int all_ord
) {
    my_assert(0 <= all_ord && all_ord < MAX_INSERT_SLOT_B);
    topk_expanded_candidates[all_ord].push_back(pair_qidx);
}

// 責務: pair_info の common_ranges を展開し、base_all_ords に含まれない候補だけを追加する。
template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::Helper::append_common_range_except_base_ords(
        InsertPairCandidatesByAllOrd &topk_expanded_candidates,
        const BasePairInfo &pair_info
) {
    static std::array<int, MAX_INSERT_SLOT_B> base_ord_stamps{};
    static int call_id = 0;
    call_id++;
    if (call_id == INT_MAX) {
        base_ord_stamps.fill(0);
        call_id = 1;
    }
    for (int all_ord: pair_info.base_all_ords) {
        my_assert(0 <= all_ord && all_ord < MAX_INSERT_SLOT_B);
        base_ord_stamps[all_ord] = call_id;
    }
    int added_count = 0;
    for (int range_i = 0; range_i < pair_info.common_ranges.count; range_i++) {
        for_each_all_ord_in_circular_range(pair_info.common_ranges.ranges[range_i], [&](AllOrd all_ord) {
            const int all_ord_idx = all_ord.get();
            if (base_ord_stamps[all_ord_idx] == call_id) return;
            append_expanded_pair_qidx(topk_expanded_candidates, pair_info.pair_qidx, all_ord_idx);
            added_count++;
        });
    }
    return added_count;
}

// 責務: base_result の top pair を common range 全 all_ord に展開し、base にない候補だけを集める。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::InsertPairCandidatesByAllOrd
GreedyQueriesConstructor<MAX_N>::Helper::build_topk_expanded_candidates(
        const BasePairCandidates &base_candidates,
        const TopKPlanResult &base_result
) {
    InsertPairCandidatesByAllOrd topk_expanded_candidates;
    for (int top_idx = 0; top_idx < static_cast<int>(base_result.topk_pairs.size()); top_idx++) {
        const ScoredPair &top_pair = base_result.topk_pairs[top_idx];
        const PairQidx &pair_qidx = top_pair.pair_qidx;
        const BasePairInfo &pair_info = find_base_pair_info(base_candidates, pair_qidx);
        const int added_count = append_common_range_except_base_ords(topk_expanded_candidates, pair_info);
        (void) added_count;
    }
    return topk_expanded_candidates;
}

// 責務: base_candidates 内の BasePairInfo から pair_qidx と同じ pair を返す。
//ネックにならなさそうなのでpairの数だけループする方式で
//重そうだったら、pairを添え字にアクセスできるようにする
template<size_t MAX_N>
const typename GreedyQueriesConstructor<MAX_N>::Helper::BasePairInfo &
GreedyQueriesConstructor<MAX_N>::Helper::find_base_pair_info(
        const BasePairCandidates &base_candidates,
        const PairQidx &pair_qidx
) {
    for (const BasePairInfo &info: base_candidates.base_pair_infos) {
        if (info.pair_qidx.a2b_idx == pair_qidx.a2b_idx && info.pair_qidx.b2a_idx == pair_qidx.b2a_idx) {
            return info;
        }
    }
    my_assert(false);
    static BasePairInfo dummy;
    return dummy;
}

// 責務: ScoredPair が既存 best より良ければ BestPlanResult を更新する。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::update_best_result(
        BestPlanResult &result,
        const ScoredPair &scored_pair
) {
    if (result.found && result.best_pair.diff <= scored_pair.diff) return;
    result.found = true;
    result.best_pair = scored_pair;
}

// 責務: top pair refine に渡す上位 scored pair を top_k 件まで保持する。
// 必要な理由: even unit sampling の取り逃しを、良い pair の周辺点評価で補うため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::insert_top_pair_result(
        BestPlanResult &result,
        const ScoredPair &scored_pair,
        int top_k
) {
    if (top_k <= 0) return;
    int insert_idx = 0;
    while (insert_idx < static_cast<int>(result.top_pairs.size()) &&
           result.top_pairs[insert_idx].diff <= scored_pair.diff) {
        insert_idx++;
    }
    if (insert_idx >= top_k) return;
    result.top_pairs.insert(result.top_pairs.begin() + insert_idx, scored_pair);
    if (static_cast<int>(result.top_pairs.size()) > top_k) {
        result.top_pairs.pop_back();
    }
}

// 責務: DEBUG 時に top pair 件数と diff 昇順を検査する。
// 必要な理由: refine の入力が壊れた状態で周辺点を作らないよう早期に停止するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::validate_top_pairs_debug(
        const BestPlanResult &result,
        int top_k
) {
#ifdef DEBUG
    my_assert(static_cast<int>(result.top_pairs.size()) <= top_k);
    for (int i = 1; i < static_cast<int>(result.top_pairs.size()); i++) {
        my_assert(result.top_pairs[i - 1].diff <= result.top_pairs[i].diff);
    }
#else
    (void) result;
    (void) top_k;
#endif
}

// 責務: DEBUG 時に a2b_idx/b2a_idx/all_ord_b が直接評価できる範囲か確認する。
// 必要な理由: collector 外 q_idx を試す実験で、不正な点を評価関数へ渡さないため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::validate_refine_point_debug(
        const RefinePoint &point,
        int qcnt,
        int b_insert_slot_count
) {
#ifdef DEBUG
    my_assert(0 <= point.a2b_idx);
    my_assert(point.a2b_idx < point.b2a_idx);
    my_assert(point.b2a_idx <= qcnt);
    my_assert(0 <= point.all_ord_b);
    my_assert(point.all_ord_b < b_insert_slot_count);
#else
    (void) point;
    (void) qcnt;
    (void) b_insert_slot_count;
#endif
}

// 責務: DEBUG 時に各点の範囲と同一点の重複が残っていないことを検査する。
// 必要な理由: refine 評価回数と採用候補の意味を単純に保つため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::validate_refine_points_debug(
        const my_vector<RefinePoint> &points,
        int qcnt,
        int b_insert_slot_count
) {
#ifdef DEBUG
    for (int i = 0; i < static_cast<int>(points.size()); i++) {
        validate_refine_point_debug(points[i], qcnt, b_insert_slot_count);
        for (int j = i + 1; j < static_cast<int>(points.size()); j++) {
            my_assert(points[i].a2b_idx != points[j].a2b_idx ||
                      points[i].b2a_idx != points[j].b2a_idx ||
                      points[i].all_ord_b != points[j].all_ord_b);
        }
    }
#else
    (void) points;
    (void) qcnt;
    (void) b_insert_slot_count;
#endif
}

// 責務: DEBUG 時に refine 点の diff が既存 naive insert plan の diff と一致するか検査する。
// 必要な理由: collector 外 q_idx を直接評価しても既存評価意味からずれていないことを確認するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::validate_refine_score_debug(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const RefinePoint &point,
        const ScoredPair &scored_pair
) {
#ifdef DEBUG
    auto naive = evaluate_naive_insert_plan(
            yst, init_st, v, point.a2b_idx, point.b2a_idx, AllOrd(point.all_ord_b));
    const int naive_diff = naive.score - yst.queries_state.sum_turn;
    if (scored_pair.diff != naive_diff) {
        out("refine_score_mismatch",
            "v", v,
            "a2b_idx", point.a2b_idx,
            "b2a_idx", point.b2a_idx,
            "all_ord_b", point.all_ord_b,
            "refine_diff", scored_pair.diff,
            "naive_diff", naive_diff,
            "naive_score", naive.score,
            "base_score", yst.queries_state.sum_turn);
        my_assert(false);
    }
#else
    (void) yst;
    (void) init_st;
    (void) v;
    (void) point;
    (void) scored_pair;
#endif
}

// 責務: top pair の a2b_idx/b2a_idx/all_ord_b 近傍を作り、範囲外と重複を除く。
// 必要な理由: even unit sampling で見なかった近傍点を collector 外でも追加評価するため。
template<size_t MAX_N>
my_vector<typename GreedyQueriesConstructor<MAX_N>::Helper::RefinePoint>
GreedyQueriesConstructor<MAX_N>::Helper::build_refine_points(
        const BestPlanResult &base_result,
        const PairRefineParams &refine_params,
        int qcnt,
        int b_insert_slot_count
) {
    my_vector<RefinePoint> points;
    if (b_insert_slot_count <= 0) return points;
    validate_top_pairs_debug(base_result, refine_params.top_k);
    for (const ScoredPair &top_pair: base_result.top_pairs) {
        const int base_a2b_idx = top_pair.pair_qidx.a2b_idx;
        const int base_b2a_idx = top_pair.pair_qidx.b2a_idx;
        const int base_all_ord = top_pair.all_ord.get();
        for (int da = -refine_params.a2b_idx_range; da <= refine_params.a2b_idx_range; da++) {
            const int a2b_idx = base_a2b_idx + da;
            for (int db = -refine_params.b2a_idx_range; db <= refine_params.b2a_idx_range; db++) {
                const int b2a_idx = base_b2a_idx + db;
                if (!(0 <= a2b_idx && a2b_idx < b2a_idx && b2a_idx <= qcnt)) continue;
                for (int dord = -refine_params.all_ord_range; dord <= refine_params.all_ord_range; dord++) {
                    if (da == 0 && db == 0 && dord == 0) continue;
                    int all_ord = (base_all_ord + dord) % b_insert_slot_count;
                    if (all_ord < 0) all_ord += b_insert_slot_count;
                    points.push_back(RefinePoint{a2b_idx, b2a_idx, all_ord});
                }
            }
        }
    }
    std::sort(points.begin(), points.end(), [](const RefinePoint &lhs, const RefinePoint &rhs) {
        if (lhs.a2b_idx != rhs.a2b_idx) return lhs.a2b_idx < rhs.a2b_idx;
        if (lhs.b2a_idx != rhs.b2a_idx) return lhs.b2a_idx < rhs.b2a_idx;
        return lhs.all_ord_b < rhs.all_ord_b;
    });
    points.erase(std::unique(points.begin(), points.end(), [](const RefinePoint &lhs, const RefinePoint &rhs) {
        return lhs.a2b_idx == rhs.a2b_idx &&
               lhs.b2a_idx == rhs.b2a_idx &&
               lhs.all_ord_b == rhs.all_ord_b;
    }), points.end());
    validate_refine_points_debug(points, qcnt, b_insert_slot_count);
    return points;
}

// 責務: direct band を一時除外し、指定 all_ord_b 1 点の band diff と no-band diff を合算する。
// 必要な理由: collector 外の周辺点も既存 pair range 評価と同じ band 意味で高速評価するため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::ScoredPair
GreedyQueriesConstructor<MAX_N>::Helper::eval_refine_point(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const RefinePoint &point,
        BandMinSeg &seg,
        InsertPairPrecalc &prec
) {
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const int b_insert_slot_count =
            calc_insert_slot_count(yst.query_order.border.get_all_order_entries_size());
    validate_refine_point_debug(point, qcnt, b_insert_slot_count);
    vec_array<int, 2> direct_band_q_idxs;
    int direct1_idx = normalize_a2b_direct1_idx(point.a2b_idx, prec, qcnt);
    int direct2_idx = normalize_a2b_direct2_idx(direct1_idx, point.a2b_idx, prec, qcnt);
    if (point.a2b_idx <= direct1_idx && direct1_idx < point.b2a_idx) direct_band_q_idxs.push_back(direct1_idx);
    if (point.a2b_idx <= direct2_idx && direct2_idx < point.b2a_idx && direct2_idx != direct1_idx) {
        direct_band_q_idxs.push_back(direct2_idx);
    }
    for (int q_idx: direct_band_q_idxs) add_band_range_to_min_seg(seg, prec.a2b_band_ranges[q_idx], -1);
    BandMinNode node = seg.query_range(point.all_ord_b, point.all_ord_b + 1);
    my_assert(node.min_all_ord == point.all_ord_b);
    const int without_band_diff = calc_diff_without_band(
            yst, init_st, v, point.a2b_idx, point.b2a_idx, AllOrd(point.all_ord_b), prec);
    ScoredPair scored_pair{
            PairQidx{point.a2b_idx, point.b2a_idx},
            AllOrd(point.all_ord_b),
            without_band_diff + node.min_band_diff
    };
    for (int q_idx: direct_band_q_idxs) add_band_range_to_min_seg(seg, prec.a2b_band_ranges[q_idx], +1);
    validate_refine_score_debug(yst, init_st, v, point, scored_pair);
    return scored_pair;
}

// 責務: refine 点を a2b_idx/b2a_idx の Mo 順に処理し、最良点を返す。
// 必要な理由: 周辺点ごとに band を全再構築せず、既存 pair range 評価と同じ高速化方針を使うため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::BestPlanResult
GreedyQueriesConstructor<MAX_N>::Helper::solve_refine_points_mo(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const my_vector<RefinePoint> &points,
        InsertPairPrecalc &prec
) {
    BestPlanResult result;
    if (points.empty()) return result;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    int block_size = 1;
    while (block_size * block_size < qcnt + 1) block_size++;
    my_vector<int> point_order;
    point_order.reserve(points.size());
    for (int idx = 0; idx < static_cast<int>(points.size()); idx++) point_order.push_back(idx);
    std::sort(point_order.begin(), point_order.end(), [&](int lhs_idx, int rhs_idx) {
        const RefinePoint &lhs = points[lhs_idx];
        const RefinePoint &rhs = points[rhs_idx];
        int lhs_block = lhs.a2b_idx / block_size;
        int rhs_block = rhs.a2b_idx / block_size;
        if (lhs_block != rhs_block) return lhs_block < rhs_block;
        if (lhs_block & 1) return lhs.b2a_idx > rhs.b2a_idx;
        return lhs.b2a_idx < rhs.b2a_idx;
    });
    const int b_insert_slot_count =
            calc_insert_slot_count(yst.query_order.border.get_all_order_entries_size());
    validate_refine_points_debug(points, qcnt, b_insert_slot_count);
    BandMinSeg seg;
    seg.init(b_insert_slot_count);
    int cur_l = 0;
    int cur_r = 0;
    auto add_q_idx = [&](int q_idx, int delta) {
        my_assert(0 <= q_idx && q_idx < qcnt);
        add_band_range_to_min_seg(seg, prec.a2b_band_ranges[q_idx], delta);
    };
    for (int point_idx: point_order) {
        const RefinePoint &point = points[point_idx];
        while (cur_l > point.a2b_idx) add_q_idx(--cur_l, +1);
        while (cur_r < point.b2a_idx) add_q_idx(cur_r++, +1);
        while (cur_l < point.a2b_idx) add_q_idx(cur_l++, -1);
        while (cur_r > point.b2a_idx) add_q_idx(--cur_r, -1);
        ScoredPair scored_pair = eval_refine_point(yst, init_st, v, point, seg, prec);
        update_best_result(result, scored_pair);
    }
    return result;
}

// 責務: top pair 近傍点を作って Mo 評価し、改善した場合だけ best pair を置き換える。
// 必要な理由: even unit sampling で落とした all_ord/q_idx 近傍を A2A 比較前に補うため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::BestPlanResult
GreedyQueriesConstructor<MAX_N>::Helper::refine_top_pairs(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const BestPlanResult &base_result,
        InsertPairPrecalc &prec
) {
    BestPlanResult result = base_result;
    if (!base_result.found) return result;
    PairRefineParams refine_params{
            G::pair_refine_top_k,
            G::pair_refine_a2b_idx_range,
            G::pair_refine_b2a_idx_range,
            G::pair_refine_all_ord_range
    };
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const int b_insert_slot_count =
            calc_insert_slot_count(yst.query_order.border.get_all_order_entries_size());
    my_vector<RefinePoint> points =
            build_refine_points(base_result, refine_params, qcnt, b_insert_slot_count);
    BestPlanResult refine_result = solve_refine_points_mo(yst, init_st, v, points, prec);
    if (refine_result.found && refine_result.best_pair.diff < result.best_pair.diff) {
        result.best_pair = refine_result.best_pair;
    }
    return result;
}

// 責務: base_cands_by_allord を採点して BestPlanResult の best 1 件を返す。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::BestPlanResult
GreedyQueriesConstructor<MAX_N>::Helper::scan_candidates_for_best(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        InsertPairCandidatesByAllOrd &candidates,
        InsertPairPrecalc &prec,
        int &evaluated_count
) {
    evaluated_count = 0;
    BestPlanResult result;
    my_bitset<MAX_QUERY_CNT> is_band;
    for (int all_ord_b = 0; all_ord_b < MAX_INSERT_SLOT_B; all_ord_b++) {
        for (auto in_band_q_idx: prec.b_band_start_qidxs[all_ord_b]) {
            is_band[in_band_q_idx] = true;
        }
        for (auto out_band_q_idx: prec.b_band_end_qidxs[all_ord_b]) {
            is_band[out_band_q_idx] = false;
        }
        for (PairQidx &pair_qidx: candidates[all_ord_b]) {
            ScoredPair scored_pair =
                    eval_insert_pair_qidx(yst, init_st, v, pair_qidx, all_ord_b, is_band, prec);
            evaluated_count++;
            update_best_result(result, scored_pair);
        }
    }
    return result;
}

// 責務: top_k pair から展開された未評価候補を採点し、best 1 件を返す。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::BestPlanResult
GreedyQueriesConstructor<MAX_N>::Helper::calc_topk_expanded_best(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        InsertPairCandidatesByAllOrd &topk_expanded_candidates,
        InsertPairPrecalc &prec,
        int &cand_cnt2
) {
    return scan_candidates_for_best(yst, init_st, v, topk_expanded_candidates, prec, cand_cnt2);
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_v_by_nieve(YakinamashiState<MAX_N> &yst, int v) {
    Helper &helper = Helper::instance();
    helper.apply_collector_log_mode(collector_log_mode);
    if (Helper::is_collector_log_enabled()) {
        clear_file();
        clear_output2_file();
    }
    typename Helper::InsertPairPrecalc &prec = helper.build_construct_v_precalc(init_st, yst, v);
    // この経路は現時点では新 owner range collector へ切り替えない。
    typename Helper::BasePairCandidates &candidates = helper.base_pair_candidates_workspace();
    candidates.clear();
    helper.collect_candidates(yst, init_st, v, prec, params, is_never_construct_v, candidates);
    typename Helper::BestPlanResult best_result =
            helper.solve_pair_range_tasks(yst, init_st, v, candidates, prec);
    my_assert(best_result.found);
    typename Helper::PairQidx best_pair_qidx = best_result.best_pair.pair_qidx;
    AllOrd best_all_ord_b = best_result.best_pair.all_ord;
    yst.query_order.border.update_insert_ord_v(v, best_all_ord_b);
    helper.insert_eval_a2b(yst, init_st, v, best_pair_qidx.a2b_idx);
    int shifted_b2a_idx = best_pair_qidx.b2a_idx;
    if (best_pair_qidx.a2b_idx <= best_pair_qidx.b2a_idx) shifted_b2a_idx++;
    helper.insert_eval_b2a(yst, init_st, v, shifted_b2a_idx);
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_v_only_query_destroyed(
        const State &current_st,
        YakinamashiState<MAX_N> &yst,
        int v
) {
    const auto timing_v_start = std::chrono::steady_clock::now();
    auto timing_stage_start = timing_v_start;
    auto write_construct_timing = [&](const std::string &stage) {
        if (!is_timing_log_enabled()) return;
        const auto timing_now = std::chrono::steady_clock::now();
        const auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(timing_now - timing_stage_start).count();
        std::ostringstream ss;
        ss << "stage=" << stage
           << " v=" << v
           << " qcnt=" << yst.queries_state.queries.size()
           << " sum_turn=" << yst.queries_state.sum_turn
           << " owner=" << use_owner_collector
           << " array=" << use_owner_array_range
           << " unit=" << use_owner_unit_range
           << " even=" << use_owner_even_unit
           << " refine=" << use_pair_refine
           << " elapsed_ms=" << elapsed_ms;
        write_timing_log(ss.str());
        timing_stage_start = timing_now;
    };
    write_construct_timing("construct_v_begin");
    Helper &helper = Helper::instance();
    using InsertPairPrecalc = typename Helper::InsertPairPrecalc;
    using BasePairCandidates = typename Helper::BasePairCandidates;
    using BestPlanResult = typename Helper::BestPlanResult;
    using ScoredPair = typename Helper::ScoredPair;
    using PairQidx = typename Helper::PairQidx;
    using BestInsertPlanKind = typename Helper::BestInsertPlanKind;
    helper.apply_collector_log_mode(collector_log_mode);
    if (Helper::is_collector_log_enabled()) {
        clear_file();
        clear_output2_file();
    }
    // 責務: construct 前の query 列が current_st から順に実行可能か検証する。
    QueriesStateValidator<MAX_N>::validate_query_order(current_st, yst.queries_state.queries);
    QueriesStateValidator<MAX_N>::validate_query_top_transfer(current_st, yst.queries_state.queries);
    write_construct_timing("validate");
    InsertPairPrecalc &prec = helper.build_construct_v_precalc(current_st, yst, v);
    write_construct_timing("precalc");
#ifdef DEBUG
    Helper::clear_legacy_check_trace();
#endif
    BasePairCandidates &base_candidates = helper.base_pair_candidates_workspace();
    base_candidates.clear();
    if (use_owner_collector) {
        const typename Helper::OwnerOverlapSource overlap_source =
                use_owner_array_range ? Helper::OwnerOverlapSource::array : Helper::OwnerOverlapSource::map;
        helper.collect_candidates_owner_range(
                yst, current_st, v, prec, params, is_never_construct_v,
                base_candidates, overlap_source);
    } else {
        helper.collect_candidates(
                yst, current_st, v, prec, params, is_never_construct_v, base_candidates);
    }
    if (is_timing_log_enabled()) {
        const auto timing_now = std::chrono::steady_clock::now();
        const auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(timing_now - timing_stage_start).count();
        std::ostringstream ss;
        ss << "stage=collect_candidates"
           << " v=" << v
           << " qcnt=" << yst.queries_state.queries.size()
           << " sum_turn=" << yst.queries_state.sum_turn
           << " pair_task_count=" << base_candidates.pair_tasks.size()
           << " pair_range_count=" << base_candidates.pair_ranges.size()
           << " ord_range_total=" << base_candidates.ord_range_total_count
           << " elapsed_ms=" << elapsed_ms;
        write_timing_log(ss.str());
        timing_stage_start = timing_now;
    }
//    std::cout << "construct_ord_range_total v=" << v
//              << " pair_task_count=" << base_candidates.pair_tasks.size()
//              << " ord_range_total=" << base_candidates.ord_range_total_count
//              << std::endl;
    // 責務: Collector が作った PairRangeTask 群を Mo で評価し、pair best を 1 件選ぶ。
    // 必要な理由: top-k pair の all_ord 全展開を使わず、pair ごとの range 列から最小 band all_ord を直接選ぶため。
    BestPlanResult pair_result =
            helper.solve_pair_range_tasks(yst, current_st, v, base_candidates, prec);
    write_construct_timing("solve_pair_range_tasks");
    const bool force_a2a = !pair_result.found;
    if (force_a2a && is_timing_log_enabled()) {
        std::ostringstream ss;
        ss << "stage=pair_missing_force_a2a"
           << " v=" << v
           << " qcnt=" << yst.queries_state.queries.size()
           << " sum_turn=" << yst.queries_state.sum_turn;
        write_timing_log(ss.str());
    }
    if (!force_a2a && use_pair_refine) {
        pair_result = helper.refine_top_pairs(yst, current_st, v, pair_result, prec);
        my_assert(pair_result.found);
    }
    write_construct_timing("refine_top_pairs");
    if (!force_a2a && is_timing_log_enabled()) {
        typename Helper::BestPairNeed best_need =
                helper.calc_best_pair_need(yst, current_st, v, pair_result.best_pair, prec, is_never_construct_v);
        helper.write_best_pair_need(v, pair_result.best_pair, best_need);
    }
    if (!force_a2a && use_owner_collector && use_owner_check) {
        helper.abort_if_legacy_best_pair_differs(
                yst, current_st, v, pair_result, prec, params, is_never_construct_v);
    }
    write_construct_timing("owner_check");
    typename Helper::ChosenInsertPlan chosen;
    typename Helper::A2ACandidate best_a2a = helper.collect_best_a2a_fast(yst, current_st, v);
    write_construct_timing("collect_best_a2a");
    if (force_a2a) {
        my_assert(best_a2a.found);
        chosen.kind = BestInsertPlanKind::single_a2a;
        chosen.a2a = best_a2a;
        chosen.diff = best_a2a.diff;
    } else {
        // 責務: Mo 評価で選ばれた top pair 1 件だけを、AOrder target 候補の実挿入評価で最適化する。
        // 必要な理由: A2A との比較に使う pair score と、採用時に適用する target A all_ord を確定するため。
        ScoredPair best_scored_pair = helper.optimize_pair_target_a(yst, current_st, v, pair_result.best_pair);
        write_construct_timing("optimize_pair_target_a");
        chosen = helper.choose_a2a_or_pair_plan(best_scored_pair, best_a2a);
    }
#ifdef DEBUG
//    helper.run_legacy_check(yst, current_st, v, base_candidates, pair_result, prec, params, is_never_construct_v);
#endif
    if (is_timing_log_enabled()) {
        const auto timing_now = std::chrono::steady_clock::now();
        const auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(timing_now - timing_stage_start).count();
        std::ostringstream ss;
        ss << "stage=choose"
           << " v=" << v
           << " qcnt=" << yst.queries_state.queries.size()
           << " sum_turn=" << yst.queries_state.sum_turn
           << " chosen=" << (chosen.kind == BestInsertPlanKind::single_a2a ? "a2a" : "pair")
           << " diff=" << chosen.diff
           << " elapsed_ms=" << elapsed_ms;
        write_timing_log(ss.str());
        timing_stage_start = timing_now;
    }
    int turn_diff = chosen.diff;
    int prev_all_turn = yst.queries_state.sum_turn;
    if (Helper::is_collector_log_enabled()) {
        if (chosen.kind == BestInsertPlanKind::insert_pair) {
            PairQidx best_pair_qidx = chosen.pair.pair_qidx;
            AllOrd all_ord_b = chosen.pair.all_ord;
            auto[best_a2b_idx, best_b2a_idx] = best_pair_qidx;
            my_assert(0 <= best_a2b_idx && best_a2b_idx <= best_b2a_idx);
            typename Helper::PairCostLog best_cost_log = helper.build_pair_cost_log(
                    yst, current_st, v, best_a2b_idx, best_b2a_idx, all_ord_b, prec);
            const int b_insert_slot_count =
                    helper.calc_insert_slot_count(yst.query_order.border.get_all_order_entries_size());
            typename Helper::BandRangeFenwick best_active_bands = helper.build_active_bands(
                    best_b2a_idx, b_insert_slot_count, prec);
            typename Helper::B2AAfterCost best_b2a_after_cost = helper.calc_b2a_after_cost(
                    yst, current_st, v, best_b2a_idx, all_ord_b, best_active_bands, prec);
            helper.apply_b2a_after_log(best_cost_log, best_b2a_after_cost);
            ::out2("best_candidate",
                   "kind =", "pair",
                   "v =", v,
                   ", a2b_idx =", best_a2b_idx,
                   ", b2a_idx =", best_b2a_idx,
                   ", all_ord =", all_ord_b.get(),
                   ", target_a_all_ord =", chosen.pair.target_a_all_ord.get(),
                   ", optimized_pair_diff =", chosen.pair.diff,
                   ", collector_turn_diff =", turn_diff);
            helper.out_pair_cost_log(best_b2a_idx, best_a2b_idx, all_ord_b, best_cost_log);
            helper.outaaa("construct_v_best_insert_pair",
                          "v", v,
                          "best_a2b_idx", best_a2b_idx,
                          "best_b2a_idx", best_b2a_idx,
                          "best_target_a_all_ord", chosen.pair.target_a_all_ord.get());
        } else {
            my_assert(chosen.kind == BestInsertPlanKind::single_a2a);
            ::out2("best_candidate",
                   "kind =", "a2a",
                   "v =", v,
                   ", q_idx =", chosen.a2a.q_idx,
                   ", a_all_ord =", chosen.a2a.a_all_ord.get(),
                   ", collector_turn_diff =", turn_diff);
            helper.outaaa("construct_v_best_insert_a2a",
                          "v", v,
                          "best_q_idx", chosen.a2a.q_idx,
                          "best_a_all_ord", chosen.a2a.a_all_ord.get());
        }
    }
    helper.apply_chosen_insert_plan(yst, current_st, v, chosen);
    if (is_timing_log_enabled()) {
        const auto timing_now = std::chrono::steady_clock::now();
        const auto apply_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(timing_now - timing_stage_start).count();
        const auto total_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(timing_now - timing_v_start).count();
        std::ostringstream ss;
        ss << "stage=apply"
           << " v=" << v
           << " qcnt=" << yst.queries_state.queries.size()
           << " before_sum_turn=" << prev_all_turn
           << " after_sum_turn=" << yst.queries_state.sum_turn
           << " expected_diff=" << turn_diff
           << " actual_diff=" << yst.queries_state.sum_turn - prev_all_turn
           << " elapsed_ms=" << apply_ms
           << " total_ms=" << total_ms;
        write_timing_log(ss.str());
        timing_stage_start = timing_now;
    }
    if (Helper::is_collector_log_enabled()) {
        ::out2("best_apply_result",
               "v =", v,
               ", kind =", chosen.kind == BestInsertPlanKind::single_a2a ? "a2a" : "pair",
               ", before_sum_turn =", prev_all_turn,
               ", after_sum_turn =", yst.queries_state.sum_turn,
               ", actual_diff =", yst.queries_state.sum_turn - prev_all_turn,
               ", collector_turn_diff =", turn_diff);
    }
    Helper::out_a2a_candidate_log(
            "a2a_chosen_apply_result",
            "v", v,
            "chosen", chosen.kind == BestInsertPlanKind::single_a2a ? "a2a" : "pair",
            "expected_diff", turn_diff,
            "actual_diff", yst.queries_state.sum_turn - prev_all_turn);
#ifdef DEBUG
    my_assert(prev_all_turn + turn_diff == yst.queries_state.sum_turn);
#endif
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_v_physical_destroyed(
        PhysicalDestroyContext<MAX_N> &context,
        int v
) {
    Helper &helper = Helper::instance();
    helper.log_construct_v_begin(context, v);
    helper.restore_order_entry(init_st, context, v);
    helper.restore_state_value(init_st, context.current_st, v);
    helper.rebuild_queries_for_state(context.current_st, context.yst);
    // 責務: rebuild 後の query 列が current_st から順に実行可能か検証する。
    QueriesStateValidator<MAX_N>::validate_query_order(context.current_st, context.yst.queries_state.queries);
    QueriesStateValidator<MAX_N>::validate_query_top_transfer(context.current_st, context.yst.queries_state.queries);
    const int before_sum_turn = context.yst.queries_state.sum_turn;
    const int before_query_count = static_cast<int>(context.yst.queries_state.queries.size());
    construct_v_only_query_destroyed(context.current_st, context.yst, v);
    helper.log_construct_v_end(context, v, before_sum_turn, before_query_count);
}

// 責務: erased v を state/order/query に戻し、既存の only-query 1-v construct で query を挿入する。
// 必要な理由: 段階復元時に INIT/TARGET の置ける範囲中央を使いながら、候補列挙や score 計算は既存処理を保つため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_v_physical_middle(
        PhysicalDestroyContext<MAX_N> &context,
        int v
) {
    Helper &helper = Helper::instance();
    helper.log_construct_v_begin(context, v);
    helper.restore_order_entry_middle(init_st, context, v);
    helper.restore_state_value(init_st, context.current_st, v);
    helper.rebuild_queries_for_state(context.current_st, context.yst);
    QueriesStateValidator<MAX_N>::validate_query_order(context.current_st, context.yst.queries_state.queries);
    QueriesStateValidator<MAX_N>::validate_query_top_transfer(context.current_st, context.yst.queries_state.queries);
    const int before_sum_turn = context.yst.queries_state.sum_turn;
    const int before_query_count = static_cast<int>(context.yst.queries_state.queries.size());
    construct_v_only_query_destroyed(context.current_st, context.yst, v);
    helper.log_construct_v_end(context, v, before_sum_turn, before_query_count);
}

// 責務: fixed B pair と A2A を比較し、BOrder に TARGET(v) がない場合は A2A だけを適用する。
// 必要な理由: fixed B construct でも通常 construct と同じく pair/A2A の良い方を採用しつつ、A2A-only v を扱うため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_v_fixed_b(
        YakinamashiState<MAX_N> &yst,
        const State &current_st,
        int v
) {
    Helper &helper = Helper::instance();
    typename Helper::A2ACandidate best_a2a = helper.collect_best_a2a_fast(yst, current_st, v);
    if (!helper.has_b_target_order(yst, v)) {
        my_assert(best_a2a.found);
        helper.apply_a2a_plan(yst, current_st, v, best_a2a);
        return;
    }
    const AllOrd fixed_b_all_ord = yst.query_order.border.get_target_order(v);
    typename Helper::FixedBPairResult result =
            helper.collect_fixed_b_pair(yst, current_st, v, fixed_b_all_ord);
    my_assert(result.found);
    typename Helper::ChosenInsertPlan chosen = helper.choose_a2a_or_pair_plan(result.pair, best_a2a);
    helper.apply_chosen_insert_plan(yst, current_st, v, chosen);
}

// 責務: destroy 前 BOrder に v がある場合は fixed B pair と A2A を比較し、ない場合は A2A だけを適用する。
// 必要な理由: physical fixed B construct でも A2A-only v を扱い、BOrder 復元 assert を避けるため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_v_physical_fixed_b(
        PhysicalDestroyContext<MAX_N> &context,
        int v
) {
    Helper &helper = Helper::instance();
    helper.log_construct_v_begin(context, v);
    helper.restore_order_entry(init_st, context, v);
    const bool has_b_target = helper.snapshot_has_b_target_order(context, v);
    AllOrd fixed_b_all_ord = AllOrd(0);
    if (has_b_target) {
        fixed_b_all_ord = helper.restore_fixed_b_order(context, v);
    }
    helper.restore_state_value(init_st, context.current_st, v);
    helper.rebuild_queries_for_state(context.current_st, context.yst);
    const int before_sum_turn = context.yst.queries_state.sum_turn;
    const int before_query_count = static_cast<int>(context.yst.queries_state.queries.size());
    typename Helper::A2ACandidate best_a2a =
            helper.collect_best_a2a_fast(context.yst, context.current_st, v);
    if (!has_b_target) {
        my_assert(best_a2a.found);
        helper.apply_a2a_plan(context.yst, context.current_st, v, best_a2a);
        helper.log_construct_v_end(context, v, before_sum_turn, before_query_count);
        return;
    }
    typename Helper::FixedBPairResult result =
            helper.collect_fixed_b_pair(context.yst, context.current_st, v, fixed_b_all_ord);
    my_assert(result.found);
    typename Helper::ChosenInsertPlan chosen = helper.choose_a2a_or_pair_plan(result.pair, best_a2a);
    helper.apply_chosen_insert_plan(context.yst, context.current_st, v, chosen);
    helper.log_construct_v_end(context, v, before_sum_turn, before_query_count);
}

// 責務: fixed-A の pair/A2A best を比較し、AOrder を動かさず query を挿入する。
// 必要な理由: AOrder range destroy で最初に作った planned AOrder を construct 中も固定するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_v_fixed_a(
        YakinamashiState<MAX_N> &yst,
        const State &current_st,
        int v
) {
    Helper &helper = Helper::instance();
    using BestInsertPlanKind = typename Helper::BestInsertPlanKind;
    helper.validate_fixed_construct_debug(current_st, yst);
    typename Helper::ScoredPair best_pair =
            helper.collect_best_pair_fixed_a(yst, current_st, v, params, is_never_construct_v);
    typename Helper::A2ACandidate best_a2a = helper.collect_best_a2a_fixed_a(yst, current_st, v);
    typename Helper::ChosenInsertPlan chosen = helper.choose_fixed_a_plan(best_pair, best_a2a);
    if (chosen.kind == BestInsertPlanKind::single_a2a) {
        helper.apply_a2a_plan_fixed_a(yst, current_st, v, chosen.a2a);
        return;
    }
    my_assert(chosen.kind == BestInsertPlanKind::insert_pair);
    helper.apply_pair_plan_fixed_a(yst, current_st, v, chosen.pair);
}

// 責務: planned AOrder 完全一致を保ちながら destroyed_vs を cost 順に construct する。
// 必要な理由: query destroy 側で A target を固定したまま通常と同じ cost 順再構築を行うため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_planned_query(
        YakinamashiState<MAX_N> &yst,
        const AOrderRangePlan<MAX_N> &plan
) {
    Helper &helper = Helper::instance();
    helper.validate_plan_entries_debug(yst, plan);
    my_vector<std::pair<int, int>> ordered_vs =
            helper.sort_destroyed_vs_by_cost_desc(plan.destroyed_vs);
    reset_never_construct_vs(ordered_vs);
    for (const auto &[v, destroyed_cost]: ordered_vs) {
        (void) destroyed_cost;
        construct_v_fixed_a(yst, init_st, v);
        mark_constructed_v(v);
        helper.validate_plan_entries_debug(yst, plan);
    }
    helper.validate_fixed_construct_debug(init_st, yst);
}

// 責務: planned AOrder の missing subset へ v を戻し、state/query を復元して fixed-A construct を行う。
// 必要な理由: physical destroy で消えた v をランダムではなく planned AOrder に沿って戻すため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_v_physical_fixed_a(
        PhysicalDestroyContext<MAX_N> &context,
        const AOrderRangePlan<MAX_N> &plan,
        int v,
        my_bitset<MAX_N> &missing_v
) {
    Helper &helper = Helper::instance();
    helper.log_construct_v_begin(context, v);
    my_bitset<MAX_N> missing_after_restore = missing_v;
    missing_after_restore.reset(v);
    helper.restore_entry_by_plan(context, plan, v, missing_after_restore);
    missing_v = missing_after_restore;
    helper.validate_plan_subset_debug(context.yst, plan, missing_v);
    helper.restore_state_value(init_st, context.current_st, v);
    helper.rebuild_queries_for_state(context.current_st, context.yst);
    const int before_sum_turn = context.yst.queries_state.sum_turn;
    const int before_query_count = static_cast<int>(context.yst.queries_state.queries.size());
    construct_v_fixed_a(context.yst, context.current_st, v);
    helper.validate_plan_subset_debug(context.yst, plan, missing_v);
    helper.log_construct_v_end(context, v, before_sum_turn, before_query_count);
}

// 責務: physical destroy 後の planned subset から、destroyed_vs cost 順に v を復元・construct する。
// 必要な理由: physical destroy 側でも A target を最初に作った planned AOrder に固定するため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::construct_planned_physical(
        PhysicalDestroyContext<MAX_N> &context,
        const AOrderRangePlan<MAX_N> &plan
) {
    Helper &helper = Helper::instance();
    my_bitset<MAX_N> missing_v = plan.is_destroyed_v;
    helper.validate_destroyed_absent_debug(context, plan);
    helper.validate_plan_subset_debug(context.yst, plan, missing_v);
    my_vector<std::pair<int, int>> ordered_vs =
            helper.sort_destroyed_vs_by_cost_desc(plan.destroyed_vs);
    reset_never_construct_vs(ordered_vs);
    for (const auto &[v, destroyed_cost]: ordered_vs) {
        (void) destroyed_cost;
        construct_v_physical_fixed_a(context, plan, v, missing_v);
        mark_constructed_v(v);
    }
    helper.validate_plan_entries_debug(context.yst, plan);
    helper.validate_fixed_construct_debug(context.current_st, context.yst);
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::debug_ins2(
        int case_n,
        const YakinamashiState<MAX_N> &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    Helper::instance().debug_ins2_impl(init_st, case_n, yst, destroyed_vs);
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::debug_run_a2b_indirect_q_idx_checks(
        const YakinamashiState<MAX_N> &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    Helper::instance().debug_run_a2b_indirect_q_idx_checks_impl(init_st, yst, destroyed_vs);
}

template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::debug_dump_a2b_b2a_indirect_diffs(
        const YakinamashiState<MAX_N> &yst,
        const my_vector<std::pair<int, int>> &destroyed_vs
) {
    Helper::instance().debug_dump_a2b_b2a_indirect_diffs_impl(init_st, yst, destroyed_vs);
}

//template<size_t MAX_N>
//void GreedyQueriesConstructor<MAX_N>::construct(
//        YakinamashiState &yst, const my_vector<std::pair<int, int>> &destroyed_vs
//) {
//    Helper::instance().construct_impl(init_st, yst, destroyed_vs);
//}

// _REVIEW: main.cpp の明示的なテスト入口で使う MAX_N 版を cpp 側で生成するため。
template class GreedyQueriesConstructor<MAX_BUFF>;
template class GreedyQueriesConstructor<100>;
template class GreedyQueriesConstructor<101>;
