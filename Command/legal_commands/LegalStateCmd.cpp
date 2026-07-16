//
//

#include "LegalStateCmd.h"

void test_same_cmds_range()
{
    //push_syncの場合
    //ok
//    {
//        SameCmdsRange scr(e_PushSyncCmds_type::PUSH_SYNC);
//        for (int i = 0; i < 11; i++){
//            bool same = scr.same_type((command)i);
//            if(same ){
//                out((command)i, "is_push_sync");
//            }else{
//                out((command)i, "is_1side");
//            }
//        }
//    }
//    //side1cmdの場合
//    //ok
//    {
//        SameCmdsRange scr(e_PushSyncCmds_type::ONE_SIDE_CMDS);
//        for (int i = 0; i < 11; i++){
//            bool same = scr.same_type((command)i);
//            if(!same){
//                out((command)i, "is_push_sync");
//            }else{
//                out((command)i, "is_1side");
//            }
//        }
//    }
//    out("------------");
//    {
//        SameCmdsRange scr(e_PushSyncCmds_type::PUSH_SYNC);
//        bool ok = true;
//        for (int i = 0; i < 100; i++){
//            scr.add(rand_push_sync_cmd());
//            if(!scr.can_do((command)( my_rand()%11))){
//                ok = false;
//                out("failed scr cando");
//            }
//        }
//        if(ok){
//            out("ok scr push_sync");
//        }
//    }
//    //add side1
//    if(0){
//        SameCmdsRange scr(e_PushSyncCmds_type::ONE_SIDE_CMDS);
//        bool ok = true;
//        for (int i = 0; i < 100; i++){
//            auto cmd = rand_ab_cmd();
//            scr.add(cmd);
//            out("add",cmd );
//            scr.print_cnt();
//            vector<command> can;
//            for (int j = 0; j < 11; j++){
//                if(scr.can_do((command)j)){
//                    can.push_back((command)j);
//                }
//            }
//            out("can", can);
//        }
//        if(ok){
//            out("ok scr push_sync");
//        }
//    }
//    if(0){
//        SameCmdsRange scr(e_PushSyncCmds_type::ONE_SIDE_CMDS);
//        auto can_do=[&](){;
//            vector<command> can;
//            for (int j = 0; j < 11; j++){
//                if(scr.can_do((command)j)){
//                    can.push_back((command)j);
//                }
//            }
//            out("can", can);
//        };
//        scr.add(RA);
//        scr.add(RRA);
//        scr.add(SA);
//        scr.add(SA);
//        scr.add(RB);
//        scr.print_cnt();
//        can_do();
//        scr.pop_back(RB);
//        can_do();
//        scr.pop_back(SA);
//        can_do();
//        scr.pop_back(SA);
//        can_do();
//        scr.pop_back(RRA);
//        can_do();
//        scr.pop_back(RA);
//        can_do();
//    }
//    out("test LegalStateCmd");
//    {
//            LegalStateCmd lc;
//            vector<command> cmd_list;
//            auto cmd = rand_cmd();
//            auto do_cmd=[&](auto cmd){
//                lc.do_cmd(cmd);
//                out("do_cmd ", cmd);
//                lc.print();
//                out("legal best_path_holder");
//                out(lc.legal_cmds());
//                cmd_list.push_back(cmd);
//            };
//            for (int i = 0; i < 100; i++){
//                auto cmds = lc.legal_cmds();
//                cmd = cmds[my_rand() % cmds.size()];
//                out(cmd);
//                do_cmd(cmd);
//                if(cmd_list.size() > 10){
//                    while(cmd_list.size()){
//                        lc.undo_cmd(cmd_list.back());
//                        out("undo_cmd ", cmd);
//                        lc.print();
//                        out("legal best_path_holder");
//                        out(lc.legal_cmds());
//                        cmd_list.pop_back();
//                }
//            }
//        }
//    }
}

void test_last_ab(){
//    LegalStateCmd lc;
//    vector<command>cmd_list;
//    for (int i = 0; i < 100; i++){
//        auto cmds = lc.legal_cmds();
//        auto cmd = cmds[my_rand() % cmds.size()];
//        if(((i/10)%2)==0){
//            out("do_cmd", cmd);
//            lc.do_cmd(cmd);
//            cmd_list.push_back(cmd);
//        }else{
//            out("pop", cmd_list.back());
//            lc.undo_cmd(cmd_list.back());
//            cmd_list.pop_back();
//        };
//        out("best_path_holder");
//        out(cmd_list);
//        lc.print();
//    }

}
