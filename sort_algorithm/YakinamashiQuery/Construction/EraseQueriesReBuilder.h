#pragma once

#include "State/YakinamashiState.h"
#include "State/QueriesState.h"
#include "Query/A2AQueryUtil.h"
#include "Query/QueryCommonRIFactory.h"
#include "Query/QueriesCalculator.h"
#include "Query/QueryStateApplyUtil.h"
#include "Order/ExistAllOrdManager.h"
#include "Order/OrdPosCalculator.h"
#include "Util/FinalRotCalculator.h"
#include "Validation/YakinamashiStateValidator.h"
#include "YakinamashiQueryConstants.h"
#include <cstdio>
#include <fstream>
#include <sstream>

template<size_t MAX_N>
struct RebuildReplayState {
    State st;
    my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()> exist_a_ords;
    my_bitset<YQConstants::GET_MAX_ALL_ORD_B<MAX_N>()> exist_b_ords;
    my_vector<ValState> now_a_val_state;
    my_vector<ValState> now_b_val_state;
};

template<size_t MAX_N>
class EraseQueriesReBuilder {
public:
    static void rebuild_by_order(
            PhysicalDestroyContext<MAX_N>& context,
            const my_vector<std::pair<int, int>>& erased_vs
    );

    static void rebuild_by_state_query(
            PhysicalDestroyContext<MAX_N>& context,
            const my_vector<std::pair<int, int>>& state_erased_vs,
            const my_vector<std::pair<int, int>>& query_skipped_vs
    );

    static void rebuild_by_current_order(
            State& current_st,
            YakinamashiState<MAX_N>& yst
    );

private:
    // #define LOG_PHYSICAL
    static constexpr const char* PHYSICAL_TRACE_PATH = "physical_destroy_trace.txt";

    static void clear_physical_trace();

    static void append_physical_trace_line(
            const std::string& line
    );

    static std::string stack_to_string(
            const RingBuffer500& stack
    );

    static std::string state_to_string(
            const State& st
    );

    static std::string destroyed_vs_to_string(
            const my_vector<std::pair<int, int>>& destroyed_vs
    );

    static std::string erased_values_to_string(
            const my_vector<int>& erased_values
    );

    static std::string query_common_to_string(
            const QueryCommon& q
    );

    static std::string query_ri_to_string(
            const QueryCommonRI& qri
    );

    static std::string order_summary_to_string(
            const QueryOrder& order
    );

    static void log_rebuild_begin(
            const State& current_st,
            const YakinamashiState<MAX_N>& yst,
            const my_vector<std::pair<int, int>>& erased_vs
    );

    static void log_rebuild_snapshot(
            const QueryOrder& before_erased_order,
            const QueryOrder& after_erased_order,
            const my_vector<int>& erased_values
    );

    static void log_rebuild_state_after_erase(
            const State& current_st
    );

    static void log_rebuild_query_step(
            int read_idx,
            const QueryCommonRI& old_qri,
            bool skipped,
            const QueryCommon* new_q
    );

    static void log_rebuild_end(
            const State& current_st,
            const YakinamashiState<MAX_N>& yst
    );

    static my_bitset<MAX_N> build_is_erased_v(
            const my_vector<std::pair<int, int>>& erased_vs
    );

    static my_vector<int> build_erased_values(
            const my_vector<std::pair<int, int>>& erased_vs
    );

    static State build_erased_state(
            const State& current_st,
            const my_bitset<MAX_N>& is_erased_v
    );

    static RebuildReplayState<MAX_N> init_replay_state(
            const State& current_st,
            const YakinamashiState<MAX_N>& yst,
            const my_bitset<MAX_N>& is_erased_v
    );

    static void save_erase_order_snapshot(
            PhysicalDestroyContext<MAX_N>& context,
            const QueryOrder& before_erased_order,
            const QueryOrder& after_erased_order,
            const my_bitset<MAX_N>& is_erased_v
    );

    static void init_exist_b_ords(
            RebuildReplayState<MAX_N>& replay,
            const YakinamashiState<MAX_N>& yst
    );

