//
//

#ifndef UNTITLED_LIS_H
#define UNTITLED_LIS_H

// human_review_begin: 公開用コピー単体で include が解決できるようにローカル参照へ変更する。
#include "all_include.h"
#include "util.h"
// human_review_end

template<size_t MAX_N>
class s_lis_info_t {
    array<bool, MAX_N> is_lis_;
    array<int, MAX_N> swap_cnt_;//必要なswap回数
    int swap_sum_;
    int lis_len_;
public:
    s_lis_info_t() : is_lis_({}), swap_cnt_({}), swap_sum_(0), lis_len_(0) {}

    //同じ値を登録できない
    void register_val(int val, int swap_cnt) {
        my_assert(0 <= val && val < MAX_N);
        my_assert(!is_lis_[val]);
        is_lis_[val] = true;
        swap_cnt_[val] = swap_cnt;
        swap_sum_ += swap_cnt;
        lis_len_++;
    }

    int lis_len() {
        return lis_len_;
    }

    int swap_sum() {
        return swap_sum_;
    }

    int is_lis(int v) {
        my_assert(0 <= v && v < MAX_N);
        return is_lis_[v];
    }

    int swap_cnt(int v) {
        my_assert(is_lis(v));
        return swap_cnt_[v];
    }

    //vをswapしたことを伝える
    void swap(int v) {
        my_assert(swap_cnt(v) > 0);
        swap_cnt_[v]--;
        swap_sum_--;
    }

    void print_state() {
        vector<int> lis;
        for (int v = 0; v < MAX_N; v++) {
            if (is_lis(v)) {
                lis.push_back(v);
            }
        }
//        out(lis);
//        for (int tar_v  = 0; tar_v  < MAX_N; tar_v ++){
//            if(is_lis(tar_v)){
//                out("tar_v : ",tar_v);
//                out("is_lis : ", is_lis(tar_v));
//                out("wap_cnt", swap_cnt(tar_v));
//            }
//        }
//        out("swap yet_sorted_dis_to_0_sum :", swap_sum());
        //    out("lis len :", lis_len());

    }
};

void test_lis_info();

using val_swap_t = pair<int, int>;


//void out_vs_tab(vector<vector<val_swap_t>> &tab, bool with_idx) {
//    for (int i = 0; i < tab.sz(); i++) {
//        if (with_idx) {
//            cout << i << ": ";
//        }
//        for (int j = 0; j < tab[i].sz(); j++) {
//            cout << tab[i][j] << ", ";
//        }
//        cout << endl;
//    }
//}

//0:59
//template<size_t ARG_N>
//set<val_swap_t>
//s_lis_info_t<ARG_N> get_s_lis_past(const deque<int>& dq, int start, int max_swap){
//todo_delete
set<val_swap_t> get_s_lis_past(const deque<int> &dq, int start, int max_swap);

