// YstQueriesBuildPipeline.h
// YstFactory から Query 組み立て責務を分離するパイプライン
#pragma once

#include "ExistAllOrdManager.h"
#include "QueryProvider.h"
#include "A2AQueryUtil.h"
#include "OrdPosCalculator.h"
#include "QueryBuildValidationUtil.h"
#include "QueryListOrdUtil.h"
#include "QueryStateApplyUtil.h"
#include "State.h"
#include "YQAliases.h"
#include "Yakinamashi/Order/ValState.h"
#include "QueryCommonRI.h"
#include "QueriesState.h"
#include "YakinamashiQueryConstants.h"
#include "QueryCommonRI.h"
#include "QueryCommonRIFactory.h"
#include "YakinamashiState.h"
#include "YQTraits.h"
#include "BriBuilder.h"
#include "FinalRotCalculator.h"


//x Query 生成・適用・検証を一括で担当するパイプライン
template<size_t MAX_N>
class YstQueriesBuildPipeline {
public:
    using BeamQuery = ::BeamQuery;
    using command = ::command;
    constexpr static size_t MAX_ALL_ORD_A = YQConstants::GET_MAX_ALL_ORD_A<MAX_N>();
    constexpr static size_t MAX_ALL_ORD_B = YQConstants::GET_MAX_ALL_ORD_B<MAX_N>();

    static void build(YakinamashiState<MAX_N> &yst,
                      const State &init_st,
                      const my_vector<command> &sep_cmds,
                      const my_vector<BeamQuery> &beam_queries,
                      const my_bitset<MAX_N> &is_lis) {
        // //Optuna out("=== [YST_BUILD] init_state ===");
        // //Optuna init_st.print();
        int N = init_st.current_N;
        QCRIFactory cri_factory;
        QueryBuildBuffers buffers = init_build_buffers(init_st, yst);

        QueryBuildContext qb_ctx{
            buffers.st,
            yst,
            init_st,
            cri_factory,
            yst.queries_state,
            buffers.exist_a_ords,
            buffers.exist_b_ords,
            buffers.now_a_val_state,
            buffers.now_b_val_state,
            is_lis,
        };

        process_sep_cmds_to_queries(sep_cmds, qb_ctx);
        // //Optuna out("=== [YST_BUILD] after sep_cmds ===");
        // //Optuna buffers.st.print();
        // //Optuna out("=== [YST_BUILD] beam_queries.size():", beam_queries.size());
        for (int i = 0; i < (int) beam_queries.size(); i++) {
            // //Optuna out("[YST_BUILD] bq[", i, "]", beam_queries[i]);
        }
        auto prev_st = buffers.st;
        auto prev_a_val_state = buffers.now_a_val_state;
        auto prev_b_val_state = buffers.now_b_val_state;
        process_beam_queries_to_queries(N, beam_queries, qb_ctx);
        append_lis_noop_a2a_queries(N, qb_ctx);
        validate_all_effect_symmetry(yst, init_st);
    }

private:
    struct QueryBuildBuffers {
        State st;
        my_bitset<MAX_ALL_ORD_A> exist_a_ords;
        my_bitset<MAX_ALL_ORD_B> exist_b_ords;
        my_vector<ValState> now_a_val_state, now_b_val_state;
    };

