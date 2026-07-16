#pragma once

#include "BaseGreedyConstruction.h"
#include "Destroy/AOrderRangePlanBuilder.h"
#include "Destroy/PhyQDestroyPlan.h"
#include "Query/QueryCommon.h"
#include "State.h"
#include "State/YakinamashiState.h"

#include <array>
#include <climits>
#include <string>

enum class CollectorLogMode {
    disabled,
    enabled
};

// 責務: GreedyQueriesConstructor の候補列挙と top_k 展開に使う調整値を保持する。
// 必要な理由: Optuna などの外部実験から top_k, ext, limit をまとめて指定するため。
struct GreedyConstructorParams {
    int top_k;
    // 責務: A 側と B 側で別々に指定する候補 range 拡張幅を保持する。
    // 必要な理由: query destroy 後に A 側だけ広い候補範囲を試せるようにするため。
    int from_ext_a;
    int to_ext_a;
    int from_ext_b;
    int to_ext_b;
    int a2b_lim;
    int b2a_lim;
    int destroy_mask = 3;
};

template<size_t MAX_N>
class GreedyQueriesConstructor : public BaseGreedyConstruction<MAX_N> {
public:
    static constexpr size_t MAX_ALL_ORD_A = YQConstants::GET_MAX_ALL_ORD_A<MAX_N>();
    static constexpr size_t MAX_ALL_ORD_B = YQConstants::GET_MAX_ALL_ORD_B<MAX_N>();
    static constexpr size_t MAX_INSERT_SLOT_B = MAX_ALL_ORD_B + 1;
    static constexpr size_t MAX_INSERT_EVENT_B = MAX_ALL_ORD_B + 2;
    static constexpr int MAX_QUERY_CNT = MAX_N * 2 + 1;
    static void set_trace_enabled(bool enabled);
    static void clear_trace_log();
    static void append_trace_marker(const std::string& line);
    static const my_vector<std::string>& get_last_construct_output_lines();
    static void set_a_order_gap_priority_enabled(bool enabled);
    static bool is_a_order_gap_priority_enabled();
    static void set_owner_collector_enabled(bool enabled);
    static void set_owner_check_enabled(bool enabled);
    static void set_owner_unit_range_enabled(bool enabled);
    static void set_owner_even_unit_enabled(bool enabled);
    static void set_pair_refine_enabled(bool enabled);
    static void set_pair_refine_params(int top_k, int a2b_idx_range, int b2a_idx_range, int all_ord_range);
    static void set_owner_array_enabled(bool enabled);
    static void set_timing_log_enabled(bool enabled);
    static bool is_timing_log_enabled();
    static void set_timing_iter(int iter);
    static void set_timing_target_iter(int iter);
    static void write_timing_log(const std::string& line);

    explicit GreedyQueriesConstructor(
            const State& init_st,
            const GreedyConstructorParams& params,
            CollectorLogMode collector_log_mode = CollectorLogMode::disabled
    );

    void nieve_construct(YakinamashiState<MAX_N>& yst, const my_vector<std::pair<int, int>>& destroyed_vs) ;
//    void construct(YakinamashiState& yst, const my_vector<std::pair<int, int>>& destroyed_vs) override;
    void debug_ins2(int case_n,
            const YakinamashiState<MAX_N>& yst, const my_vector<std::pair<int, int>>& destroyed_vs
    );
    void debug_run_a2b_indirect_q_idx_checks(
            const YakinamashiState<MAX_N>& yst, const my_vector<std::pair<int, int>>& destroyed_vs
    );
    // : Add a dedicated debug entry for the A2B+B2A fixed-diff builder so it can be inspected without changing the existing checker flow.
    void debug_dump_a2b_b2a_indirect_diffs(
        const YakinamashiState<MAX_N>& yst, const my_vector<std::pair<int, int>>& destroyed_vs
    );

    void construct(YakinamashiState<MAX_N>& yst,
                   const my_vector<std::pair<int, int>>& destroyed_vs);
    void construct_only_query_destroyed(
            YakinamashiState<MAX_N>& yst,
            const my_vector<std::pair<int, int>>& destroyed_vs
    );

    void construct_physical_destroyed(
            PhysicalDestroyContext<MAX_N>& context,
            const my_vector<std::pair<int, int>>& destroyed_vs
    );

