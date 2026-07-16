//
// Created by Auto
//

#ifndef UNTITLED_BEAMEVALUATOR_H
#define UNTITLED_BEAMEVALUATOR_H

#include "all_include.h"
#include "util.h"
#include "Command/InitCommand.h"
#include "StateHash.h"
#include "BeamState.h"
#include "BeamOperations.h"
#include "RotateCost.h"
#include "BeamStateUtil.h"
#include "BeamStateRunner.h"

inline pair<int, int> normalize_rotate_distance_pair(int forward_dis, int size) {
    my_assert(0 <= forward_dis && forward_dis <= size);
    my_assert(size >= 0);
    if (size <= 1) return {0, 0};
    if (forward_dis == 0 || forward_dis == size) {
        return {0, 0};
    }
    int reverse_dis = size - forward_dis;
    return {forward_dis, reverse_dis};
}

inline pair<int, int> calc_a_rotate_distance_ring(const RingBuffer500 &A, int v) {
    int sz = A.size();
    if (sz <= 1) {
        return {0, 0};
    }
    for (int j = 0; j < sz; j++) {
        int left = A[(j - 1 + sz) % sz];
        int right = A[j];
        if (is_sorted_of_ring(left, v, right)) {
            return normalize_rotate_distance_pair(j, sz);
        }
    }
    // フォールバック（見つからない場合は0挿入と同等に扱う）
    return {0, 0};
}

class BeamEvaluator {
private:
    // 粗い評価で残す候補数のデフォルト値
    static constexpr int ROUGH_EVAL_CANDIDATE_LIMIT = 4;

public:
    template<int MAX_N>
    static int evaluaet_finish(const BeamStateRunner &runner, const BeamState<MAX_N> &state) {
        const State &st = runner.get_state();
        int AN = st.A.size();
        int BN = st.B.size();
        int state_n = AN + BN;
        my_assert(state.sorted_a_count == state_n);
        my_assert(AN == state_n);
        my_assert(BN == 0);
        int a0 = st.A[0];
        return state.current_turn + min(a0, state_n - a0);
    }

    //garbageは2回見なくていいとしているが
    //見守る人は、2回見るという扱いにした方が良いのでは TODO
    template<int MAX_N>
    static int
    calc_watch_all_unsorted_pos(const BeamStateRunner &runner, const State &st, const BeamState<MAX_N> &state) {
        //上の数
        //上を二回見る
        //ゴミは一回見るだけで良い
        int AN = st.A.size();
        int up_cnt, down_cnt;
        if (state.min_a_pos == 0) {
            up_cnt = 0;
        } else {
            up_cnt = AN - state.min_a_pos;
        }
        down_cnt = AN - up_cnt;
        my_assert(down_cnt >= 0);
        //閉区間
        auto calc_watch2 = [&](int last_pos, int first_pos) -> tuple<int, int, int> {
            int max_serial_len = 1;
            int max_serial_lv = -1;//len==1のときは-1で返る
            int garbage_cnt = 0;
            int prev_serial_len = 0;
            int prev_v = -100;
            int all_cnt = last_pos - first_pos + 1;
            my_assert(1 <= all_cnt && all_cnt <= AN);
            for (int i = last_pos; i >= first_pos; i--) {
                int v = st.A[i];
                if (state.is_sorted_v[v]) {
                    if (v + 1 == prev_v) {
                        prev_v = v;
                        prev_serial_len++;
                        if (chma(max_serial_len, prev_serial_len)) {
                            max_serial_lv = v;
                        }
                    } else {
                        prev_v = v;
                        prev_serial_len = 1;
                    }
                } else {
                    garbage_cnt++;
                    prev_serial_len = 0;
                    prev_v = -100;
                }
            }
            int res = all_cnt * 2 - garbage_cnt;//2回見る時のベース
            res -= max_serial_len * 2;
            return {max_serial_lv, max_serial_len, res};
        };
        if (state.min_a_pos == 0) {
            auto [max_down_lv, max_down_len, watch_down2_turn] = calc_watch2(AN - 1, 0);
            int watch_all1_turn = AN;
            int res = min(watch_down2_turn, watch_all1_turn);
            return res;
        } else {
            //2回見る時のコスト(serialを逃す)
            auto [up_lv, up_len, watch_up2_turn] = calc_watch2(AN - 1, state.min_a_pos);
            auto [down_lv, down_len, watch_down2_turn] = calc_watch2(state.min_a_pos - 1, 0);
            int res_up2 = down_cnt + watch_up2_turn;
            int res_down2 = up_cnt + watch_down2_turn;
            int res = min(res_up2, res_down2);
            return res;
        }
    }