    struct QueryBuildContext {
        State &st;
        YakinamashiState<MAX_N> &yst;
        const State &init_st;
        QCRIFactory &cri_factory;
        QueriesState &yaki_state;  // why: yst.queries_stateへの参照をショートカットとして保持
        my_bitset<MAX_ALL_ORD_A> &exist_a_ords;
        my_bitset<MAX_ALL_ORD_B> &exist_b_ords;
        my_vector<ValState> &now_a_val_state;
        my_vector<ValState> &now_b_val_state;
        const my_bitset<MAX_N> &is_lis;
    };

#ifdef DEBUG
    // _BEGIN: A2A range の beam 手数と追加 QRI 手数を直接比較し、A2AP->A2B+B2A 変換のずれを range 単位で止める。
    static int find_a2a_range_end_exclusive(const my_vector<BeamQuery> &beam_queries, int a2a_start_idx) {
        my_assert(0 <= a2a_start_idx && a2a_start_idx < static_cast<int>(beam_queries.size()));
        my_assert(beam_queries[a2a_start_idx].is_a2a());
        int i = a2a_start_idx;
        while (i < static_cast<int>(beam_queries.size()) && beam_queries[i].is_a2a()) {
            i++;
        }
        if (i < static_cast<int>(beam_queries.size())) {
            my_assert(beam_queries[i].is_a2b_b2a());
            return i + 1;
        }
        return i;
    }

    static int calc_beam_range_turn_sum(
        const my_vector<BeamQuery> &beam_queries,
        int start_idx,
        int end_exclusive
    ) {
        my_assert(0 <= start_idx && start_idx <= end_exclusive);
        int sum_turn = 0;
        for (int i = start_idx; i < end_exclusive; i++) {
            sum_turn += beam_queries[i].get_ins_turn_with_prev_diff();
        }
        return sum_turn;
    }

    static int calc_qri_range_turn_sum(
        const QRIList &queries,
        int start_idx,
        int end_exclusive
    ) {
        my_assert(0 <= start_idx && start_idx <= end_exclusive);
        int sum_turn = 0;
        for (int i = start_idx; i < end_exclusive; i++) {
            sum_turn += queries[i].get_ins_turn();
        }
        return sum_turn;
    }

    static int calc_final_rot_turn_from_state(const State &st) {
        my_assert(st.B.empty());
        int pos_zero = -1;
        for (int i = 0; i < st.A.size(); i++) {
            if (st.A[i] == 0) {
                pos_zero = i;
                break;
            }
        }
        my_assert(pos_zero != -1);
        int ra_dis = mod(-pos_zero, st.current_N);
        int rra_dis = pos_zero;
        return std::min(ra_dis, rra_dis);
    }

    static void validate_a2a_range_turn_sum_if_needed(
        int N,
        const my_vector<BeamQuery> &beam_queries,
        int beam_start_idx,
        int beam_end_exclusive,
        const QueriesState &yaki_state,
        int qri_start_idx,
        int qri_end_exclusive,
        const State &st_after_range,
        bool is_final_rot_range
    ) {
        int beam_sum_turn = calc_beam_range_turn_sum(beam_queries, beam_start_idx, beam_end_exclusive);
        int qri_sum_turn = calc_qri_range_turn_sum(yaki_state.queries, qri_start_idx, qri_end_exclusive);
        int beam_final_rot_turn = 0;
        int qri_final_rot_turn = 0;
        if (is_final_rot_range) {
            beam_final_rot_turn = calc_final_rot_turn_from_state(st_after_range);
            qri_final_rot_turn = FinalRotCalculator::build_final_rot_info(N, yaki_state).second;
            beam_sum_turn += beam_final_rot_turn;
            qri_sum_turn += qri_final_rot_turn;
        }
        if (beam_sum_turn == qri_sum_turn) return;
        out("=== A2A_RANGE_TURN_MISMATCH ===");
        out("beam_range", beam_start_idx, beam_end_exclusive);
        out("qri_range", qri_start_idx, qri_end_exclusive);
        out("is_final_rot_range", is_final_rot_range ? 1 : 0);
        out("beam_sum_turn", beam_sum_turn, "qri_sum_turn", qri_sum_turn);
        out("beam_final_rot_turn", beam_final_rot_turn, "qri_final_rot_turn", qri_final_rot_turn);
        for (int i = beam_start_idx; i < beam_end_exclusive; i++) {
            out("beam_q", i, beam_queries[i]);
        }
        for (int i = qri_start_idx; i < qri_end_exclusive; i++) {
            out("yst_q", i, yaki_state.queries[i]);
        }
        my_assert(false);
    }
    // _END
#endif

