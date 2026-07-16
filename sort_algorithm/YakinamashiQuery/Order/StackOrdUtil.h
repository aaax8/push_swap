// StackOrdUtil.h
// stack状態をord列へ変換し、表示・検証するユーティリティ
#pragma once

#include "State.h"
#include "sort_algorithm/Yakinamashi/Order/AOrder.h"
#include "sort_algorithm/Yakinamashi/Order/BOrder.h"
#include <algorithm>
#include <map>
#include <utility>

namespace StackOrdUtil {
    //x
    inline my_vector<int> get_compress(const my_vector<int> &values) {
        std::map<int, int> id_map;
        for (int v: values) {
            id_map[v];
        }

        int id = 0;
        for (auto &[key, mapped_id]: id_map) {
            mapped_id = id++;
        }

        my_vector<int> compressed;
        for (int v: values) {
            compressed.push_back(id_map[v]);
        }
        return compressed;
    }

    //x
    template<size_t MAX_N>
    inline std::pair<my_vector<int>, my_vector<int>>
    get_stack_ord(State &st,
                  AOrder &aorder,
                  BOrder &border,
                  my_vector<ValState> &now_a_val_state,
                  my_vector<ValState> &now_b_val_state) {
        my_vector<int> a_all_ords;
        for (int i = 0; i < st.A.size(); i++) {
            int v = st.A[i];
            AllOrd all_ord = aorder.get_all_order(v, now_a_val_state[v]);
            a_all_ords.push_back(all_ord.get());
        }

        my_vector<int> b_all_ords;
        for (int i = 0; i < st.B.size(); i++) {
            int v = st.B[i];
            AllOrd all_ord = border.get_all_order(v, now_b_val_state[v]);
            b_all_ords.push_back(all_ord.get());
        }

        return {get_compress(a_all_ords), get_compress(b_all_ords)};
    }

    //x
    template<size_t MAX_N>
    inline void print_stack_ord(State &st,
                                AOrder &aorder,
                                BOrder &border,
                                my_vector<ValState> &now_a_val_state,
                                my_vector<ValState> &now_b_val_state) {
        auto [a_stack_ords, b_stack_ords] = get_stack_ord<MAX_N>(st, aorder, border, now_a_val_state, now_b_val_state);
        out("A(ord):", a_stack_ords);
        out("B(ord):", b_stack_ords);
    }

    //x
    template<size_t MAX_N>
    inline bool validate_ring_sorted_stack_ords(State &st,
                                                AOrder &aorder,
                                                BOrder &border,
                            my_vector<ValState> &now_a_val_state,
                            my_vector<ValState> &now_b_val_state) {
#ifndef DEBUG
        return true;
#endif
        auto [a_all_ords, b_all_ords] = get_stack_ord<MAX_N>(st, aorder, border, now_a_val_state, now_b_val_state);

        out("A(v rawAllOrd valSt):");
        for (int i = 0; i < (int)st.A.size(); i++) {
            int v = st.A[i];
            out(v, aorder.get_all_order(v, now_a_val_state[v]).get(), (int)now_a_val_state[v]);
        }
        out("B(v rawAllOrd valSt):");
        for (int i = 0; i < (int)st.B.size(); i++) {
            int v = st.B[i];
            out(v, border.get_all_order(v, now_b_val_state[v]).get(), (int)now_b_val_state[v]);
        }

        auto chk_is_sorted = [](auto &ords)->bool {
            if (ords.empty()) {
                return true;
            }
            auto min_it = std::min_element(ords.begin(), ords.end());
            int min_i = (int) (min_it - ords.begin());
            int prev_ord = -1;
            for (int loop_i = 0; loop_i < ords.size(); loop_i++) {
                int i = mod(loop_i + min_i, ords.size());
                int ord = ords[i];
                if(!(prev_ord < ord)){
                    return false;
                }
                prev_ord = ord;
            }
            return true;
        };

        if(!chk_is_sorted(a_all_ords)){
            return false;
        }
        if(!chk_is_sorted(b_all_ords)){
            return false;
        }
        return true;
    }
}
