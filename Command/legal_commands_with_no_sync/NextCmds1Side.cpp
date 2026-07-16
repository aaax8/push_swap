//
//

#include "NextCmds1Side.h"

//(i, j) := 同期以降の、最後ののa_cmdがiで、b_cmdがjであるときの選択肢
//同期以降にa_cmdやb_cmdがない場合は、NO_COMMANDになる

//(last_a_eff, last_b_eff) := 使えるコマンド
//SS, RB等としたときは
//最後の二つが(SS, RB)になる
//ra, RRは認めず, RR, raのように、可能な限りsyncが前になるような順序で行う

//SS, RB, SBはSB, RB, SSと等価だが、同様の理由で
//SS, RB, SBだけを認める
//逆かも、SB, RB, SSをやらないようにするのはむずかしい

//そもそも根本的に難しいか
//SB, RB, SAを封じたいが...
array<array<vector<command>, NO_COMMAND + 1>, NO_COMMAND + 1> next_cmds_without_sync;


array<array<array<bool, NO_COMMAND + 1>, NO_COMMAND + 1>, NO_COMMAND + 1> can_next_cmds_without_sync;


//syncコマンドを使わない場合の次の選択達

vector<command> get_nexts_without_sync(command last_a, command last_b) {
    my_assert(!is_sync_cmd(last_a) && !is_sync_cmd(last_b));
    set<command> res;
    for (int cmd1 = 0; cmd1 < 11; cmd1++) {
        if(is_sync_cmd((command)cmd1))continue;
        res.insert((command) cmd1);
    }
    auto try_erase = [&](const command &cmd) {
        if (res.count(cmd)) {
            res.erase(cmd);
        }
    };
    if (last_a == NO_COMMAND && last_b == NO_COMMAND) { ;
    } else if (is_push_cmd(last_a) || is_push_cmd(last_b)) {
        my_assert(last_a == last_b);
        try_erase(revert_cmd[last_a]);
    } else {
        my_assert((is_a_cmd(last_a) || last_a == NO_COMMAND) &&
                  (is_b_cmd(last_b) || last_b == NO_COMMAND));
        if (is_a_cmd(last_a)) {
            try_erase(revert_cmd[last_a]);
        }
        if (is_b_cmd(last_b)) {
            //Aコマンドを消す
            try_erase(SA);
            try_erase(RA);
            try_erase(RRA);
            try_erase(revert_cmd[last_b]);
        }
    }
    vector<command> res_v;
    for (auto &s: res) {
        res_v.push_back(s);
    }
    return res_v;
}