    // why: apply/revertの対称性が崩れるとswap等で無言で壊れるため、ビルド完了時に全クエリで検証する
    static void validate_all_effect_symmetry(const YakinamashiState<MAX_N> &yst, const State &init_st) {
#ifdef DEBUG
        QueryProvider provider(yst, init_st);
        for (int qi = 0; qi < static_cast<int>(yst.queries_state.queries.size()); ++qi) {
            provider.validate_effect_symmetry(yst.queries_state.queries[qi].get_query());
        }
#endif
    }

    static QueryBuildBuffers init_build_buffers(const State &init_st, YakinamashiState<MAX_N> &yst) {
        my_assert(init_st.B.size() == 0);
        QueryBuildBuffers buffers{
            State(init_st),
            my_bitset<MAX_ALL_ORD_A>(),
            my_bitset<MAX_ALL_ORD_B>(),
            yst.query_order.aorder.initial_value_type(init_st),
            my_vector<ValState>(init_st.initial_N, ValState::NONE)
        };
        ExistAllOrdManager::init_exist_a_ords_from_state<MAX_N>(
            init_st,
            yst.query_order.aorder,
            buffers.now_a_val_state,
            buffers.exist_a_ords
        );
        return buffers;
    }

    static OrdPos get_ord_pos_a_by_pos(const State &st,
                                       const my_vector<ValState> &now_a_val_state,
                                       const AOrder &aorder,
                                       const my_bitset<MAX_ALL_ORD_A> &exist_a_ords,
                                       int i) {
        my_assert(i == 0 || i < st.A.size());
        if (st.A.size() == 0) {
            //@ utilにempty時のordがあるそれをよぶo
            return OrdPos(0);
        }
        int v = st.A[i];
        ValState val_state = now_a_val_state[v];
        AllOrd all_orda = aorder.get_all_order(v, val_state);
        return to_ord_pos(OrdCalc::get_rel_ord(exist_a_ords, all_orda), st.A.size());
    }

    static void apply_query_to_state(QueryBuildContext &ctx, int tar_q_idx) {
        QCRIFactory::validate_exist_q_idx(ctx.yaki_state, tar_q_idx);
        const QueryCommonRI &qri = ctx.yaki_state.queries[tar_q_idx];
        const QueryCommon &query = qri.get_query();
        if (query.is_ab_type()) {
            BestRotInfo bri = BriBuilder::build_bri_ignore_P_a2a(
                ctx.st.current_N,
                ctx.cri_factory,
                ctx.yaki_state,
                tar_q_idx
            );
            QueryStateApplyUtil::apply_ab_rot_to_state(ctx.st, bri);
            if (query.is_a2b_type()) {
                ctx.st.pb();
            } else {
                my_assert(query.is_b2a_type());
                ctx.st.pa();
            }
        } else {
            my_assert(query.is_a2a_type());
            QueryStateApplyUtil::apply_query_to_state(ctx.st, qri);
        }
    }

