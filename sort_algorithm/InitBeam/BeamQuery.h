//
// Created by Auto
//

#ifndef UNTITLED_BEAMQUERY_H
#define UNTITLED_BEAMQUERY_H

#include "all_include.h"
#include "Command/Command.h"
#include "Enums.h"
#include "a2a_calculator.h"
#include "BeamEnums.h"
#include "util.h"
#include "State.h"

struct ConnectInfo {
    bool make_cur_sa;
    bool make_pre_sb;
    int prev_diff_turn;
    int new_a_count;
    int new_b_count;
    int new_cur_turn;

    ConnectInfo() : make_cur_sa(false), make_pre_sb(false), prev_diff_turn(0), new_a_count(0), new_b_count(0),
                    new_cur_turn(0) {}

    ConnectInfo(bool make_cur_sa, bool make_pre_sb, int prev_diff_turn, int new_a_count, int new_b_count,
                int new_cur_turn)
            : make_cur_sa(make_cur_sa), make_pre_sb(make_pre_sb), prev_diff_turn(prev_diff_turn),
              new_a_count(new_a_count), new_b_count(new_b_count), new_cur_turn(new_cur_turn) {}

    bool operator==(const ConnectInfo &other) const {
        return make_cur_sa == other.make_cur_sa &&
               make_pre_sb == other.make_pre_sb &&
               prev_diff_turn == other.prev_diff_turn &&
               new_a_count == other.new_a_count &&
               new_b_count == other.new_b_count &&
               new_cur_turn == other.new_cur_turn;
    }
};

inline ostream &operator<<(ostream &os, const ConnectInfo &ci) {
    os << "{make_cur_sa:" << ci.make_cur_sa
       << ", make_pre_sb:" << ci.make_pre_sb
       << ", prev_diff_turn:" << ci.prev_diff_turn
       << ", new_a_count:" << ci.new_a_count
       << ", new_b_count:" << ci.new_b_count
       << ", new_cur_turn:" << ci.new_cur_turn << "}";
    return os;
}

struct PrevA2AInfo {
    int if_push_prev_diff;
    int push_prev_ra_sum;
    int push_prev_rra_sum;
    int if_swap_prev_diff;
    int swap_prev_ra_sum;
    int swap_prev_rra_sum;
    bool prev_is_a2a;
    int if_push_last_a_pos_diff;
    int if_swap_last_a_pos_diff;

    PrevA2AInfo() : if_push_prev_diff(0), push_prev_ra_sum(0), push_prev_rra_sum(0),
                    if_swap_prev_diff(0), swap_prev_ra_sum(0), swap_prev_rra_sum(0),
                    prev_is_a2a(false), if_push_last_a_pos_diff(0), if_swap_last_a_pos_diff(0) {}

    PrevA2AInfo(int if_push_prev_diff, int push_prev_ra_sum, int push_prev_rra_sum,
                int if_swap_prev_diff, int swap_prev_ra_sum, int swap_prev_rra_sum, bool prev_is_a2a,
                int if_push_last_a_pos_diff, int if_swap_last_a_pos_diff)
            : if_push_prev_diff(if_push_prev_diff), push_prev_ra_sum(push_prev_ra_sum),
              push_prev_rra_sum(push_prev_rra_sum), if_swap_prev_diff(if_swap_prev_diff),
              swap_prev_ra_sum(swap_prev_ra_sum), swap_prev_rra_sum(swap_prev_rra_sum),
              prev_is_a2a(prev_is_a2a), if_push_last_a_pos_diff(if_push_last_a_pos_diff),
              if_swap_last_a_pos_diff(if_swap_last_a_pos_diff) {}

    bool is_prev_a2a() const {
        return prev_is_a2a;
    }
};

//a2aの時は
//最後終わる時に、vがtopの時と、vが2番目の時がある

//yakinamashiの場合
//二つを切り替えられるようにする？ いや　毎回少ない方で良い


