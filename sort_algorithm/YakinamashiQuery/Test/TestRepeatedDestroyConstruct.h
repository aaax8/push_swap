// TestRepeatedDestroyConstruct.h
#pragma once

#include "all_include.h"
#include "State.h"
#include "YstFactory.h"
#include "YakinamashiState.h"
#include "Destroy/RangeQueryDestroyer.h"
#include "Destroy/RandomTarVRangeDestroyer.h"
#include "Destroy/HighImpactVTopMRandomDestroyer.h"
#include "Destroy/AOrderRangePlanBuilder.h"
#include "Construction/EraseQueriesReBuilder.h"
#include "Construction/GreedyQueriesConstructor.h"
#include "QueriesModifier.h"
#include "Query/QueryProvider.h"
#include "NH/NH_BAllOrdMove.h"
#include "Validation/YakinamashiStateValidator.h"
#include "Command/YstCommandsBuilder.h"
#include "Command/optimize/OptimizeRangeSearch.h"
#include <array>
#include <chrono>
#include <sstream>
#include <variant>

namespace {
    // 責務: 通常 constructor と worst-half 二段 constructor のどちらを使うかを切り替える。
    // 必要な理由: 全 arm へ新 constructor を自然に適用しつつ、false に戻すだけで既存経路へ戻せるようにするため。
    static constexpr bool USE_WORST_HALF_REBUILD_CONSTRUCTOR = false;

    // 責務: ALNS の候補受理方式を表す。
    // 必要な理由: LAHC/山登り/焼きなましを bool の組み合わせではなく 1 つの mode で排他的に切り替えるため。
    enum class AlnsAcceptMode {
        HillClimbing,
        Lahc,
        Annealing
    };

    // 責務: main 現行経路を含む repeated destroy/construct の受理方式を表す。
    // 必要な理由: 実験対象の受理方式を 1 箇所で変更できるようにするため。
    static constexpr AlnsAcceptMode ALNS_ACCEPT_MODE = AlnsAcceptMode::Lahc;

    // 責務: construct 前の destroy marker retry/mark/decay を使うか切り替える。
    // 必要な理由: ban/tabu 相当の候補回避を 1 箇所の定数変更で無効化できるようにするため。
    static constexpr bool USE_DESTROY_MARKER = false;

    // 責務: ALNS iter 後に BOrder AllOrd move 近傍を実行するかを切り替える。
    // 必要な理由: BMove 有無による 1 iterate あたりの改善差を同じ経路で比較するため。
    static constexpr bool USE_B_ALL_ORD_MOVE = false;

    // 責務: 1 回の ALNS destroy/construct 後に追加で試す BAllOrdMove 近傍回数を表す。
    // 必要な理由: 指定された 30 回の近傍探索を名前付き定数として扱うため。
    static constexpr int B_ALL_ORD_MOVE_TRIAL_COUNT = 31;

    // 責務: destroy marker の最大値を表す。
    // 必要な理由: 最近 construct まで試した v を固定値で marked にするため。
    static constexpr int ALNS_DESTROY_MARKER_MAX = 3;

    // 責務: construct 前に destroy 候補を引き直す回数を表す。
    // 必要な理由: 重い construct を行う前に marker 平均が低い v 群を選ぶため。
    static constexpr int ALNS_DESTROY_MARKER_RETRY = 8;

    // 責務: 比較実験中に B 側 all_ord_move 近傍を runtime で使うか保持する。
    // 必要な理由: 既存の compile-time 定数を変えず、同じ init_state/seed で BMove の有無だけを比較するため。
    static bool g_b_move_enabled = false;

    // 責務: 比較実験中に B 側 all_ord_move 近傍を使うか更新する。
    // 必要な理由: owner 通常 run と owner+BMove run を同じ binary 内で切り替えるため。
    void set_b_move_enabled(bool enabled) {
        g_b_move_enabled = enabled;
    }

    // 責務: compile-time 切替または runtime 比較 flag により BMove を使うか返す。
    // 必要な理由: 既存の USE_B_ALL_ORD_MOVE を残しつつ、一時比較だけ runtime で有効化するため。
    bool is_b_move_enabled() {
        return g_b_move_enabled;
    }

    // 責務: 比較実験中に AOrderRange arm を runtime で追加するか保持する。
    // 必要な理由: 既存 arm 構成を残し、同じ init_state/seed で AOrderRange arm 追加の有無だけを比較するため。
    static bool g_aorder_range_enabled = false;

    // 責務: 比較実験中に AOrderRange arm を追加するか更新する。
    // 必要な理由: owner 通常 run と owner+AOrderRange run を同じ binary 内で切り替えるため。
    void set_aorder_range_enabled(bool enabled) {
        g_aorder_range_enabled = enabled;
    }

    // 責務: runtime 比較 flag により AOrderRange arm を追加するか返す。
    // 必要な理由: 通常 arm set を変えず、一時比較だけ AOrderRange arm を有効化するため。
    bool is_aorder_range_enabled() {
        return g_aorder_range_enabled;
    }

    // 責務: 比較実験中に worst-half rebuild constructor を runtime で使うか保持する。
    // 必要な理由: 既存の compile-time 定数を変えず、同じ init_state/seed で通常 owner と worst-half 版を並べて走らせるため。
    static bool g_worst_half_enabled = false;

    // 責務: 比較実験中に worst-half rebuild constructor を使うか更新する。
    // 必要な理由: owner 通常 run と owner+worst-half run を同じ binary 内で切り替えるため。
    void set_worst_half_enabled(bool enabled) {
        g_worst_half_enabled = enabled;
    }

    // 責務: compile-time 切替または runtime 比較 flag により worst-half を使うか返す。
    // 必要な理由: 既存の USE_WORST_HALF_REBUILD_CONSTRUCTOR を残しつつ、一時比較だけ runtime で有効化するため。
    bool is_worst_half_enabled() {
        return g_worst_half_enabled;
    }

    // : 初期状態ログを output.txt に揃えて、小さい destroy の反復結果を他テストと比較しやすくするため。
    std::string repeated_state_to_string(const RingBuffer500 &stack) {
        std::stringstream ss;
        for (int i = 0; i < stack.size(); i++) {
            if (i != 0) ss << ", ";
            ss << stack[i];
        }
        return ss.str();
    }

    // : 同じ permutation から複数 destroy size を比較できるように、State 化を helper に寄せるため。
    State build_repeated_test_state(const std::vector<int> &perm) {
        return State(deque<int>(perm.begin(), perm.end()), deque<int>());
    }

    // : ランダムケースでも既存の my_rand を使い、過去テストと再現性の前提を揃えるため。
    std::vector<int> build_repeated_test_perm(int n) {
        std::vector<int> perm(n);
        std::iota(perm.begin(), perm.end(), 0);
        for (int i = n - 1; i > 0; i--) {
            int j = my_rand(i + 1);
            std::swap(perm[i], perm[j]);
        }
        return perm;
    }

    // : score の定義を既存比較と合わせて、destroy size だけの違いを見やすくするため。
    int get_repeated_test_score(const QueriesState &queries_state) {
        return queries_state.sum_turn;
    }

    // : each iterate の復元コマンド列を 1 行で比較できるようにするため。
    std::string repeated_cmds_to_string(const my_vector<command> &cmds) {
        std::stringstream ss;
        for (int i = 0; i < static_cast<int>(cmds.size()); i++) {
            if (i != 0) ss << ", ";
            ss << cmd_str[(int) cmds[i]];
        }
        return ss.str();
    }

    // 責務: init_state と ALNS IterativeGreedy 後の command 列を OutPut/iterative_greedy_result.txt に出力する。
    // 必要な理由: 現在 main から実際に呼ばれる経路の入力と最終解を output.txt の調査ログから分離して確認できるようにするため。
    template<size_t MAX_N>
    void write_alns_result(const State &init_state, const YakinamashiState<MAX_N> &yst) {
        std::filesystem::create_directories(output_dir);
        const std::string file_path = (output_dir / "iterative_greedy_result.txt").string();
        my_vector<command> cmds = YstCommandsBuilder::build_all_commands_from_state(init_state, yst.queries_state);
        out_file(file_path, "init_current_N", init_state.current_N);
        out_file(file_path, "init_A", repeated_state_to_string(init_state.A));
        out_file(file_path, "init_B", repeated_state_to_string(init_state.B));
        out_file(file_path, "final_cmd_count", static_cast<int>(cmds.size()));
        out_file(file_path, "commands", repeated_cmds_to_string(cmds));
    }

