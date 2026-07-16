
#pragma once
#include "Command.h"
#include "QueryCommon.h"
#include "QueryCommonRI.h"
#include "BestRotInfo.h"
#include <iostream>

// --- QueryRI基底クラス ---
class BaseQueryRI {
public:
    virtual ~BaseQueryRI() = default;
    virtual int get_ins_turn() const = 0;
    virtual QueryCommon get_query_common() const = 0;
    virtual void print() const = 0;
};

// --- A2BQueryRI ---
class A2BQueryRI : public BaseQueryRI {
public:
    A2BQuery query;
    BestRotInfo best_rpi;

    A2BQueryRI(const A2BQuery& q, const BestRotInfo& bri)
        : query(q), best_rpi(bri) {}

    int get_ins_turn() const override { return best_rpi.ins_turn; }
    QueryCommon get_query_common() const override { return to_common_query(query); }

    command get_a_cmd() const { return best_rpi.cmd_a; }
    command get_b_cmd() const { return best_rpi.cmd_b; }
    int get_a_cnt() const { return best_rpi.cmd_a_cnt; }
    int get_b_cnt() const { return best_rpi.cmd_b_cnt; }

    void print() const override {
        std::cout << "[A2BQueryRI] ";
        query.print();
        std::cout << "  cmd_a=" << best_rpi.cmd_a << " cmd_b=" << best_rpi.cmd_b
                  << " cmd_a_cnt=" << best_rpi.cmd_a_cnt << " cmd_b_cnt=" << best_rpi.cmd_b_cnt
                  << " ins_turn=" << best_rpi.ins_turn << std::endl;
    }

    const A2BQuery& get_a2b_query() const {
        return static_cast<const A2BQuery&>(query);
    }
};


// ...existing code...

// --- B2AQueryRI ---
class B2AQueryRI : public BaseQueryRI {
public:
    B2AQuery query;
    BestRotInfo best_rpi;
    
    B2AQueryRI(const B2AQuery& q, const BestRotInfo& bri)
        : query(q), best_rpi(bri) {}
    
    int get_ins_turn() const override { return best_rpi.ins_turn; }
    QueryCommon get_query_common() const override { return to_common_query(query); }

    command get_a_cmd() const { return best_rpi.cmd_a; }
    command get_b_cmd() const { return best_rpi.cmd_b; }
    int get_a_cnt() const { return best_rpi.cmd_a_cnt; }
    int get_b_cnt() const { return best_rpi.cmd_b_cnt; }
    
    void print() const override {
        std::cout << "[B2AQueryRI] ";
        query.print();
        std::cout << "  cmd_a=" << best_rpi.cmd_a << " cmd_b=" << best_rpi.cmd_b
                  << " cmd_a_cnt=" << best_rpi.cmd_a_cnt << " cmd_b_cnt=" << best_rpi.cmd_b_cnt
                  << " ins_turn=" << best_rpi.ins_turn << std::endl;
    }

    const B2AQuery& get_b2a_query() const {
        return static_cast<const B2AQuery&>(query);
    }
};

// --- A2AQueryRI ---
//swap_rotの場合、cmd1が終わったらswapから開始
class A2AQueryRI : public BaseQueryRI {
private:
    A2AQuery query;
    command cmd_rot_a1;
    int cmd_rot_a1_cnt;
    int ins_turn;

public:
    
    A2AQueryRI(const A2AQuery& q, command cmd_rot_a1_, int cmd_rot_a1_cnt_, int ins_turn_)
        : query(q), cmd_rot_a1(cmd_rot_a1_), cmd_rot_a1_cnt(cmd_rot_a1_cnt_), ins_turn(ins_turn_) {}
    
    int get_ins_turn() const override { return ins_turn; }
    QueryCommon get_query_common() const override { return to_common_query(query); }
    command get_cmd_rot_a1() const { return cmd_rot_a1; }
    int get_cmd_rot_a1_cnt() const { return cmd_rot_a1_cnt; }
    
    void print() const override {
        std::cout << "[A2AQueryRI] ";
        query.print();
        std::cout << "  cmd_rot_a1=" << cmd_rot_a1 << " cmd_rot_a1_cnt=" << cmd_rot_a1_cnt
                   << " ins_turn=" << ins_turn << std::endl;
    }

    const A2AQuery& get_a2a_query() const {
        return static_cast<const A2AQuery&>(query);
    }
};


//x
inline QueryCommonRI build_common_ri_from_a2a(const A2AQueryRI &qri) {
    return QueryCommonRI(
        to_common_query(qri.get_a2a_query()),
        qri.get_ins_turn(),
        qri.get_cmd_rot_a1(),
        qri.get_cmd_rot_a1_cnt()
    );
}

//x
//inline QueryCommonRI to_common_ri(const BaseQueryRI &qri) {
//    QueryCommon common_query = qri.get_query_common();
//    if (common_query.get_push_type() == CommonPushType::A2B) {
//        return build_common_ri_from_a2b(static_cast<const A2BQueryRI &>(qri));
//    }
//    if (common_query.get_push_type() == CommonPushType::B2A) {
//        return build_common_ri_from_b2a(static_cast<const B2AQueryRI &>(qri));
//    }
//    my_assert(common_query.get_push_type() == CommonPushType::A2AP ||
//              common_query.get_push_type() == CommonPushType::A2AS);
//    return build_common_ri_from_a2a(static_cast<const A2AQueryRI &>(qri));
//}