template<size_t ARG_N>
s_lis_info_t<ARG_N> get_s_lis(const deque<int> &dq, int start, int max_swap) {
    auto get_s_lis_on_perm = [&](const deque<int> &dq, int start, int max_swap) {
        my_assert(is_permutation(dq, dq.size()));
        int N = dq.size();
        //    int inf = 1<<30;
        pair<int, int> ini_node{10000, 0};
        auto is_ini_node = [&](pair<int, int> &p) {
            return p == ini_node;
        };
        vector<val_swap_t> lis_vals(N + 4, val_swap_t(ini_node));
        //その値がそこへ送られるのに何回swapされたか
        //    out("dq");cout << dq << endl;
        //swapされたときの直前か、自分で老いた時の直前か

        //直前が普通の場合
        //  普通に持つ
        //直前がswapされた人の場合
        //  swapされた場合のその人を直前に持つとする
        //自分がswapした場合
        //  挿入した時の手前を直前で持つ
        //  割り込まれた人は、割り込まれた場合は自分を子に持つ

        int INSERTED = max_swap + 1;
        //ARG_N+2, swap回数 := 自分の前のノードの、値, それが何回swapされた時か
        vector<vector<val_swap_t>> prev_node(N + 2, vector<val_swap_t>(max_swap + 2, ini_node));
        for (int loop_i = 0; loop_i < N; loop_i++) {
            int dq_i = mod(start + loop_i, N);
            int dqv = dq[dq_i];
            auto lis_it = lower_bound(lis_vals.begin(), lis_vals.end(), val_swap_t(dqv, 0));
            int lis_i = lis_it - lis_vals.begin();
            //swapする場合と、そうでない場合を同時にこなせる...?
            //swapしてないときだけswapできる

            auto can_swap = [&]() {
                if (is_ini_node(lis_it[0])) {
                    return false;
                }
                int target_val = lis_vals[lis_i].first;
                int target_swap_cnt = lis_vals[lis_i].second;
                int target_prev_val = prev_node[target_val][target_swap_cnt].first;
                //自分よりも大きいことはすでに保証されている
                //後必要な事
                //  swap回数制限
                //  swap仕様としてる人の直前の数が自分未満
                return target_swap_cnt < max_swap &&
                       target_prev_val < dqv;;
            };
            //何回swapした時のprevかをかかないとおかしくなりそう
            //(i, j)iについて、iがj回swapしたときのprev(値, swap回数)

            //        auto past_dat0 = lis_it[0];
            auto target = lis_it[0];
            assert(target.second != INSERTED);
            if (can_swap()) {

                lis_it[1] = val_swap_t(lis_it[0].first, lis_it[0].second + 1);
                lis_it[0] = val_swap_t(dqv, 0);

                //横入りする人のprev
                prev_node[dqv][INSERTED] = prev_node[target.first][target.second];
                //横入りされた人のprevを設定
                prev_node[target.first][target.second + 1] = val_swap_t(dqv, INSERTED);

                //左に何もない時 prevはない
                //置く場所に数字がなかった時 prevは左の
                //置く場所に数字がある時    prevは前任者の
                if (lis_i == 0) {
                    prev_node[dqv][0] = ini_node;
                } else if (target == ini_node) {
                    prev_node[dqv][0] = lis_it[-1];
                } else {
                    prev_node[dqv][0] = lis_it[-1];
                    //                prev_node[dqv][0] = prev_node[past_dat0.first][past_dat0.second];
                }

            } else {
                lis_it[0] = val_swap_t(dqv, 0);

                //左に何もない時 prevはない
                //置く場所に数字がなかった時 prevは左の
                //置く場所に数字がある時    prevは前任者の
                if (lis_i == 0) {
                    prev_node[dqv][0] = ini_node;
                } else if (target == ini_node) {
                    prev_node[dqv][0] = lis_it[-1];
                } else {
                    prev_node[dqv][0] = lis_it[-1];
                    //                prev_node[dqv][0] = prev_node[past_dat0.first][past_dat0.second];
                }
            }
            //        out("loop_i, dq_i, lis_i, dq_v", loop_i, dq_i, lis_i, dqv);
            //        out("lis_vals");
            //        out(lis_vals);
            //        out("val_swap_tab");
            //        out_vs_tab(prev_node, true);;
            //        out("---------\n\n");
        }


        auto lis_it = lower_bound(lis_vals.begin(), lis_vals.end(), ini_node);
        val_swap_t now_node = *(lis_it - 1);
//        set<val_swap_t> liss;
        s_lis_info_t<ARG_N> lis_info;
        //INSERTEDがswap回数と解釈されないように
        auto insert_to_lis = [&](val_swap_t &node) {
            if (node.second == INSERTED) {
                lis_info.register_val(node.first, 0);
//                liss.insert(val_swap_t(node.first, 0));
            } else {
                lis_info.register_val(node.first, node.second);
//                liss.insert(node);
            }
        };
        insert_to_lis(now_node);
        //    int swap_sum = 0;
        while (true) {
            val_swap_t prev = prev_node[now_node.first][now_node.second];
            //        out("now", now_node);
            //        out("prev", prev);

            if (prev == ini_node)break;
            insert_to_lis(prev);
            now_node = prev;
        }
        //    out("liss");
        //    out(liss);
        //    return {liss, swap_sum};
        return lis_info;
    };
//    pair<R, vector<int>> compress(V a) {
    auto [comp_dq, before_comp_val] = compress<deque<int>>(dq);
    s_lis_info_t lis_on_comp = get_s_lis_on_perm(comp_dq, start, max_swap);
    s_lis_info_t<ARG_N> lis;
//    set<val_swap_t> lis;
    for (int v = 0; v < ARG_N; v++) {
        if (!lis_on_comp.is_lis(v))continue;
        lis.register_val(before_comp_val[v], lis_on_comp.swap_cnt(v));
//        lis.insert(val_swap_t(before_comp_val[s.first], s.second));
//        lis.register_val(before_comp_val[s.first], s.second);
    }
    return lis;
}