    // 責務: AOrder entry 列を TARGET は 0 埋め 3 桁、INIT(1) は i01、その他 INIT は ___ として文字列化する。
    // 必要な理由: output3 で AOrder target の位置変化と INIT(1) の位置を追えるようにするため。
    std::string aorder_code_string(const AOrder &aorder) {
        std::stringstream ss;
        const my_vector<OrderEntry> &entries = aorder.get_order_entries();
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            if (i != 0) ss << ", ";
            if (entries[i].val_state == ValState::TARGET) {
                ss << std::setw(3) << std::setfill('0') << entries[i].value;
            } else if (entries[i].val_state == ValState::INIT && entries[i].value == 1) {
                ss << "i01";
            } else {
                ss << "___";
            }
        }
        return ss.str();
    }

    // 責務: BOrder entry 列の value を現在順のまま 0 埋め 3 桁で文字列化する。
    // 必要な理由: border には min-front を使わず、現在の B 側 order の v 列をそのまま追うため。
    std::string border_code_string(const BOrder &border) {
        std::stringstream ss;
        const my_vector<OrderEntry> &entries = border.get_order_entries();
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            if (i != 0) ss << ", ";
            ss << std::setw(3) << std::setfill('0') << entries[i].value;
        }
        return ss.str();
    }

    // : score と一緒に復元コマンド列も見て、destroy size の影響を人手で追えるようにするため。
    my_vector<command> build_reconstructed_cmds(const State &init_st, const QueriesState &queries_state) {
        return YstCommandsBuilder::build_all_commands_from_state(init_st, queries_state);
    }

    void print_alns_initial_state_to_output3(const State &init_state, int initial_score) {
        clear_output3_file();
        out3("init_st.current_N =", init_state.current_N);
        out3("init_st.A =", repeated_state_to_string(init_state.A));
        out3("init_st.B =", repeated_state_to_string(init_state.B));
        out3("initial_score =", initial_score);
    }

    template<size_t MAX_N>
    void print_alns_best_update_to_output3(
            int iter,
            const State &init_state,
            const YakinamashiState<MAX_N> &best_yst,
            int best_score
    ) {
        my_vector<command> cmds = build_reconstructed_cmds(init_state, best_yst.queries_state);
        out3("best_update iter =", iter,
             "score =", best_score,
             "cmd_count =", static_cast<int>(cmds.size()));
        out3("commands =", repeated_cmds_to_string(cmds));
        AOrder aorder_copy = best_yst.query_order.aorder;
        aorder_copy.normalize_min_front();
        out3("aorder_code:", aorder_code_string(aorder_copy));
        out3("border_code:", border_code_string(best_yst.query_order.border));
    }

    // 責務: owner_array_unit_500_best_cmds.txt に case と init_state を 1 回だけ書く。
    // 必要な理由: best 更新 command 列を読む前提となる入力だけを同じ専用ファイルで確認するため。
    void write_owner_case_init_log(
            std::ofstream &log_file,
            int case_idx,
            const State &init_state
    ) {
        log_file << "case=" << case_idx << '\n';
        log_file << "init_current_N=" << init_state.current_N << '\n';
        log_file << "init_A=" << repeated_state_to_string(init_state.A) << '\n';
        log_file << "init_B=" << repeated_state_to_string(init_state.B) << '\n';
    }

    // 責務: best 更新 iter、score、復元 command 列を 1 行で書く。
    // 必要な理由: ALNS の best 更新タイミングだけを専用ファイルで追えるようにするため。
    template<size_t MAX_N>
    void write_owner_best_cmd_log(
            std::ofstream &log_file,
            int iter,
            const State &init_state,
            const YakinamashiState<MAX_N> &best_yst,
            int best_score
    ) {
        my_vector<command> cmds = build_reconstructed_cmds(init_state, best_yst.queries_state);
        log_file << "iter" << iter
                 << ", score=" << best_score
                 << ", cmd=" << repeated_cmds_to_string(cmds)
                 << '\n';
    }

    // 責務: init_state に YakinamashiState から復元した command 列を適用し、cmd_list 付き State を返す。
    // 必要な理由: optimize_all_range は State の cmd_list を直接置換するため、ALNS 結果 command を State へ反映する必要があるため。
    template<size_t MAX_N>
    State build_owner_optimized_state(
            const State &init_state,
            const YakinamashiState<MAX_N> &yst
    ) {
        State optimized_state = init_state;
        my_vector<command> cmds = build_reconstructed_cmds(init_state, yst.queries_state);
        for (command cmd: cmds) {
            optimized_state.do_cmd(cmd);
        }
        return optimized_state;
    }

    // human_review_begin: 単一入力 solver から後続定義の ALNS 実装を呼ぶため。
    template<size_t MAX_N, int HABA, int K>
    int run_alns_destroy_construct(
            const State &init_state,
            YakinamashiState<MAX_N> &yst,
            int iter_count,
            AlnsAcceptMode accept_mode,
            const GreedyConstructorParams &params,
            int time_limit_sec,
            bool show_alns_log = false,
            bool show_alns_file_log = false,
            std::ofstream *owner_best_cmd_log = nullptr
    );
    // human_review_end

    // human_review_begin: nafuka tester 用に、N=100 の単一 argv 入力へ既存 owner_array_unit_100 と同じ処理を適用するため。
    // 責務: 100件入力の State に既存 run_owner_array_unit_100 相当の初期解、ALNS、range optimize を適用した State を返す。
    // 必要な理由: tester 形式では case loop ではなく 1 入力ごとに command 列だけを返す必要があるため。
    template<size_t MAX_N, int HABA, int K>
    State solve_owner_unit_100(const State &init_state) {
        constexpr int ALNS_SEED = 2026041451;
        constexpr int FIRST_ITER_COUNT = 200;
        constexpr int EXTRA_ITER_COUNT = 300;
        constexpr int LIS_RANGE = 20;
        constexpr int NARROW_HABA = 500;
        constexpr int OPT_RANGE_SIZE = 9;
        constexpr int OPT_LEFT = 0;
        constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
        constexpr int NO_TIME_LIMIT_SEC = -1;
        GreedyConstructorParams params{10, 5, 5, 5, 5, 20, 24, 3};

        GreedyQueriesConstructor<MAX_N>::set_timing_log_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_array_enabled(true);
        GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(true);
        GreedyQueriesConstructor<MAX_N>::set_owner_even_unit_enabled(true);
        GreedyQueriesConstructor<MAX_N>::set_pair_refine_enabled(true);
        GreedyQueriesConstructor<MAX_N>::set_pair_refine_params(6, 1, 1, 3);
        set_worst_half_enabled(false);
        set_b_move_enabled(false);
        set_aorder_range_enabled(false);
        set_my_rand_seed(static_cast<uint32_t>(ALNS_SEED));
        ChunkAndBeamSolver<MAX_N, NARROW_HABA, K>::set_lis_start_log_enabled(false);

        YstFactory<MAX_N, HABA, K> factory;
        auto build_result = factory.template build_lis_range_score<NARROW_HABA>(init_state, LIS_RANGE, false);
        YakinamashiState<MAX_N> current_yst = std::move(build_result.yst);
        //

        YakinamashiState<MAX_N> first_yst = current_yst;
        YakinamashiState<MAX_N> second_yst = current_yst;
        const int first_score = run_alns_destroy_construct<MAX_N, HABA, K>(
                init_state,
                first_yst,
                FIRST_ITER_COUNT,
                ACCEPT_MODE,
                params,
                NO_TIME_LIMIT_SEC,
                false,
                false,
                nullptr);
        const int second_score = run_alns_destroy_construct<MAX_N, HABA, K>(
                init_state,
                second_yst,
                FIRST_ITER_COUNT,
                ACCEPT_MODE,
                params,
                NO_TIME_LIMIT_SEC,
                false,
                false,
                nullptr);
        YakinamashiState<MAX_N> best_yst = (first_score <= second_score)
                                           ? std::move(first_yst)
                                           : std::move(second_yst);
        const int extra_score = run_alns_destroy_construct<MAX_N, HABA, K>(
                init_state,
                best_yst,
                EXTRA_ITER_COUNT,
                ACCEPT_MODE,
                params,
                NO_TIME_LIMIT_SEC,
                false,
                false,
                nullptr);
        (void) extra_score;
        State optimized_state = build_owner_optimized_state(init_state, best_yst);
        optimize_all_range(optimized_state, OPT_RANGE_SIZE, OPT_LEFT);
        return optimized_state;
    }
    // human_review_end

    // human_review_begin: nafuka tester 用に、N=500 の単一 argv 入力へ既存 owner_array_unit_500 と同じ処理を適用するため。
    // 責務: 500件入力の State に既存 run_owner_array_unit_500 相当の初期解、ALNS、range optimize を適用した State を返す。
    // 必要な理由: tester 形式では case loop ではなく 1 入力ごとに command 列だけを返す必要があるため。
    template<size_t MAX_N, int HABA, int K>
    State solve_owner_unit_500(const State &init_state) {
        constexpr int ALNS_SEED = 2026041451;
        constexpr int ITER_COUNT = 150;
        constexpr int EXTRA_ITER_COUNT = 50;
        constexpr int EXTRA_SCORE_THRESHOLD = 3000;
        constexpr int LIS_RANGE = 50;
        constexpr int NARROW_HABA = 200;
        constexpr int OPT_RANGE_SIZE = 7;
        constexpr int OPT_LEFT = 0;
        constexpr AlnsAcceptMode ACCEPT_MODE = AlnsAcceptMode::HillClimbing;
        constexpr int NO_TIME_LIMIT_SEC = -1;
        GreedyConstructorParams params{10, 4, 4, 4, 4, 25, 24, 3};

        GreedyQueriesConstructor<MAX_N>::set_timing_log_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_array_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_even_unit_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_pair_refine_enabled(false);
        set_worst_half_enabled(false);
        set_b_move_enabled(false);
        set_aorder_range_enabled(false);
        set_my_rand_seed(static_cast<uint32_t>(ALNS_SEED));
        ChunkAndBeamSolver<MAX_N, NARROW_HABA, K>::set_lis_start_log_enabled(false);

        YstFactory<MAX_N, HABA, K> factory;
        auto build_result = factory.template build_lis_range_score<NARROW_HABA>(init_state, LIS_RANGE, false);
        YakinamashiState<MAX_N> current_yst = std::move(build_result.yst);

        int alns_score = run_alns_destroy_construct<MAX_N, HABA, K>(
                init_state,
                current_yst,
                ITER_COUNT,
                ACCEPT_MODE,
                params,
                NO_TIME_LIMIT_SEC,
                false,
                false,
                nullptr);
        if (alns_score >= EXTRA_SCORE_THRESHOLD) {
            alns_score = run_alns_destroy_construct<MAX_N, HABA, K>(
                    init_state,
                    current_yst,
                    EXTRA_ITER_COUNT,
                    ACCEPT_MODE,
                    params,
                    NO_TIME_LIMIT_SEC,
                    false,
                    false,
                    nullptr);
        }
        (void) alns_score;
        State optimized_state = build_owner_optimized_state(init_state, current_yst);
        optimize_all_range(optimized_state, OPT_RANGE_SIZE, OPT_LEFT);
        return optimized_state;
    }
    // human_review_end

    // 責務: case ごとの ALNS score、OptimizeRangeSearch 後 score、最適化後 command 列を 1 行で書く。
    // 必要な理由: ALNS best 更新ログとは別に、case 最終結果として採用した range 最適化後 command を確認するため。
    void write_owner_optimized_cmd_log(
            std::ofstream &log_file,
            int case_idx,
            int alns_score,
            const State &optimized_state
    ) {
        const vector<command> &cmds = optimized_state.get_cmd_list().list;
        log_file << "case" << case_idx
                 << ", optimize_range_after_alns_score=" << alns_score
                 << ", optimized_score=" << static_cast<int>(cmds.size())
                 << ", cmd=";
        for (int i = 0; i < static_cast<int>(cmds.size()); i++) {
            if (i != 0) log_file << ", ";
            log_file << cmd_str[static_cast<int>(cmds[i])];
        }
        log_file << '\n';
    }

    // : trace 側でも iter ごとの destroyed_vs を見返せるようにする。
    std::string repeated_destroyed_vs_to_string(const my_vector<std::pair<int, int>> &destroyed_vs) {
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < static_cast<int>(destroyed_vs.size()); i++) {
            if (i != 0) ss << ", ";
            ss << "(" << destroyed_vs[i].first << ":" << destroyed_vs[i].second << ")";
        }
        ss << "]";
        return ss.str();
    }

    // : destroyer ログで選ばれた v 群を読みやすく出して、top-m random の結果を追いやすくするため。
    std::string repeated_ints_to_string(const my_vector<int> &values) {
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < (int) values.size(); i++) {
            if (i != 0) ss << ", ";
            ss << values[i];
        }
        ss << "]";
        return ss.str();
    }

    // : top-m 候補の v と decrease を並べて、destroyer が見ていた強い候補集合を出せるようにするため。
    std::string repeated_v_decreases_to_string(const my_vector<std::pair<int, int>> &values) {
        std::stringstream ss;
        ss << "[";
        for (int i = 0; i < (int) values.size(); i++) {
            if (i != 0) ss << ", ";
            ss << "(" << values[i].first << ":" << values[i].second << ")";
        }
        ss << "]";
        return ss.str();
    }

    // : output.txt を入口ログとして保つため、construct 内部統計を iter 文脈つきで短く転記できるようにする。
    // : output.txt でも pair 詳細を読めるようにし、trace を切った ALNS 実験でも同じ統計を比較できるようにする。
    template<size_t MAX_N>
    void print_construct_profile_lines_to_output(const std::string &header) {
        const my_vector<std::string> &lines = GreedyQueriesConstructor<MAX_N>::get_last_construct_output_lines();
        if (lines.empty()) return;
        out(header + " construct_profile_begin");
        for (const std::string &line: lines) out(header + " " + line);
        out(header + " construct_profile_end");
    }

    // : 後続 template helper から destroyer ログ関数を参照できるように前方宣言して可視性を揃える。
    template<size_t MAX_N>
    std::string repeated_destroyer_info_to_string(const RangeQueryDestroyer<MAX_N> &, int destroy_size);

    // : 後続 template helper から destroyer ログ関数を参照できるように前方宣言して可視性を揃える。
    template<size_t MAX_N>
    std::string repeated_destroyer_info_to_string(const RandomTarVRangeDestroyer<MAX_N> &destroyer, int destroy_size);

    // : 後続 template helper から destroyer ログ関数を参照できるように前方宣言して可視性を揃える。
    template<size_t MAX_N>
    std::string
    repeated_destroyer_info_to_string(const HighImpactVTopMRandomDestroyer<MAX_N> &destroyer, int destroy_size);

    // : range destroy を size 別 arm として学習させ、6/8/10/12 のどれが効くかを ALNS 側で分離できるようにする。
    // _BEGIN: arm 配列へ切り替えたので、旧 enum/getter ベース定義は一旦退避して source of truth を分散させない。
    constexpr double ALNS_DISCOUNT_GAMMA = 0.97;
    constexpr double ALNS_UCB_EXPLORATION_C = 1.2;
    constexpr double ALNS_UCB_EPS = 1e-9;
    constexpr int DEFAULT_ALNS_LAHC_HISTORY_SIZE = 30;
    constexpr int DEFAULT_ALNS_WORSEN_LIMIT = 10;
    constexpr int ALNS_RESTART_LIMIT = 30;
    constexpr int ALNS_ANNEALING_ITER_COUNT = 500;
    constexpr double ALNS_ANNEALING_START_TEMP = 10.0;
    constexpr double ALNS_ANNEALING_END_TEMP = 0.1;
    constexpr int ALNS_ACCEPT_RANDOM_SCALE = 1 << 30;
    constexpr double PHY_Q_PHYSICAL_RATIO = 0.5;
    constexpr int PHY_Q_RANDOM_SCALE = 1000000;

    // 責務: ALNS 受理方式で使う runtime 指定可能なパラメータを保持する。
    // 必要な理由: CLI option が無い場合は既存デフォルトを使い、指定がある場合だけ LAHC 挙動を変えるため。
    struct AlnsAcceptParams {
        int lahc_history_size = DEFAULT_ALNS_LAHC_HISTORY_SIZE;
        int worsen_limit = DEFAULT_ALNS_WORSEN_LIMIT;
    };

    // 責務: 現在の ALNS 受理パラメータを返す。
    // 必要な理由: main 通常実行と Optuna 実行の両方から同じ runtime 設定を参照するため。
    AlnsAcceptParams &get_alns_accept_params_ref() {
        static AlnsAcceptParams params;
        return params;
    }

    // 責務: 現在の ALNS 受理パラメータを読み取り用に返す。
    // 必要な理由: LAHC 判定や runtime 初期化で同じ設定値を使うため。
    const AlnsAcceptParams &get_alns_accept_params() {
        return get_alns_accept_params_ref();
    }

    // 責務: CLI から parse した ALNS 受理パラメータを保存する。
    // 必要な理由: main で読んだ option 値を ALNS 実行側へ渡すため。
    void set_alns_accept_params(const AlnsAcceptParams &params) {
        my_assert(0 < params.lahc_history_size);
        my_assert(0 <= params.worsen_limit);
        get_alns_accept_params_ref() = params;
    }

    template<size_t MAX_N>
    class PhysicalRangeDestroyer {
        RangeQueryDestroyer<MAX_N> query_destroyer;

    public:
        explicit PhysicalRangeDestroyer(const State &current_st, int range_size_override = -1)
                : query_destroyer(current_st, range_size_override) {}

        my_vector<std::pair<int, int>> pick_destroyed_vs(const YakinamashiState<MAX_N> &yst) {
            YakinamashiState<MAX_N> copied_yst = yst;
            return query_destroyer.destroy(copied_yst);
        }

        int get_range_size_override() const {
            return query_destroyer.get_range_size_override();
        }
    };

    template<size_t MAX_N>
    class PhysicalTarRangeDestroyer {
        RandomTarVRangeDestroyer<MAX_N> query_destroyer;

    public:
        explicit PhysicalTarRangeDestroyer(const State &current_st, int target_v_range_size)
                : query_destroyer(current_st, target_v_range_size) {}

        my_vector<std::pair<int, int>> pick_destroyed_vs(const YakinamashiState<MAX_N> &yst) {
            YakinamashiState<MAX_N> copied_yst = yst;
            return query_destroyer.destroy(copied_yst);
        }

        int get_target_v_range_size() const { return query_destroyer.get_target_v_range_size(); }

        int get_last_start_v() const { return query_destroyer.get_last_start_v(); }

        int get_last_end_v_exclusive() const { return query_destroyer.get_last_end_v_exclusive(); }
    };

    template<size_t MAX_N>
    class PhysicalImpactDestroyer {
        HighImpactVTopMRandomDestroyer<MAX_N> query_destroyer;

    public:
        explicit PhysicalImpactDestroyer(const State &current_st, int select_v_count, int candidate_top_m)
                : query_destroyer(current_st, select_v_count, candidate_top_m) {}

        my_vector<std::pair<int, int>> pick_destroyed_vs(const YakinamashiState<MAX_N> &yst) {
            YakinamashiState<MAX_N> copied_yst = yst;
            return query_destroyer.destroy(copied_yst);
        }

        int get_select_v_count() const { return query_destroyer.get_select_v_count(); }

        int get_candidate_top_m() const { return query_destroyer.get_candidate_top_m(); }

        const my_vector<int> &get_last_selected_vs() const { return query_destroyer.get_last_selected_vs(); }

        const my_vector<std::pair<int, int>> &get_last_top_candidates() const {
            return query_destroyer.get_last_top_candidates();
        }
    };

    // 責務: 元の query 列から 2 つの range を選び、target v が重ならない場合だけ union して一度に query erase する。
    // 必要な理由: RangeQueryDestroyer を逐次実行せず、2 区間分を同じ destroy 反映状態で construct に渡すため。
    template<size_t MAX_N>
    class CompositeRangeQueryDestroyer {
        const State &current_st;
        const int first_range_size;
        const int second_range_size;

    public:
        // 責務: 合成 range destroyer が使う 2 つの range size を保持する。
        // 必要な理由: ALNS arm 定義から range 5+5 のような合成設定を渡せるようにするため。
        explicit CompositeRangeQueryDestroyer(
                const State &current_st,
                int first_range_size,
                int second_range_size
        ) : current_st(current_st),
            first_range_size(first_range_size),
            second_range_size(second_range_size) {}

        // 責務: v が重ならない 2 range を選び、対象 v の query をまとめて消して cost 付き v 集合を返す。
        // 必要な理由: query erase を 1 回に保ったまま、2 つの range query destroy を 1 arm として試すため。
        my_vector<std::pair<int, int>> destroy(YakinamashiState<MAX_N> &yst) {
            my_assert(has_disjoint_range_pair(yst.queries_state));
            my_bitset<MAX_N> first_vs;
            my_bitset<MAX_N> second_vs;
            while (true) {
                first_vs = pick_range_vs(yst.queries_state, first_range_size);
                if (!has_disjoint_second_range(yst.queries_state, first_vs)) continue;
                second_vs = pick_disjoint_range_vs(yst.queries_state, second_range_size, first_vs);
                break;
            }
            my_bitset<MAX_N> combined_vs = first_vs;
            combined_vs |= second_vs;
            my_vector<int> tar_idxs;
            my_vector<std::pair<int, int>> destroyed_vs;
            collect_target_idxs_costs(yst.queries_state, combined_vs, tar_idxs, destroyed_vs);
            erase_tar_idxs(yst, tar_idxs);
            return destroyed_vs;
        }

        // 責務: 1 個目の range size を返す。
        // 必要な理由: ALNS ログへ合成 arm の設定を出すため。
        int get_first_range_size() const { return first_range_size; }

        // 責務: 2 個目の range size を返す。
        // 必要な理由: ALNS ログへ合成 arm の設定を出すため。
        int get_second_range_size() const { return second_range_size; }

    private:
        // 責務: query 数を超えない実 range size を返す。
        // 必要な理由: 既存 RangeQueryDestroyer と同じく、指定 size が query 数より大きい場合を丸めるため。
        int calc_range_size(const QueriesState &qs, int range_size) const {
            my_assert(0 < range_size);
            my_assert(!qs.queries.empty());
            return std::min(static_cast<int>(qs.queries.size()), range_size);
        }

        // 責務: 指定 start/range size から、その区間に含まれる target v の bitset を作る。
        // 必要な理由: 2 range の v 重複判定と最終 erase 対象作成で同じ target v 定義を使うため。
        my_bitset<MAX_N> build_range_vs(const QueriesState &qs, int range_size, int range_start) const {
            my_bitset<MAX_N> target_vs;
            const int actual_range_size = calc_range_size(qs, range_size);
            for (int i = range_start; i < range_start + actual_range_size; i++) {
                target_vs.set(qs.queries[i].get_tar_v());
            }
            return target_vs;
        }

        // 責務: query 列からランダムに 1 range を選び、その target v bitset を返す。
        // 必要な理由: 合成 arm 内でも既存 range destroyer と同じランダム range 選択を使うため。
        my_bitset<MAX_N> pick_range_vs(const QueriesState &qs, int range_size) const {
            const int actual_range_size = calc_range_size(qs, range_size);
            const int range_start = my_rand(static_cast<int>(qs.queries.size()) - actual_range_size + 1);
            return build_range_vs(qs, range_size, range_start);
        }

        // 責務: 2 つの target v bitset に共通 v があるか返す。
        // 必要な理由: 合成 range の target v 重複を禁止するため。
        bool has_overlap(const my_bitset<MAX_N> &lhs, const my_bitset<MAX_N> &rhs) const {
            for (int v = 0; v < current_st.initial_N; v++) {
                if (lhs[v] && rhs[v]) return true;
            }
            return false;
        }

        // 責務: first_vs と重ならない 2 個目の range が存在するか返す。
        // 必要な理由: 無制限再抽選の前に、不可能な選択だけを避けるため。
        bool has_disjoint_second_range(const QueriesState &qs, const my_bitset<MAX_N> &first_vs) const {
            const int actual_range_size = calc_range_size(qs, second_range_size);
            const int start_limit = static_cast<int>(qs.queries.size()) - actual_range_size;
            for (int range_start = 0; range_start <= start_limit; range_start++) {
                my_bitset<MAX_N> second_vs = build_range_vs(qs, second_range_size, range_start);
                if (!has_overlap(first_vs, second_vs)) return true;
            }
            return false;
        }

        // 責務: 現在の query 列に v 重複なしの 2 range 組が存在するか返す。
        // 必要な理由: 条件を満たせない場合は再抽選で止めず assert にするため。
        bool has_disjoint_range_pair(const QueriesState &qs) const {
            const int actual_range_size = calc_range_size(qs, first_range_size);
            const int start_limit = static_cast<int>(qs.queries.size()) - actual_range_size;
            for (int range_start = 0; range_start <= start_limit; range_start++) {
                my_bitset<MAX_N> first_vs = build_range_vs(qs, first_range_size, range_start);
                if (has_disjoint_second_range(qs, first_vs)) return true;
            }
            return false;
        }

        // 責務: used_vs と重ならない range が選ばれるまで 1 range を抽選する。
        // 必要な理由: ユーザー指定どおり、同じ v が選ばれた場合は上限なしで抽選し直すため。
        my_bitset<MAX_N> pick_disjoint_range_vs(
                const QueriesState &qs,
                int range_size,
                const my_bitset<MAX_N> &used_vs
        ) const {
            while (true) {
                my_bitset<MAX_N> target_vs = pick_range_vs(qs, range_size);
                if (!has_overlap(used_vs, target_vs)) return target_vs;
            }
        }

        // 責務: destroyed_vs に v の削除 cost を集約して加算する。
        // 必要な理由: 同じ v の複数 query を消す既存 RangeQueryDestroyer の返り値意味に揃えるため。
        void add_destroyed_v_cost(
                my_vector<std::pair<int, int>> &destroyed_vs,
                my_vector<int> &destroyed_vs_idxs,
                int v,
                int ins_turn
        ) const {
            if (destroyed_vs_idxs[v] < 0) {
                destroyed_vs_idxs[v] = static_cast<int>(destroyed_vs.size());
                destroyed_vs.push_back({v, 0});
            }
            destroyed_vs[destroyed_vs_idxs[v]].second += ins_turn;
        }

        // 責務: target v に対応する query index と、v ごとの ins_turn 合計を同時に集める。
        // 必要な理由: query erase 対象と constructor へ渡す destroyed_vs の source of truth を一致させるため。
        void collect_target_idxs_costs(
                const QueriesState &qs,
                const my_bitset<MAX_N> &target_vs,
                my_vector<int> &tar_idxs,
                my_vector<std::pair<int, int>> &destroyed_vs
        ) const {
            my_vector<int> destroyed_vs_idxs(current_st.initial_N, -1);
            for (int i = 0; i < static_cast<int>(qs.queries.size()); i++) {
                const QueryCommonRI &qri = qs.queries[i];
                const int v = qri.get_tar_v();
                if (!target_vs[v]) continue;
                tar_idxs.push_back(i);
                add_destroyed_v_cost(destroyed_vs, destroyed_vs_idxs, v, qri.get_ins_turn());
            }
        }

        // 責務: 集めた query index を QueriesModifier へ渡して query を一度だけ erase する。
        // 必要な理由: 2 range の対象を逐次 destroy ではなく同じ erase 操作で反映するため。
        void erase_tar_idxs(YakinamashiState<MAX_N> &yst, const my_vector<int> &tar_idxs) {
            QueryProvider provider(yst, current_st);
            QueriesModifier modifier(current_st, yst.query_order.aorder, yst.queries_state, provider);
            modifier.erase_queries<MAX_N>(tar_idxs);
        }
    };

    // 責務: 元の query 列から 2 つの range を選び、target v が重ならない場合だけ union した destroyed_vs を返す。
    // 必要な理由: 既存 physical destroy 経路を使いながら、2 区間分を同じ physical destroy 反映状態で construct に渡すため。
    template<size_t MAX_N>
    class CompositePhysicalRangeDestroyer {
        const State &current_st;
        const int first_range_size;
        const int second_range_size;

    public:
        // 責務: 合成 physical range destroyer が使う 2 つの range size を保持する。
        // 必要な理由: ALNS arm 定義から physical range 5+5 の設定を渡せるようにするため。
        explicit CompositePhysicalRangeDestroyer(
                const State &current_st,
                int first_range_size,
                int second_range_size
        ) : current_st(current_st),
            first_range_size(first_range_size),
            second_range_size(second_range_size) {}

        // 責務: v が重ならない 2 range を選び、対象 v の cost 付き union を返す。
        // 必要な理由: physical erase は既存 candidate 経路に任せ、合成 arm は対象選択だけを担当するため。
        my_vector<std::pair<int, int>> pick_destroyed_vs(const YakinamashiState<MAX_N> &yst) {
            my_assert(has_disjoint_range_pair(yst.queries_state));
            my_bitset<MAX_N> first_vs;
            my_bitset<MAX_N> second_vs;
            while (true) {
                first_vs = pick_range_vs(yst.queries_state, first_range_size);
                if (!has_disjoint_second_range(yst.queries_state, first_vs)) continue;
                second_vs = pick_disjoint_range_vs(yst.queries_state, second_range_size, first_vs);
                break;
            }
            my_bitset<MAX_N> combined_vs = first_vs;
            combined_vs |= second_vs;
            my_vector<std::pair<int, int>> destroyed_vs;
            collect_target_costs(yst.queries_state, combined_vs, destroyed_vs);
            return destroyed_vs;
        }

        // 責務: 1 個目の range size を返す。
        // 必要な理由: ALNS ログへ合成 arm の設定を出すため。
        int get_first_range_size() const { return first_range_size; }

        // 責務: 2 個目の range size を返す。
        // 必要な理由: ALNS ログへ合成 arm の設定を出すため。
        int get_second_range_size() const { return second_range_size; }

    private:
        // 責務: query 数を超えない実 range size を返す。
        // 必要な理由: 既存 RangeQueryDestroyer と同じく、指定 size が query 数より大きい場合を丸めるため。
        int calc_range_size(const QueriesState &qs, int range_size) const {
            my_assert(0 < range_size);
            my_assert(!qs.queries.empty());
            return std::min(static_cast<int>(qs.queries.size()), range_size);
        }

        // 責務: 指定 start/range size から、その区間に含まれる target v の bitset を作る。
        // 必要な理由: 2 range の v 重複判定と final physical erase 対象作成で同じ target v 定義を使うため。
        my_bitset<MAX_N> build_range_vs(const QueriesState &qs, int range_size, int range_start) const {
            my_bitset<MAX_N> target_vs;
            const int actual_range_size = calc_range_size(qs, range_size);
            for (int i = range_start; i < range_start + actual_range_size; i++) {
                target_vs.set(qs.queries[i].get_tar_v());
            }
            return target_vs;
        }

        // 責務: query 列からランダムに 1 range を選び、その target v bitset を返す。
        // 必要な理由: physical 合成 arm 内でも既存 range destroyer と同じランダム range 選択を使うため。
        my_bitset<MAX_N> pick_range_vs(const QueriesState &qs, int range_size) const {
            const int actual_range_size = calc_range_size(qs, range_size);
            const int range_start = my_rand(static_cast<int>(qs.queries.size()) - actual_range_size + 1);
            return build_range_vs(qs, range_size, range_start);
        }

        // 責務: 2 つの target v bitset に共通 v があるか返す。
        // 必要な理由: 合成 physical range の target v 重複を禁止するため。
        bool has_overlap(const my_bitset<MAX_N> &lhs, const my_bitset<MAX_N> &rhs) const {
            for (int v = 0; v < current_st.initial_N; v++) {
                if (lhs[v] && rhs[v]) return true;
            }
            return false;
        }

        // 責務: first_vs と重ならない 2 個目の range が存在するか返す。
        // 必要な理由: 無制限再抽選の前に、不可能な選択だけを避けるため。
        bool has_disjoint_second_range(const QueriesState &qs, const my_bitset<MAX_N> &first_vs) const {
            const int actual_range_size = calc_range_size(qs, second_range_size);
            const int start_limit = static_cast<int>(qs.queries.size()) - actual_range_size;
            for (int range_start = 0; range_start <= start_limit; range_start++) {
                my_bitset<MAX_N> second_vs = build_range_vs(qs, second_range_size, range_start);
                if (!has_overlap(first_vs, second_vs)) return true;
            }
            return false;
        }

        // 責務: 現在の query 列に v 重複なしの 2 range 組が存在するか返す。
        // 必要な理由: 条件を満たせない場合は再抽選で止めず assert にするため。
        bool has_disjoint_range_pair(const QueriesState &qs) const {
            const int actual_range_size = calc_range_size(qs, first_range_size);
            const int start_limit = static_cast<int>(qs.queries.size()) - actual_range_size;
            for (int range_start = 0; range_start <= start_limit; range_start++) {
                my_bitset<MAX_N> first_vs = build_range_vs(qs, first_range_size, range_start);
                if (has_disjoint_second_range(qs, first_vs)) return true;
            }
            return false;
        }

        // 責務: used_vs と重ならない range が選ばれるまで 1 range を抽選する。
        // 必要な理由: ユーザー指定どおり、同じ v が選ばれた場合は上限なしで抽選し直すため。
        my_bitset<MAX_N> pick_disjoint_range_vs(
                const QueriesState &qs,
                int range_size,
                const my_bitset<MAX_N> &used_vs
        ) const {
            while (true) {
                my_bitset<MAX_N> target_vs = pick_range_vs(qs, range_size);
                if (!has_overlap(used_vs, target_vs)) return target_vs;
            }
        }

        // 責務: destroyed_vs に v の削除 cost を集約して加算する。
        // 必要な理由: 既存 PhysicalRangeDestroyer が使う RangeQueryDestroyer 由来の返り値意味に揃えるため。
        void add_destroyed_v_cost(
                my_vector<std::pair<int, int>> &destroyed_vs,
                my_vector<int> &destroyed_vs_idxs,
                int v,
                int ins_turn
        ) const {
            if (destroyed_vs_idxs[v] < 0) {
                destroyed_vs_idxs[v] = static_cast<int>(destroyed_vs.size());
                destroyed_vs.push_back({v, 0});
            }
            destroyed_vs[destroyed_vs_idxs[v]].second += ins_turn;
        }

        // 責務: target v に対応する query の ins_turn 合計を v ごとに集める。
        // 必要な理由: physical construct 順に使う destroyed_vs を既存 range destroy と同じ cost 意味にするため。
        void collect_target_costs(
                const QueriesState &qs,
                const my_bitset<MAX_N> &target_vs,
                my_vector<std::pair<int, int>> &destroyed_vs
        ) const {
            my_vector<int> destroyed_vs_idxs(current_st.initial_N, -1);
            for (const QueryCommonRI &qri: qs.queries) {
                const int v = qri.get_tar_v();
                if (!target_vs[v]) continue;
                add_destroyed_v_cost(destroyed_vs, destroyed_vs_idxs, v, qri.get_ins_turn());
            }
        }
    };

    // 責務: コピーした state/query から 1 v だけを physical remove し、sum_turn 差分を cost として返す。
    // 必要な理由: 対象選択は既存 destroyer に任せつつ、再構築順だけを state と query の両方を消した正確な重さにするため。
    template<size_t MAX_N>
    class ExactCostMeasurer {
    public:
        static int measure(const State &current_st, const YakinamashiState<MAX_N> &yst, int v) {
            State trial_st = current_st;
            YakinamashiState<MAX_N> trial_yst = yst;
            PhysicalDestroyContext<MAX_N> context(trial_st, trial_yst);
            my_vector<std::pair<int, int>> one_v;
            one_v.push_back({v, 0});
            const int before_sum_turn = yst.queries_state.sum_turn;
            EraseQueriesReBuilder<MAX_N>::rebuild_by_order(context, one_v);
            return before_sum_turn - context.yst.queries_state.sum_turn;
        }

        static my_vector<std::pair<int, int>> attach_costs(
                const State &current_st,
                const YakinamashiState<MAX_N> &yst,
                const my_vector<std::pair<int, int>> &picked_vs
        ) {
            my_vector<std::pair<int, int>> costed_vs;
            costed_vs.reserve(picked_vs.size());
            for (const auto &[v, ignored_cost]: picked_vs) {
                (void) ignored_cost;
                costed_vs.push_back({v, measure(current_st, yst, v)});
            }
            return costed_vs;
        }
    };

    // 責務: 既存 range query destroyer で query を削除し、返却 cost だけを 1 v physical remove 差分へ置き換える。
    // 必要な理由: range 選択と query erase の既存仕様を保ったまま、constructor の v 順を正確な cost に基づかせるため。
    template<size_t MAX_N>
    class ExactCostRangeDestroyer {
        const State &current_st;
        RangeQueryDestroyer<MAX_N> query_destroyer;

    public:
        explicit ExactCostRangeDestroyer(const State &current_st, int range_size_override = -1)
                : current_st(current_st), query_destroyer(current_st, range_size_override) {}

        my_vector<std::pair<int, int>> destroy(YakinamashiState<MAX_N> &yst) {
            YakinamashiState<MAX_N> before_yst = yst;
            my_vector<std::pair<int, int>> picked_vs = query_destroyer.destroy(yst);
            return ExactCostMeasurer<MAX_N>::attach_costs(current_st, before_yst, picked_vs);
        }

        int get_range_size_override() const { return query_destroyer.get_range_size_override(); }
    };

    // 責務: 既存 tar-v range query destroyer で query を削除し、返却 cost だけを 1 v physical remove 差分へ置き換える。
    // 必要な理由: target v range の既存選択仕様を保ったまま、constructor の v 順を正確な cost に基づかせるため。
    template<size_t MAX_N>
    class ExactCostTarDestroyer {
        const State &current_st;
        RandomTarVRangeDestroyer<MAX_N> query_destroyer;

    public:
        explicit ExactCostTarDestroyer(const State &current_st, int target_v_range_size)
                : current_st(current_st), query_destroyer(current_st, target_v_range_size) {}

        my_vector<std::pair<int, int>> destroy(YakinamashiState<MAX_N> &yst) {
            YakinamashiState<MAX_N> before_yst = yst;
            my_vector<std::pair<int, int>> picked_vs = query_destroyer.destroy(yst);
            return ExactCostMeasurer<MAX_N>::attach_costs(current_st, before_yst, picked_vs);
        }

        int get_target_v_range_size() const { return query_destroyer.get_target_v_range_size(); }

        int get_last_start_v() const { return query_destroyer.get_last_start_v(); }

        int get_last_end_v_exclusive() const { return query_destroyer.get_last_end_v_exclusive(); }
    };

    // 責務: 既存 high-impact query destroyer で query を削除し、返却 cost だけを 1 v physical remove 差分へ置き換える。
    // 必要な理由: top-m random の既存選択仕様を保ったまま、constructor の v 順を正確な cost に基づかせるため。
    template<size_t MAX_N>
    class ExactCostImpactDestroyer {
        const State &current_st;
        HighImpactVTopMRandomDestroyer<MAX_N> query_destroyer;

    public:
        explicit ExactCostImpactDestroyer(const State &current_st, int select_v_count, int candidate_top_m)
                : current_st(current_st), query_destroyer(current_st, select_v_count, candidate_top_m) {}

        my_vector<std::pair<int, int>> destroy(YakinamashiState<MAX_N> &yst) {
            YakinamashiState<MAX_N> before_yst = yst;
            my_vector<std::pair<int, int>> picked_vs = query_destroyer.destroy(yst);
            return ExactCostMeasurer<MAX_N>::attach_costs(current_st, before_yst, picked_vs);
        }

        int get_select_v_count() const { return query_destroyer.get_select_v_count(); }

        int get_candidate_top_m() const { return query_destroyer.get_candidate_top_m(); }

        const my_vector<int> &get_last_selected_vs() const { return query_destroyer.get_last_selected_vs(); }

        const my_vector<std::pair<int, int>> &get_last_top_candidates() const {
            return query_destroyer.get_last_top_candidates();
        }
    };

    // 責務: 既存 physical range destroyer で対象 v を選び、返却 cost だけを 1 v physical remove 差分へ置き換える。
    // 必要な理由: physical destroy の実削除仕様を保ったまま、constructor の v 順を正確な cost に基づかせるため。
    template<size_t MAX_N>
    class PhysicalExactCostRangeDestroyer {
        const State &current_st;
        PhysicalRangeDestroyer<MAX_N> physical_destroyer;

    public:
        explicit PhysicalExactCostRangeDestroyer(const State &current_st, int range_size_override = -1)
                : current_st(current_st), physical_destroyer(current_st, range_size_override) {}

        my_vector<std::pair<int, int>> pick_destroyed_vs(const YakinamashiState<MAX_N> &yst) {
            my_vector<std::pair<int, int>> picked_vs = physical_destroyer.pick_destroyed_vs(yst);
            return ExactCostMeasurer<MAX_N>::attach_costs(current_st, yst, picked_vs);
        }

        int get_range_size_override() const { return physical_destroyer.get_range_size_override(); }
    };

    // 責務: 既存 physical tar-v range destroyer で対象 v を選び、返却 cost だけを 1 v physical remove 差分へ置き換える。
    // 必要な理由: physical target v range の実削除仕様を保ったまま、constructor の v 順を正確な cost に基づかせるため。
    template<size_t MAX_N>
    class PhysicalExactCostTarDestroyer {
        const State &current_st;
        PhysicalTarRangeDestroyer<MAX_N> physical_destroyer;

    public:
        explicit PhysicalExactCostTarDestroyer(const State &current_st, int target_v_range_size)
                : current_st(current_st), physical_destroyer(current_st, target_v_range_size) {}

        my_vector<std::pair<int, int>> pick_destroyed_vs(const YakinamashiState<MAX_N> &yst) {
            my_vector<std::pair<int, int>> picked_vs = physical_destroyer.pick_destroyed_vs(yst);
            return ExactCostMeasurer<MAX_N>::attach_costs(current_st, yst, picked_vs);
        }

        int get_target_v_range_size() const { return physical_destroyer.get_target_v_range_size(); }

        int get_last_start_v() const { return physical_destroyer.get_last_start_v(); }

        int get_last_end_v_exclusive() const { return physical_destroyer.get_last_end_v_exclusive(); }
    };

    // 責務: 既存 physical high-impact destroyer で対象 v を選び、返却 cost だけを 1 v physical remove 差分へ置き換える。
    // 必要な理由: physical high-impact の実削除仕様を保ったまま、constructor の v 順を正確な cost に基づかせるため。
    template<size_t MAX_N>
    class PhysicalExactCostImpactDestroyer {
        const State &current_st;
        PhysicalImpactDestroyer<MAX_N> physical_destroyer;

    public:
        explicit PhysicalExactCostImpactDestroyer(const State &current_st, int select_v_count, int candidate_top_m)
                : current_st(current_st), physical_destroyer(current_st, select_v_count, candidate_top_m) {}

        my_vector<std::pair<int, int>> pick_destroyed_vs(const YakinamashiState<MAX_N> &yst) {
            my_vector<std::pair<int, int>> picked_vs = physical_destroyer.pick_destroyed_vs(yst);
            return ExactCostMeasurer<MAX_N>::attach_costs(current_st, yst, picked_vs);
        }

        int get_select_v_count() const { return physical_destroyer.get_select_v_count(); }

        int get_candidate_top_m() const { return physical_destroyer.get_candidate_top_m(); }

        const my_vector<int> &get_last_selected_vs() const { return physical_destroyer.get_last_selected_vs(); }

        const my_vector<std::pair<int, int>> &get_last_top_candidates() const {
            return physical_destroyer.get_last_top_candidates();
        }
    };

    // 責務: PHY_Q は physical destroy と query destroy を v ごとに分ける arm であり、両側が非空になるまで割当を作り直す。
    // 必要な理由: 対象 v 選択と cost は既存 query destroyer に揃えつつ、construct 側へ physical/query 区分を渡すため。
    template<size_t MAX_N>
    PhyQDestroyPlan<MAX_N> build_phy_q_plan(
            const my_vector<std::pair<int, int>> &destroyed_vs,
            double physical_ratio
    ) {
        my_assert(2 <= static_cast<int>(destroyed_vs.size()));
        PhyQDestroyPlan<MAX_N> plan;
        plan.destroyed_vs = destroyed_vs;
        while (true) {
            plan.is_physical_v.reset();
            int physical_count = 0;
            int query_count = 0;
            for (const auto &[v, destroyed_cost]: destroyed_vs) {
                (void) destroyed_cost;
                const double rand_0_1 =
                        static_cast<double>(my_rand(PHY_Q_RANDOM_SCALE)) / static_cast<double>(PHY_Q_RANDOM_SCALE);
                if (rand_0_1 < physical_ratio) {
                    plan.is_physical_v.set(v);
                    physical_count++;
                } else {
                    query_count++;
                }
            }
            if (0 < physical_count && 0 < query_count) return plan;
        }
    }

    // 責務: PhyQDestroyPlan の destroyed_vs から physical 割当 v の pair だけを同じ cost 付きで返す。
    // 必要な理由: rebuild_by_state_query の State erase 入力と query skip 入力を分けるため。
    template<size_t MAX_N>
    my_vector<std::pair<int, int>> build_phy_q_physical_vs(
            const PhyQDestroyPlan<MAX_N> &plan
    ) {
        my_vector<std::pair<int, int>> physical_vs;
        for (const auto &[v, destroyed_cost]: plan.destroyed_vs) {
            if (plan.is_physical_v[v]) physical_vs.push_back({v, destroyed_cost});
        }
        return physical_vs;
    }

    // 責務: 既存 RangeQueryDestroyer で対象 v/cost を取り、PHY_Q の physical/query 割当 plan を作る。
    // 必要な理由: range 構築の意味を変えずに physical/query 混在 arm を作るため。
    template<size_t MAX_N>
    class PhyQRangeDestroyer {
        RangeQueryDestroyer<MAX_N> query_destroyer;

    public:
        explicit PhyQRangeDestroyer(const State &current_st, int range_size_override)
                : query_destroyer(current_st, range_size_override) {}

        PhyQDestroyPlan<MAX_N> build_plan(const YakinamashiState<MAX_N> &yst) {
            YakinamashiState<MAX_N> copied_yst = yst;
            my_vector<std::pair<int, int>> destroyed_vs = query_destroyer.destroy(copied_yst);
            return build_phy_q_plan<MAX_N>(destroyed_vs, PHY_Q_PHYSICAL_RATIO);
        }

        int get_range_size_override() const { return query_destroyer.get_range_size_override(); }
    };

    // 責務: 既存 RandomTarVRangeDestroyer で対象 v/cost を取り、PHY_Q の physical/query 割当 plan を作る。
    // 必要な理由: target v range の意味を変えずに physical/query 混在 arm を作るため。
    template<size_t MAX_N>
    class PhyQTarDestroyer {
        RandomTarVRangeDestroyer<MAX_N> query_destroyer;

    public:
        explicit PhyQTarDestroyer(const State &current_st, int target_v_range_size)
                : query_destroyer(current_st, target_v_range_size) {}

        PhyQDestroyPlan<MAX_N> build_plan(const YakinamashiState<MAX_N> &yst) {
            YakinamashiState<MAX_N> copied_yst = yst;
            my_vector<std::pair<int, int>> destroyed_vs = query_destroyer.destroy(copied_yst);
            return build_phy_q_plan<MAX_N>(destroyed_vs, PHY_Q_PHYSICAL_RATIO);
        }

        int get_target_v_range_size() const { return query_destroyer.get_target_v_range_size(); }

        int get_last_start_v() const { return query_destroyer.get_last_start_v(); }

        int get_last_end_v_exclusive() const { return query_destroyer.get_last_end_v_exclusive(); }
    };

    // 責務: 既存 HighImpactVTopMRandomDestroyer で対象 v/cost を取り、PHY_Q の physical/query 割当 plan を作る。
    // 必要な理由: high-impact の候補順位と top-m random の意味を変えずに physical/query 混在 arm を作るため。
    template<size_t MAX_N>
    class PhyQImpactDestroyer {
        HighImpactVTopMRandomDestroyer<MAX_N> query_destroyer;

    public:
        explicit PhyQImpactDestroyer(const State &current_st, int select_v_count, int candidate_top_m)
                : query_destroyer(current_st, select_v_count, candidate_top_m) {}

        PhyQDestroyPlan<MAX_N> build_plan(const YakinamashiState<MAX_N> &yst) {
            YakinamashiState<MAX_N> copied_yst = yst;
            my_vector<std::pair<int, int>> destroyed_vs = query_destroyer.destroy(copied_yst);
            return build_phy_q_plan<MAX_N>(destroyed_vs, PHY_Q_PHYSICAL_RATIO);
        }

        int get_select_v_count() const { return query_destroyer.get_select_v_count(); }

        int get_candidate_top_m() const { return query_destroyer.get_candidate_top_m(); }

        const my_vector<int> &get_last_selected_vs() const { return query_destroyer.get_last_selected_vs(); }

        const my_vector<std::pair<int, int>> &get_last_top_candidates() const {
            return query_destroyer.get_last_top_candidates();
        }
    };

    template<size_t MAX_N>
    using AlnsArmDestroyerVariant = std::variant<
            RangeQueryDestroyer<MAX_N>,
            RandomTarVRangeDestroyer<MAX_N>,
            HighImpactVTopMRandomDestroyer<MAX_N>,
            CompositeRangeQueryDestroyer<MAX_N>,
            ExactCostRangeDestroyer<MAX_N>,
            ExactCostTarDestroyer<MAX_N>,
            ExactCostImpactDestroyer<MAX_N>,
            PhysicalRangeDestroyer<MAX_N>,
            PhysicalTarRangeDestroyer<MAX_N>,
            PhysicalImpactDestroyer<MAX_N>,
            CompositePhysicalRangeDestroyer<MAX_N>,
            PhysicalExactCostRangeDestroyer<MAX_N>,
            PhysicalExactCostTarDestroyer<MAX_N>,
            PhysicalExactCostImpactDestroyer<MAX_N>,
            PhyQRangeDestroyer<MAX_N>,
            PhyQTarDestroyer<MAX_N>,
            PhyQImpactDestroyer<MAX_N>,
            AOrderRangeQueryDestroyer<MAX_N, 1, 4>,
            AOrderRangePhysicalDestroyer<MAX_N, 1, 4>,
            AOrderRangeQueryDestroyer<MAX_N, 5, 10>,
            AOrderRangePhysicalDestroyer<MAX_N, 5, 10>,
            AOrderRangeQueryDestroyer<MAX_N, 11, 15>,
            AOrderRangePhysicalDestroyer<MAX_N, 11, 15>,
            AOrderRangeQueryDestroyer<MAX_N, 16, 20>,
            AOrderRangePhysicalDestroyer<MAX_N, 16, 20>,
            AOrderRangeQueryDestroyer<MAX_N, 1, 5>,
            AOrderRangePhysicalDestroyer<MAX_N, 1, 5>,
            AOrderRangeQueryDestroyer<MAX_N, 1, 14>,
            AOrderRangePhysicalDestroyer<MAX_N, 1, 14>
    >;

    template<size_t MAX_N>
    struct AlnsArmEntry {
        const char *name;
        AlnsArmDestroyerVariant<MAX_N> destroyer;
    };

    struct AlnsArmRuntimeState {
        std::vector<double> discounted_reward_sums;
        std::vector<double> discounted_pull_counts;
        std::vector<int> lahc_history;
        my_vector<int> destroy_markers;
        int lahc_idx = 0;
        int best_score = 0;
        int no_improve_count = 0;
    };

    template<size_t MAX_N>
    struct AlnsBestState {
        YakinamashiState<MAX_N> best_yst;

        explicit AlnsBestState(const YakinamashiState<MAX_N> &init_yst)
                : best_yst(init_yst) {}
    };

    // : arm 数が可変でも runtime を同じ形で初期化できるようにし、discounted UCB の統計配列長を arm 定義と一致させる。
    AlnsArmRuntimeState build_initial_alns_arm_runtime_state(int initial_score, int arm_count, int initial_n) {
        AlnsArmRuntimeState runtime;
        runtime.discounted_reward_sums.assign(arm_count, 0.0);
        runtime.discounted_pull_counts.assign(arm_count, 0.0);
        runtime.destroy_markers.assign(initial_n, 0);
        runtime.best_score = initial_score;
        runtime.lahc_history.assign(get_alns_accept_params().lahc_history_size, initial_score);
        return runtime;
    }

    // 責務: destroyed_vs の marker 合計を返す。
    // 必要な理由: destroy 個数が異なる arm 間でも平均 marker で候補を比較するため。
    int calc_destroy_marker_sum(
            const AlnsArmRuntimeState &runtime,
            const my_vector<std::pair<int, int>> &destroyed_vs
    ) {
        int marker_sum = 0;
        for (const auto &[v, destroyed_cost]: destroyed_vs) {
            (void) destroyed_cost;
            my_assert(0 <= v && v < static_cast<int>(runtime.destroy_markers.size()));
            marker_sum += runtime.destroy_markers[v];
        }
        return marker_sum;
    }

    // 責務: target v 群の marker 合計を返す。
    // 必要な理由: AOrderRange plan は construct 直前まで destroyed_vs cost を持たないため、target_vs で同じ比較を行うため。
    int calc_destroy_marker_sum(
            const AlnsArmRuntimeState &runtime,
            const my_vector<int> &target_vs
    ) {
        int marker_sum = 0;
        for (int v: target_vs) {
            my_assert(0 <= v && v < static_cast<int>(runtime.destroy_markers.size()));
            marker_sum += runtime.destroy_markers[v];
        }
        return marker_sum;
    }

    // 責務: marker 平均が右辺より小さいかを整数積で比較する。
    // 必要な理由: double を使わず、destroy 個数の異なる候補を平均 marker で比較するため。
    bool is_lower_destroy_marker_avg(int lhs_sum, int lhs_count, int rhs_sum, int rhs_count) {
        if (rhs_count <= 0) return true;
        if (lhs_count <= 0) return false;
        return static_cast<long long>(lhs_sum) * rhs_count < static_cast<long long>(rhs_sum) * lhs_count;
    }

    // 責務: retry で選んだ最小平均候補が全 v max marker か返す。
    // 必要な理由: 引ける候補が marked で詰まった場合だけ marker を緩めるため。
    bool is_destroy_marker_full(int marker_sum, int marker_count) {
        return 0 < marker_count && marker_sum == ALNS_DESTROY_MARKER_MAX * marker_count;
    }

    // 責務: accepted 時に既存 marker を 1 減らす。
    // 必要な理由: current_yst が変化したときだけ、過去に試した v の再試行価値を少し戻すため。
    void decay_destroy_markers(AlnsArmRuntimeState &runtime) {
        for (int &marker: runtime.destroy_markers) {
            if (0 < marker) marker--;
        }
    }

    // 責務: construct まで試した v を max marker にする。
    // 必要な理由: accepted/rejected に関係なく、直近で試した v 群を次回以降選ばれにくくするため。
    void mark_constructed_vs(AlnsArmRuntimeState &runtime, const my_vector<int> &constructed_vs) {
        for (int v: constructed_vs) {
            my_assert(0 <= v && v < static_cast<int>(runtime.destroy_markers.size()));
            runtime.destroy_markers[v] = ALNS_DESTROY_MARKER_MAX;
        }
    }

    bool should_accept_by_lahc(const AlnsArmRuntimeState &runtime, int current_score, int new_score) {
        if (new_score <= runtime.best_score) return true;
        if (new_score > runtime.best_score + get_alns_accept_params().worsen_limit) return false;
        if (new_score <= current_score) return true;
        return new_score <= runtime.lahc_history[runtime.lahc_idx];
    }

    // : LAHC を残したまま即時比較だけの山登り判定へ切り替えられるようにし、acceptance の比較実験を容易にする。
    bool should_accept_by_hill_climbing(int current_score, int new_score) {
        return new_score <= current_score;
    }

    // 責務: ALNS iter に対応する焼きなまし温度を返す。
    // 必要な理由: 500 iter の中で温度を 10.0 から 1.0 へ線形低下させるため。
    double get_alns_temperature(int iter) {
        my_assert(0 <= iter);
        my_assert(1 < ALNS_ANNEALING_ITER_COUNT);
        const int capped_iter = std::min(iter, ALNS_ANNEALING_ITER_COUNT - 1);
        const double progress = static_cast<double>(capped_iter) /
                                static_cast<double>(ALNS_ANNEALING_ITER_COUNT - 1);
        return ALNS_ANNEALING_START_TEMP +
               (ALNS_ANNEALING_END_TEMP - ALNS_ANNEALING_START_TEMP) * progress;
    }

    // 責務: [0,1) の乱数を my_rand 由来で返す。
    // 必要な理由: 既存の seed 再現性に合わせて焼きなましの確率受理を行うため。
    double get_alns_random_0_1() {
        return static_cast<double>(my_rand(ALNS_ACCEPT_RANDOM_SCALE)) /
               static_cast<double>(ALNS_ACCEPT_RANDOM_SCALE);
    }

    // 責務: 焼きなましの悪化受理判定を返す。
    // 必要な理由: current より悪い候補も温度に応じた確率で受理できるようにするため。
    bool should_accept_by_annealing(int current_score, int new_score, double temperature) {
        my_assert(temperature > 0.0);
        if (new_score <= current_score) return true;
        const int score_diff = new_score - current_score;
        const double probability = std::exp(-static_cast<double>(score_diff) / temperature);
        return probability > get_alns_random_0_1();
    }

    // 責務: mode に応じた ALNS 候補の受理理由を返す。
    // 必要な理由: LAHC/山登り/焼きなましを同じ更新経路で排他的に扱うため。
    std::string get_alns_accept_reason(
            AlnsAcceptMode accept_mode,
            const AlnsArmRuntimeState &runtime,
            int current_score,
            int new_score,
            int iter
    ) {
        if (new_score <= runtime.best_score) return "best";
        if (new_score > runtime.best_score + get_alns_accept_params().worsen_limit) return "reject";
        if (new_score <= current_score) return "current";
        if (accept_mode == AlnsAcceptMode::Lahc &&
            new_score <= runtime.lahc_history[runtime.lahc_idx]) {
            return "history";
        }
        if (accept_mode == AlnsAcceptMode::Annealing &&
            should_accept_by_annealing(current_score, new_score, get_alns_temperature(iter))) {
            return "annealing";
        }
        return "reject";
    }

    bool is_alns_accept_reason_accepted(const std::string &accept_reason) {
        return accept_reason != "reject";
    }

    // : 段階的な改善量の差も重みに反映できるよう、報酬を before からの純改善量へ統一する。
    int calc_alns_reward(int before_score, int after_score, int best_score_before, bool accepted) {
        if (!accepted) return 0;
        if (after_score < best_score_before) return 10;
        if (after_score < before_score) return 3;
        return 1;
    }

    void update_lahc_history(AlnsArmRuntimeState &runtime, int before_score) {
        runtime.lahc_history[runtime.lahc_idx] = before_score;
        runtime.lahc_idx = (runtime.lahc_idx + 1) % (int) runtime.lahc_history.size();
    }

    void reset_lahc_history(AlnsArmRuntimeState &runtime) {
        std::fill(runtime.lahc_history.begin(), runtime.lahc_history.end(), runtime.best_score);
        runtime.lahc_idx = 0;
    }

    double get_alns_arm_total_pull_count(const AlnsArmRuntimeState &runtime) {
        double total = 0.0;
        for (double pull_count: runtime.discounted_pull_counts) total += pull_count;
        return total;
    }

    double get_alns_arm_mean_reward(const AlnsArmRuntimeState &runtime, int arm_idx) {
        if (runtime.discounted_pull_counts[arm_idx] <= ALNS_UCB_EPS) return 0.0;
        return runtime.discounted_reward_sums[arm_idx] / runtime.discounted_pull_counts[arm_idx];
    }

    double calc_alns_arm_ucb_score(const AlnsArmRuntimeState &runtime, int arm_idx, double total_pull_count) {
        double pull_count = runtime.discounted_pull_counts[arm_idx];
        my_assert(ALNS_UCB_EPS < pull_count);
        double mean = get_alns_arm_mean_reward(runtime, arm_idx);
        double bonus = ALNS_UCB_EXPLORATION_C * std::sqrt(std::log(total_pull_count + 1.0) / pull_count);
        return mean + bonus;
    }

    bool try_choose_untried_alns_arm(
            const AlnsArmRuntimeState &runtime,
            int &out_arm_idx
    ) {
        for (int arm_idx = 0; arm_idx < (int) runtime.discounted_pull_counts.size(); arm_idx++) {
            if (runtime.discounted_pull_counts[arm_idx] <= ALNS_UCB_EPS) {
                out_arm_idx = arm_idx;
                return true;
            }
        }
        return false;
    }

    // : arm を外から絞らず、定義された arm 全体をそのまま bandit の候補集合として扱う。
    int choose_alns_arm(const AlnsArmRuntimeState &runtime) {
        my_assert(!runtime.discounted_pull_counts.empty());
        int untried_arm_idx = 0;
        if (try_choose_untried_alns_arm(runtime, untried_arm_idx)) return untried_arm_idx;
        double total_pull_count = get_alns_arm_total_pull_count(runtime);
        int best_arm_idx = 0;
        double best_score = calc_alns_arm_ucb_score(runtime, best_arm_idx, total_pull_count);
        for (int arm_idx = 1; arm_idx < (int) runtime.discounted_pull_counts.size(); arm_idx++) {
            double score = calc_alns_arm_ucb_score(runtime, arm_idx, total_pull_count);
            if (best_score < score) {
                best_score = score;
                best_arm_idx = arm_idx;
            }
        }
        return best_arm_idx;
    }

    void add_alns_arm_reward(AlnsArmRuntimeState &runtime, int arm_idx, int reward) {
        runtime.discounted_reward_sums[arm_idx] += reward;
        runtime.discounted_pull_counts[arm_idx] += 1.0;
    }

    void apply_discount_to_alns_arm_stats(AlnsArmRuntimeState &runtime) {
        for (int arm_idx = 0; arm_idx < (int) runtime.discounted_pull_counts.size(); arm_idx++) {
            runtime.discounted_reward_sums[arm_idx] *= ALNS_DISCOUNT_GAMMA;
            runtime.discounted_pull_counts[arm_idx] *= ALNS_DISCOUNT_GAMMA;
        }
    }

    template<size_t MAX_N>
    std::string alns_arm_stats_to_string(
            const AlnsArmRuntimeState &runtime,
            const std::vector<AlnsArmEntry<MAX_N>> &arms
    ) {
        double total_pull_count = get_alns_arm_total_pull_count(runtime);
        std::stringstream ss;
        ss << "ucb_stats=(";
        for (int arm_idx = 0; arm_idx < (int) arms.size(); arm_idx++) {
            if (arm_idx != 0) ss << ", ";
            ss << arms[arm_idx].name
               << ":mean=" << get_alns_arm_mean_reward(runtime, arm_idx)
               << ":pull=" << runtime.discounted_pull_counts[arm_idx];
            if (runtime.discounted_pull_counts[arm_idx] <= ALNS_UCB_EPS) ss << ":score=untried";
            else ss << ":score=" << calc_alns_arm_ucb_score(runtime, arm_idx, total_pull_count);
        }
        ss << ")";
        return ss.str();
    }

    // : ALNS の 1 iter 候補生成結果をまとめ、accept 前に score と destroy 情報を一括で扱えるようにする。
    template<size_t MAX_N>
    struct AlnsCandidateResult {
        YakinamashiState<MAX_N> candidate_yst;
        int before_score = 0;
        int after_destroy_score = 0;
        int after_construct_score = 0;
        int destroyed_cnt = 0;
        my_vector<std::pair<int, int>> destroyed_vs;
        my_vector<int> constructed_vs;
        std::string destroyer_info;
        int marker_sum = 0;
        int marker_count = 0;

        explicit AlnsCandidateResult(const YakinamashiState<MAX_N> &yst) : candidate_yst(yst) {}
    };

    // 責務: destroyed_vs の各 pair から v だけを取り出して返す。
    // 必要な理由: 各 ALNS candidate builder で同じ変換を使い、BMove 対象を今回 construct で扱った v に揃えるため。
    my_vector<int> build_constructed_vs_from_destroyed(
            const my_vector<std::pair<int, int>> &destroyed_vs
    ) {
        my_vector<int> constructed_vs;
        constructed_vs.reserve(destroyed_vs.size());
        for (const auto &destroyed_v: destroyed_vs) constructed_vs.push_back(destroyed_v.first);
        return constructed_vs;
    }

    // : destroy 後の共通処理を 1 か所に寄せ、destroyer ごとの差分をテンプレート引数に閉じ込める。
    template<size_t MAX_N, int HABA, int K, class Destroyer>
    AlnsCandidateResult<MAX_N> build_alns_candidate_common(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            Destroyer &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            int destroy_size
    ) {
        AlnsCandidateResult<MAX_N> result(current_yst);
        result.before_score = get_repeated_test_score(current_yst.queries_state);
        my_vector<std::pair<int, int>> destroyed_vs = destroyer.destroy(result.candidate_yst);
        result.constructed_vs = build_constructed_vs_from_destroyed(destroyed_vs);
        result.after_destroy_score = get_repeated_test_score(result.candidate_yst.queries_state);
        if constexpr (USE_WORST_HALF_REBUILD_CONSTRUCTOR) {
            constructor.construct_worst_half_rebuild(result.candidate_yst, destroyed_vs);
        } else if (is_worst_half_enabled()) {
            constructor.construct_worst_half_rebuild(result.candidate_yst, destroyed_vs);
        } else {
            constructor.construct(result.candidate_yst, destroyed_vs);
        }
        QueriesStateValidator<MAX_N>::validate_state(init_st, result.candidate_yst);
        result.after_construct_score = get_repeated_test_score(result.candidate_yst.queries_state);
        result.destroyed_cnt = (int) destroyed_vs.size();
        result.destroyer_info = repeated_destroyer_info_to_string(destroyer, destroy_size);
        return result;
    }

    // 責務: plan.target_vs の各 v について、現在 query 列に残る ins_turn 合計を plan.destroyed_vs に保存する。
    // 必要な理由: planned AOrder 反映後・破壊前の重さで fixed-A construct 順を決めるため。
    template<size_t MAX_N>
    void attach_aorder_plan_destroyed_costs(
            AOrderRangePlan<MAX_N> &plan,
            const YakinamashiState<MAX_N> &yst,
            int initial_n
    ) {
        plan.destroyed_vs.clear();
        my_vector<int> idx_by_v(initial_n, -1);
        for (int v: plan.target_vs) {
            my_assert(0 <= v && v < initial_n);
            idx_by_v[v] = static_cast<int>(plan.destroyed_vs.size());
            plan.destroyed_vs.push_back({v, 0});
        }
        for (const QueryCommonRI &qri: yst.queries_state.queries) {
            const int v = qri.get_tar_v();
            if (v < 0 || initial_n <= v) continue;
            const int idx = idx_by_v[v];
            if (idx < 0) continue;
            plan.destroyed_vs[idx].second += qri.get_ins_turn();
        }
    }

    // 責務: plan.is_destroyed_v に含まれる target v の query index を昇順で返す。
    // 必要な理由: planned AOrder を維持したまま QueriesModifier::erase_queries に渡すため。
    template<size_t MAX_N>
    my_vector<int> collect_aorder_plan_query_idxs(
            const AOrderRangePlan<MAX_N> &plan,
            const YakinamashiState<MAX_N> &yst
    ) {
        my_vector<int> query_idxs;
        for (int i = 0; i < static_cast<int>(yst.queries_state.queries.size()); i++) {
            const int v = yst.queries_state.queries[i].get_tar_v();
            if (plan.is_destroyed_v[v]) query_idxs.push_back(i);
        }
        return query_idxs;
    }

    // 責務: EraseQueriesReBuilder に空 destroy を渡し、現在 order に合わせて query を作り直す。
    // 必要な理由: AOrder range destroy 前に全 OrdPos/RelOrd を planned AOrder へ同期するため。
    template<size_t MAX_N>
    void rebuild_queries_after_planned_aorder(
            State &current_st,
            YakinamashiState<MAX_N> &yst
    ) {
        PhysicalDestroyContext<MAX_N> context(current_st, yst);
        my_vector<std::pair<int, int>> empty_vs;
        EraseQueriesReBuilder<MAX_N>::rebuild_by_order(context, empty_vs);
    }

    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const RangeQueryDestroyer<MAX_N> &destroyer) {
        return repeated_destroyer_info_to_string(destroyer, destroyer.get_range_size_override());
    }

    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const RandomTarVRangeDestroyer<MAX_N> &destroyer) {
        return repeated_destroyer_info_to_string(destroyer, destroyer.get_target_v_range_size());
    }

    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const HighImpactVTopMRandomDestroyer<MAX_N> &destroyer) {
        return repeated_destroyer_info_to_string(destroyer, destroyer.get_select_v_count());
    }

    // 責務: 通常 query destroyer を current_yst のコピーに 1 回だけ適用し、construct 前候補として返す。
    // 必要な理由: retry 中は construct を実行せず、destroyed_vs の marker 平均だけで候補を比較するため。
    template<size_t MAX_N, class Destroyer>
    AlnsCandidateResult<MAX_N> build_query_destroy_trial(
            const YakinamashiState<MAX_N> &current_yst,
            Destroyer destroyer,
            const AlnsArmRuntimeState &runtime
    ) {
        AlnsCandidateResult<MAX_N> result(current_yst);
        result.before_score = get_repeated_test_score(current_yst.queries_state);
        my_vector<std::pair<int, int>> destroyed_vs = destroyer.destroy(result.candidate_yst);
        result.destroyed_vs = destroyed_vs;
        result.constructed_vs = build_constructed_vs_from_destroyed(destroyed_vs);
        result.after_destroy_score = get_repeated_test_score(result.candidate_yst.queries_state);
        result.destroyed_cnt = static_cast<int>(destroyed_vs.size());
        result.destroyer_info = build_alns_destroyer_info(destroyer);
        result.marker_sum = calc_destroy_marker_sum(runtime, destroyed_vs);
        result.marker_count = static_cast<int>(destroyed_vs.size());
        return result;
    }

    // 責務: 通常 query destroyer の候補を retry 回数だけ引き、marker 平均が最小の候補を返す。
    // 必要な理由: construct 前に最近試した v 群を避け、全候補が max marker の場合だけ marker を緩めるため。
    template<size_t MAX_N, class Destroyer>
    AlnsCandidateResult<MAX_N> choose_query_destroy_trial(
            const YakinamashiState<MAX_N> &current_yst,
            Destroyer &destroyer,
            AlnsArmRuntimeState &runtime
    ) {
        auto choose_once = [&]() {
            if constexpr (!USE_DESTROY_MARKER) {
                return build_query_destroy_trial<MAX_N>(current_yst, destroyer, runtime);
            }
            AlnsCandidateResult<MAX_N> best =
                    build_query_destroy_trial<MAX_N>(current_yst, destroyer, runtime);
            for (int retry = 1; retry < ALNS_DESTROY_MARKER_RETRY; retry++) {
                AlnsCandidateResult<MAX_N> trial =
                        build_query_destroy_trial<MAX_N>(current_yst, destroyer, runtime);
                if (is_lower_destroy_marker_avg(
                        trial.marker_sum, trial.marker_count, best.marker_sum, best.marker_count
                )) {
                    best = trial;
                }
            }
            return best;
        };

        AlnsCandidateResult<MAX_N> best = choose_once();
        if constexpr (!USE_DESTROY_MARKER) return best;
        if (is_destroy_marker_full(best.marker_sum, best.marker_count)) {
            decay_destroy_markers(runtime);
            best = choose_once();
        }
        return best;
    }

    // 責務: CompositeRangeQueryDestroyer の種別と 2 つの range size を文字列化する。
    // 必要な理由: output から range 5+5 合成 arm の実行を確認できるようにするため。
    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const CompositeRangeQueryDestroyer<MAX_N> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=composite_range_query"
           << " first_range_size=" << destroyer.get_first_range_size()
           << " second_range_size=" << destroyer.get_second_range_size();
        return ss.str();
    }

    // 責務: ExactCostRangeDestroyer の種別と destroy size をログ文字列にする。
    // 必要な理由: output から正確 cost 版の arm 実行を追えるようにするため。
    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const ExactCostRangeDestroyer<MAX_N> &destroyer) {
        return std::string("destroyer=exact_cost_range destroy_size=") +
               std::to_string(destroyer.get_range_size_override());
    }

    // 責務: ExactCostTarDestroyer の種別、range size、直近 target range をログ文字列にする。
    // 必要な理由: output から正確 cost 版の target range 選択を追えるようにするため。
    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const ExactCostTarDestroyer<MAX_N> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=exact_cost_tar_range"
           << " target_v_range_size=" << destroyer.get_target_v_range_size()
           << " start_v=" << destroyer.get_last_start_v()
           << " end_v_exclusive=" << destroyer.get_last_end_v_exclusive();
        return ss.str();
    }

    // 責務: ExactCostImpactDestroyer の種別、top-m 設定、直近候補をログ文字列にする。
    // 必要な理由: output から正確 cost 版 high-impact arm の選択内容を追えるようにするため。
    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const ExactCostImpactDestroyer<MAX_N> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=exact_cost_high_impact"
           << " select_v_count=" << destroyer.get_select_v_count()
           << " candidate_top_m=" << destroyer.get_candidate_top_m()
           << " selected_vs=" << repeated_ints_to_string(destroyer.get_last_selected_vs())
           << " top_candidates=" << repeated_v_decreases_to_string(destroyer.get_last_top_candidates());
        return ss.str();
    }

    template<size_t MAX_N, int HABA, int K, class Destroyer>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_query_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            Destroyer &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        AlnsCandidateResult<MAX_N> result = choose_query_destroy_trial<MAX_N>(current_yst, destroyer, runtime);
        const my_vector<std::pair<int, int>> &destroyed_vs = result.destroyed_vs;
        if constexpr (USE_WORST_HALF_REBUILD_CONSTRUCTOR) {
            constructor.construct_worst_half_rebuild(result.candidate_yst, destroyed_vs);
        } else if (is_worst_half_enabled()) {
            constructor.construct_worst_half_rebuild(result.candidate_yst, destroyed_vs);
        } else {
            constructor.construct(result.candidate_yst, destroyed_vs);
        }
        QueriesStateValidator<MAX_N>::validate_state(init_st, result.candidate_yst);
        result.after_construct_score = get_repeated_test_score(result.candidate_yst.queries_state);
        return result;
    }

    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const PhysicalRangeDestroyer<MAX_N> &destroyer) {
        return std::string("destroyer=physical_range destroy_size=") +
               std::to_string(destroyer.get_range_size_override());
    }

    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const PhysicalTarRangeDestroyer<MAX_N> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=physical_tar_range"
           << " target_v_range_size=" << destroyer.get_target_v_range_size()
           << " start_v=" << destroyer.get_last_start_v()
           << " end_v_exclusive=" << destroyer.get_last_end_v_exclusive();
        return ss.str();
    }

    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const PhysicalImpactDestroyer<MAX_N> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=physical_high_impact"
           << " select_v_count=" << destroyer.get_select_v_count()
           << " candidate_top_m=" << destroyer.get_candidate_top_m()
           << " selected_vs=" << repeated_ints_to_string(destroyer.get_last_selected_vs())
           << " top_candidates=" << repeated_v_decreases_to_string(destroyer.get_last_top_candidates());
        return ss.str();
    }

    // 責務: CompositePhysicalRangeDestroyer の種別と 2 つの range size を文字列化する。
    // 必要な理由: output から physical range 5+5 合成 arm の実行を確認できるようにするため。
    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const CompositePhysicalRangeDestroyer<MAX_N> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=composite_physical_range"
           << " first_range_size=" << destroyer.get_first_range_size()
           << " second_range_size=" << destroyer.get_second_range_size();
        return ss.str();
    }

    // 責務: PhysicalExactCostRangeDestroyer の種別と destroy size をログ文字列にする。
    // 必要な理由: output から正確 cost 版 physical arm の実行を追えるようにするため。
    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const PhysicalExactCostRangeDestroyer<MAX_N> &destroyer) {
        return std::string("destroyer=physical_exact_cost_range destroy_size=") +
               std::to_string(destroyer.get_range_size_override());
    }

    // 責務: PhysicalExactCostTarDestroyer の種別、range size、直近 target range をログ文字列にする。
    // 必要な理由: output から正確 cost 版 physical target range 選択を追えるようにするため。
    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const PhysicalExactCostTarDestroyer<MAX_N> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=physical_exact_cost_tar_range"
           << " target_v_range_size=" << destroyer.get_target_v_range_size()
           << " start_v=" << destroyer.get_last_start_v()
           << " end_v_exclusive=" << destroyer.get_last_end_v_exclusive();
        return ss.str();
    }

    // 責務: PhysicalExactCostImpactDestroyer の種別、top-m 設定、直近候補をログ文字列にする。
    // 必要な理由: output から正確 cost 版 physical high-impact arm の選択内容を追えるようにするため。
    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const PhysicalExactCostImpactDestroyer<MAX_N> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=physical_exact_cost_high_impact"
           << " select_v_count=" << destroyer.get_select_v_count()
           << " candidate_top_m=" << destroyer.get_candidate_top_m()
           << " selected_vs=" << repeated_ints_to_string(destroyer.get_last_selected_vs())
           << " top_candidates=" << repeated_v_decreases_to_string(destroyer.get_last_top_candidates());
        return ss.str();
    }

    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const PhyQRangeDestroyer<MAX_N> &destroyer) {
        return std::string("destroyer=PHY_Q_range destroy_size=") +
               std::to_string(destroyer.get_range_size_override());
    }

    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const PhyQTarDestroyer<MAX_N> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=PHY_Q_tar_range"
           << " target_v_range_size=" << destroyer.get_target_v_range_size()
           << " start_v=" << destroyer.get_last_start_v()
           << " end_v_exclusive=" << destroyer.get_last_end_v_exclusive();
        return ss.str();
    }

    template<size_t MAX_N>
    std::string build_alns_destroyer_info(const PhyQImpactDestroyer<MAX_N> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=PHY_Q_high_impact"
           << " select_v_count=" << destroyer.get_select_v_count()
           << " candidate_top_m=" << destroyer.get_candidate_top_m()
           << " selected_vs=" << repeated_ints_to_string(destroyer.get_last_selected_vs())
           << " top_candidates=" << repeated_v_decreases_to_string(destroyer.get_last_top_candidates());
        return ss.str();
    }

    // 責務: fixed-A AOrder range query arm の min/max v count を文字列化する。
    // 必要な理由: ALNS ログから新 arm の対象個数範囲を確認できるようにするため。
    template<size_t MAX_N, int MIN_V_CNT, int MAX_V_CNT>
    std::string build_alns_destroyer_info(const AOrderRangeQueryDestroyer<MAX_N, MIN_V_CNT, MAX_V_CNT> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=aorder_range_query"
           << " min_v_cnt=" << destroyer.get_min_v_cnt()
           << " max_v_cnt=" << destroyer.get_max_v_cnt();
        return ss.str();
    }

    // 責務: fixed-A AOrder range physical arm の min/max v count を文字列化する。
    // 必要な理由: ALNS ログから新 arm の対象個数範囲を確認できるようにするため。
    template<size_t MAX_N, int MIN_V_CNT, int MAX_V_CNT>
    std::string build_alns_destroyer_info(const AOrderRangePhysicalDestroyer<MAX_N, MIN_V_CNT, MAX_V_CNT> &destroyer) {
        std::stringstream ss;
        ss << "destroyer=aorder_range_physical"
           << " min_v_cnt=" << destroyer.get_min_v_cnt()
           << " max_v_cnt=" << destroyer.get_max_v_cnt();
        return ss.str();
    }

    // 責務: physical destroyer の pick 結果と marker 評価を保持する。
    // 必要な理由: physical erase/construct 前に複数 pick から marker 平均が低いものを選ぶため。
    struct MarkedDestroyedVs {
        my_vector<std::pair<int, int>> destroyed_vs;
        std::string destroyer_info;
        int marker_sum = 0;
        int marker_count = 0;
    };

    // 責務: physical destroyer の候補を retry 回数だけ引き、marker 平均が最小の候補を返す。
    // 必要な理由: physical construct 前に最近試した v 群を避け、全候補が max marker の場合だけ marker を緩めるため。
    template<size_t MAX_N, class Destroyer>
    MarkedDestroyedVs choose_physical_destroy_trial(
            const YakinamashiState<MAX_N> &current_yst,
            Destroyer &destroyer,
            AlnsArmRuntimeState &runtime
    ) {
        auto build_trial = [&]() {
            Destroyer trial_destroyer = destroyer;
            MarkedDestroyedVs trial;
            trial.destroyed_vs = trial_destroyer.pick_destroyed_vs(current_yst);
            trial.destroyer_info = build_alns_destroyer_info(trial_destroyer);
            trial.marker_sum = calc_destroy_marker_sum(runtime, trial.destroyed_vs);
            trial.marker_count = static_cast<int>(trial.destroyed_vs.size());
            return trial;
        };
        auto choose_once = [&]() {
            if constexpr (!USE_DESTROY_MARKER) {
                return build_trial();
            }
            MarkedDestroyedVs best = build_trial();
            for (int retry = 1; retry < ALNS_DESTROY_MARKER_RETRY; retry++) {
                MarkedDestroyedVs trial = build_trial();
                if (is_lower_destroy_marker_avg(
                        trial.marker_sum, trial.marker_count, best.marker_sum, best.marker_count
                )) {
                    best = trial;
                }
            }
            return best;
        };

        MarkedDestroyedVs best = choose_once();
        if constexpr (!USE_DESTROY_MARKER) return best;
        if (is_destroy_marker_full(best.marker_sum, best.marker_count)) {
            decay_destroy_markers(runtime);
            best = choose_once();
        }
        return best;
    }

    template<size_t MAX_N, int HABA, int K, class Destroyer>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_physical_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            Destroyer &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        AlnsCandidateResult<MAX_N> result(current_yst);
        result.before_score = get_repeated_test_score(current_yst.queries_state);
        State candidate_st = init_st;
        PhysicalDestroyContext<MAX_N> context(candidate_st, result.candidate_yst);
        MarkedDestroyedVs picked = choose_physical_destroy_trial<MAX_N>(current_yst, destroyer, runtime);
        const my_vector<std::pair<int, int>> &destroyed_vs = picked.destroyed_vs;
        result.destroyed_vs = destroyed_vs;
        result.constructed_vs = build_constructed_vs_from_destroyed(destroyed_vs);
        result.marker_sum = picked.marker_sum;
        result.marker_count = picked.marker_count;
        EraseQueriesReBuilder<MAX_N>::rebuild_by_order(context, destroyed_vs);
        result.after_destroy_score = get_repeated_test_score(result.candidate_yst.queries_state);
        if constexpr (USE_WORST_HALF_REBUILD_CONSTRUCTOR) {
            constructor.construct_physical_worst_half_rebuild(context, destroyed_vs);
        } else if (is_worst_half_enabled()) {
            constructor.construct_physical_worst_half_rebuild(context, destroyed_vs);
        } else {
            constructor.construct_physical_destroyed(context, destroyed_vs);
        }
        QueriesStateValidator<MAX_N>::validate_state(init_st, result.candidate_yst);
        result.after_construct_score = get_repeated_test_score(result.candidate_yst.queries_state);
        result.destroyed_cnt = (int) destroyed_vs.size();
        result.destroyer_info = picked.destroyer_info;
        return result;
    }

    // 責務: PHY_Q plan と marker 評価を保持する。
    // 必要な理由: PHY_Q construct 前に複数 plan から marker 平均が低いものを選ぶため。
    template<size_t MAX_N>
    struct MarkedPhyQPlan {
        PhyQDestroyPlan<MAX_N> plan;
        std::string destroyer_info;
        int marker_sum = 0;
        int marker_count = 0;
    };

    // 責務: PHY_Q destroyer の plan を retry 回数だけ作り、marker 平均が最小の plan を返す。
    // 必要な理由: PHY_Q の physical/query 分割仕様を変えずに、construct 前の v 群選択だけを retry するため。
    template<size_t MAX_N, class Destroyer>
    MarkedPhyQPlan<MAX_N> choose_phy_q_destroy_plan(
            const YakinamashiState<MAX_N> &current_yst,
            Destroyer &destroyer,
            AlnsArmRuntimeState &runtime
    ) {
        auto build_trial = [&]() {
            Destroyer trial_destroyer = destroyer;
            MarkedPhyQPlan<MAX_N> trial;
            trial.plan = trial_destroyer.build_plan(current_yst);
            trial.destroyer_info = build_alns_destroyer_info(trial_destroyer);
            trial.marker_sum = calc_destroy_marker_sum(runtime, trial.plan.destroyed_vs);
            trial.marker_count = static_cast<int>(trial.plan.destroyed_vs.size());
            return trial;
        };
        auto choose_once = [&]() {
            if constexpr (!USE_DESTROY_MARKER) {
                return build_trial();
            }
            MarkedPhyQPlan<MAX_N> best = build_trial();
            for (int retry = 1; retry < ALNS_DESTROY_MARKER_RETRY; retry++) {
                MarkedPhyQPlan<MAX_N> trial = build_trial();
                if (is_lower_destroy_marker_avg(
                        trial.marker_sum, trial.marker_count, best.marker_sum, best.marker_count
                )) {
                    best = trial;
                }
            }
            return best;
        };

        MarkedPhyQPlan<MAX_N> best = choose_once();
        if constexpr (!USE_DESTROY_MARKER) return best;
        if (is_destroy_marker_full(best.marker_sum, best.marker_count)) {
            decay_destroy_markers(runtime);
            best = choose_once();
        }
        return best;
    }

    // 責務: PHY_Q plan の physical v だけを State/order から消し、全対象 v の query を消してから cost 順 construct を行う。
    // 必要な理由: physical/query を二段階 destroy にせず、同じ destroy 反映状態から PHY_Q construct を始めるため。
    template<size_t MAX_N, int HABA, int K, class Destroyer>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_phy_q_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            Destroyer &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        AlnsCandidateResult<MAX_N> result(current_yst);
        result.before_score = get_repeated_test_score(current_yst.queries_state);
        State candidate_st = init_st;
        PhysicalDestroyContext<MAX_N> context(candidate_st, result.candidate_yst);
        MarkedPhyQPlan<MAX_N> picked = choose_phy_q_destroy_plan<MAX_N>(current_yst, destroyer, runtime);
        PhyQDestroyPlan<MAX_N> plan = picked.plan;
        result.destroyed_vs = plan.destroyed_vs;
        result.marker_sum = picked.marker_sum;
        result.marker_count = picked.marker_count;
        result.constructed_vs = build_constructed_vs_from_destroyed(plan.destroyed_vs);
        my_vector<std::pair<int, int>> physical_vs = build_phy_q_physical_vs(plan);
        EraseQueriesReBuilder<MAX_N>::rebuild_by_state_query(context, physical_vs, plan.destroyed_vs);
        result.after_destroy_score = get_repeated_test_score(result.candidate_yst.queries_state);
        constructor.construct_phy_q_destroyed(context, plan);
        QueriesStateValidator<MAX_N>::validate_state(init_st, result.candidate_yst);
        result.after_construct_score = get_repeated_test_score(result.candidate_yst.queries_state);
        result.destroyed_cnt = static_cast<int>(plan.destroyed_vs.size());
        result.destroyer_info = picked.destroyer_info;
        return result;
    }

    // 責務: AOrderRange plan を retry 回数だけ作り、target_vs の marker 平均が最小の plan を返す。
    // 必要な理由: AOrderRange の planned AOrder 仕様を変えずに、construct 前の対象 v 群選択だけを retry するため。
    template<size_t MAX_N, class Destroyer>
    AOrderRangePlan<MAX_N> choose_aorder_destroy_plan(
            const YakinamashiState<MAX_N> &current_yst,
            Destroyer &destroyer,
            AlnsArmRuntimeState &runtime,
            AlnsCandidateResult<MAX_N> &result
    ) {
        auto build_trial = [&]() {
            return destroyer.build_plan(current_yst);
        };
        auto marker_sum_of = [&](const AOrderRangePlan<MAX_N> &plan) {
            return calc_destroy_marker_sum(runtime, plan.target_vs);
        };
        auto choose_once = [&]() {
            AOrderRangePlan<MAX_N> best = build_trial();
            if constexpr (!USE_DESTROY_MARKER) {
                result.marker_sum = marker_sum_of(best);
                result.marker_count = static_cast<int>(best.target_vs.size());
                result.destroyer_info = build_alns_destroyer_info(destroyer);
                return best;
            }
            int best_sum = marker_sum_of(best);
            int best_count = static_cast<int>(best.target_vs.size());
            for (int retry = 1; retry < ALNS_DESTROY_MARKER_RETRY; retry++) {
                AOrderRangePlan<MAX_N> trial = build_trial();
                int trial_sum = marker_sum_of(trial);
                int trial_count = static_cast<int>(trial.target_vs.size());
                if (is_lower_destroy_marker_avg(trial_sum, trial_count, best_sum, best_count)) {
                    best = trial;
                    best_sum = trial_sum;
                    best_count = trial_count;
                }
            }
            result.marker_sum = best_sum;
            result.marker_count = best_count;
            result.destroyer_info = build_alns_destroyer_info(destroyer);
            return best;
        };

        AOrderRangePlan<MAX_N> best = choose_once();
        if constexpr (!USE_DESTROY_MARKER) return best;
        if (is_destroy_marker_full(result.marker_sum, result.marker_count)) {
            decay_destroy_markers(runtime);
            best = choose_once();
        }
        return best;
    }

    // 責務: candidate 上で planned AOrder 反映、全 rebuild、query erase、fixed-A construct を行う。
    // 必要な理由: 通常 query destroy 経路へ混ぜず、A target 固定仕様を型で分けるため。
    template<size_t MAX_N, int HABA, int K, int MIN_V_CNT, int MAX_V_CNT>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            AOrderRangeQueryDestroyer<MAX_N, MIN_V_CNT, MAX_V_CNT> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        AlnsCandidateResult<MAX_N> result(current_yst);
        result.before_score = get_repeated_test_score(current_yst.queries_state);
        State candidate_st = init_st;
        AOrderRangePlan<MAX_N> plan = choose_aorder_destroy_plan<MAX_N>(current_yst, destroyer, runtime, result);
        result.candidate_yst.query_order.aorder = plan.planned_aorder;
        result.candidate_yst.query_order.aorder.validate_target_entry_order();
        rebuild_queries_after_planned_aorder(candidate_st, result.candidate_yst);
        attach_aorder_plan_destroyed_costs(plan, result.candidate_yst, init_st.initial_N);
        result.destroyed_vs = plan.destroyed_vs;
        result.constructed_vs = build_constructed_vs_from_destroyed(plan.destroyed_vs);
        my_vector<int> query_idxs = collect_aorder_plan_query_idxs(plan, result.candidate_yst);
        QueryProvider provider(result.candidate_yst, init_st);
        QueriesModifier modifier(init_st, result.candidate_yst.query_order.aorder, result.candidate_yst.queries_state,
                                 provider);
        modifier.erase_queries<MAX_N>(query_idxs);
        result.after_destroy_score = get_repeated_test_score(result.candidate_yst.queries_state);
        constructor.construct_planned_query(result.candidate_yst, plan);
        QueriesStateValidator<MAX_N>::validate_state(init_st, result.candidate_yst);
        result.after_construct_score = get_repeated_test_score(result.candidate_yst.queries_state);
        result.destroyed_cnt = static_cast<int>(plan.destroyed_vs.size());
        result.destroyer_info = build_alns_destroyer_info(destroyer);
        return result;
    }

    // 責務: candidate 上で planned AOrder 反映、全 rebuild、physical erase、fixed-A physical construct を行う。
    // 必要な理由: 通常 physical destroy 経路へ混ぜず、planned AOrder subset 復元仕様を型で分けるため。
    template<size_t MAX_N, int HABA, int K, int MIN_V_CNT, int MAX_V_CNT>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            AOrderRangePhysicalDestroyer<MAX_N, MIN_V_CNT, MAX_V_CNT> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        AlnsCandidateResult<MAX_N> result(current_yst);
        result.before_score = get_repeated_test_score(current_yst.queries_state);
        State candidate_st = init_st;
        AOrderRangePlan<MAX_N> plan = choose_aorder_destroy_plan<MAX_N>(current_yst, destroyer, runtime, result);
        result.candidate_yst.query_order.aorder = plan.planned_aorder;
        result.candidate_yst.query_order.aorder.validate_target_entry_order();
        rebuild_queries_after_planned_aorder(candidate_st, result.candidate_yst);
        attach_aorder_plan_destroyed_costs(plan, result.candidate_yst, init_st.initial_N);
        result.destroyed_vs = plan.destroyed_vs;
        result.constructed_vs = build_constructed_vs_from_destroyed(plan.destroyed_vs);
        PhysicalDestroyContext<MAX_N> context(candidate_st, result.candidate_yst);
        EraseQueriesReBuilder<MAX_N>::rebuild_by_order(context, plan.destroyed_vs);
        result.after_destroy_score = get_repeated_test_score(result.candidate_yst.queries_state);
        constructor.construct_planned_physical(context, plan);
        QueriesStateValidator<MAX_N>::validate_state(init_st, result.candidate_yst);
        result.after_construct_score = get_repeated_test_score(result.candidate_yst.queries_state);
        result.destroyed_cnt = static_cast<int>(plan.destroyed_vs.size());
        result.destroyer_info = build_alns_destroyer_info(destroyer);
        return result;
    }

    template<size_t MAX_N, int HABA, int K, class Destroyer>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            Destroyer &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return build_alns_candidate_by_query_destroyer<MAX_N, HABA, K>(
                init_st, current_yst, destroyer, constructor, runtime
        );
    }

    template<size_t MAX_N, int HABA, int K>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            PhysicalRangeDestroyer<MAX_N> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return build_alns_candidate_by_physical_destroyer<MAX_N, HABA, K>(
                init_st, current_yst, destroyer, constructor, runtime
        );
    }

    template<size_t MAX_N, int HABA, int K>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            PhysicalTarRangeDestroyer<MAX_N> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return build_alns_candidate_by_physical_destroyer<MAX_N, HABA, K>(
                init_st, current_yst, destroyer, constructor, runtime
        );
    }

    template<size_t MAX_N, int HABA, int K>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            PhysicalImpactDestroyer<MAX_N> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return build_alns_candidate_by_physical_destroyer<MAX_N, HABA, K>(
                init_st, current_yst, destroyer, constructor, runtime
        );
    }

    template<size_t MAX_N, int HABA, int K>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            CompositePhysicalRangeDestroyer<MAX_N> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return build_alns_candidate_by_physical_destroyer<MAX_N, HABA, K>(
                init_st, current_yst, destroyer, constructor, runtime
        );
    }

    // 責務: PhysicalExactCostRangeDestroyer の候補生成を rebuild_by_order 使用経路へ委譲する。
    // 必要な理由: pick_destroyed_vs を持つ exact cost physical destroyer を query destroy 経路で扱わないようにするため。
    template<size_t MAX_N, int HABA, int K>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            PhysicalExactCostRangeDestroyer<MAX_N> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return build_alns_candidate_by_physical_destroyer<MAX_N, HABA, K>(
                init_st, current_yst, destroyer, constructor, runtime
        );
    }

    // 責務: PhysicalExactCostTarDestroyer の候補生成を rebuild_by_order 使用経路へ委譲する。
    // 必要な理由: pick_destroyed_vs を持つ exact cost physical destroyer を query destroy 経路で扱わないようにするため。
    template<size_t MAX_N, int HABA, int K>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            PhysicalExactCostTarDestroyer<MAX_N> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return build_alns_candidate_by_physical_destroyer<MAX_N, HABA, K>(
                init_st, current_yst, destroyer, constructor, runtime
        );
    }

    // 責務: PhysicalExactCostImpactDestroyer の候補生成を rebuild_by_order 使用経路へ委譲する。
    // 必要な理由: pick_destroyed_vs を持つ exact cost physical destroyer を query destroy 経路で扱わないようにするため。
    template<size_t MAX_N, int HABA, int K>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            PhysicalExactCostImpactDestroyer<MAX_N> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return build_alns_candidate_by_physical_destroyer<MAX_N, HABA, K>(
                init_st, current_yst, destroyer, constructor, runtime
        );
    }

    template<size_t MAX_N, int HABA, int K>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            PhyQRangeDestroyer<MAX_N> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return build_alns_candidate_by_phy_q_destroyer<MAX_N, HABA, K>(
                init_st, current_yst, destroyer, constructor, runtime
        );
    }

    template<size_t MAX_N, int HABA, int K>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            PhyQTarDestroyer<MAX_N> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return build_alns_candidate_by_phy_q_destroyer<MAX_N, HABA, K>(
                init_st, current_yst, destroyer, constructor, runtime
        );
    }

    template<size_t MAX_N, int HABA, int K>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_destroyer(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            PhyQImpactDestroyer<MAX_N> &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return build_alns_candidate_by_phy_q_destroyer<MAX_N, HABA, K>(
                init_st, current_yst, destroyer, constructor, runtime
        );
    }

    template<size_t MAX_N, int HABA, int K>
    AlnsCandidateResult<MAX_N> build_alns_candidate_by_arm(
            const State &init_st,
            const YakinamashiState<MAX_N> &current_yst,
            AlnsArmEntry<MAX_N> &arm,
            GreedyQueriesConstructor<MAX_N> &constructor,
            AlnsArmRuntimeState &runtime
    ) {
        return std::visit(
                [&](auto &destroyer) {
                    return build_alns_candidate_by_destroyer<MAX_N, HABA, K>(
                            init_st, current_yst, destroyer, constructor, runtime
                    );
                },
                arm.destroyer
        );
    }

    template<size_t MAX_N>
    void print_alns_candidate_cmds(const State &init_st, const AlnsCandidateResult<MAX_N> &candidate) {
        my_vector<command> candidate_cmds = build_reconstructed_cmds(init_st, candidate.candidate_yst.queries_state);
        out(std::string("candidate_cmds=") + repeated_cmds_to_string(candidate_cmds));
    }

    template<size_t MAX_N>
    void print_alns_arm_iter_log(
            int iter,
            int arm_idx,
            const std::vector<AlnsArmEntry<MAX_N>> &arms,
            const AlnsCandidateResult<MAX_N> &candidate,
            int history_threshold,
            bool accepted,
            const std::string &accept_reason,
            int reward,
            int current_score,
            const AlnsArmRuntimeState &runtime
    ) {
        std::stringstream ss;
        int accepted_int = 0;
        if (accepted) accepted_int = 1;
        ss << "alns_iter=" << iter << " destroyer=" << arms[arm_idx].name
           << " before=" << candidate.before_score << " after_destroy=" << candidate.after_destroy_score
           << " after_construct=" << candidate.after_construct_score << " accepted=" << accepted_int
           << " accept_reason=" << accept_reason
           << " history_threshold=" << history_threshold << " reward=" << reward
           << " current_score=" << current_score << " best_score=" << runtime.best_score
           << " no_improve_count=" << runtime.no_improve_count
           << " lahc_idx=" << runtime.lahc_idx;
        out(ss.str());
        out(candidate.destroyer_info);
        out(alns_arm_stats_to_string(runtime, arms));
    }

    template<size_t MAX_N>
    bool update_alns_best_state(
            YakinamashiState<MAX_N> &current_yst,
            AlnsBestState<MAX_N> &best_state,
            AlnsArmRuntimeState &runtime,
            bool accepted
    ) {
        if (accepted && get_repeated_test_score(current_yst.queries_state) < runtime.best_score) {
            best_state.best_yst = current_yst;
            runtime.best_score = get_repeated_test_score(current_yst.queries_state);
            runtime.no_improve_count = 0;
            return true;
        }
        runtime.no_improve_count++;
        return false;
    }

    // 責務: accepted flag を介さず、現在状態の score が best を改善していれば best_state と runtime を更新する。
    // 必要な理由: BOrder 近傍は 30 回まとめて扱い、近傍ごとの best 更新を避けるため。
    template<size_t MAX_N>
    bool update_alns_best_state_without_accept_gate(
            const YakinamashiState<MAX_N> &current_yst,
            AlnsBestState<MAX_N> &best_state,
            AlnsArmRuntimeState &runtime
    ) {
        const int current_score = get_repeated_test_score(current_yst.queries_state);
        if (current_score < runtime.best_score) {
            best_state.best_yst = current_yst;
            runtime.best_score = current_score;
            runtime.no_improve_count = 0;
            return true;
        }
        return false;
    }

    template<size_t MAX_N>
    bool restart_alns_if_stalled(
            int iter,
            YakinamashiState<MAX_N> &current_yst,
            const AlnsBestState<MAX_N> &best_state,
            AlnsArmRuntimeState &runtime
    ) {
        if (runtime.no_improve_count < ALNS_RESTART_LIMIT) return false;
        current_yst = best_state.best_yst;
        reset_lahc_history(runtime);
        runtime.no_improve_count = 0;
        //Optuna out("alns_restart iter=" + std::to_string(iter) + " best_score=" + std::to_string(runtime.best_score));
        return true;
    }

    bool should_print_alns_iter_log(int iter, bool improved_best_score, bool restarted) {
        if (improved_best_score) return true;
        if (restarted) return true;
        return iter % 100 == 0;
    }

    // 責務: start_time から time_limit_sec 秒以上が経過したか返す。
    // 必要な理由: 1 iterate の途中で止めず、完了境界で 120 秒制限を適用するため。
    bool has_elapsed_time_limit(
            const std::chrono::steady_clock::time_point &start_time,
            int time_limit_sec
    ) {
        if (time_limit_sec < 0) return false;
        std::chrono::steady_clock::time_point now_time = std::chrono::steady_clock::now();
        std::chrono::seconds elapsed_sec =
                std::chrono::duration_cast<std::chrono::seconds>(now_time - start_time);
        return elapsed_sec.count() >= time_limit_sec;
    }

    struct AlnsIterUpdateResult {
        bool accepted = false;
        bool improved_best_score = false;
        bool restarted = false;
        int reward = 0;
        std::string accept_reason;
    };

    // 責務: target_vs に含まれる BOrder TARGET entry の BMove 候補を試し、明確に改善した候補を current_yst へ反映する。
    // 必要な理由: destroy/construct で扱った v に限定して BOrder の単一 AllOrd 順序を局所探索するため。
    template<size_t MAX_N>
    void run_b_all_ord_move_neighborhoods(
            const State &init_st,
            YakinamashiState<MAX_N> &current_yst,
            const my_vector<int> &target_vs,
            AlnsAcceptMode accept_mode,
            AlnsArmRuntimeState &runtime
    ) {
        (void) accept_mode;
        (void) runtime;
        for (int trial = 0; trial < B_ALL_ORD_MOVE_TRIAL_COUNT; trial++) {
            const int before_score = get_repeated_test_score(current_yst.queries_state);
            YakinamashiState<MAX_N> candidate_yst =
                    NH_BAllOrdMove<MAX_N>::build_candidate(init_st, current_yst, target_vs);
            QueriesStateValidator<MAX_N>::validate_state(init_st, candidate_yst);
            const int candidate_score = get_repeated_test_score(candidate_yst.queries_state);
            if (candidate_score < before_score) {
                cout << "b_all_ord_move_accept"
                     << " trial " << trial
                     << " before " << before_score
                     << " after " << candidate_score
                     << " diff " << candidate_score - before_score
                     << endl;
                current_yst = candidate_yst;
            }
        }
    }

    template<size_t MAX_N>
    AlnsIterUpdateResult update_alns_state_by_candidate(
            int iter,
            int arm_idx,
            int before_score,
            AlnsAcceptMode accept_mode,
            YakinamashiState<MAX_N> &current_yst,
            const AlnsCandidateResult<MAX_N> &candidate,
            AlnsArmRuntimeState &runtime,
            AlnsBestState<MAX_N> &best_state
    ) {
        AlnsIterUpdateResult result;
        result.accept_reason = get_alns_accept_reason(
                accept_mode, runtime, before_score, candidate.after_construct_score, iter
        );
        result.accepted = is_alns_accept_reason_accepted(result.accept_reason);
        result.reward = calc_alns_reward(before_score, candidate.after_construct_score, runtime.best_score,
                                         result.accepted);
        if (result.accepted) current_yst = candidate.candidate_yst;
        result.improved_best_score = update_alns_best_state(current_yst, best_state, runtime, result.accepted);
        add_alns_arm_reward(runtime, arm_idx, result.reward);
        if constexpr (USE_DESTROY_MARKER) {
            if (result.accepted) decay_destroy_markers(runtime);
            mark_constructed_vs(runtime, candidate.constructed_vs);
        }
        update_lahc_history(runtime, candidate.after_construct_score);
        return result;
    }

    template<size_t MAX_N, int HABA, int K>
    void run_alns_lahc_iter_by_arms(
            const State &init_st,
            YakinamashiState<MAX_N> &current_yst,
            GreedyQueriesConstructor<MAX_N> &constructor,
            std::vector<AlnsArmEntry<MAX_N>> &arms,
            AlnsArmRuntimeState &runtime,
            AlnsBestState<MAX_N> &best_state,
            int iter,
            AlnsAcceptMode accept_mode,
            bool show_alns_log,
            bool show_alns_file_log, std::ofstream *owner_best_cmd_log = nullptr
    ) {
        apply_discount_to_alns_arm_stats(runtime);
        int arm_idx = choose_alns_arm(runtime);
        int before_score = get_repeated_test_score(current_yst.queries_state);
        int history_threshold = runtime.lahc_history[runtime.lahc_idx];
        const auto timing_iter_start = std::chrono::steady_clock::now();
        auto timing_stage_start = timing_iter_start;
        auto write_iter_timing = [&](const std::string &stage) {
            if (!GreedyQueriesConstructor<MAX_N>::is_timing_log_enabled()) return;
            const auto timing_now = std::chrono::steady_clock::now();
            const auto elapsed_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(timing_now - timing_stage_start).count();
            std::ostringstream ss;
            ss << "stage=" << stage
               << " arm=" << arms[arm_idx].name
               << " before_score=" << before_score
               << " current_sum=" << current_yst.queries_state.sum_turn
               << " history_threshold=" << history_threshold
               << " elapsed_ms=" << elapsed_ms;
            GreedyQueriesConstructor<MAX_N>::write_timing_log(ss.str());
            timing_stage_start = timing_now;
        };
        write_iter_timing("iter_begin");
        if (show_alns_log) {
            cout << "alns_iter_begin " << iter << " arm " << arms[arm_idx].name << endl;
        }
        AlnsCandidateResult<MAX_N> candidate = build_alns_candidate_by_arm<MAX_N, HABA, K>(
                init_st, current_yst, arms[arm_idx], constructor, runtime
        );
        if (GreedyQueriesConstructor<MAX_N>::is_timing_log_enabled()) {
            const auto timing_now = std::chrono::steady_clock::now();
            const auto elapsed_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(timing_now - timing_stage_start).count();
            std::ostringstream ss;
            ss << "stage=build_candidate"
               << " arm=" << arms[arm_idx].name
               << " before_score=" << before_score
               << " after_destroy=" << candidate.after_destroy_score
               << " after_construct=" << candidate.after_construct_score
               << " constructed_count=" << candidate.constructed_vs.size()
               << " elapsed_ms=" << elapsed_ms;
            GreedyQueriesConstructor<MAX_N>::write_timing_log(ss.str());
            timing_stage_start = timing_now;
        }
        if constexpr (USE_B_ALL_ORD_MOVE) {
            run_b_all_ord_move_neighborhoods(
                    init_st, candidate.candidate_yst, candidate.constructed_vs, accept_mode, runtime
            );
            write_iter_timing("b_move_constexpr");
            candidate.after_construct_score = get_repeated_test_score(candidate.candidate_yst.queries_state);
        } else if (is_b_move_enabled()) {
            run_b_all_ord_move_neighborhoods(
                    init_st, candidate.candidate_yst, candidate.constructed_vs, accept_mode, runtime
            );
            write_iter_timing("b_move_runtime");
            candidate.after_construct_score = get_repeated_test_score(candidate.candidate_yst.queries_state);
        }
        AlnsIterUpdateResult result = update_alns_state_by_candidate(
                iter, arm_idx, before_score, accept_mode, current_yst, candidate, runtime, best_state
        );
        if (GreedyQueriesConstructor<MAX_N>::is_timing_log_enabled()) {
            const auto timing_now = std::chrono::steady_clock::now();
            const auto elapsed_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(timing_now - timing_stage_start).count();
            std::ostringstream ss;
            ss << "stage=update_alns_state"
               << " arm=" << arms[arm_idx].name
               << " accepted=" << result.accepted
               << " improved=" << result.improved_best_score
               << " reward=" << result.reward
               << " accept_reason=" << result.accept_reason
               << " elapsed_ms=" << elapsed_ms;
            GreedyQueriesConstructor<MAX_N>::write_timing_log(ss.str());
            timing_stage_start = timing_now;
        }
        result.restarted = restart_alns_if_stalled(iter, current_yst, best_state, runtime);
        if (GreedyQueriesConstructor<MAX_N>::is_timing_log_enabled()) {
            const auto timing_now = std::chrono::steady_clock::now();
            const auto elapsed_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(timing_now - timing_stage_start).count();
            const auto total_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(timing_now - timing_iter_start).count();
            std::ostringstream ss;
            ss << "stage=iter_end"
               << " arm=" << arms[arm_idx].name
               << " restarted=" << result.restarted
               << " accepted=" << result.accepted
               << " best_score=" << runtime.best_score
               << " current_sum=" << current_yst.queries_state.sum_turn
               << " elapsed_ms=" << elapsed_ms
               << " total_ms=" << total_ms;
            GreedyQueriesConstructor<MAX_N>::write_timing_log(ss.str());
            timing_stage_start = timing_now;
        }
        if (show_alns_log) {
            cout << "alns_iter_end " << iter
                 << " before " << before_score
                 << " after " << candidate.after_construct_score
                 << " accepted " << result.accepted
                 << " best_score " << runtime.best_score
                 << endl;
        }
        //Optuna cout << "iter"<<iter << endl;
        if (result.improved_best_score) {
            if (show_alns_log) {
                cout << "alns_iter " << iter
                     << " best_score " << runtime.best_score << endl;
            }
            if (show_alns_file_log) {
                print_alns_best_update_to_output3(iter, init_st, current_yst, runtime.best_score);
            }
            if (owner_best_cmd_log != nullptr) {
                write_owner_best_cmd_log(*owner_best_cmd_log, iter, init_st, current_yst, runtime.best_score);
            }
        }
        if (should_print_alns_iter_log(iter, result.improved_best_score, result.restarted)) {
            //Optuna print_alns_arm_iter_log(
            //Optuna     iter, arm_idx, arms, candidate, history_threshold, result.accepted, result.accept_reason, result.reward,
            //Optuna     get_repeated_test_score(current_yst.queries_state), runtime
            //Optuna );
            if (result.improved_best_score) {
                //Optuna print_construct_profile_lines_to_output<MAX_N>(
                //Optuna     "alns_iter=" + std::to_string(iter) + " destroyer=" + arms[arm_idx].name
                //Optuna );
                //Optuna print_alns_candidate_cmds(init_st, candidate);
            }
        }
    }

    // 責務: output.txt の先頭に acceptance の種類を出す。
    // 必要な理由: 同じ destroyer 構成でも LAHC/山登り/焼きなましの違いを見分けやすくするため。
    void print_alns_accept_mode(AlnsAcceptMode accept_mode) {
        if (accept_mode == AlnsAcceptMode::Lahc) {
            out("alns_accept_mode=lahc_l500_best_limit");
            return;
        }
        if (accept_mode == AlnsAcceptMode::Annealing) {
            out("alns_accept_mode=annealing_t10_to_t1");
            return;
        }
        out("alns_accept_mode=hill_climbing");
    }

    // 責務: 現在 active な ALNS destroyer arm を作り、params.destroy_mask に合う種別だけを残す。
    // 必要な理由: run_optuna_case100/case500 のどちらでも同じ mask 指定で arm 構成を比較できるようにするため。
    template<size_t MAX_N>
    std::vector<AlnsArmEntry<MAX_N>> build_alns_destroyer_arms_for_n100(
            const State &init_state, const GreedyConstructorParams &params
    ) {
        std::vector<AlnsArmEntry<MAX_N>> arms;
        const int state_n = init_state.current_N;
        if (1 <= state_n && state_n < 20) {
            arms.push_back({"range_2", RangeQueryDestroyer<MAX_N>(init_state, 2)});
        } else if (20 <= state_n && state_n < 100) {
            arms.push_back({"aorder_range_q_1_14", AOrderRangeQueryDestroyer<MAX_N, 1, 5>(init_state)});
            arms.push_back({"aorder_range_p_1_14", AOrderRangePhysicalDestroyer<MAX_N, 1, 5>(init_state)});
//            if (params.destroy_mask == 3) {
//                arms.push_back({"PHY_Q_range_6", PhyQRangeDestroyer<MAX_N>(init_state, 6)});
//                arms.push_back({"PHY_Q_tar_range_5", PhyQTarDestroyer<MAX_N>(init_state, 5)});
//                arms.push_back({"PHY_Q_high_impact_6_12", PhyQImpactDestroyer<MAX_N>(init_state, 6, 12)});
//            }
//            arms.push_back({"range_6", RangeQueryDestroyer<MAX_N>(init_state, 6)});
//            arms.push_back({"random_tar_v_range", RandomTarVRangeDestroyer<MAX_N>(init_state, 5)});
//            arms.push_back({"high_impact", HighImpactVTopMRandomDestroyer<MAX_N>(init_state, 6, 12)});
//
//            arms.push_back({"physical_range_2", PhysicalRangeDestroyer<MAX_N>(init_state, 2)});
//            arms.push_back({"physical_range_3", PhysicalRangeDestroyer<MAX_N>(init_state, 3)});
//            arms.push_back({"physical_range_4", PhysicalRangeDestroyer<MAX_N>(init_state, 4)});
//            arms.push_back({"physical_range_5", PhysicalRangeDestroyer<MAX_N>(init_state, 5)});
//            arms.push_back({"physical_tar_range_2", PhysicalTarRangeDestroyer<MAX_N>(init_state, 2)});
//            arms.push_back({"physical_tar_range_3", PhysicalTarRangeDestroyer<MAX_N>(init_state, 3)});
//            arms.push_back({"physical_tar_range_4", PhysicalTarRangeDestroyer<MAX_N>(init_state, 4)});
//            arms.push_back({"physical_tar_range_5", PhysicalTarRangeDestroyer<MAX_N>(init_state, 5)});
//            arms.push_back({"physical_high_impact_2_4", PhysicalImpactDestroyer<MAX_N>(init_state, 2, 4)});
//            arms.push_back({"physical_high_impact_3_6", PhysicalImpactDestroyer<MAX_N>(init_state, 3, 6)});
//            arms.push_back({"physical_high_impact_4_8", PhysicalImpactDestroyer<MAX_N>(init_state, 4, 8)});
//            arms.push_back({"physical_high_impact_5_10", PhysicalImpactDestroyer<MAX_N>(init_state, 5, 10)});
        } else if (MAX_N < 500) {
            arms.push_back({"physical_range_6", PhysicalRangeDestroyer<MAX_N>(init_state, 6)});
//            arms.push_back({"physical_composite_range_5_5", CompositePhysicalRangeDestroyer<MAX_N>(init_state, 3, 5)});
//             arms.push_back({"physical_composite_range_5_5", CompositePhysicalRangeDestroyer<MAX_N>(init_state, 5, 5)});
//             arms.push_back({"physical_composite_range_5_5", CompositePhysicalRangeDestroyer<MAX_N>(init_state, 5, 7)});
            arms.push_back({"physical_range_8", PhysicalRangeDestroyer<MAX_N>(init_state, 8)});
            arms.push_back({"physical_range_10", PhysicalRangeDestroyer<MAX_N>(init_state, 10)});
            arms.push_back({"physical_range_12", PhysicalRangeDestroyer<MAX_N>(init_state, 12)});
            arms.push_back({"physical_range_14", PhysicalRangeDestroyer<MAX_N>(init_state, 14)});
            arms.push_back({"physical_range_16", PhysicalRangeDestroyer<MAX_N>(init_state, 16)});
            arms.push_back({"physical_range_18", PhysicalRangeDestroyer<MAX_N>(init_state, 18)});
//             arms.push_back({"physical_range_20", PhysicalRangeDestroyer<MAX_N>(init_state, 20)});
            arms.push_back({"physical_tar_range_5", PhysicalTarRangeDestroyer<MAX_N>(init_state, 5)});
            arms.push_back({"physical_high_impact_6_12", PhysicalImpactDestroyer<MAX_N>(init_state, 6, 12)});
            arms.push_back({"range_6", RangeQueryDestroyer<MAX_N>(init_state, 6)});
//             arms.push_back({"composite_range_5_5", CompositeRangeQueryDestroyer<MAX_N>(init_state, 3, 5)});
//             arms.push_back({"composite_range_5_5", CompositeRangeQueryDestroyer<MAX_N>(init_state, 5, 5)});
//             arms.push_back({"composite_range_5_5", CompositeRangeQueryDestroyer<MAX_N>(init_state, 5, 7)});
//             arms.push_back({"composite_range_5_5", CompositeRangeQueryDestroyer<MAX_N>(init_state, 5, 9)});
//             arms.push_back({"composite_range_5_5", CompositeRangeQueryDestroyer<MAX_N>(init_state, 7, 7)});
//             arms.push_back({"composite_range_5_5", CompositeRangeQueryDestroyer<MAX_N>(init_state, 7, 9)});
            arms.push_back({"range_8", RangeQueryDestroyer<MAX_N>(init_state, 8)});
            arms.push_back({"range_10", RangeQueryDestroyer<MAX_N>(init_state, 10)});
            arms.push_back({"range_12", RangeQueryDestroyer<MAX_N>(init_state, 12)});
            arms.push_back({"range_14", RangeQueryDestroyer<MAX_N>(init_state, 14)});
            arms.push_back({"range_16", RangeQueryDestroyer<MAX_N>(init_state, 16)});
            arms.push_back({"range_18", RangeQueryDestroyer<MAX_N>(init_state, 18)});
//             arms.push_back({"range_20", RangeQueryDestroyer<MAX_N>(init_state, 20)});
            arms.push_back({"random_tar_v_range", RandomTarVRangeDestroyer<MAX_N>(init_state, 5)});
            arms.push_back({"random_tar_v_range", RandomTarVRangeDestroyer<MAX_N>(init_state, 10)});
            arms.push_back({"high_impact", HighImpactVTopMRandomDestroyer<MAX_N>(init_state, 6, 12)});
//             if (params.destroy_mask == 3) {
//                 arms.push_back({"PHY_Q_range_6", PhyQRangeDestroyer<MAX_N>(init_state, 6)});
//                 arms.push_back({"PHY_Q_range_8", PhyQRangeDestroyer<MAX_N>(init_state, 8)});
//                 arms.push_back({"PHY_Q_range_10", PhyQRangeDestroyer<MAX_N>(init_state, 10)});
//                 arms.push_back({"PHY_Q_range_12", PhyQRangeDestroyer<MAX_N>(init_state, 12)});
//                 arms.push_back({"PHY_Q_range_14", PhyQRangeDestroyer<MAX_N>(init_state, 14)});
//                 arms.push_back({"PHY_Q_range_16", PhyQRangeDestroyer<MAX_N>(init_state, 16)});
//                 arms.push_back({"PHY_Q_range_18", PhyQRangeDestroyer<MAX_N>(init_state, 18)});
//                 arms.push_back({"PHY_Q_range_20", PhyQRangeDestroyer<MAX_N>(init_state, 20)});
//                 arms.push_back({"PHY_Q_tar_range_5", PhyQTarDestroyer<MAX_N>(init_state, 5)});
//                 arms.push_back({"PHY_Q_tar_range_10", PhyQTarDestroyer<MAX_N>(init_state, 10)});
//                 arms.push_back({"PHY_Q_high_impact_6_12", PhyQImpactDestroyer<MAX_N>(init_state, 6, 12)});
//             }
            if (is_aorder_range_enabled()) {
                arms.push_back({"aorder_range_q_1_4", AOrderRangeQueryDestroyer<MAX_N, 1, 4>(init_state)});
                arms.push_back({"aorder_range_p_1_4", AOrderRangePhysicalDestroyer<MAX_N, 1, 4>(init_state)});
                arms.push_back({"aorder_range_q_5_10", AOrderRangeQueryDestroyer<MAX_N, 5, 10>(init_state)});
                arms.push_back({"aorder_range_p_5_10", AOrderRangePhysicalDestroyer<MAX_N, 5, 10>(init_state)});
                arms.push_back({"aorder_range_q_11_15", AOrderRangeQueryDestroyer<MAX_N, 11, 15>(init_state)});
                arms.push_back({"aorder_range_p_11_15", AOrderRangePhysicalDestroyer<MAX_N, 11, 15>(init_state)});
            }

        } else {
            arms.push_back({"physical_range_6", PhysicalRangeDestroyer<MAX_N>(init_state, 6)});
            arms.push_back({"physical_range_8", PhysicalRangeDestroyer<MAX_N>(init_state, 8)});
            arms.push_back({"physical_range_10", PhysicalRangeDestroyer<MAX_N>(init_state, 10)});
            arms.push_back({"physical_range_12", PhysicalRangeDestroyer<MAX_N>(init_state, 12)});
            arms.push_back({"physical_range_14", PhysicalRangeDestroyer<MAX_N>(init_state, 14)});
            arms.push_back({"physical_range_16", PhysicalRangeDestroyer<MAX_N>(init_state, 16)});
            arms.push_back({"physical_range_18", PhysicalRangeDestroyer<MAX_N>(init_state, 18)});
            arms.push_back({"physical_range_20", PhysicalRangeDestroyer<MAX_N>(init_state, 20)});
//            arms.push_back({"physical_range_22", PhysicalRangeDestroyer<MAX_N>(init_state, 22)});
//            arms.push_back({"physical_range_24", PhysicalRangeDestroyer<MAX_N>(init_state, 24)});
//            arms.push_back({"physical_range_26", PhysicalRangeDestroyer<MAX_N>(init_state, 26)});
//            arms.push_back({"physical_range_28", PhysicalRangeDestroyer<MAX_N>(init_state, 28)});
//            arms.push_back({"physical_range_30", PhysicalRangeDestroyer<MAX_N>(init_state, 30)});
            arms.push_back({"physical_tar_range_5", PhysicalTarRangeDestroyer<MAX_N>(init_state, 5)});
            arms.push_back({"physical_tar_range_9", PhysicalTarRangeDestroyer<MAX_N>(init_state, 9)});
            arms.push_back({"physical_tar_range_13", PhysicalTarRangeDestroyer<MAX_N>(init_state, 13)});
            arms.push_back({"physical_tar_range_17", PhysicalTarRangeDestroyer<MAX_N>(init_state, 17)});
            arms.push_back({"physical_tar_range_21", PhysicalTarRangeDestroyer<MAX_N>(init_state, 21)});
//            arms.push_back({"physical_tar_range_25", PhysicalTarRangeDestroyer<MAX_N>(init_state, 25)});
//            arms.push_back({"physical_tar_range_29", PhysicalTarRangeDestroyer<MAX_N>(init_state, 29)});
            arms.push_back({"physical_high_impact_6_12", PhysicalImpactDestroyer<MAX_N>(init_state, 6, 12)});
            arms.push_back({"physical_high_impact_12_30", PhysicalImpactDestroyer<MAX_N>(init_state, 12, 30)});
            arms.push_back({"physical_high_impact_16_50", PhysicalImpactDestroyer<MAX_N>(init_state, 16, 50)});
            arms.push_back({"physical_high_impact_20_100", PhysicalImpactDestroyer<MAX_N>(init_state, 20, 100)});
//            arms.push_back({"physical_high_impact_24_100", PhysicalImpactDestroyer<MAX_N>(init_state, 24, 100)});
//            arms.push_back({"physical_high_impact_28_100", PhysicalImpactDestroyer<MAX_N>(init_state, 28, 100)});
            arms.push_back({"range_6", RangeQueryDestroyer<MAX_N>(init_state, 6)});
            arms.push_back({"range_8", RangeQueryDestroyer<MAX_N>(init_state, 8)});
            arms.push_back({"range_10", RangeQueryDestroyer<MAX_N>(init_state, 10)});
            arms.push_back({"range_12", RangeQueryDestroyer<MAX_N>(init_state, 12)});
            arms.push_back({"range_14", RangeQueryDestroyer<MAX_N>(init_state, 14)});
            arms.push_back({"range_16", RangeQueryDestroyer<MAX_N>(init_state, 16)});
            arms.push_back({"range_18", RangeQueryDestroyer<MAX_N>(init_state, 18)});
            arms.push_back({"range_20", RangeQueryDestroyer<MAX_N>(init_state, 20)});
            arms.push_back({"range_22", RangeQueryDestroyer<MAX_N>(init_state, 22)});
//           arms.push_back({"range_24", RangeQueryDestroyer<MAX_N>(init_state, 24)});
//           arms.push_back({"range_26", RangeQueryDestroyer<MAX_N>(init_state, 26)});
//           arms.push_back({"range_28", RangeQueryDestroyer<MAX_N>(init_state, 28)});
//           arms.push_back({"range_30", RangeQueryDestroyer<MAX_N>(init_state, 30)});
            arms.push_back({"random_tar_v_range", RandomTarVRangeDestroyer<MAX_N>(init_state, 5)});
            arms.push_back({"random_tar_v_range", RandomTarVRangeDestroyer<MAX_N>(init_state, 9)});
            arms.push_back({"random_tar_v_range", RandomTarVRangeDestroyer<MAX_N>(init_state, 13)});
            arms.push_back({"random_tar_v_range", RandomTarVRangeDestroyer<MAX_N>(init_state, 17)});
            arms.push_back({"random_tar_v_range", RandomTarVRangeDestroyer<MAX_N>(init_state, 21)});
//           arms.push_back({"random_tar_v_range", RandomTarVRangeDestroyer<MAX_N>(init_state, 25)});
//           arms.push_back({"random_tar_v_range", RandomTarVRangeDestroyer<MAX_N>(init_state, 29)});
            arms.push_back({"high_impact", HighImpactVTopMRandomDestroyer<MAX_N>(init_state, 6, 12)});
            arms.push_back({"high_impact", HighImpactVTopMRandomDestroyer<MAX_N>(init_state, 12, 30)});
            arms.push_back({"high_impact", HighImpactVTopMRandomDestroyer<MAX_N>(init_state, 16, 50)});
            arms.push_back({"high_impact", HighImpactVTopMRandomDestroyer<MAX_N>(init_state, 20, 100)});
//           arms.push_back({"high_impact", HighImpactVTopMRandomDestroyer<MAX_N>(init_state, 24, 100)});
//           arms.push_back({"high_impact", HighImpactVTopMRandomDestroyer<MAX_N>(init_state, 28, 100)});
        }
        my_assert(1 <= params.destroy_mask && params.destroy_mask <= 3);
        const bool use_physical_destroy = (params.destroy_mask & 1) != 0;
        const bool use_query_destroy = (params.destroy_mask & 2) != 0;
        auto is_physical_arm = [](const AlnsArmEntry<MAX_N> &arm) {
            const std::string arm_name(arm.name);
            return arm_name.rfind("physical", 0) == 0 || arm_name.find("_p_") != std::string::npos;
        };
        std::vector<AlnsArmEntry<MAX_N>> filtered_arms;
        filtered_arms.reserve(arms.size());
        for (auto &arm: arms) {
            const bool keep_arm = is_physical_arm(arm) ? use_physical_destroy : use_query_destroy;
            if (keep_arm) filtered_arms.push_back(std::move(arm));
        }
        my_assert(!filtered_arms.empty());
        return filtered_arms;
    }

    // human_review_begin: push_swap CLI では ALNS 進捗ログを stdout に出さないようにするため。
    // 責務: ALNS の iter/sum 進捗ログ出力可否を保持する。
    // 必要な理由: 既存実験の進捗ログは保ち、tester 実行時だけ stdout 出力を止めるため。
    bool &alns_sum_log_enabled() {
        static bool enabled = true;
        return enabled;
    }
    // human_review_end

    // human_review_begin: push_swap CLI では ALNS 進捗ログを stdout に出さないようにするため。
    // 責務: ALNS の iter/sum 進捗ログ出力可否を切り替える。
    // 必要な理由: 単一入力 solver から iter/sum の stdout 出力を止めるため。
    void set_alns_sum_log(bool enabled) {
        alns_sum_log_enabled() = enabled;
    }
    // human_review_end

    //alns+igの唯一の入り口
    template<size_t MAX_N, int HABA, int K>
    int
    run_alns_destroy_construct(
            const State &init_state,
            YakinamashiState<MAX_N> &yst,
            int iter_count,
            AlnsAcceptMode accept_mode,
            const GreedyConstructorParams &params,
            int time_limit_sec,
            bool show_alns_log,
            bool show_alns_file_log,
            std::ofstream *owner_best_cmd_log
    ) {
        my_assert(iter_count >= 0);
        //Optuna std::cout<<" run_alns_destroy_construct"<< std::endl;
        GreedyQueriesConstructor<MAX_N> constructor(init_state, params);
        std::vector<AlnsArmEntry<MAX_N>> arms = build_alns_destroyer_arms_for_n100<MAX_N>(init_state, params);
        AlnsArmRuntimeState runtime = build_initial_alns_arm_runtime_state(
                get_repeated_test_score(yst.queries_state), static_cast<int>(arms.size()), init_state.initial_N
        );
        if (show_alns_file_log) {
            print_alns_initial_state_to_output3(init_state, runtime.best_score);
        }
        AlnsBestState<MAX_N> best_state(yst);
        //Optuna print_alns_accept_mode(accept_mode);
        //Optuna out(std::string("alns_initial_state=") + repeated_state_to_string(init_state.A));
        //Optuna out(std::string("alns_initial_score=") + std::to_string(runtime.best_score));
        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
        for (int iter = 0; iter < iter_count; iter++) {
            GreedyQueriesConstructor<MAX_N>::set_timing_iter(iter);
            if (show_alns_log) cout << "iter " << iter << endl;
            // human_review_begin: 通常実験では進捗ログを保ち、push_swap CLI では stdout 汚染を止めるため。
            if (alns_sum_log_enabled()) {
                cout << "iter " << iter << ", sum " << yst.queries_state.sum_turn << endl;
            }
            // human_review_end
            run_alns_lahc_iter_by_arms<MAX_N, HABA, K>(
                    init_state, yst, constructor, arms, runtime, best_state, iter, accept_mode,
                    show_alns_log, show_alns_file_log, owner_best_cmd_log
            );
            if (has_elapsed_time_limit(start_time, time_limit_sec)) break;
        }
        yst = best_state.best_yst;
        int best_score = runtime.best_score;
        return best_score;
    }

    // 責務: 指定 collector mode と固定 ALNS seed で同じ init_state から ALNS を実行し、最終 score を返す。
    // 必要な理由: owner 経路単体の平均 score を旧経路と同条件で比較するため。
    template<size_t MAX_N, int HABA, int K>
    int run_collector_score(
            const State &init_state,
            bool use_owner
    ) {
        constexpr int ALNS_SEED = 2026041451;
        constexpr int ITER_COUNT = 100;
        constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
        constexpr int NO_TIME_LIMIT_SEC = -1;
        GreedyConstructorParams params{10, 10, 10, 10, 10, 20, 15, 3};
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(use_owner);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
        set_my_rand_seed(static_cast<uint32_t>(ALNS_SEED));
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState<MAX_N> current_yst = factory.build(init_state);
        return run_alns_destroy_construct<MAX_N, HABA, K>(
                init_state,
                current_yst,
                ITER_COUNT,
                ACCEPT_MODE,
                params,
                NO_TIME_LIMIT_SEC,
                false,
                false);
    }

    // 責務: 100 個の init_state で旧/owner score と累積平均を標準出力する。
    // 必要な理由: 個別 case の揺れではなく、owner 経路の平均的な score 変化を見るため。
    template<size_t MAX_N, int HABA, int K>
    void run_collector_compare_100() {
        constexpr int CASE_COUNT = 100;
        constexpr int INIT_BASE_SEED = 2026041451;
        constexpr int TEST_N = 100;
        long long legacy_sum = 0;
        long long owner_sum = 0;
        for (int case_idx = 0; case_idx < CASE_COUNT; case_idx++) {
            set_my_rand_seed(static_cast<uint32_t>(INIT_BASE_SEED + case_idx));
            std::vector<int> perm = build_repeated_test_perm(TEST_N);
            State init_state = build_repeated_test_state(perm);
            const int legacy_score = run_collector_score<MAX_N, HABA, K>(init_state, false);
            const int owner_score = run_collector_score<MAX_N, HABA, K>(init_state, true);
            legacy_sum += legacy_score;
            owner_sum += owner_score;
            const double legacy_avg = static_cast<double>(legacy_sum) / (case_idx + 1);
            const double owner_avg = static_cast<double>(owner_sum) / (case_idx + 1);
            cout << "collector_compare"
                 << " case=" << case_idx
                 << " legacy=" << legacy_score
                 << " owner=" << owner_score
                 << " legacy_avg=" << legacy_avg
                 << " owner_avg=" << owner_avg
                 << endl;
        }
        cout << "collector_compare_summary"
             << " cases=" << CASE_COUNT
             << " legacy_avg=" << (static_cast<double>(legacy_sum) / CASE_COUNT)
             << " owner_avg=" << (static_cast<double>(owner_sum) / CASE_COUNT)
             << endl;
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    }

    // 責務: owner collector と固定 ALNS seed で、worst-half の有無だけ変えて最終 score を返す。
    // 必要な理由: 同じ init_state と seed で owner 通常 construct と owner+worst-half の score 差を見るため。
    template<size_t MAX_N, int HABA, int K>
    int run_owner_score(
            const State &init_state,
            bool use_worst_half
    ) {
        constexpr int ALNS_SEED = 2026041451;
        constexpr int ITER_COUNT = 100;
        constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
        constexpr int NO_TIME_LIMIT_SEC = -1;
        GreedyConstructorParams params{10, 10, 10, 10, 10, 20, 15, 3};
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
        set_worst_half_enabled(use_worst_half);
        set_my_rand_seed(static_cast<uint32_t>(ALNS_SEED));
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState<MAX_N> current_yst = factory.build(init_state);
        return run_alns_destroy_construct<MAX_N, HABA, K>(
                init_state,
                current_yst,
                ITER_COUNT,
                ACCEPT_MODE,
                params,
                NO_TIME_LIMIT_SEC,
                false,
                false);
    }

    // 責務: 100 個の init_state で owner 通常/owner+worst-half score と累積平均を標準出力する。
    // 必要な理由: 個別 case の揺れではなく、worst-half 追加による平均的な score 変化を見るため。
    template<size_t MAX_N, int HABA, int K>
    void run_owner_worst_compare_100() {
        constexpr int CASE_COUNT = 100;
        constexpr int INIT_BASE_SEED = 2026041451;
        constexpr int TEST_N = 100;
        long long owner_sum = 0;
        long long worst_sum = 0;
        for (int case_idx = 0; case_idx < CASE_COUNT; case_idx++) {
            set_my_rand_seed(static_cast<uint32_t>(INIT_BASE_SEED + case_idx));
            std::vector<int> perm = build_repeated_test_perm(TEST_N);
            State init_state = build_repeated_test_state(perm);
            const int owner_score = run_owner_score<MAX_N, HABA, K>(init_state, false);
            const int worst_score = run_owner_score<MAX_N, HABA, K>(init_state, true);
            owner_sum += owner_score;
            worst_sum += worst_score;
            const double owner_avg = static_cast<double>(owner_sum) / (case_idx + 1);
            const double worst_avg = static_cast<double>(worst_sum) / (case_idx + 1);
            cout << "owner_worst_compare"
                 << " case=" << case_idx
                 << " owner=" << owner_score
                 << " worst_half=" << worst_score
                 << " owner_avg=" << owner_avg
                 << " worst_half_avg=" << worst_avg
                 << endl;
        }
        cout << "owner_worst_compare_summary"
             << " cases=" << CASE_COUNT
             << " owner_avg=" << (static_cast<double>(owner_sum) / CASE_COUNT)
             << " worst_half_avg=" << (static_cast<double>(worst_sum) / CASE_COUNT)
             << endl;
        set_worst_half_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    }

    // 責務: owner collector と固定 ALNS seed で、BMove の有無だけ変えて最終 score を返す。
    // 必要な理由: 同じ init_state と seed で owner 通常 construct と owner+BMove の score 差を見るため。
    template<size_t MAX_N, int HABA, int K>
    int run_owner_bmove_score(
            const State &init_state,
            bool use_b_move
    ) {
        constexpr int ALNS_SEED = 2026041451;
        constexpr int ITER_COUNT = 100;
        constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
        constexpr int NO_TIME_LIMIT_SEC = -1;
        GreedyConstructorParams params{10, 10, 10, 10, 10, 20, 15, 3};
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
        set_worst_half_enabled(false);
        set_b_move_enabled(use_b_move);
        set_my_rand_seed(static_cast<uint32_t>(ALNS_SEED));
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState<MAX_N> current_yst = factory.build(init_state);
        return run_alns_destroy_construct<MAX_N, HABA, K>(
                init_state,
                current_yst,
                ITER_COUNT,
                ACCEPT_MODE,
                params,
                NO_TIME_LIMIT_SEC,
                false,
                false);
    }

    // 責務: 100 個の init_state で owner 通常/owner+BMove score と累積平均を標準出力する。
    // 必要な理由: 個別 case の揺れではなく、BMove 追加による平均的な score 変化を見るため。
    template<size_t MAX_N, int HABA, int K>
    void run_owner_bmove_compare_100() {
        constexpr int CASE_COUNT = 50;
        constexpr int INIT_BASE_SEED = 2026041451;
        constexpr int TEST_N = 100;
        long long owner_sum = 0;
        long long bmove_sum = 0;
        for (int case_idx = 0; case_idx < CASE_COUNT; case_idx++) {
            set_my_rand_seed(static_cast<uint32_t>(INIT_BASE_SEED + case_idx));
            std::vector<int> perm = build_repeated_test_perm(TEST_N);
            State init_state = build_repeated_test_state(perm);
            const int owner_score = run_owner_bmove_score<MAX_N, HABA, K>(init_state, false);
            const int bmove_score = run_owner_bmove_score<MAX_N, HABA, K>(init_state, true);
            owner_sum += owner_score;
            bmove_sum += bmove_score;
            const double owner_avg = static_cast<double>(owner_sum) / (case_idx + 1);
            const double bmove_avg = static_cast<double>(bmove_sum) / (case_idx + 1);
            cout << "owner_bmove_compare"
                 << " case=" << case_idx
                 << " owner=" << owner_score
                 << " bmove=" << bmove_score
                 << " owner_avg=" << owner_avg
                 << " bmove_avg=" << bmove_avg
                 << endl;
        }
        cout << "owner_bmove_compare_summary"
             << " cases=" << CASE_COUNT
             << " owner_avg=" << (static_cast<double>(owner_sum) / CASE_COUNT)
             << " bmove_avg=" << (static_cast<double>(bmove_sum) / CASE_COUNT)
             << endl;
        set_b_move_enabled(false);
        set_worst_half_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    }

    // 責務: owner collector と固定 ALNS seed で、AOrderRange arm 追加の有無だけ変えて最終 score を返す。
    // 必要な理由: 同じ init_state と seed で owner 通常 arm set と owner+AOrderRange arm set の score 差を見るため。
    template<size_t MAX_N, int HABA, int K>
    int run_owner_aorder_score(
            const State &init_state,
            bool use_aorder_range
    ) {
        constexpr int ALNS_SEED = 2026041451;
        constexpr int ITER_COUNT = 100;
        constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
        constexpr int NO_TIME_LIMIT_SEC = -1;
        GreedyConstructorParams params{10, 10, 10, 10, 10, 20, 15, 3};
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
        set_worst_half_enabled(false);
        set_b_move_enabled(false);
        set_aorder_range_enabled(use_aorder_range);
        set_my_rand_seed(static_cast<uint32_t>(ALNS_SEED));
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState<MAX_N> current_yst = factory.build(init_state);
        return run_alns_destroy_construct<MAX_N, HABA, K>(
                init_state,
                current_yst,
                ITER_COUNT,
                ACCEPT_MODE,
                params,
                NO_TIME_LIMIT_SEC,
                false,
                false);
    }

    // 責務: 100 個の init_state で owner 通常/owner+AOrderRange score と累積平均を標準出力する。
    // 必要な理由: 個別 case の揺れではなく、AOrderRange arm 追加による平均的な score 変化を見るため。
    template<size_t MAX_N, int HABA, int K>
    void run_owner_aorder_compare_100() {
        constexpr int CASE_COUNT = 100;
        constexpr int INIT_BASE_SEED = 2026041451;
        constexpr int TEST_N = 100;
        long long owner_sum = 0;
        long long aorder_sum = 0;
        for (int case_idx = 0; case_idx < CASE_COUNT; case_idx++) {
            set_my_rand_seed(static_cast<uint32_t>(INIT_BASE_SEED + case_idx));
            std::vector<int> perm = build_repeated_test_perm(TEST_N);
            State init_state = build_repeated_test_state(perm);
            const int owner_score = run_owner_aorder_score<MAX_N, HABA, K>(init_state, false);
            const int aorder_score = run_owner_aorder_score<MAX_N, HABA, K>(init_state, true);
            owner_sum += owner_score;
            aorder_sum += aorder_score;
            const double owner_avg = static_cast<double>(owner_sum) / (case_idx + 1);
            const double aorder_avg = static_cast<double>(aorder_sum) / (case_idx + 1);
            cout << "owner_aorder_compare"
                 << " case=" << case_idx
                 << " owner=" << owner_score
                 << " aorder_range=" << aorder_score
                 << " owner_avg=" << owner_avg
                 << " aorder_range_avg=" << aorder_avg
                 << endl;
        }
        cout << "owner_aorder_compare_summary"
             << " cases=" << CASE_COUNT
             << " owner_avg=" << (static_cast<double>(owner_sum) / CASE_COUNT)
             << " aorder_range_avg=" << (static_cast<double>(aorder_sum) / CASE_COUNT)
             << endl;
        set_aorder_range_enabled(false);
        set_b_move_enabled(false);
        set_worst_half_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    }

    // 責務: 500 個の init_state で owner collector だけを使った ALNS score と累積平均を標準出力する。
    // 必要な理由: 単一 case の揺れではなく、owner collector 単体実行の平均的な score を見るため。
    template<size_t MAX_N, int HABA, int K>
    void run_owner_single_100() {
        constexpr int CASE_COUNT = 2000;
        constexpr int INIT_BASE_SEED = 2026041451;
        constexpr int ALNS_SEED = 2026041451;
        constexpr int ITER_COUNT = 500;
        constexpr int TEST_N = 100;
        constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
        constexpr int NO_TIME_LIMIT_SEC = -1;
        GreedyConstructorParams params{10, 10, 10, 10, 10, 20, 15, 3};
        long long score_sum = 0;
        vector<int> scores;
        for (int case_idx = 0; case_idx < CASE_COUNT; case_idx++) {
            set_my_rand_seed(static_cast<uint32_t>(INIT_BASE_SEED + case_idx));
            std::vector<int> perm = build_repeated_test_perm(TEST_N);
            State init_state = build_repeated_test_state(perm);
            GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
            GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
            set_worst_half_enabled(false);
            set_b_move_enabled(false);
            set_aorder_range_enabled(false);
            set_my_rand_seed(static_cast<uint32_t>(ALNS_SEED));
            YstFactory<MAX_N, HABA, K> factory;
            YakinamashiState<MAX_N> current_yst = factory.build(init_state);
            const int score = run_alns_destroy_construct<MAX_N, HABA, K>(
                    init_state,
                    current_yst,
                    ITER_COUNT,
                    ACCEPT_MODE,
                    params,
                    NO_TIME_LIMIT_SEC,
                    true,
                    false);
            score_sum += score;
            scores.push_back(score);
            const double score_avg = static_cast<double>(score_sum) / (case_idx + 1);
            std::sort(scores.begin(), scores.end());
            cout << "owner_single"
                 << " case=" << case_idx
                 << " score=" << score
                 << " score_avg=" << score_avg
                 << "score_min=" << scores[0]
                 << " score_med=" << scores[scores.size() / 2]
                 << "score_max=" << scores.back()
                 << endl;
        }
        cout << "owner_single_summary"
             << " cases=" << CASE_COUNT
             << " score_avg=" << (static_cast<double>(score_sum) / CASE_COUNT)
             << endl;
        set_aorder_range_enabled(false);
        set_b_move_enabled(false);
        set_worst_half_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    }

    template<size_t MAX_N, int HABA, int K>
    void run_owner_single_100_multi() {
        constexpr int CASE_COUNT = 2000;
        constexpr int INIT_BASE_SEED = 2026041451;
        constexpr int ALNS_SEED = 2026041451;
        constexpr int ITER_COUNT = 500;
        constexpr int TEST_N = 100;
        constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
        constexpr int NO_TIME_LIMIT_SEC = -1;
        GreedyConstructorParams params{10, 10, 10, 10, 10, 20, 15, 3};
        int do_cnt = 6;
        vector<long long> score_sums(do_cnt + 1);
        vector<vector<int>> scores(do_cnt + 1);
        for (int case_idx = 0; case_idx < CASE_COUNT; case_idx++) {
            set_my_rand_seed(static_cast<uint32_t>(INIT_BASE_SEED + case_idx));
            std::vector<int> perm = build_repeated_test_perm(TEST_N);
            State init_state = build_repeated_test_state(perm);
            GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
            GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
            set_worst_half_enabled(false);
            set_b_move_enabled(false);
            set_aorder_range_enabled(false);
            set_my_rand_seed(static_cast<uint32_t>(ALNS_SEED));
            YstFactory<MAX_N, HABA, K> factory;
            YakinamashiState<MAX_N> base_current_yst = factory.build(init_state);
            auto get_score = [&](int do_i) {
                cout << "do_i " << do_i << endl;
                auto current_yst = base_current_yst;
                return run_alns_destroy_construct<MAX_N, HABA, K>(
                        init_state,
                        current_yst,
                        ITER_COUNT,
                        ACCEPT_MODE,
                        params,
                        NO_TIME_LIMIT_SEC,
                        true,
                        false);
            };
            vector<int> score_vec;
            for (int i = 0; i < do_cnt; i++) score_vec.push_back(get_score(i));
            vector<int> score_vals(do_cnt + 1);
            for (int i = 1; i < do_cnt + 1; i++) {
                score_vals[i] = *min_element(score_vec.begin(), score_vec.begin() + i);
                score_sums[i] += score_vals[i];
                scores[i].push_back(score_vals[i]);


                const double score_avg = static_cast<double>(score_sums[i]) / (case_idx + 1);
                std::sort(scores[i].begin(), scores[i].end());
                cout << "owner_single"
                     << " case=" << case_idx
                     << " score=" << score_sums[i]
                     << " score_avg=" << score_avg
                     << "score_min=" << scores[i][0]
                     << " score_med=" << scores[i][scores[i].size() / 2]
                     << "score_max=" << scores[i].back()
                     << endl;
            }


        }
        set_aorder_range_enabled(false);
        set_b_move_enabled(false);
        set_worst_half_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    }

    template<size_t MAX_N, int HABA, int K>
    void run_any_seed_1case() {
        constexpr int SEED_CASE_COUNT = 500;
        constexpr int INIT_BASE_SEED = 2026041451;
        constexpr int ALNS_BASE_SEED = 2026041451;
        constexpr int ITER_COUNT = 500;
        constexpr int TEST_N = 100;
        constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
        constexpr int NO_TIME_LIMIT_SEC = -1;
        GreedyConstructorParams params{10, 10, 10, 10, 10, 20, 15, 3};
        std::vector<int> perm = build_repeated_test_perm(TEST_N);
        long long score_sum = 0;
        vector<int> scores;
        for (int case_idx = 0; case_idx < SEED_CASE_COUNT; case_idx++) {
            cout << "run_any_seed_1case" << endl;
            State init_state = build_repeated_test_state(perm);
            GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
            GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
            set_worst_half_enabled(false);
            set_b_move_enabled(false);
            set_aorder_range_enabled(false);
            set_my_rand_seed(static_cast<uint32_t>(ALNS_BASE_SEED + case_idx));
            YstFactory<MAX_N, HABA, K> factory;
            YakinamashiState<MAX_N> current_yst = factory.build(init_state);
            const int score = run_alns_destroy_construct<MAX_N, HABA, K>(
                    init_state,
                    current_yst,
                    ITER_COUNT,
                    ACCEPT_MODE,
                    params,
                    NO_TIME_LIMIT_SEC,
                    true,
                    false);
            score_sum += score;
            scores.push_back(score);
            const double score_avg = static_cast<double>(score_sum) / (case_idx + 1);
            std::sort(scores.begin(), scores.end());
            cout << "owner_single"
                 << " case=" << case_idx
                 << " score=" << score
                 << " score_avg=" << score_avg
                 << "score_min=" << scores[0]
                 << " score_med=" << scores[scores.size() / 2]
                 << "score_max=" << scores.back()
                 << endl;
            cout << scores << endl;
        }
        cout << "owner_single_summary"
             << " cases=" << SEED_CASE_COUNT
             << " score_avg=" << (static_cast<double>(score_sum) / SEED_CASE_COUNT)
             << endl;
        set_aorder_range_enabled(false);
        set_b_move_enabled(false);
        set_worst_half_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    }

    struct LisStartRow {
        int zero_init;
        int best_init;
        int zero_final;
        int best_final;
        int best_lis_start;
    };

    struct ScoreStats {
        int min_score;
        double median_score;
        double avg_score;
        int max_score;
    };

    ScoreStats calc_score_stats(const vector<int> &scores) {
        my_assert(!scores.empty());
        vector<int> sorted = scores;
        std::sort(sorted.begin(), sorted.end());
        long long sum = 0;
        for (int score: sorted) {
            sum += score;
        }
        double median;
        int n = static_cast<int>(sorted.size());
        if (n % 2 == 1) {
            median = sorted[n / 2];
        } else {
            median = (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
        }
        return {sorted.front(), median, static_cast<double>(sum) / n, sorted.back()};
    }

    std::ofstream &lis_start_compare_stream() {
        static std::ofstream log_file;
        return log_file;
    }

    void reset_lis_start_compare_file() {
        std::ofstream &log_file = lis_start_compare_stream();
        if (log_file.is_open()) log_file.close();
        std::filesystem::create_directories(output_dir);
        log_file.open(output_dir / "lis_start_compare.txt", std::ios::trunc);
        if (!log_file.is_open()) {
            std::cerr << "failed to open lis_start_compare.txt" << std::endl;
            std::abort();
        }
    }

    void print_lis_start_line(const std::string &line) {
        cout << line << endl;
        out3(line);
        std::ofstream &log_file = lis_start_compare_stream();
        if (log_file.is_open()) {
            log_file << line << endl;
        }
    }

    void print_score_stats(const std::string &name, const ScoreStats &stats) {
        std::stringstream ss;
        ss << name
           << " min=" << stats.min_score
           << " median=" << stats.median_score
           << " avg=" << stats.avg_score
           << " max=" << stats.max_score;
        print_lis_start_line(ss.str());
    }

    // 責務: array overlap かつ unit range の owner collector だけを 2000 case 実行し、各 case 時点の score 統計を出す。
    // 必要な理由: 他の owner 条件を混ぜず、array + unit 条件の min/median/avg/max 手数を継続的に確認するため。
    template<size_t MAX_N, int HABA, int K>
    void run_owner_array_unit_100() {
        constexpr int CASE_COUNT = 2000;
        constexpr int INIT_BASE_SEED = 2026041451;
        constexpr int ALNS_SEED = 2026041451;
        // human_review_begin: 同じ初期解から 200 回を 2 本試し、良い方に 300 回追加するため。
        constexpr int FIRST_ITER_COUNT = 200;
        constexpr int EXTRA_ITER_COUNT = 300;
        // human_review_end
        constexpr int TEST_N = 100;
        // human_review_begin: run_owner_array_unit_100 でも 500 側と同じ lis_start range 比較を使うため。
        constexpr int LIS_RANGE = 20;
        constexpr int NARROW_HABA = 500;
        // human_review_end
        // human_review_begin: run_owner_array_unit_100 の ALNS 結果へ range optimize を適用するため。
        constexpr int OPT_RANGE_SIZE = 9;
        constexpr int OPT_LEFT = 0;
        // human_review_end
//        constexpr int TEST_N = 20;
        constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
        constexpr int NO_TIME_LIMIT_SEC = -1;
        //a7-7より10-10のほうがよかった(時間変わらない)2手
        //        GreedyConstructorParams params{10, 5, 5, 5, 5, 20, 15, 3};
        GreedyConstructorParams params{10, 5, 5, 5, 5, 20, 24, 3};
//        GreedyConstructorParams params{10, 5, 5, 5, 5, 10, 8, 3};
//        GreedyConstructorParams params{3, 3, 3, 2, 2, 6, 5, 3};
        vector<int> scores;
        scores.reserve(CASE_COUNT);
        GreedyQueriesConstructor<MAX_N>::set_timing_log_enabled(false);
        for (int case_idx = 0; case_idx < CASE_COUNT; case_idx++) {
            set_my_rand_seed(static_cast<uint32_t>(INIT_BASE_SEED + case_idx));
            std::vector<int> perm = build_repeated_test_perm(TEST_N);
            State init_state = build_repeated_test_state(perm);
            GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
            GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
            GreedyQueriesConstructor<MAX_N>::set_owner_array_enabled(true);
            GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(true);
            GreedyQueriesConstructor<MAX_N>::set_owner_even_unit_enabled(true);
            GreedyQueriesConstructor<MAX_N>::set_pair_refine_enabled(true);
            GreedyQueriesConstructor<MAX_N>::set_pair_refine_params(6, 1, 1, 3);
            set_worst_half_enabled(false);
            set_b_move_enabled(false);
            set_aorder_range_enabled(false);
            set_my_rand_seed(static_cast<uint32_t>(ALNS_SEED));
            YstFactory<MAX_N, HABA, K> factory;
            // human_review_begin: 細い幅で複数 lis_start を比較し、選択した初期解から ALNS を始めるため。
            auto build_result = factory.template build_lis_range_score<NARROW_HABA>(init_state, LIS_RANGE, false);
            YakinamashiState<MAX_N> current_yst = std::move(build_result.yst);
            // human_review_end
            // human_review_begin: 同じ初期解から 2 本の ALNS 候補を作り、200 回時点の score で採用候補を選ぶため。
            YakinamashiState<MAX_N> first_yst = current_yst;
            YakinamashiState<MAX_N> second_yst = current_yst;
            const int first_score = run_alns_destroy_construct<MAX_N, HABA, K>(
                    init_state,
                    first_yst,
                    FIRST_ITER_COUNT,
                    ACCEPT_MODE,
                    params,
                    NO_TIME_LIMIT_SEC,
                    false,
                    false);
            const int second_score = run_alns_destroy_construct<MAX_N, HABA, K>(
                    init_state,
                    second_yst,
                    FIRST_ITER_COUNT,
                    ACCEPT_MODE,
                    params,
                    NO_TIME_LIMIT_SEC,
                    false,
                    false);
            YakinamashiState<MAX_N> best_yst = (first_score <= second_score)
                                               ? std::move(first_yst)
                                               : std::move(second_yst);
            // human_review_end
            // human_review_begin: 200 回時点で良かった候補から、さらに 300 回 ALNS を続けるため。
            const int extra_score = run_alns_destroy_construct<MAX_N, HABA, K>(
                    init_state,
                    best_yst,
                    EXTRA_ITER_COUNT,
                    ACCEPT_MODE,
                    params,
                    NO_TIME_LIMIT_SEC,
                    false,
                    false);
            (void) extra_score;
            // human_review_end
            // human_review_begin: 500 側と同様に ALNS 後 command を range optimize し、その結果を集計 score にするため。
            State optimized_state = build_owner_optimized_state(init_state, best_yst);
            optimize_all_range(optimized_state, OPT_RANGE_SIZE, OPT_LEFT);
            const int score = static_cast<int>(optimized_state.get_cmd_list().list.size());
            // human_review_end
            scores.push_back(score);
            const ScoreStats stats = calc_score_stats(scores);
            cout << "owner_array_unit"
                 << " case=" << case_idx
                 << " score=" << score
                 << " min=" << stats.min_score
                 << " median=" << stats.median_score
                 << " avg=" << stats.avg_score
                 << " max=" << stats.max_score
                 << endl;
        }
        const ScoreStats summary = calc_score_stats(scores);
        cout << "owner_array_unit_summary"
             << " cases=" << CASE_COUNT
             << " min=" << summary.min_score
             << " median=" << summary.median_score
             << " avg=" << summary.avg_score
             << " max=" << summary.max_score
             << endl;
        set_aorder_range_enabled(false);
        set_b_move_enabled(false);
        set_worst_half_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_array_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(true);
        GreedyQueriesConstructor<MAX_N>::set_owner_even_unit_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_pair_refine_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    }

    //実際はmapで、unitではない
    template<size_t MAX_N, int HABA, int K>
    void run_owner_array_unit_500() {
        constexpr int CASE_COUNT = 2000;
        constexpr int INIT_BASE_SEED = 2026041451;
        constexpr int ALNS_SEED = 2026041451;
        constexpr int ITER_COUNT = 150;
        constexpr int EXTRA_ITER_COUNT = 50;
        constexpr int EXTRA_SCORE_THRESHOLD = 3000;
        constexpr int TEST_N = 500;
        constexpr int LIS_RANGE = 50;
        constexpr int NARROW_HABA = 200;
        constexpr int OPT_RANGE_SIZE = 7;
        constexpr int OPT_LEFT = 0;
        constexpr bool WRITE_BEST_CMD_LOG = true;
//        constexpr int TEST_N = 20;
        constexpr AlnsAcceptMode ACCEPT_MODE = AlnsAcceptMode::HillClimbing;
        constexpr int NO_TIME_LIMIT_SEC = -1;
        //a7-7より10-10のほうがよかった(時間変わらない)2手
//        GreedyConstructorParams params{10, 10, 10, 10, 10, 30, 22, 3};
        GreedyConstructorParams params{10, 4, 4, 4, 4, 25, 24, 3};
        vector<int> scores;
        scores.reserve(CASE_COUNT);
        std::ofstream best_cmd_log;
        if constexpr (WRITE_BEST_CMD_LOG) {
            std::filesystem::create_directories(output_dir);
            best_cmd_log.open(output_dir / "owner_array_unit_500_best_cmds.txt", std::ios::out | std::ios::trunc);
        }
        std::filesystem::create_directories(output_dir);
        std::ofstream stats_log(output_dir / "owner_array_unit_500_stats.txt", std::ios::out | std::ios::trunc);
        GreedyQueriesConstructor<MAX_N>::set_timing_log_enabled(false);
        for (int case_idx = 0; case_idx < CASE_COUNT; case_idx++) {
            set_my_rand_seed(static_cast<uint32_t>(INIT_BASE_SEED + case_idx));
            std::vector<int> perm = build_repeated_test_perm(TEST_N);
            State init_state = build_repeated_test_state(perm);
            if constexpr (WRITE_BEST_CMD_LOG) {
                write_owner_case_init_log(best_cmd_log, case_idx, init_state);
            }
            GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
            GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
            GreedyQueriesConstructor<MAX_N>::set_owner_array_enabled(false);
            GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(false);
            GreedyQueriesConstructor<MAX_N>::set_owner_even_unit_enabled(false);
            GreedyQueriesConstructor<MAX_N>::set_pair_refine_enabled(false);
            set_worst_half_enabled(false);
            set_b_move_enabled(false);
            set_aorder_range_enabled(false);
            set_my_rand_seed(static_cast<uint32_t>(ALNS_SEED));
            YstFactory<MAX_N, HABA, K> factory;
//            YakinamashiState<MAX_N> current_yst = factory.build(init_state);
            auto build_result = factory.template build_lis_range_score<NARROW_HABA>(init_state, LIS_RANGE, false);
            YakinamashiState<MAX_N> current_yst = std::move(build_result.yst);
            int alns_score = run_alns_destroy_construct<MAX_N, HABA, K>(
                    init_state,
                    current_yst,
                    ITER_COUNT,
                    ACCEPT_MODE,
                    params,
                    NO_TIME_LIMIT_SEC,
                    false,
                    false, WRITE_BEST_CMD_LOG ? &best_cmd_log : nullptr
            );
            if (alns_score >= EXTRA_SCORE_THRESHOLD) {
                alns_score = run_alns_destroy_construct<MAX_N, HABA, K>(
                        init_state,
                        current_yst,
                        EXTRA_ITER_COUNT,
                        ACCEPT_MODE,
                        params,
                        NO_TIME_LIMIT_SEC,
                        false,
                        false,
                        WRITE_BEST_CMD_LOG ? &best_cmd_log : nullptr
                );
            }
            State optimized_state = build_owner_optimized_state(init_state, current_yst);
            optimize_all_range(optimized_state, OPT_RANGE_SIZE, OPT_LEFT);
            const int score = static_cast<int>(optimized_state.get_cmd_list().list.size());
            if constexpr (WRITE_BEST_CMD_LOG) {
                write_owner_optimized_cmd_log(best_cmd_log, case_idx, alns_score, optimized_state);
            }
            scores.push_back(score);
            const ScoreStats stats = calc_score_stats(scores);
            stats_log << "case=" << case_idx
                      << " score=" << score
                      << " min=" << stats.min_score
                      << " median=" << stats.median_score
                      << " max=" << stats.max_score
                      << '\n';
            cout << "owner_array_unit"
                 << " case=" << case_idx
                 << " score=" << score
                 << " min=" << stats.min_score
                 << " median=" << stats.median_score
                 << " avg=" << stats.avg_score
                 << " max=" << stats.max_score
                 << endl;
        }
        const ScoreStats summary = calc_score_stats(scores);
        cout << "owner_array_unit_summary"
             << " cases=" << CASE_COUNT
             << " min=" << summary.min_score
             << " median=" << summary.median_score
             << " avg=" << summary.avg_score
             << " max=" << summary.max_score
             << endl;
        set_aorder_range_enabled(false);
        set_b_move_enabled(false);
        set_worst_half_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_array_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(true);
        GreedyQueriesConstructor<MAX_N>::set_owner_even_unit_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_pair_refine_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_timing_log_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
        GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    }

//    void test_lis_start_compare_50() {
//        constexpr int COMPARE_N = 100;
//        constexpr int CASE_COUNT = 20;
//        constexpr int HABA = 3000;
//        constexpr int K = 2;
//        constexpr int ITER_COUNT = 200;
//        constexpr bool USE_LAHC = true;
//        constexpr int TIME_LIMIT_SEC = 100000;
//        constexpr int BASE_SEED = 2026041451;
//        GreedyConstructorParams params{10, 10, 10, 20, 15};
////        GreedyConstructorParams params{10, 5, 5, 10, 8};
//        reset_lis_start_compare_file();
//
//        vector<LisStartRow> rows;
//        rows.reserve(CASE_COUNT);
//        for (int case_idx = 0; case_idx < CASE_COUNT; case_idx++) {
//            cout << case_idx << endl;
//            int case_seed = BASE_SEED + case_idx;
//            vector<int> perm(COMPARE_N);
//            std::iota(perm.begin(), perm.end(), 0);
//            std::mt19937 engine(case_seed);
//            std::shuffle(perm.begin(), perm.end(), engine);
//            State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
//
//            YstFactory<COMPARE_N, HABA, K> zero_factory;
//            set_my_rand_seed(static_cast<uint32_t>(case_seed));
//            auto zero_build = zero_factory.build_with_score(init_state, false, false);
//            set_my_rand_seed(static_cast<uint32_t>(case_seed + 1000000));
//            int zero_final = run_alns_destroy_construct<COMPARE_N, HABA, K>(
//                    init_state, zero_build.yst, ITER_COUNT, USE_LAHC, params, TIME_LIMIT_SEC);
//
//            YstFactory<COMPARE_N, HABA, K> best_factory;
//            set_my_rand_seed(static_cast<uint32_t>(case_seed));
//            auto best_build = best_factory.build_with_score(init_state, true, false);
//            set_my_rand_seed(static_cast<uint32_t>(case_seed + 1000000));
//            int best_final = run_alns_destroy_construct<COMPARE_N, HABA, K>(
//                    init_state, best_build.yst, ITER_COUNT, USE_LAHC, params, TIME_LIMIT_SEC);
//
//            LisStartRow row{
//                    zero_build.init_score,
//                    best_build.init_score,
//                    zero_final,
//                    best_final,
//                    best_build.selected_lis_start
//            };
//            rows.push_back(row);
//
//            std::stringstream ss;
//            ss << "lis_start_case idx=" << case_idx
//               << " best_lis_start=" << row.best_lis_start
//               << " zero_init=" << row.zero_init
//               << " best_init=" << row.best_init
//               << " zero_final=" << row.zero_final
//               << " best_final=" << row.best_final;
//            print_lis_start_line(ss.str());
//        }
//
//        vector<int> zero_init_scores;
//        vector<int> best_init_scores;
//        vector<int> zero_final_scores;
//        vector<int> best_final_scores;
//        zero_init_scores.reserve(rows.size());
//        best_init_scores.reserve(rows.size());
//        zero_final_scores.reserve(rows.size());
//        best_final_scores.reserve(rows.size());
//        for (const LisStartRow &row: rows) {
//            zero_init_scores.push_back(row.zero_init);
//            best_init_scores.push_back(row.best_init);
//            zero_final_scores.push_back(row.zero_final);
//            best_final_scores.push_back(row.best_final);
//        }
//
//        print_score_stats("zero_init_stats", calc_score_stats(zero_init_scores));
//        print_score_stats("best_init_stats", calc_score_stats(best_init_scores));
//        print_score_stats("zero_final_stats", calc_score_stats(zero_final_scores));
//        print_score_stats("best_final_stats", calc_score_stats(best_final_scores));
//    }

    // : ALNS 実験で固定初期状態を再利用し、destroyer 比較時の条件差を抑えるため固定 perm を helper 化する。
    std::vector<int> build_repeated_fixed_perm_100() {
        return {
                36, 37, 41, 85, 84, 13, 33, 43, 76, 78, 92, 61, 22, 10, 83, 81, 89, 34, 90, 29, 45, 55, 51, 86, 50, 68,
                95, 94, 25, 79, 75, 42, 96, 97, 4, 64, 52, 11, 47, 91, 28, 14, 60, 35, 87, 57, 19, 99, 69, 53,
                48, 59, 1, 98, 7, 8, 24, 65, 9, 88, 12, 38, 21, 31, 58, 0, 17, 82, 77, 27, 66, 40, 71, 72, 93, 5, 80,
                26, 18, 20, 3, 30, 63, 44, 54, 62, 6, 70, 67, 56, 16, 39, 15, 74, 2, 23, 32, 46, 73, 49
        };
    }

    void print_repeated_score_log(
            int iter,
            int destroy_size,
            int destroyed_cnt,
            int before_score,
            int after_destroy_score,
            int after_construct_score,
            const my_vector<command> &cmds
    ) {
        std::stringstream ss;
        ss << "iter=" << iter
           << " mode=repeated"
           << " destroy_size=" << destroy_size
           << " destroyed_vs=" << destroyed_cnt
           << " before=" << before_score
           << " after_destroy=" << after_destroy_score
           << " after_construct=" << after_construct_score
           << " destroy_diff=" << (after_destroy_score - before_score)
           << " construct_diff=" << (after_construct_score - after_destroy_score)
           << " total_diff=" << (after_construct_score - before_score);
        out(ss.str());
        out(std::string("iter_cmds=") + repeated_cmds_to_string(cmds));
    }

    // : 同じ initial_state を共有した比較ログだと分かるように、case 境界を明示するため。
    void print_repeated_case_begin(const State &init_st, int destroy_size) {
        out(std::string("case_begin destroy_size=") + std::to_string(destroy_size));
        out(std::string("shared_initial_state=") + repeated_state_to_string(init_st.A));
    }

    // : 同じ initial_state 上の case 終端を明示して、output.txt をサイズごとに追いやすくするため。
    void print_repeated_case_end(int destroy_size) {
        out(std::string("case_end destroy_size=") + std::to_string(destroy_size));
    }

    // : destroyer ごとの破壊条件をログへ出して、score 推移と destroy 形状を対応付けて読めるようにするため。
    template<size_t MAX_N>
    std::string repeated_destroyer_info_to_string(const RangeQueryDestroyer<MAX_N> &, int destroy_size) {
        return std::string("destroyer=range destroy_size=") + std::to_string(destroy_size);
    }

    // : random tar_v range destroy では選ばれた circular 範囲を出して、どの v 帯を壊したか追えるようにするため。
    template<size_t MAX_N>
    std::string repeated_destroyer_info_to_string(const RandomTarVRangeDestroyer<MAX_N> &destroyer, int) {
        std::stringstream ss;
        ss << "destroyer=random_tar_v_range target_v_range_size=" << destroyer.get_target_v_range_size()
           << " start_v=" << destroyer.get_last_start_v()
           << " end_v_exclusive=" << destroyer.get_last_end_v_exclusive();
        return ss.str();
    }

    // : v 寄与 destroy では top-m 候補と実際の選択結果を出して、なぜその v が壊されたか読めるようにするため。
    template<size_t MAX_N>
    std::string
    repeated_destroyer_info_to_string(const HighImpactVTopMRandomDestroyer<MAX_N> &destroyer, int destroy_size) {
        std::stringstream ss;
        ss << "destroyer=high_impact_v_top_m_random"
           << " destroy_size=" << destroy_size
           << " select_v_count=" << destroyer.get_select_v_count()
           << " candidate_top_m=" << destroyer.get_candidate_top_m()
           << " selected_vs=" << repeated_ints_to_string(destroyer.get_last_selected_vs())
           << " top_candidates=" << repeated_v_decreases_to_string(destroyer.get_last_top_candidates());
        return ss.str();
    }

    // : 既存 repeated テストを小さい range destroy 用へ戻し、destroy size だけ主変数として比べられるようにするため。
    template<size_t MAX_N, int HABA, int K, class Destroyer>
    void run_destroy_construct_once(
            const State &init_st,
            YakinamashiState<MAX_N> &yst,
            Destroyer &destroyer,
            GreedyQueriesConstructor<MAX_N> &constructor,
            int iter,
            int destroy_size
    ) {
        // _BEGIN: 再構築が悪化した時に元へ戻せるよう、iter 開始時点の完成状態を保持する。
        YakinamashiState<MAX_N> before_yst = yst;
        // _END
        int before_score = get_repeated_test_score(yst.queries_state);
        my_vector<std::pair<int, int>> destroyed_vs = destroyer.destroy(yst);
        int after_destroy_score = get_repeated_test_score(yst.queries_state);
        out(repeated_destroyer_info_to_string(destroyer, destroy_size));
        // _BEGIN: output.txt と greedy trace を iter 単位で対応付け、後段挿入での悪化位置を拾いやすくする。
        GreedyQueriesConstructor<MAX_N>::append_trace_marker("");
        GreedyQueriesConstructor<MAX_N>::append_trace_marker(
                "iter_begin destroy_size=" + std::to_string(destroy_size) +
                " iter=" + std::to_string(iter) +
                " before=" + std::to_string(before_score) +
                " after_destroy=" + std::to_string(after_destroy_score) +
                " destroyed_vs=" + repeated_destroyed_vs_to_string(destroyed_vs)
        );
        // _END
        if constexpr (USE_WORST_HALF_REBUILD_CONSTRUCTOR) {
            constructor.construct_worst_half_rebuild(yst, destroyed_vs);
        } else {
            constructor.construct(yst, destroyed_vs);
        }
        // _BEGIN: trace を見ない実験でも construct の探索量を output.txt から追えるようにする。
        print_construct_profile_lines_to_output<MAX_N>(
                "iter=" + std::to_string(iter) + " destroy_size=" + std::to_string(destroy_size)
        );
        // _END
        QueriesStateValidator<MAX_N>::validate_state(init_st, yst);
        int after_construct_score = get_repeated_test_score(yst.queries_state);
        // _BEGIN: destroy+construct が悪化した場合は before 状態へ戻し、反復で悪化を持ち越さないようにする。
        bool reverted_to_before = false;
        if (after_construct_score > before_score) {
            yst = before_yst;
            after_construct_score = before_score;
            reverted_to_before = true;
        }
        // _END
        my_vector<command> cmds = build_reconstructed_cmds(init_st, yst.queries_state);
        print_repeated_score_log < MAX_N, HABA, K > (
                iter, destroy_size, static_cast<int>(destroyed_vs.size()),
                        before_score, after_destroy_score, after_construct_score, cmds
        );
        // _BEGIN: 出力だけで revert の有無を判断できるようにする。
        out(std::string("reverted_to_before=") + (reverted_to_before ? "1" : "0"));
        // _END
    }

    // : 既存 repeated テストの形は保ったまま、main から destroy size を差し替えられるようにするため。
    template<size_t MAX_N, int HABA, int K, class Destroyer>
    void
    run_repeated_destroy_construct_case(const State &init_st, Destroyer &destroyer, int repeat_n, int destroy_size) {
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState<MAX_N> yst = factory.build(init_st);
        GreedyConstructorParams params{10, 24, 24, 24, 24, 1 << 30, 25};
        GreedyQueriesConstructor<MAX_N> constructor(init_st, params);
        my_vector<command> initial_cmds = build_reconstructed_cmds(init_st, yst.queries_state);
        // _BEGIN: case 開始点の初期 score を trace にも残し、iter 0 の construct 直前状態を固定する。
        GreedyQueriesConstructor<MAX_N>::append_trace_marker("");
        GreedyQueriesConstructor<MAX_N>::append_trace_marker(
                "case_trace_begin destroy_size=" + std::to_string(destroy_size) +
                " initial_score=" + std::to_string(get_repeated_test_score(yst.queries_state))
        );
        // _END
        out(std::string("initial_state=") + repeated_state_to_string(init_st.A));
        out(std::string("initial_score=") + std::to_string(get_repeated_test_score(yst.queries_state)));
        out(std::string("initial_cmds=") + repeated_cmds_to_string(initial_cmds));
        for (int iter = 0; iter < repeat_n; iter++) {
            run_destroy_construct_once<MAX_N, HABA, K>(init_st, yst, destroyer, constructor, iter, destroy_size);
        }
    }
}

