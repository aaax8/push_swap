//
// filepath: sort_algorithm/YakinamashiQuery/YstCommandsBuilder.h
#pragma once

#include "Enums.h"
#include "QueryToCommandBuilder.h"
#include "QueriesState.h"
#include "YQAliases.h"
#include "YQTraits.h"
#include <vector>

#include "QueryCommonRI.h"
#include "QueriesCalculator.h"
#include "util.h"
//
class YstCommandsBuilder {
public:
    static my_vector<command> build_all_commands_from_state(const State &initial_state, const QueriesState &state) {
        my_vector<command> query_cmds = build_commands_from_queries(initial_state, state.queries);
        my_vector<command> final_rot_cmds = build_commands_from_final_rot_info(state.final_rot_info);
        query_cmds.insert(query_cmds.end(), final_rot_cmds.begin(), final_rot_cmds.end());
        return query_cmds;
    }

private:
    static my_vector<command> build_commands_from_queries(const State& initial_state, const QRIList &yst_queries) {
        int N = initial_state.current_N;
        CommandBuildState build_st(initial_state);
        QueryToCommandBuilder builder(build_st);
        QueriesCalculator qs_calc(N, yst_queries);
        int idx = 0;
        for (const auto &qri: yst_queries) {
            const QueryCommon &query = qri.get_query();
            int prev_cmd_size = build_st.cmds.size();
            auto get_inserted_cnt = [&]() -> int {
                return static_cast<int>(build_st.cmds.size()) - prev_cmd_size;
            };
            if (query.get_push_type() == CommonPushType::A2B) {
                builder.add_a2b_b2a(e_push_type::a2b, qri.get_a_cmd_AB(), qri.get_b_cmd_AB(), qs_calc.get_orig_a_cmd_cnt_A2B(idx), qs_calc.get_orig_b_cmd_cnt_A2B(idx));
                my_assert(get_inserted_cnt() == qri.get_ins_turn());
            } else if (query.get_push_type() == CommonPushType::B2A) {
                // 責務: B2A query で実際に追加された command 数と qri の ins_turn がずれた場合に診断情報を出す。
                // 必要な理由: ins_turn 計算側・orig count 側・command builder 側のどこがずれたかを assert 停止前に切り分けるため。
                const int orig_a_cnt = qs_calc.get_orig_a_cmd_cnt_B2A(idx);
                const int orig_b_cnt = qs_calc.get_orig_b_cmd_cnt_B2A(idx);
                const int cmd_size_before = static_cast<int>(build_st.cmds.size());
                builder.add_a2b_b2a(e_push_type::b2a, qri.get_a_cmd_AB(), qri.get_b_cmd_AB(), orig_a_cnt, orig_b_cnt);
                const int cmd_size_after = static_cast<int>(build_st.cmds.size());
                const int inserted_cnt = get_inserted_cnt();
                if (inserted_cnt != qri.get_ins_turn()) {
                    out2("yst_cmd_build_mismatch");
                    out2("q_idx", idx);
                    out2("type", "B2A");
                    out2("inserted_cnt", inserted_cnt);
                    out2("ins_turn", qri.get_ins_turn());
                    out2("orig_a_cnt", orig_a_cnt);
                    out2("orig_b_cnt", orig_b_cnt);
                    out2("a_cmd", qri.get_a_cmd_AB());
                    out2("b_cmd", qri.get_b_cmd_AB());
                    out2("cmd_size_before", cmd_size_before);
                    out2("cmd_size_after", cmd_size_after);
                    out2("prev_seq_a2a_cur_after", build_st.prev_seq_a2a_cur);
                    out2("prev_seq_a2a_end_after", build_st.prev_seq_a2a_end);
                    out2("on_pb_after", build_st.on_pb);
                    out2("qri_a_cost", qri.get_a_cmd_cost_AB());
                    out2("qri_b_cost", qri.get_b_cmd_cost_AB());
#ifdef DEBUG
                    out2("debug_prev_a2a_ra_sum", qri.debug_prev_a2a_rot_sum.ra_sum);
                    out2("debug_prev_a2a_rra_sum", qri.debug_prev_a2a_rot_sum.rra_sum);
                    out2("debug_orig_ra_cnt", qri.debug_orig_rot_info_AB_P_A2A.ra_cnt);
                    out2("debug_orig_rra_cnt", qri.debug_orig_rot_info_AB_P_A2A.rra_cnt);
                    out2("debug_orig_rb_cnt", qri.debug_orig_rot_info_AB_P_A2A.orig_rb_cnt);
                    out2("debug_orig_rrb_cnt", qri.debug_orig_rot_info_AB_P_A2A.orig_rrb_cnt);
#endif
                    out2("qri", qri);
                    if (idx > 0) {
                        out2("prev_qri", yst_queries[idx - 1]);
                    } else {
                        out2("prev_qri", "none");
                    }
                    out2("yst_cmd_build_query_window_begin");
                    const int window_begin = (idx > 5) ? idx - 5 : 0;
                    for (int window_idx = window_begin; window_idx <= idx; ++window_idx) {
                        out2("window_q_idx", window_idx);
                        out2("window_qri", yst_queries[window_idx]);
                    }
                    out2("yst_cmd_build_query_window_end");
                }
                my_assert(get_inserted_cnt() == qri.get_ins_turn());
            } else {
                // _BEGIN: A2AP 廃止に伴い A2AP の assert を削除。
                my_assert(query.get_push_type() == CommonPushType::A2AS);
                // _END
                command cmd1 = qri.get_cmd_rot_a1_A2A();
                int cnt1 = qri.get_cmd_rot_a1_cnt_A2A();
                int flip_dis_dir = qri.get_flip_dis_dir_A2A();
                if (flip_dis_dir == 0) {
                    // 責務: no-cmd A2A が builder の A2A 同期候補範囲をどう更新するかを記録する。
                    // 必要な理由: no-cmd A2A が直後 B2A の同期対象を空にしているか判定するため。
                    out2("yst_cmd_build_nocmd_a2a");
                    out2("q_idx", idx);
                    out2("cmd_size_before", static_cast<int>(build_st.cmds.size()));
                    out2("prev_seq_a2a_cur_before", build_st.prev_seq_a2a_cur);
                    out2("prev_seq_a2a_end_before", build_st.prev_seq_a2a_end);
                    out2("on_pb_before", build_st.on_pb);
                    out2("qri", qri);
                    if (idx > 0) {
                        out2("prev_qri", yst_queries[idx - 1]);
                    } else {
                        out2("prev_qri", "none");
                    }
                }
                // _BEGIN: A2AP 廃止で常に swap_rot のため get_a2a_type() を直接置換。
                builder.add_a2a(e_a2a_type::swap_rot, cmd1, cnt1, flip_dis_dir);
                // _END
                if (flip_dis_dir == 0) {
                    out2("yst_cmd_build_nocmd_a2a_after");
                    out2("q_idx", idx);
                    out2("cmd_size_after", static_cast<int>(build_st.cmds.size()));
                    out2("prev_seq_a2a_cur_after", build_st.prev_seq_a2a_cur);
                    out2("prev_seq_a2a_end_after", build_st.prev_seq_a2a_end);
                    out2("on_pb_after", build_st.on_pb);
                }
                my_assert(get_inserted_cnt() == qri.get_ins_turn());
            }
            idx++;
        }
        
        return build_st.cmds;
    }

