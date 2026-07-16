// QueryEffectCalculator.h
// クエリ適用時の効果計算を担当するクラス（旧設計: QueryCommonRIベース）
#pragma once

#include "Enums.h"
#include "ExistAllOrdManager.h"
#include "QueryCommon.h"
#include "State.h"
#include "QueryCommonRI.h"
#include "YakinamashiQueryConstants.h"
#include "QueriesState.h"
#include "YakinamashiState.h"
#include "Yakinamashi/Order/OrdTypes.h"
#include "util.h"
#include "AllOrdCalculator.h"
#include <memory>

// : Query/QueryEffectCalculator.h から Effect/ フォルダへ移動。Effect責務の集約のため。
struct QueryEffect{
    e_stack_type apply_stack_type;
    AllOrd apply_all_ord;
    e_stack_type revert_stack_type;
    AllOrd revert_all_ord;
    int diff_stack_a_cnt;
};


template<size_t MAX_N>
class QueryEffectCalculator {
    void validate_qi(int qi) const {
        my_assert(0 <= qi && qi < (int)yst.queries.size());
    }
public:
    const QueriesState& yst;
    const YakinamashiState<MAX_N>& yst_state;

    QueryEffectCalculator(const QueriesState& yst_, const YakinamashiState<MAX_N>& yst_state_)
        : yst(yst_), yst_state(yst_state_) {}

    // _BEGIN: コメントアウトされていた関数を復元。get_reverted_a_cnt_diffから呼ばれているため。
    template<CommonPushType push_type>
    constexpr int get_applied_a_cnt_diff() const {
        if constexpr(push_type == CommonPushType::A2B) {
            return -1;
        } else if constexpr(push_type == CommonPushType::B2A) {
            return 1;
        } else {
            return 0;
        }
    }
    // _END

