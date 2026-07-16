#ifndef UNTITLED_BEAMSTATERUNNER_H
#define UNTITLED_BEAMSTATERUNNER_H

#include "all_include.h"
#include "State.h"
#include "StateHash.h"
#include "SortedStateA.h"
#include "BeamQuery.h"
#include "BeamHashHelper.h"
#include "BeamState.h"
#include "BeamStateValidator.h"
#include "StateCommandRecorder.h"
#include "A2AHelper.h"

struct StackWithPos {
    int N;
    int offset;
    my_vector<int> pos;

    StackWithPos(int N, const RingBuffer500 &stack) : N(N), offset(0), pos(N) {
        for (int i = 0; i < (int) stack.size(); ++i) {
            pos[stack[i]] = i;
        }
    }

    bool is_same_st(const RingBuffer500 &stack) const {
        if (stack.size() > 0) {
            return position(stack[0]) == 0;
        }
        return true;
    }

    void rotate(const RingBuffer500 &stack) {
        my_assert(stack.size() >= 2);
        my_assert(is_same_st(stack));
        int v = stack[0];
        offset -= 1;
        pos[v] = (stack.size() - 1) - offset;
    }

    void rev_rotate(const RingBuffer500 &stack) {
        my_assert(stack.size() >= 2);
        my_assert(is_same_st(stack));
        int v = stack[stack.size() - 1];
        offset += 1;
        pos[v] = -offset;
    }

    void swap(const RingBuffer500 &stack) {
        my_assert(stack.size() >= 2);
        my_assert(is_same_st(stack));
        int v0 = stack[0];
        int v1 = stack[1];
        pos[v0] = 1 - offset;
        pos[v1] = -offset;
    }

    void push_out() {
        offset -= 1;
    }

    void push_in(int in_v) {
//        my_assert(st.B.size() >= 1);
//        my_assert(is_same_st(st));
        offset += 1;
        pos[in_v] = -offset;
    }

    [[nodiscard]] int position(int v) const {
        return pos[v] + offset;
    }
};

class BeamStateRunner {
private:
    State base_state;
    State current_state;
    BeamPos current_pos;
    StackWithPos stack_a_pos;
    SortedStateA sorted_state_a;

public:
    const StackWithPos &get_stack_a_pos() const {
        return stack_a_pos;
    }

    template<int MAX_N>
    BeamStateRunner(const State &base, const BeamPos &pos, const BeamState<MAX_N> &beam_st)
            : base_state(base), current_state(base), current_pos(pos), stack_a_pos(base.current_N, base.A),
              sorted_state_a(create_sorted_state_a(base, beam_st)) {
        ::validate_st_beam_st_without_query(base, beam_st);
    }

    const State &get_state() const {
        return current_state;
    }

    State &get_state() {
        return current_state;
    }

    const BeamPos &get_pos() const {
        return current_pos;
    }

    const SortedStateA &get_sorted_state_a() const {
        return sorted_state_a;
    }

    SortedStateA &get_sorted_state_a() {
        return sorted_state_a;
    }

    template<int MAX_N>
    void validate_sorted_st(const my_vector<my_vector<BeamState<MAX_N>>> &beams_by_act_cnt) const {
#ifdef DEBUG
        my_assert(current_pos.act_cnt >= 0 && current_pos.act_cnt < (int) beams_by_act_cnt.size());
        my_assert(current_pos.idx < (int) beams_by_act_cnt[current_pos.act_cnt].size());
        const BeamState<MAX_N> &beam_st = beams_by_act_cnt[current_pos.act_cnt][current_pos.idx];
        SortedStateA expected = create_sorted_state_a(current_state, beam_st);
        my_assert(sorted_state_a == expected);
#endif
    }

    void validate_stack_a_pos() const {
#ifdef DEBUG
        for (int i = 0; i < current_state.A.size(); ++i) {
            my_assert(stack_a_pos.position(current_state.A[i]) == i);
        }
#endif
    }


