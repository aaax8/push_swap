//
//

#include "NextCmds.h"




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
array<array<vector<command>, NO_COMMAND + 1>, NO_COMMAND + 1> next_cmds;


// (a, b)が到達不能の場合、(a, b)が持つ値は不定
array<array<array<bool, NO_COMMAND + 1>, NO_COMMAND + 1>, NO_COMMAND + 1> can_next_cmds;

//片B -> 片Aは禁止
//直前のA, Bのコマンドについて
//  syncできるコマンドは禁止
//  打ち消すコマンドは禁止
//下位のコマンドから、その上位のコマンドは禁止

//今見てる属性
//a, b, ab, push

//aの中に含まれる物と、マッチしないようにする必要がある...?
//めんどくさい...

//RA, SA, RRA
//なら、RB, SB, RRBはマッチングできるから不可
vector<command> get_nexts(command last_a, command last_b) {
    set<command> res;
    for (int cmd1 = 0; cmd1 < 11; cmd1++) {
        res.insert((command) cmd1);
    }
    auto try_erase = [&](const command &cmd) {
        if (res.count(cmd)) {
            res.erase(cmd);
        }
    };
    auto is_same_push=[&](command& last_a, command& last_b){
        return (is_push_cmd(last_a) && last_a == last_b);
    };
    auto is_another_sync_push=[&](command& last_a, command& last_b){
        return is_sync_push_cmd(last_a) && is_sync_push_cmd(last_b) && last_a != last_b;
    };
    //SA, SB, SS, RA, RB, RR, RRA, RRB, RRR
//    auto set_no_cmd=[&](){
//        res.init();
//        res.insert(NO_COMMAND);
//    };
//    if(is_a_cmd(last_b) || is_b_cmd(last_a)){
//        set_no_cmd();
//    }
//    //NO_COMMANDで一致する可能性があるのでこう書いた
//    if(is_a_cmd(last_a) && sync_partner[last_a] == last_b){
//        my_assert(is_b_cmd(last_b) && sync_partner[last_b] == last_a);
//        set_no_cmd();
//    }
//    if(last_a != last_b && (is_sync_cmd(last_a) && last_b == revert_cmd[sync_child_b[last_a]])){
//        set_no_cmd();
//    }
//    if(last_a != last_b && is_sync_cmd(last_b) && last_a == revert_cmd[sync_child_a[last_b]]){
//        set_no_cmd();
//    }
    //PA, PB,
    //Pushだけなら
    if(is_same_push(last_a, last_b)){
        try_erase(revert_cmd[last_a]);
    }
//    else if(is_another_sync_push(last_a, last_b)){
//        set_no_cmd();
//    }
//    if(last_a!=NO_COMMAND && last_b!=NO_COMMAND && last_a != SS && revert_cmd[last_a] == last_b){
//        set_no_cmd();
//    }
//    if(is_sync_push_cmd(last_a) && last_b == NO_COMMAND){
//        set_no_cmd();
//    }
//    if(is_sync_push_cmd(last_b) && last_a == NO_COMMAND){
//        set_no_cmd();
//    }


    if (is_a_cmd(last_a)) {
        try_erase(revert_cmd[last_a]);
        //sync相手
        //上位のsync
        try_erase(sync_partner[last_a]);
        try_erase(sync_top[last_a]);
        for(auto&hrev_cmd : harf_revert[last_a]){
            try_erase(hrev_cmd);
        }
    }
    if (is_b_cmd(last_b)) {
        //Aコマンドを消す
        try_erase(SA);
        try_erase(RA);
        try_erase(RRA);

        try_erase(revert_cmd[last_b]);
        try_erase(sync_partner[last_b]);//Aなので要らないけど分かりやすさのためにかく
        try_erase(sync_top[last_b]);
        for(auto&hrev_cmd : harf_revert[last_b]){
            try_erase(hrev_cmd);
        }
    }

    if(is_sync_cmd(last_a)){
        try_erase(revert_cmd[sync_child_a[last_a]]);
        try_erase(revert_cmd[last_a]);
    }
    if(is_sync_cmd(last_b)){
        try_erase(revert_cmd[sync_child_b[last_b]]);
        try_erase(revert_cmd[last_b]);
    }
    //pushが片方隠れてたら無影響

    //異なるpush or syncと異なるpush or syncがある場合はダメ

    //片A, 片B, すべてあり得る
    //  片Aは全滅で
    //  A, Bについての打ち消しがきえる
    //  A, BについてのSyncパートナーが消える
    //  A, Bについての上位Syncが消える
    //  rotateなら、A, Bについての反転上位Sync

    //片A, (push, No_command)だけなら
    //打ち消し、パートナー、上位Sync、　反転上位Sync(raにたいして、RRR)

    //片B, (push, No_command)だけなら
    //片A全て、 打ち消し、パートナー、上位Sync、　反転上位Sync

    //No, No,
    //全部可能

    //Syncについて
    //AB同じSyncなら
    //SS, SS
    //SA, SB, SSはだめ

    //RR, RR
    //RRR, RRA, RRBはだめ

    //RRR, RRR
    //RR, RA, RB

    //片A, SS
    //SB, SSはだめ

    //片A, RR
    //RRB, RRR
    //RBのrevertと、RRBの上位

    //片A, RRR
    //RB, RR
    //RRBのrevertと、RBの上位
    vector<command> res_v;
    for (auto &s: res) {
        res_v.push_back(s);
    }
    return res_v;
}
