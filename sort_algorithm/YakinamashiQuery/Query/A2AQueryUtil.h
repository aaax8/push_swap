// A2AQueryUtil.h
// A2A Query生成に必要な計算と検証のユーティリティ
#pragma once

#include "OrdPosCalculator.h"
#include "Yakinamashi/Order/AOrder.h"
#include "Yakinamashi/Order/ValState.h"
#include "YakinamashiQueryConstants.h"
#include "RotCalculator.h"
#include "Command/Command.h"
#include "InitBeam/BeamQuery.h"
#include <bitset>

namespace A2AQueryUtil {
    struct A2ACalcResult {
        OrdPos tar_orda1;
        RelOrd tar_rel_orda2;
        int flip_dis_dir;
        e_a2a_type a2a_type;
    };

    //x
    inline e_a2a_type get_determined_a2a_type(const my_vector<BeamQuery> &queries, int query_idx) {
        e_a2a_type a2a_type = queries[query_idx].get_cur_default_a2a_type();
        if (query_idx + 1 < (int) queries.size()) {
            const BeamQuery &next_query = queries[query_idx + 1];
            my_assert(next_query.is_valid_query());
            a2a_type = next_query.get_determined_prev_a2a_type();
        }
        return a2a_type;
    }

    //x
    inline OrdPos get_tar_ord_pos_a2(OrdPos tar_orda1, int flip_dis_dir, int a_size) {
        if (flip_dis_dir == 0) {
            return tar_orda1;
        }
        if (flip_dis_dir > 0) {
            return OrdCalc::get_ord_add(tar_orda1, flip_dis_dir + 1, a_size);
        }
        return OrdCalc::get_ord_add(tar_orda1, flip_dis_dir, a_size);
    }

    // 責務: A2A が command no-cmd でも target の OrdPos 移動で他 OrdPos をずらす境界ケースか返す。
    // 必要な理由: command no-cmd と order no-cmd を同一視せず finished top を補正するため。
    inline bool is_zero_flip_boundary(OrdPos tar_ord_pos_a1, RelOrd tar_rel_ord_a2, int stack_a_cnt) {
        my_assert(stack_a_cnt > 0);
        validate_OrdPos(tar_ord_pos_a1, stack_a_cnt);
        validate_RelOrd(tar_rel_ord_a2, stack_a_cnt);
        return (tar_ord_pos_a1.get() == 0 && tar_rel_ord_a2.get() == stack_a_cnt) ||
               (tar_ord_pos_a1.get() == stack_a_cnt - 1 && tar_rel_ord_a2.get() == 0);
    }

    // 責務: A2A target を tar_ord_pos_a1 から tar_rel_ord_a2 へ移した後の target OrdPos を返す。
    // 必要な理由: 同じ計算を複数箇所に重複させず、境界挿入の扱いを揃えるため。
    inline OrdPos calc_inserted_a2a_ord(OrdPos tar_ord_pos_a1, RelOrd tar_rel_ord_a2, int stack_a_cnt) {
        my_assert(stack_a_cnt > 0);
        validate_OrdPos(tar_ord_pos_a1, stack_a_cnt);
        validate_RelOrd(tar_rel_ord_a2, stack_a_cnt);
        if (to_rel_ord(tar_ord_pos_a1) < tar_rel_ord_a2) {
            return to_ord_pos(RelOrd(tar_rel_ord_a2.get() - 1), stack_a_cnt);
        }
        return OrdPos(tar_rel_ord_a2.get());
    }

    // 責務: A2A 後に次 query の start として使う A 側 finished top OrdPos を返す。
    // 必要な理由: flip_dis_dir==0 の境界 A2A で後続 query の回転開始位置がずれないようにするため。
    inline OrdPos calc_a2a_finished_top(OrdPos start_top_ord_a,
                                        OrdPos tar_ord_pos_a1,
                                        RelOrd tar_rel_ord_a2,
                                        int stack_a_cnt,
                                        int flip_dis_dir) {
        my_assert(stack_a_cnt > 0);
        validate_OrdPos(start_top_ord_a, stack_a_cnt);
        validate_OrdPos(tar_ord_pos_a1, stack_a_cnt);
        validate_RelOrd(tar_rel_ord_a2, stack_a_cnt);
        if (flip_dis_dir == 0) {
            if (stack_a_cnt == 1) {
                return start_top_ord_a;
            }
            if (is_zero_flip_boundary(tar_ord_pos_a1, tar_rel_ord_a2, stack_a_cnt)) {
                if (tar_ord_pos_a1.get() == 0) {
                    return OrdCalc::get_ord_pos_prev(start_top_ord_a, stack_a_cnt);
                }
                return OrdCalc::get_ord_pos_next(start_top_ord_a, stack_a_cnt);
            }
            return start_top_ord_a;
        }
        const OrdPos inserted_ord = calc_inserted_a2a_ord(tar_ord_pos_a1, tar_rel_ord_a2, stack_a_cnt);
        if (flip_dis_dir < 0) {
            return inserted_ord;
        }
        return OrdCalc::get_ord_pos_prev(inserted_ord, stack_a_cnt);
    }

