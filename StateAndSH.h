//
//

#ifndef UNTITLED_STATEANDSH_H
#define UNTITLED_STATEANDSH_H


#include "all_include.h"
#include "util.h"
#include "Command/Command.h"
#include "Command/SyncCommand.h"
#include "array_manager.h"
#include "RingBuffer500.h"
#include "Command/InitCommand.h"
#include "State.h"
#include "StateHash.h"

//二つを持っているだけ
//StateHashは後々計量にしたい（内部にdq持ってるのが無駄） stateと合体させたい
class StateAndSH{
public:
    State &st;
    StateHash &sh;

    StateAndSH(State& st, StateHash& sh) : st(st), sh(sh){
    }

    void do_cmd(const command& cmd){
        st.do_cmd(cmd);
        sh.do_cmd(cmd);
    }

    void undo_cmd(){
        sh.do_cmd(revert_cmd[st.get_cmd_list().list.back()]);
        st.revert_last_cmd();
        if(sh.hash()==(int64_t)212863391751359351){
            // out("cur"); // why: output.txt出力抑制のため
        }
        if(sh.hash()==(int64_t)227636214333905817){
            // out("cur"); // why: output.txt出力抑制のため
            ;
        }
    }

    void sa(){
        do_cmd(SA);
    }

    void sb(){
        do_cmd(SB);
    }

    void ss(){
        do_cmd(SS);
    }

    void pa(){
        do_cmd(PA);
    }

    void pb(){
        do_cmd(PB);
    }

    void ra(){
        do_cmd(RA);
    }

    void rb(){
        do_cmd(RB);
    }

    void rr(){
        do_cmd(RR);
    }

    void rra(){
        do_cmd(RRA);
    }

    void rrb(){
        do_cmd(RRB);
    }

    void rrr(){
        do_cmd(RRR);
    }

    int N() const {
        return st.current_N;
    }
};


#endif //UNTITLED_STATEANDSH_H
