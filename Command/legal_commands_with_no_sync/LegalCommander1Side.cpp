//
//

#include "LegalCommander1Side.h"

//dmcはsyncがaddされない仕様
//コメントアウトしたコードを使ってる部分があるので前部コメントアウト
//void test_dynamic_match_cmd_c(){
////    DynamicMatchCmds
//
//    ns_optimize_cmd::optimize_match_c opt ;
//    DynamicMatchCmds dmc;
////    optimize_test_no_del
//
//    for (int i = 0; i < 1000; i++){
//        dmc = DynamicMatchCmds();
//        cmd_list_t cmds_a, cmds_b;
//        vector<command> cmds_shuf;
//        vector<command> cmds_ab;
//        auto rand_cmd_no_sync=[&](){
//            command cmd = (command)(my_rand()%11);
//            while(is_sync_cmd(cmd)){
//                cmd = (command)(my_rand()%11);
//            }
//            return cmd;
//        };
//        int cmd_len = my_rand()%500;
//        for (int i = 0; i < cmd_len; i++){
//            auto cmd = rand_cmd_no_sync();
//            cmds_shuf.push_back(cmd);
//        }
//        {
//            deque<command> cmd_dq(cmds_shuf.begin(), cmds_shuf.end());
//            while(cmd_dq.sz()){
//                auto[as, bs] = tract_ab_cmd(cmd_dq);
//                cmds_ab.insert(cmds_ab.end(), as.begin(), as.end());
//                cmds_ab.insert(cmds_ab.end(), bs.begin(), bs.end());
//                while(cmd_dq.sz() && is_push_cmd( cmd_dq[0])){
//                    cmds_ab.push_back(cmd_dq[0]);
//                    cmd_dq.pop_front();
//                }
//            }
//        }
//        for(auto&cmd: cmds_ab){
//            dmc.add(cmd);
//        }
//        int dmc_len = dmc.compressed_len;
//        int opt_len = opt.optimize_test_no_del(cmds_ab).sz();
//        if(dmc_len != opt_len){
//            out("failed");
//            out(cmds_shuf);
//            out("dmc", dmc_len);
//            out("opt", opt_len);
//            exit(0);
//
//        }
//    }
//}
void init_cmds_without_sync() {
    for (int last_a = 0; last_a <= NO_COMMAND; last_a++) {
        for (int last_b = 0; last_b <= NO_COMMAND; last_b++) {
            auto set_no_cmd=[&](){
                next_cmds_without_sync[last_a][last_b].push_back(NO_COMMAND);
                can_next_cmds_without_sync[last_a][last_b][NO_COMMAND] = true;
            };
            //todo 乗法の散逸
            if (last_a == NO_COMMAND && last_b == NO_COMMAND) { ;
            } else if (is_push_cmd((command)last_a) || is_push_cmd((command)last_b)) {
                if(last_a != last_b){
                    set_no_cmd();
                    continue;
                }
            } else if (is_sync_cmd((command)last_a) || is_sync_cmd((command)last_b)) {
//                if(last_a != last_b){
                set_no_cmd();
                continue;
//                }
            } else {
                bool must = ((is_a_cmd((command)last_a) || last_a == NO_COMMAND) &&
                             (is_b_cmd((command)last_b) || last_b == NO_COMMAND));
                if(!must){
                    set_no_cmd();
                    continue;
                }
            }
            next_cmds_without_sync[last_a][last_b] = get_nexts_without_sync((command) last_a, (command) last_b);
            for(auto& cmd : next_cmds_without_sync[last_a][last_b]){
                can_next_cmds_without_sync[last_a][last_b][cmd] = true;
            }
//            out("a, b ", (command)last_a, (command)last_b);
//            out(next_cmds_without_sync[last_a][last_b]);
//            out("");
        }
    }
}
