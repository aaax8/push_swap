//
//

#ifndef UNTITLED_MERGEBSALE_H
#define UNTITLED_MERGEBSALE_H
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
// human_review_end
#include "EvaluateByPrecure.h"

const int MAX_CHANK_SZ = 18;

//extern array<array<pair<int,int>, 20 >, (1<<MAX_CHANK_SZ)> dp;
extern pair<int, int> g_dp[(1 << MAX_CHANK_SZ)][20];
extern int g_a_ord[1 << MAX_CHANK_SZ][500];


//next_a = (mask, chank_val) := chank_valの次の値
//逐次的に構築できる

//next_aを使えば、次何が欲しいか分かる
//maskがこれの時に、maskとaをふくめた者の中で何番目か(上から何番目というわけではなく、小さいのから何番目か)
//a_ord(mask, a) := aが何番目か

//aとしてみるのは、ソート時にトップだったものから～最大のチャンクの次の値まで
//todo maskのサイズ
//どこかで上限決めたい
//(paしたmask, 次にpushする値として) := aのトップに持ってくる必要がある値
extern int g_nex_a[1 << MAX_CHANK_SZ][MAX_CHANK_SZ];
extern int g_new_nex_a[1 << MAX_CHANK_SZ][MAX_CHANK_SZ];


template<size_t ARG_N>
class MergeBSale {
    const State& start;
    State& st;
    ChankInfo<ARG_N> &chank;
    int target_chank_id;
    int NO_PUSH_ID;
    int ini_a_top;
    vector<int>& sorted_chank_vals;
    vector<int> ini_ord_chanks;
    int chank_n;
    int mask_size;
    int last_push_n;

public:
    MergeBSale(State& st, ChankInfo<ARG_N> &chank, int target_chank_id) :
        start(st),
        st(st), chank(chank), target_chank_id(target_chank_id),
        sorted_chank_vals(chank.chank_vals[target_chank_id]),
        chank_n(sorted_chank_vals.size()),
        mask_size( 1<<(chank_n)),
        last_push_n(chank_n + 1),
        is_first_merge(target_chank_id == chank.chank_sizes.size()-1),
        is_last_merge(target_chank_id == 0)
    {
        ini_a_top = st.A[0];
        ini_ord_chanks = get_ini_ord_chanks();
        NO_PUSH_ID = sorted_chank_vals.size();//いちばん最初の状態（なにもpushしてないので特殊）
    }

    //stの対照のchankを上から順に取り出す
    vector<int> get_ini_ord_chanks() {
        vector<int> res;
        int chank_size = chank.chank_sizes[target_chank_id];
        my_assert(chank_size == chank.chank_vals[target_chank_id].size());
        for (int i = 0; i < st.B.size(); i++) {
            if (!chank.belong_chank(st.B[i]) || chank.get_chank_id(st.B[i]) != target_chank_id) {
                my_assert(i >= chank_size);
            } else {
                my_assert(i < chank_size);
                res.push_back(st.B[i]);
            }
        }
        auto s = res;
        std::sort(s.begin(),s.end());
        my_assert(s == sorted_chank_vals);
        return res;
    }

    //real_bより大きい最小のAにある値を返す
    //real_bが最大の場合も試す
    int find_next_a(State &st, int real_b) {
        int ini = 1 << 20;
        int min_lower_a = ini;
        auto dqA = to_dq(st.A);
        for (auto &a: dqA) {
            if (a > real_b) {
                if (min_lower_a > a) {
                    min_lower_a = a;
                }
            }
        }
        //自分より大きい物がない場合、最小の値が次
        if (min_lower_a == ini) {
            return *min_element(dqA.begin(), dqA.end());
        }
        return min_lower_a;
    }


    int  chank_val_by_id(vector<int>& ini_ord_chanks, int i){
        my_assert(i < ini_ord_chanks.size());
        return ini_ord_chanks[i];
    }

    //todo findでいいのでは
    int get_pos(const deque<int> &dq, int tar) {
        for (int i = 0; i < dq.size(); i++) {
            if (dq[i] == tar) {
                return i;
            }
        }
        my_assert(0);
        return -1;
    };


    pair<int, int> get_rotate_dis(const deque<int> &dq, int val) {
        int pos = get_pos(dq, val);
        int r_dis = pos;
        int rr_dis = dq.size() - pos;
        return make_pair(r_dis, rr_dis);
    }


