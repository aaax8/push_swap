// ChunkAndBeamSolver.h
// チャンク分割＋ビーム探索による初期解生成クラス
//x
// チャンク分割→コマンド生成→ビーム探索→query列・cmd列生成までを一括で行う
// MAX_N, HABA, Kなどはテンプレート引数で指定
// 使い方: ChunkAndBeamSolver<MAX_N, HABA, K>::solve(...)

#pragma once
#include "State.h"
#include "sort_algorithm/YakinamashiChunk/YakinamashiChunkState.h"
#include "sort_algorithm/YakinamashiChunk/ChunkSeparator.h"
#include "sort_algorithm/InitBeam/BeamSearcher.h"
#include "sort_algorithm/InitBeam/BeamQuery.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <vector>
#include <utility>
#include <fstream>

template<size_t MAX_N, int HABA, int K>
class ChunkAndBeamSolver {
public:
    using ChunkState = YakinamashiChunk::YakinamashiChunkState<MAX_N>;
    using ChunkQuery = YakinamashiChunk::ChunkQuery;
    using Result = std::tuple<my_vector<command>, my_vector<BeamQuery>, my_vector<command>,
    my_bitset<MAX_N>, int
    >;

    // human_review_begin: push_swap CLI では command 以外を stdout に出さないようにするため。
    // 責務: lis_start 候補ログの出力可否を specialization ごとに保持する。
    // 必要な理由: tester 実行時だけ selected_lis_starts の stdout 出力を止めるため。
    static bool &lis_start_log_enabled() {
        static bool enabled = true;
        return enabled;
    }
    // human_review_end

    // human_review_begin: push_swap CLI では command 以外を stdout に出さないようにするため。
    // 責務: lis_start 候補ログの出力可否を切り替える。
    // 必要な理由: 単一入力 solver から selected_lis_starts の stdout 出力を止めるため。
    static void set_lis_start_log_enabled(bool enabled) {
        lis_start_log_enabled() = enabled;
    }
    // human_review_end

    // 責務: A 側値列と LIS 本体の値集合を一緒に保持して set 比較可能にする。
    // 必要な理由: 同じ a_values でも LIS/swap 内訳が違う候補を誤って skip しないため。
    struct LisKey {
        std::vector<std::vector<int>> a_values;
        std::vector<int> lis_values;

        bool operator<(const LisKey& other) const {
            if (a_values != other.a_values) return a_values < other.a_values;
            return lis_values < other.lis_values;
        }
    };

    // 責務: chunk state の A 側値列と LIS 本体の値集合を std::set で比較できるキーへ変換する。
    // 必要な理由: 同じ a_values でも LIS/swap 内訳が違う候補を別候補として beam search へ渡すため。
    static LisKey build_lis_key(const ChunkState& chunk_st) {
        LisKey key;
        key.a_values.reserve(chunk_st.val_state.a_values.size());
        for (const auto& values: chunk_st.val_state.a_values) {
            key.a_values.emplace_back(values.begin(), values.end());
        }
        for (int v = 0; v < static_cast<int>(MAX_N); v++) {
            if (chunk_st.val_state.is_a_lis[v]) key.lis_values.push_back(v);
        }
        return key;
    }