// : 既存 repeated destroy テストで、小さい range destroy の size 指定だけを外から変えられるようにするため。
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_repeated_destroy_construct(int destroy_size = -1) {
    constexpr int TEST_N = 100;
    constexpr int REPEAT_N = 50;
    // _BEGIN: 単体実行時も古い trace と混ざらないように先頭で掃除して無効化する。
    GreedyQueriesConstructor<MAX_N>::clear_trace_log();
    GreedyQueriesConstructor<MAX_N>::set_trace_enabled(false);
    // _END
    std::vector<int> perm = build_repeated_test_perm(TEST_N);
    State init_state = build_repeated_test_state(perm);
    RangeQueryDestroyer<MAX_N> destroyer(init_state, destroy_size);
    run_repeated_destroy_construct_case<MAX_N, HABA, K>(init_state, destroyer, REPEAT_N, destroy_size);
    // _BEGIN: 他テストへ trace が漏れないように単体実行の末尾で止める。
    GreedyQueriesConstructor<MAX_N>::set_trace_enabled(false);
    // _END
}

// : 同じ initial_state に対する destroy size 比較を 1 実行で揃えて、サイズ差だけを見やすくするため。
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_repeated_destroy_construct_multi_sizes(const std::vector<int> &destroy_sizes) {
    // _BEGIN: 現状確認では長時間実行を避け、順序変更後の健全性だけを短時間で見たい。
    constexpr int TEST_N = 100;
    constexpr int REPEAT_N = 200;
    // _END
    // _BEGIN: 複数 size 比較でも trace を今回の実行だけに限定し、case 間比較時も無効化する。
    GreedyQueriesConstructor<MAX_N>::clear_trace_log();
    GreedyQueriesConstructor<MAX_N>::set_trace_enabled(false);
    // _END
    std::vector<int> perm = build_repeated_test_perm(TEST_N);
    State init_state = build_repeated_test_state(perm);
    for (int destroy_size: destroy_sizes) {
        print_repeated_case_begin(init_state, destroy_size);
        RangeQueryDestroyer<MAX_N> destroyer(init_state, destroy_size);
        run_repeated_destroy_construct_case<MAX_N, HABA, K>(init_state, destroyer, REPEAT_N, destroy_size);
        print_repeated_case_end(destroy_size);
    }
    // _BEGIN: 比較実験の後に trace を止め、他 run の混入を防ぐ。
    GreedyQueriesConstructor<MAX_N>::set_trace_enabled(false);
    // _END
}