    static void init_first_top_ords(
            YakinamashiState<MAX_N>& yst,
            const RebuildReplayState<MAX_N>& replay
    );

    static OrdPos calc_first_top_ord_a(
            const RebuildReplayState<MAX_N>& replay,
            const YakinamashiState<MAX_N>& yst
    );

    static OrdPos calc_first_top_ord_b(
            const RebuildReplayState<MAX_N>& replay,
            const YakinamashiState<MAX_N>& yst
    );

    static QueryCommon rebuild_query(
            const QueryCommon& old_q,
            YakinamashiState<MAX_N>& yst,
            RebuildReplayState<MAX_N>& replay
    );

    static QueryCommon rebuild_a2b_query(
            int v,
            YakinamashiState<MAX_N>& yst,
            RebuildReplayState<MAX_N>& replay
    );

    static QueryCommon rebuild_b2a_query(
            int v,
            YakinamashiState<MAX_N>& yst,
            RebuildReplayState<MAX_N>& replay
    );

    static QueryCommon rebuild_a2a_query(
            int v,
            YakinamashiState<MAX_N>& yst,
            RebuildReplayState<MAX_N>& replay
    );

    static void append_rebuilt_query(
            YakinamashiState<MAX_N>& yst,
            RebuildReplayState<MAX_N>& replay,
            QCRIFactory& cri_factory,
            const QueryCommon& new_q
    );

    static void advance_replay(
            YakinamashiState<MAX_N>& yst,
            RebuildReplayState<MAX_N>& replay,
            int q_idx,
            const QueryCommonRI& qri
    );

    static void update_replay_order(
            YakinamashiState<MAX_N>& yst,
            RebuildReplayState<MAX_N>& replay,
            const QueryCommon& q
    );

