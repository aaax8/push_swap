//
//

#ifndef UNTITLED_EVALUATEBYPRECURE_H
#define UNTITLED_EVALUATEBYPRECURE_H

//#include "all_include.h"
//#include "State.h"
//#include "util.h"
//#include "Chank.h"
//#include "Lis.h"
//#include "SyncCommand.h"

// human_review_begin: 公開用コピー単体で include が解決できるようにローカル参照へ変更する。
#include "all_include.h"
#include "State.h"
#include "util.h"
// human_review_end
#include "Chank.h"
#include "Lis.h"
// human_review_begin: 公開用コピー単体で include が解決できるようにローカル参照へ変更する。
#include "Command/SyncCommand.h"
#include "Command/InitCommand.h"
#include "InvNumOfPS.h"
// human_review_end

template<size_t ARG_N>
class EvaluateByPrecure {
    State &st;
    ChankInfo<ARG_N> &chank;
    int target_chank_id;
    int chank_size;
public:
    EvaluateByPrecure(State &st, ChankInfo<ARG_N> &chank, int target_chank_id) : st(st), chank(chank),
                                                                                 target_chank_id(target_chank_id) {
        chank_size = chank.chank_size(target_chank_id);
    }

    int get_pos_b(int tar) {
        for (int i = 0; i < st.B.size(); i++) {
            if (st.B[i] == tar) {
                return i;
            }
            int j = st.B.size() - 1 - i;
            if (st.B[j] == tar) {
                return j;
            }
        }

        return -1;
    }

    int get_pos_a(int tar) {
        for (int i = 0; i < st.A.size(); i++) {
            if (st.A[i] == tar) {
                return i;
            }

            int j = st.A.size() - 1 - i;
            if (st.A[j] == tar) {
                return st.A.size() - 1 - i;
            }
        }
        return -1;
    }

    int set_tar_with_swap_lis(int tar, int &need_swap) {
        State first_st(st);
        if (st.A.back() == tar) {
            st.rra();
            my_assert(!need_swap);
            //tarが0の場合終わりなので矛盾は起きない
            return tar - 1;
        }
        my_assert(!st.B.empty());
        int pos_b = get_pos_b(tar);
        my_assert(pos_b != -1);
        int rb_dis = get_rb_dis(st, pos_b);
        int rrb_dis = get_rrb_dis(st, pos_b);
        if (rb_dis < rrb_dis) {
            for (int _ = 0; _ < rb_dis; _++) {
                if (!need_swap && st.B[0] == tar - 1) {
                    need_swap = true;
                    st.pa();
                }else if(st.A.back() < st.B[0]){
                    st.pa();
                    st.ra();
                }else {
                    st.rb();
                }
            }
            st.pa();
            if (need_swap) {
                st.sa();
                need_swap=false;
                return tar - 2;
            }
            return tar - 1;
        } else {
            for (int _ = 0; _ < rrb_dis; _++) {
                if (!need_swap && st.B[0] == tar - 1) {
                    need_swap = true;
                    st.pa();
                }
                while(!st.B.empty() && st.A.back() < st.B[0]){
                    st.pa();
                    //☆_ジュエルを追加している間に、一周してrrbでpaするはずだったものをpaすることがある
                    //A : 5 -----0
                    //B : 3, 1, 4, 2
                    //等の時に
                    //need_swap = true
                    //A : 3, 5 -----0, 1
                    //B : 4, 2
                    //という状態になることがある
                    if(st.A[0]==tar){
//                        my_assert(ystaaa_st.B.empty());
                        break;
                    }else{
                        st.ra();
                    }
                }
                //☆
                if(st.A[0]==tar){
                    break;
                }
                st.rrb();
            }
            //☆
            if(st.A[0]!=tar){
                my_assert( !st.B.empty()&&st.B[0]==tar);
                st.pa();
            }
            if (need_swap) {
                need_swap=false;
                st.sa();
                return tar - 2;
            }
            return tar - 1;
        }
    }

    //Aは円環上でソートされている
    int set_tar(int tar) {
        if (st.A.back() == tar) {
            st.rra();
            //tarが0の場合終わりなので矛盾は起きない
            return tar - 1;
        }
        int pos_b = get_pos_b(tar);
        my_assert(pos_b != -1);
        int rb_dis = get_rb_dis(st, pos_b);
        int rrb_dis = get_rrb_dis(st, pos_b);
        if (rb_dis < rrb_dis) {
            for (int _ = 0; _ < rb_dis; _++) {
                st.rb();
            }
            st.pa();
        } else {
            for (int _ = 0; _ < rrb_dis; _++) {
                st.rrb();
            }
            st.pa();
        }
        return tar - 1;
    }

    int next(int v) {
        if (v == ARG_N - 1) {
            return 0;
        } else {
            return v + 1;
        }
    }

    int push_target_max(State &ini_st, int target_min, int target_max) {
        ini_st.chk_all_exist();
        st.chk_all_exist();
        int a_pos = get_pos_a(next(target_max));
        int b_pos = get_pos_b(target_max);
//        out("call push_target_max");
//        out("target_min, max", target_min, target_max);
//        out("a, b, pos", a_pos, b_pos);
        push_a_with_rot(st, a_pos, b_pos);
        if (target_min == target_max) {
            return -1;
        } else {
            return target_max - 1;
        }
    }

    int max_b() {
        int max_v = -1;
        for (int i = 0; i < st.B.size(); i++) {
            chma(max_v, st.B[i]);
        }
        return max_v;
    }

