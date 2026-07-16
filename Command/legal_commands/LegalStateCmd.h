//
//

#ifndef UNTITLED_LEGALSTATECMD_H
#define UNTITLED_LEGALSTATECMD_H
#include "../Command.h"
#include "../IsCommand.h"
#include "../InitCommand.h"
#include "../SyncCommand.h"
#include "NextCmds.h"
#include "../Testmd.h"
#include "../../State.h"

enum e_cmds_state{CMDS_EMPTY, CMDS_NOT_EMPTY};

//ngのリストをもつ
class CmdsOpt{
public:
    ~CmdsOpt(){
//        out("destructor cmd");
    }
    virtual bool same_type(const command& cmd)=0;
    virtual bool can_do(const command& cmd)=0;
    virtual e_cmds_state pop_back(const command& cmd)=0;
    virtual void add(const command& cmd)=0;
};

enum e_PushSyncCmds_type{PUSH_SYNC, ONE_SIDE_CMDS};

//bがあるときに、aを行えないなどのルールはここでは考えない
class SameCmdsRange : public CmdsOpt{
    int push_cnt;
    int ra_cnt, rra_cnt, sa_cnt;
    e_PushSyncCmds_type type ;
public:
    void print(){
        if(type==::PUSH_SYNC){
            out("type : PUSH_SYN");
        }else{
            out("type : ONE_SIDE_CMDS");
        }
        out("push_cnt :", push_cnt);
        print_cnt();
    }

    void print_cnt(){
        out("ra_cnt, rra_cnt, sa_cnt : ", ra_cnt, rra_cnt, sa_cnt);
    }

    SameCmdsRange(e_PushSyncCmds_type type): push_cnt(0), ra_cnt(0), rra_cnt(0), sa_cnt(0), type(type){
    }

    bool same_type(const command& cmd) override{
        if(is_sync_push_cmd(cmd)){
            return type == PUSH_SYNC;
        }else{
            return type == ONE_SIDE_CMDS;
        }
    }

    bool same_type(const e_PushSyncCmds_type& rhs){
        return type == rhs;
    }

    bool can_do(const command& cmd){
        if(!is_b_cmd(cmd)){
            return true;
        }else if(cmd == RB){
            return ra_cnt == 0;
        }else if(cmd == RRB){
            return rra_cnt == 0;
        }else if(cmd == SB){
            return sa_cnt == 0;
        }
        return true;
    }

    void add(const command& cmd) override{
        my_assert(same_type(cmd));
        if(cmd==RA){
            ra_cnt++;
        }else if(cmd == RRA){
            rra_cnt++;
        }else if(cmd == SA){
            sa_cnt++;
        }
        push_cnt++;
    }

    e_cmds_state pop_back(const command& cmd)override{
        my_assert(push_cnt > 0);
        push_cnt--;
        if(cmd==RA){
            my_assert(--ra_cnt >= 0);
        }else if(cmd == RRA){
            my_assert(--rra_cnt >= 0);
        }else if(cmd == SA){
            my_assert(--sa_cnt >= 0);
        }
        if(push_cnt == 0){
            return CMDS_EMPTY;
        }else{
            return CMDS_NOT_EMPTY;
        }
    }
};

//
class Legal{
protected:
    bool was_build;
    //!
    Legal():was_build(false){

    }
public:
    virtual bool can_do(const command& cmd)=0;
    virtual void do_cmd(const command& cmd)=0;
    virtual void undo_cmd(const command& cmd)=0;
    virtual vector<command> legal_cmds()=0;
    //Legalを実行する前に、buildで初期化する
    //コピー
    virtual void build(const State& st)=0;
};

//bがあるときに、aができないのは　どこでやってるのか
//nextsか
class LegalStateCmd : public Legal{
    void update_last_ab(const command& cmd){
        if(is_push_cmd(cmd)){
            last_as.emplace_back(cmd);
            last_bs.emplace_back(cmd);
        }else if(is_sync_cmd(cmd)){
            last_as.emplace_back(cmd);
            last_bs.emplace_back(cmd);
        }else if(is_a_cmd(cmd)){
            last_as.emplace_back(cmd);
        }else if(is_b_cmd(cmd)){
            last_bs.emplace_back(cmd);
        }else{
            my_assert(0);
        }
    }

    void add_to_range(const command& cmd){
        const e_PushSyncCmds_type type = get_cmd_type(cmd);
        if(cmd_ranges.empty() || !cmd_ranges.back().same_type(type)){
            cmd_ranges.emplace_back(type);
        }
        cmd_ranges.back().add(cmd);
    }

public:
    vector<SameCmdsRange> cmd_ranges;
    vector<command> last_as, last_bs;
    State st;
//    command last_a, last_b;

//    LegalStateCmd() : last_a(NO_COMMAND), last_b(NO_COMMAND){
    void clear(){
        cmd_ranges.clear();
        last_as.clear();
        last_bs.clear();
        last_as.emplace_back(NO_COMMAND);
        last_bs.emplace_back(NO_COMMAND);
    }

    bool can_do(const command& cmd){
        return (st.can_do(cmd) && can_next_cmds[last_as.back()][last_bs.back()][cmd]);
    }

    LegalStateCmd(): Legal(), st({}, {}){
        my_assert(!was_build);
        clear();
        my_assert(was_init_cmds);
    }

    void print(){
        out("-------------");
        out("last_as", last_as);
        out("last_bs", last_bs);
        for(auto& cmds : cmd_ranges){
            cmds.print();
        }
        out("-------------");
    }

    e_PushSyncCmds_type get_cmd_type(const command& cmd){
        if(is_sync_push_cmd(cmd)){
            return ::PUSH_SYNC;
        }else{
            return ::ONE_SIDE_CMDS;
        }
    }

    void do_cmd(const command& cmd)override{
        my_assert(can_next_cmds[last_as.back()][last_bs.back()][cmd]);
        update_last_ab(cmd);
        add_to_range(cmd);
        st.do_cmd(cmd);
    }

    //LegalCommanderの順序に従ってコマンドが行われ、後ろからpopされる前提
    void undo_cmd(const command& cmd)override{
        my_assert(!cmd_ranges.empty());
        if(cmd_ranges.back().pop_back(cmd) == e_cmds_state::CMDS_EMPTY){
            cmd_ranges.pop_back();
        }
        if(last_as.back() == cmd){
            last_as.pop_back();
        }
        if(last_bs.back()==cmd){
            last_bs.pop_back();
        }

        my_assert(st.get_cmd_list().list.back() == cmd);
        st.revert_last_cmd();
    }

    //実行可能なコマンドを返す（スタックの状況は考慮しない）
    vector<command> legal_cmds()override{
        vector<command> res;
        for(auto&cmd : ALL_COMMANDS){
            if(can_do(cmd)){
                res.emplace_back(cmd);
            }
        }
        return res;
    }

    //実行する前に呼ぶ
    void build(const State& st)override{
        clear();
        was_build = true;
        this->st = st;
    }

};

void test_same_cmds_range();
void test_last_ab();
#endif //UNTITLED_LEGALSTATECMD_H
