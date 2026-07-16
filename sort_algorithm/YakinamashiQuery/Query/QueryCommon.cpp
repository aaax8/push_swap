// YakinamashiQuery.cpp
#include "QueryCommon.h"
#include "InitBeam/BeamEnums.h"
#include "A2AQueryUtil.h"
#include "OrdPosCalculator.h"
#include "util.h"

void QueryCommon::set_top_ord_pos_b_A2A(OrdPos ord_pos) {
    validate_is_a2a_type();
    my_assert(ord_pos.get() >= 0);
    ord3 = ord_pos.get();
}
    
// : A2A の finished_top_ord_a は直前 query 依存なので、ここでは AB のみを返す。
OrdPos QueryCommon::get_finished_top_ord_a(int N) const {
    if (push_type == CommonPushType::A2B) {
        return OrdCalc::get_erased_next_ord(get_tar_ord_pos_a_A2B(), get_stack_a_cnt());
    }
    if (push_type == CommonPushType::B2A) {
        return OrdCalc::get_inserted_ord(get_tar_rel_ord_a_B2A());
    }
    my_assert(push_type == CommonPushType::A2AS);
    my_assert(false);
    return OrdPos(0);
}

// 責務: A2A 後に次 query の start として使う A 側 finished top OrdPos を返す。
// 必要な理由: flip_dis_dir==0 の境界 A2A で後続 query の回転開始位置がずれないようにするため。
OrdPos QueryCommon::get_finished_top_ord_a_a2a(OrdPos start_top_ord_a) const{
    return A2AQueryUtil::calc_a2a_finished_top(
            start_top_ord_a,
            get_tar_ord_pos_a1_A2A(),
            get_tar_rel_ord_a2_A2A(),
            get_stack_a_cnt(),
            get_flip_dis_dir());
}

OrdPos QueryCommon::get_finished_top_ord_b(int N) const {
    if (push_type == CommonPushType::A2B) {
        return OrdCalc::get_inserted_ord(get_tar_rel_ord_b_A2B());
    }
    if (push_type == CommonPushType::B2A) {
        const int stack_b_cnt = N - get_stack_a_cnt();
        my_assert(stack_b_cnt >= 0);
        if (stack_b_cnt == 0) {
            return OrdPos(0);
        }
        return OrdCalc::get_erased_next_ord(get_tar_ord_pos_b_B2A(), stack_b_cnt);
    }
    // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
    my_assert(push_type == CommonPushType::A2AS);
    // _END
    const int stack_b_cnt = N - get_stack_a_cnt();
    my_assert(stack_b_cnt >= 0);
    if (stack_b_cnt == 0) {
        return OrdPos(0);
    }
    validate_OrdPos(get_top_ord_pos_b(), stack_b_cnt);
    return get_top_ord_pos_b();
}