    static void process_sep_cmds_to_queries(const my_vector<command> &sep_cmds,
                                            QueryBuildContext &ctx) {
        //TODO_COPILOT: first_top_ord_aを取得する関数があったはず
        OrdPos first_top_ord_a = get_ord_pos_a_by_pos(
            ctx.st,
            ctx.now_a_val_state,
            ctx.yst.query_order.aorder,
            ctx.exist_a_ords,
            0
        );
        ctx.yaki_state.first_top_ord_a = first_top_ord_a;
        ctx.yaki_state.first_top_ord_b = OrdPos(0);
        for (int sep_i = 0; sep_i < sep_cmds.size(); sep_i++) {
            const command cmd = sep_cmds[sep_i];
            my_assert(cmd != command::PA);
            if (cmd == command::PB) {
                int tar_v = ctx.st.A[0];
                OrdPos ord_pos_a = get_ord_pos_a_by_pos(ctx.st, ctx.now_a_val_state, ctx.yst.query_order.aorder, ctx.exist_a_ords, 0);
                RelOrd rel_ord_b = OrdCalc::get_tar_rel_ordb_of_a2b<MAX_N>(ctx.exist_b_ords, ctx.yst.query_order.border, tar_v, ctx.st.B.size());
                QueryCommon query_common(CommonPushType::A2B, tar_v, ctx.st.A.size(), ord_pos_a.get(), rel_ord_b.get());
                const int q_idx = static_cast<int>(ctx.yaki_state.queries.size());
                QueryCommonRI qri = ctx.cri_factory.build_ri(ctx.yaki_state, query_common, q_idx);
                ctx.yaki_state.queries.push_back(qri);
                ExistAllOrdManager::upd_ord_a2b<MAX_N>(
                    tar_v,
                    ctx.exist_a_ords,
                    ctx.exist_b_ords,
                    ctx.yst.query_order.aorder,
                    ctx.yst.query_order.border,
                    ctx.now_a_val_state,
                    ctx.now_b_val_state
                );
            }
            ctx.st.do_cmd(cmd);
            // //Optuna StackOrdUtil::print_stack_ord<MAX_N>(
            // //Optuna     ctx.st,
            // //Optuna     ctx.yst.query_order.aorder,
            // //Optuna     ctx.yst.query_order.border,
            // //Optuna     ctx.now_a_val_state,
            // //Optuna     ctx.now_b_val_state
            // //Optuna );
        }
    }

    static void process_a2b_beam_query(int tar_v, QueryBuildContext &ctx) {
        OrdPos tar_ord_posa = OrdCalc::get_tar_ord_posa_of_a2b<MAX_N>(ctx.exist_a_ords, ctx.yst.query_order.aorder, tar_v,
                                                                       ctx.now_a_val_state[tar_v], ctx.st.A.size());
        RelOrd tar_rel_ordb = OrdCalc::get_tar_rel_ordb_of_a2b<MAX_N>(ctx.exist_b_ords, ctx.yst.query_order.border, tar_v,
                                                                       ctx.st.B.size());
        QueryCommon query_common = ctx.cri_factory.build_a2b(
            tar_v,
            ctx.st.A.size(),
            tar_ord_posa,
            tar_rel_ordb
        );
        const int q_idx = static_cast<int>(ctx.yaki_state.queries.size());
        QueryCommonRI qri = ctx.cri_factory.build_ri(ctx.yaki_state, query_common, q_idx);
        ctx.yaki_state.queries.emplace_back(qri);
        ExistAllOrdManager::upd_ord_a2b<MAX_N>(
            tar_v,
            ctx.exist_a_ords,
            ctx.exist_b_ords,
            ctx.yst.query_order.aorder,
            ctx.yst.query_order.border,
            ctx.now_a_val_state,
            ctx.now_b_val_state
        );
        apply_query_to_state(ctx, q_idx);
    }

    static void process_b2a_beam_query(int tar_v, QueryBuildContext &ctx) {
        RelOrd tar_rel_orda = OrdCalc::get_tar_rel_orda_of_b2a<MAX_N>(ctx.exist_a_ords, ctx.yst.query_order.aorder, tar_v,
                                                                       ctx.st.A.size());
        OrdPos tar_ord_posb = OrdCalc::get_tar_ord_posb_of_b2a<MAX_N>(ctx.exist_b_ords, ctx.yst.query_order.border, tar_v,
                                                                       ctx.now_b_val_state[tar_v], ctx.st.B.size());
        QueryCommon query_common = ctx.cri_factory.build_b2a(
            tar_v,
            ctx.st.A.size(),
            tar_rel_orda,
            tar_ord_posb
        );
        const int q_idx = static_cast<int>(ctx.yaki_state.queries.size());
        QueryCommonRI qri = ctx.cri_factory.build_ri(ctx.yaki_state, query_common, q_idx);
        ctx.yaki_state.queries.emplace_back(qri);
        ExistAllOrdManager::upd_ord_b2a<MAX_N>(
            tar_v,
            ctx.exist_a_ords,
            ctx.exist_b_ords,
            ctx.yst.query_order.aorder,
            ctx.yst.query_order.border,
            ctx.now_a_val_state,
            ctx.now_b_val_state
        );
        apply_query_to_state(ctx, q_idx);
    }

