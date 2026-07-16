// YstFactory.h
// BeamQuery列からYakinamashiStateを生成するファクトリ
#pragma once

#include "State.h"
#include "QueryCommon.h"
#include "YakinamashiQueryConstants.h"
#include "QueriesState.h"
#include "YakinamashiState.h"
#include "YQAliases.h"
#include "YstQueriesBuildPipeline.h"
#include "Util/FinalRotCalculator.h"
#include "sort_algorithm/ChunkAndBeamSolver.h"
#include "sort_algorithm/Yakinamashi/Order/AOrder.h"
#include "sort_algorithm/Yakinamashi/Order/BOrder.h"
#include <bitset>
#include <utility>
#include <vector>

#ifdef DEBUG
#include "YakinamashiStateValidator.h"
#endif

//x YstFactory: YakinamashiQuery用の初期化パイプライン
//x ChunkAndBeamSolverで初期解生成→Order生成→YakinamashiState生成までを一括で行う
//x MAX_N, HABA, Kはテンプレート引数
//x 使い方: YstFactory<MAX_N, HABA, K>::build(...)
template<size_t MAX_N, int HABA, int K>
class YstFactory {
public:

    using ChunkSolver = ChunkAndBeamSolver<MAX_N, HABA, K>;
    using ChunkQuery = YakinamashiChunk::ChunkQuery; //x 型は外部依存
    using BeamQuery = ::BeamQuery;
    using command = ::command;
    constexpr static size_t MAX_ALL_ORD_A = YQConstants::GET_MAX_ALL_ORD_A<MAX_N>();
    constexpr static size_t MAX_ALL_ORD_B = YQConstants::GET_MAX_ALL_ORD_B<MAX_N>();

//    using QRIList = QRIList;



    // //x
    // static int get_tar_ordb_of_a2b(bitset<MAX_ALL_ORD_N>& exist_b_ords, BOrder& border, int tar_v, int stack_b_size) {
    //     int tar_rel_ord_b = get_rel_ord_(exist_b_ords, border.get_pushed_order(tar_v));
    //     if(tar_rel_ord_b == stack_b_size){
    //         return 0;
    //     }else{
    //         OrdValidator::validate_ord(tar_rel_ord_b, stack_b_size);
    //         return tar_rel_ord_b;
    //     }
    // }
private:

    struct ChunkSolveResult {
        my_vector<command> sep_cmds;
        my_vector<BeamQuery> beam_queries;
        my_vector<command> all_cmds;
        my_bitset<MAX_N> is_lis;
        State separated_st;
        int selected_lis_start;
    };

    struct YstBuildResult {
        YakinamashiState<MAX_N> yst;
        int init_score;
        int selected_lis_start;
    };

    //x
    static my_vector<ChunkQuery> build_default_chunk_queries(int current_N) {
        namespace YC = YakinamashiChunk;
        if (current_N < 500) {
            return {
                    ChunkQuery(
                            YC::SeparateTargets(
                                    YC::SeparateUnit::TOP | YC::SeparateUnit::BTM | YC::SeparateUnit::REMAIN),
                            YC::StackType::A
                    )
            };
        } else {
            return {
                    ChunkQuery(YC::SeparateTargets(YC::SeparateUnit::TOP | YC::SeparateUnit::BTM | YC::SeparateUnit::REMAIN),YC::StackType::A),
                    ChunkQuery(YC::SeparateTargets(YC::SeparateUnit::TOP | YC::SeparateUnit::BTM | YC::SeparateUnit::REMAIN),YC::StackType::A),
                    ChunkQuery(YC::SeparateTargets(YC::SeparateUnit::TOP | YC::SeparateUnit::BTM | YC::SeparateUnit::REMAIN),YC::StackType::A)
            };
        }
    }

    //x
    static ChunkSolveResult solve_chunk_and_beam(const State &initial_state,
                                                 const my_vector<ChunkQuery> &chunk_queries,
                                                 int state_N,
                                                 bool try_all_lis_start,
                                                 bool log_lis_detail) {
        auto [sep_cmds, beam_queries, all_cmds, is_lis, selected_lis_start] = ChunkSolver::solve(
                initial_state, chunk_queries, state_N, try_all_lis_start, log_lis_detail);
        State separated_st(initial_state);
        for (command cmd: sep_cmds) {
            separated_st.do_cmd(cmd);
        }
        return {std::move(sep_cmds), std::move(beam_queries), std::move(all_cmds), std::move(is_lis),
                std::move(separated_st), selected_lis_start};
    }

