//
//

#ifndef UNTITLED_OPTIMIZEMATCH_H
#define UNTITLED_OPTIMIZEMATCH_H

#include "../../State.h"
#include "../Command.h"
#include "../IsCommand.h"
#include "../SyncCommand.h"

namespace ns_optimize_cmd {
    class optimize_match_c {
        //    enum command{SA, SB, SS_, PA_, PB_, RA, RB, RR, RRA, RRB, RRR};
//    private:
    public:
        void move_top_syncs(vector<command> &res, deque<command> &rem_data) {
            vector<command> syncs;
            while (!rem_data.empty() && is_sync_push_cmd(rem_data[0])) {
                syncs.push_back(rem_data[0]);
                rem_data.pop_front();
            }
            syncs = del_offset_cmds(syncs);
            res.insert(res.end(), syncs.begin(), syncs.end());
        }

        //remの上から、a, bコマンドを移す
        pair<vector<command>, vector<command>> tract_ab_cmd(deque<command> &rem) {
            vector<command> cmd_as;
            vector<command> cmd_bs;
            auto move_top = [&](auto &to, auto &from) {
                my_assert(from.size());
                to.push_back(from[0]);
                from.pop_front();
            };
            while (!rem.empty() && !is_sync_push_cmd(rem[0])) {
                if (is_a_cmd(rem[0])) {
                    move_top(cmd_as, rem);
                } else if (is_b_cmd(rem[0])) {
                    move_top(cmd_bs, rem);
                } else {
                    my_assert(0);
                }
            }
            return make_pair(cmd_as, cmd_bs);
        }

        void test_() {
            deque<command> cmds = {SS, PA, PB, RA, RA, RB, RRB, RRA, RRB, RR, RRR, SA, SB};
            ns_optimize_cmd::optimize_match_c mt;
            {
                vector<command> res;
                mt.move_top_syncs(res, cmds);
                out("{", res, "}");
            }
            {
                auto [as, bs] = mt.tract_ab_cmd(cmds);
                out(as);
                out(bs);
            }
            {
                vector<command> res;
                mt.move_top_syncs(res, cmds);
                out(res);
            }
            {
                auto [as, bs] = mt.tract_ab_cmd(cmds);
                out(as);
                out(bs);
            }
        }


        //ra, rra等の相殺されるコマンドを消す
        template<class List>
        vector<command> del_offset_cmds(List &cmds) {
            vector<command> res;
            for (int i = 0; i < cmds.size(); i++) {
                auto &cmd = cmds[i];
                if (!res.empty() && is_offset(res.back(), cmd)) {
                    res.pop_back();
                } else {
                    res.push_back(cmd);
                }
            }
            return res;
        }

        void test_del_offset_cmds() {
            vector<command> cmds = {RA, PA, PA, PB, PB, RRA, RRR, RRR, RA, RB, RRB, SA, SA, SB, SB, RA, SS, SS, RRB, RB,
                                    SB, SB, RRA, RRB, SA};
            auto ret = del_offset_cmds(cmds);
            out(ret);
        }


        pair<vector<int>, vector<int>> max_match_idx(vector<command> &cmd_as, vector<command> &cmd_bs) {
            struct choose_t {
                int sync_cnt;
                int a_i;
                int b_i;
            };
            int NA = cmd_as.size();
            int NB = cmd_bs.size();
            //何文字目か
            vector<vector<choose_t>> dp(NA + 1, vector<choose_t>(NB + 1, {0, -1, -1}));
            dp[0][0] = {0, -1, -1};
            auto chma = [&](choose_t &lhs, const choose_t &rhs) {
                if (lhs.sync_cnt < rhs.sync_cnt) {
                    lhs = rhs;
                }
            };
            for (int a_i = 0; a_i < NA; a_i++) {
                for (int b_i = 0; b_i < NB; b_i++) {
                    auto &tar = dp[a_i + 1][b_i + 1];
                    //a, bをsyncする場合
                    if (can_sync_cmd(cmd_as[a_i], cmd_bs[b_i])) {
                        choose_t rhs = {dp[a_i][b_i].sync_cnt + 1, a_i, b_i};
                        chma(tar, rhs);
                    }
                    //a-1, b
                    chma(tar, dp[a_i][b_i + 1]);
                    //a, b-1
                    chma(tar, dp[a_i + 1][b_i]);
                }
            }
            vector<int> idx_as;
            vector<int> idx_bs;
            int a_cnt = NA, b_cnt = NB;
            while (dp[a_cnt][b_cnt].sync_cnt > 0) {
                int a_i = dp[a_cnt][b_cnt].a_i;
                int b_i = dp[a_cnt][b_cnt].b_i;
                idx_as.push_back(a_i);
                idx_bs.push_back(b_i);
                a_cnt = a_i;
                b_cnt = b_i;
            }
            std::reverse(idx_as.begin(), idx_as.end());
            std::reverse(idx_bs.begin(), idx_bs.end());
            return {idx_as, idx_bs};
        }

