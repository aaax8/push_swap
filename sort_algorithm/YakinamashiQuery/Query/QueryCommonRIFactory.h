//QueryCommonRIFactory.h
//
//

#ifndef UNTITLED_QUERYCOMMONRIFACTORY_H
#define UNTITLED_QUERYCOMMONRIFACTORY_H

#include "OrdTypes.h"
#include "A2AQueryUtil.h"
#include "QueryCommon.h"
#include "QueryCommonRI.h"
#include "State.h"
#include "a2a_calculator.h"
#include "RotCalculator.h"
#include "BestRotInfo.h"
#include "YQTraits.h"
#include "QueriesState.h"
#include "PrevA2ARotSum.h"
#include <tuple>
#include "RotCntInfo.h"

// 共通の回転量計算＋BestRotInfo生成
//TODO_COPILOT: 
//消さないこと
inline RotCntInfo calc_rot_cnt_info(int a_size, int b_size,
                                    const OrdPos& start_ord_pos_a, const OrdPos& start_ord_pos_b,
                                    const OrdPos& tar_ord_pos_a, const OrdPos& tar_ord_pos_b,
                                    const PrevA2ARotSum& prev_a2a_rot_sum) {
validate_OrdPos(start_ord_pos_a, a_size);
    validate_OrdPos(start_ord_pos_b, b_size);
    validate_OrdPos(tar_ord_pos_a, a_size);
    validate_OrdPos(tar_ord_pos_b, b_size);
    RotCntInfo info;
    info.ra_cnt = RotCalculator::get_ra_dis(start_ord_pos_a, tar_ord_pos_a, a_size);
    info.rra_cnt = RotCalculator::get_rra_dis(start_ord_pos_a, tar_ord_pos_a, a_size);
    info.orig_rb_cnt = RotCalculator::get_rb_dis(start_ord_pos_b, tar_ord_pos_b, b_size);
    info.orig_rrb_cnt = RotCalculator::get_rrb_dis(start_ord_pos_b, tar_ord_pos_b, b_size);
    auto [sync_rb_cnt, sync_rrb_cnt] = PrevA2ARotSumCalc::get_prev_a2a_syncd_rot_b(info.orig_rb_cnt, info.orig_rrb_cnt, prev_a2a_rot_sum);
    info.brp = build_brp_by_rot(info.ra_cnt, info.rra_cnt, sync_rb_cnt, sync_rrb_cnt);
    return info;
}


//メンバ関数のsuffixに条件が書かれていることがある
//_P_A2Aは、prev_queryがA2Aであることを意味し
//_P_ABは、prev_queryがA2Bか、B2Aであることを意味する

class QCRIFactory {
public:
    enum qri_build_type {
        consider_P_a2a,
        ignore_P_a2a
    };

private:
    static OrdPos calc_finished_top_ord_a_ab(const QueryCommon& query) {
        if (query.is_a2b_type()) {
            return OrdCalc::get_erased_next_ord(query.get_tar_ord_pos_a_A2B(), query.get_stack_a_cnt());
        }
        my_assert(query.is_b2a_type());
        return OrdCalc::get_inserted_ord(query.get_tar_rel_ord_a_B2A());
    }

    // 責務: A2A 後に次 query の start として使う A 側 finished top OrdPos を返す。
    // 必要な理由: QueryCommon 側と同じ helper を使い、RI 作成時の finished top 計算を揃えるため。
    static OrdPos calc_finished_top_ord_a_a2a(OrdPos start_top_ord_a, const QueryCommon& query) {
        return A2AQueryUtil::calc_a2a_finished_top(
                start_top_ord_a,
                query.get_tar_ord_pos_a1_A2A(),
                query.get_tar_rel_ord_a2_A2A(),
                query.get_stack_a_cnt(),
                query.get_flip_dis_dir());
    }

    //QueryFactoryからのコピペ
    int calc_a2a_ins_turn2(int inter_cnt, e_a2a_type a2a_type) {
        if (a2a_type == e_a2a_type::swap_rot) {
            return calc_a2a_by_sa_rot(inter_cnt);
        } else if (a2a_type == e_a2a_type::push_rot) {
            return calc_a2a_by_push(inter_cnt);
        } else {
            my_assert(false);
            return -1;
        }
    }

