//x
// filepath: sort_algorithm/YakinamashiQuery/SimpleYakinamashi.h
#pragma once

#include "NH/NH_MoveToFactory.h"
#include "Test/MoveToAnalyzer.h"
#include "YakinamashiState.h"
#include "State.h"
#include "util.h"
#include <cmath>

class SimpleYakinamashi {
public:
    static constexpr int SCORE_DIFF_ACCEPT_THRESHOLD = 0;
    static constexpr int RANDOM_SCALE = 1 << 30;

    template<size_t bitset_size>
    static void run(YakinamashiState<bitset_size> &yst,
                    const State &init_st,
                    int iteration_count,
                    double start_temperature,
                    double end_temperature) {
        my_assert(iteration_count >= 0);
        my_assert(start_temperature >= end_temperature);
        my_assert(end_temperature > 0.0);
        if (yst.queries_state.queries.size() <= 1) {
            return;
        }
        for (int iter = 0; iter < iteration_count; iter++) {
            double temperature = get_temperature_by_iteration(iter, iteration_count,
                                                              start_temperature, end_temperature);
            auto nh = Nh_MoveToFactory<bitset_size>::create(yst, init_st, get_progress(iter, iteration_count));
            int score_diff = nh->calc_score_diff();
            if (should_accept_transition(score_diff, temperature)) {
                nh->transition();
            } else {
                nh->revert_calc();
            }
        }
        MoveToAnalyzer<bitset_size>::analyze(yst, init_st);

    }

private:
    static double get_progress(int iteration, int iteration_count) {
        my_assert(iteration_count >= 0);
        if (iteration_count <= 1) {
            return NHConstants::MAX_PROGRESS;
        }
        double progress = static_cast<double>(iteration) / static_cast<double>(iteration_count - 1);
        my_assert(NHConstants::MIN_PROGRESS <= progress && progress <= NHConstants::MAX_PROGRESS);
        return progress;
    }

    static double get_temperature_by_iteration(int iteration,
                                               int iteration_count,
                                               double start_temperature,
                                               double end_temperature) {
        double progress = get_progress(iteration, iteration_count);
        return start_temperature + (end_temperature - start_temperature) * progress;
    }

    static bool should_accept_transition(int score_diff, double temperature) {
        my_assert(temperature > 0.0);
        if (score_diff <= SCORE_DIFF_ACCEPT_THRESHOLD) {
            return true;
        }
        double acceptance_probability = std::exp(-static_cast<double>(score_diff) / temperature);
        return acceptance_probability > get_random_0_1();
    }

    static double get_random_0_1() {
        return static_cast<double>(my_rand(RANDOM_SCALE)) / static_cast<double>(RANDOM_SCALE);
    }
};