    //
    //ini_a_topがa_top_ngになることは、チャンクがひとつだったらあり得る
    int virtual_sort_a(int ini_a_top, int a_top_ng) {
        //a_top_ngが-1になることは基本的にない
        //なる可能性があるのは最初のチャンクの時に
        //初期状態のa内で最小の数が、最初のチャンクの最大を上回る時だけで
        //チャンクを6つに分けている場合これはまず起きない
        //のちのちチャンクの数が変わったら問題が起きる可能性がある
        my_assert(a_top_ng != -1);
        st.chk_all_exist();
        auto on_lis_v = [&](int av) -> int {
            if (av == ini_a_top) {
                return ARG_N;
            } else {
                return av;
            }
        };
        auto on_real_v = [&](int lis_av) -> int {
            if (lis_av == ARG_N) {
                return ini_a_top;
            } else {
                return lis_av;
            }
        };
        int prev_rra_cnt = 0;
        while (st.A.back() != a_top_ng) {
            st.rra();
            prev_rra_cnt++;
        }
        InvNumOfPS<ARG_N + 1> inv_counter;
        vector<int> sorted_a;
        for (int i = 0; ; i++) {
            int lisv = on_lis_v(st.A[i]);
            inv_counter.add(lisv);
            sorted_a.push_back(lisv);
            if(st.A[i]==ini_a_top){
                break;
            }
        }
        int inv_cnt = inv_counter.get_inv();
        {
            std::sort(sorted_a.begin(), sorted_a.end());
            for (int i = 0; i < sorted_a.size(); i++) {
                st.A[i] = on_real_v(sorted_a[i]);
            }
            for (int _ = 0; _ < prev_rra_cnt; _++) {
                st.revert_last_cmd();
            }
        }
        return inv_cnt;
    }
//    //
    int virtual_sort_a(int ini_a_top, int ng_a1, int ng_a2) {
        st.chk_all_exist();
        if (ng_a1 == -1 || ng_a2 == -1) {
            my_assert(ng_a1 == ng_a2);
        }
//        out("call virtual sort_a");
//        out("ng_a1, ng_a2", ng_a1, ng_a2);
        auto on_lis_v = [&](int av) -> int {
            if (av == ini_a_top) {
                return ARG_N;
            } else {
                return av;
            }
        };
        auto on_real_v = [&](int lis_av) -> int {
            if (lis_av == ARG_N) {
                return ini_a_top;
            } else {
                return lis_av;
            }
        };
        int prev_rra_cnt = 0;
        while (st.A.back() != ng_a1) {
            st.rra();
            prev_rra_cnt++;
        }
        InvNumOfPS<ARG_N + 1> inv_counter;
        vector<int> sorted_a;
        for (int i = 0; st.A[i] != ng_a2; i++) {
            int lisv = on_lis_v(st.A[i]);
            inv_counter.add(lisv);
            sorted_a.push_back(lisv);
        }
        int inv_cnt = inv_counter.get_inv();
        {
            std::sort(sorted_a.begin(), sorted_a.end());
            for (int i = 0; i < sorted_a.size(); i++) {
                st.A[i] = on_real_v(sorted_a[i]);
            }
            for (int _ = 0; _ < prev_rra_cnt; _++) {
                st.revert_last_cmd();
            }
        }
        return inv_cnt;
    }
//

    //渡されるとき、左はソートされていることが保証されている
    //todo 返すときにコピーしたstを返しているので、revertしなくていい
    int evaluate_by_simple_precure(int ini_a_top, int a_top_ng, int ng_a2) {
        st.chk_all_exist();
        State ini_st(st);
        ini_st.chk_all_exist();
        auto ret_val = [&](int v) {
            ini_st.chk_all_exist();
            st = ini_st;

            st.chk_all_exist();
            return v;
        };
        //左のlisに含まれない物を全てpbする
        int hand = virtual_sort_a(ini_a_top, a_top_ng, ng_a2);
//        int hand = virtual_sort_a(ini_a_top, a_top_ng);
        ini_st.chk_all_exist();
        st.chk_all_exist();
        validate_ring_sortedA(st);
        if (st.B.size() == 0) {
            return ret_val(hand);
//            return 0;
        }
        int ini_turn = st.turn();
        int target_min = chank.chank_vals[target_chank_id][0];
        //ini_a_topはpbやsa, ssをされない
        int target_max = max_b();

        //Bからの挿入は終わってる
        if (target_max < target_min) {
            while(st.A[0] != target_min){
                my_assert(st.A[0]-1 == st.A.back());
                st.rra();
                hand++;
            }
            return ret_val(hand);
//            return 0;
        }
        //右がないときは終わっているという前提(変なところにpushしない前提)
        //終わってないのに、bトップがチャンクでない場合
//        if (chank.get_chank_id(ystaaa_st.B[0]) != target_chank_id) {
//            return inf_score;
//        }

//                chank.chank_vals[target_chank_id].back();
//        //todo 本番は呼ばない

        int tar = push_target_max(ini_st, target_min, target_max);
//        //a : tar + 1をaのトップに持ってくる
//        //b : tarをbのトップに持ってくr
        int need_swap = false;
        while (tar >= target_min) {
            ini_st.chk_all_exist();
            //swapあっても変わんない
            tar = set_tar_with_swap_lis(tar, need_swap);
            ini_st.chk_all_exist();
        }
//        ystaaa_st.print();
        int end_turn = st.turn();
        return ret_val(hand + end_turn - ini_turn);
    }
};


#endif //UNTITLED_EVALUATEBYPRECURE_H