    //QueryFactoryからのコピペ
    tuple<command, int, int> get_cmd1_cnt1_ins(e_a2a_type a2a_type, OrdPos start_top_ord_a, OrdPos tar_orda1,
                                               int a_size, int flip_dis_dir) {
        //vをtopにもっていく
        if (flip_dis_dir == 0) {
            return {NO_COMMAND, 0, 0};
        }
        if(a2a_type == e_a2a_type::swap_rot && flip_dis_dir < 0){
            OrdPos prev_tar_ord_a1 = OrdCalc::get_ord_pos_prev(tar_orda1, a_size);
            auto[cmd1, cnt1] = RotCalculator::get_best_rot_a(start_top_ord_a, prev_tar_ord_a1, a_size);
            int ins_turn = cnt1 + calc_a2a_ins_turn2(-flip_dis_dir, a2a_type);
            return {cmd1, cnt1, ins_turn};
        }else{
            my_assert(a2a_type != e_a2a_type::not_a2a);
            auto[cmd1, cnt1] = RotCalculator::get_best_rot_a(start_top_ord_a, tar_orda1, a_size);
            int ins_turn = cnt1 + calc_a2a_ins_turn2(abs(flip_dis_dir), a2a_type);
            return {cmd1, cnt1, ins_turn};
        }
    }
public:
    // 責務: factory 自体は状態を保持せず、各 build 呼び出しの QueriesState / QueryCommon だけを参照する。
    // 必要な理由: SwapBeamSearch の途中状態 suffix 再生成で初期 State への依存を持ち込まないため。
    QCRIFactory() = default;

    //query_common系----------
    QueryCommon build_a2b(int tar_v, int stack_a_cnt, OrdPos ord_pos_a, RelOrd rel_ord_b) {
        return QueryCommon(CommonPushType::A2B, tar_v, stack_a_cnt, ord_pos_a.get(), rel_ord_b.get());
    }

    QueryCommon build_b2a(int tar_v, int stack_a_cnt, RelOrd rel_ord_a, OrdPos ord_pos_b) {
        return QueryCommon(CommonPushType::B2A, tar_v, stack_a_cnt, rel_ord_a.get(), ord_pos_b.get());
    }

    QueryCommon build_a2as(int tar_v,
                                  int stack_a_cnt,
                                  OrdPos ord_pos_a1,
                                  RelOrd rel_ord_a2,
                                  OrdPos top_ord_b,
                                  int flip_dis_dir,
                                  int current_N) {
        QueryCommon ret = QueryCommon(
                CommonPushType::A2AS,
                tar_v,
                stack_a_cnt,
                ord_pos_a1.get(),
                rel_ord_a2.get(),
                top_ord_b.get(),
                flip_dis_dir
        );
        validate_OrdPos(ord_pos_a1, stack_a_cnt);
        validate_RelOrd(rel_ord_a2, stack_a_cnt);
        validate_OrdPos(top_ord_b, ret.get_stack_b_cnt(current_N));
        return ret;
    }

    // : A2AP 廃止に伴い build_a2ap を削除。
    QueryCommon build_a2a(e_a2a_type a2a_type,
                                 int tar_v,
                                  int stack_a_cnt,
                                  OrdPos ord_pos_a1,
                                  RelOrd rel_ord_a2,
                                  OrdPos top_ord_b,
                                  int flip_dis_dir,
                                  int current_N) {
        // _BEGIN: push_rot は上流で A2B+B2A へ変換済みなので、factory では A2AS のみを受ける。
        my_assert(a2a_type == e_a2a_type::swap_rot);
        QueryCommon ret = build_a2as(tar_v, stack_a_cnt, ord_pos_a1, rel_ord_a2, top_ord_b, flip_dis_dir, current_N);
        validate_OrdPos(ord_pos_a1, stack_a_cnt);
        validate_RelOrd(rel_ord_a2, stack_a_cnt);
        validate_OrdPos(top_ord_b, ret.get_stack_b_cnt(current_N));
        return ret;
        // _END
    }
private:
    // A2BQueryRI(const A2BQuery& q, const BestRotInfo& bri)
    static QueryCommonRI build_ab_ri(const QueryCommon& query, const BestRotInfo& bri){
        query.validate_is_ab_type();
        return QueryCommonRI(query, bri, calc_finished_top_ord_a_ab(query));
    }