    //comp_bをaにプッシュする
    void push_a(State &st, int real_b) {
        //todo 重い
        auto [rb_dis, rrb_dis] = get_rotate_dis(to_dq(st.B), real_b);
        //この値の上にpushする
        int target_a = find_next_a(st, real_b);
        auto [ra_dis, rra_dis] = get_rotate_dis(to_dq(st.A), target_a);
        push_a_with_rot(st, ra_dis, rb_dis);
    };


    State  reproduct_st(const State& start, int mask, int last_push,
                        int push_state_sz,
                        vector<int>& sorted_chank_vals,
                        vector<int>& ini_ord_chanks
    ){
        if(mask == 0 || last_push == NO_PUSH_ID){
            my_assert(mask == 0 && last_push == NO_PUSH_ID);
            State cur(start);
            return cur;
        }
        State cur(start);
        for (int i = 0; i < push_state_sz; i++){
            if(i == last_push){
                continue;
            }
            if(!((mask>>i)&1)){
                continue;
            }
            int target_b =  chank_val_by_id(ini_ord_chanks, i);
            push_a(cur, target_b);
        }
        //no_pushはここにこない
        my_assert((mask >> last_push) & 1);
        push_a(cur, chank_val_by_id(ini_ord_chanks, last_push));
        return cur;
    }

    //chank_idでi->jにいくコスト, 方向
    int a_dis, b_dis;
    command a_cmd, b_cmd;
    void calc_rot_b(int mask, int f_id, int to_id, int ra_dis, int rra_dis){
        if(f_id == NO_PUSH_ID){
            //todo のちのち、前回のraと組み合わせるような実装にすると問題が起きるかも
            f_id = 0;
        }
        //[l, r)のビット数を数える
        auto calc_bit_cnt=[&](int mask, int l, int r){
            return my_popcount((mask >> l) & ((1<<(r - l))-1)) ;
        };
        auto calc_rot_cost=[&](int dis_a, int dis_b, bool same_dir){
            if(same_dir){
                return max(dis_a, dis_b);
            }else{
                return dis_a + dis_b;
            }
        };
        int min_ab_rot_cost = 1 << 30 ;
        a_dis=-1;
        b_dis=-1;
        a_cmd=RA;
        b_cmd=RB;
        int rem_b_size =  st.B.size() - my_popcount(mask);
        int rb_dis, rrb_dis;
        if(f_id < to_id){
            rb_dis = to_id - f_id - calc_bit_cnt(mask, f_id, to_id);
            rrb_dis =rem_b_size - rb_dis;
        }else{
            rrb_dis = f_id - to_id -  calc_bit_cnt(mask, to_id, f_id);
            rb_dis = rem_b_size - rrb_dis;
        }
        //簡単に、0でも最後のチャンクでもない場合は、a, bともに貪欲にえらぶ
        //最大のチャンクの場合は、aを両方見る必要がある
        //最小のチャンクの場合は、bを両方見る必要がある
        if(is_first_merge){
            //a確定
            if(ra_dis < rra_dis){
                a_dis = ra_dis;
                a_cmd = RA;
                //RA, RB
                if(chmi(min_ab_rot_cost, calc_rot_cost(ra_dis, rb_dis, true))){
                    b_dis = rb_dis;
                    b_cmd = RB;
                }
                //RA, RRB
                if(chmi(min_ab_rot_cost, calc_rot_cost(ra_dis, rrb_dis, false))){
                    b_dis = rrb_dis;
                    b_cmd = RRB;
                }
            }else{
                a_dis = rra_dis;
                a_cmd = RRA;
                //RRA, RRB
                if(chmi(min_ab_rot_cost, calc_rot_cost(rra_dis, rrb_dis, true))){
                    b_dis = rrb_dis;
                    b_cmd = RRB;
                }
                //RRA, RB
                if(chmi(min_ab_rot_cost, calc_rot_cost(rra_dis, rb_dis, false))){
                    b_dis = rb_dis;
                    b_cmd = RB;
                }
            }
        }else if(is_last_merge){
            //RB確定
            if(rb_dis < rrb_dis){
                b_dis = rb_dis;
                b_cmd = RB;
                //RA, RB
                if(chmi(min_ab_rot_cost, calc_rot_cost(ra_dis, rb_dis, true))){
                    a_dis = ra_dis;
                    a_cmd = RA;
                }
                //RRA, RB
                if(chmi(min_ab_rot_cost, calc_rot_cost(rra_dis, rb_dis, false))){
                    a_dis = rra_dis;
                    a_cmd = RRA;
                }
            }else{
                //RRB確定
                b_dis = rrb_dis;
                b_cmd = RRB;
                //RRA, RRB
                if(chmi(min_ab_rot_cost, calc_rot_cost(rra_dis, rrb_dis, true))){
                    a_dis = rra_dis;
                    a_cmd = RRA;
                }
                //RA, RRB
                if(chmi(min_ab_rot_cost, calc_rot_cost(ra_dis, rrb_dis, false))){
                    a_dis = ra_dis;
                    a_cmd = RA;
                }
            }
        }else{
            if(ra_dis < rra_dis){
                a_dis = ra_dis;
                a_cmd = RA;
            }else{
                a_dis = rra_dis;
                a_cmd = RRA;
            }
            if(rb_dis < rrb_dis){
                b_dis = rb_dis;
                b_cmd = RB;
            }else{
                b_dis = rrb_dis;
                b_cmd = RRB;
            }
        }
    };

