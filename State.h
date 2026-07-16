//
//

#ifndef UNTITLED_STATE_H
#define UNTITLED_STATE_H

#include "all_include.h"
#include "util.h"
#include "Command/Command.h"
#include "Command/SyncCommand.h"
#include "array_manager.h"
#include "RingBuffer500.h"
#include "Command/InitCommand.h"

class State;

State get_start_state(const State &st);

State get_state_turn_i(const State &st, size_t i);

bool same_stack(const State &lhs, const State &rhs);

class State {
    bool can_swap(RingBuffer500 &dq) {
        return (dq.size() >= 2);
    }

    bool can_push(RingBuffer500 &from, RingBuffer500 &to) {
        return (from.size() >= 1);
    }

    void push(RingBuffer500 &from, RingBuffer500 &to) {
        to.push_front(from.front());
        from.pop_front();
    }

    bool can_rotate(RingBuffer500 &q) {
        return (q.size() >= 2);
    }

    bool can_rrotate(RingBuffer500 &q) {
        return (q.size() >= 2);
    }

    void rotate(RingBuffer500 &q) {
        q.push_back(q[0]);
        q.pop_front();
    }

    void rrotate(RingBuffer500 &q) {
        q.push_front(q.back());
        q.pop_back();
    }

    cmd_list_t cmd_list;
    bool track_cmd_list;

public:
//    RingBuffer500 A;
//    RingBuffer500 B;
//メンバ変数を増やしたら、st_cpyも書き換える
    RingBuffer500 A, B;

    int current_N;
    int initial_N;
    
    const cmd_list_t &get_cmd_list() const {
        return cmd_list;
    }

    //!
    //コマンド列の[l, r)番目を、rhsで置き換える
    const void set_equiv_cmds(int l, int r, const cmd_list_t &mid) {
#ifdef DEBUG
        State prev_r_st = get_state_turn_i(*this, r);
#endif
        cmd_list_t new_cmd_list(get_cmd_list());
        new_cmd_list.list.erase(new_cmd_list.list.begin() + l, new_cmd_list.list.begin() + r);
        new_cmd_list.list.insert(new_cmd_list.list.begin() + l, mid.list.begin(), mid.list.end());
        cmd_list = new_cmd_list;
#ifdef DEBUG
        State aft_r_st = get_state_turn_i(*this, l + mid.list.size());
        my_assert(same_stack(prev_r_st, aft_r_st));
#endif
    }

    //コマンド列として等価な物を代入する
    const void set_equiv_cmds(const cmd_list_t &rhs) {
#ifdef DEBUG
        State start1 = get_start_state(*this);
#endif
        cmd_list = rhs;
#ifdef DEBUG
        State start2 = get_start_state(*this);
        my_assert(start1 == start2);
#endif
    }

    bool operator==(const State &rhs) const {
        return A == rhs.A && B == rhs.B && current_N == rhs.current_N && initial_N == rhs.initial_N && cmd_list == rhs.cmd_list;
    }

    const void set_equiv_cmds(const vector<command> &rhs) {
        cmd_list_t list;
        list.list = rhs;
        set_equiv_cmds(list);
    }

    void validate_stack() {
#ifdef DEBUG_VALIDATE
        const int actual_N = A.size() + B.size();
        my_assert(actual_N == current_N);
        vector<int> seen(initial_N, 0);
        vector<int> v;
        v.reserve(actual_N);

        for (int i = 0; i < A.size(); ++i) v.push_back(A[i]);
        for (int i = 0; i < B.size(); ++i) v.push_back(B[i]);

        for (int value : v) {
            if (!(0 <= value && value < initial_N)) {
                std::stringstream ss;
                ss << "validate_stack: Stack value " << value << " is outside 0 to " << (initial_N - 1) << ".\n";
                cout << ss.str() << endl;
                throw std::runtime_error(ss.str());
            }
            if (seen[value]) {
                std::stringstream ss;
                ss << "validate_stack: Stack value " << value << " is duplicated.\n";
                cout << ss.str() << endl;
                throw std::runtime_error(ss.str());
            }
            seen[value] = 1;
        }
#endif
    }