    static QueryCommonRI build_a2b_ri(const QueryCommon& query, const BestRotInfo& bri){
        return build_ab_ri(query, bri);
    }

    static QueryCommonRI build_b2a_ri(const QueryCommon& query, const BestRotInfo& bri){
        return build_ab_ri(query, bri);
    }

    QueryCommonRI build_a2b_ri_P_AB(const QueryCommonRI& prev_qri, const QueryCommon& q, int current_N){
        prev_qri.validate_is_ab_type();
        q.validate_is_a2b_type();
        const QueryCommon& prev_q = prev_qri.get_query();
        OrdPos start_ord_pos_a = prev_qri.get_finished_top_ord_a();
        OrdPos start_ord_pos_b = prev_q.get_finished_top_ord_b(current_N);
        OrdPos tar_ord_pos_a = q.get_tar_ord_pos_a_A2B();
        OrdPos tar_ord_pos_b = to_ord_pos(q.get_tar_rel_ord_b_A2B(), q.get_stack_b_cnt(current_N));
        int a_size = q.get_stack_a_cnt();
        int b_size = q.get_stack_b_cnt(current_N);
        validate_OrdPos(start_ord_pos_a, a_size);
        validate_OrdPos(start_ord_pos_b, b_size);
        validate_OrdPos(tar_ord_pos_a, a_size);
        validate_OrdPos(tar_ord_pos_b, b_size);
        auto brp = build_brp_by_ord(a_size,b_size, start_ord_pos_a, start_ord_pos_b, tar_ord_pos_a, tar_ord_pos_b);
        QueryCommonRI ret = build_a2b_ri(q, brp);
        return ret;
    }

    QueryCommonRI build_b2a_ri_P_AB(const QueryCommonRI& prev_qri, const QueryCommon& q, int current_N){
        prev_qri.validate_is_ab_type();
        q.validate_is_b2a_type();
        const QueryCommon& prev_q = prev_qri.get_query();
        OrdPos start_ord_pos_a = prev_qri.get_finished_top_ord_a();
        OrdPos start_ord_pos_b = prev_q.get_finished_top_ord_b(current_N);
        OrdPos tar_ord_pos_a = to_ord_pos(q.get_tar_rel_ord_a_B2A(), q.get_stack_a_cnt());
        OrdPos tar_ord_pos_b = q.get_tar_ord_pos_b_B2A();
        int a_size = q.get_stack_a_cnt();
        int b_size = q.get_stack_b_cnt(current_N);
        validate_OrdPos(start_ord_pos_a, a_size);
        validate_OrdPos(start_ord_pos_b, b_size);
        validate_OrdPos(tar_ord_pos_a, a_size);
        validate_OrdPos(tar_ord_pos_b, b_size);
        auto brp = build_brp_by_ord(a_size,b_size, start_ord_pos_a, start_ord_pos_b, tar_ord_pos_a, tar_ord_pos_b);
        QueryCommonRI ret = build_b2a_ri(q, brp);
        return ret;
    }
    //TODO_COPILOT: 絶対に消さない事
    // build_a2b_ri_P_A2Aは必須build_b2a_ri_P_A2Aもどうよう
    //これが使えないなら、使えるように他の部分を変える事
    QueryCommonRI build_a2b_ri_P_A2A(const QueryCommonRI& prev_qri, const QueryCommon& query, const PrevA2ARotSum &prev_a2a_rot_sum,
                                     int current_N)
    {
        my_assert(prev_qri.is_a2a_type());
        query.validate_is_a2b_type();
        prev_a2a_rot_sum.validate();
        const QueryCommon& prev_q = prev_qri.get_query();
        OrdPos start_ord_pos_a = prev_qri.get_finished_top_ord_a();
        OrdPos start_ord_pos_b = prev_q.get_finished_top_ord_b(current_N);
        OrdPos tar_ord_pos_a = query.get_tar_ord_pos_a_A2B();
        OrdPos tar_ord_pos_b = to_ord_pos(query.get_tar_rel_ord_b_A2B(), query.get_stack_b_cnt(current_N));
        validate_OrdPos(start_ord_pos_a, prev_q.get_stack_a_cnt());
        validate_OrdPos(start_ord_pos_b, prev_q.get_stack_b_cnt(current_N));
        validate_OrdPos(tar_ord_pos_a, query.get_stack_a_cnt());
        validate_OrdPos(tar_ord_pos_b, query.get_stack_b_cnt(current_N));
        RotCntInfo rot = calc_rot_cnt_info(
            query.get_stack_a_cnt(), query.get_stack_b_cnt(current_N),
            start_ord_pos_a, start_ord_pos_b,
            tar_ord_pos_a, tar_ord_pos_b,
            prev_a2a_rot_sum);
        QueryCommonRI ret = build_a2b_ri(query, rot.brp);
        ret.debug_set_prev_a2a_rot_sum_AB(prev_a2a_rot_sum);
        ret.debug_set_orig_rot_info_AB_P_A2A(rot);
        return ret;
    }

