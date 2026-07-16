// QueryProvider.h
// YakinamashiQuery用 QueryProvider クラス定義
#pragma once

#include "Enums.h"
#include "YQAliases.h"
#include "Yakinamashi/Order/OrdTypes.h"
#include "QueryCommon.h"
#include "util.h"
#include "QueriesState.h"
#include "QueryCommonRIFactory.h"
#include "QueriesCalculator.h"
#include "A2AQueryUtil.h"
#include "Command/Command.h"
#include "YakinamashiState.h"
#include "State.h"
#include "QueryOrdEffectSet.h"
#include "QueryOrdEffect.h"
#include "QueryOrdEffectProvider.h"

inline e_push_type to_e_push_type(CommonPushType push_type) {
    if (push_type == CommonPushType::A2B) {
        return e_push_type::a2b;
    }
    if (push_type == CommonPushType::B2A) {
        return e_push_type::b2a;
    }
    // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
    my_assert(push_type == CommonPushType::A2AS);
    // _END
    return e_push_type::a2a;
}

class QueryProvider {
private:
    const QueryOrder &query_order;
    const QueriesState &queries_state;
    mutable QCRIFactory cri_factory; // why: buildメソッドはconst文脈から呼ばれるが、factory自体は状態を持たない
    // : effect生成責務をQueryOrdEffectProviderに委譲するためメンバとして保持
    QueryOrdEffectProvider effect_provider_;

    const QueriesState& get_qs() const {
        return queries_state;
    }

    int get_query_count() const {
        return static_cast<int>(get_qs().queries.size());
    }

    void validate_qi(int qi) const {
        my_assert(0 <= qi && qi < get_query_count());
    }

    CommonPushType get_push_type(int qi) const {
        validate_qi(qi);
        return get_qs().queries[qi].get_query().get_push_type();
    }

    const QueryCommon& get_query_common(int qi) const {
        validate_qi(qi);
        return get_qs().queries[qi].get_query();
    }

public:
    // : effect_provider_ の初期化を追加
    template<size_t MAX_N>
    QueryProvider(const YakinamashiState<MAX_N> &yst_state, const State &init_st)
        : query_order(yst_state.query_order),
          queries_state(yst_state.queries_state),
          cri_factory(),
          effect_provider_(yst_state) {}

    // : effect生成をQueryOrdEffectProviderに委譲
    QueryOrdEffect get_apply_effect(const QueryCommon& apply_q) const {
        return effect_provider_.get_apply_effect(apply_q);
    }

    QueryOrdEffect get_apply_effect(int apply_qi) const {
        return effect_provider_.get_apply_effect(apply_qi);
    }

    QueryOrdEffect get_revert_effect(const QueryCommon& erase_q) const {
        return effect_provider_.get_revert_effect(erase_q);
    }

    //erase_qi番目のクエリをeraseすることによる、以後のクエリに与える影響を返す
    QueryOrdEffect get_revert_effect(int erase_qi) const {
        return effect_provider_.get_revert_effect(erase_qi);
    }

    void validate_effect_symmetry(const QueryCommon& q) const {
        effect_provider_.validate_effect_symmetry(q);
    }

public:
    // : 片側 effect から ord 補正を計算する共通処理。
    //BOrderのinsert_update_ordと同じ仕様で、すでに同じall_ordがある場合は、それよりも前に挿入したあつかいになる(addだけ)
    template<class OrdType>
    static OrdType apply_single_side_ord(const OrdType &prev_ord, const AllOrd &prev_all_ord, const SingleSideOrdEffect &side) {
        int ord_diff = 0;
        if (side.has_add()) {
//            my_assert(prev_all_ord != side.get_add_ord());
            if (prev_all_ord >= side.get_add_ord()) ord_diff += 1;
        }
        if (side.has_del()) {
            my_assert(prev_all_ord != side.get_del_ord());
            if (prev_all_ord >= side.get_del_ord()) ord_diff -= 1;
        }
        return OrdType(prev_ord.get() + ord_diff);
    }

private:
    template<class OrdType, e_stack_type tar_stack_type, size_t MAX_N>
    OrdType get_applied_ord_set(const OrdType& prev_ord, AllOrd prev_all_ord, const QueryOrdEffectSet<MAX_N>& effect) const {
        int delta;
        if constexpr (tar_stack_type == e_stack_type::a) {
            delta = effect.get_ord_delta_a(prev_all_ord);
        } else {
            delta = effect.get_ord_delta_b(prev_all_ord);
        }
        return OrdType(prev_ord.get() + delta);
    }

public:
    template<class OrdType>
    OrdType get_applied_a_ord(const OrdType &prev_ord_a, const AllOrd &prev_all_ord_a, const QueryOrdEffect &effect) const {
        return apply_single_side_ord(prev_ord_a, prev_all_ord_a, effect.a_effect);
    }

