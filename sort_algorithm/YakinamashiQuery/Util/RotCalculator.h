// filepath: sort_algorithm/YakinamashiQuery/RotCalculator.h
#pragma once

#include "OrdPosCalculator.h"
#include "all_include.h"
#include "OrdValidator.h"

namespace RotCalculator{

  


    //x
    inline int calc_ra_rb_ins_turn(int ra_dis, int rb_dis){
        return max(ra_dis, rb_dis) + 1;
    }

    //x
    inline int calc_ra_rrb_ins_turn(int ra_dis, int rrb_dis){
        return ra_dis + rrb_dis + 1;
    }

    //x
    inline int calc_rra_rb_ins_turn(int rra_dis, int rb_dis){
        return rra_dis + rb_dis + 1;
    }
    
    //x
    inline int calc_rra_rrb_ins_turn(int rra_dis, int rrb_dis){
        return max(rra_dis, rrb_dis) + 1;
    }


    inline int get_rot_dis(OrdPos ord_from, OrdPos ord_to, int stack_size) {
        validate_OrdPos(ord_from, stack_size);
        validate_OrdPos(ord_to, stack_size);
        return OrdCalc::get_ord_pos_dec(ord_to, ord_from, stack_size);
    }

    inline int get_rev_rot_dis(OrdPos ord_from, OrdPos ord_to, int stack_size) {
        validate_OrdPos(ord_from, stack_size);
        validate_OrdPos(ord_to, stack_size);
        return OrdCalc::get_ord_pos_dec(ord_from, ord_to, stack_size);
    }

    // ord_fromからord_toに到達するのにRA操作が何回必要か
    //x
    inline int get_ra_dis(OrdPos ord_from, OrdPos ord_to, int stack_a_size) {
        return get_rot_dis(ord_from, ord_to, stack_a_size);
    }

    // ord_fromからord_toに到達するのにRRA操作が何回必要か
    //x
    inline int get_rra_dis(OrdPos ord_from, OrdPos ord_to, int stack_a_size) {
        return get_rev_rot_dis(ord_from, ord_to, stack_a_size);
    }

    // ord_fromからord_toに到達するのにRB操作が何回必要か
    //x
    inline int get_rb_dis(OrdPos ord_from, OrdPos ord_to, int stack_b_size) {
        return get_rot_dis(ord_from, ord_to, stack_b_size);
    }

    // ord_fromからord_toに到達するのにRRB操作が何回必要か
    //x
    inline int get_rrb_dis(OrdPos ord_from, OrdPos ord_to, int stack_b_size) {
        return get_rev_rot_dis(ord_from, ord_to, stack_b_size);
    }

    
    inline pair<command, int> get_best_rot(OrdPos ord_from, OrdPos ord_to, int stack_size, command rot_cmd, command rev_rot_cmd) {
        int rot_dis = get_rot_dis(ord_from, ord_to, stack_size);
        int rev_rot_dis = get_rev_rot_dis(ord_from, ord_to, stack_size);
        if (rot_dis == 0) {
            my_assert(rot_dis <= rev_rot_dis && rot_dis == 0);
            return {rot_cmd, rot_dis};
        } else if (rot_dis < rev_rot_dis) {
            return {rot_cmd, rot_dis};
        } else {
            return {rev_rot_cmd, rev_rot_dis};
        }
    }

    inline pair<command, int> get_best_rot_a(OrdPos ord_from, OrdPos ord_to, int stack_a_size) {
        validate_OrdPos(ord_from, stack_a_size);
        validate_OrdPos(ord_to, stack_a_size);
        return get_best_rot(ord_from, ord_to, stack_a_size, RA, RRA);
    }

    inline pair<command, int> get_best_rot_b(OrdPos ord_from, OrdPos ord_to, int stack_b_size) {
        validate_OrdPos(ord_from, stack_b_size);
        validate_OrdPos(ord_to, stack_b_size);
        return get_best_rot(ord_from, ord_to, stack_b_size, RB, RRB);
    }

    
}