#pragma once

#include "Command/Command.h"
#include "PrevA2ARotSum.h"
#include "QueryCommon.h"
#include "BestRotInfo.h"
#include "RotCntInfo.h"

class QCRIFactory;


// : A2A の 0手ケースでは finished_top_ord_a が直前依存になるため、RI 側へ保持先を移す。
class QueryCommonRI {
    QueryCommon query;

    command cmd1;
    command cmd2;

    int cmd_cnt1;
    int cmd_cnt2;
    int ins_turn;
    OrdPos finished_top_ord_a;

public:
    QueryCommonRI() = delete;

#ifdef DEBUG
    PrevA2ARotSum debug_prev_a2a_rot_sum;
    RotCntInfo debug_orig_rot_info_AB_P_A2A;//prevがa2aの時だけ正しい値が入っている
#endif

    bool is_a2b_type() const {
        return query.is_a2b_type();
    }

    bool is_b2a_type() const {
        return query.is_b2a_type();
    }

    bool is_ab_type() const {
        return query.is_ab_type();
    }

    bool is_a2a_type() const {
        return query.is_a2a_type();
    }

    void validate_is_ab_type() const {
        query.validate_is_ab_type();
    }

    void validate_is_a2b_type() const {
        query.validate_is_a2b_type();
    }

    void validate_is_b2a_type() const {
        query.validate_is_b2a_type();
    }

    void validate_is_a2a_type() const {
        query.validate_is_a2a_type();
    }

    CommonPushType get_push_type() const {
        return query.get_push_type();
    }

    int get_tar_v() const {
        return query.get_tar_v();
    }

    int get_stack_a_cnt() const {
        return query.get_stack_a_cnt();
    }

    int get_stack_b_cnt(int N) const {
        return N - get_stack_a_cnt();
    }

    int get_ins_turn() const {
        return ins_turn;
    }

    const QueryCommon &get_query() const {
        return query;
    }
    
 QueryCommon &get_query()  {
        return query;
    }
private:
    friend class QCRIFactory;

    QueryCommonRI(const QueryCommon &query, command cmd_a, command cmd_b, int cmd_a_cnt, int cmd_b_cnt, int ins_turn,
                  OrdPos finished_top_ord_a)
            : query(query), cmd1(cmd_a), cmd2(cmd_b), cmd_cnt1(cmd_a_cnt), cmd_cnt2(cmd_b_cnt), ins_turn(ins_turn),
              finished_top_ord_a(finished_top_ord_a) {
        validate_is_ab_type();
    }

    QueryCommonRI(const QueryCommon &query, const BestRotInfo &bri, OrdPos finished_top_ord_a)
            : QueryCommonRI(query, bri.cmd_a, bri.cmd_b, bri.cmd_a_cnt, bri.cmd_b_cnt, bri.ins_turn, finished_top_ord_a) {}

public:

    bool operator==(const QueryCommonRI &rhs) const {
        return query == rhs.query &&
               cmd1 == rhs.cmd1 &&
               cmd2 == rhs.cmd2 &&
               cmd_cnt1 == rhs.cmd_cnt1 &&
               cmd_cnt2 == rhs.cmd_cnt2 &&
               ins_turn == rhs.ins_turn &&
               finished_top_ord_a == rhs.finished_top_ord_a;
    }

    bool operator!=(const QueryCommonRI &rhs) const {
        return !(*this == rhs);
    }

    const command &get_a_cmd_AB() const {
        validate_is_ab_type();
        return cmd1;
    }

    const command &get_b_cmd_AB() const {
        validate_is_ab_type();
        return cmd2;
    }

    //prevのa2aなどとsyncした残りの回数を返すので、costという名前にした
    //prevがa2aでない場合は、単にa_cmdの回数を返す
    int get_a_cmd_cost_AB() const {
        validate_is_ab_type();
        return cmd_cnt1;
    }

    int get_b_cmd_cost_AB() const {
        validate_is_ab_type();
        return cmd_cnt2;
    }

    int ab_rot_turn() const {
        validate_is_ab_type();
        return ins_turn;
    }

    command ab_cmd_a() const {
        return get_a_cmd_AB();
    }


    command ab_cmd_b() const {
        return get_b_cmd_AB();
    }


private:
    QueryCommonRI(const QueryCommon &query, command cmd_rot_a1, int cmd_rot_a1_cnt, int ins_turn,
                  OrdPos finished_top_ord_a)
            : query(query), cmd1(cmd_rot_a1), cmd2(NO_COMMAND), cmd_cnt1(cmd_rot_a1_cnt), cmd_cnt2(0), ins_turn(ins_turn),
              finished_top_ord_a(finished_top_ord_a) {
        validate_is_a2a_type();
    }

public:
    OrdPos get_finished_top_ord_a() const {
        return finished_top_ord_a;
    }

    int get_flip_dis_dir_A2A() const {
        validate_is_a2a_type();
        return query.get_flip_dis_dir();
    }

    command get_cmd_rot_a1_A2A() const {
        validate_is_a2a_type();
        return cmd1;
    }

    int get_cmd_rot_a1_cnt_A2A() const {
        validate_is_a2a_type();
        return cmd_cnt1;
    }

    command a2a_cmd_rot_a1() const {
        return get_cmd_rot_a1_A2A();
    }

    int a2a_cmd_rot_a1_cnt() const {
        return get_cmd_rot_a1_cnt_A2A();
    }

    int a2a_ins_turn() const {
        validate_is_a2a_type();
        return ins_turn;
    }

    std::pair<int, int> get_can_sync_ra_rra_A2A() const {
        validate_is_a2a_type();
        const QueryCommon &q = get_query();
        command rot_cmd = get_cmd_rot_a1_A2A();
        int rot_cnt = get_cmd_rot_a1_cnt_A2A();
        PrevA2ARotSum sum = PrevA2ARotSumCalc::calc_a2a_sync_rot_sum(
            rot_cmd, rot_cnt, q.get_flip_dis_dir(), e_a2a_type::swap_rot
        );
        return {sum.ra_sum, sum.rra_sum};
    }

    void debug_set_prev_a2a_rot_sum_AB(const PrevA2ARotSum &prev_a2a_rot_sum) {
#ifdef DEBUG
        debug_prev_a2a_rot_sum = prev_a2a_rot_sum;
#endif
    }

    void debug_set_orig_rot_info_AB_P_A2A(const RotCntInfo &info) {
#ifdef DEBUG
        debug_orig_rot_info_AB_P_A2A = info;
#endif
    }
};

inline ostream &operator<<(ostream &os, const QueryCommonRI &qri) {
    os << qri.get_query();
    if (qri.is_ab_type()) {
        os << " [a_cmd:" << qri.ab_cmd_a()
           << " a_cost:" << qri.get_a_cmd_cost_AB()
           << " b_cmd:" << qri.ab_cmd_b()
           << " b_cost:" << qri.get_b_cmd_cost_AB()
           << " turn:" << qri.get_ins_turn() << "]";
    } else {
        os << " [rot_a1:" << qri.a2a_cmd_rot_a1()
           << " rot_a1_cnt:" << qri.a2a_cmd_rot_a1_cnt()
           << " finished_top_a:" << qri.get_finished_top_ord_a().get()
           << " turn:" << qri.get_ins_turn() << "]";
    }
    return os;
}