    //x
    template<size_t MAX_N>
    inline A2ACalcResult calc_a2a_result(const my_vector<BeamQuery> &beam_queries,
                                         int bq_i,
                                         int tar_v,
                                         my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()> &exist_a_ords,
                                         AOrder &aorder,
                                         const my_vector<ValState> &now_a_val_state,
                                         int stack_a_size) {
        const BeamQuery &bq = beam_queries[bq_i];
        OrdPos tar_orda1 = OrdCalc::get_tar_orda1_of_a2a<MAX_N>(
                exist_a_ords,
                aorder,
                tar_v,
                stack_a_size,
                now_a_val_state[tar_v]
        );
        RelOrd tar_rel_orda2 = OrdCalc::get_tar_rel_orda2_of_a2a<MAX_N>(
                exist_a_ords,
                aorder,
                tar_v,
                stack_a_size,
                now_a_val_state[tar_v]
        );
        int flip_dis_dir = bq.get_flip_dis_dir();
        e_a2a_type a2a_type = get_determined_a2a_type(beam_queries, bq_i);
        return {tar_orda1, tar_rel_orda2, flip_dis_dir, a2a_type};
    }

    // flip_dis_dirを計算する（最短方向を自動選択）
    inline int calc_flip_dis_dir(OrdPos tar_orda1, OrdPos tar_orda2, int a_size, e_a2a_type a2a_type) {
        if (tar_orda1 == tar_orda2) {
            return 0;
        }
        int ra_inter_cnt = RotCalculator::get_ra_dis(tar_orda1, tar_orda2, a_size) - 1;
        int rra_inter_cnt = RotCalculator::get_rra_dis(tar_orda1, tar_orda2, a_size);
        my_assert(0 <= ra_inter_cnt && ra_inter_cnt < a_size);
        my_assert(0 <= rra_inter_cnt && rra_inter_cnt < a_size);

        command cmd1;
        int inter_cnt;
        if (ra_inter_cnt < rra_inter_cnt) {
            cmd1 = RA;
            inter_cnt = ra_inter_cnt;
        } else {
            cmd1 = RRA;
            inter_cnt = rra_inter_cnt;
        }
        int flip_dis_dir = (cmd1 == RA) ? inter_cnt : -inter_cnt;
        return flip_dis_dir;
    }

    // 方向を指定してflip_dis_dirを計算する
    // direction=RAの場合は正の値、direction=RRAの場合は負の値を返す
    inline int calc_flip_dis_dir_with_direction(OrdPos tar_orda1, OrdPos tar_orda2,
                                                 int a_size, command direction) {
        if (tar_orda1 == tar_orda2) {
            return 0;
        }
        if (direction == RA) {
            int ra_inter_cnt = RotCalculator::get_ra_dis(tar_orda1, tar_orda2, a_size) - 1;
            my_assert(0 <= ra_inter_cnt && ra_inter_cnt < a_size);
            return ra_inter_cnt;
        }
        my_assert(direction == RRA);
        int rra_inter_cnt = RotCalculator::get_rra_dis(tar_orda1, tar_orda2, a_size);
        my_assert(0 <= rra_inter_cnt && rra_inter_cnt < a_size);
        return -rra_inter_cnt;
    }

    //x
    inline void validate_a2a_result(const A2ACalcResult &calc, int stack_a_size) {
#ifdef DEBUG
        OrdPos tar_orda2 = get_tar_ord_pos_a2(calc.tar_orda1, calc.flip_dis_dir, stack_a_size);
        my_assert(to_ord_pos(calc.tar_rel_orda2, stack_a_size) == tar_orda2);
#endif
    }
}
