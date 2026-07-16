#pragma once
#include "SimpleYakinamashi.h"
#include "Destroy/FullQueryDestroyer.h"
#include "Destroy/RangeQueryDestroyer.h"
#include "Construction/BaseGreedyConstruction.h"
#include "Construction/EraseQueriesReBuilder.h"
#include "Construction/GreedyQueriesConstructor.h"
#include "Validation/YakinamashiStateValidator.h"
#include "Command/YstCommandsBuilder.h"
#include "YakinamashiState.h"
#include "State.h"
#include <algorithm>
#include <cmath>
#include <memory>

// 責務: IterativeGreedy が候補解を採用する方式を表す。
enum class IterativeSearchMode {
    lahc,
    hill_climb
};

// 責務: N を縮めてから段階的に v を戻す IterativeGreedy 入口の実験パラメータを保持する。
// 必要な理由: 既存 run の引数を増やさず、新しい実験入口だけで keep_count と restore 間隔を指定するため。
struct ProgressiveRestoreConfig {
    int keep_count = 60;
    int restore_interval = 100;
};

class IterativeGreedy {
public:
    template<size_t MAX_N>
    static void run(YakinamashiState<MAX_N>& yst,
                    const State& init_st,
                    int outer_loop_count,
                    int annealing_iter_per_loop,
                    double start_temperature,
                    double end_temperature,
                    IterativeSearchMode search_mode = IterativeSearchMode::lahc,
                    bool clear_outputs = true
                    ) {
        my_assert(outer_loop_count >= 0);
        (void)annealing_iter_per_loop;
        (void)start_temperature;
        (void)end_temperature;
        if (clear_outputs) {
            clear_file();
            clear_output2_file();
        }
        YakinamashiState<MAX_N> best_yst = yst;
        int best_score = yst.queries_state.sum_turn;
        my_vector<int> lahc_history;
        init_lahc_history(lahc_history, best_score);
        int lahc_idx = 0;
        int no_improve_count = 0;
        for (int loop = 0; loop < outer_loop_count; loop++) {
            run_lahc_iteration(loop, yst, init_st, best_yst, best_score,
                               lahc_history, lahc_idx, no_improve_count,
                               search_mode
                               );
            QueriesStateValidator<MAX_N>::validate_state(init_st, yst.queries_state);

        }
    }

    // 責務: 初期 A idx で keep_count 個を残す physical destroy 後、既存 IG loop を回しながら idx 昇順で v を復元する。
    // 必要な理由: 小さい state で探索してから段階的に元の N へ戻す progressive restore 実験を既存 run と分離して試すため。
    template<size_t MAX_N>
    static void run_progressive_restore(YakinamashiState<MAX_N>& yst,
                                        const State& init_st,
                                        int outer_loop_count,
                                        int annealing_iter_per_loop,
                                        double start_temperature,
                                        double end_temperature,
                                        ProgressiveRestoreConfig config = ProgressiveRestoreConfig(),
                                        IterativeSearchMode search_mode = IterativeSearchMode::lahc
                                        ) {
        my_assert(outer_loop_count >= 0);
        my_assert(0 < config.keep_count && config.keep_count <= init_st.current_N);
        my_assert(config.restore_interval > 0);
        clear_file();
        clear_output2_file();

        State current_st = init_st;
        my_vector<std::pair<int, int>> restore_vs =
                build_restore_vs<MAX_N>(init_st, yst, config.keep_count);
        PhysicalDestroyContext<MAX_N> destroy_context(current_st, yst);
        EraseQueriesReBuilder<MAX_N>::rebuild_by_order(destroy_context, restore_vs);
        EraseOrderSnapshot<MAX_N> restore_snapshot = destroy_context.erase_order_snapshot;
        validate_progress_state<MAX_N>(current_st, yst);

        int restore_idx = 0;
        GreedyConstructorParams params{10, 24, 24, 24, 24, 1 << 30, 25};
        GreedyQueriesConstructor<MAX_N> restore_constructor(init_st, params);
        while (restore_idx < static_cast<int>(restore_vs.size())) {
            cout << "progressive_run restore_idx " << restore_idx
                 << " current_N " << current_st.current_N
                 << " loop_count " << config.restore_interval << endl;
            run<MAX_N>(
                    yst, current_st, config.restore_interval,
                    annealing_iter_per_loop, start_temperature, end_temperature,
                    search_mode, false
            );
            validate_progress_state<MAX_N>(current_st, yst);
            cout << "progressive_restore restore_idx " << restore_idx
                 << " v " << restore_vs[restore_idx].first
                 << " before_N " << current_st.current_N << endl;
            restore_one_v(current_st, yst, restore_snapshot, restore_constructor, restore_vs[restore_idx]);
            cout << "progressive_restored restore_idx " << restore_idx
                 << " after_N " << current_st.current_N << endl;
            restore_idx++;
            validate_progress_state<MAX_N>(current_st, yst);
        }
        cout << "progressive_final_run current_N " << current_st.current_N
             << " loop_count " << outer_loop_count << endl;
        run<MAX_N>(
                yst, current_st, outer_loop_count,
                annealing_iter_per_loop, start_temperature, end_temperature,
                search_mode, false
        );
    }

private:
    static constexpr int LAHC_HISTORY_SIZE = 500;
    static constexpr int LAHC_WORSEN_LIMIT = 5;
    static constexpr int LAHC_RESTART_LIMIT = 300;