    State(const RingBuffer500 &A, const RingBuffer500 &B, int current_N_, int initial_N_)
        : A(A), B(B), current_N(current_N_), initial_N(initial_N_), track_cmd_list(false) {
        my_assert(current_N == A.size() + B.size());
        my_assert(0 <= current_N && current_N <= initial_N);
        validate_stack();
    }
    
    State(const RingBuffer500 &A, const RingBuffer500 &B, bool track_cmd_list = true)
            : A(A), B(B), track_cmd_list(track_cmd_list) {
        const int stack_size = A.size() + B.size();
        current_N = stack_size;
        initial_N = stack_size;
        my_assert(0 <= current_N && current_N <= initial_N);
        validate_stack();;
    }
    
    State(const State &rhs) : A(rhs.A), B(rhs.B), current_N(rhs.current_N), initial_N(rhs.initial_N), cmd_list(rhs.cmd_list), track_cmd_list(rhs.track_cmd_list) {
    }
    
    int turn() const {
        return cmd_list.list.size();
    }

    State &operator=(const State &rhs) {
        A = rhs.A;
        B = rhs.B;
        current_N = rhs.current_N;
        initial_N = rhs.initial_N;
        cmd_list = rhs.cmd_list;
        track_cmd_list = rhs.track_cmd_list;
        return *this;
    }

    void revert_last_cmd() {
        my_assert(!cmd_list.list.empty());
        do_cmd(revert_cmd[cmd_list.list.back()]);
        cmd_list.list.pop_back();
        cmd_list.list.pop_back();
    };

    bool can_sa() {
        return can_swap(A);
    }

    bool can_sb() {
        return can_swap(B);
    }

    bool can_ss() {
        return can_sa() && can_sb();
    }


    bool can_pa() {
        return can_push(B, A);
    }

    bool can_pb() {
        return can_push(A, B);
    }


    bool can_ra() {
        return can_rotate(A);
    }

    bool can_rb() {
        return can_rotate(B);
    }

    bool can_rr() {
        return can_ra() && can_rb();
    }

    bool can_rra() {
        return can_rrotate(A);
    }

    bool can_rrb() {
        return can_rrotate(B);
    }

    bool can_rrr() {
        return can_rra() && can_rrb();
    }

    void sa() {
        my_assert(can_sa());
        A.swap_top();
        if (track_cmd_list) cmd_list.add(SA);
    }

    void sb() {
        my_assert(can_sb());
        B.swap_top();
        if (track_cmd_list) cmd_list.add(SB);
    }

    void ss() {
        my_assert(can_ss());
        A.swap_top();
        B.swap_top();
        if (track_cmd_list) cmd_list.add(SS);
    }

    void pa() {
        my_assert(can_pa());
        push(B, A);
        if (track_cmd_list) cmd_list.add(PA);
    }

    void pb() {
        my_assert(can_pb());
        push(A, B);
        if (track_cmd_list) cmd_list.add(PB);
    }

    void ra() {
        my_assert(can_ra());
        rotate(A);
        if (track_cmd_list) cmd_list.add(RA);
    }

    void rb() {
        my_assert(can_rb());
        rotate(B);
        if (track_cmd_list) cmd_list.add(RB);
    }

    void rr() {
        my_assert(can_rr());
        rotate(A);
        rotate(B);
        if (track_cmd_list) cmd_list.add(RR);
    }

    void rra() {
        my_assert(can_rra());
        rrotate(A);
        if (track_cmd_list) cmd_list.add(RRA);
    }

    void rrb() {
        my_assert(can_rrb());
        rrotate(B);
        if (track_cmd_list) cmd_list.add(RRB);
    }

    void rrr() {
        my_assert(can_rrr());
        rrotate(A);
        rrotate(B);
        if (track_cmd_list) cmd_list.add(RRR);
    }

