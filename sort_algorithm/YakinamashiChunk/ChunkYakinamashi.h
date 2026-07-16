//
// ChunkYakinamashi.h - Chunk-based simulated annealing for push_swap
// 責務: ChunkState管理 + 評価関数実装 + 焼きなまし統括（近傍は後で）
//

#ifndef UNTITLED_CHUNKYAKINAMASHI_H
#define UNTITLED_CHUNKYAKINAMASHI_H

#include "../../all_include.h"
#include "../../util.h"
#include "../../State.h"
#include "../../Command/Command.h"
#include "../../TimeKeeperDouble.h"
#include "ChunkSeparator.h"
#include "ChunkStateFactory.h"
#include "TemperatureSchedule.h"
#include "NHSelector.h"
#include "YakinamashiChunk/YakinamashiChunkState.h"

namespace YakinamashiChunk {
}  // namespace YakinamashiChunk

//x ChunkYakinamashi - Chunk-based焼きなまし
template<size_t MAX_N>
class ChunkYakinamashi {
private:

    int state_N;
    const State &initial_state;  // 初期状態への参照
    int last_score;  // 最後に計算した評価スコア（ターン数）

    //x Chunk分離処理を実行し、分離後のコマンド数と状態を返す
    pair<int, State> run_chunk_separation(
            const YakinamashiChunk::YakinamashiChunkState<MAX_N> &chunk_st) {
        YakinamashiChunk::ChunkCommandGenerator<MAX_N> generator(
                chunk_st.tree, chunk_st.constraint, chunk_st.queries);
        auto [sep_commands, separated_state] = generator.generate_commands_constraint(initial_state);
        out("run_chunk_sep");
        initial_state.print();
        out("sep_cmds");
        out(sep_commands);
        return {static_cast<int>(sep_commands.size()), separated_state};
    }

    //x Beam検索を実行し、コマンド数を返す
    template<size_t HABA = 15, int K = 2>
    int run_beam_search(
            const State &separated_state,
            const YakinamashiChunk::YakinamashiChunkState<MAX_N> &chunk_st) {
        BeamSearcher<HABA, K, MAX_N> searcher;
        auto beam_result = searcher.search_finished_a2b(
                separated_state, chunk_st.val_state.is_a_lis, chunk_st.tree);
        auto [best_query, best_cmds] = searcher.reconstruct_commands(separated_state, beam_result);
        out("beam_cmds");
        out(best_cmds);
        if (HABA > 15) {
            my_vector<int> turns(separated_state.current_N, -1);
            int sum=0;
            for (BeamQuery &q: best_query) {
                int turn_diff=q.get_ins_turn_with_prev_diff();
                turns[q.get_tar_val()] = turn_diff;
                sum += turn_diff;
            }
            std::sort(turns.rbegin(), turns.rend());
            out("sum_turn", sum);
            out("avg_turn", sum / (double)(best_query.size()));
            out("ins_turns");
            for(auto& t : turns){
                out(t);
            }
        }
        return static_cast<int>(best_cmds.size());
    }

public:
    //o
    ChunkYakinamashi(int n, const State &init_state)
            : state_N(n), initial_state(init_state), last_score(-1) {
        my_assert(0 < state_N && state_N <= MAX_N);
        my_assert(init_state.A.size() == state_N);
    }

    //x 評価関数 - 与えられたChunkStateを評価してターン数を返す
    template<size_t HABA = 15, int K = 2>
    int evaluate(const YakinamashiChunk::YakinamashiChunkState<MAX_N> &chunk_st) {
        my_assert(0 < state_N && state_N <= MAX_N);

        auto [sep_turns, separated_state] = run_chunk_separation(chunk_st);
        int beam_turns = run_beam_search<HABA, K>(separated_state, chunk_st);

        last_score = sep_turns + beam_turns;
        return last_score;
    }

    //o 最後に計算した評価スコアを返す
//    int get_last_score() const {
//        return last_score;
//    }

    int best_score;

    int get_best_score() const {
        return best_score;
    }

    //o 状態のサイズを返す
    int get_state_n() const {
        return state_N;
    }

    // //x 焼きなまし主ループを実行し、最良のstateを返す
    pair<YakinamashiChunk::YakinamashiChunkState<MAX_N>, int> run_annealing(
            const YakinamashiChunk::YakinamashiChunkState<MAX_N> &init_state,
            TimeKeeperDouble &time_keeper,
            double init_temp = 15.0,
            double final_temp = 1.0) {

        my_assert(0 < state_N && state_N <= MAX_N);

        YakinamashiChunk::YakinamashiChunkState<MAX_N> current_state = init_state;
        YakinamashiChunk::YakinamashiChunkState<MAX_N> best_state = init_state;

        int current_score = evaluate(current_state);
        best_score = current_score;
        out("start_score", current_score);

        int start_scores_deep = evaluate<500, 2>(current_state);
        out("start_state's 3000, 2 score", start_scores_deep);

        // 時間制測開始
        double total_time = 1000.0;  // デフォルト1秒（ms単位）

        YakinamashiChunk::TemperatureSchedule schedule(init_temp, final_temp, total_time);
        YakinamashiChunk::NHSelector<MAX_N> selector(current_state, state_N);
        out("start_st");
        YakinamashiChunk::print_st(current_state);
        int loop_all = 1000;

        for (int _ = 0; _ < loop_all; _++) {
            // 焼きなまし主ループ
//        while (true) {


            time_keeper.setNowTime();
            double elapsed = time_keeper.getNowTime();

            // 時間制限チェック
//            if (time_keeper.isTimeOver()) {
//                break;
//            }

            // 進捗度合いを計算 [0.0, 1.0]
//            double progress = elapsed / total_time;
            double progress = (double) _ / (loop_all - 1);

            double temperature = schedule.get_temperature(progress);

            // 近傍を選択・生成
            YakinamashiChunk::YakinamashiChunkState<MAX_N> new_state = selector.select_and_generate(progress);
            cout << _ << endl;
            out("new_st");
            YakinamashiChunk::print_st(new_state);
            int new_score = evaluate(new_state);

            // 受理判定
            int score_diff = new_score - current_score;
            double accept_prob = (score_diff < 0) ? 1.0 : exp(-score_diff / temperature);
            bool accept = (rand() / (double) RAND_MAX) < accept_prob;

            if (accept) {
//            if ((score_diff < 0)) {
                out("accept");
                current_state = new_state;
                current_score = new_score;

                if (new_score < best_score) {
                    best_state = new_state;
                    best_score = new_score;
                }
            }
            out("new_score", new_score);
        }
        int best_score2 = evaluate<500, 2>(best_state);
        out("best_state's 3000, 2 score", best_score2);

        return {best_state, best_score};
    }
};

#endif //UNTITLED_CHUNKYAKINAMASHI_H