    static my_vector<command> build_commands_from_final_rot_info(const QueriesState::FinalRotInfo &final_rot_info) {
        const auto [final_rot_cmd, final_rot_cnt] = final_rot_info;
        my_assert(final_rot_cnt >= 0);
        my_vector<command> cmds;
        if (final_rot_cnt == 0) {
            return cmds;
        }
        my_assert(final_rot_cmd == RA || final_rot_cmd == RRA);
        append_repeat_cmd(cmds, final_rot_cmd, final_rot_cnt);
        return cmds;
    }

    static void append_repeat_cmd(my_vector<command> &cmds, command cmd, int cnt) {
        my_assert(cnt >= 0);
        for (int i = 0; i < cnt; ++i) {
            cmds.push_back(cmd);
        }
    }

    // //prev_a2aの最適化はしていない
    // static int append_ab_rot_commands(const QueriesCalculator&qs_calc, int q_idx, build_command_state &build_st, const QueryCommonRI &qri){
    //     qri.validate_is_ab_type();
    //     command cmd_a = qri.get_a_cmd_AB();
    //     command cmd_b = qri.get_b_cmd_AB();
    //     int a_cnt, b_cnt;
    //     if(q_idx==0){
    //         a_cnt = qs_calc.get_orig_a_cmd_cnt_FIRST_AB();
    //         b_cnt = qs_calc.get_orig_b_cmd_cnt_FIRST_AB();
    //     }else{
    //         if(qri.is_a2b_type()){
    //             a_cnt = qs_calc.get_orig_a_cmd_cnt_A2B(q_idx);
    //             b_cnt = qs_calc.get_orig_b_cmd_cnt_A2B(q_idx);
    //         }else{
    //             my_assert(qri.is_b2a_type());
    //             a_cnt = qs_calc.get_orig_a_cmd_cnt_B2A(q_idx);
    //             b_cnt = qs_calc.get_orig_b_cmd_cnt_B2A(q_idx);
    //         }
    //     }
    //     my_assert(a_cnt >= 0 && b_cnt >= 0);
    //     if (cmd_a == RA && cmd_b == RB) {
    //         const int rr_cnt = std::min(a_cnt, b_cnt);
    //         append_repeat_cmd(cmds, RR, rr_cnt);
    //         append_repeat_cmd(cmds, RA, a_cnt - rr_cnt);
    //         append_repeat_cmd(cmds, RB, b_cnt - rr_cnt);
    //     } else if (cmd_a == RRA && cmd_b == RRB) {
    //         const int rrr_cnt = std::min(a_cnt, b_cnt);
    //         append_repeat_cmd(cmds, RRR, rrr_cnt);
    //         append_repeat_cmd(cmds, RRA, a_cnt - rrr_cnt);
    //         append_repeat_cmd(cmds, RRB, b_cnt - rrr_cnt);
    //     } else {
    //         my_assert((cmd_a == RA || cmd_a == RRA) && (cmd_b == RB || cmd_b == RRB));
    //         append_repeat_cmd(cmds, cmd_a, a_cnt);
    //         append_repeat_cmd(cmds, cmd_b, b_cnt);
    //     }
    //     return static_cast<int>(cmds.size());
    // }

