////
////
//
//#ifndef UNTITLED_BEAMALL_H
//#define UNTITLED_BEAMALL_H
//
//#include "../Legacy/lis_chank/PrecureSort.h"
//#include "../TimeKeeper.h"
//#include "../all_include.h"
//#include "../State.h"
//#include "../StateHash.h"
//#include "../util.h"
//#include "../Command/SyncCommand.h"
//#include "../Command/InitCommand.h"
//#include "../Command/optimize/OptimizeRangeSearch.h"
//#include "../Command/optimize/OptimizeMatch.h"
//#include "../Legacy/lis_chank/Lis.h"
//#include "SortMemo.h"
//#include "../StateAndSH.h"
//
//extern int inf_score;
//
////!
//template<size_t ARG_N>
//class SimplePrecureMem : public SortMemo{
//private:
//    int kotei_min;
//public:
//    //!
//    //todo 円環の上でlis的にできるようにする
//    void test_calc_lis_and_rev_lis(){
//        //aのlisの範囲が取れているか
//        {
//            State st({6, 10, 15, 20, 0, 2, 3}, {11, 1, 14, 17, 8, 9});
//            StateHash sh(st);
//            StateAndSH ssh(st, sh);
//            out("20, 17, RA, RB");
//            calc_lis_and_rev_lis(ssh, 20, 17, RA, RB);
//
//            out("20, 17, RA, RRB");
//            calc_lis_and_rev_lis(ssh, 20, 17, RA, RRB);
//
//            out("20, 17, RRA, RB");
//            calc_lis_and_rev_lis(ssh, 20, 17, RRA, RB);
//
//            out("20, 17, RRA, RRB");
//            calc_lis_and_rev_lis(ssh, 20, 17, RRA, RRB);
//        }
//        {
//            State st({20, 0, 2, 3, 6, 10, 15}, {11, 1, 14, 17, 8, 9});
//            StateHash sh(st);
//            StateAndSH ssh(st, sh);
//            //RRAが0のばあい
//            out("20, 17, RRA, RRB");
//            calc_lis_and_rev_lis(ssh, 20, 17, RRA, RRB);
//
//
//            //RRBが0の場合
//            out("15, 11, RRA, RRB");
//            calc_lis_and_rev_lis(ssh, 15, 11, RRA, RRB);
//        }
//    }
//
//    void test_push_val_lis(){
//        auto test=[&](State &st, int push_v){
//            StateHash sh(st);
//            StateAndSH ssh(st, sh);
//            out("bef - ystaaa_st");
//            st.print();
//            push_val(ssh, push_v);
//            out("aft - ystaaa_st");
//            st.print();
//            out(st.get_cmd_list().list);
//        };
//
//        //RA, RB, 0を超える
//        {
//            State st({24, 30, 0, 3, 5, 10, 15, 18, 21, 22, 23}, {2, 27, 11, 1, 4, 8, 17, 9, 27, 6});
//            test(st, 4);
//        }
//        //RA, RRB
//        {
//            State st({24, 30, 0, 3, 5, 12,13, 14, 15, 16, 18, 19, 20, 21, 22, 23}, {10, 99, 98, 97, 96, 95, 2, 27, 11, 1, 8, 17, 4, 2,9, 27, 6});
//            test(st, 4);
//        }
//
//        //RRA, RB
//        {
//            State st({10, 15, 18, 24, 30, 0, 2, 3, 5}, {11, 1, 8, 17, 4, 2,9, 27, 6, 10, 99, 98, 97, 96, 95, 2, 27});
//            test(st, 4);
//        }
//        //RRA, RRB
//        {
//            State st({30, 0, 2, 3, 6, 10, 15, 18, 24}, {10, 12, 11, 1, 14, 8, 17, 9, 21, 27, 6});
//            test(st, 17);
//        }
//
//
//    }
//
//    //挿入において、関係するalisを返す
//    //返すvectorをaとして
//    //bが挿入する値は
//    //a(0)以上 a.back()未満になる
//    my_vector<int> get_a_lis_part(StateAndSH& ssh, int want_a_top_v, const command& a_cmd_dir){
//        my_assert(get_pos(ssh.st.A, want_a_top_v) != -1);
//        my_vector<int> alis;
//        if(a_cmd_dir == RA){
//            alis.push_back(ssh.st.A.back());
//            for (int i = 0; ; i++){
//                alis.push_back(ssh.st.A[i]);
//                if(ssh.st.A[i]==want_a_top_v){
//                    break;
//                }
//            }
//        }else{
//            if(ssh.st.A[0]==want_a_top_v){
//                alis.push_back(ssh.st.A[0]);
//                alis.push_back(ssh.st.A.back());
//            }else{
//                alis.push_back(ssh.st.A[0]);
//                for (int i = ssh.st.A.size()-1; ; i--){
//                    alis.push_back(ssh.st.A[i]);
//                    if(ssh.st.A[i]==want_a_top_v){
//                        my_assert(i>0);
//                        alis.push_back(ssh.st.A[i - 1]);
//                        break;
//                    }
//                }
//            }
//            std::reverse(alis.begin(), alis.end());
//        }
//        return alis;
//    }
//
//    //b_push_vは入れない
//    my_vector<int> get_b_list(StateAndSH& ssh, int b_push_v, const command& b_cmd_dir){
//        my_assert(b_cmd_dir==RB || b_cmd_dir==RRB);
//        if(ssh.st.B[0] == b_push_v){
//            my_vector<int>res;
////            res.push_back(b_push_v);
//            return res;
//        }else if(b_cmd_dir == RB){
//            my_vector<int>res;
//            for (int i = 0; i < ssh.st.B.size(); i++){
//                if(ssh.st.B[i]==b_push_v){
//                    return res;
//                }
//                res.push_back(ssh.st.B[i]);
//            }
//            my_assert(0);
//        }else{
//            my_vector<int>res;
//            res.push_back(ssh.st.B[0]);
//            for (int i = ssh.st.B.size()-1 ; i >= 0; i--){
//                if(ssh.st.B[i]==b_push_v){
//                    return res;
//                }
//                res.push_back(ssh.st.B[i]);
//            }
//            my_assert(0);
//        }
//        my_assert(0);
//        return {};
//    }
//
//    //min_a, max_aの範囲でlisを求める
//
////円環上ではないただのLIS
////大きい順を先にしたい
//
//    //aのlis btm(現状の一番下)からgoal_topにかけて
//    //RRの場合、RA, RBと呼ぶ(方向が知りたいだけ)
//    my_vector<int> calc_lis_and_rev_lis(StateAndSH& ssh, int want_a_top_v, int b_push_v, const command& a_cmd_dir, const command& b_cmd_dir){
//        my_assert(get_pos(ssh.st.A, want_a_top_v) != -1 &&
//                    get_pos(ssh.st.B, b_push_v)!=-1);
//        my_vector<int> alis = get_a_lis_part(ssh, want_a_top_v, a_cmd_dir);
////        out("alis");
////        out(alis);
//        my_vector<int> b_range = get_b_list(ssh, b_push_v, b_cmd_dir);
////        out("b_range");
////        out(b_range);
//        //むずかしいので
//        my_assert(alis.size()>1);
//        my_vector<int> blis;
//        if(a_cmd_dir == RA){
//            blis = ring_lis(b_range, e_lis_q::e_lis, alis[0], alis.back());
//        }else{
//            blis = ring_lis(b_range, e_lis_q::e_lis_reverse, alis[0], alis.back());
//        }
////        out("blis");
////        out(blis);
//        return blis;
//    }
//
//    template<class List>
////Aの各値について、持っていきたい場所を求める
//    my_vector<int> get_want_pos(const List &A, int kotei_min) {
//        my_vector<tuple<int, int>> sorted;
//        for (int i = 0; i < A.size(); i++) {
//            sorted.emplace_back(A[i], i);
//        }
//        std::sort(sorted.begin(), sorted.end());
//        my_vector<int> want_pos(ARG_N, -1);
//        int idx = 0;
//        for (auto &[v, _]: sorted) {
//            want_pos[v] = idx++;
//        }
//        int kotei_pos = find(A.begin(), A.end(), kotei_min) - A.begin();
//        my_assert(kotei_pos != A.size());//kotei_minが見つからない場合
////        out("sorted");
////        out(sorted);
////        out("want_pos bef");
////        out(want_pos);
//        int geta = kotei_pos - want_pos[kotei_min];
//        for (int i = 0; i < ARG_N; i++) {
//            int prev_want_pos = want_pos[i];
//            if (want_pos[i] == -1) {
//                continue;
//            }
//            want_pos[i] += geta;
//            want_pos[i]%=A.size();
//            want_pos[i]+=A.size();
//            want_pos[i]%=A.size();
//            //assertのためにインデックスにアクセスできるか見てる
//            my_assert( 0<=want_pos[i] && want_pos[i]<A.size());
//        }
//
////        out("want_pos aft");
////        out(want_pos);
//        return want_pos;
//    }
//
//    template<class List>
//    my_vector<int> get_now_pos(const List &list) {
//        my_vector<int> pos(ARG_N, -1);
//        for (int i = 0; i < list.size(); i++) {
//            pos[list[i]] = i;
//        }
//        return pos;
//    }
//
//    //todo
//    //evprのコピー
//    int next(int v) {
//        if (v == ARG_N - 1) {
//            return 0;
//        } else {
//            return v + 1;
//        }
//    }
//
//    //!
//    //push_a_with_rotのコピペ
//    void push_a_with_lis(StateAndSH& ssh, int a_pos, int b_pos){
//        auto& st = ssh.st;
//        my_assert(0<=a_pos && a_pos < st.A.size());
//        my_assert(0<=b_pos && b_pos < st.B.size());
//        int ra_dis = get_ra_dis(st, a_pos);
//        int rra_dis = get_rra_dis(st, a_pos);
//        int rb_dis =  get_rb_dis(st, b_pos);
//        int rrb_dis = get_rrb_dis(st, b_pos);
//        int a_dis;
//        int b_dis;
//        command a_cmd;
//        command b_cmd;
//        int min_ab_rot_cost = 1<<30;
//        bool same_dir= true;
//        //RA, RRB
//        if(chmi(min_ab_rot_cost, calc_rot_cost(ra_dis, rrb_dis, false))){
//            a_dis = ra_dis;
//            a_cmd = RA;
//
//            b_dis = rrb_dis;
//            b_cmd = RRB;
//            same_dir = false;
//        }
//        if(chmi(min_ab_rot_cost, calc_rot_cost(rra_dis, rrb_dis, true))){
//            a_dis = rra_dis;
//            a_cmd = RRA;
//
//            b_dis = rrb_dis;
//            b_cmd = RRB;
//            same_dir = true;
//        }
////RA, RB
//        if(chmi(min_ab_rot_cost, calc_rot_cost(ra_dis, rb_dis, true))){
//            a_dis = ra_dis;
//            a_cmd = RA;
//
//            b_dis = rb_dis;
//            b_cmd = RB;
//            same_dir = true;
//        }
//        //RRA, RB
//        if(chmi(min_ab_rot_cost, calc_rot_cost(rra_dis, rb_dis, false))){
//            a_dis = rra_dis;
//            a_cmd = RRA;
//
//            b_dis = rb_dis;
//            b_cmd = RB;
//            same_dir = false;
//        }
//        auto b_lis = calc_lis_and_rev_lis(ssh, st.A[a_pos], st.B[b_pos], a_cmd, b_cmd);
////        out("first push");
////        ystaaa_st.print();
////        out("blis");
////        out("apos, bpos", a_pos, b_pos);
////        out(b_lis);
////        out(cmd1, cmd2);
//
////        my_vector<int> calc_lis_and_rev_lis(StateAndSH& ssh, int want_a_top_v, int b_push_v, const command& a_cmd_dir, const command& b_cmd_dir){
//
//        int blis_i = 0;
//        auto has_next_blis=[&](){
//            return blis_i < b_lis.size();
//        };
//        auto pa_lis=[&](){
//            blis_i++;
//            st.pa();
//        };
//        auto get_next_blis_v=[&](){
//            my_assert(has_next_blis());
//            return b_lis[blis_i];
//        };
//        int want_a_top = st.A[a_pos];
//        int want_b_push = st.B[b_pos];
//        auto can_rot_b=[&](){
//            my_assert(st.B.size());
//            if(has_next_blis()){
//                if(st.B[0] == get_next_blis_v()){
//                    return false;
//                }else{
//                    return true;
//                }
//            }else{
//                //まだ終わってないという前提
//                if(st.B[0] == want_b_push){
//                    return false;
//                }else{
//                    return true;
//                }
//            }
//        };
//        auto can_rot_a=[&](){
//            //aのbtmとaのトップの間に、次のb_lis_vが入るならrotateできない
//            //aのトップが
//            if(st.A[0]==want_a_top){
//                return false;
//            }else if(has_next_blis()){
//                int bv =get_next_blis_v();
//                if(ring_order(st.A.back(), bv, st.A[0])){
//                    return false;
//                }else{
//                    return true;
//                }
//            }else{
//                //次に挿入されるのpush_bv
//                my_assert(st.A[0] != want_a_top);
//                return true;
//            }
//        };
//
//        //!
//        if (a_cmd==RA && b_cmd==RB) {
//            auto sync_cmd_v = sync_cmd(a_cmd, b_cmd);
//            while(true){
//                bool can_a = can_rot_a();
//                bool can_b = can_rot_b();
//                if(can_a && can_b){
//                    st.do_cmd(sync_cmd_v);
//                }else if(can_a){
//                    st.do_cmd(a_cmd);
//                }else if(can_b){
//                    st.do_cmd(b_cmd);
//                }else{
//                    pa_lis();
//                    if(st.A[0]==want_b_push){
//                        break;
//                    }else if(can_rot_b()){
//                        st.do_cmd(sync_cmd_v);
//                    }else{
//                        st.ra();
//                    }
//                }
//            }
//            //!
//        } else  if (a_cmd==RRA && b_cmd==RRB) {
//            auto sync_cmd_v = sync_cmd(a_cmd, b_cmd);
//            while(true){
//                bool can_a = can_rot_a();
//                bool can_b = can_rot_b();
//                if(can_a && can_b){
//                    st.do_cmd(sync_cmd_v);
//                }else if(can_a){
//                    st.do_cmd(a_cmd);
//                }else if(can_b){
//                    st.do_cmd(b_cmd);
//                }else{
//                    pa_lis();
//                    if(st.A[0]==want_b_push){
//                        break;
//                    }/*else if(can_rot_b()){
//                        ystaaa_st.do_cmd(sync_cmd_v);
//                    }else{
//                        ystaaa_st.ra();
//                    }*/
//                }
//            }
//        }else{
//            while(true){
//                while(can_rot_a()){
//                    st.do_cmd(a_cmd);
//                }
//                while(can_rot_b()){
//                    st.do_cmd(b_cmd);
//                }
//                my_assert(!can_rot_a() && !can_rot_b());
//                pa_lis();
//                if(st.A[0]==want_b_push){
//                    break;
//                }
//            }
//        }
//    }
//
//    //todo
//    //evprのコピー改変
//    int push_val(StateAndSH &ssh, int val) {
//        ssh.st.chk_all_exist();
//        //valの次はpushされていることが保証されている
//        int a_pos = get_pos(ssh.st.A, next(val));
//        int b_pos = get_pos(ssh.st.B, val);
//        {
//            int prev_turn = ssh.st.turn();
//            push_a_with_lis(ssh, a_pos, b_pos);
//       //            push_a_with_rot(ssh.ystaaa_st, a_pos, b_pos);
//            for (int i = prev_turn; i < ssh.st.turn(); i++){
//                ssh.sh.do_cmd(ssh.st.get_cmd_list().list[i]);
//            }
//            validate_eq_st_sh(ssh.st, ssh.sh);
//        }
//        if(val==0){
//            return -1;
//        }else{
//            return val-1;
//        }
//    }
//
//    int not_exist_a_max(const State & st){
//        my_vector<int> ex(ARG_N);
//        for(int i=0;i<st.A.size();i++){
//            ex[st.A[i]]=1;
//        }
//        for(int v = ARG_N-1;v>=0;v--){
//            if(!ex[v]){
//                return v;
//            }
//        }
////        my_assert(0);
//        return -1;
//    }
//
//
//    //todo
//    //prからのコピー
//    int set_tar_with_swap_lis(StateAndSH& ssh, int tar, int &need_swap) {
////        State first_st(ystaaa_st);
//        if (ssh.st.A.back() == tar) {
//            ssh.rra();
//            my_assert(!need_swap);
//            //tarが0の場合終わりなので矛盾は起きない
//            return tar - 1;
//        }
//        my_assert(!ssh.st.B.empty());
//        int pos_b = get_pos(ssh.st.B, tar);
////        int pos_b = get_pos_b(tar);
//        my_assert(pos_b != -1);
//        int rb_dis = get_rb_dis(ssh.st, pos_b);
//        int rrb_dis = get_rrb_dis(ssh.st, pos_b);
//        if (rb_dis < rrb_dis) {
//            for (int _ = 0; _ < rb_dis; _++) {
//                if (!need_swap && ssh.st.B[0] == tar - 1) {
//                    need_swap = true;
//                    ssh.pa();
//                }else if(ssh.st.A.back() < ssh.st.B[0]){
//                    ssh.pa();
//                    ssh.ra();
//                }else {
//                    ssh.rb();
//                }
//            }
//            ssh.pa();
//            if (need_swap) {
//                ssh.sa();
//                need_swap=false;
//                return tar - 2;
//            }
//            return tar - 1;
//        } else {
//            for (int _ = 0; _ < rrb_dis; _++) {
//                if (!need_swap && ssh.st.B[0] == tar - 1) {
//                    need_swap = true;
//                    ssh.pa();
//                }
//                while(!ssh.st.B.empty() && ssh.st.A.back() < ssh.st.B[0]){
//                    ssh.pa();
//                    //☆_ジュエルを追加している間に、一周してrrbでpaするはずだったものをpaすることがある
//                    //A : 5 -----0
//                    //B : 3, 1, 4, 2
//                    //等の時に
//                    //need_swap = true
//                    //A : 3, 5 -----0, 1
//                    //B : 4, 2
//                    //という状態になることがある
//                    if(ssh.st.A[0]==tar){
////                        my_assert(ystaaa_st.B.empty());
//                        break;
//                    }else{
//                        ssh.ra();
//                    }
//                }
//                //☆
//                if(ssh.st.A[0]==tar){
//                    break;
//                }
//                ssh.rrb();
//            }
//            //☆
//            if(ssh.st.A[0]!=tar){
//                my_assert( !ssh.st.B.empty()&&ssh.st.B[0]==tar);
//                ssh.pa();
//            }
//            if (need_swap) {
//                need_swap=false;
//                ssh.sa();
//                return tar - 2;
//            }
//            return tar - 1;
//        }
//    }
//
//    //try_get_memo_stepの参照に渡すための物
//    int out_step_res;
//int cnt_eval_by;
//
//
//    //todo
//    //プリキュアからのコピー
//    int eval_by_simple_precure(StateAndSH& ssh, int first_push) {
//        cnt_eval_by++;
//        int tar = push_val(ssh, first_push);
//        if(try_get_memo_rem_step(ssh.sh, out_step_res)){
//            int res_step = ssh.st.turn() + out_step_res ;
//            memo_until_state(ssh, out_step_res);
//            return res_step;
//        }
//        int need_swap = false;
//        while (tar >= 0) {
//            tar = set_tar_with_swap_lis(ssh, tar, need_swap);
//            if(try_get_memo_rem_step(ssh.sh, out_step_res)){
//                int res_step = ssh.st.turn() + out_step_res ;
//                memo_until_state(ssh, out_step_res);
//                return res_step;
//            }
////            ini_st.chk_all_exist();
//        }
//        return ssh.st.turn();
//    }
//
//    int evaluate_by_lis_precure(StateAndSH& ssh){
//        validate_eq_st_sh(ssh.st, ssh.sh);
//        //簡単のために、一番下のゴミまでraする形にする
//        auto calc_lis=[&](){
////        set<val_swap_t> get_s_lis_past(const deque<int> &dq, int start, int max_swap) {
//            int start_pos = get_pos(ssh.st.A, kotei_min);
//            //todo
//            //本当はkotei_minよりも上にある小さい数もlisに入れたい
//            //kotei_minをふくむlisをさがすってかんじにしたいが...
//            //kotei_minの右についてどこかまで増加していって
//            //左についてどこかまで減少していく
//            //どこまでやるかでo n log nでできそう?
//            set<val_swap_t> lis_vs = get_s_lis_past(to_dq(ssh.st.A), start_pos, 0);//とりあえず0で
//            my_vector<int> is_lis(ARG_N);
//            for(auto&[v, s] : lis_vs){
//                is_lis[v]=true;
//            }
//            return is_lis;
//        };
//        auto is_lis=calc_lis();
//        int lis_cnt =sum(is_lis);
//        //todo rraもみるべき
//        while(ssh.st.A.size() > lis_cnt){
//            if(is_lis[ssh.st.A[0]]){
//                ssh.ra();
//            }else{
//                ssh.pb();
//            }
//            if(try_get_memo_rem_step(ssh.sh, out_step_res)){
//                int res_step = ssh.st.turn() + out_step_res ;
//                memo_until_state(ssh, out_step_res);
//                return res_step;
//            }
//        }
//        int first_push = not_exist_a_max(ssh.st);
//        if(first_push==-1){
//            //未実装？
//            my_assert(0);
//            return 0;
//        }
//        int step = eval_by_simple_precure(ssh, first_push);
//        validate_eq_st_sh(ssh.st, ssh.sh);
//        return step;
//    }
//
//    //親のevaluateから呼ばれる
//    //手数を返す
//    //答えが分かったら、memo_until_stateを呼ぶ
//    int evaluate_body(StateAndSH &ssh)override{
//
//        //ここに呼ばれている場合、メモはない
//        int ret=  evaluate_by_lis_precure(ssh);
//        //巻き戻しがないと言う事は、ゴールしていると言う事
//        if(ssh.st.turn() != called_turn){
//#ifdef DEBUG
//            my_assert(ssh.st.A.size()==ARG_N);
//            for (int i = 0; i < ARG_N; i++){
//                my_assert(ssh.st.A[i]==i) ;
//            }
//#endif
//            memo_until_state(ssh, 0);
//        }
//        return ret;
//    }
//
////    tuple<int, int,int> evaluate_by_precure(const State& __st){
//////        int diff_sort_score = evaluate_by_diff_pr(__st);
////        int diff_sort_score = 1;
////        int lis_push_score=evaluate_by_lis_precure(__st);
////        int score = diff_sort_score * lis_push_score;
////        return make_tuple(score,diff_sort_score, lis_push_score);
//////        return score;
////    }
//public:
//    SimplePrecureMem(int kotei_min) : kotei_min(kotei_min), out_step_res(-1){
//        cnt_eval_by = 0;
//    }
//};
//
//template<size_t ARG_N>
//class BeamAll {
//public:
//
////    int virtual_sort(State& st, int kotei){
////        deque<int> cpyA = to_dq(st.A);
////        auto want_pos = get_want_pos(cpyA, kotei);
////        my_vector<int> pos = get_now_pos(cpyA);
//////        out("pos");
//////        out(pos);
////        int cost=0;
////        for (int i = 0; i < st.A.size(); i++){
////            int diff1 = abs(want_pos[st.A[i]] - i);
////            int diff2 = st.A.size() - diff1;
//////            out("i, diff,  diff2", i,diff1, diff2);
////            int best_diff = min(diff1, diff2);
////            if(best_diff < 2){
////                cost += best_diff;
////            }else{
////                cost += best_diff+1;
////            }
////        }
//////        out("cost");
//////        out(cost);
////        for(auto&a : cpyA){
////            st.A[want_pos[a]] = a;
////        }
//////        st.print();
////        return cost;
////    }
//public:
//
//    BeamAll(){
//
//    }
//
////    int evaluate_by_diff_pr(const State& __st){
////        State st(__st);
////        int ini_turn = st.turn();
////        //!
////        //todo とりあえず0にしてる
////        int sort_a_cost = virtual_sort(st, kotei_min);
////        int first_push = not_exist_a_max(st);
////        if(first_push==-1){
////            return sort_a_cost;
////        }
////        eval_by_simple_precure(st, first_push);
//////        return st.turn() - ini_turn + sort_a_cost;
////        return st.turn() + sort_a_cost;
////    }
//
//    //!
//    void sort_by_beam(State& __st){
//        int kotei_min=0;//todo
//        SimplePrecureMem<ARG_N> algo(kotei_min);
//
//        //!
//        //todo 値変える
//        State &cur(__st);
//        cur.chk_all_exist();
//        //上位k個数
//        constexpr int haba = 50;
//        //評価, コマンド列
//        my_vector<tuple<int, my_vector<command>>> cur_q;
//        my_vector<tuple<int, my_vector<command>>> nex_q;
//        //curにたいして
//        auto can_do = [&](const command &cmd) {
//            if (!cur.can_do(cmd)) {
//                return false;
//            }
//            my_assert(cur.A.size());
//            if(cmd==PB && cur.A[0]==kotei_min){
//                return false;
//            }
//            return true;
//        };
//        StateHash cur_sh(cur);
//        auto revert_all = [&]() {
//            while (cur.turn()) {
//                int size = cur.turn();
//                cur_sh.do_cmd(revert_cmd[cur.get_cmd_list().list.back()]);
//                cur.revert_last_cmd();
//            }
//            my_assert(__st == cur);
//        };
//        auto do_cmd = [&](const command &cmd) {
//            cur.do_cmd(cmd);
//            cur_sh.do_cmd(cmd);
//        };
////        unordered_set<uint64_t> was_hash;
//        unordered_map<uint64_t, int> was_hash_turn;
////        auto make_inf_score=[&](){
////            return make_tuple(inf_score, -1,-1);
////        };
//        auto evaluate = [&](const my_vector<command> &cmds) {
//            cur.chk_all_exist();
//            for (int i = 0; i < cmds.size(); i++) {
//                auto &cmd = cmds[i];
//                if (!can_do(cmd)) {
//                    revert_all();
//                    return inf;
//                }
//                do_cmd(cmd);
//                cur.chk_all_exist();
//            }
//            cur.chk_all_exist();
//            int hash = cur_sh.hash();
//            if (was_hash_turn.find(hash) != was_hash_turn.end()) {
//                int& was_turn = was_hash_turn[hash];
//                if(was_turn <= cur.turn()){
//                    revert_all();
//                    return inf_score;
//                }else{
//                    was_turn = cur.turn();
//                }
//            }else{
////                was_hash.insert(cur_sh.hash());
//                was_hash_turn[hash] = cur.turn();
//            }
////            if (was_hash.find(cur_sh.hash()) != was_hash.end()) {
////                revert_all();
////                return inf_score;
////            }
//            cur.chk_all_exist();
//            int hand = algo.evaluate(cur, cur_sh);
////                    algo.evaluate_by_precure(cur);
//            cur.chk_all_exist();
//            if (hand == (inf_score)) {
//                revert_all();
//                return inf_score;
//            }
//            revert_all();
//            return hand;
//        };
//
////        State st(0,{1,3,4,5}, {6, 7,8, 2});
//        cur_q.emplace_back(evaluate(my_vector<command>{}), my_vector<command>{});
////        my_vector<command> allows = {
////                SA, SS, PB,
////                SB, RA, RB, RR, PA, RRA, RRB, RRR
////        };
//        my_vector<my_vector<command>> allows = {
//                {SA}, {SS}, {PB},
//                {SB}, {RA}, {RB}, {RR}, {PA}, {RRA}, {RRB}, {RRR}
//        };
//        //!
//        //todo 0にした
//        if(1){
////            allows.push_back({PA, SA});
////            allows.push_back({SB, PA});
//        for (int len = 1; len < 6; len++){
//            auto get_nth_cmd=[&](const command& cmd, int len){
//                return my_vector<command>(len, cmd);
//            };
//            auto add_nth_cmd=[&](const command& cmd, int len){
//                auto list = get_nth_cmd(cmd, len);
//                allows.push_back(list);
//            };
//            auto add_nth2_cmd=[&](const command& cmd1, const command& cmd2, int len){
//                auto list1 = get_nth_cmd(cmd1, len);
//                auto list2 = get_nth_cmd(cmd2, len);
//                list1.insert(list1.end(), list2.begin(), list2.end());
//                allows.push_back(list1);
//            };
//            auto add_nth_plus=[&](const command& cmd, int len, const command& cmd2){
//                auto list = get_nth_cmd(cmd, len);
//                list.push_back(cmd2);
//                allows.push_back(list);
//            };
//            add_nth2_cmd(RA, RB, len);
//
//            add_nth_cmd(RA, len);
//            add_nth_cmd(RB, len);
//            add_nth_cmd(RRA, len);
//            add_nth_cmd(RRB, len);
//            add_nth_cmd(RR, len);
//            add_nth_cmd(RRR, len);
//
//            add_nth_plus(RA, len, PA);
//            add_nth_plus(RB, len, PA);
//            add_nth_plus(RRA, len, PA);
//            add_nth_plus(RRB, len, PA);
//            add_nth_plus(RR, len, PA);
//            add_nth_plus(RRR, len, PA);
//
//            add_nth_plus(RA, len, PB);
//            add_nth_plus(RB, len, PB);
//            add_nth_plus(RRA, len, PB);
//            add_nth_plus(RRB, len, PB);
//            add_nth_plus(RR, len, PB);
//            add_nth_plus(RRR, len, PB);
//
//        }}
//        for (int turn = 1; turn < 1001; turn++) {
//            int cnt = 0;
//            int min_ev = inf_score;
//            my_vector<command> now_best_cmds;
//            int now_best_ev_diff = inf_score;
//            int now_best_ev_push = inf_score;
//            for (auto &[ev, past_cmds]: cur_q) {
////                out("-------------");
////                cout<<"ev : "<<ev<<endl;
////                {
////                    out(past_cmds);
////                    State st2(__st);
////                    for (auto &cmd: past_cmds) {
////                        st2.do_cmd(cmd);
////                    }
////                    st2.print();
////                }
//
//                if (ev == turn - 1) {
//                    out("cmd len", past_cmds.size());
//                    out(past_cmds);
//                    out("prev");
//                    __st.print();
//                    int cmd_i =0;
//                    for (auto &cmd: past_cmds) {
//                        __st.do_cmd(cmd);
//                        cmd_i++;
//                    }
//                    out("apply");
//                    __st.print();
//                    return;
//                }
//                cnt++;
//                if (cnt > haba) {
//                    break;
//                }
//                my_vector<command> new_cmds(past_cmds);
//                for (auto &nex_cmd_list: allows) {
////                    out("new_cmds bef", new_cmds);
//                    for(auto&nex_cmd : nex_cmd_list){
//                        new_cmds.push_back(nex_cmd);
//                    }
////                    out("new_cmds add", new_cmds);
//                    auto new_ev = evaluate(new_cmds);
////                    out("    child_ev", new_ev, nex_cmd_list);
//                    if (new_ev < (inf_score)) {
//                        nex_q.emplace_back(new_ev, new_cmds);
//                        if(chmi(min_ev, new_ev)){
//                            now_best_cmds = new_cmds ;
////                            now_best_ev_diff = ev1;
////                            now_best_ev_push = ev2;
//                        }
//                    }
//                    //
//                    for (int i = 0; i < nex_cmd_list.size(); i++){
//                        new_cmds.pop_back();
//                    }
////                    out("new_cmds aft", new_cmds);
//                }
//            }
//            out("----------------");
//            out("turn min_ev", turn, min_ev);
//            out("ev_f, ev_p", now_best_ev_diff, now_best_ev_push);
//            {
//                out("cmd:",now_best_cmds);
//                {
//                    State st2(__st);
//                    for (auto &cmd: now_best_cmds) {
//                        st2.do_cmd(cmd);
//                    }
//                    st2.print();
//                }
//            }
//            out("----------------");
//
//            swap(cur_q, nex_q);
//            nex_q.clear();
//            std::nth_element(cur_q.begin(), cur_q.begin() + haba, cur_q.end());
//        }
//
//    }
//};
//
//
//#endif //UNTITLED_BEAMALL_H
