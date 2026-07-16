//
// Created by Auto
//

#ifndef UNTITLED_BEAMSTATEVALIDATOR_H
#define UNTITLED_BEAMSTATEVALIDATOR_H

#include "all_include.h"
#include "State.h"
#include "BeamState.h"
#include "BeamQuery.h"
#include "RotateCost.h"

template<int MAX_N>
void validate_st_beam_st_without_query(const State &st, const BeamState<MAX_N> &beam_st) {
#ifdef DEBUG
    my_assert(st.A.size());
    my_assert(beam_st.min_a_pos >= 0);
    my_assert(beam_st.min_a_pos < st.A.size());
    my_assert(st.A[beam_st.min_a_pos] == beam_st.min_a_v);
#endif
}

template<int MAX_N>
void validate_st_beam_st(const State &st, const BeamState<MAX_N> &beam_st) {
#ifdef DEBUG
    validate_st_beam_st_without_query(st, beam_st);
    if (beam_st.last_query.get_tar_val() == -1) {
        my_assert(st.A.size() == st.current_N);
        my_assert(beam_st.act_cnt == 0 && beam_st.parent_idx == -1);
    } else {
        if (beam_st.last_query.get_push_type() == e_push_type::a2b) {
            my_assert(st.B.size() > 0);
            my_assert(st.B[0] == beam_st.last_query.get_tar_val());
        } else if (beam_st.last_query.get_push_type() == e_push_type::b2a) {
            my_assert(st.A[0] == beam_st.last_query.get_tar_val());
        } else if (beam_st.last_query.get_push_type() == e_push_type::a2a) {
            my_assert(0);
        } else {
            my_assert(0);
        }
    }
#endif
}

inline void validate_ins_turn(const BeamQuery &q) {
#ifndef DEBUG
    return;
#endif
    if (q.get_push_type() == e_push_type::a2b) {
        int predict_ins_turn;
        if (q.get_a_cmd() == RA && q.get_b_cmd() == RB) {
            predict_ins_turn = max(q.get_a_count(), q.get_b_count()) + 1;
        } else if (q.get_a_cmd() == RA && q.get_b_cmd() == RRB) {
            predict_ins_turn = q.get_a_count() + q.get_b_count() + 1;
        } else if (q.get_a_cmd() == RRA && q.get_b_cmd() == RB) {
            predict_ins_turn = q.get_a_count() + q.get_b_count() + 1;
        } else {
            predict_ins_turn = max(q.get_a_count(), q.get_b_count()) + 1;
        }
        my_assert(predict_ins_turn == q.get_ins_turn_with_prev_diff());
    } else if (q.get_push_type() == e_push_type::a2a) {
        int predict_ins_turn = calc_a2a_ins_turn_with_type(q, q.get_cur_default_a2a_type());
        my_assert(predict_ins_turn == q.get_ins_turn_with_prev_diff());
    } else {
        my_assert(false);
    }
}

inline void
validate_ins_turn(const BeamQuery &q, const my_vector<BeamQuery> &prev_a2a_queries, const PrevA2AInfo &prev_info) {
#ifndef DEBUG
    (void)prev_a2a_queries;
#endif
    my_assert(q.get_push_type() == e_push_type::b2a || q.get_push_type() == e_push_type::a2b);

    e_a2a_type determined_type = q.get_determined_prev_a2a_type();

    int prev_ra_sum = 0;
    int prev_rra_sum = 0;
    int prev_a2a_ins_turn_diff = 0;

    if (prev_info.is_prev_a2a()) {
        my_assert(determined_type == e_a2a_type::push_rot || determined_type == e_a2a_type::swap_rot);
        prev_ra_sum = (determined_type == e_a2a_type::push_rot) ? prev_info.push_prev_ra_sum
                                                                : prev_info.swap_prev_ra_sum;
        prev_rra_sum = (determined_type == e_a2a_type::push_rot) ? prev_info.push_prev_rra_sum
                                                                 : prev_info.swap_prev_rra_sum;
        prev_a2a_ins_turn_diff = (determined_type == e_a2a_type::push_rot) ? prev_info.if_push_prev_diff
                                                                           : prev_info.if_swap_prev_diff;
    }

    int rb_dis_base = 0;
    int rrb_dis_base = 0;
    if (q.get_b_cmd() == RB) {
        rb_dis_base = q.get_b_count();
    } else if (q.get_b_cmd() == RRB) {
        rrb_dis_base = q.get_b_count();
    } else {
        my_assert(false);
    }

    int rem_rb = u0(rb_dis_base - prev_ra_sum);
    int rem_rrb = u0(rrb_dis_base - prev_rra_sum);

    int ra_dis = q.get_a_count();
    int rra_dis = q.get_a_count();
    if (q.get_a_cmd() == RA) {
        rra_dis = 0;
    } else if (q.get_a_cmd() == RRA) {
        ra_dis = 0;
    } else {
        my_assert(false);
    }

    int predict_ins_turn_simple;
    if (q.get_a_cmd() == RA && q.get_b_cmd() == RB) {
        predict_ins_turn_simple = calc_joint_rot_cost(ra_dis, rem_rb, true) + 1;
    } else if (q.get_a_cmd() == RRA && q.get_b_cmd() == RRB) {
        predict_ins_turn_simple = calc_joint_rot_cost(rra_dis, rem_rrb, true) + 1;
    } else if (q.get_a_cmd() == RA && q.get_b_cmd() == RRB) {
        predict_ins_turn_simple = calc_joint_rot_cost(ra_dis, rem_rrb, false) + 1;
    } else {
        predict_ins_turn_simple = calc_joint_rot_cost(rra_dis, rem_rb, false) + 1;
    }

    my_assert(prev_a2a_ins_turn_diff + predict_ins_turn_simple
              == q.get_ins_turn_with_prev_diff());
    my_assert(prev_a2a_ins_turn_diff == q.get_prev_a2a_ins_turn_diff());
}

#endif //UNTITLED_BEAMSTATEVALIDATOR_H

