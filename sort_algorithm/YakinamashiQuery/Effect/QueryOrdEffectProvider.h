#pragma once
#include "QueryOrdEffect.h"
#include "QueryCommon.h"
#include "YakinamashiState.h"
#include "util.h"

class QueryOrdEffectProvider {
    const QueryOrder& query_order_;
    const QueriesState& queries_state_;

    int get_query_count() const {
        return static_cast<int>(queries_state_.queries.size());
    }

    void validate_qi(int qi) const {
        my_assert(0 <= qi && qi < get_query_count());
    }

    const QueryCommon& get_query_common(int qi) const {
        validate_qi(qi);
        return queries_state_.queries[qi].get_query();
    }

    int get_add_a_cnt_diff(CommonPushType push_type) const {
        if (push_type == CommonPushType::A2B) return -1;
        if (push_type == CommonPushType::B2A) return 1;
        // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
        my_assert(push_type == CommonPushType::A2AS);
        // _END
        return 0;
    }

    int get_erased_a_cnt_diff(CommonPushType push_type) const {
        if (push_type == CommonPushType::A2B) return 1;
        if (push_type == CommonPushType::B2A) return -1;
        // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
        my_assert(push_type == CommonPushType::A2AS);
        // _END
        return 0;
    }

public:
    template<size_t MAX_N>
    explicit QueryOrdEffectProvider(const YakinamashiState<MAX_N>& yst_state)
        : query_order_(yst_state.query_order), queries_state_(yst_state.queries_state) {}

    QueryOrdEffect get_apply_effect(const QueryCommon& apply_q) const {
        CommonPushType apply_type = apply_q.get_push_type();
        int tar_v = apply_q.get_tar_v();
        if (apply_type == CommonPushType::A2B) {
            AllOrd del_a = query_order_.aorder.get_init_order(tar_v);
            AllOrd add_b = query_order_.border.get_target_order(tar_v);
            SingleSideOrdEffect a_eff; a_eff.set_del(del_a);
            SingleSideOrdEffect b_eff; b_eff.set_add(add_b);
            return QueryOrdEffect{a_eff, b_eff, get_add_a_cnt_diff(apply_type)};
        }
        if (apply_type == CommonPushType::B2A) {
            AllOrd add_a = query_order_.aorder.get_target_order(tar_v);
            AllOrd del_b = query_order_.border.get_target_order(tar_v);
            SingleSideOrdEffect a_eff; a_eff.set_add(add_a);
            SingleSideOrdEffect b_eff; b_eff.set_del(del_b);
            return QueryOrdEffect{a_eff, b_eff, get_add_a_cnt_diff(apply_type)};
        }
        // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
        my_assert(apply_type == CommonPushType::A2AS);
        // _END
        AllOrd add_a = query_order_.aorder.get_target_order(tar_v);
        AllOrd del_a = query_order_.aorder.get_init_order(tar_v);
        SingleSideOrdEffect a_eff; a_eff.set_add(add_a); a_eff.set_del(del_a);
        return QueryOrdEffect{a_eff, {}, get_add_a_cnt_diff(apply_type)};
    }

    QueryOrdEffect get_apply_effect(int apply_qi) const {
        validate_qi(apply_qi);
        return get_apply_effect(get_query_common(apply_qi));
    }

    QueryOrdEffect get_revert_effect(const QueryCommon& erase_q) const {
        CommonPushType erase_type = erase_q.get_push_type();
        int tar_v = erase_q.get_tar_v();
        if (erase_type == CommonPushType::A2B) {
            AllOrd add_a = query_order_.aorder.get_init_order(tar_v);
            AllOrd del_b = query_order_.border.get_target_order(tar_v);
            SingleSideOrdEffect a_eff; a_eff.set_add(add_a);
            SingleSideOrdEffect b_eff; b_eff.set_del(del_b);
            return QueryOrdEffect{a_eff, b_eff, get_erased_a_cnt_diff(erase_type)};
        }
        if (erase_type == CommonPushType::B2A) {
            AllOrd del_a = query_order_.aorder.get_target_order(tar_v);
            AllOrd add_b = query_order_.border.get_target_order(tar_v);
            SingleSideOrdEffect a_eff; a_eff.set_del(del_a);
            SingleSideOrdEffect b_eff; b_eff.set_add(add_b);
            return QueryOrdEffect{a_eff, b_eff, get_erased_a_cnt_diff(erase_type)};
        }
        // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
        my_assert(erase_type == CommonPushType::A2AS);
        // _END
        AllOrd add_a = query_order_.aorder.get_init_order(tar_v);
        AllOrd del_a = query_order_.aorder.get_target_order(tar_v);
        SingleSideOrdEffect a_eff; a_eff.set_add(add_a); a_eff.set_del(del_a);
        return QueryOrdEffect{a_eff, {}, get_erased_a_cnt_diff(erase_type)};
    }

    QueryOrdEffect get_revert_effect(int erase_qi) const {
        validate_qi(erase_qi);
        return get_revert_effect(get_query_common(erase_qi));
    }

    // why: apply/revertの対称性が崩れるとswap等で無言で壊れるため、構造的に検証する
    void validate_effect_symmetry(const QueryCommon& q) const {
#ifdef DEBUG
        auto apply = get_apply_effect(q);
        auto revert = get_revert_effect(q);
        my_assert(apply.a_effect.has_add() == revert.a_effect.has_del());
        my_assert(apply.a_effect.has_del() == revert.a_effect.has_add());
        if (apply.a_effect.has_add()) my_assert(apply.a_effect.get_add_ord() == revert.a_effect.get_del_ord());
        if (apply.a_effect.has_del()) my_assert(apply.a_effect.get_del_ord() == revert.a_effect.get_add_ord());
        my_assert(apply.b_effect.has_add() == revert.b_effect.has_del());
        my_assert(apply.b_effect.has_del() == revert.b_effect.has_add());
        if (apply.b_effect.has_add()) my_assert(apply.b_effect.get_add_ord() == revert.b_effect.get_del_ord());
        if (apply.b_effect.has_del()) my_assert(apply.b_effect.get_del_ord() == revert.b_effect.get_add_ord());
        my_assert(apply.diff_stack_a_cnt == -revert.diff_stack_a_cnt);
#endif
    }
};