    // 責務: 候補生成から採用・restartまでを1反復単位に閉じる。
    template<size_t MAX_N>
    static void run_lahc_iteration(int loop,
                                   YakinamashiState<MAX_N>& yst,
                                   const State& init_st,
                                   YakinamashiState<MAX_N>& best_yst,
                                   int& best_score,
                                   my_vector<int>& lahc_history,
                                   int& lahc_idx,
                                   int& no_improve_count,
                                   IterativeSearchMode search_mode) {
        cout << loop << " best_sum "<< best_score << " current_sum " << yst.queries_state.sum_turn <<endl;
        YakinamashiState<MAX_N> cand_yst = build_candidate(loop, yst, init_st);
        apply_candidate(yst, cand_yst, best_score, lahc_history, lahc_idx, search_mode);
        update_best_state(loop, yst, init_st, best_yst, best_score, no_improve_count);
        restart_if_stalled(loop, yst, best_yst, best_score, lahc_history, lahc_idx, no_improve_count);
    }

    template<size_t MAX_N>
    static YakinamashiState<MAX_N> build_candidate(int loop,
                                                   const YakinamashiState<MAX_N>& yst,
                                                   const State& init_st) {
        YakinamashiState<MAX_N> cand_yst = yst;
        RangeQueryDestroyer<MAX_N> destroyer(init_st, pick_range_size(loop, yst.queries_state.queries.size()));
        std::unique_ptr<BaseGreedyConstruction<MAX_N>> constructor = create_constructor<MAX_N>(init_st);
        my_vector<std::pair<int, int>> destroyed_vs = destroyer.destroy(cand_yst);
        constructor->construct(cand_yst, destroyed_vs);
        return cand_yst;
    }

    // 責務: 候補状態を採用するか判定し、採用する場合は current state を更新する。
    template<size_t MAX_N>
    static void apply_candidate(YakinamashiState<MAX_N>& yst,
                                const YakinamashiState<MAX_N>& cand_yst,
                                int best_score,
                                my_vector<int>& lahc_history,
                                int& lahc_idx,
                                IterativeSearchMode search_mode) {
        const int current_score = yst.queries_state.sum_turn;
        const int cand_score = cand_yst.queries_state.sum_turn;
        if (accept_candidate(search_mode, cand_score, current_score, lahc_history[lahc_idx], best_score)) {
            yst = cand_yst;
        }
        update_lahc_history(lahc_history, lahc_idx, current_score);
    }

