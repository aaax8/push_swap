// QueryStateApplyUtil.h
// QueryRIをStateへ適用する責務を分離したユーティリティ
#pragma once

#include "BestRotInfo.h"
#include "QueriesCalculator.h"
#include "QueryCommonRI.h"
#include "State.h"

namespace QueryStateApplyUtil {
    using command = ::command;

    //x
    inline void apply_a_rot(State &st, command rot_cmd, int rot_cnt) {
        if (rot_cmd == RA) {
            for (int i = 0; i < rot_cnt; i++) {
                st.ra();
            }
        } else if (rot_cmd == RRA) {
            for (int i = 0; i < rot_cnt; i++) {
                st.rra();
            }
        }
    }

    //x
    inline void apply_push_rot_a2a(State &st, int flip_dis_dir) {
        int abs_flip = abs(flip_dis_dir);
        st.pb();
        if (flip_dis_dir > 0) {
            for (int i = 0; i < abs_flip; i++) {
                st.ra();
            }
        } else {
            for (int i = 0; i < abs_flip; i++) {
                st.rra();
            }
        }
        st.pa();
    }

    //x
    inline void apply_swap_rot_a2a(State &st, int flip_dis_dir) {
        int abs_flip = abs(flip_dis_dir);
        if (flip_dis_dir > 0) {
            st.pb();
            for (int i = 0; i < abs_flip; i++) {
                st.ra();
            }
            st.pa();
            st.rra();
            return;
        }
        st.ra();
        st.pb();
        for (int i = 0; i < abs_flip; i++) {
            st.rra();
        }
        st.pa();
    }

    //x
    inline void apply_ab_rot_to_state(State &st, const BestRotInfo &brp) {
        command a_cmd = brp.cmd_a;
        int a_cnt = brp.cmd_a_cnt;
        if (a_cmd == RA) {
            for (int i = 0; i < a_cnt; i++) {
                st.ra();
            }
        } else if (a_cmd == RRA) {
            for (int i = 0; i < a_cnt; i++) {
                st.rra();
            }
        }

        command b_cmd = brp.cmd_b;
        int b_cnt = brp.cmd_b_cnt;
        if (b_cmd == RB) {
            for (int i = 0; i < b_cnt; i++) {
                st.rb();
            }
        } else if (b_cmd == RRB) {
            for (int i = 0; i < b_cnt; i++) {
                st.rrb();
            }
        }
    }

    //x
    inline void apply_a2a_to_state(State &st, const QueryCommonRI &qri) {
        // _BEGIN: A2AP 廃止で a2a_type 変数と push_rot ブランチを削除。常に swap_rot。
        apply_a_rot(st, qri.a2a_cmd_rot_a1(), qri.a2a_cmd_rot_a1_cnt());
        int flip_dis_dir = qri.get_query().get_flip_dis_dir();
        if (flip_dis_dir == 0) {
            return;
        }
        apply_swap_rot_a2a(st, flip_dis_dir);
        // _END
    }

    inline BestRotInfo build_ab_best_rot_info_from_orig(const QueriesCalculator &qs_calc,
                                                        int q_idx,
                                                        const QueryCommonRI &qri) {
        qri.validate_is_ab_type();
        if (qri.is_a2b_type()) {
            return BestRotInfo{
                qri.ab_cmd_a(),
                qri.ab_cmd_b(),
                qs_calc.get_orig_a_cmd_cnt_A2B(q_idx),
                qs_calc.get_orig_b_cmd_cnt_A2B(q_idx),
                -1
            };
        }
        my_assert(qri.is_b2a_type());
        return BestRotInfo{
            qri.ab_cmd_a(),
            qri.ab_cmd_b(),
            qs_calc.get_orig_a_cmd_cnt_B2A(q_idx),
            qs_calc.get_orig_b_cmd_cnt_B2A(q_idx),
            -1
        };
    }

    inline void apply_query_to_state(State &st,
                                     const QueriesCalculator &qs_calc,
                                     int q_idx,
                                     const QueryCommonRI &qri) {
        const QueryCommon &query = qri.get_query();
        if (query.get_push_type() == CommonPushType::A2B) {
            BestRotInfo bri = build_ab_best_rot_info_from_orig(qs_calc, q_idx, qri);
            apply_ab_rot_to_state(st, bri);
            st.pb();
        } else if (query.get_push_type() == CommonPushType::B2A) {
            BestRotInfo bri = build_ab_best_rot_info_from_orig(qs_calc, q_idx, qri);
            apply_ab_rot_to_state(st, bri);
            st.pa();
        } else {
            // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
            my_assert(query.get_push_type() == CommonPushType::A2AS);
            // _END
            apply_a2a_to_state(st, qri);
        }
    }

    //x
    inline void apply_query_to_state(State &st, const QueryCommonRI &qri) {
        const QueryCommon &query = qri.get_query();
        // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
        my_assert(query.get_push_type() == CommonPushType::A2AS);
        // _END
        apply_a2a_to_state(st, qri);
    }
}