    template<class OrdType>
    OrdType get_applied_b_ord(const OrdType &prev_ord_b, const AllOrd &prev_all_ord_b, const QueryOrdEffect &effect) const {
        return apply_single_side_ord(prev_ord_b, prev_all_ord_b, effect.b_effect);
    }

    int get_applied_a_cnt(int prev_stack_a_cnt, const QueryOrdEffect &effect) const {
        return prev_stack_a_cnt + effect.diff_stack_a_cnt;
    }

    int get_stack_b_cnt_from_a_cnt(int stack_a_cnt) const {
        return queries_state.current_N - stack_a_cnt;
    }

    int calc_applied_flip_dis_dir_a2a(const QueryCommon& query,
                                      OrdPos new_ord_pos_a1,
                                      OrdPos new_ord_pos_a2,
                                      int new_stack_a_cnt) const {
        if (new_ord_pos_a1 == new_ord_pos_a2) {
            return 0;
        }
        int old_flip_dis_dir = query.get_flip_dis_dir();
        if (old_flip_dis_dir == 0) {
            return A2AQueryUtil::calc_flip_dis_dir(
                // _BEGIN: A2AP 廃止で常に swap_rot のため get_a2a_type() を直接置換。
                new_ord_pos_a1, new_ord_pos_a2, new_stack_a_cnt, e_a2a_type::swap_rot);
                // _END
        }
        command existing_direction = (old_flip_dis_dir > 0) ? RA : RRA;
        return A2AQueryUtil::calc_flip_dis_dir_with_direction(
            new_ord_pos_a1, new_ord_pos_a2, new_stack_a_cnt, existing_direction);
    }

private:
    QueryCommon build_applied_a2b_common(const QueryCommon &query, const QueryOrdEffect &effect) const {
        query.validate_is_a2b_type();
        const int tar_v = query.get_tar_v();
        const AllOrd prev_all_ord_a = query_order.aorder.get_init_order(tar_v);
        const AllOrd prev_all_ord_b = query_order.border.get_target_order(tar_v);
        OrdPos new_ord_pos_a = get_applied_a_ord(query.get_tar_ord_pos_a_A2B(), prev_all_ord_a, effect);
        RelOrd new_rel_ord_b = get_applied_b_ord(query.get_tar_rel_ord_b_A2B(), prev_all_ord_b, effect);
        int new_stack_a_cnt = get_applied_a_cnt(query.get_stack_a_cnt(), effect);
        return cri_factory.build_a2b(query.get_tar_v(), new_stack_a_cnt, new_ord_pos_a, new_rel_ord_b);
    }

    QueryCommon build_applied_b2a_common(const QueryCommon &query, const QueryOrdEffect &effect) const {
        query.validate_is_b2a_type();
        const int tar_v = query.get_tar_v();
        const AllOrd prev_all_ord_a = query_order.aorder.get_target_order(tar_v);
        const AllOrd prev_all_ord_b = query_order.border.get_target_order(tar_v);
        RelOrd new_rel_ord_a = get_applied_a_ord(query.get_tar_rel_ord_a_B2A(), prev_all_ord_a, effect);
        OrdPos new_ord_pos_b = get_applied_b_ord(query.get_tar_ord_pos_b_B2A(), prev_all_ord_b, effect);
        int new_stack_a_cnt = get_applied_a_cnt(query.get_stack_a_cnt(), effect);
        return cri_factory.build_b2a(query.get_tar_v(), new_stack_a_cnt, new_rel_ord_a, new_ord_pos_b);
    }

