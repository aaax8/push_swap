//QueryCommon.h
//YakinamashiQuery.hと内容がかぶってるのが気になる



//
//

#ifndef UNTITLED_QUERYCOMMON_H
#define UNTITLED_QUERYCOMMON_H

#pragma once
#include "sort_algorithm/InitBeam/BeamEnums.h"
#include "Yakinamashi/Order/OrdTypes.h"
#include <cstdint>

namespace QueryCommonConstants {
    constexpr int UNUSED_ORD = -1;
    constexpr int UNUSED_FLIP_DIS_DIR = 0;
}

// : A2AP を廃止。BeamSearch 結果は上流で A2B+B2A に変換済みのため不要。
enum class CommonPushType: std::uint8_t { A2B, B2A, A2AS};

// 既存クエリとは独立して使う共通値型
class QueryCommon {
    CommonPushType push_type;
    int tar_v;
    int stack_a_cnt;
    int ord1;
    int ord2;
    int ord3;
    int flip_dis_dir;

public:
    QueryCommon():push_type(CommonPushType::A2B), 
    tar_v(-1), stack_a_cnt(-1), ord1(-1), ord2(-1), ord3(-1), flip_dis_dir(0)
    {
        
    }

    QueryCommon(CommonPushType push_type,
                int tar_v,
                int stack_a_cnt,
                int ord1,
                int ord2)
            : push_type(push_type),
              tar_v(tar_v),
              stack_a_cnt(stack_a_cnt),
              ord1(ord1),
              ord2(ord2),
              ord3(QueryCommonConstants::UNUSED_ORD),
              flip_dis_dir(QueryCommonConstants::UNUSED_FLIP_DIS_DIR) {
    }

    QueryCommon(CommonPushType push_type,
                int tar_v,
                int stack_a_cnt,
                int ord1,
                int ord2,
                int ord3,
                int flip_dis_dir)
            : push_type(push_type),
              tar_v(tar_v),
              stack_a_cnt(stack_a_cnt),
              ord1(ord1),
              ord2(ord2),
              ord3(ord3),
              flip_dis_dir(flip_dis_dir) {
    }

    bool operator==(const QueryCommon &rhs) const {
        return push_type == rhs.push_type &&
               tar_v == rhs.tar_v &&
               stack_a_cnt == rhs.stack_a_cnt &&
               ord1 == rhs.ord1 &&
               ord2 == rhs.ord2 &&
               ord3 == rhs.ord3 &&
               flip_dis_dir == rhs.flip_dis_dir;
    }

    bool operator!=(const QueryCommon &rhs) const {
        return !(*this == rhs);
    }

    bool is_a2b_type() const { return push_type == CommonPushType::A2B; }
    bool is_b2a_type() const { return push_type == CommonPushType::B2A; }
    bool is_ab_type() const { return is_a2b_type() || is_b2a_type(); }
    // _BEGIN: A2AP 廃止に伴い A2AP の条件を削除。
    bool is_a2a_type() const { return push_type == CommonPushType::A2AS; }
    // _END

    void validate_is_a2b_type()const{
        my_assert(is_a2b_type());
    }

    void validate_is_b2a_type()const{
        my_assert(is_b2a_type());
    }

    void validate_is_a2a_type()const{
        my_assert(is_a2a_type());
    }

    void validate_is_ab_type()const {
        my_assert(is_a2b_type() || is_b2a_type());
    }

    CommonPushType get_push_type() const { return push_type; }
    int get_tar_v() const { return tar_v; }
    int get_stack_a_cnt() const { return stack_a_cnt; }
    int get_stack_b_cnt(int N) const { return N - stack_a_cnt; }

    // A2B専用 getter
    OrdPos get_tar_ord_pos_a_A2B() const {
        validate_is_a2b_type();
        return OrdPos(ord1);
    }

    RelOrd get_tar_rel_ord_b_A2B() const {
        validate_is_a2b_type();
        return RelOrd(ord2);
    }

    // B2A専用 getter
    RelOrd get_tar_rel_ord_a_B2A() const {
        validate_is_b2a_type();
        return RelOrd(ord1);
    }

    OrdPos get_tar_ord_pos_b_B2A() const {
        validate_is_b2a_type();
        return OrdPos(ord2);
    }

    // A2A専用 getter
    OrdPos get_tar_ord_pos_a1_A2A() const {
        validate_is_a2a_type();
        return OrdPos(ord1);
    }

    RelOrd get_tar_rel_ord_a2_A2A() const {
        validate_is_a2a_type();
        return RelOrd(ord2);
    }

    OrdPos get_tar_ord_pos_a2_A2A(int stack_size) const {
        return to_ord_pos(get_tar_rel_ord_a2_A2A(), stack_size);
    }


    OrdPos get_top_ord_pos_b() const {
        validate_is_a2a_type();
        return OrdPos(ord3);
    }

    void set_top_ord_pos_b_A2A(OrdPos ord_pos);

    // : A2AP 廃止で A2AS のみになり常に swap_rot 固定のため get_a2a_type() を削除。

    int get_flip_dis_dir() const {
        validate_is_a2a_type();
        return flip_dis_dir;
    }

    OrdPos get_finished_top_ord_a(int N) const;

    //flip_dis_dirが0の時は、何もしないクエリになる
    OrdPos get_finished_top_ord_a_a2a(OrdPos start_top_ord_a) const;

    OrdPos get_finished_top_ord_b(int N) const;

};

inline string get_common_push_type_str(CommonPushType pt) {
    switch (pt) {
        // _BEGIN: A2AP 廃止に伴い case を削除。
        case CommonPushType::A2B: return "A2B";
        case CommonPushType::B2A: return "B2A";
        case CommonPushType::A2AS: return "A2AS";
        // _END
        default: return "???";
    }
}

inline string get_a2a_type_str_common(e_a2a_type t) {
    switch (t) {
        case e_a2a_type::push_rot: return "push_rot";
        case e_a2a_type::swap_rot: return "swap_rot";
        case e_a2a_type::not_a2a: return "not_a2a";
        default: return "???";
    }
}

inline ostream &operator<<(ostream &os, const QueryCommon &q) {
    os << "{type:" << get_common_push_type_str(q.get_push_type())
       << " v:" << q.get_tar_v()
       << " a_cnt:" << q.get_stack_a_cnt();
    if (q.is_a2b_type()) {
        os << " ord_pos_a:" << q.get_tar_ord_pos_a_A2B().get()
           << " rel_ord_b:" << q.get_tar_rel_ord_b_A2B().get();
    } else if (q.is_b2a_type()) {
        os << " rel_ord_a:" << q.get_tar_rel_ord_a_B2A().get()
           << " ord_pos_b:" << q.get_tar_ord_pos_b_B2A().get();
    } else {
        os << " ord_pos_a1:" << q.get_tar_ord_pos_a1_A2A().get()
           << " rel_ord_a2:" << q.get_tar_rel_ord_a2_A2A().get()
           << " top_ord_b:" << q.get_top_ord_pos_b().get()
           << " flip:" << q.get_flip_dis_dir()
           // _BEGIN: A2AP 廃止で get_a2a_type() 削除のため固定文字列に変更。
           << " a2a:swap_rot";
    }
    os << "}";
    return os;
}

#endif //UNTITLED_QUERYCOMMON_H
