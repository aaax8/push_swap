// OrdCalc.h
// YakinamashiQuery用 Ord計算関数群
#pragma once

#include <bitset>
#include <vector>
#include "Command.h"
#include "ExistAllOrdManager.h"
#include "Yakinamashi/Order/AOrder.h"
#include "Yakinamashi/Order/BOrder.h"
#include "Yakinamashi/Order/ValState.h"
#include "Yakinamashi/Order/OrdTypes.h"
#include "util.h"
// 必要に応じてOrderType等のヘッダをinclude
#include "YakinamashiQueryConstants.h"
#include "OrdValidator.h"

namespace OrdCalc {
    //
    template<size_t bit_size>
    inline RelOrd get_rel_ord(const my_bitset<bit_size> &exist_ords, AllOrd all_ord) {
        return RelOrd(pop_cnt(exist_ords, 0, all_ord.get()));
    }

    template<size_t bit_size, typename OrderGetter>
    inline RelOrd get_rel_ord(const my_bitset<bit_size> &exist_ords, const OrderGetter &order_getter, int tar_v,
                              ValState now_val_state) {
        AllOrd all_orda = order_getter.get_all_order(tar_v, now_val_state);
        return OrdCalc::get_rel_ord(exist_ords, all_orda);
    }


    //get_rel_ordの、対象が存在することを保証する版
    template<size_t bit_size, typename OrderGetter>
    inline RelOrd
    get_rel_ord_assert_exist(const my_bitset<bit_size> &exist_ords, const OrderGetter &order_getter, int tar_v,
                             ValState now_val_state) {
        AllOrd all_orda = order_getter.get_all_order(tar_v, now_val_state);
        my_assert(ExistAllOrdManager::get_exist<bit_size>(exist_ords, all_orda));
        return OrdCalc::get_rel_ord<bit_size>(exist_ords, all_orda);
    }

    inline RelOrd get_normalize_rel_ord(int may_overflow_val, int stack_size) {
        int val = may_overflow_val;
        if (may_overflow_val > stack_size) {
            val = may_overflow_val - stack_size;
        } else if (may_overflow_val < 0) {
            val = may_overflow_val + stack_size;
        }
        RelOrd ret = RelOrd(val);
        validate_RelOrd(ret, stack_size);
        return ret;
    }

    inline OrdPos get_normalized_ord_pos(int may_overflow_val, int stack_size) {
        int val = may_overflow_val;
        if (may_overflow_val >= stack_size) {
            val = may_overflow_val - stack_size;
        } else if (may_overflow_val < 0) {
            val = may_overflow_val + stack_size;
        }
        OrdPos ret = OrdPos(val);
        validate_OrdPos(ret, stack_size);
        return ret;
    }

    //
    inline OrdPos get_ord_add(OrdPos rel_ord, int offset, int stack_size) {
        validate_OrdPos(rel_ord, stack_size);
        validate_OrdPos(abs(offset), stack_size);
        return get_normalized_ord_pos(rel_ord.get() + offset, stack_size);
    }

    inline OrdPos get_ord_pos_dec(OrdPos rel_ord, int offset, int stack_size) {
        return get_ord_add(rel_ord, -offset, stack_size);
    }


    inline OrdPos get_ord_pos_prev(OrdPos ord_pos, int stack_size) {
        return get_ord_pos_dec(ord_pos, 1, stack_size);
    }

    inline RelOrd get_rel_ord_add(RelOrd rel_ord, int offset, int stack_size) {
        validate_RelOrd(rel_ord, stack_size);
        validate_OrdPos(abs(offset), stack_size);
        return get_normalize_rel_ord(rel_ord.get() + offset, stack_size);
    }

    inline RelOrd get_rel_ord_dec(RelOrd rel_ord, int offset, int stack_size) {
        return get_rel_ord_add(rel_ord, -offset, stack_size);
    }

    inline RelOrd get_rel_ord_prev(RelOrd rel_ord, int stack_size) {
        return get_rel_ord_dec(rel_ord, 1, stack_size);
    }

    inline OrdPos get_ord_pos_next(OrdPos rel_ord, int stack_size) {
        return get_ord_add(rel_ord, 1, stack_size);
    }

    //ord1 - ord2
    inline int get_ord_pos_dec(OrdPos rel_ord1, OrdPos rel_ord2, int stack_size) {
        validate_OrdPos(rel_ord1, stack_size);
        validate_OrdPos(rel_ord2, stack_size);
        return get_normalized_ord_pos(rel_ord1.get() - rel_ord2.get(), stack_size).get();
    }

    //
    inline OrdPos get_rotated_ord(OrdPos now_rel_ord, int stack_size, command rot_cmd, int cnt) {
        validate_OrdPos(now_rel_ord, stack_size);
        validate_OrdPos(abs(cnt), stack_size);
        if (rot_cmd == RA || rot_cmd == RB) {
            return get_ord_add(now_rel_ord, cnt, stack_size);
        } else if (rot_cmd == RRA || rot_cmd == RRB) {
            return get_ord_add(now_rel_ord, -cnt, stack_size);
        } else {
            my_assert(false);
            return OrdPos(0);
        }
    }

    // 
    //rel_ordを挿入したときにtopのordがどうなるか
    inline OrdPos get_inserted_ord(RelOrd rel_ord) {
        //RelOrdは現状のスタックとの相対的な順序なので、挿入後のordも同じになる
        return OrdPos(rel_ord.get());
    }

