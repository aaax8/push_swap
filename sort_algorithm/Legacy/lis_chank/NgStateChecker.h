//
//

#ifndef UNTITLED_NGSTATECHECKER_H
#define UNTITLED_NGSTATECHECKER_H

//#include "all_include.h"
//#include "State.h"
//#include "util.h"
//#include "Chank.h"
//#include "Lis.h"
//#include "SyncCommand.h"

// human_review_begin: 公開用コピー単体で include が解決できるようにローカル参照へ変更する。
#include "all_include.h"
#include "State.h"
#include "StateHash.h"
#include "util.h"
// human_review_end
#include "Chank.h"
//#include "Lis.h"
//#include "../../Command/SyncCommand.h"
//#include "../../Command/InitCommand.h"
//#include "EvaluateByPrecure.h"
//#include "MergeBSale.h"


//(1)番目のbtmに対してswapする操作は無駄だが、それを許した方がビームサーチのスコアが良いため放置
template<size_t ARG_N>
class NgStateChecker{
public:
    State&st;//初期化の時だけ使う
    ChankInfo<ARG_N> &chank;
    int target_chank_id;

    int a_top_ng;//top禁止
    //virtual_sort_aとの整合性を取るために、一旦_pastとした
    int a_btm_ng_past;//top禁止

    //チャンクのソートを始める初期状態での、aのtopの値
    int a_btm;//top状態でいくつかのコマンド禁止
    int b_top_ng;//top禁止
    int b_btm;//top状態でいくつかのコマンド禁止
    bool is_first_merge;

    vector<int> is_alis;
    vector<int> is_bchank;

    void print(){
        out("chank id", target_chank_id);
        out("chank ", chank.chank_vals[target_chank_id]);
        vector<int> alis;
        for (int i = 0;  i < ARG_N; i++){
            if(is_alis[i]){
                alis.push_back(i);
            }
        }
        out("is_first_merge", is_first_merge);
        out("alis");
        out(alis);
        out("atop, btm", a_top_ng, a_btm_ng_past);
        out("btop, btm", b_top_ng, b_btm);
    }

    void set_a(){
        auto register_to_alis=[&](int v){
            my_assert(!is_alis[v] );
            is_alis[v]=true;
        };
        a_top_ng =-1;
        a_btm_ng_past =-1;
        register_to_alis(st.A[0]);
        int b_min = chank.chank_vals[target_chank_id][0];
        for (int i = st.A.size()-1; i >= 0; i--){
            //土台として使えるなら alisに入る
            //大小関係の問題は起きない
            if(st.A[i] > b_min){
                register_to_alis(st.A[i]);
            }else{
                if(!is_alis[st.A[i]]){
                    a_top_ng = st.A[i];
                }
                break;
            }
        }
        if(st.A.size() > 1 && !is_alis[st.A[1]]){
            a_btm_ng_past = st.A[1];
        }
    }

    void set_b(){
        b_top_ng =-1;
        b_btm =-1;
        for(auto&v : chank.chank_vals[target_chank_id]){
            is_bchank[v] = true;
        }
        int chank_size = chank.chank_size(target_chank_id);
        if(st.B.size() == chank_size){
            b_top_ng = -1;
            b_btm = -1;
        }else{
            my_assert(!is_bchank[st.B[chank_size]]);
            b_top_ng = st.B.back();
            b_btm = st.B[chank_size];
        }
    }


    NgStateChecker(State&st, ChankInfo<ARG_N> &chank, int target_chank_id, int ini_a_top): st(st), chank(chank),
                                                                                           target_chank_id(target_chank_id),
                                                                                           is_first_merge(target_chank_id == chank.chank_sizes.size()-1)//todo ルール
            , is_alis(ARG_N)
            , is_bchank(ARG_N)
            , a_btm(ini_a_top)
    {
        set_a();
        set_b();
    }

private:

    bool can_do_of_ini_a_top(State &st, const command& cmd){
        if(st.A.empty()){
            my_assert(0);
            return true;
        }
        int a_top = st.A[0];
        if(a_top == a_btm){
            switch (cmd) {
                case SA:
                case SS:
                case PB:
                case RA:
                case RR:
                    return false;
                default:
                    break;
            }
            //円環でない前提で考えている
        }
//        else if(ystaaa_st.A.size() >1 && ystaaa_st.A[1] == a_btm && (cmd == SA || cmd==SS)){
//            return false;
//        }
        return true;
    }

    bool can_do_of_btm_b(State &st, const command& cmd){
        if(st.B.empty()){
            my_assert(b_top_ng==-1 && b_btm == -1);
            return true;
        }
        int b_top = st.B[0];
        if(b_top == b_btm){
            switch (cmd) {
                case SB:
                case SS:
                case PA:
                case RB:
                case RR:
                    return false;
                default:
                    break;
            }
        }
        //付けると1手下がった
//        else if(target_chank_id >0&& ystaaa_st.B.size() >1 && ystaaa_st.B[1] == b_btm && (cmd == SB || cmd==SS)){
//            return false;
//        }
        return true;
    }


    bool ok_sate(State& st){
        auto ok_a=[&](){
            if(st.A.empty()){
                my_assert(a_top_ng==-1 && a_btm_ng_past == -1);
                return true;
            }
            int a_top = st.A[0];
            return (a_top != a_top_ng && a_top != a_btm_ng_past) ;
        };
        auto ok_b=[&](){
            if(st.B.empty()){
                my_assert(b_top_ng==-1 && b_btm == -1);
                return true;
            }
            int b_top = st.B[0];
            return (b_top != b_top_ng);
        };
        return ok_a() && ok_b();
    }

public:

    bool can_do(State &st, const command& cmd){
        my_assert(st.can_do(cmd));
        if(!can_do_of_ini_a_top(st, cmd)){
            return false;
        }
        if(!can_do_of_btm_b(st, cmd)){
            return false;
        }
        st.do_cmd(cmd);
        bool res = ok_sate(st);
        st.revert_last_cmd();
        return res;
    }
};


#endif //UNTITLED_NGSTATECHECKER_H