    template<int MAX_N>
    void apply_query(const BeamQuery &query, const BeamPos &next_pos,
                     my_vector<my_vector<BeamState<MAX_N>>> &beams_by_act_cnt, const BeamQuery &prev_q) {
        my_assert(next_pos.act_cnt == current_pos.act_cnt + 1);
        my_assert(next_pos.act_cnt < (int) beams_by_act_cnt.size());
        BeamState<MAX_N> &cur_beam_st = beams_by_act_cnt[current_pos.act_cnt][current_pos.idx];
#ifdef DEBUG
        State st_bef = current_state;
        SortedStateA sorted_sa_bef = sorted_state_a;
#endif
//        auto &next_is_sorted_v = cur_beam_st.is_sorted_v;//後で直す
//        if (query.get_push_type() == e_push_type::b2a) {
//            next_is_sorted_v[query.get_tar_val()] = true;
//        } else if (query.get_push_type() == e_push_type::a2b) {
//            // a2bの場合はnext_is_sorted_vは変化しない
//        } else if (query.get_push_type() == e_push_type::a2a) {
//            next_is_sorted_v[query.get_tar_val()] = true;
//        } else {
//            my_assert(false);
//        }
        //next_beam_stはまだ作られていない
//        State &st = get_state();
//----------
        auto do_ra = [&]() {
            sorted_state_a.if_sorted_ra<MAX_N>(current_state, cur_beam_st.is_sorted_v);
            stack_a_pos.rotate(current_state.A);
            current_state.ra();
        };
        auto do_rra = [&](auto &is_sorted_v) {
            sorted_state_a.if_sorted_rra<MAX_N>(current_state, is_sorted_v);
            stack_a_pos.rev_rotate(current_state.A);
            current_state.rra();
        };
        auto do_pa = [&]() {
            sorted_state_a.pa(query.get_tar_val());
            stack_a_pos.push_in(current_state.B[0]);
            current_state.pa();
        };
        auto do_pb = [&]() {
            stack_a_pos.push_out();
            current_state.pb();
        };
        int prev_a2a_diff_ra = 0, prev_a2a_diff_rra = 0;
        if (prev_q.is_valid_query() && prev_q.get_push_type() == e_push_type::a2a) {
            tie(prev_a2a_diff_ra, prev_a2a_diff_rra) = change_type_diff(current_state, prev_q,
                                                                        query.get_determined_prev_a2a_type());
            my_assert(prev_a2a_diff_ra >= 0 && prev_a2a_diff_rra >= 0);
            if (prev_a2a_diff_ra > 0) {
                my_assert(prev_a2a_diff_ra == 1);
                do_ra();
            }
            if (prev_a2a_diff_rra > 0) {
                my_assert(prev_a2a_diff_rra == 1);
                do_rra(cur_beam_st.is_sorted_v);
            }
        }
        //-----------------
        if (query.is_a2b_b2a()) {
            if (query.get_a_cmd() == RA) {
                for (int i = 0; i < query.get_a_count(); i++) {
                    do_ra();
                }
            } else if (query.get_a_cmd() == RRA) {
                for (int i = 0; i < query.get_a_count(); i++) {
                    do_rra(cur_beam_st.is_sorted_v);
                }
            }

            if (query.get_b_cmd() == RB) {
                for (int i = 0; i < query.get_b_count(); i++) {
                    current_state.rb();
                }
            } else if (query.get_b_cmd() == RRB) {
                for (int i = 0; i < query.get_b_count(); i++) {
                    current_state.rrb();
                }
            }
            if (query.get_push_type() == e_push_type::a2b) {
                do_pb();
            } else if (query.get_push_type() == e_push_type::b2a) {
                //現状b2aは正しい値しか入れない
                do_pa();
            } else {
                my_assert(0);
            }
        } else if (query.is_a2a()) {
            //a2aは前のa2を変更しない
            my_assert(!prev_q.is_valid_query() || !prev_q.is_a2a()
                      || query.get_determined_prev_a2a_type() == prev_q.get_cur_default_a2a_type());
            auto [cmd1, q_cnt1] = query.get_cmd1_cnt1_(query.get_cur_default_a2a_type(), query.get_a_size());
            if (cmd1 == RA) {
                for (int _ = 0; _ < q_cnt1; _++) {
                    do_ra();
                }
            } else if (cmd1 == RRA) {
                for (int _ = 0; _ < q_cnt1; _++) {
                    do_rra(cur_beam_st.is_sorted_v);
                }
            } else {
                my_assert(false);
            }
            int flip_dis_dir = query.get_flip_dis_dir();
            my_assert(flip_dis_dir != 0);
            int abs_flip = abs(flip_dis_dir);
            if (query.get_cur_default_a2a_type() == e_a2a_type::push_rot) {
                if (flip_dis_dir > 0) {
                    do_pb();
                    for (int _ = 0; _ < abs_flip; _++) {
                        do_ra();
                    }
                    do_pa();
                } else {
                    do_pb();
                    for (int _ = 0; _ < abs_flip; _++) {
                        do_rra(cur_beam_st.is_sorted_v);
                    }
                    do_pa();
                }
            } else if (query.get_cur_default_a2a_type() == e_a2a_type::swap_rot) {
                if (flip_dis_dir > 0) {
                    //topにvがある
                    my_assert(current_state.A[0] == query.get_tar_val());
                    do_pb();
                    for (int _ = 0; _ < abs_flip; _++) {
                        do_ra();
                    }
                    do_pa();
                    my_assert(!cur_beam_st.is_sorted_v[query.get_tar_val()]);
                    cur_beam_st.is_sorted_v[query.get_tar_val()] = true;
                    do_rra(cur_beam_st.is_sorted_v);
                    cur_beam_st.is_sorted_v[query.get_tar_val()] = false;
                } else {
                    //secondにvがある
                    do_ra();
                    my_assert(current_state.A[0] == query.get_tar_val());
                    do_pb();
                    for (int _ = 0; _ < abs_flip; _++) {
                        do_rra(cur_beam_st.is_sorted_v);
                    }
                    do_pa();
                }
            } else {
                my_assert(false);
            }
        } else {
            my_assert(false);
        }
        current_pos = next_pos;
        validate_stack_a_pos();
    }