//set<val_swap_t> get_s_lis_past(const deque<int>& dq, int start, int max_swap){
void test_get_s_lis();

template<size_t ARG_N>
s_lis_info_t<ARG_N> get_ring_s_lis(const deque<int> &dq, int max_swap, int start_max_dis) {
    int len = dq.size();
    s_lis_info_t<ARG_N> best_lis;
    for (int i = 0; i < len; i++) {
        if (start_max_dis < i && i < dq.size() - start_max_dis)
            continue;
        s_lis_info_t<ARG_N> s_lis = get_s_lis<ARG_N>(dq, i, max_swap);
        if (best_lis.lis_len() < s_lis.lis_len()) {
            best_lis = s_lis;
        }
    }
    return best_lis;
}

// void test_get_ring_s_lis()
// {
//     deque<int> dq={
//             10, 98, 3, 8, 12, 5, 4, 11,27,
//     };
//     for (int ystaaa_st = 0; ystaaa_st < dq.sz(); ystaaa_st++){
//         out("ystaaa_st : ", ystaaa_st);
//         s_lis_info_t<100> info = get_s_lis<100>(dq, ystaaa_st, 2);
//         info.print_state();
//         out("\n\n");
//     }
//     out("\n\n\n\n\n\n\n\n");
//     auto ret = get_ring_s_lis<100>(dq, 2);
//     ret.print_state();
// }

//bool exist_on_lis(set<val_swap_t> & s_lis, int val){
//    auto it = s_lis.lower_bound(val_swap_t(val, -(1<<30)));
//    return (it!=s_lis.end() && it->first == val);
//}
//
//deque<int> get_rem_dq(const deque<int>& dq, set<val_swap_t>& s_lis1){
//    deque<int> rem_dq;
//    for(auto& dq_v : dq){
//        if(exist_on_lis(s_lis1, dq_v)){
//            continue;
//        }
//        rem_dq.push_back(dq_v);
//    }
//    return rem_dq;
//};

template<size_t ARG_N>
deque<int> get_rem_dq(const deque<int> &dq, s_lis_info_t<ARG_N> &s_lis) {
    deque<int> rem_dq;
    for (auto &dq_v: dq) {
        if (s_lis.is_lis(dq_v)) {
            continue;
        }
        rem_dq.push_back(dq_v);
    }
    return rem_dq;
};


void test_get_rem_dq();


