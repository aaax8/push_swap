// TestGreedyQueriesConstructorIndirect.h
#pragma once

#include "all_include.h"
#include "State.h"
#include "YstFactory.h"
#include "YakinamashiState.h"
#include "Destroy/RangeQueryDestroyer.h"
#include "Construction/GreedyQueriesConstructor.h"
#include <random>


// template<size_t MAX_N>
// void GreedyQueriesConstructor<MAX_N>::construct(

template<size_t MAX_N, int HABA = 3, int K = 2>
void test_greedy_queries_constructor(int destroy_size = 4, int case_n = 1){
    constexpr int TEST_N = 100;
    constexpr int CASE_SEED_BASE = 202604145;
    constexpr int ONLY_CASE_IDX = -1;
    for (int case_idx = 0; case_idx < case_n; case_idx++) {
        if (ONLY_CASE_IDX >= 0 && case_idx != ONLY_CASE_IDX) continue;
        if(case_idx%100==0){
            cout << "case_idx : "<<case_idx << endl;
        }
        clear_file();
        vector<int> perm(TEST_N);
        std::iota(perm.begin(), perm.end(), 0);
        const int seed = CASE_SEED_BASE + case_idx;
        std::mt19937 engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);
        out("");
        out("=== 間接検証ケース開始 ===");
        out("case_idx", case_idx, "seed", seed, "destroy_size", destroy_size);
        out("init_A_perm", perm);
        State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
        set_my_rand_seed(static_cast<uint32_t>(seed));
        set_my_rand_seed(static_cast<uint32_t>(seed));
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState<MAX_N> yst = factory.build(init_state);
        RangeQueryDestroyer<MAX_N> destroyer(init_state, destroy_size);
        GreedyConstructorParams params{10, 24, 24, 24, 24, 1 << 30, 25};
        GreedyQueriesConstructor<MAX_N> constructor(init_state, params);
        my_vector<std::pair<int, int>> destroyed_vs = destroyer.destroy(yst);
        constructor.construct(yst, destroyed_vs);
        out("=== 間接検証ケース終了 ===", "case_idx", case_idx, "seed", seed);

    }
}

template<size_t MAX_N, int HABA = 3, int K = 2>
void test_ins2_diff(int destroy_size = 4, int case_n = 1){
    constexpr int TEST_N = 100;
    constexpr int CASE_SEED_BASE = 202604145;
    constexpr int ONLY_CASE_IDX = -1;
    for (int case_idx = 0; case_idx < case_n; case_idx++) {
        if (ONLY_CASE_IDX >= 0 && case_idx != ONLY_CASE_IDX) continue;
        clear_file();
        vector<int> perm(TEST_N);
        std::iota(perm.begin(), perm.end(), 0);
        const int seed = CASE_SEED_BASE + case_idx;
        std::mt19937 engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);
        out("");
        out("=== 間接検証ケース開始 ===");
        out("case_idx", case_idx, "seed", seed, "destroy_size", destroy_size);
        out("init_A_perm", perm);
        State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
        set_my_rand_seed(static_cast<uint32_t>(seed));
        set_my_rand_seed(static_cast<uint32_t>(seed));
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState<MAX_N> yst = factory.build(init_state);
        RangeQueryDestroyer<MAX_N> destroyer(init_state, destroy_size);
        GreedyConstructorParams params{10, 24, 24, 24, 24, 1 << 30, 25};
        GreedyQueriesConstructor<MAX_N> constructor(init_state, params);
        my_vector<std::pair<int, int>> destroyed_vs = destroyer.destroy(yst);
        constructor.debug_ins2(100, yst, destroyed_vs);
        out("=== 間接検証ケース終了 ===", "case_idx", case_idx, "seed", seed);

    }
}

template<size_t MAX_N, int HABA = 3, int K = 2>
void test_greedy_queries_constructor_indirect(int destroy_size = 4, int case_n = 1) {
    constexpr int TEST_N = 25;
    constexpr int CASE_SEED_BASE = 202604145;
    constexpr int ONLY_CASE_IDX = -1;
    for (int case_idx = 0; case_idx < case_n; case_idx++) {
        if (ONLY_CASE_IDX >= 0 && case_idx != ONLY_CASE_IDX) continue;
        clear_file();
        vector<int> perm(TEST_N);
        std::iota(perm.begin(), perm.end(), 0);
        const int seed = CASE_SEED_BASE + case_idx;
        std::mt19937 engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);
        out("");
        out("=== 間接検証ケース開始 ===");
        out("case_idx", case_idx, "seed", seed, "destroy_size", destroy_size);
        out("init_A_perm", perm);
        State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
        set_my_rand_seed(static_cast<uint32_t>(seed));
        set_my_rand_seed(static_cast<uint32_t>(seed));
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState<MAX_N> yst = factory.build(init_state);
        RangeQueryDestroyer<MAX_N> destroyer(init_state, destroy_size);
        GreedyConstructorParams params{10, 24, 24, 24, 24, 1 << 30, 25};
        GreedyQueriesConstructor<MAX_N> constructor(init_state, params);
        my_vector<std::pair<int, int>> destroyed_vs = destroyer.destroy(yst);
        constructor.debug_run_a2b_indirect_q_idx_checks(yst, destroyed_vs);
        out("=== 間接検証ケース終了 ===", "case_idx", case_idx, "seed", seed);
    }
}

template<size_t MAX_N, int HABA = 3, int K = 2>
void test_greedy_queries_constructor_a2b_b2a_indirect(int destroy_size = 4, int case_n = 1) {
    constexpr int TEST_N = 10;
    constexpr int CASE_SEED_BASE = 202604145;
    constexpr int ONLY_CASE_IDX = -1;
    for (int case_idx = 0; case_idx < case_n; case_idx++) {
        if (ONLY_CASE_IDX >= 0 && case_idx != ONLY_CASE_IDX) continue;
        clear_file();
        vector<int> perm(TEST_N);
        std::iota(perm.begin(), perm.end(), 0);
        const int seed = CASE_SEED_BASE + case_idx;
        std::mt19937 engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);
        out("");
        out("=== A2B_B2A 間接検証ケース開始 ===");
        out("case_idx", case_idx, "seed", seed, "destroy_size", destroy_size);
        out("init_A_perm", perm);
        State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
        set_my_rand_seed(static_cast<uint32_t>(seed));
        set_my_rand_seed(static_cast<uint32_t>(seed));
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState<MAX_N> yst = factory.build(init_state);
        RangeQueryDestroyer<MAX_N> destroyer(init_state, destroy_size);
        GreedyConstructorParams params{10, 24, 24, 24, 24, 1 << 30, 25};
        GreedyQueriesConstructor<MAX_N> constructor(init_state, params);
        my_vector<std::pair<int, int>> destroyed_vs = destroyer.destroy(yst);
        constructor.debug_dump_a2b_b2a_indirect_diffs(yst, destroyed_vs);
        out("=== A2B_B2A 間接検証ケース終了 ===", "case_idx", case_idx, "seed", seed);
    }
}
