//
//

#include "State.h"
#include "util.h"
void test_state(){
    auto chk_eq=[&](){
        State a({1,2,3,4, 5,6}, {});
        State b({1,2,3,4, 5,6}, {});
        b.sa();
        b.sa();
        my_assert(a!=b);
        State ini_b = get_start_state(b);
        my_assert(a==ini_b);
    };
    auto chk_set_equiv=[&](){
        State a({1,2,3,4, 5,6}, {});
        a.ra();
        a.pb();
        a.rra();
        a.set_equiv_cmds({SA, PB});//内部でassert
    };
    chk_eq();
    chk_set_equiv();
    // out("ok"); // why: output.txt出力抑制のため
}

void test_add_cmd() {
    State st({10, 9, 8, 7, 6, 5}, {});
    st.pb();
    // out("pb"); // why: output.txt出力抑制のため
    st.print();

    st.pb();
    // out("pb"); // why: output.txt出力抑制のため
    st.print();

    st.pb();
    // out("pb"); // why: output.txt出力抑制のため
    st.print();

    st.pa();
    // out("pa"); // why: output.txt出力抑制のため
    st.print();


    st.ra();
    // out("ra"); // why: output.txt出力抑制のため
    st.print();

    st.rb();
    // out("rb"); // why: output.txt出力抑制のため
    st.print();

    st.rr();
    // out("rr"); // why: output.txt出力抑制のため
    st.print();


    st.rra();
    // out("rra"); // why: output.txt出力抑制のため
    st.print();

    st.rrb();
    // out("rrb"); // why: output.txt出力抑制のため
    st.print();

    st.rrr();
    // out("rrr"); // why: output.txt出力抑制のため
    st.print();

    st.sa();
    // out("sa"); // why: output.txt出力抑制のため
    st.print();

    st.sb();
    // out("sb"); // why: output.txt出力抑制のため
    st.print();

    st.ss();
    // out("ss"); // why: output.txt出力抑制のため
    st.print();

    st.print();
}



ostream &operator<<(ostream &os, State &st) {
    // out("turn : ", st.turn()); // why: output.txt出力抑制のため
    st.A.print();
    st.B.print();
//    cout << "A : " << ystaaa_st.A << endl;
//    cout << "B : " << ystaaa_st.B << endl << endl;
    return os;
}

State init_state(int argc, char *argv[]) {
    vector<int> arg = arg_to_vec(argc, argv);
    auto [comp, _] = compress<deque<int>>(arg);
    return State( comp, deque<int>());
}


//stに対して、cmd_listのl~rを適用させるという使いにくい関数
//置き換えていきたい
State get_apply_cmds(const State& st, const cmd_list_t& cmd_list, int cmd_l, int cmd_r){
    my_assert(0 <= cmd_l && cmd_l <= cmd_r && cmd_r <= cmd_list.list.size());
    State ret(st);
    for (int i = cmd_l; i < cmd_r; i++){
        ret.do_cmd(cmd_list.list[i]);
    }
    return ret;
}

int get_ra_dis(State& st, int a_pos){
    my_assert( 0<=a_pos && a_pos<st.A.size());
    return a_pos;
}

int get_rra_dis(State& st, int a_pos){
    my_assert( 0<=a_pos && a_pos<st.A.size());
    if(a_pos == 0){
        return 0;
    }else{
        return st.A.size() - a_pos;
    }
}

int get_rb_dis(State& st, int b_pos){
    my_assert( 0<=b_pos && b_pos<st.B.size());
    return b_pos;
}

int get_rrb_dis(State& st, int b_pos){
    my_assert( 0<=b_pos && b_pos<st.B.size());
    if(b_pos==0){
        return 0;
    }else{
        return st.B.size() - b_pos;
    }
}

int calc_rot_cost(int dis_a, int dis_b, bool same_dir) {
    if (same_dir) {
        return max(dis_a, dis_b);
    } else {
        return dis_a + dis_b;
    }
};