//pair<pair<set<val_swap_t>, int>, pair<set<val_swap_t>, int>>  get_ring_s_lis2(const deque<int>& dq, int max_swap){
template<size_t ARG_N>
pair<s_lis_info_t<ARG_N>, s_lis_info_t<ARG_N>>
get_ring_s_lis2(const deque<int> &dq, int max_swap0, int max_swap1, int start_max_dis) {
    auto get_s_lis_nodes2 = [&](s_lis_info_t<ARG_N> &s_lis1) {
        auto rem_dq = get_rem_dq<ARG_N>(dq, s_lis1);
        auto [remdq_comp, sorted_rem] = compress<deque<int>>(rem_dq);
        //auto [s_lis_comp2, swap_sum2] =
        s_lis_info_t<ARG_N> s_lis_comp2 = get_ring_s_lis<ARG_N>(remdq_comp, max_swap1, start_max_dis);//todo
//        out("rem_dq");
//        out(rem_dq);
//        out("rem_dq_comp");
//        out(remdq_comp);
//        out("sorted_rem");
//        out(sorted_rem);;
//        out("s_lis_comp2");
//        out(s_lis_comp2);
//        out("swap_sum2");
//        out(swap_sum2);
        s_lis_info_t<ARG_N> s_lis2;
//        set<val_swap_t> s_lis2;
        //for(auto&[comp_v, swap_cnt] : s_lis_comp2){
        for (int i = 0; i < ARG_N; i++) {
            if (!s_lis_comp2.is_lis(i))
                continue;
            s_lis2.register_val(sorted_rem[i], s_lis_comp2.swap_cnt(i));
//            s_lis2.insert(make_pair(sorted_rem[comp_v], swap_cnt));
        }
//        out("s_lis2");out(s_lis2);
        return s_lis2;
    };
    s_lis_info_t<ARG_N> s_lis1 = get_ring_s_lis<ARG_N>(dq, max_swap0, start_max_dis);
//    out("max_swap, ", max_swap);
//    out("dq");
//    out(dq);
//    out("s_lis_nodes1");
//    out(s_lis_nodes1);
//    out("swap_sum1");
//    out(swap_sum1);
    s_lis_info_t<ARG_N> s_lis2 = get_s_lis_nodes2(s_lis1);
    return make_pair(s_lis1, s_lis2);
}

enum e_lis_q{
    e_lis_reverse,
    e_lis
};

//円環上ではないただのLIS
//大きい順を先にしたい

vector<int> LIS(const vector<int> &v, e_lis_q lis_type, int min_v, int max_v);

vector<int> LIS(const vector<int> &v, e_lis_q lis_type);

vector<int> ring_lis(const vector<int> &A, e_lis_q lis_type, int min_v, int max_v);

vector<int> get_lis_ring_use_a0(const deque<int> &A);

tuple<vector<int>,vector<int>> calc_incdec_lis(const vector<int> &_A);

//pair<s_lis_info_t<ARG_N>, s_lis_info_t<ARG_N>>  get_ring_s_lis2(const deque<int>& dq, int max_swap){
// void test_get_ring_s_lis2()
// {
//     deque<int> dq={10,11, 8, 7, 0, 1, 2, 4, 6, 5, 3, 9, 12};
//     auto[l1, l2]= get_ring_s_lis2<13>(dq, 2);
//     out(dq);
//     out("l1");
//     l1.print_state();
//     out("l2");
//     l2.print_state();
// }

//void calc_avg2(int len, int max_swap){
//    deque<int> dq_100;
//    for (int i  = 0; i  < len; i ++)dq_100.push_back(i);
//    double yet_sorted_dis_to_0_sum = 0;
//    double sw_sum2=0;
//    int query_n=100;
//    vector<int> lens;
//    int max_len=-1;
//    int best_sw_sum=-1;
//    for (int q = 0; q < query_n; q++){
//        std::shuffle(dq_100.begin(), dq_100.end(), engine);
//        auto[slis1_info, slis2_info] = get_ring_s_lis2(dq_100, max_swap);
//        auto &[slis1_nodes, slis1_swap_cnt] = slis1_info;
//        auto &[slis2_nodes, slis2_swap_cnt] = slis2_info;
//        int lis_len12 = (int)slis1_nodes.sz() + (int)slis2_nodes.sz();
//        lens.push_back(lis_len12);
//        if(max_len< lis_len12){
//            max_len = lis_len12;
//            best_sw_sum = slis1_swap_cnt + slis2_swap_cnt;
//        }
//        yet_sorted_dis_to_0_sum += max_len;
//        sw_sum2 += best_sw_sum;
//    }
//    std::sort(lens.begin(), lens.end());
//    out("len : ", len);
//    out("max_swap : ", max_swap);
//    out("avg_s-lis_len_ : ", yet_sorted_dis_to_0_sum / query_n);
//    out("med_s-lis_len_ : ", lens[lens.sz()/2]);
//    out("avg_sw_sum", sw_sum2 / query_n);
//
//    out("");
//
//}
#endif //UNTITLED_LIS_H