//面倒だから、直前一つのa2aしか見ない
struct BeamQuery {
    //a2b, b2aの場合
    //cmd1はa_cmd, cmd2はb_cmdを表す
    //dis1はa_cmdの回数, dis2はb_cmdの回数を表す
    //
    //a2aの場合
    //cmd1は最初のposに行くためのa_cmd,
    //target_vをtopに置くためのcmdの回数（swapの場合はここから一路ズレたところをtopにしたりする）
    //cmd2は持っていくためのcmd
    //amount2は、間にある邪魔な物の個数
private:
    int tar_val;
    int ins_turn_with_prev_diff;
    int amount1;
    int amount2;
    command cmd1;
    command cmd2;
    e_push_type push_type;
    e_a2a_type determined_prev_a2a_type;
    int prev_a2a_ins_turn_diff;
    e_a2a_type cur_default_a2a_type;
    int a_size;
    my_vector<int> a2b_skip_vals;


public:
    BeamQuery() : tar_val(-1), ins_turn_with_prev_diff(-1), amount1(-1), amount2(-1), cmd1(NO_COMMAND),
                  cmd2(NO_COMMAND),
                  push_type(e_push_type::e_push_type_default), determined_prev_a2a_type(e_a2a_type::e_a2a_type_default),
                  prev_a2a_ins_turn_diff(-1),
                  cur_default_a2a_type(e_a2a_type::e_a2a_type_default), a_size(-1), a2b_skip_vals() {}

    BeamQuery(int b_to_a_value, int ins_turn_with_prev_diff, int a_count, int dis2, command a_cmd, command b_cmd,
              e_push_type push_type, e_a2a_type prev_a2a_type, int prev_a2a_ins_turn_diff,
              e_a2a_type cur_default_a2a_type, int a_size)
            : tar_val(b_to_a_value), ins_turn_with_prev_diff(ins_turn_with_prev_diff), amount1(a_count), amount2(dis2),
              cmd1(a_cmd),
              cmd2(b_cmd), push_type(push_type), determined_prev_a2a_type(prev_a2a_type),
              prev_a2a_ins_turn_diff(prev_a2a_ins_turn_diff),
              cur_default_a2a_type(cur_default_a2a_type), a_size(a_size), a2b_skip_vals() {
#ifdef DEBUG
        if (push_type == e_push_type::a2a) {
            my_assert(cur_default_a2a_type == e_a2a_type::swap_rot ||
                      cur_default_a2a_type == e_a2a_type::push_rot
            );
        } else if (push_type == e_push_type::b2a || push_type == e_push_type::a2b) {
            my_assert(cur_default_a2a_type == e_a2a_type::not_a2a);
        } else {
            my_assert(false);
        }
#endif
    }

    //一番最初のprev_q等の時は、defaultのままのクエリを扱ったりする
    bool is_valid_query() const {
        return tar_val >= 0;
    }

    int get_tar_val() const {
        my_assert(is_valid_query());
        return tar_val;
    }

    void set_tar_val(int val) { tar_val = val; }

    int get_ins_turn_simple() const {
        my_assert(is_valid_query());
#ifdef DEBUG 
if(is_a2a()){
    my_assert(prev_a2a_ins_turn_diff == 0);
}
#endif
        return get_ins_turn_with_prev_diff() - prev_a2a_ins_turn_diff;
    }

    int get_ins_turn_with_prev_diff() const {
        my_assert(is_valid_query());
        return ins_turn_with_prev_diff;
    }

    void set_ins_turn_with_prev_diff(int val) {
        ins_turn_with_prev_diff = val;
    }
//    int get_ins_turn_simple() const {
//        my_assert(is_valid_query());
//        return ins_turn_with_prev_diff; }
//
//    void set_ins_turn_simple(int val) {
//        ins_turn_with_prev_diff = val;
//    }

    int get_prev_a2a_ins_turn_diff() const {
        my_assert(is_valid_query());
        return prev_a2a_ins_turn_diff;
    }

    void set_prev_a2a_ins_turn_diff(int val) { prev_a2a_ins_turn_diff = val; }

    bool is_a2b_b2a() const {
        my_assert(is_valid_query());
        return push_type == e_push_type::a2b || push_type == e_push_type::b2a;
    }