    int get_predict_b_dis_lim(int N) {
        my_assert(N == 100);
        if (N <= 100) {
            return 4;
        } else {
            return N / 20;
        }
    }

    //何周するか
    template<int MAX_N>
    static int
    calc_predict_b_rot_turn_impl(const BeamStateRunner &runner, const BeamState<MAX_N> &beam_st, int rot_block_cnt) {
        //端数回転+rot_block_cnt する
        //スタートが微妙な位置だったら+1
        const State &start_st = runner.get_state();
        my_assert(start_st.B.size() > 0);
        const SortedStateA &sorted_a = runner.get_sorted_state_a();
        my_assert(sorted_a.A.size() > 0);
        State now_st_b({}, start_st.B, start_st.current_N, start_st.initial_N);
        StackWithPos stack_b_pos(start_st.current_N, start_st.B);
        StackWithPos ini_stack_b_pos(start_st.current_N, start_st.B);
        int N = start_st.current_N;
        auto prev_v = [&](int v) {
            if (v == 0) {
                return N - 1;
            } else {
                return v - 1;
            }
        };

        my_bitset<MAX_N> exist_b;
        for (int i = 0; i < start_st.B.size(); i++) {
            exist_b[start_st.B[i]] = true;
        }
        my_bitset<MAX_N> ini_exist_b = exist_b;
        auto get_prev_bv = [&](int v) {
            int cur_v = prev_v(v);
            while (!exist_b[cur_v]) {
                cur_v = prev_v(cur_v);
            }
            return cur_v;
        };
        //bcnt

//        int dis_lim = get_predict_b_dis_lim(N);
        //dis_limを超える移動は後ろに渡す
        //後ろが無い場合は、dis_lim=infとする
        auto sim_rot1 = [&](int start_v, int dis_lim) -> pair<int, int> {
            my_assert(now_st_b.B.size());
            int now_v = start_v;
            int nex_v = -1;
            int dis_sum = 0;
            //最初のしゅう
            while (true) {
                int pos_b = stack_b_pos.position(now_v);
                my_assert(now_st_b.B[pos_b] == now_v);
                int rb_dis = pos_b;
                int rrb_dis;
                if (pos_b == 0) {
                    rrb_dis = 0;
                } else {
                    rrb_dis = now_st_b.B.size() - pos_b;
                }
                int dis = min(rb_dis, rrb_dis);
                if (dis > dis_lim) {
                    nex_v = get_prev_bv(now_v);
                    if (now_v < nex_v) {
                        now_v = nex_v;
                        break;
                    }
                    now_v = nex_v;
                    continue;
                }
                dis_sum += dis;
                if (rb_dis < rrb_dis) {
                    for (int _ = 0; _ < rb_dis; _++) {
                        stack_b_pos.rotate(now_st_b.B);
                        now_st_b.rb();
                    }
                } else {
                    for (int _ = 0; _ < rrb_dis; _++) {
                        stack_b_pos.rev_rotate(now_st_b.B);
                        now_st_b.rrb();
                    }
                }
                exist_b[now_st_b.B[0]] = false;
                my_assert(now_st_b.B[0] == now_v);
                stack_b_pos.push_out();
                now_st_b.pa();
                if (now_st_b.B.size() == 0) {
                    return {-1, dis_sum};
                    break;
                }
                nex_v = get_prev_bv(now_v);
                if (now_v < nex_v) {
                    now_v = nex_v;
                    break;
                }
                now_v = nex_v;
            }
            my_assert(nex_v == now_v);
            return {now_v, dis_sum};
        };
        int start_v = get_prev_bv(sorted_a.A[0]);
        if (rot_block_cnt == 1) {
            //とりあえずあまり + 1ターン固定
            if (pop_cnt(exist_b, start_v + 1, N) == 0) {
                //start_vから0までに全部含まれる
                auto [now_v1, dis_sum1] = sim_rot1(start_v, inf);
                my_assert(pop_cnt(exist_b, 0, N) == 0);
                return dis_sum1 + dis_sum1;
            } else {
                auto [now_v1, dis_sum1] = sim_rot1(start_v, N / 20);
                auto [now_v2, dis_sum2] = sim_rot1(now_v1, inf);
                my_assert(pop_cnt(exist_b, 0, N) == 0);
                return dis_sum1 + dis_sum2;
            }
        } else {
            my_assert(0);
            return inf;
        }
    }