    template<int MAX_N>
    int
    get_parent_act_cnt(int act_cnt, const BeamState<MAX_N> &state,
                       const my_vector<my_vector<BeamState<MAX_N>>> &beams_by_act_cnt) const {
        my_assert(act_cnt > 0);
        int parent_act_cnt = act_cnt - 1;
        my_assert(state.act_cnt == act_cnt);
        if (state.parent_idx >= 0) {
            my_assert(parent_act_cnt >= 0 && parent_act_cnt < (int) beams_by_act_cnt.size());
            my_assert(state.parent_idx < (int) beams_by_act_cnt[parent_act_cnt].size());
            const BeamState<MAX_N> &parent_st = beams_by_act_cnt[parent_act_cnt][state.parent_idx];
            my_assert(parent_st.act_cnt == parent_act_cnt);
        }
        return parent_act_cnt;
    }

    template<int MAX_N>
    void rollback_query(const my_vector<my_vector<BeamState<MAX_N>>> &beams_by_act_cnt, const BeamQuery &prev_q) {
        my_assert(current_pos.act_cnt > 0);
        my_assert(current_pos.idx < (int) beams_by_act_cnt[current_pos.act_cnt].size());
        const BeamState<MAX_N> &cur_beam_st = beams_by_act_cnt[current_pos.act_cnt][current_pos.idx];
        my_assert(cur_beam_st.parent_idx >= 0);
        int parent_act_cnt = get_parent_act_cnt(current_pos.act_cnt, cur_beam_st, beams_by_act_cnt);
        BeamPos prev_pos = BeamPos(parent_act_cnt, cur_beam_st.parent_idx);
        const BeamQuery &query = cur_beam_st.last_query;

        my_assert(prev_pos.act_cnt >= 0 && prev_pos.act_cnt < (int) beams_by_act_cnt.size());
        my_assert(prev_pos.idx < (int) beams_by_act_cnt[prev_pos.act_cnt].size());
        const BeamState<MAX_N> &prev_beam_st = beams_by_act_cnt[prev_pos.act_cnt][prev_pos.idx];

        //
        auto revert_ra = [&](const my_bitset<MAX_N> &tar_is_sorted_v) {
            sorted_state_a.if_sorted_rra<MAX_N>(current_state, tar_is_sorted_v);
            stack_a_pos.rev_rotate(current_state.A);
            current_state.rra();
        };

        auto revert_rra = [&](const my_bitset<MAX_N> &tar_is_sorted_v) {
            sorted_state_a.if_sorted_ra<MAX_N>(current_state, tar_is_sorted_v);
            stack_a_pos.rotate(current_state.A);
            current_state.ra();
        };
        auto revert_pa = [&]() {
            sorted_state_a.pb();
            stack_a_pos.push_out();
            current_state.pb();
        };
        auto revert_pb = [&]() {
            stack_a_pos.push_in(current_state.B[0]);
            current_state.pa();
        };

        if (query.is_a2b_b2a()) {
            if (query.get_push_type() == e_push_type::a2b) {
                //ゴミがpaされるのでsorted_state_aは更新しない
                my_assert(!is_sorted_of_ring(sorted_state_a.A.back(), current_state.B[0], sorted_state_a.A[0]));
                stack_a_pos.push_in(current_state.B[0]);
                current_state.pa();
            } else if (query.get_push_type() == e_push_type::b2a) {
                my_assert(cur_beam_st.is_sorted_v[query.get_tar_val()]);
                my_assert(sorted_state_a.A.size() > 0 && sorted_state_a.A[0] == query.get_tar_val());
                sorted_state_a.pb();
                stack_a_pos.push_out();
                current_state.pb();
            } else if (query.get_push_type() == e_push_type::a2a) {
                my_assert(0);
            } else {
                my_assert(0);
            }

            if (query.get_b_cmd() == RB) {
                for (int i = 0; i < query.get_b_count(); i++) {
                    current_state.rrb();
                }
            } else if (query.get_b_cmd() == RRB) {
                for (int i = 0; i < query.get_b_count(); i++) {
                    current_state.rb();
                }
            }

            if (query.get_a_cmd() == RA) {
                for (int i = 0; i < query.get_a_count(); i++) {
                    //push処理を巻き戻した後なので、prev_beam_st.is_sorted_v
                    sorted_state_a.if_sorted_rra<MAX_N>(current_state, prev_beam_st.is_sorted_v);
                    stack_a_pos.rev_rotate(current_state.A);
                    current_state.rra();
                }
            } else if (query.get_a_cmd() == RRA) {
                for (int i = 0; i < query.get_a_count(); i++) {
                    sorted_state_a.if_sorted_ra<MAX_N>(current_state, prev_beam_st.is_sorted_v);
                    stack_a_pos.rotate(current_state.A);
                    current_state.ra();
                }
            }
        } else if (query.is_a2a()) {
            int flip_dis_dir = query.get_flip_dis_dir();
            my_assert(flip_dis_dir != 0);
            int abs_flip = abs(flip_dis_dir);
            if (query.get_cur_default_a2a_type() == e_a2a_type::push_rot) {
                if (flip_dis_dir > 0) {
                    revert_pa();
                    for (int _ = 0; _ < abs_flip; _++) {
                        revert_ra(prev_beam_st.is_sorted_v);
                    }
                    revert_pb();
                } else {
                    revert_pa();
                    for (int _ = 0; _ < abs_flip; _++) {
                        revert_rra(prev_beam_st.is_sorted_v);
                    }
                    revert_pb();
                }
            } else if (query.get_cur_default_a2a_type() == e_a2a_type::swap_rot) {
                if (flip_dis_dir > 0) {
                    revert_rra(cur_beam_st.is_sorted_v);
                    revert_pa();
                    for (int _ = 0; _ < abs_flip; _++) {
                        revert_ra(prev_beam_st.is_sorted_v);
                    }
                    revert_pb();
                } else {
                    revert_pa();
                    for (int _ = 0; _ < abs_flip; _++) {
                        revert_rra(prev_beam_st.is_sorted_v);
                    }
                    revert_pb();
                    revert_ra(prev_beam_st.is_sorted_v);
                }
            } else {
                my_assert(false);
            }
            auto [cmd1, q_cnt1] = query.get_cmd1_cnt1_(query.get_cur_default_a2a_type(), query.get_a_size());
            if (cmd1 == RA) {
                for (int _ = 0; _ < q_cnt1; _++) {
                    revert_ra(prev_beam_st.is_sorted_v);
                }
            } else if (cmd1 == RRA) {
                for (int _ = 0; _ < q_cnt1; _++) {
                    revert_rra(prev_beam_st.is_sorted_v);
                }
            } else {
                my_assert(false);
            }
        } else {
            my_assert(false);
        }
        int prev_a2a_diff_ra = 0, prev_a2a_diff_rra = 0;
        if (prev_q.is_valid_query() && prev_q.get_push_type() == e_push_type::a2a) {
            tie(prev_a2a_diff_ra, prev_a2a_diff_rra) = change_type_diff(current_state, prev_q,
                                                                        query.get_determined_prev_a2a_type());
            my_assert(prev_a2a_diff_ra >= 0 && prev_a2a_diff_rra >= 0);
            if (prev_a2a_diff_ra > 0) {
                my_assert(prev_a2a_diff_ra == 1);
                revert_ra(prev_beam_st.is_sorted_v);
            }
            if (prev_a2a_diff_rra > 0) {
                my_assert(prev_a2a_diff_rra == 1);
                revert_rra(prev_beam_st.is_sorted_v);
            }
        }
        current_pos = prev_pos;
        validate_sorted_st(beams_by_act_cnt);
        validate_stack_a_pos();
    }
};

#endif //UNTITLED_BEAMSTATERUNNER_H