    bool is_a2a() const {
        my_assert(is_valid_query());
        return push_type == e_push_type::a2a;
    }

    int get_a_count() const {
        my_assert(is_valid_query());
        my_assert(is_a2b_b2a());
        return amount1;
    }

    void set_a_count(int val) {
        my_assert(is_a2b_b2a());
        amount1 = val;
    }

    int get_b_count() const {
        my_assert(is_valid_query());
        my_assert(is_a2b_b2a());
        return amount2;
    }

    void set_b_count(int val) {
        my_assert(is_a2b_b2a());
        amount2 = val;
    }

    command get_a_cmd() const {
        my_assert(is_valid_query());
        my_assert(is_a2b_b2a());
        return cmd1;
    }

    void set_a_cmd(command val) {
        my_assert(is_a2b_b2a());
        cmd1 = val;
    }

    command get_b_cmd() const {
        my_assert(is_valid_query());
        my_assert(is_a2b_b2a());
        return cmd2;
    }

    void set_b_cmd(command val) {
        my_assert(is_a2b_b2a());
        cmd2 = val;
    }

    //a2a_typeによって、RA0の時と、RRA1の時があるため、決定にa2a_typeが必要
    command get_cmd1(e_a2a_type a2a_type, int a_size) const {
        my_assert(is_valid_query());
        my_assert(is_a2a());
        auto [res_cmd1, cnt1] = get_cmd1_cnt1(a2a_type, get_target_a_pos(), a_size, get_cmd2(a2a_type),
                                              get_flip_dis_dir());
        return res_cmd1;
    }


    //a
    inline pair<command, int> get_cmd1_cnt1_(e_a2a_type a2a_type, int a_size) const {
        my_assert(is_valid_query());
        my_assert(is_a2a());
        return get_cmd1_cnt1(a2a_type, get_target_a_pos(), a_size, get_cmd2(a2a_type), get_flip_dis_dir());
    }

    void set_cmd1(command val) {
        my_assert(is_a2a());
        cmd1 = val;
    }

    int get_cnt1(e_a2a_type a2a_type, int a_size) {
        my_assert(is_valid_query());
        my_assert(is_a2a());
        auto [cmd1, cnt1] = get_cmd1_cnt1(a2a_type, get_target_a_pos(), a_size, get_cmd2(a2a_type), get_flip_dis_dir());
        return cnt1;
    }

    //a
    int get_cmd2_cnt(e_a2a_type a2a_type) const {
        my_assert(is_valid_query());
        my_assert(is_a2a());
        if (a2a_type == e_a2a_type::push_rot) {
            return abs(get_flip_dis_dir());
        } else if (a2a_type == e_a2a_type::swap_rot) {
            return abs(get_flip_dis_dir()) - 1;
        } else {
            my_assert(false);
            return -1;
        }
    }

    int get_target_a_pos() const {
        my_assert(is_valid_query());
        my_assert(is_a2a());
        return amount1;
    }

    void set_target_a_pos(int val) {
        my_assert(is_a2a());
        amount1 = val;
    }

    command get_cmd2(e_a2a_type a2a_type) const {
        my_assert(is_valid_query());
        my_assert(is_a2a());
        int flip_dis_dir = get_flip_dis_dir();
        if (flip_dis_dir > 0) {
            if (a2a_type == e_a2a_type::push_rot) {
                return RA;
            } else if (a2a_type == e_a2a_type::swap_rot) {
                //cmd2_cnt ==0のときもRAで良いので
                return RA;
            } else {
                my_assert(false);
                return NO_COMMAND;
            }
        } else if (flip_dis_dir < 0) {
            if (a2a_type == e_a2a_type::push_rot) {
                return RRA;
            } else if (a2a_type == e_a2a_type::swap_rot) {
                if (flip_dis_dir == -1) {
                    return RA;//0なので
                }
                return RRA;
            } else {
                my_assert(false);
                return NO_COMMAND;
            }
        } else {
            my_assert(false);
            return NO_COMMAND;
        }
    }


    void set_cmd2(command val) {
        my_assert(is_a2a());
        cmd2 = val;
    }