    //Bを円環上の大きい順に見ていく
    //現状のaの一番上のsortedの値から 1,2,3回転するときの移動量
    template<int MAX_N>
    static int calc_predict_b_rot_turn(const BeamStateRunner &runner, const BeamState<MAX_N> &state) {
        const State &st = runner.get_state();
        if (st.B.size() == 0) {
            return 0;
        }
        //近い順に見る？
        //大きい順でなくてもいい
        //近くの中で、一番いい物を選ぶ
        //Bに残ってるもの以外は、Aはソートできているという前提での移動量でどうか

        //Aソート済みで、いままでのAの評価値で、その上での貪欲
        //その際のBの移動量をスコアとするのが良いか

        //わかりやすく、大きい順で、規定以上に遠かったらskip

        //何週か
        return calc_predict_b_rot_turn_impl(runner, state, 1);

    }


    static double get_a_dis_lim(int N) {
        if(N<=100){
            return 4.75;
        }else if(N<=500){
            return 4.75;
        }else{
            my_assert(false);
            return -1;
        }
    }

    template<int MAX_N>
    static double evaluate_rough(const BeamStateRunner &runner, const BeamState<MAX_N> &state, int lis_length) {
        //,int ins_all_b_cost, my_vector<int> &b_min, my_vector<int> &b_med, my_vector<int> &b_max) {
        const State &st = runner.get_state();
        int AN = st.A.size();
        int BN = st.B.size();
        int state_n = AN + BN;
//        int ins_a_cnt = state.sorted_a_count - lis_length;
//        my_assert(ins_a_cnt >= 0);
        int a_garbage_cnt = AN - state.sorted_a_count;
        my_assert(a_garbage_cnt >= 0);
        double a_dis_lim = get_a_dis_lim(st.current_N);

        double predict_all_ins_turn = state.current_turn + a_garbage_cnt * a_dis_lim + BN * 3.5;
//        int a_dis_sum = state.get_a_dis_sum();
//        double predict_all_ins_turn = state.current_turn + a_dis_sum + BN * 3.5;

        if (state.sorted_a_count == state_n) {
            int a0 = st.A[0];
            return predict_all_ins_turn + min(a0, state_n - a0);
        } else {
            int yet_sorted_cnt = a_garbage_cnt + BN;
            my_assert(yet_sorted_cnt == state_n - state.sorted_a_count);
            my_assert(0 < yet_sorted_cnt && yet_sorted_cnt < state_n);
            if (yet_sorted_cnt > 0) {
                //現状のBを動かすコスト
//                return predict_all_ins_turn + calc_watch_all_unsorted_pos(runner, st, state);
//                out("ins, bn",ins_all_b_cost, BN * 3.5);

//                double now_med = b_med[st.B.size()];
//                my_assert( 0 < now_med);
                //@fix 
//                double per = (double)(ins_all_b_cost - b_min[st.B.size()])  / (b_max[st.B.size()] - b_min[st.B.size()] );
//                per = std::clamp(per, 0.0, 1.0);
//                double score = (ins_all_b_cost / now_med) * 3.5*BN;
//                double lv = 3;
//                double rv = 3.5;
//                double add_v = lv + (rv - lv)*per;
                return state.current_turn + a_garbage_cnt * a_dis_lim + calc_watch_all_unsorted_pos(runner, st, state) + 3.5*BN ;
//                return state.current_turn + a_dis_sum + calc_watch_all_unsorted_pos(runner, st, state) + 3.5 * BN;
//                return state.current_turn + state.get_a_dis_sum() + calc_watch_all_unsorted_pos(runner, st, state) + 3.5*BN ;



                ;
            } else {
                my_assert(false);
                return -1;
            }
        }
    }

};

#endif //UNTITLED_BEAMEVALUATOR_H