void push_a_with_rot(State& st, int a_pos, int b_pos){
    my_assert(0<=a_pos && a_pos < st.A.size());
    my_assert(0<=b_pos && b_pos < st.B.size());
    int ra_dis = get_ra_dis(st, a_pos);
    int rra_dis = get_rra_dis(st, a_pos);
    int rb_dis =  get_rb_dis(st, b_pos);
    int rrb_dis = get_rrb_dis(st, b_pos);
    int a_dis;
    int b_dis;
    command a_cmd;
    command b_cmd;
    int min_ab_rot_cost = 1<<30;
    bool same_dir= true;
    //RA, RRB
    if(chmi(min_ab_rot_cost, calc_rot_cost(ra_dis, rrb_dis, false))){
        a_dis = ra_dis;
        a_cmd = RA;

        b_dis = rrb_dis;
        b_cmd = RRB;
        same_dir = false;
    }
    if(chmi(min_ab_rot_cost, calc_rot_cost(rra_dis, rrb_dis, true))){
        a_dis = rra_dis;
        a_cmd = RRA;

        b_dis = rrb_dis;
        b_cmd = RRB;
        same_dir = true;
    }
//RA, RB
    if(chmi(min_ab_rot_cost, calc_rot_cost(ra_dis, rb_dis, true))){
        a_dis = ra_dis;
        a_cmd = RA;

        b_dis = rb_dis;
        b_cmd = RB;
        same_dir = true;
    }
    //RRA, RB
    if(chmi(min_ab_rot_cost, calc_rot_cost(rra_dis, rb_dis, false))){
        a_dis = rra_dis;
        a_cmd = RRA;

        b_dis = rb_dis;
        b_cmd = RB;
        same_dir = false;
    }
    if (same_dir) {
        int sync_len = min(a_dis, b_dis);
        int rem_len;
        command rem_cmd;
        if (a_dis > b_dis) {
            rem_len = a_dis - b_dis;
            rem_cmd = a_cmd;
        } else {
            rem_len = b_dis - a_dis;
            rem_cmd = b_cmd;
        };
        auto sync_cmd_v = sync_cmd(a_cmd, b_cmd);
        for (int _ = 0; _ < sync_len; _++) {
            st.do_cmd(sync_cmd_v);
        }
        for (int _ = 0; _ < rem_len; _++) {
            st.do_cmd(rem_cmd);
        }
        st.do_cmd(PA);
    } else {
        for (int _ = 0; _ < a_dis; _++) {
            st.do_cmd(a_cmd);
        }
        for (int _ = 0; _ < b_dis; _++) {
            st.do_cmd(b_cmd);
        }
        st.do_cmd(PA);
    }
}

//sからcmdsを適用していった結果を出力
void visualize_cmds(const State& s, const vector<command>& cmds){
    State cur(s);
    vector<command> c(cmds);
    c.push_back(NO_COMMAND);
    std::reverse(c.begin(), c.end());
    cur.print();
    while(c.back() != NO_COMMAND){
        command cmd = c.back();
        int cnt=0;
        while(c.back()==cmd){
            c.pop_back();
            cnt++;
        }
        // out(cmd, "*", cnt); // why: output.txt出力抑制のため
        for (int loop = 0; loop < cnt; loop++){
            cur.do_cmd(cmd);
        }
        cur.print();
    }
}

void validate_ring_sortedA(State& st) {
#ifdef DEBUG_VALIDATE
    int max_a = -1, max_a_i = -1;
    for (int i = 0; i < st.A.size(); i++) {
        if (chma(max_a, st.A[i])) {
            max_a_i = i;
        }
    }
    int cur = max_a_i;
    for (int i = 0; i < st.A.size() - 1; i++) {
        int nex = cur - 1;
        if (nex < 0) {
            nex = st.A.size() - 1;
        }
        my_assert(st.A[cur] > st.A[nex]);
        cur = nex;
    }
#endif
}

//stのiターン目の状態を返す
//cmd_listもiターン目の状態になる
State get_state_turn_i(const State& st, size_t i){
    int st_cmd_len = st.get_cmd_list().list.size();
    my_assert(st_cmd_len >= i);
    State ret(st);
    while(ret.get_cmd_list().list.size() > i){
        ret.revert_last_cmd();
    }
    return ret;
}

State get_start_state(const State& st){
    return get_state_turn_i(st, 0);
}



void test_get_start_state(){
    State a({1,2,3, 4}, {5,6,7,8});
    a.sa();
    a.pb();
    a.pb();
    a.pb();
    auto ret = get_start_state(a);
    ret.print();
}


bool same_stack(const State&lhs, const State&rhs){
    return lhs.A==rhs.A && lhs.B==rhs.B;
}

//テスト
//State ystaaa_st(0, deque<int>{1,2,3}, deque<int>{4,5,6,7,8});
//for (int i = 0; i < 3; i++){
//out("ra_dis ", i, get_ra_dis(ystaaa_st,  i));
//}
//for (int i = 0; i < 3; i++){
//out("rra_dis ", i, get_rra_dis(ystaaa_st,  i));
//}
//for (int i = 0; i < 5; i++){
//out("rb_dis ", i, get_rb_dis(ystaaa_st,  i));
//}
//for (int i = 0; i < 5; i++){
//out("rb_dis ", i, get_rrb_dis(ystaaa_st,  i));
//}