    static constexpr int inf = 1<<30;
    bool is_first_merge;
    bool is_last_merge;

    void validate_a_sorted(){
        for (int i = 0; i + 1 < st.A.size(); i++){
            my_assert(st.A[i] < st.A[i+1]);
        }
    }

    void set_dp_preparation(){
        auto& a_ord = g_a_ord;
        auto& nex_a = g_nex_a;
        auto& new_nex_a = g_new_nex_a;
        //ルールがあるので、これはクラスにするべきなのでは
        //aはいちばん上から、最小のチャンクの次まで
        vector<int> alist;//a_topを大きい値として扱う
        auto push_to_alist=[&](int v){
            if(v == ini_a_top && is_first_merge){
                my_assert(ini_a_top < st.A.back());
                alist.push_back(v + ARG_N);
            }else{
                alist.push_back(v);
            }
        };
        push_to_alist(st.A[0]);
        for (int i = st.A.size()-1; i >= 0; i--){
            //最小の次までなので、円環に由来した問題は起きない
            if(st.A[i] < sorted_chank_vals[0]){
                break;
            }else if(alist.back() < st.A[i]){
                //A : 17, 18, .... 98, 99, 5, 9, 15
                //などで、5の次の99を入れないようにするための処理
                break   ;
            }else{
                push_to_alist(st.A[i]);
            }
        }
        std::reverse(alist.begin(), alist.end());
        {
            auto sorted_alist = alist;
            std::sort(sorted_alist.begin(), sorted_alist.end());
            my_assert(alist == sorted_alist);
        }
        for (int mask = 0; mask < mask_size; mask++){
            //bitでいいのか
            vector<int> cur_a(alist.begin(),alist.end());
            for (int i = 0; i < chank_n; i++){
                if(bget(mask, i)){
                    //0が挿入されることがあるが、それはchankのidが0の時で、0がつぎのかずとしてつかわれることがないので
                    //ARG_Nにしなくて大丈夫
                    cur_a.push_back(chank_val_by_id(ini_ord_chanks, i));
                }
            }
            std::sort(cur_a.begin(), cur_a.end());
            for (int nex_id = 0; nex_id < chank_n; nex_id++){
                if(bget(mask, nex_id)){
                    nex_a[mask][nex_id] = -1;
                    continue;
                }
                auto next_it = std::lower_bound(cur_a.begin(), cur_a.end(), chank_val_by_id(ini_ord_chanks,nex_id));
                my_assert(next_it != cur_a.end());
                if(*next_it >= ARG_N){
                    nex_a[mask][nex_id] = (*next_it) - ARG_N;//ルールがある
                }else{
                    nex_a[mask][nex_id] = *next_it;
                }
            }
            for (int i = 0; i < cur_a.size(); i++){
                int av = cur_a[i];
                if(av>=ARG_N){//ルールがある
                    av -= ARG_N;
                }
                a_ord[mask][av] = i;
            }
        }
        for (int mask = 1; mask < mask_size; mask++){
            for (int nex_id = 0; nex_id < chank_n; nex_id++){
                if(bget(mask, nex_id)){
                    new_nex_a[mask][nex_id]=-1;
                    continue;
                }
                //aaa
            }
        }
    }