    //abs(ret)が、何個の要素とswap必要する必要があるかを表す
    //+ならRA, -ならRRA
    int get_flip_dis_dir() const {
        my_assert(is_valid_query());
        my_assert(is_a2a());
        my_assert(amount2 != 0);//0だと、既にソートされていることになる
        return amount2;
    }

    void set_flip_dis_dir(int val) {
        my_assert(is_a2a());
        my_assert(val != 0);
        amount2 = val;
    }

    e_push_type get_push_type() const {
        my_assert(is_valid_query());
        return push_type;
    }

    void set_push_type(e_push_type val) { push_type = val; }

    e_a2a_type get_determined_prev_a2a_type() const {
        my_assert(is_valid_query());
        return determined_prev_a2a_type;
    }

    void set_determined_prev_a2a_type(e_a2a_type val) { determined_prev_a2a_type = val; }

    e_a2a_type get_cur_default_a2a_type() const {
        my_assert(is_valid_query());
        return cur_default_a2a_type;
    }

    void set_cur_default_a2a_type(e_a2a_type val) { cur_default_a2a_type = val; }

    int get_a_size() const {
        my_assert(is_valid_query());
        return a_size;
    }

    void set_a_size(int val) { a_size = val; }

    const my_vector<int> &get_a2b_skip_vals() const {
        my_assert(is_valid_query());
        my_assert(get_push_type() == e_push_type::a2b);
        return a2b_skip_vals;
    }

    void set_a2b_skip_vals(const my_vector<int> &vals) {
        my_assert(get_push_type() == e_push_type::a2b);
        a2b_skip_vals = vals;
    }

    bool operator==(const BeamQuery &other) const {
        return tar_val == other.tar_val &&
               ins_turn_with_prev_diff == other.ins_turn_with_prev_diff &&
               amount1 == other.amount1 &&
               amount2 == other.amount2 &&
               cmd1 == other.cmd1 &&
               cmd2 == other.cmd2 &&
               push_type == other.push_type &&
               determined_prev_a2a_type == other.determined_prev_a2a_type &&
               prev_a2a_ins_turn_diff == other.prev_a2a_ins_turn_diff &&
               cur_default_a2a_type == other.cur_default_a2a_type &&
               a_size == other.a_size &&
               a2b_skip_vals == other.a2b_skip_vals;
    }
};

inline string get_push_type_str(e_push_type push_type) {
    if (push_type == e_push_type::a2b) {
        return "a2b";
    } else if (push_type == e_push_type::b2a) {
        return "b2a";
    } else if (push_type == e_push_type::a2a) {
        return "a2a";
    } else {
        my_assert(false);
        return "";
    }
}

inline string get_a2a_type_str(e_a2a_type a2a_type) {
    if (a2a_type == e_a2a_type::not_a2a) {
        return "not_a2a";
    } else if (a2a_type == e_a2a_type::swap_rot) {
        return "swap_rot";
    } else if (a2a_type == e_a2a_type::push_rot) {
        return "push_rot";
    } else if (a2a_type == e_a2a_type::e_a2a_type_default) {
        return "e_a2a_type_default";
    } else {
        my_assert(false);
        return "";
    }
}