    template<class OrdType>
    inline OrdType get_added_ord(OrdType tar_ord, AllOrd tar_all_ord, AllOrd added_all_ord) {
        my_assert(tar_all_ord.get() != added_all_ord.get());
        if (tar_all_ord.get() > added_all_ord.get()) {
            return OrdType(tar_ord.get() + 1);
        } else {
            return tar_ord;
        }
    }


    template<class OrdType>
    inline OrdType get_erased_ord(OrdType tar_ord, AllOrd tar_all_ord, AllOrd erase_all_ord) {
        my_assert(tar_all_ord.get() != erase_all_ord.get());
        if (tar_all_ord.get() > erase_all_ord.get()) {
            my_assert(tar_ord.get() > 0);
            return OrdType(tar_ord.get() - 1);
        } else {
            return tar_ord;
        }
    }

    //x
    //erase_ordを削除したときの、新しいrel_ordのordを返す
    inline OrdPos get_erased_ord(OrdPos rel_ord, OrdPos erase_ord) {
        my_assert(rel_ord.get() != erase_ord.get());
        if (rel_ord.get() < erase_ord.get()) {
            return rel_ord;
        } else {
            return OrdPos(rel_ord.get() - 1);
        }
    }

    //x
    inline OrdPos get_erased_next_ord(OrdPos ord_pos, int stack_size) {
        validate_OrdPos(ord_pos.get(), stack_size);
        if (stack_size == 0) {
            return OrdPos(0);
        } else {
            if (ord_pos.get() == stack_size - 1) {
                return OrdPos(0);
            } else {
                validate_OrdPos(ord_pos.get(), stack_size - 1);
                return ord_pos;
            }
        }
    }

    //x 
    template<size_t MAX_N>
    static OrdPos get_tar_ord_posa_of_a2b(
            my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()> &exist_a_ords,
            AOrder &aorder, int tar_v, ValState now_tar_orda_val_state,
            int stack_a_size
    ) {
        my_assert(is_init(now_tar_orda_val_state));
#ifdef DEBUG
        AllOrd all_orda = aorder.get_all_order(tar_v, now_tar_orda_val_state);
        my_assert(exist_a_ords[all_orda.get()] == 1);
#endif
        return to_ord_pos(get_rel_ord(exist_a_ords, aorder, tar_v, now_tar_orda_val_state), stack_a_size);
    }

    //x
    template<size_t MAX_N>
    static RelOrd get_tar_rel_ordb_of_a2b(
            my_bitset<YQConstants::GET_MAX_ALL_ORD_B<MAX_N>()> &exist_b_ords,
            BOrder &border, int tar_v, int stack_b_size) {
        my_assert(!ExistAllOrdManager::get_exist(exist_b_ords, border.get_all_order(tar_v, ValState::TARGET)));
        int rel_ord_b = get_rel_ord(exist_b_ords, border.get_target_order(tar_v)).get();
        validate_RelOrd(rel_ord_b, stack_b_size);
        return RelOrd(rel_ord_b);
    }

    //x
    template<size_t MAX_N>
    static RelOrd get_tar_rel_orda_of_b2a(
            my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()> &exist_a_ords,
            AOrder &aorder, int tar_v, int stack_a_size) {
        RelOrd rel_ord_a = get_rel_ord(exist_a_ords, aorder.get_target_order(tar_v));
        validate_RelOrd(rel_ord_a, stack_a_size);
        return rel_ord_a;
    }

    //x
    template<size_t MAX_N>
    static OrdPos get_tar_ord_posb_of_b2a(
            my_bitset<YQConstants::GET_MAX_ALL_ORD_B<MAX_N>()> &exist_b_ords,
            BOrder &border, int tar_v, ValState now_tar_ordb_val_state,
            int stack_b_size
    ) {
        my_assert(now_tar_ordb_val_state == ValState::TARGET);
        AllOrd all_ordb = border.get_all_order(tar_v, now_tar_ordb_val_state);
        my_assert(exist_b_ords[all_ordb.get()] == 1);
        return to_ord_pos(get_rel_ord(exist_b_ords, all_ordb), stack_b_size);
    }

    //x
    template<size_t MAX_N>
    static OrdPos get_tar_orda1_of_a2a(
            my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()> &exist_a_ords,
            AOrder &aorder, int tar_v, int stack_a_size, ValState now_tar_orda_val_state) {
        my_assert(is_init(now_tar_orda_val_state));
        OrdPos ord_posa = to_ord_pos(get_rel_ord(exist_a_ords, aorder, tar_v, now_tar_orda_val_state), stack_a_size);
        validate_OrdPos(ord_posa, stack_a_size);
        return ord_posa;
    }

    //x
    template<size_t MAX_N>
    static RelOrd get_tar_rel_orda2_of_a2a(
            my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()> &exist_a_ords,
            AOrder &aorder, int tar_v, int stack_a_size, ValState now_tar_orda_val_state) {
        my_assert(is_init(now_tar_orda_val_state));
        RelOrd rel_orda2 = get_rel_ord(exist_a_ords, aorder, tar_v, ValState::TARGET);
        validate_RelOrd(rel_orda2, stack_a_size);
        return rel_orda2;
    }
}