    // 責務: 指定 lis_start だけで ChunkAndBeamSolver を実行し、YstFactory 内部の ChunkSolveResult へ変換する。
    // 必要な理由: build_lis_range_score が選択済み lis_start を既存の order/query build pipeline に渡すため。
    static ChunkSolveResult solve_chunk_lis_start(const State &initial_state,
                                                  const my_vector<ChunkQuery> &chunk_queries,
                                                  int state_N,
                                                  int lis_start,
                                                  bool log_lis_detail) {
        auto [sep_cmds, beam_queries, all_cmds, is_lis, selected_lis_start] = ChunkSolver::solve_lis_start(
                initial_state, chunk_queries, state_N, lis_start, log_lis_detail);
        State separated_st(initial_state);
        for (command cmd: sep_cmds) {
            separated_st.do_cmd(cmd);
        }
        return {std::move(sep_cmds), std::move(beam_queries), std::move(all_cmds), std::move(is_lis),
                std::move(separated_st), selected_lis_start};
    }

    // 責務: ChunkAndBeamSolver の Result から separated_st を復元し、YstFactory 内部の ChunkSolveResult に詰め替える。
    // 必要な理由: 狭幅結果が太幅結果より良い場合に、狭幅結果そのものを初期解へ採用するため。
    static ChunkSolveResult to_chunk_solve_result(typename ChunkSolver::Result solve_result,
                                                  const State &initial_state) {
        auto [sep_cmds, beam_queries, all_cmds, is_lis, selected_lis_start] = std::move(solve_result);
        State separated_st(initial_state);
        for (command cmd: sep_cmds) {
            separated_st.do_cmd(cmd);
        }
        return {std::move(sep_cmds), std::move(beam_queries), std::move(all_cmds), std::move(is_lis),
                std::move(separated_st), selected_lis_start};
    }

    //x
    static std::pair<AOrder, BOrder> build_orders(const State &initial_state,
                                                  const my_vector<command> &sep_cmds,
                                                  const my_vector<BeamQuery> &beam_queries,
                                                  const my_bitset<MAX_N> &is_lis) {
        AOrder aorder = AOrder::build(
                sep_cmds, initial_state, beam_queries, to_aorder_is_lis(is_lis));
        BOrder border = BOrder::build(sep_cmds, initial_state, beam_queries);
        return {std::move(aorder), std::move(border)};
    }

    static my_bitset<MAX_BUFF> to_aorder_is_lis(const my_bitset<MAX_N> &is_lis) {
        static_assert(MAX_N <= MAX_BUFF);
        my_bitset<MAX_BUFF> converted;
        for (int v = 0; v < static_cast<int>(MAX_N); v++) {
            converted[v] = is_lis[v];
        }
        return converted;
    }

    //x
    static int calc_sum_turn(const QueriesState &state) {
        int sum_turn = 0;
        for (const auto &qri: state.queries) {
            sum_turn += qri.get_ins_turn();
        }
        my_assert(state.final_rot_info.second >= 0);
        return sum_turn + state.final_rot_info.second;
    }

    //x
    static void print_build_states(const State &initial_state, const State &separated_state) {
        // out("init_st"); // why: output.txt出力抑制のため
        // //Optuna initial_state.print();
        // out("separated_st"); // why: output.txt出力抑制のため
        // //Optuna separated_state.print();
    }

    //x
    static QueriesState build_initial_queries_state(int current_N) {
        // why: build中にインスタンスを一意に維持するため、空stateを先に1回だけ生成する
        return QueriesState(QRIList(), 0, {NO_COMMAND, 0}, OrdPos(0), OrdPos(0), current_N);
    }

    static void finalize_queries_state(QueriesState &queries_state) {
        queries_state.final_rot_info = FinalRotCalculator::build_final_rot_info(queries_state.current_N, queries_state);
        queries_state.sum_turn = calc_sum_turn(queries_state);
    }

public:
    //x 初期状態からYakinamashiStateを生成

    YakinamashiState<MAX_N> build(const State &initial_state) {
        return build_with_score(initial_state, false, true).yst;
    }

    YstBuildResult build_with_score(const State &initial_state,
                                    bool try_all_lis_start,
                                    bool log_lis_detail) {
        int state_N = initial_state.current_N;
        my_vector<ChunkQuery> chunk_queries = build_default_chunk_queries(state_N);
        ChunkSolveResult solve_result = solve_chunk_and_beam(
                initial_state, chunk_queries, state_N, try_all_lis_start, log_lis_detail);
        print_build_states(initial_state, solve_result.separated_st);

        auto [aorder, border] = build_orders(initial_state, solve_result.sep_cmds, solve_result.beam_queries,
                                             solve_result.is_lis);
        QueriesState queries_state = build_initial_queries_state(state_N);
        my_bitset<MAX_N> pipeline_is_lis = solve_result.is_lis;
        YakinamashiState<MAX_N> yst(
                std::move(solve_result.is_lis),
                QueryOrder(std::move(aorder), std::move(border)),
                std::move(queries_state)
        );
        YstQueriesBuildPipeline<MAX_N>::build(
                yst,
                initial_state,
                solve_result.sep_cmds,
                solve_result.beam_queries,
                pipeline_is_lis
        );
        finalize_queries_state(yst.queries_state);
#ifdef DEBUG
        QueriesStateValidator<MAX_N>::validate_state(initial_state, yst.queries_state);
        QueriesStateValidator<MAX_N>::validate_original_commands_length(yst.queries_state, solve_result.all_cmds);
#endif
        int init_score = yst.queries_state.sum_turn;
        return {std::move(yst), init_score, solve_result.selected_lis_start};
    }