    void do_cmd(command cmd) {
        switch (cmd) {
            case SA:
                sa();
                break;
            case SB:
                sb();
                break;
            case SS:
                ss();
                break;
            case PA:
                pa();
                break;
            case PB:
                pb();
                break;
            case RA:
                ra();
                break;
            case RB:
                rb();
                break;
            case RR:
                rr();
                break;
            case RRA:
                rra();
                break;
            case RRB:
                rrb();
                break;
            case RRR:
                rrr();
                break;
            default:
                // out("switch cmd error"); // why: output.txt出力抑制のため
                my_assert(0);
                break;
        }
    }

    bool can_do(command cmd) {
        switch (cmd) {
            case SA:
                return can_sa();
            case SB:
                return can_sb();
            case SS:
                return can_ss();
            case PA:
                return can_pa();
            case PB:
                return can_pb();
            case RA:
                return can_ra();
            case RB:
                return can_rb();
            case RR:
                return can_rr();
            case RRA:
                return can_rra();
            case RRB:
                return can_rrb();
            case RRR:
                return can_rrr();
            default:
                // out("switch cmd error2"); // why: output.txt出力抑制のため
                my_assert(0);
                return false;
        }
    }

    void print() const{
//        out("turtn :", turn());
        // out("A"); // why: output.txt出力抑制のため
        A.print();
        // out("B"); // why: output.txt出力抑制のため
        B.print();
//        out("A", A);
//        out("B", B);
//        cout<<cmd_list;
    }

    void chk_all_exist() {
        my_assert(A.size() + B.size() == current_N);
        vector<int> seen(initial_N, 0);
        for (int i = 0; i < B.size(); i++) {
            int v = B[i];
            my_assert(0 <= v && v < initial_N);
            my_assert(seen[v] == 0);
            seen[v] = 1;
        }
        for (int i = 0; i < A.size(); i++) {
            int v = A[i];
            my_assert(0 <= v && v < initial_N);
            my_assert(seen[v] == 0);
            seen[v] = 1;
        }
    }
};