    // push_typeのクエリが消えた時の、後ろのクエリのstack_a_cntに与える影響
//x
    template<CommonPushType push_type>
    constexpr int get_reverted_a_cnt_diff() const{
        return -get_applied_a_cnt_diff<push_type>();
    }

//x
    // _BEGIN: AllOrdCalcはQueryCommonを受け取るためget_query()経由で渡すよう修正。
    QueryEffect get_apply_a2b_effect(const QueryCommonRI& a2b_q) const {
        a2b_q.validate_is_a2b_type();
        //AからINIT(tar_v)が消える
        //BにPUSHED(tar_v)が追加される
        AllOrd add_b_ord = AllOrdCalc::get_tar_all_ord_b_A2B(a2b_q.get_query(), yst_state);
        AllOrd del_a_ord = AllOrdCalc::get_tar_all_ord_a_A2B(a2b_q.get_query(), yst_state);
        return QueryEffect{e_stack_type::b, add_b_ord, e_stack_type::a, del_a_ord, get_applied_a_cnt_diff<CommonPushType::A2B>()};
    }
    // _END

//x
    // _BEGIN: AllOrdCalcはQueryCommonを受け取るためget_query()経由で渡すよう修正。
    QueryEffect get_apply_b2a_effect(const QueryCommonRI& b2a_q) const {
        b2a_q.validate_is_b2a_type();
        //AにPUSHED(tar_v)が追加される
        //BからPUSHED(tar_v)が消える
        AllOrd add_a_ord = AllOrdCalc::get_tar_all_ord_a_B2A(b2a_q.get_query(), yst_state);
        AllOrd del_b_ord = AllOrdCalc::get_tar_all_ord_b_B2A(b2a_q.get_query(), yst_state);
        return QueryEffect{e_stack_type::a, add_a_ord, e_stack_type::b, del_b_ord, get_applied_a_cnt_diff<CommonPushType::B2A>()};
    }
    // _END

//x
    // _BEGIN: AllOrdCalcはQueryCommonを受け取るためget_query()経由で渡すよう修正。
    QueryEffect get_apply_a2a_effect(const QueryCommonRI& a2a_q) const {
        a2a_q.validate_is_a2a_type();
        //AにPUSHED(tar_v)が追加される
        //AからINIT(tar_v)が消える
        AllOrd add_a_ord = AllOrdCalc::get_tar_all_ord_a2_A2A(a2a_q.get_query(), yst_state);
        AllOrd del_a_ord = AllOrdCalc::get_tar_all_ord_a1_A2A(a2a_q.get_query(), yst_state);
        return QueryEffect{e_stack_type::a, add_a_ord, e_stack_type::a, del_a_ord, get_applied_a_cnt_diff<CommonPushType::A2AS>()};
    }
    // _END

//x
    // _BEGIN: AllOrdCalcはQueryCommonを受け取るためget_query()経由で渡すよう修正。
    QueryEffect get_revert_a2b_effect(const QueryCommonRI& a2b_q) const {
        a2b_q.validate_is_a2b_type();
        //AにINIT(tar_v)が追加される
        //BからPUSHED(tar_v)が消える
        AllOrd add_a_ord = AllOrdCalc::get_tar_all_ord_a_A2B(a2b_q.get_query(), yst_state);
        AllOrd del_b_ord = AllOrdCalc::get_tar_all_ord_b_A2B(a2b_q.get_query(), yst_state);
        return QueryEffect{e_stack_type::a, add_a_ord, e_stack_type::b, del_b_ord, get_reverted_a_cnt_diff<CommonPushType::A2B>()};
    }
    // _END

//x
    // _BEGIN: AllOrdCalcはQueryCommonを受け取るためget_query()経由で渡すよう修正。
    QueryEffect get_revert_b2a_effect(const QueryCommonRI& b2a_q) const {
        b2a_q.validate_is_b2a_type();
        //AからPUSHED(tar_v)が消える
        //BにPUSHED(tar_v)が追加される
        AllOrd add_b_ord = AllOrdCalc::get_tar_all_ord_b_B2A(b2a_q.get_query(), yst_state);
        AllOrd del_a_ord = AllOrdCalc::get_tar_all_ord_a_B2A(b2a_q.get_query(), yst_state);
        return QueryEffect{e_stack_type::b, add_b_ord, e_stack_type::a, del_a_ord, get_reverted_a_cnt_diff<CommonPushType::B2A>()};
    }
    // _END

//x
    // _BEGIN: AllOrdCalcはQueryCommonを受け取るためget_query()経由で渡すよう修正。
    QueryEffect get_revert_a2a_effect(const QueryCommonRI& a2a_q) const {
        a2a_q.validate_is_a2a_type();
        //AからPUSHED(tar_v)が消える
        //AにINIT(tar_v)が追加される
        AllOrd add_a_ord = AllOrdCalc::get_tar_all_ord_a1_A2A(a2a_q.get_query(), yst_state);
        AllOrd del_a_ord = AllOrdCalc::get_tar_all_ord_a2_A2A(a2a_q.get_query(), yst_state);
        return QueryEffect{e_stack_type::a, add_a_ord, e_stack_type::a, del_a_ord, get_reverted_a_cnt_diff<CommonPushType::A2AS>()};
    }
    // _END

//     //erase_qi番目のクエリをeraseすることによる、以後のクエリに与える影響を返す
//x
    QueryEffect get_revert_effect(int erase_qi) const {
        validate_qi(erase_qi);
        QueryCommonRI &erase_qri = yst.queries[erase_qi];
        CommonPushType erase_type = erase_qri.get_push_type();
        if (erase_type == CommonPushType::A2B){
            return get_revert_a2b_effect(erase_qri);
        }else if (erase_type == CommonPushType::B2A){
            return get_revert_b2a_effect(erase_qri);
        }else {
            // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
            my_assert(erase_type == CommonPushType::A2AS);
            // _END
            return get_revert_a2a_effect(erase_qri);
        }
    }

    //x
    QueryEffect get_apply_effect(int apply_qi) const {
        validate_qi(apply_qi);
        QueryCommonRI &apply_qri = yst.queries[apply_qi];
        CommonPushType apply_type = apply_qri.get_push_type();
        if (apply_type == CommonPushType::A2B){
            return get_apply_a2b_effect(apply_qri);
        }else if (apply_type == CommonPushType::B2A){
            return get_apply_b2a_effect(apply_qri);
        }else {
            // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
            my_assert(apply_type == CommonPushType::A2AS);
            // _END
            return get_apply_a2a_effect(apply_qri);
        }
    }
};
