// TestGreedyQueriesConstructor.h
#pragma once

#include "all_include.h"
#include "State.h"
#include "YstFactory.h"
#include "YakinamashiState.h"
#include "Destroy/RangeQueryDestroyer.h"
#include "Construction/GreedyQueriesConstructor.h"
#include "Validation/YakinamashiStateValidator.h"
#include <fstream>

namespace {
constexpr const char* GREEDY_CASE_INPUTS_PATH = "greedy_case_inputs.txt";
constexpr const char* GREEDY_SCAN_DEBUG_PATH = "greedy_scan_debug.txt";
constexpr const char* GREEDY_FAILURE_DEBUG_PATH = "greedy_failure_debug.txt";

std::string to_perm_string(const std::vector<int>& perm) {
    std::stringstream ss;
    ss << "[";
    for (int i = 0; i < static_cast<int>(perm.size()); i++) {
        if (i != 0) ss << ", ";
        ss << perm[i];
    }
    ss << "]";
    return ss.str();
}

void append_greedy_case_input_log(int case_idx, int seed, const std::vector<int>& perm) {
    std::ofstream ofs(GREEDY_CASE_INPUTS_PATH, std::ios::app);
    if (!ofs.is_open()) return;
    ofs << "case=" << case_idx << ", seed=" << seed << ", perm=" << to_perm_string(perm) << '\n';
}
}

/* : construct と destroy の後で validator が落ちる case を具体入力で追えるようにする。 */
template<size_t MAX_N, int HABA = 3, int K = 2>
class TestGreedyQueriesConstructor {
    using YakinamashiState = ::YakinamashiState<MAX_N>;
    const State& init_st;
public:
    explicit TestGreedyQueriesConstructor(const State& init_st) : init_st(init_st) {}

    void run(int test_n = 20) {
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState original_yst = factory.build(init_st);
        for (int iter = 0; iter < test_n; iter++) run_single_case(original_yst);
    }

private:
    void run_single_case(const YakinamashiState& original_yst) {
        YakinamashiState yst = original_yst;
        cout << "bef sum_turn"<<yst.queries_state.sum_turn << endl;
        RangeQueryDestroyer<MAX_N> destroyer(init_st);
        GreedyConstructorParams params{10, 24, 24, 24, 24, 1 << 30, 25};
        GreedyQueriesConstructor<MAX_N> constructor(init_st, params);
        my_vector<std::pair<int, int>> destroyed_vs = destroyer.destroy(yst);
        constructor.construct(yst, destroyed_vs);
        QueriesStateValidator<MAX_N>::validate_state(init_st, yst.queries_state);
        cout << "aft sum_turn"<<yst.queries_state.sum_turn << endl;
    }
};

/* : 1 case に絞り、seed と permutation をその場で見えるようにする。 */
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_greedy_queries_constructor() {
    constexpr int TEST_CASE_N = 1000;
    constexpr int TEST_N = 100;
    constexpr int CASE_SEED_BASE = 20260319;
    GreedyQueriesConstructor<MAX_N>::clear_trace_log();
    GreedyQueriesConstructor<MAX_N>::set_trace_enabled(false);
    std::remove(GREEDY_CASE_INPUTS_PATH);
    std::remove(GREEDY_SCAN_DEBUG_PATH);
    std::remove(GREEDY_FAILURE_DEBUG_PATH);
    for (int case_idx = 0; case_idx < TEST_CASE_N; case_idx++) {
        cout << "case_" << case_idx << endl;
        vector<int> perm(TEST_N);
        std::iota(perm.begin(), perm.end(), 0);
        const int seed = CASE_SEED_BASE + case_idx;
        std::mt19937 engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);
        cout << "seed=" << seed << ", perm=" << to_perm_string(perm) << endl;
        append_greedy_case_input_log(case_idx, seed, perm);
        State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
        TestGreedyQueriesConstructor<MAX_N, HABA, K> tester(init_state);
        tester.run(1);
    }
    GreedyQueriesConstructor<MAX_N>::set_trace_enabled(false);
}
