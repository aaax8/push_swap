// filepath: sort_algorithm/YakinamashiQuery/QueryFactory.h
#pragma once

#include "InitBeam/BeamEnums.h"
#include "InitBeam/a2a_calculator.h"
#include "OrdPosCalculator.h"
#include "RotCalculator.h"
#include "Yakinamashi/Order/OrdTypes.h"
#include "QueryCommon.h"
#include "QueryCommonRI.h"
#include "YQTraits.h"
#include "BestRotInfo.h"
#include "util.h"
#include "State.h"
#include "YQTraits.h"
#include"OrdTypes.h"
//TODO_COPILOT:
//これは完全になくしてQCRIFacotryに置き換えるべき
class QueryFactory {
private:
    const State &init_st;
    int N;
    OrdPos first_top_ord_a;
    OrdPos first_top_ord_b;

    std::pair<OrdPos, OrdPos> get_finished_top_ords(const QueryCommon &prev_q) const {
        return {prev_q.get_finished_top_ord_a(N), prev_q.get_finished_top_ord_b(N)};
    }


public:
    QueryFactory(const State &init_st, int N, OrdPos first_top_ord_a, OrdPos first_top_ord_b) :
            init_st(init_st), N(N), first_top_ord_a(first_top_ord_a), first_top_ord_b(first_top_ord_b) {
        my_assert(init_st.A.size() == N);
        my_assert(init_st.B.size() == 0);
        validate_OrdPos(first_top_ord_a, N);
        validate_OrdPos(first_top_ord_b, 0);
    }

    //x
//     QueryCommonRI build_first_qri(const QueryCommon &query) {
//         int stack_a_size = query.get_stack_a_cnt();
//         int stack_b_size = query.get_stack_b_cnt(N);
//         if (query.get_push_type() == CommonPushType::A2B) {
//             return QueryCommonRI(query, build_brp_by_ord(
//                 stack_a_size, stack_b_size,
//                 first_top_ord_a, first_top_ord_b, 
//                 query.get_tar_ord_pos_a_A2B(), 
//                 to_ord_pos(query.get_tar_rel_ord_b_A2B(), stack_b_size)));
//         }
//         if (query.get_push_type() == CommonPushType::B2A) {
//                   return QueryCommonRI(query, build_brp_by_ord(
//                 stack_a_size, stack_b_size,
//                 first_top_ord_a, first_top_ord_b, 
//                 to_ord_pos(query.get_tar_rel_ord_a_B2A(), stack_a_size),
//                 query.get_tar_ord_pos_b_B2A()));
//         }
//         return QueryCommonRI();
//         // A2AQuery a2a_query = build_a2a_query_from_common(get_query);
//         // return build_common_ri_from_a2a(build_a2a_qri_impl(first_top_ord_a, a2a_query));
//     }

//    //x
//    QueryCommonRI build_qri(const QueryCommon &prev_q, const QueryCommon &query) {
//        auto [top_ord_a, top_ord_b] = get_finished_top_ords(prev_q);
//        if (query.get_push_type() == CommonPushType::A2B) {
//            A2BQuery a2b_query = build_a2b_query_from_common(query);
//            return build_common_ri_from_a2b(
//                    build_ab_push_qri_by_ord(a2b_query, top_ord_a, top_ord_b,
//                                             a2b_query.get_tar_ord_pos_a(), a2b_query.get_tar_ord_pos_b(N))
//            );
//        }
//        if (query.get_push_type() == CommonPushType::B2A) {
//            B2AQuery b2a_query = build_b2a_query_from_common(query);
//            return build_common_ri_from_b2a(
//                    build_ab_push_qri_by_ord(b2a_query, top_ord_a, top_ord_b,
//                                             b2a_query.get_tar_ord_pos_a(), b2a_query.get_tar_ord_pos_b(N))
//            );
//        }
//        A2AQuery a2a_query = build_a2a_query_from_common(query);
//        return build_common_ri_from_a2a(build_a2a_qri_impl(top_ord_a, a2a_query));
//    }

    //x

private:




    int calc_a2a_ins_turn2(int inter_cnt, e_a2a_type a2a_type) {
        if (a2a_type == e_a2a_type::swap_rot) {
            return calc_a2a_by_sa_rot(inter_cnt);
        } else if (a2a_type == e_a2a_type::push_rot) {
            return calc_a2a_by_push(inter_cnt);
        } else {
            my_assert(false);
            return -1;
        }
    }

    //x
    tuple<command, int, int> get_cmd1_cnt1_ins(e_a2a_type a2a_type, OrdPos start_top_ord_a, OrdPos tar_orda1,
                                               int a_size, int flip_dis_dir) {
        //vをtopにもっていく
        if(a2a_type == e_a2a_type::swap_rot && flip_dis_dir < 0){
            OrdPos prev_tar_ord_a1 = OrdCalc::get_ord_prev(tar_orda1, a_size);
            auto[cmd1, cnt1] = RotCalculator::get_best_rot_a(start_top_ord_a, prev_tar_ord_a1, a_size);
            int ins_turn = cnt1 + calc_a2a_ins_turn2(-flip_dis_dir, a2a_type);
            return {cmd1, cnt1, ins_turn};
        }else{
            my_assert(a2a_type != e_a2a_type::not_a2a);
            auto[cmd1, cnt1] = RotCalculator::get_best_rot_a(start_top_ord_a, tar_orda1, a_size);
            int ins_turn = cnt1 + calc_a2a_ins_turn2(abs(flip_dis_dir), a2a_type);
            return {cmd1, cnt1, ins_turn};
        }
    }
// A2AQueryRI(const A2AQuery& q, command cmd_rot_a1_, int cmd_rot_a1_cnt_, int flip_dis_dir_, int ins_turn_)

};