    void construct_phy_q_destroyed(
            PhysicalDestroyContext<MAX_N>& context,
            const PhyQDestroyPlan<MAX_N>& plan
    );

    void construct_physical_middle(
            PhysicalDestroyContext<MAX_N>& context,
            const my_vector<std::pair<int, int>>& destroyed_vs
    );

    void construct_worst_half_rebuild(
            YakinamashiState<MAX_N>& yst,
            const my_vector<std::pair<int, int>>& destroyed_vs
    );

    void construct_physical_worst_half_rebuild(
            PhysicalDestroyContext<MAX_N>& context,
            const my_vector<std::pair<int, int>>& destroyed_vs
    );

    void construct_planned_query(
            YakinamashiState<MAX_N>& yst,
            const AOrderRangePlan<MAX_N>& plan
    );

    void construct_planned_physical(
            PhysicalDestroyContext<MAX_N>& context,
            const AOrderRangePlan<MAX_N>& plan
    );

private:
    struct Helper;

    void construct_v_only_query_destroyed(
            const State& current_st,
            YakinamashiState<MAX_N>& yst,
            int v
    );

    void construct_v_physical_destroyed(
            PhysicalDestroyContext<MAX_N>& context,
            int v
    );
    void construct_v_physical_middle(
            PhysicalDestroyContext<MAX_N>& context,
            int v
    );
    void construct_v_fixed_b(
            YakinamashiState<MAX_N>& yst,
            const State& current_st,
            int v
    );

    void construct_v_physical_fixed_b(
            PhysicalDestroyContext<MAX_N>& context,
            int v
    );
    void construct_v_fixed_a(
            YakinamashiState<MAX_N>& yst,
            const State& current_st,
            int v
    );

    void construct_v_physical_fixed_a(
            PhysicalDestroyContext<MAX_N>& context,
            const AOrderRangePlan<MAX_N>& plan,
            int v,
            my_bitset<MAX_N>& missing_v
    );
    void construct_ordered_vs(
            YakinamashiState<MAX_N>& yst,
            const my_vector<std::pair<int, int>>& ordered_vs
    );

    void construct_physical_ordered_vs(
            PhysicalDestroyContext<MAX_N>& context,
            const my_vector<std::pair<int, int>>& ordered_vs
    );
    void construct_phy_q_ordered_vs(
            PhysicalDestroyContext<MAX_N>& context,
            const PhyQDestroyPlan<MAX_N>& plan,
            const my_vector<std::pair<int, int>>& ordered_vs
    );
    void construct_greedy_order(
            YakinamashiState<MAX_N>& yst,
            const my_vector<std::pair<int, int>>& destroyed_vs
    );

    void construct_physical_greedy(
            PhysicalDestroyContext<MAX_N>& context,
            const my_vector<std::pair<int, int>>& destroyed_vs
    );
    my_vector<std::pair<int, int>> pick_worst_half_rebuild_vs(
            const YakinamashiState<MAX_N>& yst,
            const my_vector<std::pair<int, int>>& destroyed_vs
    );

    void rebuild_worst_half_only_query(
            YakinamashiState<MAX_N>& yst,
            const my_vector<std::pair<int, int>>& rebuild_vs
    );

    void rebuild_worst_half_physical(
            PhysicalDestroyContext<MAX_N>& context,
            const my_vector<std::pair<int, int>>& rebuild_vs
    );
    void construct_v_by_nieve(YakinamashiState<MAX_N>& yst, int v);
    void reset_never_construct_vs(const my_vector<std::pair<int, int>>& target_vs);
    void mark_constructed_v(int v);

    const State& init_st;
    GreedyConstructorParams params;
    CollectorLogMode collector_log_mode;
    my_bitset<MAX_N> is_never_construct_v;
    static bool use_a_order_gap_priority;
    static bool use_owner_collector;
    static bool use_owner_check;
    static bool use_owner_unit_range;
    static bool use_owner_even_unit;
    static bool use_pair_refine;
    static int pair_refine_top_k;
    static int pair_refine_a2b_idx_range;
    static int pair_refine_b2a_idx_range;
    static int pair_refine_all_ord_range;
    static bool use_owner_array_range;
    static bool use_timing_log;
    static int timing_iter;
    static int timing_target_iter;
};