inline ostream &operator<<(ostream &os, const BeamQuery &q) {
    if (q.is_a2b_b2a()) {
        os << "{tar_val:" << q.get_tar_val()
           << ", ins_turn_with_prev_diff:" << q.get_ins_turn_with_prev_diff()
           << ", cmd1:" << q.get_a_cmd()
           << ", amount1:" << q.get_a_count()
           << ", cmd2:" << q.get_b_cmd()
           << ", b_count:" << q.get_b_count()
           << ", determined_prev_a2a_type:" << get_a2a_type_str(q.get_determined_prev_a2a_type())
           << ", prev_a2a_ins_turn_diff:" << q.get_prev_a2a_ins_turn_diff()
           << ", cur_default_a2a_type:" << get_a2a_type_str(q.get_cur_default_a2a_type())
           << ", push_type:" << get_push_type_str(q.get_push_type()) << "}";
    } else if (q.is_a2a()) {
        e_a2a_type a2a_type = q.get_cur_default_a2a_type();
        os << "{tar_val:" << q.get_tar_val()
           << ", ins_turn_with_prev_diff:" << q.get_ins_turn_with_prev_diff()
           << ", target_a_pos:" << q.get_target_a_pos()
           << ", flip_dis_dir:" << q.get_flip_dis_dir()
           << ", cmd1:" << q.get_cmd1(a2a_type, q.get_a_size())
           << ", cmd2:" << q.get_cmd2(a2a_type)
           << ", cur_default_a2a_type:" << get_a2a_type_str(q.get_cur_default_a2a_type())
           << ", determined_prev_a2a_type:" << get_a2a_type_str(q.get_determined_prev_a2a_type())
           << ", prev_a2a_ins_turn_diff:" << q.get_prev_a2a_ins_turn_diff()
           << ", a_size:" << q.get_a_size()
           << ", push_type:" << get_push_type_str(q.get_push_type()) << "}";
    } else {
        my_assert(false);
    }
    return os;
}

inline int calc_a2a_ins_turn_with_type(const BeamQuery &q, e_a2a_type a2a_type) {
    my_assert(q.is_a2a());
    auto [cmd1, cnt1] = q.get_cmd1_cnt1_(a2a_type, q.get_a_size());
    int inter_cnt = abs(q.get_flip_dis_dir());
    return cnt1 + calc_a2a_ins_turn(inter_cnt, a2a_type);
}

inline void apply_query_to_state(State& state, const BeamQuery& query,
                                     const my_vector<BeamQuery>& queries, int query_idx) {
    if (query.is_a2b_b2a()) {
        if (query.get_a_cmd() == RA) {
            for (int i = 0; i < query.get_a_count(); i++) {
                state.ra();
            }
        } else if (query.get_a_cmd() == RRA) {
            for (int i = 0; i < query.get_a_count(); i++) {
                state.rra();
            }
        }

        if (query.get_b_cmd() == RB) {
            for (int i = 0; i < query.get_b_count(); i++) {
                state.rb();
            }
        } else if (query.get_b_cmd() == RRB) {
            for (int i = 0; i < query.get_b_count(); i++) {
                state.rrb();
            }
        }

        if (query.get_push_type() == e_push_type::a2b) {
            state.pb();
        } else if (query.get_push_type() == e_push_type::b2a) {
            state.pa();
        }
    } else if (query.is_a2a()) {
        e_a2a_type a2a_type = query.get_cur_default_a2a_type();
        if (query_idx + 1 < (int) queries.size()) {
            const BeamQuery &next_query = queries[query_idx + 1];
            my_assert(next_query.is_valid_query());
            a2a_type = next_query.get_determined_prev_a2a_type();
        }

        auto [cmd1, cnt1] = query.get_cmd1_cnt1_(a2a_type, query.get_a_size());
        if (cmd1 == RA) {
            for (int i = 0; i < cnt1; i++) {
                state.ra();
            }
        } else if (cmd1 == RRA) {
            for (int i = 0; i < cnt1; i++) {
                state.rra();
            }
        }

        int flip_dis_dir = query.get_flip_dis_dir();
        int abs_flip = abs(flip_dis_dir);

        if (a2a_type == e_a2a_type::push_rot) {
            state.pb();
            if (flip_dis_dir > 0) {
                for (int i = 0; i < abs_flip; i++) {
                    state.ra();
                }
            } else {
                for (int i = 0; i < abs_flip; i++) {
                    state.rra();
                }
            }
            state.pa();
        } else if (a2a_type == e_a2a_type::swap_rot) {
            if (flip_dis_dir > 0) {
                state.pb();
                for (int i = 0; i < abs_flip; i++) {
                    state.ra();
                }
                state.pa();
                state.rra();
            } else {
                state.ra();
                state.pb();
                for (int i = 0; i < abs_flip; i++) {
                    state.rra();
                }
                state.pa();
            }
        }
    }
}

#endif //UNTITLED_BEAMQUERY_H

