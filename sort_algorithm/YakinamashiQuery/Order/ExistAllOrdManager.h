// filepath: sort_algorithm/YakinamashiQuery/ExistAllOrdManager.h
#pragma once

#include <bitset>
#include <cstddef>
#include "State.h"
#include "Yakinamashi/Order/OrdTypes.h"
#include "Yakinamashi/Order/ValState.h"
#include "YakinamashiQueryConstants.h"

namespace ExistAllOrdManager {
    template<size_t bit_size>
    void validate_bit_idx(AllOrd all_ord){
        my_assert(0 <= all_ord.get() && all_ord.get() < bit_size); 
    }

    //x
    template<std::size_t bit_size>
    inline bool get_exist(const my_bitset<bit_size>& exist_ords, AllOrd all_ord) {
        validate_bit_idx<bit_size>(all_ord);
        return exist_ords[all_ord.get()];
    }

    //x
    template<std::size_t bit_size>
    inline void set_exist(my_bitset<bit_size>& exist_ords, AllOrd all_ord) {
        validate_bit_idx<bit_size>(all_ord);
        my_assert(!exist_ords[all_ord.get()]);
        exist_ords.set(all_ord.get());
    }

    //x
    template<std::size_t bit_size>
    inline void reset_exist(my_bitset<bit_size>& exist_ords, AllOrd all_ord) {
        validate_bit_idx<bit_size>(all_ord);
        my_assert(exist_ords[all_ord.get()]);
        exist_ords.reset(all_ord.get());
    }

    //x
    template<std::size_t bit_size>
    inline void set_ord(my_bitset<bit_size>& all_ords_bset, AllOrd all_ord) {
        validate_bit_idx<bit_size>(all_ord);
        my_assert(all_ords_bset[all_ord.get()] == 0);
        all_ords_bset.set(all_ord.get());
    }

    //x
    template<std::size_t bit_size>
    inline void rem_ord(my_bitset<bit_size>& all_ords_bset, AllOrd all_ord) {
        validate_bit_idx<bit_size>(all_ord);
        my_assert(all_ords_bset[all_ord.get()] == 1);
        all_ords_bset.reset(all_ord.get());
    }

    template<std::size_t MAX_N, typename AOrderLike, typename TypeArrA>
    inline void init_exist_a_ords_from_state(const State &init_st,
                                             AOrderLike &aorder,
                                             const TypeArrA &now_a_val_state,
                                             my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()> &exist_a_ords) {
        for (int i = 0; i < init_st.A.size(); i++) {
            int av = init_st.A[i];
            AllOrd ord = aorder.get_all_order(av, now_a_val_state[av]);
            set_exist(exist_a_ords, ord);
        }
    }

    template<std::size_t MAX_N, typename AOrderLike, typename BOrderLike, typename TypeArrA, typename TypeArrB>
    inline void upd_ord_b2a(int v,
                            my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()> &exist_a_ords,
                            my_bitset<YQConstants::GET_MAX_ALL_ORD_B<MAX_N>()> &exist_b_ords,
                            AOrderLike &aorder,
                            BOrderLike &border,
                            TypeArrA &now_a_val_state,
                            TypeArrB &now_b_val_state) {
        // _BEGIN: A/B 側の挿入済み状態を TARGET 名へ統一する。
        set_ord(exist_a_ords, aorder.get_all_order(v, ValState::TARGET));
        rem_ord(exist_b_ords, border.get_all_order(v, ValState::TARGET));
        now_a_val_state[v] = ValState::TARGET;
        // _END
    }

    template<std::size_t MAX_N, typename AOrderLike, typename BOrderLike, typename TypeArrA, typename TypeArrB>
    inline void upd_ord_a2b(int v,
                            my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()> &exist_a_ords,
                            my_bitset<YQConstants::GET_MAX_ALL_ORD_B<MAX_N>()> &exist_b_ords,
                            AOrderLike &aorder,
                            BOrderLike &border,
                            TypeArrA &now_a_val_state,
                            TypeArrB &now_b_val_state) {
        rem_ord(exist_a_ords, aorder.get_all_order(v, ValState::INIT));
        set_ord(exist_b_ords, border.get_all_order(v, ValState::TARGET));
        now_b_val_state[v] = ValState::TARGET;
    }

    template<std::size_t MAX_N, typename AOrderLike, typename TypeArrA>
    inline void upd_ord_a2a(int v,
                            my_bitset<YQConstants::GET_MAX_ALL_ORD_A<MAX_N>()> &exist_a_ords,
                            AOrderLike &aorder,
                            TypeArrA &now_a_val_state) {
        rem_ord(exist_a_ords, aorder.get_all_order(v, ValState::INIT));
        set_ord(exist_a_ords, aorder.get_all_order(v, ValState::TARGET));
        now_a_val_state[v] = ValState::TARGET;
    }
}