// : random tar_v range destroy を既存の repeated テスト枠で比較できるようにするため。
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_repeated_random_tar_v_range_destroy_construct(int target_v_range_size = 6) {
    constexpr int TEST_N = 100;
    constexpr int REPEAT_N = 200;
    GreedyQueriesConstructor<MAX_N>::clear_trace_log();
    GreedyQueriesConstructor<MAX_N>::set_trace_enabled(false);
    std::vector<int> perm =
            {
                    36, 37, 41, 85, 84, 13, 33, 43, 76, 78, 92, 61, 22, 10, 83, 81, 89, 34, 90, 29, 45, 55, 51, 86, 50,
                    68, 95, 94, 25, 79, 75, 42, 96, 97, 4, 64, 52, 11, 47, 91, 28, 14, 60, 35, 87, 57, 19, 99, 69, 53,
                    48, 59, 1, 98, 7, 8, 24, 65, 9, 88, 12, 38, 21, 31, 58, 0, 17, 82, 77, 27, 66, 40, 71, 72, 93, 5,
                    80, 26, 18, 20, 3, 30, 63, 44, 54, 62, 6, 70, 67, 56, 16, 39, 15, 74, 2, 23, 32, 46, 73, 49
            };
//            build_repeated_test_perm(TEST_N);
    State init_state = build_repeated_test_state(perm);
    print_repeated_case_begin(init_state, target_v_range_size);
    RandomTarVRangeDestroyer<MAX_N> destroyer(init_state, target_v_range_size);
    run_repeated_destroy_construct_case<MAX_N, HABA, K>(init_state, destroyer, REPEAT_N, target_v_range_size);
    print_repeated_case_end(target_v_range_size);
    GreedyQueriesConstructor<MAX_N>::set_trace_enabled(false);
}