    static void recalc_final(
            YakinamashiState<MAX_N>& yst
    );
};

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::clear_physical_trace() {
#ifdef LOG_PHYSICAL
    std::remove(PHYSICAL_TRACE_PATH);
#endif
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::append_physical_trace_line(
        const std::string& line
) {
#ifdef LOG_PHYSICAL
    std::ofstream log_file(PHYSICAL_TRACE_PATH, std::ios::app);
    if (!log_file.is_open()) return;
    log_file << line << '\n';
#else
    (void) line;
#endif
}

template<size_t MAX_N>
std::string EraseQueriesReBuilder<MAX_N>::stack_to_string(
        const RingBuffer500& stack
) {
    std::ostringstream oss;
    oss << "[";
    for (int i = 0; i < stack.size(); i++) {
        if (i != 0) oss << ",";
        oss << stack[i];
    }
    oss << "]";
    return oss.str();
}

template<size_t MAX_N>
std::string EraseQueriesReBuilder<MAX_N>::state_to_string(
        const State& st
) {
    std::ostringstream oss;
    oss << "current_N=" << st.current_N
        << " initial_N=" << st.initial_N
        << " A=" << stack_to_string(st.A)
        << " B=" << stack_to_string(st.B);
    return oss.str();
}

template<size_t MAX_N>
std::string EraseQueriesReBuilder<MAX_N>::destroyed_vs_to_string(
        const my_vector<std::pair<int, int>>& destroyed_vs
) {
    std::ostringstream oss;
    oss << "[";
    for (int i = 0; i < static_cast<int>(destroyed_vs.size()); i++) {
        if (i != 0) oss << ",";
        oss << "(v=" << destroyed_vs[i].first << ",cost=" << destroyed_vs[i].second << ")";
    }
    oss << "]";
    return oss.str();
}

template<size_t MAX_N>
std::string EraseQueriesReBuilder<MAX_N>::erased_values_to_string(
        const my_vector<int>& erased_values
) {
    std::ostringstream oss;
    oss << "[";
    for (int i = 0; i < static_cast<int>(erased_values.size()); i++) {
        if (i != 0) oss << ",";
        oss << erased_values[i];
    }
    oss << "]";
    return oss.str();
}

template<size_t MAX_N>
std::string EraseQueriesReBuilder<MAX_N>::query_common_to_string(
        const QueryCommon& q
) {
    std::ostringstream oss;
    oss << q;
    return oss.str();
}

template<size_t MAX_N>
std::string EraseQueriesReBuilder<MAX_N>::query_ri_to_string(
        const QueryCommonRI& qri
) {
    std::ostringstream oss;
    oss << qri;
    return oss.str();
}

template<size_t MAX_N>
std::string EraseQueriesReBuilder<MAX_N>::order_summary_to_string(
        const QueryOrder& order
) {
    std::ostringstream oss;
    oss << "a_entries=" << order.aorder.get_all_order_entries_size()
        << " b_entries=" << order.border.get_all_order_entries_size();
    return oss.str();
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::log_rebuild_begin(
        const State& current_st,
        const YakinamashiState<MAX_N>& yst,
        const my_vector<std::pair<int, int>>& erased_vs
) {
    append_physical_trace_line("=== physical_rebuild_begin ===");
    append_physical_trace_line("rebuild_input_state " + state_to_string(current_st));
    append_physical_trace_line("rebuild_input_query current_N=" + std::to_string(yst.queries_state.current_N)
                               + " query_count=" + std::to_string(yst.queries_state.queries.size())
                               + " sum_turn=" + std::to_string(yst.queries_state.sum_turn)
                               + " final_rot_cnt=" + std::to_string(yst.queries_state.final_rot_info.second));
    append_physical_trace_line("rebuild_input_order " + order_summary_to_string(yst.query_order));
    append_physical_trace_line("rebuild_destroyed_vs " + destroyed_vs_to_string(erased_vs));
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::log_rebuild_snapshot(
        const QueryOrder& before_erased_order,
        const QueryOrder& after_erased_order,
        const my_vector<int>& erased_values
) {
    append_physical_trace_line("rebuild_erased_values " + erased_values_to_string(erased_values));
    append_physical_trace_line("rebuild_order_before " + order_summary_to_string(before_erased_order));
    append_physical_trace_line("rebuild_order_after " + order_summary_to_string(after_erased_order));
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::log_rebuild_state_after_erase(
        const State& current_st
) {
    append_physical_trace_line("rebuild_state_after_erase " + state_to_string(current_st));
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::log_rebuild_query_step(
        int read_idx,
        const QueryCommonRI& old_qri,
        bool skipped,
        const QueryCommon* new_q
) {
    std::string line = "rebuild_query_step read_idx=" + std::to_string(read_idx)
                       + " skipped=" + std::to_string(skipped)
                       + " old_qri=" + query_ri_to_string(old_qri);
    if (new_q != nullptr) {
        line += " new_q=" + query_common_to_string(*new_q);
    }
    append_physical_trace_line(line);
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::log_rebuild_end(
        const State& current_st,
        const YakinamashiState<MAX_N>& yst
) {
    append_physical_trace_line("rebuild_end_state " + state_to_string(current_st));
    append_physical_trace_line("rebuild_end_query current_N=" + std::to_string(yst.queries_state.current_N)
                               + " query_count=" + std::to_string(yst.queries_state.queries.size())
                               + " sum_turn=" + std::to_string(yst.queries_state.sum_turn)
                               + " final_rot_cnt=" + std::to_string(yst.queries_state.final_rot_info.second));
    append_physical_trace_line("rebuild_end_order " + order_summary_to_string(yst.query_order));
    append_physical_trace_line("=== physical_rebuild_end ===");
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::rebuild_by_order(
        PhysicalDestroyContext<MAX_N>& context,
        const my_vector<std::pair<int, int>>& erased_vs
) {
    clear_physical_trace();
    State& current_st = context.current_st;
    YakinamashiState<MAX_N>& yst = context.yst;
    log_rebuild_begin(current_st, yst, erased_vs);
    QRIList old_queries = yst.queries_state.queries;
    QueryOrder before_erased_order = yst.query_order;
    my_bitset<MAX_N> is_erased_v = build_is_erased_v(erased_vs);
    my_vector<int> erased_values = build_erased_values(erased_vs);
    yst.query_order.aorder.erase_values(erased_values);
    yst.query_order.border.erase_values(erased_values);
    QueryOrder after_erased_order = yst.query_order;
    log_rebuild_snapshot(before_erased_order, after_erased_order, erased_values);
    save_erase_order_snapshot(context, before_erased_order, after_erased_order, is_erased_v);
    current_st = build_erased_state(current_st, is_erased_v);
    log_rebuild_state_after_erase(current_st);
    RebuildReplayState<MAX_N> replay = init_replay_state(current_st, yst, is_erased_v);
    yst.queries_state.current_N = replay.st.current_N;
    init_first_top_ords(yst, replay);
    yst.queries_state.queries.clear();
    yst.queries_state.sum_turn = 0;
    QCRIFactory cri_factory;
    for (int read_idx = 0; read_idx < static_cast<int>(old_queries.size()); read_idx++) {
        const QueryCommon& old_q = old_queries[read_idx].get_query();
        if (is_erased_v[old_q.get_tar_v()]) {
            log_rebuild_query_step(read_idx, old_queries[read_idx], true, nullptr);
            continue;
        }
        QueryCommon new_q = rebuild_query(old_q, yst, replay);
        log_rebuild_query_step(read_idx, old_queries[read_idx], false, &new_q);
        append_rebuilt_query(yst, replay, cri_factory, new_q);
    }
    recalc_final(yst);
    QueriesStateValidator<MAX_N>::validate_sum_turn(yst.queries_state);
    QueriesStateValidator<MAX_N>::validate_query_order(current_st, yst.queries_state.queries);
    QueriesStateValidator<MAX_N>::validate_query_top_transfer(current_st, yst.queries_state.queries);
    log_rebuild_end(current_st, yst);
}

// 責務: State/order から消す集合と query skip 集合を分離して、残る query を physical 削除後の State に合わせて rebuild する。
// 必要な理由: query 側 v は State に残したまま query だけ削除し、physical 側 v は State/query の両方から削除するため。
template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::rebuild_by_state_query(
        PhysicalDestroyContext<MAX_N>& context,
        const my_vector<std::pair<int, int>>& state_erased_vs,
        const my_vector<std::pair<int, int>>& query_skipped_vs
) {
    clear_physical_trace();
    State& current_st = context.current_st;
    YakinamashiState<MAX_N>& yst = context.yst;
    log_rebuild_begin(current_st, yst, state_erased_vs);
    QRIList old_queries = yst.queries_state.queries;
    QueryOrder before_erased_order = yst.query_order;
    my_bitset<MAX_N> is_state_erased_v = build_is_erased_v(state_erased_vs);
    my_bitset<MAX_N> is_query_skipped_v = build_is_erased_v(query_skipped_vs);
    my_vector<int> erased_values = build_erased_values(state_erased_vs);
    yst.query_order.aorder.erase_values(erased_values);
    yst.query_order.border.erase_values(erased_values);
    QueryOrder after_erased_order = yst.query_order;
    log_rebuild_snapshot(before_erased_order, after_erased_order, erased_values);
    save_erase_order_snapshot(context, before_erased_order, after_erased_order, is_state_erased_v);
    current_st = build_erased_state(current_st, is_state_erased_v);
    log_rebuild_state_after_erase(current_st);
    RebuildReplayState<MAX_N> replay = init_replay_state(current_st, yst, is_state_erased_v);
    yst.queries_state.current_N = replay.st.current_N;
    init_first_top_ords(yst, replay);
    yst.queries_state.queries.clear();
    yst.queries_state.sum_turn = 0;
    QCRIFactory cri_factory;
    for (int read_idx = 0; read_idx < static_cast<int>(old_queries.size()); read_idx++) {
        const QueryCommon& old_q = old_queries[read_idx].get_query();
        if (is_query_skipped_v[old_q.get_tar_v()]) {
            log_rebuild_query_step(read_idx, old_queries[read_idx], true, nullptr);
            continue;
        }
        QueryCommon new_q = rebuild_query(old_q, yst, replay);
        log_rebuild_query_step(read_idx, old_queries[read_idx], false, &new_q);
        append_rebuilt_query(yst, replay, cri_factory, new_q);
    }
    recalc_final(yst);
    QueriesStateValidator<MAX_N>::validate_sum_turn(yst.queries_state);
    QueriesStateValidator<MAX_N>::validate_query_order(current_st, yst.queries_state.queries);
    QueriesStateValidator<MAX_N>::validate_query_top_transfer(current_st, yst.queries_state.queries);
    log_rebuild_end(current_st, yst);
}

// 責務: 値を削除せず、現在の QueryOrder を使って current_st から query を全 rebuild する。
// 必要な理由: BOrder 近傍で border だけを動かした後、A2B/B2A の OrdPos/RelOrd を同期するため。
template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::rebuild_by_current_order(
        State& current_st,
        YakinamashiState<MAX_N>& yst
) {
    PhysicalDestroyContext<MAX_N> context(current_st, yst);
    my_vector<std::pair<int, int>> empty_vs;
    rebuild_by_order(context, empty_vs);
}

template<size_t MAX_N>
my_bitset<MAX_N> EraseQueriesReBuilder<MAX_N>::build_is_erased_v(
        const my_vector<std::pair<int, int>>& erased_vs
) {
    my_bitset<MAX_N> is_erased_v;
    for (const auto& [v, destroyed_cost]: erased_vs) {
        (void)destroyed_cost;
        my_assert(0 <= v && v < static_cast<int>(MAX_N));
        is_erased_v.set(v);
    }
    return is_erased_v;
}

template<size_t MAX_N>
my_vector<int> EraseQueriesReBuilder<MAX_N>::build_erased_values(
        const my_vector<std::pair<int, int>>& erased_vs
) {
    my_vector<int> erased_values;
    erased_values.reserve(erased_vs.size());
    for (const auto& [v, destroyed_cost]: erased_vs) {
        (void)destroyed_cost;
        erased_values.push_back(v);
    }
    return erased_values;
}

template<size_t MAX_N>
State EraseQueriesReBuilder<MAX_N>::build_erased_state(
        const State& current_st,
        const my_bitset<MAX_N>& is_erased_v
) {
    RingBuffer500 erased_a;
    RingBuffer500 erased_b;
    for (int i = 0; i < current_st.A.size(); i++) {
        int v = current_st.A[i];
        if (!is_erased_v[v]) erased_a.push_back(v);
    }
    for (int i = 0; i < current_st.B.size(); i++) {
        int v = current_st.B[i];
        if (!is_erased_v[v]) erased_b.push_back(v);
    }
    const int current_N = erased_a.size() + erased_b.size();
    return State(erased_a, erased_b, current_N, current_st.initial_N);
}

template<size_t MAX_N>
RebuildReplayState<MAX_N> EraseQueriesReBuilder<MAX_N>::init_replay_state(
        const State& current_st,
        const YakinamashiState<MAX_N>& yst,
        const my_bitset<MAX_N>& is_erased_v
) {
    State erased_st = build_erased_state(current_st, is_erased_v);
    RebuildReplayState<MAX_N> replay{
            erased_st,
            my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()>(),
            my_bitset<YQConstants::GET_MAX_ALL_ORD_B<MAX_N>()>(),
            yst.query_order.aorder.initial_value_type(erased_st),
            my_vector<ValState>(erased_st.initial_N, ValState::NONE)
    };
    ExistAllOrdManager::init_exist_a_ords_from_state<MAX_N>(
            erased_st, yst.query_order.aorder, replay.now_a_val_state, replay.exist_a_ords
    );
    init_exist_b_ords(replay, yst);
    return replay;
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::save_erase_order_snapshot(
        PhysicalDestroyContext<MAX_N>& context,
        const QueryOrder& before_erased_order,
        const QueryOrder& after_erased_order,
        const my_bitset<MAX_N>& is_erased_v
) {
    context.erase_order_snapshot.before_erased_order = before_erased_order;
    context.erase_order_snapshot.after_erased_order = after_erased_order;
    context.erase_order_snapshot.is_erased_v = is_erased_v;
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::init_exist_b_ords(
        RebuildReplayState<MAX_N>& replay,
        const YakinamashiState<MAX_N>& yst
) {
    for (int i = 0; i < replay.st.B.size(); i++) {
        int v = replay.st.B[i];
        replay.now_b_val_state[v] = ValState::TARGET;
        ExistAllOrdManager::set_exist(
                replay.exist_b_ords,
                yst.query_order.border.get_all_order(v, replay.now_b_val_state[v])
        );
    }
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::init_first_top_ords(
        YakinamashiState<MAX_N>& yst,
        const RebuildReplayState<MAX_N>& replay
) {
    yst.queries_state.first_top_ord_a = calc_first_top_ord_a(replay, yst);
    yst.queries_state.first_top_ord_b = calc_first_top_ord_b(replay, yst);
}

template<size_t MAX_N>
OrdPos EraseQueriesReBuilder<MAX_N>::calc_first_top_ord_a(
        const RebuildReplayState<MAX_N>& replay,
        const YakinamashiState<MAX_N>& yst
) {
    if (replay.st.A.empty()) return OrdPos(0);
    int v = replay.st.A[0];
    AllOrd all_ord = yst.query_order.aorder.get_all_order(v, replay.now_a_val_state[v]);
    return to_ord_pos(OrdCalc::get_rel_ord(replay.exist_a_ords, all_ord), replay.st.A.size());
}

template<size_t MAX_N>
OrdPos EraseQueriesReBuilder<MAX_N>::calc_first_top_ord_b(
        const RebuildReplayState<MAX_N>& replay,
        const YakinamashiState<MAX_N>& yst
) {
    if (replay.st.B.empty()) return OrdPos(0);
    int v = replay.st.B[0];
    AllOrd all_ord = yst.query_order.border.get_all_order(v, replay.now_b_val_state[v]);
    return to_ord_pos(OrdCalc::get_rel_ord(replay.exist_b_ords, all_ord), replay.st.B.size());
}

template<size_t MAX_N>
QueryCommon EraseQueriesReBuilder<MAX_N>::rebuild_query(
        const QueryCommon& old_q,
        YakinamashiState<MAX_N>& yst,
        RebuildReplayState<MAX_N>& replay
) {
    int v = old_q.get_tar_v();
    if (old_q.is_a2b_type()) return rebuild_a2b_query(v, yst, replay);
    if (old_q.is_b2a_type()) return rebuild_b2a_query(v, yst, replay);
    my_assert(old_q.is_a2a_type());
    return rebuild_a2a_query(v, yst, replay);
}

template<size_t MAX_N>
QueryCommon EraseQueriesReBuilder<MAX_N>::rebuild_a2b_query(
        int v,
        YakinamashiState<MAX_N>& yst,
        RebuildReplayState<MAX_N>& replay
) {
    OrdPos ord_pos_a = OrdCalc::get_tar_ord_posa_of_a2b<MAX_N>(
            replay.exist_a_ords, yst.query_order.aorder, v, replay.now_a_val_state[v], replay.st.A.size()
    );
    RelOrd rel_ord_b = OrdCalc::get_tar_rel_ordb_of_a2b<MAX_N>(
            replay.exist_b_ords, yst.query_order.border, v, replay.st.B.size()
    );
    return QueryCommon(CommonPushType::A2B, v, replay.st.A.size(), ord_pos_a.get(), rel_ord_b.get());
}

template<size_t MAX_N>
QueryCommon EraseQueriesReBuilder<MAX_N>::rebuild_b2a_query(
        int v,
        YakinamashiState<MAX_N>& yst,
        RebuildReplayState<MAX_N>& replay
) {
    RelOrd rel_ord_a = OrdCalc::get_tar_rel_orda_of_b2a<MAX_N>(
            replay.exist_a_ords, yst.query_order.aorder, v, replay.st.A.size()
    );
    OrdPos ord_pos_b = OrdCalc::get_tar_ord_posb_of_b2a<MAX_N>(
            replay.exist_b_ords, yst.query_order.border, v, replay.now_b_val_state[v], replay.st.B.size()
    );
    return QueryCommon(CommonPushType::B2A, v, replay.st.A.size(), rel_ord_a.get(), ord_pos_b.get());
}

template<size_t MAX_N>
QueryCommon EraseQueriesReBuilder<MAX_N>::rebuild_a2a_query(
        int v,
        YakinamashiState<MAX_N>& yst,
        RebuildReplayState<MAX_N>& replay
) {
    OrdPos tar_ord_a1 = OrdCalc::get_tar_orda1_of_a2a<MAX_N>(
            replay.exist_a_ords, yst.query_order.aorder, v, replay.st.A.size(), replay.now_a_val_state[v]
    );
    RelOrd tar_rel_ord_a2 = OrdCalc::get_tar_rel_orda2_of_a2a<MAX_N>(
            replay.exist_a_ords, yst.query_order.aorder, v, replay.st.A.size(), replay.now_a_val_state[v]
    );
    OrdPos tar_ord_a2 = to_ord_pos(tar_rel_ord_a2, replay.st.A.size());
    int flip_dis_dir = A2AQueryUtil::calc_flip_dis_dir(
            tar_ord_a1, tar_ord_a2, replay.st.A.size(), e_a2a_type::swap_rot
    );
    OrdPos top_ord_b = yst.queries_state.queries.empty()
            ? yst.queries_state.first_top_ord_b
            : yst.queries_state.queries.back().get_query().get_finished_top_ord_b(yst.queries_state.current_N);
    return QueryCommon(
            CommonPushType::A2AS, v, replay.st.A.size(),
            tar_ord_a1.get(), tar_rel_ord_a2.get(), top_ord_b.get(), flip_dis_dir
    );
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::append_rebuilt_query(
        YakinamashiState<MAX_N>& yst,
        RebuildReplayState<MAX_N>& replay,
        QCRIFactory& cri_factory,
        const QueryCommon& new_q
) {
    int q_idx = static_cast<int>(yst.queries_state.queries.size());
    QueryCommonRI new_ri = cri_factory.build_ri(yst.queries_state, new_q, q_idx);
    yst.queries_state.sum_turn += new_ri.get_ins_turn();
    yst.queries_state.queries.push_back(new_ri);
    advance_replay(yst, replay, q_idx, new_ri);
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::advance_replay(
        YakinamashiState<MAX_N>& yst,
        RebuildReplayState<MAX_N>& replay,
        int q_idx,
        const QueryCommonRI& qri
) {
    update_replay_order(yst, replay, qri.get_query());
    QueriesCalculator qs_calc(yst.queries_state.current_N, yst.queries_state.queries);
    QueryStateApplyUtil::apply_query_to_state(replay.st, qs_calc, q_idx, qri);
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::update_replay_order(
        YakinamashiState<MAX_N>& yst,
        RebuildReplayState<MAX_N>& replay,
        const QueryCommon& q
) {
    int v = q.get_tar_v();
    if (q.is_a2b_type()) {
        ExistAllOrdManager::upd_ord_a2b<MAX_N>(
                v, replay.exist_a_ords, replay.exist_b_ords,
                yst.query_order.aorder, yst.query_order.border,
                replay.now_a_val_state, replay.now_b_val_state
        );
        return;
    }
    if (q.is_b2a_type()) {
        ExistAllOrdManager::upd_ord_b2a<MAX_N>(
                v, replay.exist_a_ords, replay.exist_b_ords,
                yst.query_order.aorder, yst.query_order.border,
                replay.now_a_val_state, replay.now_b_val_state
        );
        return;
    }
    ExistAllOrdManager::upd_ord_a2a<MAX_N>(
            v, replay.exist_a_ords, yst.query_order.aorder, replay.now_a_val_state
    );
}

template<size_t MAX_N>
void EraseQueriesReBuilder<MAX_N>::recalc_final(
        YakinamashiState<MAX_N>& yst
) {
    yst.queries_state.final_rot_info = FinalRotCalculator::build_final_rot_info(
            yst.queries_state.current_N, yst.queries_state
    );
    yst.queries_state.sum_turn += yst.queries_state.final_rot_info.second;
}