    // 責務: NARROW_HABA の ChunkAndBeamSolver で lis_start を選び、選択 lis_start だけ通常 HABA の build pipeline へ流す。
    // 必要な理由: run_owner_array_unit_500 で複数 lis_start の初期解を軽く比較してから ALNS に入るため。
    template<int NARROW_HABA>
    YstBuildResult build_lis_range_score(const State &initial_state,
                                         int lis_range,
                                         bool log_lis_detail) {
        int state_N = initial_state.current_N;
        my_vector<ChunkQuery> chunk_queries = build_default_chunk_queries(state_N);
        auto narrow_result = ChunkAndBeamSolver<MAX_N, NARROW_HABA, K>::select_lis_range_start(
                initial_state, chunk_queries, state_N, lis_range, false);
        const int selected_lis_start = std::get<4>(narrow_result);
        ChunkSolveResult wide_solve_result = solve_chunk_lis_start(
                initial_state, chunk_queries, state_N, selected_lis_start, log_lis_detail);
        const int narrow_score = static_cast<int>(std::get<2>(narrow_result).size());
        const int wide_score = static_cast<int>(wide_solve_result.all_cmds.size());
        ChunkSolveResult solve_result = (narrow_score < wide_score)
                                        ? to_chunk_solve_result(std::move(narrow_result), initial_state)
                                        : std::move(wide_solve_result);
        print_build_states(initial_state, solve_result.separated_st);

        auto [aorder, border] = build_orders(initial_state, solve_result.sep_cmds, solve_result.beam_queries,
                                             solve_result.is_lis);
        QueriesState queries_state = build_initial_queries_state(state_N);
        my_bitset<MAX_N> pipeline_is_lis = solve_result.is_lis;
        YakinamashiState<MAX_N> yst(
                std::move(solve_result.is_lis),
                QueryOrder(std::move(aorder), std::move(border)),
                std::move(queries_state)
        );
        YstQueriesBuildPipeline<MAX_N>::build(
                yst,
                initial_state,
                solve_result.sep_cmds,
                solve_result.beam_queries,
                pipeline_is_lis
        );
        finalize_queries_state(yst.queries_state);
#ifdef DEBUG
        QueriesStateValidator<MAX_N>::validate_state(initial_state, yst.queries_state);
        QueriesStateValidator<MAX_N>::validate_original_commands_length(yst.queries_state, solve_result.all_cmds);
#endif
        int init_score = yst.queries_state.sum_turn;
        return {std::move(yst), init_score, solve_result.selected_lis_start};
    }

    // 責務: NARROW_HABA の ChunkAndBeamSolver で全 lis_start から開始位置を選び、選択 lis_start を build pipeline へ流す。
    // 必要な理由: run_owner_array_unit_500 で lis_range を使わず、幅 10 で全 lis_start を比較するため。
    template<int NARROW_HABA>
    YstBuildResult build_lis_score(const State &initial_state,
                                   bool log_lis_detail) {
        int state_N = initial_state.current_N;
        my_vector<ChunkQuery> chunk_queries = build_default_chunk_queries(state_N);
        const int selected_lis_start = ChunkAndBeamSolver<MAX_N, NARROW_HABA, K>::select_lis_start(
                initial_state, chunk_queries, state_N, false);
        ChunkSolveResult solve_result = solve_chunk_lis_start(
                initial_state, chunk_queries, state_N, selected_lis_start, log_lis_detail);
        print_build_states(initial_state, solve_result.separated_st);

        auto [aorder, border] = build_orders(initial_state, solve_result.sep_cmds, solve_result.beam_queries,
                                             solve_result.is_lis);
        QueriesState queries_state = build_initial_queries_state(state_N);
        my_bitset<MAX_N> pipeline_is_lis = solve_result.is_lis;
        YakinamashiState<MAX_N> yst(
                std::move(solve_result.is_lis),
                QueryOrder(std::move(aorder), std::move(border)),
                std::move(queries_state)
        );
        YstQueriesBuildPipeline<MAX_N>::build(
                yst,
                initial_state,
                solve_result.sep_cmds,
                solve_result.beam_queries,
                pipeline_is_lis
        );
        finalize_queries_state(yst.queries_state);
#ifdef DEBUG
        QueriesStateValidator<MAX_N>::validate_state(initial_state, yst.queries_state);
        QueriesStateValidator<MAX_N>::validate_original_commands_length(yst.queries_state, solve_result.all_cmds);
#endif
        int init_score = yst.queries_state.sum_turn;
        return {std::move(yst), init_score, solve_result.selected_lis_start};
    }

};