void test_state();
//
//dq使ってる古いやつ
//class State {
//    bool can_swap(deque<int> &dq) {
//        return (dq.sz() >= 2);
//    }
//
//    void swap_top(deque<int> &dq) {
//        swap(dq[0], dq[1]);
//    }
//
//    bool can_push(deque<int> &from, deque<int> &to) {
//        return (from.sz() >= 1);
//    }
//
//    void push(deque<int> &from, deque<int> &to) {
//        to.push_front(from.front());
//        from.pop_front();
//    }
//
//    bool can_rotate(deque<int> &q) {
//        return (q.sz() >= 2);
//    }
//
//    bool can_rrotate(deque<int> &q) {
//        return (q.sz() >= 2);
//    }
//
//    void rotate(deque<int> &q) {
//        q.push_back(q[0]);
//        q.pop_front();
//    }
//
//    void rrotate(deque<int> &q) {
//        q.push_front(q.back());
//        q.undo_cmd();
//    }
//
//public:
//    deque<int> A;
//    deque<int> B;
//    int turn;
//    int N;
//    cmd_list_t cmd_list;
//
//    State(int turn, const deque<int> &A, const deque<int> &B) : turn(turn), A(A), B(B) {
//        N = A.sz() + B.sz();
//    }
//
//    bool can_sa() {
//        return can_swap(A);
//    }
//
//    bool can_sb() {
//        return can_swap(B);
//    }
//
//    bool can_ss() {
//        return can_sa() && can_sb();
//    }
//
//
//    bool can_pa() {
//        return can_push(B, A);
//    }
//
//    bool can_pb() {
//        return can_push(A, B);
//    }
//
//
//    bool can_ra() {
//        return can_rotate(A);
//    }
//
//    bool can_rb() {
//        return can_rotate(B);
//    }
//
//    bool can_rr() {
//        return can_ra() && can_rb();
//    }
//
//    bool can_rra() {
//        return can_rrotate(A);
//    }
//
//    bool can_rrb() {
//        return can_rrotate(B);
//    }
//
//    bool can_rrr() {
//        return can_rra() && can_rrb();
//    }
//
//    void sa() {
//        my_assert(can_sa());
//        turn++;
//        swap_top(A);
//        cmd_list.add(SA);
//    }
//
//    void sb() {
//        my_assert(can_sb());
//        turn++;
//        swap_top(B);
//        cmd_list.add(SB);
//    }
//
//    void ss() {
//        my_assert(can_ss());
//        turn++;
//        swap_top(A);
//        swap_top(B);
//        cmd_list.add(SS);
//    }
//
//    void pa() {
//        my_assert(can_pa());
//        turn++;
//        push(B, A);
//        cmd_list.add(PA);
//    }
//
//    void pb() {
//        my_assert(can_pb());
//        turn++;
//        push(A, B);
//        cmd_list.add(PB);
//    }
//
//    void ra() {
//        my_assert(can_ra());
//        turn++;
//        rotate(A);
//        cmd_list.add(RA);
//    }
//
//    void rb() {
//        my_assert(can_rb());
//        turn++;
//        rotate(B);
//        cmd_list.add(RB);
//    }
//
//    void rr() {
//        my_assert(can_rr());
//        turn++;
//        rotate(A);
//        rotate(B);
//        cmd_list.add(RR);
//    }
//
//    void rra() {
//        my_assert(can_rra());
//        turn++;
//        rrotate(A);
//        cmd_list.add(RRA);
//    }
//
//    void rrb() {
//        my_assert(can_rrb());
//        turn++;
//        rrotate(B);
//        cmd_list.add(RRB);
//    }
//
//    void rrr() {
//        my_assert(can_rrr());
//        turn++;
//        rrotate(A);
//        rrotate(B);
//        cmd_list.add(return);
//    }
//
//    void do_cmd(command cmd) {
//        switch (cmd) {
//            case SA:
//                sa();
//                break;
//            case SB:
//                sb();
//                break;
//            case SS:
//                ss();
//                break;
//            case PA:
//                pa();
//                break;
//            case PB:
//                pb();
//                break;
//            case RA:
//                ra();
//                break;
//            case RB:
//                rb();
//                break;
//            case RR:
//                rr();
//                break;
//            case RRA:
//                rra();
//                break;
//            case RRB:
//                rrb();
//                break;
//            case return:
//                rrr();
//                break;
//            default:
//                out("switch cmd error");
//                my_assert(0);
//                break;
//        }
//    }
//
//    bool can_do(command cmd) {
//        switch (cmd) {
//            case SA:
//                return can_sa();
//            case SB:
//                return can_sb();
//            case SS:
//                return can_ss();
//            case PA:
//                return can_pa();
//            case PB:
//                return can_pb();
//            case RA:
//                return can_ra();
//            case RB:
//                return can_rb();
//            case RR:
//                return can_rr();
//            case RRA:
//                return can_rra();
//            case RRB:
//                return can_rrb();
//            case return:
//                return can_rrr();
//            default:
//                out("switch cmd error2");
//                my_assert(0);
//                return false;
//        }
//    }
//
//    void print() {
//        out("turtn :", turn);
//        out("A", A);
//        out("B", B);
////        cout<<cmd_list;
//    }
//};


void test_add_cmd();

ostream &operator<<(ostream &os, State &st);


State init_state(int argc, char *argv[]);

State get_apply_cmds(const State &st, const cmd_list_t &cmd_list, int cmd_l, int cmd_r);

int get_ra_dis(State &st, int a_pos);

int get_rra_dis(State &st, int a_pos);

int get_rb_dis(State &st, int b_pos);

int get_rrb_dis(State &st, int b_pos);

int calc_rot_cost(int dis_a, int dis_b, bool same_dir);

//
//上からa_pos番目に、上からb_pos番目のを挿入する最小コマンド(swapは使わない)
void push_a_with_rot(State &st, int a_pos, int b_pos);

void visualize_cmds(const State &s, const vector<command> &cmds);

//Aがソート済みでなかったらassert
//本稼働する際は呼ばないので、時間かけていい
void validate_ring_sortedA(State &st);

//stの最初の状態を返す
State get_start_state(const State &st);

State get_state_turn_i(const State &st, size_t i);

void test_get_start_state();

bool same_stack(const State &lhs, const State &rhs);

#endif //UNTITLED_STATE_H