// : v 寄与を見て top-m random で壊す destroyer を既存 repeated テスト枠で試せるようにするため。
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_repeated_high_impact_v_top_m_random_destroy_construct(int select_v_count = 6, int candidate_top_m = 12) {
    constexpr int TEST_N = 100;
    constexpr int REPEAT_N = 200;
    GreedyQueriesConstructor<MAX_N>::clear_trace_log();
    GreedyQueriesConstructor<MAX_N>::set_trace_enabled(false);
    std::vector<int> perm =
            {
                    36, 37, 41, 85, 84, 13, 33, 43, 76, 78, 92, 61, 22, 10, 83, 81, 89, 34, 90, 29, 45, 55, 51, 86, 50,
                    68, 95, 94, 25, 79, 75, 42, 96, 97, 4, 64, 52, 11, 47, 91, 28, 14, 60, 35, 87, 57, 19, 99, 69, 53,
                    48, 59, 1, 98, 7, 8, 24, 65, 9, 88, 12, 38, 21, 31, 58, 0, 17, 82, 77, 27, 66, 40, 71, 72, 93, 5,
                    80, 26, 18, 20, 3, 30, 63, 44, 54, 62, 6, 70, 67, 56, 16, 39, 15, 74, 2, 23, 32, 46, 73, 49
            };
//            build_repeated_test_perm(TEST_N);
    State init_state = build_repeated_test_state(perm);
    print_repeated_case_begin(init_state, select_v_count);
    HighImpactVTopMRandomDestroyer<MAX_N> destroyer(init_state, select_v_count, candidate_top_m);
    run_repeated_destroy_construct_case<MAX_N, HABA, K>(init_state, destroyer, REPEAT_N, select_v_count);
    print_repeated_case_end(select_v_count);
    GreedyQueriesConstructor<MAX_N>::set_trace_enabled(false);
}