    // _BEGIN: Reuse the same B2A+prev_a2a path from construct_v when the caller already knows start tops.
    QueryCommonRI build_b2a_ri_from_start_top_P_A2A_impl(
            OrdPos start_ord_pos_a,
            OrdPos start_ord_pos_b,
            const QueryCommon& query,
            const PrevA2ARotSum &prev_a2a_rot_sum,
            int current_N
    ) {
        query.validate_is_b2a_type();
        prev_a2a_rot_sum.validate();
        OrdPos tar_ord_pos_a = to_ord_pos(query.get_tar_rel_ord_a_B2A(), query.get_stack_a_cnt());
        OrdPos tar_ord_pos_b = query.get_tar_ord_pos_b_B2A();
        validate_OrdPos(start_ord_pos_a, query.get_stack_a_cnt());
        validate_OrdPos(start_ord_pos_b, query.get_stack_b_cnt(current_N));
        validate_OrdPos(tar_ord_pos_a, query.get_stack_a_cnt());
        validate_OrdPos(tar_ord_pos_b, query.get_stack_b_cnt(current_N));
        RotCntInfo rot = calc_rot_cnt_info(
            query.get_stack_a_cnt(), query.get_stack_b_cnt(current_N),
            start_ord_pos_a, start_ord_pos_b,
            tar_ord_pos_a, tar_ord_pos_b,
            prev_a2a_rot_sum);
        QueryCommonRI ret = build_b2a_ri(query, rot.brp);
        ret.debug_set_prev_a2a_rot_sum_AB(prev_a2a_rot_sum);
        ret.debug_set_orig_rot_info_AB_P_A2A(rot);
        return ret;
    }

    QueryCommonRI build_b2a_ri_P_A2A(const QueryCommonRI& prev_qri, const QueryCommon& query, const PrevA2ARotSum &prev_a2a_rot_sum,
                                     int current_N)
    {
        my_assert(prev_qri.is_a2a_type());
        const QueryCommon& prev_q = prev_qri.get_query();
        return build_b2a_ri_from_start_top_P_A2A_impl(
            prev_qri.get_finished_top_ord_a(),
            prev_q.get_finished_top_ord_b(current_N),
            query,
            prev_a2a_rot_sum,
            current_N
        );
    }
    // _END
public:
    static void validate_exist_q_idx(const QueriesState& yaki_state, int query_idx) {
        my_assert(0 <= query_idx && query_idx < yaki_state.queries.size());
    }

    static void validate_insert_q_idx(const QueriesState& yaki_state, int query_idx) {
        my_assert(0 <= query_idx && query_idx <= yaki_state.queries.size());
    }

