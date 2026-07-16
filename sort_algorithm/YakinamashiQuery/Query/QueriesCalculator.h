// QueriesCalculator.h
// クエリリストに対する集約・計算責務クラス
#ifndef UNTITLED_QUERIESCALCULATOR_H
#define UNTITLED_QUERIESCALCULATOR_H

#include "QueryCommonRI.h"
#include "RotCalculator.h"
#include <vector>
#include "QueriesValidator.h"

//古い奴　とりあえず放置

class QueriesCalculator {
public:
    using CRIList = std::vector<QueryCommonRI>; // 必要に応じてmy_vector等に変更
private:
    const CRIList& queries;
    int N;
public:
    explicit QueriesCalculator(int N, const CRIList& queries) : N(N), queries(queries) {}


    bool is_prev_a2a_idx(int idx)const{
        QueriesValidator::validate_qi(idx, queries.size());
        if(idx==0){
            return false;
        }
        const auto& prev_q = queries[idx-1];
        return prev_q.get_query().is_a2a_type();
    }

    void validate_orig_rb_cnt(int idx, int rb_dis)const{
#ifdef DEBUG
        QueriesValidator::validate_qi(idx, queries.size());
        if(!is_prev_a2a_idx(idx)){
            return;
        }
        auto& curq = queries[idx];
        curq.validate_is_ab_type();
        my_assert(curq.debug_orig_rot_info_AB_P_A2A.orig_rb_cnt == rb_dis);
#endif
    }

    void validate_orig_rrb_cnt(int idx, int rrb_dis)const{
#ifdef DEBUG
        QueriesValidator::validate_qi(idx, queries.size());
        if(!is_prev_a2a_idx(idx)){
            return;
        }
        auto& curq = queries[idx];
        curq.validate_is_ab_type();
        my_assert(curq.debug_orig_rot_info_AB_P_A2A.orig_rrb_cnt == rrb_dis);
#endif
    }
    void validate_orig_ra_cnt(int idx, int ra_dis)const{
      #ifdef DEBUG
        QueriesValidator::validate_qi(idx, queries.size());
        if(!is_prev_a2a_idx(idx)){
            return;
        }
        auto& curq = queries[idx];
        curq.validate_is_ab_type();
        my_assert(curq.debug_orig_rot_info_AB_P_A2A.ra_cnt == ra_dis);
#endif  
    }

    void validate_orig_rra_cnt(int idx, int rra_dis)const{
#ifdef DEBUG
        QueriesValidator::validate_qi(idx, queries.size());
        if(!is_prev_a2a_idx(idx)){
            return;
        }
        auto& curq = queries[idx];
        curq.validate_is_ab_type();
        my_assert(curq.debug_orig_rot_info_AB_P_A2A.rra_cnt == rra_dis);
#endif
    }

    //todo
    //ra_rraはrb, rrbと違って、costがorig_cntと同じで
    //get_origなんとかでbと同じぐらい処理和しているのが変
    // 指定インデックスのQueryCommonRIに対して、元のaコマンド回数を取得
    //x
    int get_orig_a_cmd_cnt_A2B(int idx) const {
        QueriesValidator::validate_qi(idx, queries.size());
        if(idx==0){
            return queries[idx].get_a_cmd_cost_AB();
        }
        my_assert(0 < idx && idx < queries.size()); 
        const auto& prevq = queries[idx - 1];
        const auto& curq = queries[idx];
        curq.validate_is_ab_type();
        OrdPos start_ord_pos_a = prevq.get_finished_top_ord_a();
        OrdPos tar_ord_pos_a = curq.get_query().get_tar_ord_pos_a_A2B();
        if(curq.get_a_cmd_AB() == RA){
            int ra_dis = RotCalculator::get_ra_dis(start_ord_pos_a, tar_ord_pos_a, curq.get_stack_a_cnt());
            validate_orig_ra_cnt(idx, ra_dis);
            return ra_dis;
        }else{
            int rra_dis = RotCalculator::get_rra_dis(start_ord_pos_a, tar_ord_pos_a, curq.get_stack_a_cnt());
            validate_orig_rra_cnt(idx, rra_dis);
            return rra_dis;
        }
    }