// : destroyer arm を ALNS + 制限付きLAHC(L=500) で選択し、受理判定と重み更新を観察する。
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_alns_destroyers_with_lahc_l10(
        const State &init_state,
        const GreedyConstructorParams &params,
        int iter_n
) {
//    constexpr int ITER_N = 1<<30;
    constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
    constexpr int NO_TIME_LIMIT_SEC = -1;
    {
//        my_assert(init_state.current_N == TEST_N);
        GreedyQueriesConstructor<MAX_N>::clear_trace_log();
        GreedyQueriesConstructor<MAX_N>::set_trace_enabled(false);
        YstFactory<MAX_N, HABA, K> factory;
        YakinamashiState<MAX_N> current_yst = factory.build(init_state);
        run_alns_destroy_construct<MAX_N, HABA, K>(
                init_state, current_yst, iter_n, ACCEPT_MODE, params, -1, true, false
        );
        write_alns_result(init_state, current_yst);
        return;
    }
}

//template<size_t MAX_N, int HABA = 3, int K = 2>
//void test_alns_destroyers_with_lahc_l10() {
//    constexpr int TEST_N = 500;
//    std::vector<int> perm = build_repeated_test_perm(TEST_N);
//    State init_state = build_repeated_test_state(perm);
//    GreedyConstructorParams params{10, 24, 24, 1 << 30, 25};
//    test_alns_destroyers_with_lahc_l10<MAX_N, HABA, K>(init_state, params, 1<<30);
//}