    // static int append_a2b_commands(const QueriesCalculator& qs_calc, int q_idx, build_command_state &build_st, const QueryCommonRI &qri){
    //     qri.validate_is_a2b_type();
    //     int cnt = append_ab_rot_commands(qs_calc, q_idx, build_st, qri);
    //     cmds.push_back(PB);todo
    //     return cnt + 1;
    // }


    // static int append_b2a_commands(const QueriesCalculator&qs_calc, int q_idx, my_vector<command> &cmds, const QueryCommonRI &qri){
    //     qri.validate_is_b2a_type();
    //     int cnt = append_ab_rot_commands(qs_calc,q_idx,cmds, qri);
    //     cmds.push_back(PA);
    //     return cnt + 1;
    // }

    // //x
    // static int append_a2a_commands(my_vector<command> &cmds, const QueryCommonRI &qri) {
    //     qri.validate_is_a2a_type();
    //     const int base_size = static_cast<int>(cmds.size());
    //     append_repeat_cmd(cmds, qri.get_cmd_rot_a1_A2A(), qri.get_cmd_rot_a1_cnt_A2A());

    //     const QueryCommon &query = qri.get_query();
    //     const int flip_dis_dir = query.get_flip_dis_dir();
    //     const int abs_flip_dis = std::abs(flip_dis_dir);

    //     if (query.get_a2a_type() == e_a2a_type::push_rot) {
    //         cmds.push_back(PB);
    //         append_repeat_cmd(cmds, flip_dis_dir > 0 ? RA : RRA, abs_flip_dis);
    //         cmds.push_back(PA);
    //     } else {
    //         my_assert(query.get_a2a_type() == e_a2a_type::swap_rot);
    //         my_assert(flip_dis_dir != 0);
    //         if(flip_dis_dir > 0){
    //             cmds.push_back(SA);
    //             for (int i = 0; i < abs_flip_dis - 1; i++) {
    //                 cmds.push_back(RA);
    //                 cmds.push_back(SA);
    //             }
    //         }else{
    //             cmds.push_back(SA);
    //             for (int i = 0; i < abs_flip_dis - 1; i++) {
    //                 cmds.push_back(RRA);
    //                 cmds.push_back(SA);
    //             }
    //         }
    //     }
    //     return static_cast<int>(cmds.size()) - base_size;
    // }
};