       //x 
    int get_orig_a_cmd_cnt_B2A(int idx) const {
        QueriesValidator::validate_qi(idx, queries.size());
       if(idx==0){
           return queries[idx].get_a_cmd_cost_AB();
       }
        my_assert(0 < idx && idx < queries.size()); 
        const auto& prevq = queries[idx - 1];
        const auto& curq = queries[idx];
        curq.validate_is_ab_type();
        OrdPos start_ord_pos_a = prevq.get_finished_top_ord_a();
        OrdPos tar_ord_pos_a = to_ord_pos(curq.get_query().get_tar_rel_ord_a_B2A(), curq.get_stack_a_cnt());
        if(curq.get_a_cmd_AB() == RA){
            int ra_dis = RotCalculator::get_ra_dis(start_ord_pos_a, tar_ord_pos_a, curq.get_stack_a_cnt());
            validate_orig_ra_cnt(idx, ra_dis);
            return ra_dis;
        }else{
            int rra_dis = RotCalculator::get_rra_dis(start_ord_pos_a, tar_ord_pos_a, curq.get_stack_a_cnt());
            validate_orig_rra_cnt(idx, rra_dis);
            return rra_dis;
        }
    }
        //x 
    int get_orig_b_cmd_cnt_A2B(int idx)const{
        QueriesValidator::validate_qi(idx, queries.size());
        if(idx==0){
            return queries[idx].get_b_cmd_cost_AB();
        }
        my_assert(0 < idx && idx < queries.size()); 
        const auto& prevq = queries[idx - 1];
        const auto& curq = queries[idx];
        curq.validate_is_ab_type();
        OrdPos start_ord_pos_b = prevq.get_query().get_finished_top_ord_b(N);
        OrdPos tar_ord_pos_b = to_ord_pos(curq.get_query().get_tar_rel_ord_b_A2B(), curq.get_stack_b_cnt(N));
        if(curq.get_b_cmd_AB() == RB){
            int rb_dis = RotCalculator::get_rb_dis(start_ord_pos_b, tar_ord_pos_b, curq.get_stack_b_cnt(N));
            validate_orig_rb_cnt(idx, rb_dis);
            return rb_dis;
        }else{
            int rrb_dis = RotCalculator::get_rrb_dis(start_ord_pos_b, tar_ord_pos_b, curq.get_stack_b_cnt(N));
            validate_orig_rrb_cnt(idx, rrb_dis);
            return rrb_dis;
        }
    }

        //x 
    int get_orig_b_cmd_cnt_B2A(int idx)const{
        QueriesValidator::validate_qi(idx, queries.size());
        if(idx==0){
            return queries[idx].get_b_cmd_cost_AB();
        }
        my_assert(0 < idx && idx < queries.size()); 
        const auto& prevq = queries[idx - 1];
        const auto& curq = queries[idx];
        curq.validate_is_ab_type();
        OrdPos start_ord_pos_b = prevq.get_query().get_finished_top_ord_b(N);
        OrdPos tar_ord_pos_b = curq.get_query().get_tar_ord_pos_b_B2A();
        if(curq.get_b_cmd_AB() == RB){
            int rb_dis = RotCalculator::get_rb_dis(start_ord_pos_b, tar_ord_pos_b, curq.get_stack_b_cnt(N));
            validate_orig_rb_cnt(idx, rb_dis);
            return rb_dis;
        }else{
            int rrb_dis = RotCalculator::get_rrb_dis(start_ord_pos_b, tar_ord_pos_b, curq.get_stack_b_cnt(N));
            validate_orig_rrb_cnt(idx, rrb_dis);
            return rrb_dis;
        }
    }
};

inline OrdPos calc_start_ord_b(const QueriesState&qst, int q_idx){
    QueriesValidator::validate_qi(q_idx, qst.queries.size());
    if(q_idx == 0){
        return OrdPos(0);//todo 知識の散逸
    }
    int current_N = qst.current_N;
    return qst.queries[q_idx - 1].get_query().get_finished_top_ord_b(current_N);
}

#endif //UNTITLED_QUERIESCALCULATOR_H