        vector<vector<int>> max_match_tab(vector<command> &cmd_as, vector<command> &cmd_bs) {
            struct choose_t {
                int sync_cnt;
                int a_i;
                int b_i;
            };
            int NA = cmd_as.size();
            int NB = cmd_bs.size();
            //何文字目か
            vector<vector<choose_t>> dp(NA + 1, vector<choose_t>(NB + 1, {0, -1, -1}));
            vector<vector<int>> dp_len(NA + 1, vector<int>(NB + 1, 0));
            dp[0][0] = {0, -1, -1};
            auto chma = [&](choose_t &lhs, const choose_t &rhs) {
                if (lhs.sync_cnt < rhs.sync_cnt) {
                    lhs = rhs;
                    return true;
                }
                return false;
            };
            for (int a_i = 0; a_i < NA; a_i++) {
                for (int b_i = 0; b_i < NB; b_i++) {
                    auto &tar = dp[a_i + 1][b_i + 1];
                    //a, bをsyncする場合
                    if (can_sync_cmd(cmd_as[a_i], cmd_bs[b_i])) {
                        choose_t rhs = {dp[a_i][b_i].sync_cnt + 1, a_i, b_i};
                        if (chma(tar, rhs)) {
                            dp_len[a_i + 1][b_i + 1] = dp_len[a_i][b_i] + 1;
                        }
                    }
                    //a-1, b
                    if (chma(tar, dp[a_i][b_i + 1])) {
                        dp_len[a_i + 1][b_i + 1] = dp_len[a_i][b_i + 1];
                    }
                    //a, b-1
                    if (chma(tar, dp[a_i + 1][b_i])) {
                        dp_len[a_i + 1][b_i + 1] = dp_len[a_i + 1][b_i];
                    }
                }
            }
            return dp_len;
        }

        vector<command> optimize_match(vector<command> &cmd_as, vector<command> &cmd_bs) {
            for (auto &cmd_a: cmd_as) {
                my_assert(is_a_cmd(cmd_a));
            }
            for (auto &cmd_b: cmd_bs) {
                my_assert(is_b_cmd(cmd_b));
            }
            auto [sync_as, sync_bs] = max_match_idx(cmd_as, cmd_bs);
            sync_as.push_back(cmd_as.size());
            sync_bs.push_back(cmd_bs.size());
            vector<command> res;
            my_assert(sync_as.size() == sync_bs.size());
            int ai_l = 0, bi_l = 0;
            for (int sync_i = 0; sync_i < sync_as.size(); sync_i++) {
                int ai_r = sync_as[sync_i];
                int bi_r = sync_bs[sync_i];
                for (int ai = ai_l; ai < ai_r; ai++) {
                    res.push_back(cmd_as[ai]);
                }
                for (int bi = bi_l; bi < bi_r; bi++) {
                    res.push_back(cmd_bs[bi]);
                }
                if (sync_i + 1 == sync_as.size()) {
                    break;
                }
                res.push_back(sync_cmd(cmd_as[ai_r], cmd_bs[bi_r]));
                ai_l = ai_r + 1;
                bi_l = bi_r + 1;
            }
            return res;
        }

//        optimize_match(vector<command>& cmd_as, vector<command>& cmd_bs){
        void test_optimize_match() {
            vector<command> cmd_as = {RA, RA, RA, SA, RRA, RRA, RRA, RA, SA};
            vector<command> cmd_bs = {SB, RB, RB, SB, RB, RRB, SB, RB, RB};
            vector<command> matched = optimize_match(cmd_as, cmd_bs);
//            auto [ais, bis] = max_match_idx(cmd_as, cmd_bs);
            out(matched);
        }

    public:
//        auto[cmd_as, cmd_bs] = tract_ab_cmd(rem_data);
//        auto [matched_a, matched_b]= max_match_idx(cmd_as, cmd_bs);
//        res.insert(res.end(), matched.begin(), matched.end());

        vector<command> unzip_sync(const vector<command> &cmds) {
            vector<command> res;
            for(auto& cmd : cmds){
                if(is_sync_cmd(cmd)){
                    res.push_back(sync_child_a[cmd]);
                    res.push_back(sync_child_b[cmd]);
                }else{
                    res.push_back(cmd);
                }
            }
            return res;
        }

        vector<command> optimize_sync(const vector<command> &input_cmds) {
            auto unzip_cmds = unzip_sync(input_cmds);
            deque<command> rem_data(unzip_cmds.begin(), unzip_cmds.end());
            vector<command> res;
            move_top_syncs(res, rem_data);
            while (!rem_data.empty()) {
                auto [cmd_as, cmd_bs] = tract_ab_cmd(rem_data);
                cmd_as = del_offset_cmds(cmd_as);
                cmd_bs = del_offset_cmds(cmd_bs);
                auto matched = optimize_match(cmd_as, cmd_bs);
                res.insert(res.end(), matched.begin(), matched.end());
                move_top_syncs(res, rem_data);
            }
            return res;
        }

        //test用
        void move_top_syncs_no_del(vector<command> &res, deque<command> &rem_data) {
            vector<command> syncs;
            while (!rem_data.empty() && is_sync_push_cmd(rem_data[0])) {
                syncs.push_back(rem_data[0]);
                rem_data.pop_front();
            }
//            syncs = del_offset_cmds(syncs);
            res.insert(res.end(), syncs.begin(), syncs.end());
        }

        //test用
        vector<command> optimize_test_no_del(vector<command> &cmds) {
            deque<command> rem_data(cmds.begin(), cmds.end());
            vector<command> res;
            move_top_syncs_no_del(res, rem_data);
            while (!rem_data.empty()) {
                auto [cmd_as, cmd_bs] = tract_ab_cmd(rem_data);
//                cmd_as = del_offset_cmds(cmd_as);
//                cmd_bs = del_offset_cmds(cmd_bs);
                auto matched = optimize_match(cmd_as, cmd_bs);
                res.insert(res.end(), matched.begin(), matched.end());
                move_top_syncs_no_del(res, rem_data);
            }
            return res;
        }

    };
}

#endif //UNTITLED_OPTIMIZEMATCH_H
