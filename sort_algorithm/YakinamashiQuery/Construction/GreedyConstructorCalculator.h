#pragma once

#include "State.h"
#include "State/YakinamashiState.h"
#include "YakinamashiQueryConstants.h"

struct GreedyConstructorPrecalc {
    static constexpr size_t MAX_N = MAX_BUFF;
    static constexpr size_t MAX_ALL_ORD_B = YQConstants::GET_MAX_ALL_ORD_B<MAX_N>();
    static constexpr size_t MAX_INSERT_EVENT_B = MAX_ALL_ORD_B + 2;

    std::array<my_vector<int>, MAX_INSERT_EVENT_B> start_q_segment;
    std::array<my_vector<int>, MAX_INSERT_EVENT_B> end_q_segment;
    my_vector<int> non_band_prefix_sum;
    int non_band_total = 0;
};

template<size_t MAX_N>
class GreedyConstructorCalculator {
public:
    static GreedyConstructorPrecalc precalc(
            const State& init_st,
            const YakinamashiState<MAX_N>& yst,
            int v
    );
};
