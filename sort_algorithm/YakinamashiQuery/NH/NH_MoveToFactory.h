//x
// filepath: sort_algorithm/YakinamashiQuery/NH/NH_MoveToFactory.h
#pragma once

#include "NH_Constants.h"
#include "NH_Base.h"
#include "NH_QueryMoveTo.h"
#include "QueriesState.h"
#include "YakinamashiState.h"
#include "State.h"
#include "util.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

template<size_t bitset_size>
class NH_MoveToFactory {
public:
    static constexpr double START_TEMPERATURE = 10.0;
    static constexpr double END_TEMPERATURE = 0.1;
    static constexpr int FINAL_ROT_BONUS = 1;
    static constexpr int RANDOM_SCALE = 1 << 30;

    static std::unique_ptr<NH_Base<int, bitset_size>> create(YakinamashiState<bitset_size> &yst, const State &init_st, double progress) {
        double clamped_progress = std::clamp(progress, NHConstants::MIN_PROGRESS, NHConstants::MAX_PROGRESS);
        my_assert(clamped_progress == progress);
        my_assert(yst.queries_state.queries.size() > 0);
//        int to_idx = select_to_idx_softmax(yst.queries_state, progress);
        int to_idx = my_rand(0, yst.queries_state.queries.size()+1);
        return std::make_unique<NH_MoveTo<bitset_size>>(yst, init_st, to_idx);
    }

private:
    static double get_temperature(double progress) {
        my_assert(NHConstants::MIN_PROGRESS <= progress && progress <= NHConstants::MAX_PROGRESS);
        return START_TEMPERATURE + (END_TEMPERATURE - START_TEMPERATURE) * progress;
    }

    static std::vector<double> build_to_scores(const QueriesState &state) {
        int query_count = static_cast<int>(state.queries.size());
        std::vector<double> scores(query_count + 1);
        for (int to_idx = 0; to_idx < query_count; to_idx++) {
            scores[to_idx] = static_cast<double>(state.queries[to_idx].get_ins_turn());
        }
        scores[query_count] = static_cast<double>(state.final_rot_info.second + FINAL_ROT_BONUS);
        return scores;
    }

    static int sample_softmax_index(const std::vector<double> &scores, double temperature) {
        my_assert(!scores.empty());
        my_assert(temperature > 0.0);
        double max_score = *std::max_element(scores.begin(), scores.end());
        std::vector<double> weights(scores.size());
        double sum_weights = 0.0;
        for (int i = 0; i < static_cast<int>(scores.size()); i++) {
            weights[i] = std::exp((scores[i] - max_score) / temperature);
            sum_weights += weights[i];
        }
        double rand_0_1 = static_cast<double>(my_rand(RANDOM_SCALE)) / static_cast<double>(RANDOM_SCALE);
        double target = rand_0_1 * sum_weights;
        double acc = 0.0;
        for (int i = 0; i < static_cast<int>(weights.size()); i++) {
            acc += weights[i];
            if (acc >= target) {
                return i;
            }
        }
        return static_cast<int>(weights.size()) - 1;
    }

    static int select_to_idx_softmax(const QueriesState &state, double progress) {
        std::vector<double> scores = build_to_scores(state);
        double temperature = get_temperature(progress);
        return sample_softmax_index(scores, temperature);
    }
};

//x ユーザー指示の表記に合わせた別名
template<size_t bitset_size>
using Nh_MoveToFactory = NH_MoveToFactory<bitset_size>;
