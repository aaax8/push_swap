//
// Created by Auto
//

#ifndef UNTITLED_BEAMQUERYFACTORY_H
#define UNTITLED_BEAMQUERYFACTORY_H

#include "BeamQuery.h"
#include "Enums.h"
#include "Command/Command.h"
#include "util.h"
#include "a2a_calculator.h"

class BeamQueryFactory {
public:
    static BeamQuery build_a2a(int tar_val, int a_pos,
                               int sorted_a_pos_l, int sorted_a_pos_r,
                               const BeamQuery &prev_query, int a_size) {
        int ra_dis2_ft = rotate_dis(a_pos, sorted_a_pos_l, a_size) - 1;//sorted_a_pos_lの上に持っていきたいから
        int rra_dis2_ft = rev_rotate_dis(a_pos, sorted_a_pos_r, a_size);
        my_assert(0 <= ra_dis2_ft && ra_dis2_ft < a_size);
        my_assert(0 <= rra_dis2_ft && rra_dis2_ft < a_size);

        command cmd2;
        int inter_cnt;
        if (ra_dis2_ft < rra_dis2_ft) {
            cmd2 = RA;
            inter_cnt = ra_dis2_ft;
        } else {
            cmd2 = RRA;
            inter_cnt = rra_dis2_ft;
        }
        my_assert(cmd2 == RA || cmd2 == RRA);
        my_assert(0 <= a_pos && a_pos < a_size);
        my_assert(0 <= inter_cnt && inter_cnt < a_size);
        e_a2a_type prev_a2a_type;
        if (prev_query.is_valid_query() && prev_query.is_a2a()) {
            prev_a2a_type = prev_query.get_cur_default_a2a_type();
        }else{
            prev_a2a_type = e_a2a_type::not_a2a;
        }
        e_a2a_type greedy_a2a_type;
        int ins_turn_sa_rot = calc_a2a_by_sa_rot(inter_cnt);
        int ins_turn_push = calc_a2a_by_push(inter_cnt);
        if (ins_turn_sa_rot <= ins_turn_push) {
            greedy_a2a_type = e_a2a_type::swap_rot;
        } else {
            greedy_a2a_type = e_a2a_type::push_rot;
        }
        int flip_dis_dir = (cmd2 == RA) ? inter_cnt : -inter_cnt;
        auto [cmd1, cnt1] = get_cmd1_cnt1(greedy_a2a_type, a_pos, a_size, cmd2, flip_dis_dir);
        int ins_turn_simple = cnt1 + calc_a2a_ins_turn(inter_cnt, greedy_a2a_type);
        BeamQuery q;
        q.set_tar_val(tar_val);
        q.set_push_type(e_push_type::a2a);
        q.set_ins_turn_with_prev_diff(ins_turn_simple);
        q.set_cmd1(NO_COMMAND);
        q.set_target_a_pos(a_pos);
        q.set_cmd2(cmd2);
        q.set_flip_dis_dir(flip_dis_dir);
        q.set_determined_prev_a2a_type(prev_a2a_type);
        q.set_prev_a2a_ins_turn_diff(0);
        q.set_cur_default_a2a_type(greedy_a2a_type);
        q.set_a_size(a_size);
        return q;
    }
};

#endif //UNTITLED_BEAMQUERYFACTORY_H

