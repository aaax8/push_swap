//
//

#ifndef UNTITLED_OPTIMIZERANGESEARCH_H
#define UNTITLED_OPTIMIZERANGESEARCH_H


#include "../../State.h"
#include "../../StateHash.h"
#include "../Command.h"
#include "../IsCommand.h"
#include "../InitCommand.h"
#include "../SyncCommand.h"
#include "../legal_commands/LegalStateCmd.h"
#include "../../Legacy/lis_chank/NgStateChecker.h"

enum dfs_type_t {
    dfs1, dfs2
};
using hash_node_t = unordered_map<uint64_t, cmd_list_t>;


//startからgoalへ遷移するような最短のパスを保持する
class ShortestPathHolder {
    using path_t = vector<command>;
    int origin_path_len;
    bool was_updated;
    State &start, goal;
    path_t best_path;

    path_t merge_path(const path_t &l_cmds, const path_t &r_rev_cmds) {
        path_t res(l_cmds);
        for (int i = r_rev_cmds.size() - 1; i >= 0; i--) {
            res.emplace_back(revert_cmd[r_rev_cmds[i]]);
        }
        return res;
    }

    void validate_correct_path(const path_t &path) {
        State res = get_apply_cmds(start, path, 0, path.size());
        my_assert(same_stack(res, goal));
    }

    bool is_new_record(const path_t &path) {
        if (!was_updated) {
            return origin_path_len > path.size();
        } else {
            return best_path.size() > path.size();
        }
    }

    void update_path(const path_t &path) {
        best_path = path;
        was_updated = true;
    }

public:
    ShortestPathHolder(State &start, State &goal, int origin_path_len) :
            start(start), goal(goal), origin_path_len(origin_path_len), was_updated(false) {
        my_assert(origin_path_len > 0);
    }

    //startからl_cmdsをたどったものと、goalからr_rev_cmdsをたどったものが一致する引数が与えられる
    void try_upd_path(const path_t &l_cmds, const path_t &r_rev_cmds) {
        path_t path = merge_path(l_cmds, r_rev_cmds);
        validate_correct_path(path);
        if (is_new_record(path)) {
            update_path(path);
        }
    }

    bool has_answer() {
        return was_updated;
    }

    path_t get_shortest_path() {
        my_assert(was_updated);
        return best_path;
    }
};

void test_shortest_path_holder();

//片方だけが持つものが入る
template<class Legal>
class SearchState {
public:
    StateHash &sh;
    hash_node_t &visited;
    int cur_dep;
    int max_dep;
    dfs_type_t dfs_type;
    cmd_list_t cmd_list;//今回のサーチにおけるコマンドたち
    Legal legal;

    //Legalに必要な機能
    //  legal_command
    //  do_cmd(cmd)
    //  un_do(cmd)
    SearchState(StateHash &cur_sh, Legal &legal, hash_node_t &path_history, int max_dep, dfs_type_t dfs_type) :
            sh(cur_sh), cur_dep(0), max_dep(max_dep), dfs_type(dfs_type),
            visited(path_history),
            legal(legal) {

    }

    vector<command> legal_cmds() {
        vector<command> res;
        for (auto &cmd: legal.legal_cmds()) {
            res.emplace_back(cmd);
        }
        return res;
    }

    void do_cmd(const command &cmd) {
        cur_dep++;
        sh.do_cmd(cmd);
        legal.do_cmd(cmd);
        cmd_list.add(cmd);
    }

    void undo_cmd(const command &cmd) {
        cur_dep--;
        sh.do_cmd(revert_cmd[cmd]);
        legal.undo_cmd(cmd);
        cmd_list.list.pop_back();
    }

    //初めて到達したか、最短経路を更新できたらtrueを返す
    bool try_upd_visited() {
        const uint64_t hash = sh.hash();
        auto it = visited.find(hash);
        //初めて見る場所
        if (it == visited.end()) {
            visited[hash] = cmd_list;
            return true;
        } else {
            if (it->second.list.size() > cmd_list.list.size()) {
                it->second = cmd_list;
                return true;
            } else {
                return false;
            }
        }

    }
};


template<class Legal>
class OptimizeRangeSearch {

    int call_upd_hash_node;
    int cnt_new_node_l;
    int update_node_l;
    hash_node_t shortest_node_l;
    hash_node_t shortest_node_r;