    static void process_a2a_beam_query(int N,
                                       int bq_i,
                                       const my_vector<BeamQuery> &beam_queries,
                                       int tar_v,
                                       QueryBuildContext &ctx) {
        A2AQueryUtil::A2ACalcResult calc = A2AQueryUtil::calc_a2a_result<MAX_N>(
            beam_queries,
            bq_i,
            tar_v,
            ctx.exist_a_ords,
            ctx.yst.query_order.aorder,
            ctx.now_a_val_state,
            ctx.st.A.size()
        );
        A2AQueryUtil::validate_a2a_result(calc, ctx.st.A.size());
        // _BEGIN: push_rot は BOrder 側で確定した一時 B 順序を使い、A2B+B2A へ分解する。
        if (calc.a2a_type == e_a2a_type::push_rot) {
            process_a2b_beam_query(tar_v, ctx);
            process_b2a_beam_query(tar_v, ctx);
            return;
        }
        // _END
        OrdPos top_ordb = QueryListOrdUtil::get_finished_top_ord_b(N, ctx.yaki_state.queries, ctx.st);
        QueryCommon query_common = ctx.cri_factory.build_a2a(
            calc.a2a_type,
            tar_v,
            ctx.st.A.size(),
            calc.tar_orda1,
            calc.tar_rel_orda2,
            top_ordb,
            calc.flip_dis_dir,
            ctx.yaki_state.current_N
        );
        const int q_idx = static_cast<int>(ctx.yaki_state.queries.size());
        QueryCommonRI qri = ctx.cri_factory.build_ri(ctx.yaki_state, query_common, q_idx);
        ctx.yaki_state.queries.emplace_back(qri);
        ExistAllOrdManager::upd_ord_a2a<MAX_N>(
            tar_v,
            ctx.exist_a_ords,
            ctx.yst.query_order.aorder,
            ctx.now_a_val_state
        );
        apply_query_to_state(ctx, q_idx);
    }

    static void append_lis_noop_a2a_query(int N, int tar_v, QueryBuildContext &ctx) {
        my_assert(ctx.is_lis[tar_v]);
        my_assert(ctx.now_a_val_state[tar_v] == ValState::INIT);
        OrdPos tar_orda1 = OrdCalc::get_tar_orda1_of_a2a<MAX_N>(
            ctx.exist_a_ords, ctx.yst.query_order.aorder, tar_v,
            ctx.st.A.size(), ctx.now_a_val_state[tar_v]
        );
        RelOrd tar_rel_orda2 = OrdCalc::get_tar_rel_orda2_of_a2a<MAX_N>(
            ctx.exist_a_ords, ctx.yst.query_order.aorder, tar_v,
            ctx.st.A.size(), ctx.now_a_val_state[tar_v]
        );
        OrdPos top_ordb = QueryListOrdUtil::get_finished_top_ord_b(N, ctx.yaki_state.queries, ctx.st);
        QueryCommon query_common = ctx.cri_factory.build_a2a(
            e_a2a_type::swap_rot, tar_v, ctx.st.A.size(),
            tar_orda1, tar_rel_orda2, top_ordb, 0,
            ctx.yaki_state.current_N
        );
        my_assert(query_common.get_flip_dis_dir()==0);
        const int q_idx = static_cast<int>(ctx.yaki_state.queries.size());
        QueryCommonRI qri = ctx.cri_factory.build_ri(ctx.yaki_state, query_common, q_idx);
        ctx.yaki_state.queries.emplace_back(qri);
        ExistAllOrdManager::upd_ord_a2a<MAX_N>(
            tar_v, ctx.exist_a_ords, ctx.yst.query_order.aorder,
            ctx.now_a_val_state
        );
        apply_query_to_state(ctx, q_idx);
    }

