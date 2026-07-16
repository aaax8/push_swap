//
// Created by Auto
//

#ifndef UNTITLED_PASTYAKINAMASHI_H
#define UNTITLED_YAKINAMASHI_H

#include "../../all_include.h"
#include "../../State.h"
#include "../../util.h"
#include "../InitBeam/BeamQuery.h"
#include "../../TimeKeeperDouble.h"
#include <fstream>

namespace YakinamashiConstants {
    const int INF_VAL = numeric_limits<int>::max();
}

template<size_t my_bitset_size>
class Yakinamashi {
private:
    // TODO: 近傍の基底クラス
    struct NH_Base {
        virtual ~NH_Base() = default;
        virtual int calc_score_diff() = 0;
        virtual void transition() = 0;
        virtual void revert_calc() = 0;
    };

    // TODO: 状態の型を定義（BeamQueryに対応した状態）
    // struct PastYakinamashiState {
    //     my_vector<BeamQuery> queries;
    //     int now_sum_turn;
    //     void copy_from(const PastYakinamashiState &rhs) { ... }
    // };

    // TODO: 現在の状態への参照
    // PastYakinamashiState &now_state;

    // TODO: 近傍ファクトリー
    // std::unique_ptr<PastNH_Base> get_nh(double progress) {
    //     // 近傍を生成する処理
    //     return nullptr;
    // }

public:
    // TODO: コンストラクタ
    // Yakinamashi(PastYakinamashiState &yst) : now_state(yst) {
    // }

private:
    bool should_accept_transition(int turn_diff, double temp, std::mt19937 &mt_for_action, int INF) {
        my_assert(turn_diff <= YakinamashiConstants::INF_VAL);
        if (turn_diff == YakinamashiConstants::INF_VAL) {
            return false;
        }
        if (turn_diff < 0) {
            return true;
        }
        double probability = exp(-turn_diff / temp);
        return probability > (mt_for_action() % INF) / (double) INF;
    }

    // TODO: 最良状態の更新メソッド
    // void update_best_state_if_needed(int &best_turn, int now_turn, PastYakinamashiState &best_state) {
    //     if (best_turn > now_turn) {
    //         best_turn = now_turn;
    //         best_state.copy_from(now_state);
    //     }
    // }

    // TODO: SA反復処理
    // void perform_sa_iteration(double progress, double start_temp, double end_temp,
    //                           std::mt19937 &mt_for_action, int INF,
    //                           TimeKeeperDouble &time_keeper, int64_t time_threshold,
    //                           int &now_turn, int &best_turn, PastYakinamashiState &best_state,
    //                           std::ofstream &trans_log) {
    //     std::unique_ptr<PastNH_Base> nh = get_nh(progress);
    //     int turn_diff = nh->calc_score_diff();
    //     double temp = start_temp + (end_temp - start_temp) * progress;
    //
    //     if (should_accept_transition(turn_diff, temp, mt_for_action, INF)) {
    //         now_turn += turn_diff;
    //         nh->transition();
    //         update_best_state_if_needed(best_turn, now_turn, best_state);
    //     } else {
    //         nh->revert_calc();
    //     }
    //     time_keeper.setNowTime();
    // }

public:
    // TODO: SAメイン処理
    // PastYakinamashiState do_sa(int64_t time_threshold, double start_temp, double end_temp) {
    //     if (now_state.queries.size() < 2) {
    //         return now_state;
    //     }
    //
    //     std::filesystem::path trans_log_path = output_dir / "trans_log.txt";
    //     std::ofstream trans_log(trans_log_path);
    //
    //     std::mt19937 mt_for_action(8769);
    //     using ScoreType = int;
    //     ScoreType INF = numeric_limits<ScoreType>::max();
    //     auto time_keeper = TimeKeeperDouble((double) time_threshold);
    //     ScoreType best_turn = now_state.now_sum_turn;
    //     ScoreType now_turn = best_turn;
    //     auto best_state = now_state;
    //
    //     int loop_cnt = 0;
    //     for (;;) {
    //         loop_cnt++;
    //         double progress = ((double) time_keeper.getNowTime() / time_threshold);
    //         perform_sa_iteration(progress, start_temp, end_temp, mt_for_action, INF,
    //                              time_keeper, time_threshold, now_turn, best_turn, best_state, trans_log);
    //         if (time_keeper.isTimeOver()) {
    //             break;
    //         }
    //     }
    //     out("loop_cnt", loop_cnt);
    //     return best_state;
    // }
};

#endif //UNTITLED_PASTYAKINAMASHI_H