    //todo QueriesStateは第一引数にしたい
    //prevがa2aならrot_sum
    //TODO_COPILOT: prev_idxを指定しない形にしたい, tar_idxを指定する形にしたい
    template<qri_build_type build_type = qri_build_type::consider_P_a2a>
    QueryCommonRI build_not_first_a2b_ri(const QueriesState& yaki_state, const QueryCommon& query, int q_prev_idx)
    {
        validate_exist_q_idx(yaki_state, q_prev_idx);
        query.validate_is_a2b_type();
        int current_N = yaki_state.current_N;
        const bool is_prev_a2a = yaki_state.queries[q_prev_idx].is_a2a_type();
        //TODO_COPILOT: if constexpr
        if(is_prev_a2a){
            PrevA2ARotSum prev_a2a_rot_sum;
            if constexpr (build_type == qri_build_type::consider_P_a2a) {
                prev_a2a_rot_sum = PrevA2ARotSumCalc::calc_prev_range_P(yaki_state.queries, q_prev_idx);
            } else {
                prev_a2a_rot_sum = PrevA2ARotSum(0, 0);
            }
            return build_a2b_ri_P_A2A(yaki_state.queries[q_prev_idx], query, prev_a2a_rot_sum, current_N);
        }else{
            my_assert(yaki_state.queries[q_prev_idx].is_ab_type());
            return build_a2b_ri_P_AB(yaki_state.queries[q_prev_idx], query, current_N);
        }
    }

    //prevのidxを指定する
    //prevがa2aならrot_sum
    //todo QueriesStateは第一引数にしたい
    //TODO_COPILOT: prev_idxを指定しない形にしたい, tar_idxを指定する形にしたい

    template<qri_build_type build_type = qri_build_type::consider_P_a2a>
    QueryCommonRI build_not_first_b2a_ri(const QueriesState& yaki_state, const QueryCommon& query, int q_prev_idx)
    {
        validate_exist_q_idx(yaki_state, q_prev_idx);
        query.validate_is_b2a_type();
        int current_N = yaki_state.current_N;
        const bool is_prev_a2a = yaki_state.queries[q_prev_idx].is_a2a_type();
        //TODO_COPILOT: 絶対に消さない事
        if(is_prev_a2a){
            PrevA2ARotSum prev_a2a_rot_sum;
            if constexpr (build_type == qri_build_type::consider_P_a2a) {
                prev_a2a_rot_sum = PrevA2ARotSumCalc::calc_prev_range_P(yaki_state.queries, q_prev_idx);
            } else {
                prev_a2a_rot_sum = PrevA2ARotSum(0, 0);
            }
            return build_b2a_ri_P_A2A(yaki_state.queries[q_prev_idx], query, prev_a2a_rot_sum, current_N);
        }else{
            my_assert(yaki_state.queries[q_prev_idx].is_ab_type());
            return build_b2a_ri_P_AB(yaki_state.queries[q_prev_idx], query, current_N);
        }
    }
    
    //todo 
    //挿入時に前のa2aも変更するbuildもつくる
    //単体のクエリを返す奴は、前のクエリはへんこうしない　
    //TODO_COPILOT: prev_idxを指定しない形にしたい, tar_idxを指定する形にしたい

    static QueryCommonRI build_a2a_ri(const QueryCommon& query, command cmd_rot_a1, int cmd_rot_a1_cnt, int ins_turn,
                                      OrdPos finished_top_ord_a){
        query.validate_is_a2a_type();
        return QueryCommonRI(query, cmd_rot_a1, cmd_rot_a1_cnt, ins_turn, finished_top_ord_a);
    }
    //todo QueriesStateは第一引数にしたい
        QueryCommonRI build_first_a2b_ri(const QueriesState& yaki_state, const QueryCommon &a2b_query) {
                a2b_query.validate_is_a2b_type();
        int current_N = yaki_state.current_N;
        // 責務: first A2B の B 側 RelOrd を、現在の B stack size に対応する OrdPos へ変換する。
        // 必要な理由: B 側の回転先を A size で変換すると、SwapBeamSearch の途中再生成など B 非空の first A2B でずれるため。
        BestRotInfo brp = build_brp_by_ord(a2b_query.get_stack_a_cnt(), a2b_query.get_stack_b_cnt(current_N), yaki_state.first_top_ord_a, yaki_state.first_top_ord_b,
                                           a2b_query.get_tar_ord_pos_a_A2B(), to_ord_pos(a2b_query.get_tar_rel_ord_b_A2B(), a2b_query.get_stack_b_cnt(current_N)));
        return build_a2b_ri(a2b_query, brp);
    }

