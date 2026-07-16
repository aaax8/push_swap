//
//

#ifndef UNTITLED_LEGALCOMMANDER1SIDE_H
#define UNTITLED_LEGALCOMMANDER1SIDE_H
#include "../Command.h"
#include "../IsCommand.h"
#include "../InitCommand.h"
#include "../SyncCommand.h"
#include "NextCmds1Side.h"

enum e_cmds_state_no_sync{CMDS_EMPTY_NO_SYNC, CMDS_NOT_EMPTY_NO_SYNC};
//
class MatchCmds{
public:
    MatchCmds(){
//        out("destructor cmd");
    }
    virtual int compressed_cmd_len()=0;
    virtual bool can_add(const command& cmd)=0;
    virtual e_cmds_state_no_sync pop_back()=0;
    virtual void add(const command& cmd)=0;
};

class PushCmds : public MatchCmds{
    int push_cnt;
public:
    PushCmds():push_cnt(0){

    }

    bool can_add(const command& cmd) override{
        return (is_push_cmd(cmd));
    }

    void add(const command& cmd) override{
        my_assert(is_push_cmd(cmd));
        push_cnt++;
    }

    e_cmds_state_no_sync pop_back()override{
        my_assert(push_cnt > 0);
        push_cnt--;
        if(push_cnt == 0){
            return CMDS_EMPTY_NO_SYNC;
        }else{
            return CMDS_NOT_EMPTY_NO_SYNC;
        }
    }


    int compressed_cmd_len()override{
        return push_cnt;
    }
};

//一回pushがない前提でやる
class  Dynamic1SideCmds : public MatchCmds{
public:
    vector<command> cmds_a;
    vector<command> cmds_b;

    vector<vector<int>> dp_len;

    //Bの長さがiの時の最小

    Dynamic1SideCmds() : dp_len(1, vector<int>(1)){
    }

    bool can_add(const command& cmd)override{
        my_assert(!is_sync_cmd(cmd));
        return (!is_push_cmd(cmd));
    }

    //aのコマンドの長さ
    //該当の長さの部分を0で初期化する必要がある
    void resize_dpa(){
        int new_dp_an = cmds_a.size() +1 ;
        if( dp_len.size() + 1 == new_dp_an){
            dp_len.push_back(vector<int>(1));
        }else if(dp_len.size() >= new_dp_an){
            dp_len[new_dp_an - 1][0] = 0;//要らない
        }else{
            my_assert(0);
        }
    }

    void resize_dpb(int ai){
        int new_dp_bn = cmds_b.size() + 1 ;
        if( dp_len[ai] .size() + 1 == new_dp_bn){
            dp_len[ai] .push_back(0);
        }else if(dp_len[ai] .size() >= new_dp_bn){
            dp_len[ai][new_dp_bn-1]=0;
        }else{
            my_assert(0);
        }
    }

    void match(){
        auto chma = [&](auto &lhs, const auto &rhs) {
            if (lhs < rhs) {
                lhs = rhs;
            }
        };
//        my_assert(cmds_a.sz()+1 == dp_len.sz());
        resize_dpb(0);
        for (int a_i = 0; a_i < cmds_a.size(); a_i++) {
            resize_dpb(a_i + 1);
            int b_i = cmds_b.size()-1;
            auto &tar = dp_len[a_i + 1][b_i + 1];
            //a, bをsyncする場合
            if (can_sync_cmd(cmds_a[a_i], cmds_b[b_i])) {
                chma(tar, dp_len[a_i][b_i] + 1);
            }
            //a-1, b
            chma(tar, dp_len[a_i][b_i + 1]);
            //a, b-1
            chma(tar, dp_len[a_i + 1][b_i]);
        }
//        out(dp_len);
//        out("--------------------------");
    }

    void add(const command& cmd)override{
//        out("add : ", cmd);
        my_assert(is_a_cmd(cmd) || is_b_cmd(cmd) || is_push_cmd(cmd));
        if(!cmds_b.empty()){
            my_assert(!is_a_cmd(cmd));
        }
        if(is_a_cmd(cmd)){
            cmds_a.push_back(cmd);
//            dp_len.resize(dp_len.sz()+1);
            resize_dpa();
//            dp_len.push_back(vector<int>(1));
        }else if(is_b_cmd(cmd)){
            cmds_b.push_back(cmd);
            if(!cmds_a.empty()){
                match();
            }else{
                //aが空ならなにもしない
            }
        }else{

        }
    }

    e_cmds_state_no_sync pop_back()override{
        if(!cmds_b.empty()){
            cmds_b.pop_back();
        }else{
            my_assert(cmds_a.size());
            cmds_a.pop_back();
        }
        if(cmds_a.empty() && cmds_b.empty()){
            return CMDS_EMPTY_NO_SYNC;
        }else{
            return CMDS_NOT_EMPTY_NO_SYNC;
        }
    }

    int compressed_cmd_len()override{
        if(cmds_a.empty()){
            return cmds_b.size();
        }
        return cmds_a.size() + cmds_b.size() - dp_len[cmds_a.size()][cmds_b.size()];
    }
};

class DynamicMatchCmds{
    vector<MatchCmds*> cmds_list;

    int calc_back_len(){
        return cmds_list.back() -> compressed_cmd_len();
    }

    void set_new_len(int prev_last_len, int new_last_len){
        compressed_len = compressed_len - prev_last_len + new_last_len;
    }

public:
    int compressed_len=0;
    ~DynamicMatchCmds(){
        for(auto& cmds : cmds_list){
            delete cmds;
        }
    }

    DynamicMatchCmds(){
    }


    void add(const command& cmd){
        auto chk_make_new_node=[&](){
            if(cmds_list.empty() || !cmds_list.back()->can_add(cmd)){
                if(is_push_cmd(cmd)){
                    cmds_list.push_back(new PushCmds());
                }else if(is_a_cmd(cmd) || is_b_cmd(cmd)){
                    cmds_list.push_back(new Dynamic1SideCmds());
                }else{
                    my_assert(0);
                }
            }
        };
        chk_make_new_node();
        my_assert(cmds_list.back() -> can_add(cmd));
        int prev_last_len = calc_back_len();
        cmds_list.back() -> add(cmd);
        int new_last_len = calc_back_len();
        set_new_len(prev_last_len, new_last_len);
    }

    void pop_back(){
        my_assert(cmds_list.size());
        int prev_last_len = calc_back_len();
        if(cmds_list.back()->pop_back() == CMDS_EMPTY_NO_SYNC){
            delete cmds_list.back();
            cmds_list.pop_back();
            compressed_len -= prev_last_len;
        }else{
            int new_last_len = calc_back_len();
            set_new_len(prev_last_len, new_last_len);
        }
    }
};

//打ち消しのコマンド

void init_cmds_without_sync();


#endif //UNTITLED_LEGALCOMMANDER1SIDE_H