    void do_dp(){
        auto& a_ord = g_a_ord;
        auto& nex_a = g_nex_a;
        auto& new_nex_a = g_new_nex_a;
        auto& dp = g_dp;
        //dp初期か
        for (int mask = 0; mask < mask_size; mask++){
            for (int j = 0; j < last_push_n; j++){
                dp[mask][j] = make_pair(inf, NO_COMMAND);
            }
        }
        dp[0][NO_PUSH_ID] = make_pair(0, NO_COMMAND);
        //元のチャンクの順序に従って、各要素に番号を振る
        for (int mask = 0; mask < mask_size; mask++){
            for (int last_push = 0; last_push < last_push_n; last_push++){
//                out("mask, last",mask, last_push);
                if(dp[mask][last_push].first == inf){
                    continue;
                }
                if(last_push != NO_PUSH_ID && !bget(mask, last_push)){
                    continue;
                }
                for (int next_push = 0; next_push < last_push_n; next_push++){
                    if(next_push == NO_PUSH_ID){
                        continue;
                    }
                    if(bget(mask, next_push)){
                        continue;
                    }
                    //todo 一次元にする?
                    int need_a_top = nex_a[mask][next_push];
                    int cur_a_ord;
                    if(last_push == NO_PUSH_ID){
                        cur_a_ord = a_ord[mask][ini_a_top];
                    }else{
                        cur_a_ord = a_ord[mask][chank_val_by_id(ini_ord_chanks, last_push)] ;
                    }
                    int nex_a_ord = a_ord[mask][need_a_top];
                    int ra_dis, rra_dis;
                    int a_size = st.A.size() +  my_popcount(mask);
                    if(cur_a_ord < nex_a_ord){
                        ra_dis = nex_a_ord - cur_a_ord;
                        if(ra_dis==0){
                            rra_dis = 0;
                        }else{
                            rra_dis = a_size - ra_dis;
                        }
                    }else{
                        rra_dis = cur_a_ord - nex_a_ord;
                        if(rra_dis ==0){
                            ra_dis = 0;
                        }else{
                            ra_dis = a_size - rra_dis;
                        }
                    }
                    calc_rot_b(mask, last_push, next_push, ra_dis, rra_dis);
                    int new_cost = dp[mask][last_push].first;
                    int add_cost;
                    if((a_cmd == RA && b_cmd == RB) || (a_cmd ==RRA&&b_cmd==RRB)){
                        add_cost = max(a_dis, b_dis) + 1;
                    }else{
                        add_cost = a_dis + b_dis + 1;
                    }
                    new_cost += add_cost;
                    chmi(dp[mask | bit(next_push)][next_push], make_pair(new_cost, last_push));
                }
            }
        }
    }

    pair<int,command> rra_cnt_to_next_chank(State& cur){
        //todo to_dqおもい
        auto [ra_cnt, rra_cnt] = get_rotate_dis(to_dq(cur.A), sorted_chank_vals[0]);
        if(ra_cnt < rra_cnt){
            return make_pair(ra_cnt, RA);
        }else{
            return make_pair(rra_cnt, RRA);
        }
    };

    vector<int> restore_cmds_by_dp(){
        //左を最終手数、 last_push,
        //左をraしてそろえる時に、次のチャンクと一緒にrrにした方が良いこともあるけど、複雑なので辞めるか...
        //todo
        int min_last_push=-1, min_step=inf;
        //chank_valsの最小がtopになる状況？
        auto& dp = g_dp;
        for (int last_push = 0; last_push < last_push_n; last_push++){
            if(last_push == NO_PUSH_ID){
                continue;
            }
            State cur = reproduct_st(start, mask_size - 1, last_push, last_push_n,  sorted_chank_vals, ini_ord_chanks);
//            out("last " , last_push);
            auto [r_cnt, cmd] = rra_cnt_to_next_chank(cur);
            int cost = dp[mask_size-1][last_push].first + r_cnt;
            if(chmi(min_step, cost)){
                min_last_push = last_push;
            }
        }
        vector<int> push_history;
        int cur_mask = mask_size-1;
        int last_push = min_last_push;
        for (int _ = 0; _ < sorted_chank_vals.size(); _++){
            push_history.push_back(last_push);
            int last2_push = dp[cur_mask][last_push].second;
            cur_mask ^= bit(last_push);
            last_push = last2_push;
        }
        std::reverse(push_history.begin(), push_history.end());
        return push_history;
    }

    void apply_push_history(const vector<int> &push_history){
        int first_cmd_len = st.turn();
        for(auto&target_comp_b : push_history){
            push_a(st, chank_val_by_id(ini_ord_chanks, target_comp_b));
        }
        auto[r_cnt, cmd]  = rra_cnt_to_next_chank(st);
        for (int _ = 0; _ < r_cnt; _++){
            st.do_cmd(cmd);
        }
        int last_cmd_len = st.turn();
        out("target_chank_id", target_chank_id);
        out("cmd_len", last_cmd_len - first_cmd_len);
    }

    void merge_b_sale() {
        if(is_first_merge){
            validate_a_sorted();
        }
        set_dp_preparation();
        do_dp();
        apply_push_history(restore_cmds_by_dp());
    }

};


#endif //UNTITLED_MERGEBSALE_H