    //todo QueriesStateは第一引数にしたい
    QueryCommonRI build_first_b2a_ri(const QueriesState& yaki_state, const QueryCommon &b2a_query) {
        b2a_query.validate_is_b2a_type();
        int current_N = yaki_state.current_N;
        BestRotInfo brp = build_brp_by_ord(b2a_query.get_stack_a_cnt(), b2a_query.get_stack_b_cnt(current_N), yaki_state.first_top_ord_a, yaki_state.first_top_ord_b,
                                           to_ord_pos(b2a_query.get_tar_rel_ord_a_B2A(), b2a_query.get_stack_a_cnt()), b2a_query.get_tar_ord_pos_b_B2A());
        return build_b2a_ri(b2a_query, brp);
    }

    
    QueryCommonRI build_a2a_ri_impl(OrdPos start_top_ord_a, const QueryCommon &a2a_query){
        a2a_query.validate_is_a2a_type();
        auto [cmd1, cnt1, ins_turn] = get_cmd1_cnt1_ins(e_a2a_type::swap_rot, start_top_ord_a,
                                                        a2a_query.get_tar_ord_pos_a1_A2A(), a2a_query.get_stack_a_cnt(), a2a_query.get_flip_dis_dir());
        OrdPos finished_top_ord_a = calc_finished_top_ord_a_a2a(start_top_ord_a, a2a_query);
        QueryCommonRI cri = build_a2a_ri(a2a_query, cmd1, cnt1, ins_turn, finished_top_ord_a);
        return cri;
    }

    //todo QueriesStateは第一引数にしたい
    QueryCommonRI build_first_a2a_ri(const QueriesState& yaki_state, const QueryCommon &query) {
        return build_a2a_ri_impl(yaki_state.first_top_ord_a, query);
    }

    QueryCommonRI build_a2a_ri_from_start_top(OrdPos start_top_ord_a, const QueryCommon &a2a_query) {
        return build_a2a_ri_impl(start_top_ord_a, a2a_query);
    }

    QueryCommonRI build_b2a_ri_from_start_top_P_A2A(
            OrdPos start_top_ord_a,
            OrdPos start_top_ord_b,
            const QueryCommon &b2a_query,
            const PrevA2ARotSum &prev_a2a_rot_sum,
            int current_N
    ) {
        return build_b2a_ri_from_start_top_P_A2A_impl(
            start_top_ord_a,
            start_top_ord_b,
            b2a_query,
            prev_a2a_rot_sum,
            current_N
        );
    }

    //todo QueriesStateは第一引数にしたい
    //x
    QueryCommonRI build_first_ri(const QueriesState& yaki_state, const QueryCommon &query) {
        if(query.is_a2b_type()){
            return build_first_a2b_ri(yaki_state, query);
        }else if(query.is_b2a_type()){
            return build_first_b2a_ri(yaki_state, query);
        }else{
            my_assert(query.is_a2a_type());
            return build_first_a2a_ri(yaki_state, query);
        }
    }


    QueryCommonRI build_a2a_ri(const QueryCommonRI &prev_qri, const QueryCommon &query) {
        return build_a2a_ri_impl(prev_qri.get_finished_top_ord_a(), query);
    }

    //x
    template<qri_build_type build_type = qri_build_type::consider_P_a2a>
    QueryCommonRI build_not_first_ri(const QueriesState& yaki_state, const QueryCommon &query, int tar_idx) {
        validate_insert_q_idx(yaki_state, tar_idx);
        my_assert(tar_idx > 0);
        const int prev_idx = tar_idx - 1;
        if(query.is_a2b_type()){
            return build_not_first_a2b_ri<build_type>(yaki_state, query, prev_idx);
        }else if(query.is_b2a_type()){
            return build_not_first_b2a_ri<build_type>(yaki_state, query, prev_idx);
        }else{
            my_assert(query.is_a2a_type());
            return build_a2a_ri(yaki_state.queries[prev_idx], query);
        }
    }


    // 呼び出し側が先頭かどうかを意識しなくてよいAPI
    //todo QueriesStateは第一引数にしたい
    template<qri_build_type build_type = qri_build_type::consider_P_a2a>
    QueryCommonRI build_ri(const QueriesState& yaki_state, const QueryCommon &query, int q_idx) {
        if (q_idx == 0) {
            return build_first_ri(yaki_state, query);
        } else {
            return build_not_first_ri<build_type>(yaki_state, query, q_idx);
        }
    }

};

#endif //UNTITLED_QUERYCOMMONRIFACTORY_H
