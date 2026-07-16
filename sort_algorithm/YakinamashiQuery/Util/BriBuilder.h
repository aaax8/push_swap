#pragma once

#include "BestRotInfo.h"
#include "QueriesCalculator.h"
#include "QueryCommonRIFactory.h"
#include "QueriesState.h"

class BriBuilder {
public:
//    static BestRotInfo to_ab_best_rot_info(int N,
//                                           int q_idx,
//                                           const QRIList &queries,
//                                           const QueryCommonRI &qri) {
//        qri.validate_is_ab_type();
//        QueriesCalculator qs_calc(N, queries);
//        int a_cnt = 0;
//        int b_cnt = 0;
//        if (qri.is_a2b_type()) {
//            a_cnt = qs_calc.get_orig_a_cmd_cnt_A2B(q_idx);
//            b_cnt = qs_calc.get_orig_b_cmd_cnt_A2B(q_idx);
//        } else {
//            my_assert(qri.is_b2a_type());
//            a_cnt = qs_calc.get_orig_a_cmd_cnt_B2A(q_idx);
//            b_cnt = qs_calc.get_orig_b_cmd_cnt_B2A(q_idx);
//        }
//        return BestRotInfo{
//            qri.ab_cmd_a(),
//            qri.ab_cmd_b(),
//            a_cnt,
//            b_cnt,
//            qri.ab_rot_turn()
//        };
//    }

    static BestRotInfo build_bri_ignore_P_a2a(int N,
                                              QCRIFactory &cri_factory,
                                              const QueriesState &yaki_state,
                                              int tar_q_idx) {
        QCRIFactory::validate_exist_q_idx(yaki_state, tar_q_idx);
        QueriesCalculator qs_calc(N, yaki_state.queries);
        int a_cnt;
        int b_cnt;
        if (yaki_state.queries[tar_q_idx].is_a2b_type()) {
            a_cnt = qs_calc.get_orig_a_cmd_cnt_A2B(tar_q_idx) ;
            b_cnt = qs_calc.get_orig_b_cmd_cnt_A2B(tar_q_idx) ;
        }else{
            a_cnt = qs_calc.get_orig_a_cmd_cnt_B2A(tar_q_idx) ;
            b_cnt = qs_calc.get_orig_b_cmd_cnt_B2A(tar_q_idx) ;
        }

        return BestRotInfo{
                yaki_state.queries[tar_q_idx].get_a_cmd_AB(),
                yaki_state.queries[tar_q_idx].get_b_cmd_AB(),
            a_cnt,
            b_cnt,
            -1
        };
    }
};