    // 責務: chunk 分割済み状態から command 生成、beam search、command 復元を実行して Result を返す。
    // 必要な理由: select_lis_range_start が重複判定後の chunk state を再利用して beam search へ渡すため。
    static Result solve_lis_chunk(const State& initial_state, ChunkState chunk_st,
                                  int state_N, int lis_start, bool log_lis_detail = true) {
        auto write_lis_values = [](std::ofstream &log_file, const my_bitset<MAX_N> &is_a_lis, int n) {
            log_file << "[";
            bool first = true;
            for (int v = 0; v < n; v++) {
                if (!is_a_lis[v]) continue;
                if (!first) log_file << ",";
                first = false;
                log_file << v;
            }
            log_file << "]";
        };

        auto write_values = [](std::ofstream &log_file, const my_vector<int> &values) {
            log_file << "[";
            for (int i = 0; i < static_cast<int>(values.size()); i++) {
                if (i > 0) log_file << ",";
                log_file << values[i];
            }
            log_file << "]";
        };

        my_assert(0 <= lis_start && lis_start < state_N);
        if (log_lis_detail) {
            ensure_output3_open();
        }
        std::ofstream *log_file = log_lis_detail ? &output3_stream() : nullptr;

        YakinamashiChunk::ChunkCommandGenerator<MAX_N> generator(chunk_st.tree, chunk_st.constraint, chunk_st.queries);
        auto [sep_commands, separated_state] = generator.generate_commands_constraint(initial_state);

        if (log_file != nullptr) {
            *log_file << "lis_start=" << lis_start << "\n";
            for (int i = 0; i < static_cast<int>(chunk_st.val_state.a_values.size()); i++) {
                *log_file << "A idx=" << i
                          << " label=" << chunk_st.tree.final_a_chunks[i]->label.to_string()
                          << " range=" << ChunkSeparator::to_string(chunk_st.val_state.a_ranges[i])
                          << " vals=";
                write_values(*log_file, chunk_st.val_state.a_values[i]);
                *log_file
                          << " lis_vals=";
                write_lis_values(*log_file, chunk_st.val_state.is_a_lis, state_N);
                *log_file << "\n";
            }
            for (int i = 0; i < static_cast<int>(chunk_st.val_state.b_values.size()); i++) {
                *log_file << "B idx=" << i
                          << " label=" << chunk_st.tree.final_b_chunks[i]->label.to_string()
                          << " range=" << ChunkSeparator::to_string(chunk_st.val_state.b_ranges[i])
                          << " vals=";
                write_values(*log_file, chunk_st.val_state.b_values[i]);
                *log_file << "\n";
            }
            *log_file << "score sep=" << sep_commands.size() << "\n";
            *log_file << std::flush;
        }

        BeamSearcher<HABA, K, MAX_N> searcher;
        auto beam_result = searcher.search_finished_a2b(separated_state, chunk_st.val_state.is_a_lis, chunk_st.tree);
        auto [beam_queries, beam_cmds] = searcher.reconstruct_commands(separated_state, beam_result);

        my_vector<command> all_cmds = sep_commands;
        all_cmds.insert(all_cmds.end(), beam_cmds.begin(), beam_cmds.end());

        if (log_file != nullptr) {
            *log_file << "score beam=" << beam_cmds.size()
                      << " total=" << all_cmds.size() << "\n";
            *log_file << std::flush;
        }

        return {std::move(sep_commands), std::move(beam_queries), std::move(all_cmds),
                std::move(chunk_st.val_state.is_a_lis), lis_start};
    }

    // 責務: 指定 lis_start だけで chunk 分割し、beam search 以降を solve_lis_chunk に委譲する。
    // 必要な理由: lis_start 候補選択後に、選ばれた開始位置だけを本番 HABA で作り直すため。
    static Result solve_lis_start(const State& initial_state, const my_vector<ChunkQuery>& queries, int state_N,
                                  int lis_start, bool log_lis_detail = true) {
        ChunkState chunk_st = YakinamashiChunk::ChunkStateFactory::build_initial_state<MAX_N>(
                initial_state, queries, state_N, lis_start);
        return solve_lis_chunk(initial_state, std::move(chunk_st), state_N, lis_start, log_lis_detail);
    }

