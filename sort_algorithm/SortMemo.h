//
//

#ifndef UNTITLED_SORTMEMO_H
#define UNTITLED_SORTMEMO_H
#include "../TimeKeeper.h"
#include "../all_include.h"
#include "../State.h"
#include "../StateHash.h"
#include "../util.h"
#include "../Command/SyncCommand.h"
#include "../Command/InitCommand.h"
#include "../Command/optimize/OptimizeRangeSearch.h"
#include "../Command/optimize/OptimizeMatch.h"
#include "../StateAndSH.h"


//一度見たStateのHash値について、それをソートするのにかかる手数をメモする
class SortMemo {
protected:
    using state_hash_t = uint64_t ;
    //最終的に何手でソートが完了するかを値として持つ
    // (これから何手ではなく、これまでの手数も含む)
    unordered_map<state_hash_t, int> memo_sort_step;
    int called_turn;//stateが何ターン目の時にこのアルゴリズムが呼ばれたか
public:
    //!
    void validate_eq_st_sh(State& st, StateHash& sh){
#ifdef DEBUG
        StateHash sh2(st);
        my_assert(sh.hash() == sh2.hash());
#endif
    }
protected:
    void validate_sh_history(StateAndSH& ssh){
#ifndef DEBUG
        return;
#endif
        //ターンについてのhash値
        my_vector<int64_t> hashs(ssh.st.turn()+1);
        State cpy_st(ssh.st);
//        State get_apply_cmds(const State& ystaaa_st, const cmd_list_t& cmd_list, int cmd_l, int cmd_r){
        int cur_turn = called_turn;
        while(cur_turn < ssh.st.turn() + 1){
            cpy_st = get_state_turn_i(ssh.st, cur_turn);
            StateHash cpy_sh(cpy_st);
            hashs[cur_turn] = cpy_sh.hash();
            cur_turn++;
        }
        StateHash curSH(ssh.st);
        State curST(ssh.st);
        cur_turn = ssh.st.turn();
        while(true){
            my_assert(hashs[cur_turn] == curSH.hash());

            if(cur_turn == called_turn){
                break;
            }
            curSH.do_cmd(revert_cmd[curST.get_cmd_list().list.back()]);
            curST.revert_last_cmd();

            cur_turn--;
        }
    }

    //!
    //現状から、ソートが呼ばれたタイミングまで手数を記録する
    void memo_until_state(StateAndSH& ssh, int rem_step){
        validate_eq_st_sh(ssh.st, ssh.sh);
        my_assert(ssh.st.turn() >= called_turn);
        validate_sh_history(ssh);
        int last_turn = ssh.st.turn();
        while(true){
            int dummy=0;
            //最後のターンはすでに答えが入っている時があるので確認しない
            //・↑途中でメモを見つけたと時が該当
            //全てのコマンドごとに確認してるわけでも無いから
            //ちょうふくすることもあるか
//            if(ssh.ystaaa_st.turn() < last_turn){
//                my_assert(!try_get_memo_rem_step(ssh.sh, dummy));
//            }
            {
                int past_score=-1;
                if(try_get_memo_rem_step(ssh.sh, past_score)){
//                    my_assert(past_score == rem_step);
//                    my_assert(past_score < rem_step);
                    if(past_score > rem_step){
                        memo_sort_step[ssh.sh.hash()] = rem_step;
                    }
                }else{
                    memo_sort_step[ssh.sh.hash()] = rem_step;
                }
            }

            if(ssh.st.turn() == called_turn){
                break;
            }
            ssh.undo_cmd();
            rem_step++;
        }
        validate_eq_st_sh(ssh.st, ssh.sh);
    }

    //!
    bool try_get_memo_rem_step(StateHash& sh, int& out_step){
        auto it = memo_sort_step.find(sh.hash());
        //ない
        if(it == memo_sort_step.end()){
            out_step = -1;
            return false;
        }else{
            out_step = it->second;
            return true;
        }
    }

    //答えが分かったら、memo_until_stateを呼ぶ
    //親のevaluateで、最適なタイミングまでロールバックされるので、revert等は気にしなくていい
    virtual int evaluate_body(StateAndSH &sh)=0;

public:
    //!
    //値を返す際、stの状態は呼ばれたときと同じ状態である
    int evaluate(State& st, StateHash &sh){
        StateAndSH ssh(st, sh);
        validate_eq_st_sh(st, sh);
        int res_step=0;
        if(try_get_memo_rem_step(sh, res_step)){
            return st.turn() + res_step;
        }
        called_turn = st.turn();
        res_step = evaluate_body(ssh);
        while(ssh.st.turn() > called_turn){
            ssh.undo_cmd();
        }
        return res_step;
    }
};

#endif //UNTITLED_SORTMEMO_H