    QueryCommon build_applied_a2a_common(OrdPos new_top_ord_pos_b, const QueryCommon &query, const QueryOrdEffect &effect) const {
        query.validate_is_a2a_type();
        const int tar_v = query.get_tar_v();
        const AllOrd prev_all_ord_a1 = query_order.aorder.get_init_order(tar_v);
        const AllOrd prev_all_ord_a2 = query_order.aorder.get_target_order(tar_v);
        OrdPos new_ord_pos_a1 = get_applied_a_ord(query.get_tar_ord_pos_a1_A2A(), prev_all_ord_a1, effect);
        RelOrd new_rel_ord_a2 = get_applied_a_ord(query.get_tar_rel_ord_a2_A2A(), prev_all_ord_a2, effect);
        int new_stack_a_cnt = get_applied_a_cnt(query.get_stack_a_cnt(), effect);
        int new_stack_b_cnt = get_stack_b_cnt_from_a_cnt(new_stack_a_cnt);
        my_assert(new_stack_b_cnt >= 0);
        constexpr int EMPTY_TOP_ORD_POS_B = 0;
        if (new_stack_b_cnt == 0) {
            new_top_ord_pos_b = OrdPos(EMPTY_TOP_ORD_POS_B);
        }
        validate_OrdPos(new_top_ord_pos_b, new_stack_b_cnt);
        // effectによってa1/a2のordが変化した後も、既存の回転方向を保つ
        const OrdPos new_ord_pos_a2 = to_ord_pos(new_rel_ord_a2, new_stack_a_cnt);
        const int new_flip_dis_dir = calc_applied_flip_dis_dir_a2a(
            query, new_ord_pos_a1, new_ord_pos_a2, new_stack_a_cnt);
        // _BEGIN: A2AP 廃止に伴い A2AP ブランチを削除。A2AS のみ。
        my_assert(query.get_push_type() == CommonPushType::A2AS);
        return QueryCommon(
            CommonPushType::A2AS,
        // _END
            query.get_tar_v(),
            new_stack_a_cnt,
            new_ord_pos_a1.get(),
            new_rel_ord_a2.get(),
            new_top_ord_pos_b.get(),
            new_flip_dis_dir
        );
    }

    template<size_t MAX_N>
    QueryCommon build_applied_a2b_common_set(const QueryCommon& query, const QueryOrdEffectSet<MAX_N>& effect) const {
        query.validate_is_a2b_type();
        const int tar_v = query.get_tar_v();
        const AllOrd prev_all_ord_a = query_order.aorder.get_init_order(tar_v);
        const AllOrd prev_all_ord_b = query_order.border.get_target_order(tar_v);
        OrdPos new_ord_pos_a = get_applied_ord_set<OrdPos, e_stack_type::a>(query.get_tar_ord_pos_a_A2B(), prev_all_ord_a, effect);
        RelOrd new_rel_ord_b = get_applied_ord_set<RelOrd, e_stack_type::b>(query.get_tar_rel_ord_b_A2B(), prev_all_ord_b, effect);
        int new_stack_a_cnt = query.get_stack_a_cnt() + effect.diff_stack_a_cnt;
        return cri_factory.build_a2b(tar_v, new_stack_a_cnt, new_ord_pos_a, new_rel_ord_b);
    }

    template<size_t MAX_N>
    QueryCommon build_applied_b2a_common_set(const QueryCommon& query, const QueryOrdEffectSet<MAX_N>& effect) const {
        query.validate_is_b2a_type();
        const int tar_v = query.get_tar_v();
        const AllOrd prev_all_ord_a = query_order.aorder.get_target_order(tar_v);
        const AllOrd prev_all_ord_b = query_order.border.get_target_order(tar_v);
        RelOrd new_rel_ord_a = get_applied_ord_set<RelOrd, e_stack_type::a>(query.get_tar_rel_ord_a_B2A(), prev_all_ord_a, effect);
        OrdPos new_ord_pos_b = get_applied_ord_set<OrdPos, e_stack_type::b>(query.get_tar_ord_pos_b_B2A(), prev_all_ord_b, effect);
        int new_stack_a_cnt = query.get_stack_a_cnt() + effect.diff_stack_a_cnt;
        return cri_factory.build_b2a(tar_v, new_stack_a_cnt, new_rel_ord_a, new_ord_pos_b);
    }