    // 責務: 0..lis_range と state_N-lis_range..state_N-1 の候補から all_cmds.size() 最小の Result を返す。
    // 必要な理由: 本番幅結果より狭幅結果が良い場合に、狭幅結果そのものを初期解へ採用するため。
    static Result select_lis_range_start(const State& initial_state, const my_vector<ChunkQuery>& queries, int state_N,
                                         int lis_range, bool log_lis_detail = false) {
        my_assert(0 < state_N);
        my_assert(0 <= lis_range);
        const int front_end = std::min(lis_range, state_N - 1);
        const int back_begin = std::max(front_end + 1, state_N - lis_range);
        std::set<LisKey> seen_lis;
        std::vector<int> selected_lis_starts;
        int best_score = -1;
        Result best_result;
        auto try_lis_start = [&](int lis_start) {
            ChunkState chunk_st = YakinamashiChunk::ChunkStateFactory::build_initial_state<MAX_N>(
                    initial_state, queries, state_N, lis_start);
            LisKey key = build_lis_key(chunk_st);
            if (!seen_lis.insert(std::move(key)).second) return;
            selected_lis_starts.push_back(lis_start);
            Result result = solve_lis_chunk(
                    initial_state, std::move(chunk_st), state_N, lis_start, log_lis_detail);
            const int score = static_cast<int>(std::get<2>(result).size());
            if (best_score < 0 || score < best_score) {
                best_score = score;
                best_result = std::move(result);
            }
        };
        for (int lis_start = 0; lis_start <= front_end; lis_start++) {
            try_lis_start(lis_start);
        }
        for (int lis_start = back_begin; lis_start < state_N; lis_start++) {
            try_lis_start(lis_start);
        }
        my_assert(best_score >= 0);
        // human_review_begin: 通常実験では候補ログを保ち、push_swap CLI では stdout 汚染を止めるため。
        if (lis_start_log_enabled()) {
            std::cout << "selected_lis_starts=[";
            for (int i = 0; i < static_cast<int>(selected_lis_starts.size()); i++) {
                if (i > 0) std::cout << ",";
                std::cout << selected_lis_starts[i];
            }
            std::cout << "]" << std::endl;
        }
        // human_review_end
        return best_result;
    }

    // 責務: 0..state_N-1 の候補から、同じ LIS/swap 内訳を skip しつつ all_cmds.size() 最小の lis_start を返す。
    // 必要な理由: lis_range で端だけに絞らず、全 lis_start を幅 10 条件で比較するため。
    static int select_lis_start(const State& initial_state, const my_vector<ChunkQuery>& queries, int state_N,
                                bool log_lis_detail = false) {
        my_assert(0 < state_N);
        std::set<LisKey> seen_lis;
        std::vector<int> selected_lis_starts;
        int best_lis_start = 0;
        int best_score = -1;
        for (int lis_start = 0; lis_start < state_N; lis_start++) {
            ChunkState chunk_st = YakinamashiChunk::ChunkStateFactory::build_initial_state<MAX_N>(
                    initial_state, queries, state_N, lis_start);
            LisKey key = build_lis_key(chunk_st);
            if (!seen_lis.insert(std::move(key)).second) continue;
            selected_lis_starts.push_back(lis_start);
            Result result = solve_lis_chunk(
                    initial_state, std::move(chunk_st), state_N, lis_start, log_lis_detail);
            const int score = static_cast<int>(std::get<2>(result).size());
            if (best_score < 0 || score < best_score) {
                best_score = score;
                best_lis_start = lis_start;
            }
        }
        my_assert(best_score >= 0);
        // human_review_begin: 通常実験では候補ログを保ち、push_swap CLI では stdout 汚染を止めるため。
        if (lis_start_log_enabled()) {
            std::cout << "selected_lis_starts=[";
            for (int i = 0; i < static_cast<int>(selected_lis_starts.size()); i++) {
                if (i > 0) std::cout << ",";
                std::cout << selected_lis_starts[i];
            }
            std::cout << "]" << std::endl;
        }
        // human_review_end
        return best_lis_start;
    }