    template<size_t MAX_N>
    static void update_best_state(int loop,
                                  const YakinamashiState<MAX_N>& yst,
                                  const State& init_st,
                                  YakinamashiState<MAX_N>& best_yst,
                                  int& best_score,
                                  int& no_improve_count) {
        if (yst.queries_state.sum_turn < best_score) {
            best_yst = yst;
            best_score = yst.queries_state.sum_turn;
            no_improve_count = 0;
            out("loop", loop, yst.queries_state.sum_turn);
            out("loop_cmds", loop, YstCommandsBuilder::build_all_commands_from_state(init_st, yst.queries_state));
            return;
        }
        no_improve_count++;
    }

    template<size_t MAX_N>
    static void restart_if_stalled(int loop,
                                   YakinamashiState<MAX_N>& yst,
                                   const YakinamashiState<MAX_N>& best_yst,
                                   int best_score,
                                   my_vector<int>& lahc_history,
                                   int& lahc_idx,
                                   int& no_improve_count) {
        if (no_improve_count < LAHC_RESTART_LIMIT) return;
        yst = best_yst;
        reset_lahc_history(lahc_history, best_score);
        lahc_idx = 0;
        no_improve_count = 0;
        out("lahc_restart", loop, best_score);
    }

    static void init_lahc_history(my_vector<int>& lahc_history, int initial_score) {
        lahc_history.assign(LAHC_HISTORY_SIZE, initial_score);
    }

    static void reset_lahc_history(my_vector<int>& lahc_history, int best_score) {
        init_lahc_history(lahc_history, best_score);
    }

    static void update_lahc_history(my_vector<int>& lahc_history, int& lahc_idx, int current_score) {
        lahc_history[lahc_idx] = current_score;
        lahc_idx++;
        if (lahc_idx == LAHC_HISTORY_SIZE) {
            lahc_idx = 0;
        }
    }

    static bool accept_by_lahc(int cand_score, int current_score, int history_score, int best_score) {
        if (cand_score <= current_score) return true;
        if (cand_score <= history_score) return true;
        return cand_score <= best_score + LAHC_WORSEN_LIMIT;
    }

    // 責務: 指定された採用方式に応じて候補を採用するか返す。
    static bool accept_candidate(IterativeSearchMode search_mode,
                                 int cand_score,
                                 int current_score,
                                 int history_score,
                                 int best_score) {
        if (search_mode == IterativeSearchMode::lahc) {
            return accept_by_lahc(cand_score, current_score, history_score, best_score);
        }
        if (search_mode == IterativeSearchMode::hill_climb) {
            return accept_by_hill_climb(cand_score, current_score);
        }
        my_assert(false);
        return false;
    }

    // 責務: 候補 score が現在 score より改善しているか判定する。
    static bool accept_by_hill_climb(int cand_score, int current_score) {
        return cand_score < current_score;
    }

    static int pick_range_size(int loop, size_t query_count) {
        my_assert(query_count > 0);
        int selector = loop % 4;
        if (selector == 0) return 3;
        if (selector == 1) return 6;
        if (selector == 2) return 10;
        return std::max(1, (int)std::sqrt((double)query_count));
    }

    // 責務: round(i * n / keep_count) で選んだ idx を、重複時は近い未使用 idx へずらして返す。
    // 必要な理由: N 個中 K 個を均等な初期 A 位置で残す対象を一箇所で決めるため。
    static my_vector<int> pick_keep_indices(int n, int keep_count) {
        my_assert(0 < keep_count && keep_count <= n);
        my_vector<int> used(n, 0);
        my_vector<int> keep_indices;
        keep_indices.reserve(keep_count);
        for (int i = 0; i < keep_count; i++) {
            int idx = static_cast<int>(std::floor((double)i * (double)n / (double)keep_count + 0.5));
            if (idx >= n) idx = n - 1;
            idx = pick_nearest_unused_idx(idx, used);
            used[idx] = 1;
            keep_indices.push_back(idx);
        }
        std::sort(keep_indices.begin(), keep_indices.end());
        return keep_indices;
    }

    // 責務: target_idx に最も近い未使用 idx を返す。
    // 必要な理由: keep_count 個を必ず残し、physical destroy 対象を一意に決めるため。
    static int pick_nearest_unused_idx(int target_idx, const my_vector<int>& used) {
        const int n = static_cast<int>(used.size());
        for (int dist = 0; dist < n; dist++) {
            const int left = target_idx - dist;
            if (0 <= left && !used[left]) return left;
            const int right = target_idx + dist;
            if (right < n && !used[right]) return right;
        }
        my_assert(false);
        return target_idx;
    }

