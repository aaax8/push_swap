//
//
#include "Lis.h"

void test_lis_info() {
    s_lis_info_t<30> L;
    L.register_val(10, 2);
    L.register_val(20, 21);
    L.register_val(15, 41);
    //同じ値をregister出来ない
//    L.register_val(15, 41);
    L.print_state();

    L.swap(10);
    L.swap(15);
    L.print_state();
}

set<val_swap_t> get_s_lis_past(const deque<int> &dq, int start, int max_swap) {
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
        set<val_swap_t> liss;
//        s_lis_info_t<ARG_N> lis_info;
        //INSERTEDがswap回数と解釈されないように
        auto insert_to_lis = [&](val_swap_t &node) {
            if (node.second == INSERTED) {
//                lis_info.
                liss.insert(val_swap_t(node.first, 0));
            } else {
                liss.insert(node);
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
        return liss;
    };
//    pair<R, vector<int>> compress(V a) {
    auto [comp_dq, before_comp_val] = compress<deque<int>>(dq);
    set<val_swap_t> lis_on_comp = get_s_lis_on_perm(comp_dq, start, max_swap);
    set<val_swap_t> lis;
    for (auto &s: lis_on_comp) {
        lis.insert(val_swap_t(before_comp_val[s.first], s.second));
    }
    return lis;
}

void test_get_s_lis() {
    deque<int> dq = {
            3, 8, 12, 5, 4, 11, 27, 10, 98
    };
    for (int st = 0; st < dq.size(); st++) {
        out("ystaaa_st : ", st);
        s_lis_info_t<100> info = get_s_lis<100>(dq, st, 2);
        info.print_state();
        out("\n\n");
    }

}

//void calc_avg(int len, int max_swap){
//    deque<int> dq_100;
//    for (int i  = 0; i  < len; i ++)dq_100.push_back(i);
//    double yet_sorted_dis_to_0_sum = 0;
//    double sw_sum2=0;
//    int query_n=100;
//    vector<int> lens;
//    for (int q = 0; q < query_n; q++){
//        std::shuffle(dq_100.begin(), dq_100.end(), engine);
//        int max_len=0;
//        int best_sw_sum=0;
//        for (int i = 0; i < 100; i++){
//            auto s_lis = get_s_lis_past(dq_100, i, max_swap);
//            int sz = s_lis.sz();
//            lens.push_back(sz);
//            if(max_len< sz){
//                max_len = sz;
//                auto calc_sw_sum=[&](){
//                    int sw_sum = 0;
//                    for(auto&[_, swap_cnt] : s_lis){
//                        sw_sum += swap_cnt;
//                    }
//                    return sw_sum;
//                };
//                best_sw_sum = calc_sw_sum();
//            }
//        }
//        yet_sorted_dis_to_0_sum += max_len;
//        sw_sum2 += best_sw_sum;
//    }
//    std::sort(lens.begin(), lens.end());
////    out("len : ", len);
////    out("max_swap : ", max_swap);
////    out("avg_s-lis_len_ : ", yet_sorted_dis_to_0_sum / query_n);
////    out("med_s-lis_len_ : ", lens[lens.sz()/2]);
////    out("avg_sw_sum", sw_sum2 / query_n);
////    out("");
//}

//template<size_t ARG_N>
//todo delete

//todo
// pair<set<val_swap_t>, int> get_ring_s_lis_past(const deque<int>& dq, int max_swap){
//     int max_len=0;
//     int best_sw_sum=0;
//     int len = dq.sz();
//     set<val_swap_t> best_lis;
//     for (int i = 0; i < len; i++){
//         auto s_lis_past = get_s_lis_past(dq, i, max_swap);
// //        s_lis_info_t<ARG_N> s_lis = get_s_lis<ARG_N>(dq, i, max_swap);
//         int sz = s_lis_past.sz();
//         if(max_len< sz){
//             max_len = sz;
//             auto calc_sw_sum=[&](){
//                 int sw_sum = 0;
//                 for(auto&[_, swap_cnt] : s_lis_past){
//                     sw_sum += swap_cnt;
//                 }
//                 return sw_sum;
//             };
//             best_sw_sum = calc_sw_sum();
//             best_lis = s_lis_past;
//         }
//     }
//     return {best_lis, best_sw_sum};
// }
void test_get_rem_dq() {
    constexpr int N = 10;
    deque<int> dq(N);
    for (int i = 0; i < N; i++) {
        dq[i] = i;
    }
    s_lis_info_t<N> s;
    s.register_val(2, 2);
    s.register_val(5, 2);
    s.register_val(7, 1);
    s.register_val(3, 2);
    s.register_val(8, 1);
    out(dq);
    dq = get_rem_dq(dq, s);
    out(dq);
}

//昇順のlisと降順のlisを求める、二つの要素は相異なる
tuple<vector<int>, vector<int>> calc_incdec_lis(const vector<int> &_A) {
    vector<int> A(_A);
    auto rev_lis = LIS(A, e_lis_reverse);
    unordered_set<int> lis_set(rev_lis.begin(), rev_lis.end());
    vector<int> removed_A;
    for (auto &a: A) {
        if (lis_set.find(a) != lis_set.end()) {
            continue;
        }
        removed_A.push_back(a);
    }
    vector<int> lis = LIS(removed_A, e_lis);
    out("array");
    out(A);
    vector<int> lis_rev_vec(rev_lis.begin(), rev_lis.end());
    vector<int> lis_vec(lis.begin(), lis.end());
    out("lis_vec", lis_vec);
    out("lis_rev_vec", lis_rev_vec);

    return make_tuple(lis_vec, lis_rev_vec);
}

//円環上ではなく、普通のlis
//範囲はmin_v ~ max_v
vector<int> LIS(const vector<int> &v, e_lis_q lis_type, int min_v, int max_v) {
    my_assert(min_v < max_v);
    vector<int> dp;
    auto it = dp.begin();
    vector<int> positions;
    int mul = 1;
    if (lis_type == e_lis_reverse) {
        mul = -1;
    }
    for (const auto &_elem: v) {
        if (!(min_v < _elem && _elem < max_v)) {
            positions.push_back(-1);
            continue;
        }
        auto elem = _elem * mul;
        it = std::lower_bound(dp.begin(), dp.end(), elem);
        positions.push_back(static_cast<int>(it - dp.begin()));
        if (it == dp.end()) {
            dp.push_back(elem);
        } else {
            *it = elem;
        }
    }
    vector<int> lis_vals(dp.size());
    int si = static_cast<int>(dp.size()) - 1;
    int pi = static_cast<int>(positions.size()) - 1;
    while ((0 <= si) && (0 <= pi)) {
        if (positions[pi] == si) {
            lis_vals[si] = v[pi];
            --si;
        }
        --pi;
    }
    return lis_vals;
}

vector<int> LIS(const vector<int> &v, e_lis_q lis_type) {
    return LIS(v, lis_type, INT_MIN, INT_MAX);
}

//円環上でmin_vを下限、max_vを上限にする
//min_v = 80, max_v = 70などもゆるされる
//その場合、80, 0, 70というような順序になる(円環上だから)
vector<int> ring_lis(const vector<int> &A, e_lis_q lis_type, int min_v, int max_v) {
    bool over0 = min_v > max_v;
    int geta = 1e9;
    auto add_geta = [&](int v) {
        if (over0 && v < max_v) {
            return v + geta;
        } else {
            return v;
        }
    };
//    auto rem_geta=[&](int tar_v){
//        if(tar_v > geta){
//            return tar_v- geta;
//        }else{
//            return tar_v;
//        }
//    };
    vector<int> dp;
    auto it = dp.begin();
    vector<int> positions;
    int mul = 1;
    if (lis_type == e_lis_reverse) {
        mul = -1;
    }
    auto out_of_lis_range = [&](int v) {
        if (!over0) {
            return !(min_v < v && v < max_v);
        } else {
            return max_v < v && v < min_v;
        }
    };
    for (const auto &_elem: A) {
        if (out_of_lis_range(_elem)) {
            positions.push_back(-1);
            continue;
        }
        auto elem = add_geta(_elem) * mul;
        it = std::lower_bound(dp.begin(), dp.end(), elem);
        positions.push_back(static_cast<int>(it - dp.begin()));
        if (it == dp.end()) {
            dp.push_back(elem);
        } else {
            *it = elem;
        }
    }
    vector<int> lis_vals(dp.size());
    int si = static_cast<int>(dp.size()) - 1;
    int pi = static_cast<int>(positions.size()) - 1;
    while ((0 <= si) && (0 <= pi)) {
        if (positions[pi] == si) {
            lis_vals[si] = A[pi];
            --si;
        }
        --pi;
    }
    return lis_vals;
}

//a0をつかったlisを返す
//a0を使って、円環上でlisを求める
//a0=10, N=20なら
//10, 15, 18, 5, 8 とか
vector<int> get_lis_ring_use_a0(const deque<int> &A) {
    if (A.empty()) {
        return {};
    }

    int N = A.size();
    int start_val = A[0];

    vector<int> A_with_geta;
    A_with_geta.reserve(N);
    for (int v: A) {
        if (v < start_val) {
            A_with_geta.push_back(v + N);
        } else {
            A_with_geta.push_back(v);
        }
    }

    vector<int> lis_with_geta = LIS(A_with_geta, e_lis_q::e_lis);
    my_assert(lis_with_geta.size());
    my_assert(lis_with_geta[0] == start_val);
    deque<int> lis;
    for (int v: lis_with_geta) {
        if (v >= N) {
            lis.push_back(v - N);
        } else {
            lis.push_back(v);
        }
    }

    if (lis.empty()) {
        return vector<int>(lis.begin(), lis.end());
    }

    int min_i = std::min_element(lis.begin(), lis.end()) - lis.begin();
    for (int i = 0; i < min_i; i++) {
        lis.push_back(lis[0]);
        lis.pop_front();
    }
    my_assert(is_sorted(lis.begin(), lis.end()));
    return vector<int>(lis.begin(), lis.end());
}
////pair<pair<set<val_swap_t>, int>, pair<set<val_swap_t>, int>>  get_ring_s_lis2(const deque<int>& dq, int max_swap){
//template<size_t ARG_N>
//pair<s_lis_info_t<ARG_N>, s_lis_info_t<ARG_N>>  get_ring_s_lis2_past(const deque<int>& dq, int max_swap){
//    auto get_s_lis_nodes2=[&](set<val_swap_t>& s_lis_nodes1){
//        auto rem_dq = get_rem_dq(dq, s_lis_nodes1);
//        auto [remdq_comp, sorted_rem]  = compress<deque<int>>(rem_dq);
//        auto [s_lis_comp2, swap_sum2] = get_ring_s_lis_past(remdq_comp, max_swap);
////        out("rem_dq");
////        out(rem_dq);
////        out("rem_dq_comp");
////        out(remdq_comp);
////        out("sorted_rem");
////        out(sorted_rem);;
////        out("s_lis_comp2");
////        out(s_lis_comp2);
////        out("swap_sum2");
////        out(swap_sum2);
//        set<val_swap_t> s_lis2;
//        for(auto&[comp_v, swap_cnt] : s_lis_comp2){
//            s_lis2.insert(make_pair(sorted_rem[comp_v], swap_cnt));
//        }
////        out("s_lis2");out(s_lis2);
//        return make_pair(s_lis2, swap_sum2);
//    };
//    auto [s_lis_nodes1, swap_sum1] = get_ring_s_lis_past(dq, max_swap);
////    out("max_swap, ", max_swap);
////    out("dq");
////    out(dq);
////    out("s_lis_nodes1");
////    out(s_lis_nodes1);
////    out("swap_sum1");
////    out(swap_sum1);
//
//
//
//    auto [s_lis_nodes2, swap_sum2] = get_s_lis_nodes2(s_lis_nodes1);
//
//    return make_pair(make_pair(s_lis_nodes1, swap_sum1),
//                     make_pair(s_lis_nodes2, swap_sum2));
//}

//bool test_separate_lis12(State &ystaaa_st){
//    auto& A = ystaaa_st.A;
//    auto& B = ystaaa_st.B;
//    auto[slis1_info, slis2_info] = get_ring_s_lis2(ystaaa_st.A, 2);
//    auto &[slis1_nodes, slis1_swap_cnt] = slis1_info;
//    auto &[slis2_nodes, slis2_swap_cnt] = slis2_info;
////    out("slis1");
////    out(slis1_nodes);
////    out("slis2");
////    out(slis2_nodes);
//    auto slis1_nodes_cpy = slis1_nodes;
//    auto slis2_nodes_cpy = slis2_nodes;
//
//    auto is_lis=[&](auto& slis1_nodes, auto&slis2_nodes,  int tar_v){
//        if(exist_on_lis(slis1_nodes,  tar_v ) ||
//           exist_on_lis(slis2_nodes,  tar_v)){
//            return true;
//        }else{
//            return false;
//        }
//    };
//    auto pa=[&](){
//        ystaaa_st.pa();
//        out("pa");
//    };
//    auto pb=[&](){
//        ystaaa_st.pb();
//        out("pb");
//    };
//    auto ra=[&](){
//        ystaaa_st.ra();
//        out("ra");
//    };
//    auto rb=[&](){
//        ystaaa_st.rb();
//        out("rb");
//    };
//    auto ss=[&](){
//        ystaaa_st.ss();
//        out("ss");
//    };
//    auto sa=[&](){
//        ystaaa_st.sa();
//        out("sa");
//
//    };
//    int all_size = ystaaa_st.N;
//    for (int i  = 0; i  < all_size; i ++){
//        if(!is_lis(slis1_nodes, slis2_nodes, A[0])){
//            pb();
//        }else{
//            ra();
//        }
//    }
//    int lis1_len = slis1_nodes.sz();
//    int lis2_len = slis2_nodes.sz();
//
//    int lis12_len = A.sz();
////    ss();ss();ss();ss();ss();ss();ss();ss();
//    for (int i  = 0; i  < lis12_len; i ++){
//        if(exist_on_lis(slis1_nodes,  A[0])){
//            ra();
//        }else{
//            pb();
//        }
//    }
//
////    ss();ss();ss();ss();ss();ss();ss();ss();
//    //1をソートして、ソートできているか
////    out("slis1_nodes");
////    out(slis1_nodes_cpy);
////    out("sort1-start");
//
//    //Aのスタックにslisだけがある前提
//    auto sort_s_lis=[&](auto& slis_nodes, int swap_sum){
//        int was_swap_cnt=0;
//        int turn=0;
//        while (was_swap_cnt < swap_sum){
////            out("turn ", turn++);
//            ystaaa_st.print();
//            int val = A[0];
//            auto it = slis_nodes.lower_bound(make_pair(val, -(1<<30)));
//            int swap_cnt = it->second;
//            my_assert(it->first==val);
//            if(swap_cnt > 0){
//                sa();
//                was_swap_cnt++;
//                slis_nodes.erase(it);
//                slis_nodes.insert(make_pair(val, swap_cnt-1));
//            }
//            ra();
//        }
//        ystaaa_st.print();
////        out("sort-end");
//
//        //小さい値トップに持ってく
//        while(A[0] != slis_nodes.begin()->first){
//            ra();
//        }
//        for (int i = 0; i < A.sz()-1; i++){
//            if(A[i] > A[i+1]){
////                out("slis1");
////                out(slis1_nodes_cpy);
////                out("slis2");
////                out(slis2_nodes_cpy);
//                return false;
//            }
//        }
//        return true;
//    };
//
//
////    int was_swap_cnt1=0;
////    int turn=0;
////    while (was_swap_cnt1 < slis1_swap_cnt){
////        out("turn ", turn++);
////        ystaaa_st.print();
////        int val = A[0];
////        auto it = slis1_nodes.lower_bound(make_pair(val, -(1<<30)));
////        int swap_cnt = it->second;
////        my_assert(it->first==val);
////        if(swap_cnt > 0){
////            sa();
////            was_swap_cnt1++;
////            slis1_nodes.erase(it);
////            slis1_nodes.insert(make_pair(val, swap_cnt-1));
////        }
////        ra();
////    }
////    ystaaa_st.print();
////    out("sort1-end");
//    if(!sort_s_lis(slis1_nodes, slis1_swap_cnt)){
//        return false;
//    }
//    for (int i  = 0; i  < slis1_nodes.sz(); i ++){
//        pb();
//        rb();
//    }
//    for (int i  = 0; i  < slis2_nodes.sz(); i ++){
//        pa();
//    }
////    out("sort2 start");
//    if(!sort_s_lis(slis2_nodes, slis2_swap_cnt)){
//        return false;
//    }
////    out("sort2 end");
//    return true;
//}

////n個 s-lisを取るとあｇさながどうるか
//void calc_avg3(int len, int max_swap){
//    deque<int> dq_100;
//    for (int i  = 0; i  < len; i ++)dq_100.push_back(i);
//    double sum_len2 = 0;
//    double sum_len3 = 0;
//    double sw_sum2=0;
//    double sw_sum3=0;
//    int query_n=100;
//    vector<int> lens;
//    for (int q = 0; q < query_n; q++){
//        std::shuffle(dq_100.begin(), dq_100.end(), engine);
//        auto[slis1_info, slis2_info] = get_ring_s_lis2(dq_100, max_swap);
//        auto &[slis1_nodes, slis1_swap_cnt] = slis1_info;
//        auto &[slis2_nodes, slis2_swap_cnt] = slis2_info;
//
////        out("slis1_nodes");
////        out( slis1_nodes);
////
////        out("slis2_nodes");
////        out( slis2_nodes);
//
////        out("dq");
////        out(dq_100);
//        auto rem_dq = get_rem_dq(dq_100, slis1_nodes);
////        out("remove 1");
////        out(rem_dq);
//        rem_dq = get_rem_dq(rem_dq, slis2_nodes);
//
////        out("remove 2");
////        out(rem_dq);
//        auto [slis3_nodes, slis3_swap_cnt] = get_ring_s_lis_past(rem_dq, 2);
//        rem_dq = get_rem_dq(rem_dq, slis3_nodes);
////        out("slis3_nodes");
////        out( slis3_nodes);
//
////        out("remove 3");
////        out(rem_dq);
//
//        int lis_len_sum3 = (int)slis1_nodes.sz() + (int)slis2_nodes.sz() + (int)slis3_nodes.sz();
//
//
//
//        lens.push_back(lis_len_sum3);
//        sum_len2 += (int)slis1_nodes.sz() + (int)slis2_nodes.sz();
//        sum_len3 += lis_len_sum3;
//
//        sw_sum2 += slis1_swap_cnt +slis2_swap_cnt;
//        sw_sum3 += slis1_swap_cnt +slis2_swap_cnt + slis3_swap_cnt;
//    }
//    std::sort(lens.begin(), lens.end());
//    out("len : ", len);
//    out("max_swap : ", max_swap);
//    out("avg_s-lis_len2 : ", sum_len2 / query_n);
//    out("avg_s-lis_len3 : ", sum_len3 / query_n);
//    out("med_s-lis_len3 : ", lens[lens.sz()/2]);
//    out("avg_sw_sum", sw_sum3 / query_n);
//
//    out("");
//
//}