    static Result solve(const State& initial_state, const my_vector<ChunkQuery>& queries, int state_N,
                        bool try_all_lis_start = true, bool log_lis_detail = true) {
        struct FinishedA2BCandidate {
            ChunkState chunk_st;
            my_vector<command> sep_commands;
            State separated_state;
            my_vector<BeamQuery> beam_queries;
            my_vector<command> beam_cmds;
            my_vector<command> all_cmds;
            int lis_start;
        };

        auto write_lis_values = [](std::ofstream &log_file, const my_bitset<MAX_N> &is_a_lis, int n) {
            log_file << "[";
            bool first = true;
            for (int v = 0; v < n; v++) {
                if (!is_a_lis[v]) continue;
                if (!first) log_file << ",";
                first = false;
                log_file << v;
            }
            log_file << "]";
        };

        auto write_values = [](std::ofstream &log_file, const my_vector<int> &values) {
            log_file << "[";
            for (int i = 0; i < static_cast<int>(values.size()); i++) {
                if (i > 0) log_file << ",";
                log_file << values[i];
            }
            log_file << "]";
        };

        my_vector<FinishedA2BCandidate> candidates;
        if (log_lis_detail) {
            ensure_output3_open();
        }
        std::ofstream *log_file = log_lis_detail ? &output3_stream() : nullptr;

        int lis_start_count = try_all_lis_start ? state_N : 1;
        for (int lis_start = 0; lis_start < lis_start_count; lis_start++) {
            if( 20 < lis_start && lis_start < 480)continue;

            // human_review_begin: 通常実験では候補ログを保ち、push_swap CLI では stdout 汚染を止めるため。
            if (lis_start_log_enabled()) {
                cout << "lis_start" << lis_start << endl;
            }
            // human_review_end
            ChunkState chunk_st = YakinamashiChunk::ChunkStateFactory::build_initial_state<MAX_N>(
                    initial_state, queries, state_N, lis_start);

            YakinamashiChunk::ChunkCommandGenerator<MAX_N> generator(chunk_st.tree, chunk_st.constraint, chunk_st.queries);
            auto [sep_commands, separated_state] = generator.generate_commands_constraint(initial_state);

            if (log_file != nullptr) {
                *log_file << "lis_start=" << lis_start << "\n";
                for (int i = 0; i < static_cast<int>(chunk_st.val_state.a_values.size()); i++) {
                    *log_file << "A idx=" << i
                              << " label=" << chunk_st.tree.final_a_chunks[i]->label.to_string()
                              << " range=" << ChunkSeparator::to_string(chunk_st.val_state.a_ranges[i])
                              << " vals=";
                    write_values(*log_file, chunk_st.val_state.a_values[i]);
                    *log_file
                              << " lis_vals=";
                    write_lis_values(*log_file, chunk_st.val_state.is_a_lis, state_N);
                    *log_file << "\n";
                }
                for (int i = 0; i < static_cast<int>(chunk_st.val_state.b_values.size()); i++) {
                    *log_file << "B idx=" << i
                              << " label=" << chunk_st.tree.final_b_chunks[i]->label.to_string()
                              << " range=" << ChunkSeparator::to_string(chunk_st.val_state.b_ranges[i])
                              << " vals=";
                    write_values(*log_file, chunk_st.val_state.b_values[i]);
                    *log_file << "\n";
                }
                *log_file << "score sep=" << sep_commands.size() << "\n";
                *log_file << std::flush;
            }

            BeamSearcher<HABA, K, MAX_N> searcher;
            auto beam_result = searcher.search_finished_a2b(separated_state, chunk_st.val_state.is_a_lis, chunk_st.tree);
            auto [beam_queries, beam_cmds] = searcher.reconstruct_commands(separated_state, beam_result);

            my_vector<command> all_cmds = sep_commands;
            all_cmds.insert(all_cmds.end(), beam_cmds.begin(), beam_cmds.end());

            if (log_file != nullptr) {
                *log_file << "score beam=" << beam_cmds.size()
                          << " total=" << all_cmds.size() << "\n";
                *log_file << std::flush;
            }

            candidates.push_back({chunk_st, sep_commands, separated_state, beam_queries, beam_cmds, all_cmds, lis_start});
        }
        my_assert(!candidates.empty());
        int best_idx = 0;
        for (int i = 1; i < static_cast<int>(candidates.size()); i++) {
            if (candidates[i].all_cmds.size() < candidates[best_idx].all_cmds.size()) {
                best_idx = i;
            }
        }

        const FinishedA2BCandidate &best = candidates[best_idx];
        return {best.sep_commands, best.beam_queries, best.all_cmds, best.chunk_st.val_state.is_a_lis, best.lis_start};
    }
};
