//
//

#ifndef UNTITLED_A2A_CALCULATOR_H
#define UNTITLED_A2A_CALCULATOR_H

#include "Enums.h"
#include "Command/Command.h"
#include "util.h"
#include "RotateCost.h"
#include "BeamEnums.h"

inline int prev_pos(int pos, int size) {
    my_assert(0 <= pos && pos < size);
    if (pos == 0) {
        return size - 1;
    } else {
        return pos - 1;
    }
}

//x
inline pair<command, int> get_best_cmd_cnt(int from_a_pos, int to_a_pos, int a_size) {
    my_assert(0 <= from_a_pos && from_a_pos < a_size);
    my_assert(0 <= to_a_pos && to_a_pos < a_size);
    int ra_dis1 = rotate_dis(from_a_pos, to_a_pos, a_size);
    int rra_dis1 = rev_rotate_dis(from_a_pos, to_a_pos, a_size);
    //0ならraだが、同じ距離なら、rra優先
    if (ra_dis1 == 0) {
        my_assert(ra_dis1 <= rra_dis1 && ra_dis1 == 0);
        return {RA, ra_dis1};
    } else if (ra_dis1 < rra_dis1) {
        return {RA, ra_dis1};
    } else {
        return {RRA, rra_dis1};
    }

}

//a_posに行く最短を返す
inline pair<command, int> get_best_cmd_cnt(int a_pos, int a_size) {
    my_assert(0 <= a_pos && a_pos < a_size);
    return get_best_cmd_cnt(0, a_pos, a_size);
}

inline pair<command, int> get_cmd1_cnt1(e_a2a_type a2a_type, int a_pos, int a_size, command cmd2, int flip_dis_dir) {
    //vをtopにもっていく
    if(a2a_type == e_a2a_type::swap_rot && flip_dis_dir < 0){
        return get_best_cmd_cnt(prev_pos(a_pos, a_size), a_size);
    }else{
        my_assert(a2a_type != e_a2a_type::not_a2a);
        return get_best_cmd_cnt(a_pos, a_size);
    }
}

inline int calc_a2a_by_sa_rot(int inter_cnt) {
    return inter_cnt * 2 - 1;
}

inline int calc_a2a_by_push(int inter_cnt) {
    return inter_cnt + 2;
}

inline int calc_a2a_ins_turn(int inter_cnt) {
    return min(calc_a2a_by_sa_rot(inter_cnt), calc_a2a_by_push(inter_cnt));
}

inline int calc_a2a_ins_turn(int inter_cnt, e_a2a_type a2a_type) {
    if (a2a_type == e_a2a_type::swap_rot) {
        return calc_a2a_by_sa_rot(inter_cnt);
    } else if (a2a_type == e_a2a_type::push_rot) {
        return calc_a2a_by_push(inter_cnt);
    } else {
        my_assert(false);
        return -1;
    }
}

//inline int get_simple_ins_turn(BeamQuery &q, int a_size, e_a2a_type a2a_type) {
//    my_assert(q.get_push_type() == e_push_type::a2a);
//    my_assert(a2a_type == e_a2a_type::swap_rot || a2a_type == e_a2a_type::push_rot);
//    auto [cmd1, cnt1] = get_cmd1_cnt1(a2a_type, q.get_target_a_pos(), a_size, q.get_cmd2());
//    int ins_turn_with_prev_diff = cnt1 + calc_a2a_ins_turn(q.get_inter_cnt(), a2a_type);
//    return ins_turn_with_prev_diff;
//}

#endif //UNTITLED_A2A_CALCULATOR_H
