#ifndef QUERY_TO_COMMAND_BUILDER_H
#define QUERY_TO_COMMAND_BUILDER_H
#include "BeamEnums.h"
#include "Enums.h"
#include "State.h"

#include "State.h"
#include <vector>
#include <algorithm>

struct CommandBuildState {
    State st;
    my_vector<command> cmds;
    int predict_cmd_sum;
    int prev_seq_a2a_cur;
    int prev_seq_a2a_end;
    bool on_pb;

    CommandBuildState(const State& init_st)
        : st(init_st), cmds(), predict_cmd_sum(0), prev_seq_a2a_cur(0), prev_seq_a2a_end(0), on_pb(false) {}
};

class QueryToCommandBuilder {
public:
    QueryToCommandBuilder(CommandBuildState& state)
        : state_(state) {}


    void add_a2b_b2a(e_push_type push_type, command a_cmd, command b_cmd, int orig_a_cnt, int orig_b_cnt) {
        my_assert(orig_a_cnt >= 0 && orig_b_cnt >= 0);
        auto& cmds = state_.cmds;
        auto& prev_seq_a2a_cur = state_.prev_seq_a2a_cur;
        auto& prev_seq_a2a_end = state_.prev_seq_a2a_end;
        auto& on_pb = state_.on_pb;
        auto add_b_rot = [&](command b_rot_cmd) {
            my_assert(b_rot_cmd == RB || b_rot_cmd == RRB);
            command partner_a_cmd = sync_partner[b_rot_cmd];
            auto watch_next = [&]() {
                my_assert(prev_seq_a2a_cur < prev_seq_a2a_end);
                if (cmds[prev_seq_a2a_cur] == PB) {
                    my_assert(!on_pb);
                    on_pb = true;
                }
                if (cmds[prev_seq_a2a_cur] == PA) {
                    my_assert(on_pb);
                    on_pb = false;
                }
                prev_seq_a2a_cur++;
            };
            while (prev_seq_a2a_cur < prev_seq_a2a_end && (on_pb || cmds[prev_seq_a2a_cur] != partner_a_cmd)) {
                watch_next();
            }
            if (prev_seq_a2a_cur < prev_seq_a2a_end && cmds[prev_seq_a2a_cur] == partner_a_cmd) {
                cmds[prev_seq_a2a_cur] = sync_cmd(partner_a_cmd, b_rot_cmd);
                watch_next();
            } else {
                cmds.push_back(b_rot_cmd);
            }
        };
        if ((a_cmd == RA && b_cmd == RB) || (a_cmd == RRA && b_cmd == RRB)) {
            command both_cmd = (a_cmd == RA) ? RR : RRR;
            int common = std::min(orig_a_cnt, orig_b_cnt);
            int rem_a = orig_a_cnt - common;
            int rem_b = orig_b_cnt - common;
            for (int i = 0; i < rem_a; i++) cmds.push_back(a_cmd);
            for (int i = 0; i < rem_b; i++) add_b_rot(b_cmd);
            for (int i = 0; i < common; i++) cmds.push_back(both_cmd);
        } else {
            if (a_cmd == RA) for (int i = 0; i < orig_a_cnt; i++) cmds.push_back(RA);
            else if (a_cmd == RRA) for (int i = 0; i < orig_a_cnt; i++) cmds.push_back(RRA);
            if (b_cmd == RB) for (int i = 0; i < orig_b_cnt; i++) add_b_rot(RB);
            else if (b_cmd == RRB) for (int i = 0; i < orig_b_cnt; i++) add_b_rot(RRB);
        }
        if (push_type == e_push_type::a2b) {
            cmds.push_back(PB);
        } else if (push_type == e_push_type::b2a) {
            cmds.push_back(PA);
        } else {
            my_assert(false);
        }
        prev_seq_a2a_cur = cmds.size();
        prev_seq_a2a_end = cmds.size();
    }

    void add_a2a(e_a2a_type a2a_type, command cmd1, int cnt1, int flip_dis_dir) {
        auto& cmds = state_.cmds;
        if (flip_dis_dir == 0) {
            my_assert(cmd1 == NO_COMMAND);
            my_assert(cnt1 == 0);
            state_.prev_seq_a2a_end = cmds.size();
            return;
        }
        int abs_flip_dis_dir = std::abs(flip_dis_dir);
        for (int _ = 0; _ < cnt1; _++) {
            cmds.push_back(cmd1);
        }
        if (a2a_type == e_a2a_type::push_rot) {
            cmds.push_back(PB);
            if (flip_dis_dir > 0) {
                for (int _ = 0; _ < abs_flip_dis_dir; _++) {
                    cmds.push_back(RA);
                }
            } else {
                for (int _ = 0; _ < abs_flip_dis_dir; _++) {
                    cmds.push_back(RRA);
                }
            }
            cmds.push_back(PA);
        } else {
            my_assert(a2a_type == e_a2a_type::swap_rot);
            my_assert(flip_dis_dir != 0);
            cmds.push_back(SA);
            if (flip_dis_dir > 0) {
                for (int _ = 0; _ < abs_flip_dis_dir - 1; _++) {
                    cmds.push_back(RA);
                    cmds.push_back(SA);
                }
            } else {
                for (int _ = 0; _ < abs_flip_dis_dir - 1; _++) {
                    cmds.push_back(RRA);
                    cmds.push_back(SA);
                }
            }
        }
        state_.prev_seq_a2a_end = cmds.size();
    }

private:
    CommandBuildState& state_;
};

#endif // QUERY_TO_COMMAND_BUILDER_H