    State start;
    State goal;
    int origin_len;
    int max_dep_l;
    int max_dep_r;
    StateHash start_sh;
    StateHash goal_sh;
//    vector<tuple<uint64_t, cmd_list_t, cmd_list_t>> match_res;
    ShortestPathHolder best_path_holder;

    void print_hash_node() {
        for (auto &[hash, list]: shortest_node_l) {
            out("hash : ", hash);
            out("list : ", list);
            out("");
        }
    }

    //rのハッシュがアップデートされたらupdateをかえす
    //vector<pair<uint64_t, cmd_list_t>>&
    void try_upd_goal_path(SearchState<Legal> &search_st) {
        const uint64_t hash = search_st.sh.hash();
        const cmd_list_t &cmd_list = search_st.cmd_list;
        auto it_l = shortest_node_l.find(hash);
        if (it_l != shortest_node_l.end()) {
            best_path_holder.try_upd_path(it_l->second.list, cmd_list.list);
        }
    }

    void init() {
        cnt_new_node_l = 0;
        call_upd_hash_node = 0;
        update_node_l = 0;
        my_assert(shortest_node_l.empty());
        my_assert(shortest_node_r.empty());
        cnt_dfs = 0;
    }

    bool done;
    Legal &legal;
public:
    OptimizeRangeSearch(const State &st, Legal &legal, int cmd_l, int cmd_r) :
            start(get_state_turn_i(st, cmd_l)),
            goal(get_state_turn_i(st, cmd_r)),
            origin_len(cmd_r - cmd_l),
            max_dep_l(origin_len / 2),
            max_dep_r(origin_len - max_dep_l),
            start_sh(start),
            goal_sh(goal),
            done(false),
            legal(legal),
            best_path_holder(start, goal, origin_len) {
        init();
    }

private:
    //すでに訪れた盤面を削除
    //幅優先で盤面をコピーすると重いので、毎回操作列を一からやり直す
    //can_saなどを StateHashにいれる
    //shだけの方が良いけど、cmdの実行が可能かをstでしか現状はかれないので取りあえずこのままいく
//    opt_dfs(ystaaa_st, sh, cmd_list, cur_dep + 1, max_dep, origin2_len, shortest_node_l, dfs_type);
    void opt_dfs(SearchState<Legal> &search_st) {
        cnt_dfs++;
        if (!search_st.try_upd_visited()) {
            return;
        }
        if (search_st.dfs_type == dfs2) {
            try_upd_goal_path(search_st);
        }
        if (search_st.cur_dep == search_st.max_dep) {
            return;
        }
        for (auto &cmd: search_st.legal_cmds()) {
            search_st.do_cmd(cmd);
            opt_dfs(search_st);
            search_st.undo_cmd(cmd);
        }
    }

    int cnt_dfs;

public:
    //!
    //stの[l, r)番目のコマンドを最適化したものを返す
    auto optimize_range() {
        my_assert(!done);
        legal.build(start);
        SearchState<Legal> ss_start(start_sh, legal, shortest_node_l, max_dep_l, dfs1);
        opt_dfs(ss_start);
        legal.build(goal);
        SearchState<Legal> ss_goal(goal_sh, legal, shortest_node_r, max_dep_r, dfs2);
        opt_dfs(ss_goal);
        done = true;
//        out("cnt_dfs", cnt_dfs);
        return best_path_holder;
    }
};

template<size_t ARG_N>
void optimize_chank_cmds(State &start, State &__st, vector<command> past_cmds, NgStateChecker<ARG_N> &ng_checker) {
    State chank_start(__st);
    out("past_cmds", past_cmds.size());
    out(past_cmds);
    for (int i = 0; i < past_cmds.size(); i++) {
        chank_start.revert_last_cmd();
    }
    out("chank_start");
    chank_start.print();
    State chank_goal(__st);
    out("chank_goal");
    chank_goal.print();
    exit(0);
}

// human_review_begin: push_swap CLI では optimize の進捗ログを stdout に出さないようにするため。
void set_optimize_range_log_enabled(bool enabled);
// human_review_end

void optimize_all_range(State &st, int range, int left);

#endif //UNTITLED_OPTIMIZERANGESEARCH_H