    template<size_t MAX_N>
    QueryCommon build_applied_a2a_common_set(OrdPos new_top_ord_pos_b, const QueryCommon& query, const QueryOrdEffectSet<MAX_N>& effect) const {
        query.validate_is_a2a_type();
        const int tar_v = query.get_tar_v();
        const AllOrd prev_all_ord_a1 = query_order.aorder.get_init_order(tar_v);
        const AllOrd prev_all_ord_a2 = query_order.aorder.get_target_order(tar_v);
        OrdPos new_ord_pos_a1 = get_applied_ord_set<OrdPos, e_stack_type::a>(query.get_tar_ord_pos_a1_A2A(), prev_all_ord_a1, effect);
        RelOrd new_rel_ord_a2 = get_applied_ord_set<RelOrd, e_stack_type::a>(query.get_tar_rel_ord_a2_A2A(), prev_all_ord_a2, effect);
        int new_stack_a_cnt = query.get_stack_a_cnt() + effect.diff_stack_a_cnt;
        int new_stack_b_cnt = get_stack_b_cnt_from_a_cnt(new_stack_a_cnt);
        my_assert(new_stack_b_cnt >= 0);

        validate_OrdPos(new_top_ord_pos_b, new_stack_b_cnt);
        const OrdPos new_ord_pos_a2 = to_ord_pos(new_rel_ord_a2, new_stack_a_cnt);
        const int new_flip_dis_dir = calc_applied_flip_dis_dir_a2a(
            query, new_ord_pos_a1, new_ord_pos_a2, new_stack_a_cnt);
        // _BEGIN: A2AP 廃止に伴い A2AP ブランチを削除。A2AS のみ。
        my_assert(query.get_push_type() == CommonPushType::A2AS);
        return QueryCommon(CommonPushType::A2AS, tar_v, new_stack_a_cnt,
        // _END
            new_ord_pos_a1.get(), new_rel_ord_a2.get(), new_top_ord_pos_b.get(), new_flip_dis_dir);
    }

public:
    template<size_t MAX_N>
    QueryCommon get_applied_common_set(OrdPos new_top_ord_pos_b, const QueryCommon& tar_q, const QueryOrdEffectSet<MAX_N>& effect) const {
        if (tar_q.get_push_type() == CommonPushType::A2B) {
            return build_applied_a2b_common_set<MAX_N>(tar_q, effect);
        }
        if (tar_q.get_push_type() == CommonPushType::B2A) {
            return build_applied_b2a_common_set<MAX_N>(tar_q, effect);
        }
        // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
        my_assert(tar_q.get_push_type() == CommonPushType::A2AS);
        // _END
        return build_applied_a2a_common_set<MAX_N>(new_top_ord_pos_b, tar_q, effect);
    }

    template<size_t MAX_N>
    QueryCommon get_applied_common_set(OrdPos new_top_ord_pos_b, int q_idx, const QueryOrdEffectSet<MAX_N>& effect) const {
        validate_qi(q_idx);
        return get_applied_common_set<MAX_N>(new_top_ord_pos_b, get_query_common(q_idx), effect);
    }

public:
    QueryCommon get_applied_common(OrdPos new_top_ord_pos_b, const QueryCommon &tar_q, const QueryOrdEffect &effect) const {
        if(tar_q.get_push_type() == CommonPushType::A2B){
            return build_applied_a2b_common(tar_q, effect);
        }
        if(tar_q.get_push_type() == CommonPushType::B2A){
            return build_applied_b2a_common(tar_q, effect);
        }
        my_assert(tar_q.get_push_type() == CommonPushType::A2AS);
        return build_applied_a2a_common(new_top_ord_pos_b, tar_q, effect);
    }

    QueryCommon get_applied_common(OrdPos new_top_ord_pos_b, int tar_qi, const QueryOrdEffect &effect) const {
        const QueryCommon& query = get_query_common(tar_qi);
        return get_applied_common(new_top_ord_pos_b, query, effect);
    }

    void validate_tar_revert_qi(int tar_qi, int revert_qi) const {
        my_assert(revert_qi < tar_qi);
    }

    //x
    QueryCommon get_reverted_common(OrdPos new_start_ord_b, int tar_qi, int revert_qi) const {
        validate_qi(tar_qi);
        validate_qi(revert_qi);
        validate_tar_revert_qi(tar_qi, revert_qi);
        return get_applied_common(new_start_ord_b, tar_qi, get_revert_effect(revert_qi));
    }

    //x
    QueryCommon get_reverted_common(OrdPos new_start_ord_b, const QueryCommon &tar_q, const QueryCommon &prev_q) const {
        return get_applied_common(new_start_ord_b, tar_q, get_revert_effect(prev_q));
    }

    //x
    QueryCommon get_applied_common(OrdPos new_start_ord_b, const QueryCommon &tar_q, const QueryCommon &aft_apply_q) const {
        return get_applied_common(new_start_ord_b, tar_q, get_apply_effect(aft_apply_q));
    }

    const QueryCommon& get_prev_common(int tar_qi) const {
        validate_qi(tar_qi);
        my_assert(tar_qi > 0);
        return get_query_common(tar_qi - 1);
    }

    //x
    QueryCommonRI get_prev_reverted_qri(int tar_qi, int revert_qi) const {
        validate_qi(tar_qi);
        validate_qi(revert_qi);
        validate_tar_revert_qi(tar_qi, revert_qi);
        QueryCommon new_tar_q = get_reverted_common(calc_start_ord_b(get_qs(), revert_qi), tar_qi, revert_qi);
        my_assert(revert_qi + 1 == tar_qi);
        if(revert_qi == 0){
            return cri_factory.build_first_ri(get_qs(), new_tar_q);
        }else{
            return cri_factory.build_ri(get_qs(), new_tar_q, revert_qi);
        }
    }
};