    // 責務: 均等 keep idx 以外の v を、初期 A idx 昇順の destroyed_vs 形式で返す。
    // 必要な理由: 初回 N 縮小と、その後の idx 小さい順復元で同じ v 列を使うため。
    template<size_t MAX_N>
    static my_vector<std::pair<int, int>> build_restore_vs(
            const State& init_st,
            const YakinamashiState<MAX_N>& yst,
            int keep_count
    ) {
        my_assert(init_st.B.empty());
        my_assert(init_st.A.size() == init_st.current_N);
        my_vector<int> keep_indices = pick_keep_indices(init_st.current_N, keep_count);
        my_vector<int> keep_by_idx(init_st.current_N, 0);
        for (int idx: keep_indices) keep_by_idx[idx] = 1;
        my_vector<int> cost_by_v(init_st.initial_N, 0);
        for (const auto& qri: yst.queries_state.queries) {
            cost_by_v[qri.get_tar_v()] += qri.get_ins_turn();
        }
        my_vector<std::pair<int, int>> restore_vs;
        restore_vs.reserve(init_st.current_N - keep_count);
        for (int idx = 0; idx < init_st.current_N; idx++) {
            if (keep_by_idx[idx]) continue;
            const int v = init_st.A[idx];
            restore_vs.push_back({v, cost_by_v[v]});
        }
        return restore_vs;
    }

    // 責務: 部分集合 state では query 整合だけを検証し、全 N 復元後だけ sort 完了検証まで行う。
    // 必要な理由: physical destroy 後の state は値が 0..current_N-1 とは限らず、通常の can_sort 検証前提と一致しないため。
    template<size_t MAX_N>
    static void validate_progress_state(const State& current_st,
                                        const YakinamashiState<MAX_N>& yst) {
        if (current_st.current_N == current_st.initial_N) {
            QueriesStateValidator<MAX_N>::validate_state(current_st, yst.queries_state);
            return;
        }
        QueriesStateValidator<MAX_N>::validate_sum_turn(yst.queries_state);
        QueriesStateValidator<MAX_N>::validate_query_order(current_st, yst.queries_state.queries);
        QueriesStateValidator<MAX_N>::validate_query_top_transfer(current_st, yst.queries_state.queries);
    }

    // 責務: 保存済み erase snapshot を使い、指定 v を範囲中央 AOrder 復元で current state へ戻す。
    // 必要な理由: 初回 physical destroy の対象集合を基準に、100 loop ごとに 1 個ずつ N を増やすため。
    template<size_t MAX_N>
    static void restore_one_v(State& current_st,
                              YakinamashiState<MAX_N>& yst,
                              const EraseOrderSnapshot<MAX_N>& restore_snapshot,
                              GreedyQueriesConstructor<MAX_N>& restore_constructor,
                              const std::pair<int, int>& restore_v) {
        PhysicalDestroyContext<MAX_N> restore_context(current_st, yst);
        restore_context.erase_order_snapshot = restore_snapshot;
        my_vector<std::pair<int, int>> one_v;
        one_v.push_back(restore_v);
        restore_constructor.construct_physical_middle(restore_context, one_v);
    }

    // : 毎回すべて破壊して、重い v から全再構築する方針に統一する。
    template<size_t MAX_N>
    static std::unique_ptr<BaseDestroy<MAX_N>> create_destroyer(const State& init_st) {
        return std::make_unique<FullQueryDestroyer<MAX_N>>(init_st);
    }

    template<size_t MAX_N>
    static std::unique_ptr<BaseGreedyConstruction<MAX_N>> create_constructor(const State& init_st) {
        GreedyConstructorParams params{10, 24, 24, 24, 24, 1 << 30, 25};
        return std::make_unique<GreedyQueriesConstructor<MAX_N>>(init_st, params);
    }
};
