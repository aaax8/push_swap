//
// a2a 関連ヘルパ
//

#ifndef UNTITLED_A2AHELPER_H
#define UNTITLED_A2AHELPER_H

#include "all_include.h"
#include "State.h"
#include "util.h"
#include "Command/Command.h"
#include "Enums.h"
#include "BeamQuery.h"

//a
//queryがswapの時に、最後ターゲットvが上から2番目になるか(そうでない場合topになる)
inline bool is_if_swap_v_next(const BeamQuery &a2a_query) {
    my_assert(a2a_query.is_valid_query() && a2a_query.is_a2a());
    int flip_dis_dir = a2a_query.get_flip_dis_dir();
    if (flip_dis_dir > 0) {
        return true;
    } else if (flip_dis_dir < 0) {
        return false;
    } else {
        my_assert(false);
        return false;
    }
//
//
//    command cmd2 = a2a_query.get_cmd2(e_a2a_type::swap_rot);
//    int cmd2_cnt = a2a_query.get_cmd2_cnt(e_a2a_type::swap_rot);
//    my_assert(!(cmd2 == RRA && cmd2_cnt == 0));
//    auto [cmd1, q_cnt1] = a2a_query.get_cmd1_cnt1_(a2a_query.get_cur_default_a2a_type(),
//                                                   a2a_query.get_a_size());
//    bool last_v_second = false;
//    //vがtop
//    if (cmd2 == RA) {
//        if (cmd2_cnt > 0) {
//            last_v_second = false;
//        } else {
//            my_assert(cmd2_cnt == 0);
//            int cmd1_i = -1;
//            if (cmd1 == RA) {
//                cmd1_i = q_cnt1;
//            } else if (cmd1 == RRA) {
//                my_assert(q_cnt1 > 0);
//                cmd1_i = state.A.size() - q_cnt1;
//                my_assert(cmd1_i > 0);
//            } else {
//                my_assert(false);
//            }
//            //shosteu
//            if (state.A[cmd1_i] == a2a_query.get_tar_val()) {
//                last_v_second = true;
//            } else {
//                last_v_second = false;
//                my_assert(state.A[mod(cmd1_i + 1, state.A.size())] == a2a_query.get_tar_val());
//            }
//        }
//    } else if (cmd2 == RRA) {
//        last_v_second = false;
//    } else {
//        my_assert(false);
//    }
//    return last_v_second;
}

//ra, rraの必要な数を返す
inline std::pair<int, int> change_type_diff(const State &state, const BeamQuery &a2a_query, e_a2a_type new_type) {
    my_assert(a2a_query.get_push_type() == e_push_type::a2a);
    my_assert(new_type == e_a2a_type::push_rot ||
              new_type == e_a2a_type::swap_rot);
    e_a2a_type prev_type = a2a_query.get_cur_default_a2a_type();
    my_assert(prev_type == e_a2a_type::push_rot ||
              prev_type == e_a2a_type::swap_rot);
    if (prev_type == new_type) {
        return {0, 0};
    }
    bool if_swap_v_nex = is_if_swap_v_next(a2a_query);
    if (!if_swap_v_nex) {
        return {0, 0};
    }
    if (new_type == e_a2a_type::push_rot) {
        return {1, 0};//2つめにあるのを、一番上に戻す
    } else if (new_type == e_a2a_type::swap_rot) {
        return {0, 1};
    } else {
        my_assert(false);
        return {-1, -1};
    }
}

#endif //UNTITLED_A2AHELPER_H