template<size_t MAX_N, int HABA = 3, int K = 2>
void alns_iterative_greedy(const State &init_state) {
    int N = init_state.initial_N;
    my_assert(N == init_state.current_N);
    if (N < 100) {
        GreedyConstructorParams params{5, 2, 2, 2, 2, 2, 4};
        test_alns_destroyers_with_lahc_l10<MAX_N, HABA, K>(init_state, params, 200);
    }
    if (N < 500) {
//        GreedyConstructorParams params{10, 24, 24, 1 << 30, 25};
//        GreedyConstructorParams params{10, 8, 8, 10, 10};
//        GreedyConstructorParams params{5, 4, 8, 2, 28};
        GreedyConstructorParams params{10, 10, 10, 10, 10, 20, 15};
        test_alns_destroyers_with_lahc_l10<MAX_N, HABA, K>(init_state, params, 1700);
    } else if (N >= 500) {
        GreedyConstructorParams params{10, 24, 24, 24, 24, 1 << 30, 25};
        test_alns_destroyers_with_lahc_l10<MAX_N, HABA, K>(init_state, params, 100);
    } else {
        assert(false);
    }
}


// : A2AP を A2B+B2A へ変換した YstFactory build が random 100 case 群でも validator を通るかを一括確認する。
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_random_yst_factory_build_validate_100x500() {
    constexpr int TEST_N = 100;
    constexpr int CASE_N = 500;
    out("=== test_random_yst_factory_build_validate_100x500 ===");
    YstFactory<MAX_N, HABA, K> factory;
    for (int case_idx = 0; case_idx < CASE_N; case_idx++) {
        std::vector<int> perm = build_repeated_test_perm(TEST_N);
        State init_state = build_repeated_test_state(perm);
        YakinamashiState<MAX_N> yst = factory.build(init_state);
        QueriesStateValidator<MAX_N>::validate_state(init_state, yst);
        if (case_idx % 50 == 0) {
            out("validate_case", case_idx, "sum_turn", yst.queries_state.sum_turn);
        }
    }
    out("test_random_yst_factory_build_validate_100x500 PASSED");
}
