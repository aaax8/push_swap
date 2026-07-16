// filepath: sort_algorithm/YakinamashiQuery/BestRotInfo.h
#pragma once

#include "Command/Command.h"
#include "util.h"
#include "RotCalculator.h"

// 回転・プッシュ情報をまとめた構造体
struct BestRotInfo {
    command cmd_a, cmd_b;
    int cmd_a_cnt, cmd_b_cnt;
    int ins_turn;
};

//x
inline BestRotInfo build_brp_by_rot(int ra_dis, int rra_dis, int rb_dis, int rrb_dis) {
    int ra_rb_ins = RotCalculator::calc_ra_rb_ins_turn(ra_dis, rb_dis);
    int ra_rrb_ins = RotCalculator::calc_ra_rrb_ins_turn(ra_dis, rrb_dis);
    int rra_rb_ins = RotCalculator::calc_rra_rb_ins_turn(rra_dis, rb_dis);
    int rra_rrb_ins = RotCalculator::calc_rra_rrb_ins_turn(rra_dis, rrb_dis);
    
    int min_cost = ra_rb_ins;
    command cmd_a = RA, cmd_b = RB;
    int a_cnt = ra_dis, b_cnt = rb_dis;
    
    if (chmi(min_cost, ra_rrb_ins)) {
        cmd_a = RA;
        cmd_b = RRB;
        a_cnt = ra_dis;
        b_cnt = rrb_dis;
    }
    if (chmi(min_cost, rra_rb_ins)) {
        cmd_a = RRA;
        cmd_b = RB;
        a_cnt = rra_dis;
        b_cnt = rb_dis;
    }
    if (chmi(min_cost, rra_rrb_ins)) {
        cmd_a = RRA;
        cmd_b = RRB;
        a_cnt = rra_dis;
        b_cnt = rrb_dis;
    }
    
    return BestRotInfo{cmd_a, cmd_b, a_cnt, b_cnt, min_cost};
}

//x
inline BestRotInfo build_brp_by_ord(int stack_a_size, int stack_b_size, OrdPos from_orda, OrdPos from_ordb, OrdPos to_orda, OrdPos to_ordb) {
    int ra_dis = RotCalculator::get_ra_dis(from_orda, to_orda, stack_a_size);
    int rra_dis = RotCalculator::get_rra_dis(from_orda, to_orda, stack_a_size);
    int rb_dis = RotCalculator::get_rb_dis(from_ordb, to_ordb, stack_b_size);
    int rrb_dis = RotCalculator::get_rrb_dis(from_ordb, to_ordb, stack_b_size);
    return build_brp_by_rot(ra_dis, rra_dis, rb_dis, rrb_dis);
}