    static void append_lis_noop_a2a_queries(int N, QueryBuildContext &ctx) {
        for (int v = 0; v < ctx.st.initial_N; v++) {
            if (!ctx.is_lis[v]) continue;
            append_lis_noop_a2a_query(N, v, ctx);
        }
    }

    // 責務: beam query列を実際のquery列へ変換し、各query適用後の検証を行う。
    // 必要な理由: YakinamashiStateのquery列を構築する中心処理であり、今回ログだけを止めた対象関数だと分かるようにするため。
    static void process_beam_queries_to_queries(int N,
                                                const my_vector<BeamQuery> &beam_queries,
                                                QueryBuildContext &ctx) {
        // //Optuna out("=== [YST_QUERIES] process_beam_queries_to_queries ===");
        // //Optuna out("[YST_QUERIES] beam_queries.size():", beam_queries.size());
        for (int bq_i = 0; bq_i < beam_queries.size(); bq_i++) {
            // //Optuna out("[YST_QUERIES] bq[", bq_i, "]", beam_queries[bq_i]);
        }
        // //Optuna out("[YST_QUERIES] initial state:");
        // //Optuna ctx.st.print();
#ifdef DEBUG
        int validating_a2a_start_bq_idx = -1;
        int validating_a2a_end_bq_exclusive = -1;
        int validating_a2a_start_q_idx = -1;
#endif
        for (int bq_i = 0; bq_i < beam_queries.size(); bq_i++) {
            const BeamQuery &bq = beam_queries[bq_i];
#ifdef DEBUG
            if (bq.is_a2a() && (bq_i == 0 || !beam_queries[bq_i - 1].is_a2a())) {
                validating_a2a_start_bq_idx = bq_i;
                validating_a2a_end_bq_exclusive = find_a2a_range_end_exclusive(beam_queries, bq_i);
                validating_a2a_start_q_idx = static_cast<int>(ctx.yaki_state.queries.size());
            }
#endif
            int tar_v = bq.get_tar_val();
            if (bq.get_push_type() == e_push_type::a2b) {
                process_a2b_beam_query(tar_v, ctx);
            } else if (bq.get_push_type() == e_push_type::b2a) {
                process_b2a_beam_query(tar_v, ctx);
            } else if (bq.is_a2a()) {
                process_a2a_beam_query(N, bq_i, beam_queries, tar_v, ctx);
            } else {
                my_assert(false);
            }
            // //Optuna out("[YST_QUERIES] after bq[", bq_i, "]");
            // //Optuna ctx.st.print();
            if(!QueryBuildValidationUtil<MAX_N>::validate_after_beam_query(
                beam_queries,
                N,
                bq_i,
                ctx.yaki_state.queries,
                ctx.st,
                ctx.yst.query_order.aorder,
                ctx.yst.query_order.border,
                ctx.now_a_val_state,
                ctx.now_b_val_state
            )){
                process_beam_queries_to_queries(N, beam_queries, ctx);
            }
#ifdef DEBUG
            if (validating_a2a_start_bq_idx != -1 && bq_i + 1 == validating_a2a_end_bq_exclusive) {
                validate_a2a_range_turn_sum_if_needed(
                    N,
                    beam_queries,
                    validating_a2a_start_bq_idx,
                    validating_a2a_end_bq_exclusive,
                    ctx.yaki_state,
                    validating_a2a_start_q_idx,
                    static_cast<int>(ctx.yaki_state.queries.size()),
                    ctx.st,
                    false
                );
                validating_a2a_start_bq_idx = -1;
                validating_a2a_end_bq_exclusive = -1;
                validating_a2a_start_q_idx = -1;
            }
#endif
        }
    }
};
