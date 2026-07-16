// TestRangeQueryDestroyer.h
#pragma once

#include "all_include.h"
#include "State.h"
#include "YstFactory.h"
#include "YakinamashiState.h"
#include "QueriesState.h"
#include "YQAliases.h"
#include "QueryStateApplyUtil.h"
#include "QueriesCalculator.h"
#include "Destroy/RangeQueryDestroyer.h"
#include "Validation/YakinamashiStateValidator.h"
#include "Order/StackOrdUtil.h"
#include "QueryCommon.h"
#include "RoughQuery.h"

template<size_t MAX_N, int HABA = 3, int K = 2>
class TestRangeQueryDestroyer {
    using YakinamashiState = ::YakinamashiState<MAX_N>;
    const State& init_st;
public:
    explicit TestRangeQueryDestroyer(const State& init_st) : init_st(init_st) {}

    void run(int test_n = 100) {
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState original_yst = factory.build(init_st);

        for (int iter = 0; iter < test_n; iter++) {
            YakinamashiState yst = original_yst;
            QueriesState snapshot_qs = original_yst.queries_state;

            RangeQueryDestroyer<MAX_N> destroyer(init_st);
            my_vector<std::pair<int, int>> destroyed_vs = destroyer.destroy(yst);

            my_bitset<MAX_N> is_destroyed;
            for (const auto& [v, destroyed_cost] : destroyed_vs) {
                (void)destroyed_cost;
                is_destroyed.set(v);
            }

            verify_simulation(yst, is_destroyed);
            verify_query_common_match(snapshot_qs, yst.queries_state.queries, is_destroyed);
            verify_destroyed_cost(snapshot_qs, destroyed_vs);
            QueriesStateValidator<MAX_N>::validate_sum_turn(yst.queries_state);
        }
    }

private:
    my_vector<ValState> build_final_a_val_state(const YakinamashiState& yst) const {
        my_vector<ValState> a_val = yst.query_order.aorder.initial_value_type(init_st);
        for (const auto& qri : yst.queries_state.queries) {
            int v = qri.get_tar_v();
            if (qri.is_a2b_type()) {
                a_val[v] = ValState::NONE;
            } else {
                a_val[v] = ValState::TARGET;
            }
        }
        return a_val;
    }

    void verify_final_a_all_ord_ring_sorted(
        const State& st, const YakinamashiState& yst, const my_vector<ValState>& final_a_val
    ) const {
        my_vector<int> a_all_ords;
        for (int i = 0; i < (int)st.A.size(); i++) {
            int v = st.A[i];
            a_all_ords.push_back(yst.query_order.aorder.get_all_order(v, final_a_val[v]).get());
        }
        auto min_it = std::min_element(a_all_ords.begin(), a_all_ords.end());
        int min_i = (int)(min_it - a_all_ords.begin());
        int prev_ord = -1;
        for (int loop_i = 0; loop_i < (int)a_all_ords.size(); loop_i++) {
            int i = mod(loop_i + min_i, (int)a_all_ords.size());
            my_assert(prev_ord < a_all_ords[i]);
            prev_ord = a_all_ords[i];
        }
    }

    void verify_simulation(const YakinamashiState& yst, const my_bitset<MAX_N>& is_destroyed) {
        int N = init_st.current_N;
        State st(init_st);
        my_vector<ValState> final_a_val = build_final_a_val_state(yst);
        QueriesCalculator qs_calc(N, yst.queries_state.queries);
        for (int qi = 0; qi < (int)yst.queries_state.queries.size(); qi++) {
            QueryStateApplyUtil::apply_query_to_state(st, qs_calc, qi, yst.queries_state.queries[qi]);
        }
        auto [cmd, cnt] = yst.queries_state.final_rot_info;
        QueryStateApplyUtil::apply_a_rot(st, cmd, cnt);

        my_assert((int)st.B.size() == 0);
        my_assert((int)st.A.size() == N);
        verify_final_a_all_ord_ring_sorted(st, yst, final_a_val);

        int prev = -1;
        for (int i = 0; i < (int)st.A.size(); i++) {
            int v = st.A[i];
            if (!is_destroyed[v]) {
                my_assert(prev < v);
                prev = v;
            }
        }
    }

    static my_vector<RoughQuery> to_rough_without(
        const QRIList& queries, const my_bitset<MAX_N>& is_excluded
    ) {
        my_vector<RoughQuery> result;
        for (const auto& qri : queries) {
            if (!is_excluded[qri.get_tar_v()]) {
                result.push_back(to_rough_query(qri.get_query()));
            }
        }
        return result;
    }

    static void verify_query_common_match(
        const QueriesState& before_qs, const QRIList& after_queries,
        const my_bitset<MAX_N>& is_destroyed
    ) {
        auto before_common = to_rough_without(before_qs.queries, is_destroyed);
        auto after_common = to_rough_queries(after_queries);
        my_assert(before_common.size() == after_common.size());
        for (int i = 0; i < (int)before_common.size(); i++) {
            my_assert(before_common[i] == after_common[i]);
        }
    }

    static void verify_destroyed_cost(
        const QueriesState& snapshot_qs, const my_vector<std::pair<int, int>>& destroyed_vs
    ) {
        for (const auto& [v, destroyed_cost] : destroyed_vs) {
            int expected_cost = 0;
            for (const auto& qri : snapshot_qs.queries) {
                if (qri.get_tar_v() == v) expected_cost += qri.get_ins_turn();
            }
            my_assert(expected_cost == destroyed_cost);
        }
    }
};

template<size_t MAX_N, int HABA = 3, int K = 2>
void test_range_query_destroyer() {
    for (int _ = 0; _ < 10; _++) {
//        int TEST_N = my_rand(10, 101);
        int TEST_N = 10;


        vector<int> perm(TEST_N);
        std::iota(perm.begin(), perm.end(), 0);
        std::mt19937 engine((uint64_t)std::chrono::high_resolution_clock::now()
            .time_since_epoch().count());
        std::shuffle(perm.begin(), perm.end(), engine);
        State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
        TestRangeQueryDestroyer<MAX_N, HABA, K> tester(init_state);
        tester.run(100);
    }
}
