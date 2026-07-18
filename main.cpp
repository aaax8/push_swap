#include "all_include.h"
#include "util.h"

# include <chrono>
#include "Command/Command.h"
#include "Command/InitCommand.h"
#include "Command/IsCommand.h"
#include "Command/SyncCommand.h"
#include "Command/legal_commands/LegalStateCmd.h"
#include "Command/legal_commands/NextCmds.h"
#include "Command/legal_commands_with_no_sync/LegalCommander1Side.h"
#include "Command/legal_commands_with_no_sync/NextCmds1Side.h"
#include "Command/optimize/OptimizeMatch.h"
#include "Command/optimize/OptimizeRangeSearch.h"
#include "InvNumOfPS.h"
#include "RingBuffer500.h"
#include "State.h"
#include "StateHash.h"
#include "TimeKeeper.h"
#include "ankerl/unordered_dense.h"
#include "sort_algorithm/BeamAll.h"
#include "sort_algorithm/SortMemo.h"
#include "sort_algorithm/InitBeam/BeamEvaluator.h"
#include "sort_algorithm/InitBeam/BeamInitializer.h"
#include "sort_algorithm/InitBeam/BeamSearcher.h"
#include "sort_algorithm/InitBeam/BeamStateRunner.h"
#include "sort_algorithm/InitBeam/BeamStateUtil.h"
#include "sort_algorithm/InitBeam/CircularRange.h"
#include "sort_algorithm/YakinamashiChunk/ChunkYakinamashi.h"
#include <filesystem>
#include "QueriesModifier.h"
//
#include <cstdint>
#include "sort_algorithm/YakinamashiChunk/YakinamashiChunkState.h"
#include "YakinamashiQuery/YstFactory.h"
#include "YakinamashiQuery/Test/TestGreedyQueriesConstructor.h"
#include "YakinamashiQuery/Test/TestRepeatedDestroyConstruct.h"
#include "YakinamashiQuery/Test/TestRepeatedDestroyConstruct.h"
#include "YakinamashiQuery/Test/TestGreedyQueriesConstructorIndirect.h"
#include "YakinamashiQuery/Test/TestQueriesModifier.h"
#include "YakinamashiQuery/Test/TestRangeQueryDestroyer.h"
#include "YakinamashiQuery/Test/TestA2BB2ARangeSetLazySegTree.h"
#include "YakinamashiQuery/SimpleYakinamashi.h"
#include "YakinamashiQuery/IterativeGreedy.h"
#include "YakinamashiQuery/SwapBeam/SwapBeamSearch.h"
#include "YakinamashiQuery/SwapBeam/SwapAllOrdGreedySearch.h"
#include "YakinamashiQuery/Validation/YakinamashiStateValidator.h"
#include "ChunkAndBeamSolver.h"
#include <optional>
// human_review_begin: push_swap CLI の argv 数値検証で errno/ERANGE を使うため。
#include <cerrno>
// human_review_end
// human_review_begin: push_swap CLI の単一文字列 argv を空白区切りで読むため。
#include <sstream>
// human_review_end

void test_stackaminpos();

void test_set_cur_connect_info();

void test_chunk_separator();

void test_chunk_command();

void test_chunk_state_factory();

void test_chunk_state_factory_build();

void test_chunk_yakinamashi();

//x ChunkYakinamashi + run_annealing のテスト
void test_chunk_yakinamashi_annealing();

void test_circular_range_edge_cases();

void test_circular_range_wrapping();

void test_separated_to_beam_search();

void test_a2b_beam_minimal();

void test_a2b_beam_apply_revert();

void test_a2b_beam_expand();

void test_a2b_query_factory();

void test_a2b_evaluate_nieve();

void test_a2b_beam_search_random();

void test_b2a_under_max(int n);
void test_under_max_sort_predictor(int n);
void test_a2b_greedy_runner(int n);
void test_a2b_beam_runner(int n, int beam_width);

#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <unordered_map>
#include <vector>

//@準備
struct initon {
    initon() {
        cin.tie(0);
        ios::sync_with_stdio(false);
        cout.setf(ios::fixed);
        cout.precision(16);
        srand((unsigned) clock() + (unsigned) time(NULL));
    };
} initonv;

void easy_solver(State &st) {
    auto make_greater_on_b = [&]() {
        for (int cur = 0; cur < st.current_N; cur++) {
            my_assert(!st.A.empty());
            while (st.A[0] != cur) {
                st.ra();
                // out(RA); // why: output.txt出力抑制のため
            }
            st.pb();
            // out(PB); // why: output.txt出力抑制のため
        }
    };
    auto all_push_to_a = [&]() {
        for (int i = 0; i < st.current_N; i++) {
            st.pa();
            // out(PA); // why: output.txt出力抑制のため
        }
    };
    make_greater_on_b();
    all_push_to_a();
}

// 影響を及ぼすa, bのコマンドを入れる
// pa, rbなら lastがpa, rbになる

// match, ssとか分解しておく

void init_cmds() {
    for (int last_a = 0; last_a <= NO_COMMAND; last_a++) {
        for (int last_b = 0; last_b <= NO_COMMAND; last_b++) {
            next_cmds[last_a][last_b] = get_nexts((command) last_a, (command) last_b);
            for (auto &cmd: next_cmds[last_a][last_b]) {
                // cout << last_a <<", "<<last_b<<", ok "<<cmd  << endl;
                can_next_cmds[last_a][last_b][cmd] = true;
            }
            //            out("a, b ", (command)last_a, (command)last_b);
            //            out(next_cmds_without_sync[last_a][last_b]);
            //            out("");
        }
    }
}

void init(int N) {
    init_revert_cmd();
    init_sync_cmd();
    init_cmds();
    init_cmds_without_sync();
    was_init_cmds = true;
//    PastQueryFactory::set_N(N);
}

// 操作列から状況を
// cmd_list_t cmd_list;
// stに、cmd_listの前apply_len個のコマンドを適用した結果を返す
// State get_apply_cmds(const State& ystaaa_st, cmd_list_t& cmd_list, int
// apply_len = -1){
//     if(apply_len == -1){
//         apply_len = cmd_list.list.sz();
//     }
//     State ret(ystaaa_st);
//     for (int i = 0; i < apply_len; i++){
//         ret.do_cmd(cmd_list.list[i]);
//     }
//     return ret;
// }

void test_apply_cmds() {
    State st({1, 2, 3, 4, 5}, {});
    cmd_list_t cmds;
    cmds.add(RA);
    cmds.add(SA);
    cmds.add(PB);
    cmds.add(PB);
    cmds.add(SB);
    // out("st0"); // why: output.txt出力抑制のため
    State st0 = get_apply_cmds(st, cmds, 0, 0);
    st0.print();

    // out("ystaaa_st"); // why: output.txt出力抑制のため
    st.print();
    State st_all = get_apply_cmds(st, cmds, 0, cmds.list.size());
    // out("st_all"); // why: output.txt出力抑制のため
    st_all.print();
    // out("st_3"); // why: output.txt出力抑制のため
    State st_3 = get_apply_cmds(st, cmds, 0, 3);
    st_3.print();
}

//{hash_a, hash_b}, cmd_list

void test_hash1() {
    vector<int> perm(100);
    std::iota(perm.begin(), perm.end(), 0);
    unordered_map<uint64_t, deque<int>> map;
    int conf_cnt = 0;
    for (int _ = 0; _ < 1000000; _++) {
        std::shuffle(perm.begin(), perm.end(), engine);
        //        deque<int>(perm.begin(), perm.end())
        deque_hash dq(deque<int>(perm.begin(), perm.end()));
        //        if(map.)
        auto hash = dq.all_hash;
        auto it = map.find(hash);
        if (it == map.end()) {
            map[hash] = to_dq(dq.ring_buf);
        } else {
            deque<int> dq_content = to_dq(dq.ring_buf);
            if (map[hash] != dq_content) {
                // out("conflict!", conf_cnt++); // why: output.txt出力抑制のため
                // out("hash : ", hash); // why: output.txt出力抑制のため
                // out("dq1 : ", map[hash]); // why: output.txt出力抑制のため
                // out("dq2 : ", dq_content); // why: output.txt出力抑制のため
            }
        }
    }
}

void test_hash2() {
    vector<int> perm(100);
    std::iota(perm.begin(), perm.end(), 0);
    unordered_map<uint64_t, pair<deque<int>, deque<int>>> map;

    int conf_cnt = 0;
    for (int _ = 0; _ < 100000; _++) {
        std::shuffle(perm.begin(), perm.end(), engine);
        int an = rand() % 101;
        //        deque<int>(perm.begin(), perm.end())
        deque_hash a_hash(deque<int>(perm.begin(), perm.begin() + an));
        deque_hash b_hash(deque<int>(perm.begin() + an, perm.end()));
        StateHash sh(a_hash, b_hash);

        //        if(map.)
        auto hash = sh.hash();
        auto it = map.find(hash);
        deque<int> a_dq = to_dq(a_hash.ring_buf);
        deque<int> b_dq = to_dq(b_hash.ring_buf);
        if (it == map.end()) {
            map[hash] = make_pair(a_dq, b_dq);
        } else {
            if (map[hash] != make_pair(a_dq, b_dq)) {
                // out("conflict!", conf_cnt++); // why: output.txt出力抑制のため
                // out("hash : ", hash); // why: output.txt出力抑制のため
                // out("a1 : ", map[hash].first); // why: output.txt出力抑制のため
                // out("b1 : ", map[hash].second); // why: output.txt出力抑制のため
                // out("a2 : ", a_dq); // why: output.txt出力抑制のため
                // out("b2 : ", b_dq); // why: output.txt出力抑制のため
            }
        }
    }
}

// newを使って、push-sync系の区間と、片cmdの区間を持つ

// todo
// Sync系を子供よりもはやくみるようにする

// int rnt_dfs_next_cmds;
// void dfs_next_cmds(LegalStateCmd& lac, int dep, int max_dep, vector<command>&
// best_path_holder){
//     if(dep == max_dep){
//         rnt_dfs_next_cmds +=
//         best_path_holder[my_rand()%best_path_holder.sz()]; return;
//     }
//     auto nexts = next_cmds[lac.last_a][lac.last_b];
//     for(auto& next : nexts){
//         if(is_b_cmd(next) && !lac.is_ng_b(next)){
//             continue;
//         }
//         best_path_holder.push_back(next);
//         lac.do_cmd(next);
//         dfs_next_cmds(lac, dep+1, max_dep, best_path_holder);
//         best_path_holder.undo_cmd();
//     }
// }
//
// void test_next_cmds(){
////    int cnt=0;
////    for (int i = 0; i < 12; i++){
////        for (int j = 0; j < 12; j++){
////            if(next_cmds[i][j] == vector<command>{NO_COMMAND}) {
////                continue;
////            }
////            cnt++;
////            out("last : ",(command)i, (command)j);
////            out(next_cmds[i][j]);
////        }
////    }
////    out("cnt", cnt);
////    return;
//
//    LegalStateCmd lac;
////    auto add=[&](command cmd){
////        if(!lac.do_cmd(cmd)){
////            out("failed cmd : ", cmd);
////            return;
////        }
////        out("do cmd : ", cmd);
////        out("last a, b : ", lac.last_a, lac.last_b);
////    };
//    command cmd = RA;
//    lac.do_cmd(cmd);
//    for (int i = 0; i < 20; i++){
//        out("-----------------");
//        out("do :", cmd);
//        auto nex = next_cmds[lac.last_a][lac.last_b];
//        out("last_ab :", lac.last_a, lac.last_b);
//        out("nexts :", nex);
//        lac.print_ng_bs();
//        cmd = nex[my_rand() % nex.sz()];
//        while(is_b_cmd(cmd) && lac.is_ng_b(cmd)){
//            cmd = nex[my_rand() % nex.sz()];
//        }
//        lac.do_cmd(cmd);
//    }
//}

// void test_next_cmds_without_sync(){
//     LegalStateCmd lac;
//     auto add=[&](command cmd){
//         if(!lac.do_cmd(cmd)){
//             out("failed cmd : ", cmd);
//             return;
//         }
//         out("do cmd : ", cmd);
//         out("last a, b : ", lac.last_a, lac.last_b);
//     };
//     command cmd = RA;
//     lac.do_cmd(cmd);
//     for (int i = 0; i < 20; i++){
//         out("do :", cmd);
//         auto nex = next_cmds_without_sync[lac.last_a][lac.last_b];
//         out("last_ab :", lac.last_a, lac.last_b);
//         out("nexts :", nex);
//         cmd = nex[my_rand() % nex.sz()];
//         lac.do_cmd(cmd);
//     }
// }

// やりたいこと
// プリキュアで、チャンクをpaする部分を巡回セールスマン問題的に解く
// swapとかは使わない(あとで最適化)
// ある二つをpushする部分　とか最適か効くかも　考えなくていいか？
// セールスマン問題解くときにそれやる

// 最後次のチャンクへ向かう　もコストに含む

// 長さが凄い短い区間について　最適かをいちいちかけてもいいかも

// 右のチャンクを左に挿入するうえで
// 各順列について、左でのswapを禁じた最小ソートを求めて置けば
// lisが邪魔になっていても　いい感じの回を得られるかも？

// 置き換え　同じ場合は、pushが少ない方が強い？
// もしチャンクを焼きなますとかで、チャンクの挿入の高速な回が欲しい場合
// さっきのswapをしないソートの奴が使えるかも
// 使えないわ　間の分をrotateするコストが無視されるから弱そう
// いや、それでもつよいか（大きい順にいれるより）

// stがちゃんとコマンド列でソートできているか
bool chk_is_finish(State &st) {
    if (st.B.size() > 0) {
        return false;
    }
    int n = st.A.size();
    for (int i = 0; i < n - 1; i++) {
        if (st.A[i] >= st.A[i + 1]) {
            return false;
        }
    }
    return true;
}

// sort_f : stateを渡した時にソートしてくれるよなもの
template<class SORT_F>
void test_cases(int arg_num, int case_num, SORT_F sort_f) {
    auto get_perm = [&](int len) {
        deque<int> dq(len);
        std::iota(dq.begin(), dq.end(), 0);
        std::shuffle(dq.begin(), dq.end(), engine);
        return dq;
    };
    vector<int> steps;
    for (int _ = 0; _ < case_num; _++) {
        // out("case", _); // why: output.txt出力抑制のため
        deque<int> dq = get_perm(arg_num);
        State start(dq, {});
        State cur(start);
        sort_f(cur);
        my_assert(chk_is_finish(cur));
        steps.push_back(cur.turn());
    }
    std::sort(steps.begin(), steps.end());
    // out("sort ok"); // why: output.txt出力抑制のため
    // out("min : ", steps[0]); // why: output.txt出力抑制のため
    // out("med : ", steps[steps.size() / 2]); // why: output.txt出力抑制のため
    // out("max : ", steps.back()); // why: output.txt出力抑制のため
}

// テスト
// 長さを数重とかにして、全ての順番で挿入した場合とdpのを比較して、あってるかとか
// push_aを使うので
// pushがまちがってたらだめだけど
//constexpr int test = 1;
constexpr int test = 1;


int sum_best = 0;
int sum_greedy = 0;

constexpr int test_N = 500;

void test_beam_search_main() {
    vector<int> perm(test_N);
    std::iota(perm.begin(), perm.end(), 0);
    std::mt19937 engine((uint64_t) std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count());
    std::shuffle(perm.begin(), perm.end(), engine);
    State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
//    int beam_width;
//    int outer_k_expand;
//    int narrow_beam_width;
//    int inner_k_expand;
    constexpr int haba = 500;
    constexpr int CHUNK = 6;

    BeamSearcher<haba, CHUNK, test_N> searcher;
    auto get_fin_turn = [&](const BeamState<test_N> &beam_st) {
        return beam_st.current_turn - min(beam_st.min_a_pos, test_N - beam_st.min_a_pos);
    };
    auto result = searcher.search(init_state, CHUNK);

    out("");
    searcher.print_avg();


//    BeamSearcher greedy_searcher(BeamConfig{N, 1, 1, 1});
//    BeamState greedy_state = greedy_searcher.search(init_state);
    auto [best_query, best_cmds] = searcher.reconstruct_commands(init_state, result);
    out("init_state");
    init_state.print();

    out("best_turn", result.current_turn);
//    out("greedy_turn", greedy_state.current_turn);
    out("best_cmd_count", best_cmds.size());
    out("best_cmds");
    out(best_cmds);
//    auto [greedy_query, greedy_cmds] =
//            greedy_searcher.reconstruct_commands(greedy_state);
    out("greedy_cmds");
//    out(greedy_cmds);
    out("best_q");
    vector<int> best_turns, greedy_turns;
    vector<int> a_cmd_cnt(NO_COMMAND + 1);
    vector<int> b_cmd_cnt(NO_COMMAND + 1);
    auto per = [&](int v) {
        return (double) v / best_query.size();
    };
    out("RA", per(a_cmd_cnt[RA]), "RRA", per(a_cmd_cnt[RRA]), "NO", per(a_cmd_cnt[NO_COMMAND]));
    out("RB", per(b_cmd_cnt[RB]), "RRB", per(b_cmd_cnt[RRB]), "NO", per(b_cmd_cnt[NO_COMMAND]));
    std::sort(best_turns.begin(), best_turns.end());
    std::sort(greedy_turns.begin(), greedy_turns.end());

    auto calc_0dis = [&](vector<command> cmds) {
        State st = init_state;
        for (auto &cmd: cmds) {
            st.do_cmd(cmd);
        }
        out("print");
        st.print();
        for (int i = 0; i < test_N; i++) {
            if (st.A[i] == 0) {
                return min(i, test_N - i);
            }
        }
        my_assert(0);
        return -1;
    };
    int best_dis = calc_0dis(best_cmds);
    my_assert(best_dis == 0);
    int best_res = best_cmds.size();//optimize_sync済み
    out("best_res", best_res);
    my_assert(best_res == result.current_turn + min(result.min_a_pos, test_N - result.min_a_pos));
    sum_best += best_res;
}


void test_sep_and_beam() {
    namespace YC = YakinamashiChunk;
    vector<int> perm(test_N);
    std::iota(perm.begin(), perm.end(), 0);
    std::mt19937 engine((uint64_t) std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count());
    std::shuffle(perm.begin(), perm.end(), engine);
    State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
//    int beam_width;
//    int outer_k_expand;
//    int narrow_beam_width;
//    int inner_k_expand;
    constexpr int haba = 3000;
//    constexpr int CHUNK=4;

    int state_N = init_state.current_N;
    my_vector<YC::ChunkQuery> chunk_queries = {
            YC::ChunkQuery(
                    YC::SeparateTargets(YC::SeparateUnit::TOP | YC::SeparateUnit::BTM | YC::SeparateUnit::REMAIN),
                    YC::StackType::A
            ),
//            YC::ChunkQuery(YC::SeparateTargets(YC::SeparateUnit::TOP | YC::SeparateUnit::BTM | YC::SeparateUnit::REMAIN), YC::StackType::A),
//            YC::ChunkQuery(YC::SeparateTargets(YC::SeparateUnit::TOP | YC::SeparateUnit::BTM | YC::SeparateUnit::REMAIN), YC::StackType::A)
    };

    // 1. ChunkAndBeamSolverで初期解生成
    auto [sep_cmds, beam_queries, all_cmds, is_lis, selected_lis_start] = ChunkAndBeamSolver<test_N, haba, 2>::solve(init_state,
                                                                                                                     chunk_queries,
                                                                                                                     state_N);

    int cmd_len = all_cmds.size();
    out("cmd_len", cmd_len);
    sum_best += cmd_len;
}


void init_output(int argc, char *argv[]) {
    std::filesystem::path project_root = get_project_root(argc, argv);
    output_dir = project_root / "OutPut";
    std::filesystem::path output_path = output_dir / "output.txt";

    std::filesystem::create_directories(output_dir);
    output_file_path = output_path;
    output_file.open(output_path, std::ios::out | std::ios::trunc);
    clear_output2_file();
    // 責務: output.txt 以外の補助ログも実行開始時に空にする。
    // 必要な理由: append で書く debug log が前回実行の内容を引きずると、今回の assert 原因を誤読しやすいため。
    clear_output3_file();
    std::ofstream("query_top_transfer_debug.txt", std::ios::trunc).close();
    std::ofstream("greedy_failure_debug.txt", std::ios::trunc).close();
    std::ofstream("main_entry_debug.txt", std::ios::trunc).close();
}

void test_state_vs_statehash_speed() {
    constexpr int N = 100;
    constexpr int NUM_COMMANDS = 2000000;

    vector<int> perm(N);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), engine);

    State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
    StateHash state_hash(init_state);

    State st_for_generation = init_state;
    vector<command> commands;
    commands.reserve(NUM_COMMANDS);

    auto get_last_ab = [](const State &st) -> pair<command, command> {
        command last_a = NO_COMMAND;
        command last_b = NO_COMMAND;
        for (int i = (int) st.get_cmd_list().list.size() - 1; i >= 0; i--) {
            command cmd = st.get_cmd_list().list[i];
            if (is_push_cmd(cmd) || is_sync_cmd(cmd)) {
                if (last_a == NO_COMMAND)
                    last_a = cmd;
                if (last_b == NO_COMMAND)
                    last_b = cmd;
            } else if (is_a_cmd(cmd)) {
                if (last_a == NO_COMMAND)
                    last_a = cmd;
            } else if (is_b_cmd(cmd)) {
                if (last_b == NO_COMMAND)
                    last_b = cmd;
            }
            if (last_a != NO_COMMAND && last_b != NO_COMMAND)
                break;
        }
        return {last_a, last_b};
    };

    for (int i = 0; i < NUM_COMMANDS; i++) {
        auto [last_a, last_b] = get_last_ab(st_for_generation);
        vector<command> available = get_nexts(last_a, last_b);

        vector<command> valid_cmds;
        for (command cmd: available) {
            if (st_for_generation.can_do(cmd)) {
                valid_cmds.push_back(cmd);
            }
        }

        if (valid_cmds.empty()) {
            break;
        }

        int idx = my_rand(valid_cmds.size());
        command selected_cmd = valid_cmds[idx];
        commands.push_back(selected_cmd);
        st_for_generation.do_cmd(selected_cmd);
    }

    out("Generated", commands.size(), "commands");

    State state_for_timing = init_state;
    auto state_start_time = std::chrono::high_resolution_clock::now();
    for (command cmd: commands) {
        state_for_timing.do_cmd(cmd);
    }
    auto state_end_time = std::chrono::high_resolution_clock::now();

    StateHash state_hash_for_timing(init_state);
    auto statehash_start_time = std::chrono::high_resolution_clock::now();
    for (command cmd: commands) {
        state_hash_for_timing.do_cmd(cmd);
    }
    auto statehash_end_time = std::chrono::high_resolution_clock::now();

    StateHash state_hash_result(state_for_timing);
    uint64_t hash_from_state = state_hash_result.hash();
    uint64_t hash_from_statehash = state_hash_for_timing.hash();

    my_assert(hash_from_state == hash_from_statehash);

    auto state_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            state_end_time - state_start_time);
    auto statehash_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(
                    statehash_end_time - statehash_start_time);

    double state_seconds = state_duration.count() / 1000000.0;
    double statehash_seconds = statehash_duration.count() / 1000000.0;

    out("=== Speed Comparison ===");
    out("State time:", state_seconds, "seconds");
    out("StateHash time:", statehash_seconds, "seconds");
    out("State average per command:", state_seconds / commands.size() * 1000000,
        "microseconds");
    out("StateHash average per command:",
        statehash_seconds / commands.size() * 1000000, "microseconds");
    out("Speedup ratio:", state_seconds / statehash_seconds);
    out("Hash match:", hash_from_state == hash_from_statehash);
}


// 責務: current_N と A/B の値列を OutPut/init_state.txt に上書き出力する。
// 必要な理由: GreedySwap の入力 state を debug/log 設定に依存せず後から確認できるようにするため。
void write_iter_init_state(const State& init_state) {
    std::filesystem::create_directories(output_dir);
    std::ofstream ofs(output_dir / "init_state.txt", std::ios::trunc);
    if (!ofs.is_open()) return;
    ofs << "current_N " << init_state.current_N << '\n';
    ofs << "A";
    for (int i = 0; i < init_state.A.size(); i++) {
        ofs << ' ' << init_state.A[i];
    }
    ofs << '\n';
    ofs << "B";
    for (int i = 0; i < init_state.B.size(); i++) {
        ofs << ' ' << init_state.B[i];
    }
    ofs << '\n';
}

// 責務: init_state と IterativeGreedy 後の command 列を OutPut/iterative_greedy_result.txt に出力する。
// 必要な理由: 現行 main 経路の入力と最終解を output.txt の調査ログから分離して確認できるようにするため。
template<size_t MAX_N>
void write_iterative_greedy_result(const State& init_state, const YakinamashiState<MAX_N>& yst) {
    std::filesystem::create_directories(output_dir);
    const std::string file_path = (output_dir / "iterative_greedy_result.txt").string();
    my_vector<command> cmds = YstCommandsBuilder::build_all_commands_from_state(init_state, yst.queries_state);
    out_file(file_path, "init_current_N", init_state.current_N);
    out_file(file_path, "init_A", repeated_state_to_string(init_state.A));
    out_file(file_path, "init_B", repeated_state_to_string(init_state.B));
    out_file(file_path, "final_cmd_count", static_cast<int>(cmds.size()));
    std::string cmd_line = "commands";
    for (command cmd: cmds) cmd_line += " " + std::string(cmd_str[static_cast<int>(cmd)]);
    out_file(file_path, cmd_line);
}

// 責務: optimize_all_range 後の command 数と command 列を OutPut/optimized_range_cmds.txt に書く。
// 必要な理由: 固定 perm/command の range 最適化結果を output.txt から分離して確認するため。
void write_optimized_range_cmds_result(const State& optimized_state, int before_cmd_count) {
    std::filesystem::create_directories(output_dir);
    std::ofstream ofs(output_dir / "optimized_range_cmds.txt", std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) return;
    const vector<command>& cmds = optimized_state.get_cmd_list().list;
    ofs << "before_cmd_count=" << before_cmd_count << '\n';
    ofs << "after_cmd_count=" << static_cast<int>(cmds.size()) << '\n';
    ofs << "cmd=";
    for (int i = 0; i < static_cast<int>(cmds.size()); i++) {
        if (i != 0) ofs << ", ";
        ofs << cmd_str[static_cast<int>(cmds[i])];
    }
    ofs << '\n';
}

template<size_t MAX_N, int HABA = 3, int K = 2>
void test_iterative_greedy(const State& init_state, int destroy_size = 4){
    write_iter_init_state(init_state);
    out("");
    out("=== 間接検証ケース開始 ===");
    out("destroy_size", destroy_size);
    out(std::string("init_A_perm=") + repeated_state_to_string(init_state.A));
    YstFactory<MAX_N, HABA, K> factory;
    YakinamashiState<MAX_N> yst = factory.build(init_state);
    int before_turn = yst.queries_state.sum_turn;
    IterativeGreedy::run<MAX_N>(
            yst, init_state, 1, -1, -1, -1, IterativeSearchMode::hill_climb
    );
    QueriesStateValidator<MAX_N>::validate_state(init_state, yst.queries_state);
    write_iterative_greedy_result(init_state, yst);
//    typename SwapAllOrdGreedySearch<MAX_N>::Result swap_all_ord_greedy_result =
//            SwapAllOrdGreedySearch<MAX_N>::run(init_state, yst, false);
//    out("swap_all_ord_greedy_result_len", swap_all_ord_greedy_result.cmds.size());
//    out("swap_all_ord_greedy_score", swap_all_ord_greedy_result.score);
    int after_turn = yst.queries_state.sum_turn;
    out("after  sum_turn", after_turn, "diff", after_turn - before_turn);
}

template<size_t MAX_N, int HABA = 3, int K = 2>
void test_iterative_greedy(int destroy_size = 4, int case_n = 1){
    constexpr int TEST_N = 100;
    constexpr int CASE_SEED_BASE = 2302041451;
    constexpr int ONLY_CASE_IDX = -1;
    for (int case_idx = 0; case_idx < case_n; case_idx++) {
        if (ONLY_CASE_IDX >= 0 && case_idx != ONLY_CASE_IDX) continue;
        if(case_idx%100==0){
            cout << "case_idx : "<<case_idx <<  endl;
        }
        clear_file();
        vector<int> perm(TEST_N);
        std::iota(perm.begin(), perm.end(), 0);
        const int seed = CASE_SEED_BASE + case_idx;
        std::mt19937 engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);
        out("");
        out("=== 間接検証ケース開始 ===");
        out("case_idx", case_idx, "seed", seed, "destroy_size", destroy_size);
        out("init_A_perm", perm);
        State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
        set_my_rand_seed(static_cast<uint32_t>(seed));
        set_my_rand_seed(static_cast<uint32_t>(seed));
        test_iterative_greedy<MAX_N, HABA, K>(init_state, destroy_size);


    }
}

// 責務: 指定 seed と init_state で IterativeGreedy を指定回数走らせ、最後の sum_turn を返す。
// 必要な理由: unit 登録あり/なしの最終 score 差を同一条件で測るため。
template<size_t MAX_N, int HABA = 3, int K = 2>
int run_iterative_greedy_owner_unit_score(
        const State& init_state,
        int seed,
        int outer_loop_count,
        bool use_owner_unit_range
) {
    GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
    GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
    GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(use_owner_unit_range);
    GreedyQueriesConstructor<MAX_N>::set_owner_array_enabled(false);
    set_my_rand_seed(static_cast<uint32_t>(seed));
    YstFactory<MAX_N, HABA, K> factory;
    YakinamashiState<MAX_N> yst = factory.build(init_state);
    IterativeGreedy::run<MAX_N>(
            yst, init_state, outer_loop_count, -1, -1, -1, IterativeSearchMode::hill_climb, false
    );
    QueriesStateValidator<MAX_N>::validate_state(init_state, yst.queries_state);
    return yst.queries_state.sum_turn;
}

// 責務: 各 case で同じ init_state と seed を使い、unit score - range score の avg/min/med/max を出力する。
// 必要な理由: unit range 分解の最終手数への影響を、既存 iterative_greedy 経路で比較するため。
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_iterative_greedy_owner_unit_diff(int outer_loop_count = 200, int case_n = 500) {
    constexpr int TEST_N = 100;
    constexpr int CASE_SEED_BASE = 2302041451;
    my_assert(case_n > 0);
    my_vector<int> diffs;
    diffs.reserve(case_n);
    long long diff_sum = 0;
    for (int case_idx = 0; case_idx < case_n; case_idx++) {
        if (case_idx % 100 == 0) {
            cout << "case_idx : " << case_idx << endl;
        }
        vector<int> perm(TEST_N);
        std::iota(perm.begin(), perm.end(), 0);
        const int seed = CASE_SEED_BASE + case_idx;
        std::mt19937 engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);
        State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());

        std::chrono::system_clock::time_point  start, mid, end; // 型は auto で可
        start = std::chrono::system_clock::now();

        const int unit_score = run_iterative_greedy_owner_unit_score<MAX_N, HABA, K>(
                init_state, seed, outer_loop_count, true);
        mid = std::chrono::system_clock::now();

        const int range_score = run_iterative_greedy_owner_unit_score<MAX_N, HABA, K>(
                init_state, seed, outer_loop_count, false);
        end = std::chrono::system_clock::now();
        double elapsed_score = std::chrono::duration_cast<std::chrono::milliseconds>(end-mid).count(); //処理に要した時間をミリ秒に変換
        double elapsed_unit = std::chrono::duration_cast<std::chrono::milliseconds>(mid-start).count(); //処理に要した時間をミリ秒に変換

        const int diff = unit_score - range_score;
        diffs.push_back(diff);
        diff_sum += diff;
        cout << "owner_unit_diff_case"
             << " case_idx " << case_idx
             << " seed " << seed
             << " unit_score " << unit_score
             << " range_score " << range_score
             << " diff " << diff
             << endl;
        cout<<"elapsed_score "<<elapsed_score<<endl;
        cout<<"elapsed_unit "<<elapsed_unit<<endl;
        my_vector<int> current_diffs = diffs;
        std::sort(current_diffs.begin(), current_diffs.end());
        const double current_avg =
                static_cast<double>(diff_sum) / static_cast<double>(current_diffs.size());
        cout << "owner_unit_diff_current_avg " << current_avg << endl;
        cout << "owner_unit_diff_current_min " << current_diffs.front() << endl;
        cout << "owner_unit_diff_current_med " << current_diffs[current_diffs.size() / 2] << endl;
        cout << "owner_unit_diff_current_max " << current_diffs.back() << endl;
    }
    std::sort(diffs.begin(), diffs.end());
    const double avg = static_cast<double>(diff_sum) / static_cast<double>(diffs.size());
    out("owner_unit_diff_avg", avg);
    out("owner_unit_diff_min", diffs.front());
    out("owner_unit_diff_med", diffs[diffs.size() / 2]);
    out("owner_unit_diff_max", diffs.back());
    GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(true);
    GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
}

// 責務: 指定 seed と init_state で IterativeGreedy を走らせ、owner overlap/unit 設定別の最後の sum_turn を返す。
// 必要な理由: map/array 経路や array+unit 経路の速度・score を同じ実行入口で比較するため。
template<size_t MAX_N, int HABA = 3, int K = 2>
int run_iterative_greedy_owner_score(
        const State& init_state,
        int seed,
        int outer_loop_count,
        bool use_owner_array,
        bool use_owner_unit_range = false
) {
    GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(true);
    GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(false);
    GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(use_owner_unit_range);
    GreedyQueriesConstructor<MAX_N>::set_owner_array_enabled(use_owner_array);
    set_my_rand_seed(static_cast<uint32_t>(seed));
    YstFactory<MAX_N, HABA, K> factory;
    YakinamashiState<MAX_N> yst = factory.build(init_state);
    IterativeGreedy::run<MAX_N>(
            yst, init_state, outer_loop_count, -1, -1, -1, IterativeSearchMode::hill_climb, false
    );
    QueriesStateValidator<MAX_N>::validate_state(init_state, yst.queries_state);
    return yst.queries_state.sum_turn;
}

// 責務: 同じ init_state/seed で array 単体と array+unit を別々に実行し、case ごとの score、手数の累積統計、平均 elapsed ms を標準出力へ出す。
// 必要な理由: array 経路に unit range 分解を足したときの手数分布と時間差を、同一条件の実行結果で確認するため。
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_owner_array_unit_compare(int outer_loop_count = 500, int case_n = 200) {
    constexpr int TEST_N = 100;
    constexpr int CASE_SEED_BASE = 2302041451;
    assert(case_n > 0);
    my_vector<int> array_scores;
    my_vector<int> array_unit_scores;
    my_vector<int> score_diffs;
    array_scores.reserve(case_n);
    array_unit_scores.reserve(case_n);
    score_diffs.reserve(case_n);
    long long array_score_sum = 0;
    long long array_unit_score_sum = 0;
    long long score_diff_sum = 0;
    long long array_elapsed_sum = 0;
    long long array_unit_elapsed_sum = 0;
    auto print_int_stats = [](const std::string &label, const my_vector<int> &values, long long sum) {
        my_vector<int> sorted_values = values;
        std::sort(sorted_values.begin(), sorted_values.end());
        const double avg = static_cast<double>(sum) / static_cast<double>(sorted_values.size());
        cout << label << "_sum " << sum << endl;
        cout << label << "_avg " << avg << endl;
        cout << label << "_min " << sorted_values.front() << endl;
        cout << label << "_med " << sorted_values[sorted_values.size() / 2] << endl;
        cout << label << "_max " << sorted_values.back() << endl;
    };
    for (int case_idx = 0; case_idx < case_n; case_idx++) {
        vector<int> perm(TEST_N);
        std::iota(perm.begin(), perm.end(), 0);
        const int seed = CASE_SEED_BASE + case_idx;
        std::mt19937 engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);
        State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());

        const auto array_start = std::chrono::steady_clock::now();
        const int array_score = run_iterative_greedy_owner_score<MAX_N, HABA, K>(
                init_state, seed, outer_loop_count, true, false);
        const auto array_end = std::chrono::steady_clock::now();

        const auto array_unit_start = std::chrono::steady_clock::now();
        const int array_unit_score = run_iterative_greedy_owner_score<MAX_N, HABA, K>(
                init_state, seed, outer_loop_count, true, true);
        const auto array_unit_end = std::chrono::steady_clock::now();

        const long long array_elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(array_end - array_start).count();
        const long long array_unit_elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(array_unit_end - array_unit_start).count();
        const int score_diff = array_unit_score - array_score;
        array_scores.push_back(array_score);
        array_unit_scores.push_back(array_unit_score);
        score_diffs.push_back(score_diff);
        array_score_sum += array_score;
        array_unit_score_sum += array_unit_score;
        score_diff_sum += score_diff;
        array_elapsed_sum += array_elapsed_ms;
        array_unit_elapsed_sum += array_unit_elapsed_ms;
        cout << "owner_array_unit_case"
             << " case_idx " << case_idx
             << " seed " << seed
             << " array_score " << array_score
             << " array_unit_score " << array_unit_score
             << " score_diff " << score_diff
             << " array_elapsed_ms " << array_elapsed_ms
             << " array_unit_elapsed_ms " << array_unit_elapsed_ms
             << endl;
        print_int_stats("owner_array_unit_array_score_current", array_scores, array_score_sum);
        print_int_stats("owner_array_unit_array_unit_score_current", array_unit_scores, array_unit_score_sum);
        print_int_stats("owner_array_unit_score_diff_current", score_diffs, score_diff_sum);
    }
    print_int_stats("owner_array_unit_array_score_summary", array_scores, array_score_sum);
    print_int_stats("owner_array_unit_array_unit_score_summary", array_unit_scores, array_unit_score_sum);
    print_int_stats("owner_array_unit_score_diff_summary", score_diffs, score_diff_sum);
    cout << "owner_array_unit_array_elapsed_avg "
         << static_cast<double>(array_elapsed_sum) / static_cast<double>(case_n)
         << endl;
    cout << "owner_array_unit_array_unit_elapsed_avg "
         << static_cast<double>(array_unit_elapsed_sum) / static_cast<double>(case_n)
         << endl;
    GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(true);
    GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
    GreedyQueriesConstructor<MAX_N>::set_owner_array_enabled(false);
}

// 責務: 同じ init_state/seed で map 経路と array 経路を別々に実行し、score 差分と elapsed ms の累積統計を出力する。
// 必要な理由: 細かい同時比較ではなく、実運用に近い単独経路同士の速度差と score 差分を全 case で測るため。
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_owner_array_speed_compare(int outer_loop_count = 200, int case_n = 200) {
    constexpr int TEST_N = 100;
    constexpr int CASE_SEED_BASE = 2302041451;
    assert(case_n > 0);
    // 責務: owner_array_* の比較結果と統計だけを書き込む出力ファイルを開く。
    // 必要な理由: IterativeGreedy の best_sum/current_sum 進捗ログは標準出力に残しつつ、比較集計を別ファイルで確認できるようにするため。
    std::filesystem::create_directories(output_dir);
    std::ofstream owner_array_log(output_dir / "owner_array_speed_compare.txt", std::ios::out | std::ios::trunc);
    assert(owner_array_log.is_open());
    my_vector<long long> map_elapsed_values;
    my_vector<long long> array_elapsed_values;
    my_vector<long long> elapsed_diffs;
    my_vector<int> map_scores;
    my_vector<int> array_scores;
    my_vector<int> score_diffs;
    map_elapsed_values.reserve(case_n);
    array_elapsed_values.reserve(case_n);
    elapsed_diffs.reserve(case_n);
    map_scores.reserve(case_n);
    array_scores.reserve(case_n);
    score_diffs.reserve(case_n);
    long long map_elapsed_sum = 0;
    long long array_elapsed_sum = 0;
    long long elapsed_diff_sum = 0;
    long long map_score_sum = 0;
    long long array_score_sum = 0;
    long long score_diff_sum = 0;
    auto print_ll_stats = [](std::ostream &out, const std::string &label, const my_vector<long long> &values, long long sum) {
        my_vector<long long> sorted_values = values;
        std::sort(sorted_values.begin(), sorted_values.end());
        const double avg = static_cast<double>(sum) / static_cast<double>(sorted_values.size());
        out << label << "_sum " << sum << endl;
        out << label << "_avg " << avg << endl;
        out << label << "_min " << sorted_values.front() << endl;
        out << label << "_med " << sorted_values[sorted_values.size() / 2] << endl;
        out << label << "_max " << sorted_values.back() << endl;
    };
    auto print_int_stats = [](std::ostream &out, const std::string &label, const my_vector<int> &values, long long sum) {
        my_vector<int> sorted_values = values;
        std::sort(sorted_values.begin(), sorted_values.end());
        const double avg = static_cast<double>(sum) / static_cast<double>(sorted_values.size());
        out << label << "_sum " << sum << endl;
        out << label << "_avg " << avg << endl;
        out << label << "_min " << sorted_values.front() << endl;
        out << label << "_med " << sorted_values[sorted_values.size() / 2] << endl;
        out << label << "_max " << sorted_values.back() << endl;
    };
    // 責務: map/array の score 分布を sum/avg/min/med/max 付きで 1 行出力する。
    // 必要な理由: 個別行のログが流れても、各 case 時点の手数 min/med/max を出していることを出力から確認しやすくするため。
    auto print_score_stats_line = [](
            std::ostream &out,
            const std::string &label,
            const my_vector<int> &map_values,
            long long map_sum,
            const my_vector<int> &array_values,
            long long array_sum
    ) {
        my_vector<int> sorted_map = map_values;
        my_vector<int> sorted_array = array_values;
        std::sort(sorted_map.begin(), sorted_map.end());
        std::sort(sorted_array.begin(), sorted_array.end());
        const double map_avg = static_cast<double>(map_sum) / static_cast<double>(sorted_map.size());
        const double array_avg = static_cast<double>(array_sum) / static_cast<double>(sorted_array.size());
        out << label
             << " map_sum " << map_sum
             << " map_avg " << map_avg
             << " map_min " << sorted_map.front()
             << " map_med " << sorted_map[sorted_map.size() / 2]
             << " map_max " << sorted_map.back()
             << " array_sum " << array_sum
             << " array_avg " << array_avg
             << " array_min " << sorted_array.front()
             << " array_med " << sorted_array[sorted_array.size() / 2]
             << " array_max " << sorted_array.back()
             << endl;
    };
    for (int case_idx = 0; case_idx < case_n; case_idx++) {
        vector<int> perm(TEST_N);
        std::iota(perm.begin(), perm.end(), 0);
        const int seed = CASE_SEED_BASE + case_idx;
        std::mt19937 engine(seed);
        std::shuffle(perm.begin(), perm.end(), engine);
        State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());

        const auto map_start = std::chrono::steady_clock::now();
        const int map_score = run_iterative_greedy_owner_score<MAX_N, HABA, K>(
                init_state, seed, outer_loop_count, false);
        const auto map_end = std::chrono::steady_clock::now();

        const auto array_start = std::chrono::steady_clock::now();
        const int array_score = run_iterative_greedy_owner_score<MAX_N, HABA, K>(
                init_state, seed, outer_loop_count, true);
        const auto array_end = std::chrono::steady_clock::now();

        const long long map_elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(map_end - map_start).count();
        const long long array_elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(array_end - array_start).count();
        const int score_diff = array_score - map_score;
        const long long elapsed_diff_ms = array_elapsed_ms - map_elapsed_ms;
        map_scores.push_back(map_score);
        array_scores.push_back(array_score);
        score_diffs.push_back(score_diff);
        map_elapsed_values.push_back(map_elapsed_ms);
        array_elapsed_values.push_back(array_elapsed_ms);
        elapsed_diffs.push_back(elapsed_diff_ms);
        map_score_sum += map_score;
        array_score_sum += array_score;
        score_diff_sum += score_diff;
        map_elapsed_sum += map_elapsed_ms;
        array_elapsed_sum += array_elapsed_ms;
        elapsed_diff_sum += elapsed_diff_ms;
        owner_array_log << "owner_array_speed_case"
             << " case_idx " << case_idx
             << " seed " << seed
             << " map_score " << map_score
             << " array_score " << array_score
             << " score_diff " << score_diff
             << " map_elapsed_ms " << map_elapsed_ms
             << " array_elapsed_ms " << array_elapsed_ms
             << " elapsed_diff_ms " << elapsed_diff_ms
             << endl;
        print_int_stats(owner_array_log, "owner_array_map_score_current", map_scores, map_score_sum);
        print_int_stats(owner_array_log, "owner_array_array_score_current", array_scores, array_score_sum);
        print_score_stats_line(
                owner_array_log,
                "owner_array_score_current_line",
                map_scores,
                map_score_sum,
                array_scores,
                array_score_sum);
        print_int_stats(owner_array_log, "owner_array_score_diff_current", score_diffs, score_diff_sum);
        print_ll_stats(owner_array_log, "owner_array_map_elapsed_current", map_elapsed_values, map_elapsed_sum);
        print_ll_stats(owner_array_log, "owner_array_array_elapsed_current", array_elapsed_values, array_elapsed_sum);
        print_ll_stats(owner_array_log, "owner_array_elapsed_diff_current", elapsed_diffs, elapsed_diff_sum);
    }
    print_int_stats(owner_array_log, "owner_array_map_score_summary", map_scores, map_score_sum);
    print_int_stats(owner_array_log, "owner_array_array_score_summary", array_scores, array_score_sum);
    print_score_stats_line(
            owner_array_log,
            "owner_array_score_summary_line",
            map_scores,
            map_score_sum,
            array_scores,
            array_score_sum);
    print_int_stats(owner_array_log, "owner_array_score_diff_summary", score_diffs, score_diff_sum);
    print_ll_stats(owner_array_log, "owner_array_map_elapsed_summary", map_elapsed_values, map_elapsed_sum);
    print_ll_stats(owner_array_log, "owner_array_array_elapsed_summary", array_elapsed_values, array_elapsed_sum);
    print_ll_stats(owner_array_log, "owner_array_elapsed_diff_summary", elapsed_diffs, elapsed_diff_sum);
    GreedyQueriesConstructor<MAX_N>::set_owner_unit_range_enabled(true);
    GreedyQueriesConstructor<MAX_N>::set_owner_check_enabled(true);
    GreedyQueriesConstructor<MAX_N>::set_owner_collector_enabled(false);
    GreedyQueriesConstructor<MAX_N>::set_owner_array_enabled(false);
}

// 責務: N=100 の固定 seed 入力から初期 YakinamashiState を作り、N=60 へ縮小してから 100 loop ごとに v を戻す実験を起動する。
// 必要な理由: 新しく追加した run_progressive_restore を通常実行経路から直接検証できるようにするため。
template<size_t MAX_N, int HABA = 3, int K = 2>
void test_progressive_restore_greedy() {
    constexpr int TEST_N = 100;
    constexpr int CASE_SEED = 2026041451;
    constexpr int OUTER_LOOP_COUNT = 1 << 30;
    vector<int> perm(TEST_N);
    std::iota(perm.begin(), perm.end(), 0);
    std::mt19937 engine(CASE_SEED);
    std::shuffle(perm.begin(), perm.end(), engine);
    perm = {36, 37, 41, 85, 84, 13, 33, 43, 76, 78, 92, 61, 22, 10, 83, 81, 89, 34, 90, 29, 45, 55, 51, 86, 50, 68, 95, 94, 25, 79, 75, 42, 96, 97, 4, 64, 52, 11, 47, 91, 28, 14, 60, 35, 87, 57, 19, 99, 69, 53, 48, 59, 1, 98, 7, 8, 24, 65, 9, 88, 12, 38, 21, 31, 58, 0, 17, 82, 77, 27, 66, 40, 71, 72, 93, 5, 80, 26, 18, 20, 3, 30, 63, 44, 54, 62, 6, 70, 67, 56, 16, 39, 15, 74, 2, 23, 32, 46, 73, 49};
    State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
    set_my_rand_seed(static_cast<uint32_t>(CASE_SEED));
    out("");
    out("=== progressive restore iterative greedy 開始 ===");
    out("seed", CASE_SEED);
    out("init_A_perm", perm);
    YstFactory<MAX_N, HABA, K> factory;
    YakinamashiState<MAX_N> yst = factory.build(init_state);
    const int before_turn = yst.queries_state.sum_turn;
    ProgressiveRestoreConfig config{70, 50};
    IterativeGreedy::run_progressive_restore<MAX_N>(
            yst, init_state, OUTER_LOOP_COUNT, -1, -1, -1, config, IterativeSearchMode::hill_climb
    );
    QueriesStateValidator<MAX_N>::validate_state(init_state, yst.queries_state);
    const int after_turn = yst.queries_state.sum_turn;
    out("progressive_restore_after_sum_turn", after_turn, "diff", after_turn - before_turn);
}

// 責務: argv 内に指定された flag が存在するか判定する。
// 必要な理由: 通常実行と Optuna の 1 trial 実行を切り替えるため。
bool has_arg(int argc, char *argv[], const std::string& flag) {
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == flag) return true;
    }
    return false;
}

// 責務: argv から key に対応する次要素の文字列値を探す。
// 必要な理由: --top_k 4 のような Optuna 指定値を個別に取り出すため。
std::optional<std::string> find_arg_value(int argc, char *argv[], const std::string& key) {
    for (int i = 1; i + 1 < argc; i++) {
        if (std::string(argv[i]) == key) return std::string(argv[i + 1]);
    }
    return std::nullopt;
}

// 責務: 指定 key の CLI 値を必須 int として parse する。
// 必要な理由: 欠落した trial 設定を検出し、C++ 側へ不完全な params を渡さないため。
int parse_required_int(int argc, char *argv[], const std::string& key) {
    std::optional<std::string> value = find_arg_value(argc, argv, key);
    my_assert(value.has_value());
    int parsed_value = std::stoi(value.value());
    return parsed_value;
}

// 責務: 指定 key の CLI 値を任意 int として parse する。
// 必要な理由: option 未指定時は既存デフォルトを保ち、指定時だけ ALNS 受理設定を変えるため。
std::optional<int> parse_optional_int(int argc, char *argv[], const std::string& key) {
    std::optional<std::string> value = find_arg_value(argc, argv, key);
    if (!value.has_value()) return std::nullopt;
    return std::stoi(value.value());
}

// 責務: argv から ALNS 受理パラメータを構築する。
// 必要な理由: --ALNS_LAHC_HISTORY_SIZE と --ALNS_WORSEN_LIMIT を通常実行/Optuna 実行の両方で使うため。
AlnsAcceptParams parse_alns_accept_params(int argc, char *argv[]) {
    AlnsAcceptParams params;
    std::optional<int> lahc_history_size = parse_optional_int(argc, argv, "--ALNS_LAHC_HISTORY_SIZE");
    std::optional<int> worsen_limit = parse_optional_int(argc, argv, "--ALNS_WORSEN_LIMIT");
    if (lahc_history_size.has_value()) params.lahc_history_size = lahc_history_size.value();
    if (worsen_limit.has_value()) params.worsen_limit = worsen_limit.value();
    my_assert(0 < params.lahc_history_size);
    my_assert(0 <= params.worsen_limit);
    return params;
}

// 責務: argv から GreedyConstructorParams を構築する。
// 必要な理由: Python 側の trial params を GreedyQueriesConstructor へまとめて渡すため。
GreedyConstructorParams parse_greedy_params(int argc, char *argv[]) {
    GreedyConstructorParams params{
        parse_required_int(argc, argv, "--top_k"),
        parse_required_int(argc, argv, "--from_ext_a"),
        parse_required_int(argc, argv, "--to_ext_a"),
        parse_required_int(argc, argv, "--from_ext_b"),
        parse_required_int(argc, argv, "--to_ext_b"),
        parse_required_int(argc, argv, "--a2b_lim"),
        parse_required_int(argc, argv, "--b2a_lim"),
        parse_required_int(argc, argv, "--destroy_mask")
    };
    my_assert(1 <= params.destroy_mask && params.destroy_mask <= 3);
    return params;
}
int run_optuna_case100(
    const GreedyConstructorParams& params, bool show_alns_log, bool show_alns_file_log
) {
    constexpr int COMPARE_TEST_N = 100;
    constexpr int COMPARE_SEED = 2026041451;
    vector<int> compare_perm(COMPARE_TEST_N);
    std::iota(compare_perm.begin(), compare_perm.end(), 0);
    std::mt19937 compare_engine(COMPARE_SEED);
    std::shuffle(compare_perm.begin(), compare_perm.end(), compare_engine);
//    compare_perm = {149, 359, 60, 94, 451, 489, 392, 439, 63, 417, 430, 164, 222, 91, 346, 30, 490, 125, 29, 204, 74, 73, 497, 35, 66, 354, 308, 282, 306, 176, 300, 185, 189, 482, 297, 305, 476, 285, 401, 494, 172, 144, 97, 122, 174, 316, 440, 342, 273, 207, 254, 83, 459, 227, 416, 92, 480, 369, 263, 370, 433, 406, 410, 12, 43, 10, 345, 140, 375, 347, 445, 65, 281, 130, 180, 95, 441, 98, 319, 418, 181, 458, 153, 142, 460, 160, 102, 85, 88, 289, 266, 19, 404, 448, 366, 138, 469, 377, 239, 156, 296, 212, 4, 115, 16, 260, 265, 478, 295, 36, 457, 390, 237, 150, 228, 119, 192, 3, 179, 395, 9, 166, 465, 146, 373, 412, 195, 394, 62, 218, 196, 26, 76, 114, 328, 277, 232, 159, 481, 486, 234, 407, 391, 472, 386, 1, 413, 315, 358, 350, 152, 470, 64, 137, 437, 327, 186, 241, 444, 224, 213, 194, 201, 280, 154, 256, 291, 107, 45, 422, 442, 491, 344, 384, 343, 380, 348, 84, 151, 269, 170, 168, 44, 240, 79, 372, 261, 32, 443, 405, 127, 190, 324, 493, 80, 467, 55, 301, 250, 409, 379, 499, 50, 279, 99, 423, 145, 446, 68, 133, 475, 322, 326, 132, 332, 167, 40, 299, 286, 28, 147, 118, 258, 24, 310, 431, 143, 271, 488, 340, 48, 135, 52, 141, 453, 69, 54, 267, 396, 292, 252, 318, 206, 251, 246, 210, 187, 382, 2, 233, 388, 205, 199, 466, 131, 6, 128, 236, 47, 162, 215, 38, 248, 414, 249, 123, 495, 485, 198, 284, 175, 58, 247, 432, 57, 11, 357, 178, 161, 41, 436, 335, 226, 67, 211, 214, 202, 421, 355, 387, 474, 321, 399, 112, 397, 219, 383, 184, 429, 463, 110, 381, 223, 7, 46, 303, 165, 427, 426, 183, 173, 20, 93, 255, 72, 197, 424, 400, 14, 378, 81, 109, 96, 323, 25, 408, 349, 338, 371, 51, 208, 496, 293, 449, 376, 61, 420, 117, 333, 330, 352, 264, 477, 134, 136, 37, 312, 341, 22, 89, 18, 217, 259, 434, 419, 121, 484, 360, 389, 415, 311, 278, 356, 87, 105, 169, 334, 287, 245, 454, 447, 362, 288, 235, 191, 452, 21, 450, 374, 71, 101, 456, 27, 309, 231, 100, 82, 120, 49, 471, 42, 13, 411, 403, 158, 77, 361, 116, 498, 304, 242, 298, 221, 325, 56, 230, 393, 0, 468, 313, 104, 200, 171, 294, 365, 435, 15, 283, 225, 329, 124, 148, 220, 139, 302, 492, 243, 317, 59, 363, 90, 155, 320, 487, 455, 337, 203, 157, 275, 209, 464, 368, 367, 479, 216, 402, 272, 462, 17, 126, 290, 244, 34, 353, 31, 33, 339, 229, 39, 438, 163, 276, 274, 307, 270, 473, 238, 106, 428, 177, 182, 351, 193, 257, 314, 75, 253, 364, 8, 398, 262, 331, 336, 86, 483, 461, 268, 108, 425, 103, 111, 23, 385, 188, 113, 53, 70, 78, 129, 5};
    State compare_init_state(deque<int>(compare_perm.begin(), compare_perm.end()), deque<int>());
    set_my_rand_seed(static_cast<uint32_t>(COMPARE_SEED));
    constexpr int ITER_N = 1 << 30;
    constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
    constexpr int TIME_LIMIT_SEC = 120;
    YstFactory<100, 300, 2> factory;
    auto build_result = factory.build_with_score(compare_init_state, false, false);
    YakinamashiState<100> current_yst = std::move(build_result.yst);
    int final_score = run_alns_destroy_construct<100, 3000, 2>(
            compare_init_state, current_yst, ITER_N, ACCEPT_MODE, params, TIME_LIMIT_SEC,
            show_alns_log,
            show_alns_file_log
    );
    return final_score;
}


// 責務: 固定 seed の比較 state を作り、指定 params で ALNS を 1 回走らせて最終 score を返す。
// 必要な理由: Python/Optuna が FINAL_SCORE を objective として読める C++ 側入口を用意するため。
int run_optuna_case500(
    const GreedyConstructorParams& params, bool show_alns_log, bool show_alns_file_log
) {
    constexpr int COMPARE_TEST_N = 500;
    constexpr int COMPARE_SEED = 2026041451;
    vector<int> compare_perm(COMPARE_TEST_N);
    std::iota(compare_perm.begin(), compare_perm.end(), 0);
    std::mt19937 compare_engine(COMPARE_SEED);
    std::shuffle(compare_perm.begin(), compare_perm.end(), compare_engine);
    compare_perm = {149, 359, 60, 94, 451, 489, 392, 439, 63, 417, 430, 164, 222, 91, 346, 30, 490, 125, 29, 204, 74, 73, 497, 35, 66, 354, 308, 282, 306, 176, 300, 185, 189, 482, 297, 305, 476, 285, 401, 494, 172, 144, 97, 122, 174, 316, 440, 342, 273, 207, 254, 83, 459, 227, 416, 92, 480, 369, 263, 370, 433, 406, 410, 12, 43, 10, 345, 140, 375, 347, 445, 65, 281, 130, 180, 95, 441, 98, 319, 418, 181, 458, 153, 142, 460, 160, 102, 85, 88, 289, 266, 19, 404, 448, 366, 138, 469, 377, 239, 156, 296, 212, 4, 115, 16, 260, 265, 478, 295, 36, 457, 390, 237, 150, 228, 119, 192, 3, 179, 395, 9, 166, 465, 146, 373, 412, 195, 394, 62, 218, 196, 26, 76, 114, 328, 277, 232, 159, 481, 486, 234, 407, 391, 472, 386, 1, 413, 315, 358, 350, 152, 470, 64, 137, 437, 327, 186, 241, 444, 224, 213, 194, 201, 280, 154, 256, 291, 107, 45, 422, 442, 491, 344, 384, 343, 380, 348, 84, 151, 269, 170, 168, 44, 240, 79, 372, 261, 32, 443, 405, 127, 190, 324, 493, 80, 467, 55, 301, 250, 409, 379, 499, 50, 279, 99, 423, 145, 446, 68, 133, 475, 322, 326, 132, 332, 167, 40, 299, 286, 28, 147, 118, 258, 24, 310, 431, 143, 271, 488, 340, 48, 135, 52, 141, 453, 69, 54, 267, 396, 292, 252, 318, 206, 251, 246, 210, 187, 382, 2, 233, 388, 205, 199, 466, 131, 6, 128, 236, 47, 162, 215, 38, 248, 414, 249, 123, 495, 485, 198, 284, 175, 58, 247, 432, 57, 11, 357, 178, 161, 41, 436, 335, 226, 67, 211, 214, 202, 421, 355, 387, 474, 321, 399, 112, 397, 219, 383, 184, 429, 463, 110, 381, 223, 7, 46, 303, 165, 427, 426, 183, 173, 20, 93, 255, 72, 197, 424, 400, 14, 378, 81, 109, 96, 323, 25, 408, 349, 338, 371, 51, 208, 496, 293, 449, 376, 61, 420, 117, 333, 330, 352, 264, 477, 134, 136, 37, 312, 341, 22, 89, 18, 217, 259, 434, 419, 121, 484, 360, 389, 415, 311, 278, 356, 87, 105, 169, 334, 287, 245, 454, 447, 362, 288, 235, 191, 452, 21, 450, 374, 71, 101, 456, 27, 309, 231, 100, 82, 120, 49, 471, 42, 13, 411, 403, 158, 77, 361, 116, 498, 304, 242, 298, 221, 325, 56, 230, 393, 0, 468, 313, 104, 200, 171, 294, 365, 435, 15, 283, 225, 329, 124, 148, 220, 139, 302, 492, 243, 317, 59, 363, 90, 155, 320, 487, 455, 337, 203, 157, 275, 209, 464, 368, 367, 479, 216, 402, 272, 462, 17, 126, 290, 244, 34, 353, 31, 33, 339, 229, 39, 438, 163, 276, 274, 307, 270, 473, 238, 106, 428, 177, 182, 351, 193, 257, 314, 75, 253, 364, 8, 398, 262, 331, 336, 86, 483, 461, 268, 108, 425, 103, 111, 23, 385, 188, 113, 53, 70, 78, 129, 5};
    State compare_init_state(deque<int>(compare_perm.begin(), compare_perm.end()), deque<int>());
    set_my_rand_seed(static_cast<uint32_t>(COMPARE_SEED));
    constexpr int ITER_N = 1 << 30;
    constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
    constexpr int TIME_LIMIT_SEC = 120;
    YstFactory<501, 300, 2> factory;
    auto build_result = factory.build_with_score(compare_init_state, false, false);
    YakinamashiState<501> current_yst = std::move(build_result.yst);
    int final_score = run_alns_destroy_construct<501, 300, 2>(
        compare_init_state, current_yst, ITER_N, ACCEPT_MODE, params, TIME_LIMIT_SEC,
        show_alns_log,
        show_alns_file_log
    );
    return final_score;
}

// 責務: 固定 constructor 条件で 10 個の N=100 init_state を 500 iter ずつ評価し、score median を返す。
// 必要な理由: ALNS_LAHC_HISTORY_SIZE と ALNS_WORSEN_LIMIT だけを Optuna の objective にするため。
double run_optuna_lahc_params_case100(bool show_alns_log, bool show_alns_file_log) {
    constexpr int CASE_COUNT = 10;
    constexpr int INIT_BASE_SEED = 2026041451;
    constexpr int ALNS_SEED = 2026041451;
    constexpr int ITER_COUNT = 500;
    constexpr int TEST_N = 100;
    constexpr int NO_TIME_LIMIT_SEC = -1;
    constexpr AlnsAcceptMode ACCEPT_MODE = ALNS_ACCEPT_MODE;
    GreedyConstructorParams params{10, 5, 5, 5, 5, 20, 15, 3};
    vector<int> scores;
    scores.reserve(CASE_COUNT);
    for (int case_idx = 0; case_idx < CASE_COUNT; case_idx++) {
        set_my_rand_seed(static_cast<uint32_t>(INIT_BASE_SEED + case_idx));
        std::vector<int> perm = build_repeated_test_perm(TEST_N);
        State init_state = build_repeated_test_state(perm);
        GreedyQueriesConstructor<100>::set_owner_collector_enabled(true);
        GreedyQueriesConstructor<100>::set_owner_check_enabled(false);
        GreedyQueriesConstructor<100>::set_owner_array_enabled(true);
        GreedyQueriesConstructor<100>::set_owner_unit_range_enabled(true);
        GreedyQueriesConstructor<100>::set_owner_even_unit_enabled(true);
        GreedyQueriesConstructor<100>::set_pair_refine_enabled(true);
        GreedyQueriesConstructor<100>::set_pair_refine_params(6, 1, 1, 3);
        set_worst_half_enabled(false);
        set_b_move_enabled(false);
        set_aorder_range_enabled(false);
        set_my_rand_seed(static_cast<uint32_t>(ALNS_SEED));
        YstFactory<100, 3000, 2> factory;
        YakinamashiState<100> current_yst = factory.build(init_state);
        const int score = run_alns_destroy_construct<100, 3000, 2>(
            init_state,
            current_yst,
            ITER_COUNT,
            ACCEPT_MODE,
            params,
            NO_TIME_LIMIT_SEC,
            show_alns_log,
            show_alns_file_log
        );
        scores.push_back(score);
        cout << "OPTUNA_LAHC_CASE"
             << " case=" << case_idx
             << " score=" << score
             << endl;
    }
    const ScoreStats stats = calc_score_stats(scores);
    cout << "OPTUNA_LAHC_SUMMARY"
         << " cases=" << CASE_COUNT
         << " min=" << stats.min_score
         << " median=" << stats.median_score
         << " avg=" << stats.avg_score
         << " max=" << stats.max_score
         << endl;
    set_aorder_range_enabled(false);
    set_b_move_enabled(false);
    set_worst_half_enabled(false);
    GreedyQueriesConstructor<100>::set_owner_array_enabled(false);
    GreedyQueriesConstructor<100>::set_owner_unit_range_enabled(true);
    GreedyQueriesConstructor<100>::set_owner_even_unit_enabled(false);
    GreedyQueriesConstructor<100>::set_pair_refine_enabled(false);
    GreedyQueriesConstructor<100>::set_owner_collector_enabled(false);
    GreedyQueriesConstructor<100>::set_owner_check_enabled(true);
    return stats.median_score;
}

class G{
    string st;
public:
//    G() : st(""){
//
//    }

    G(string s) : st(s){

    }
    G& operator=(const string& other) {
        st = other;
        return *this;
    }
};

// human_review_begin: nafuka tester が渡す argv の stack A を解き、command 列だけを stdout に出すため。
// 責務: push_swap CLI 実行中だけ cout を退避し、既存調査ログが checker 入力に混ざらないようにする。
// 必要な理由: nafuka tester は push_swap の stdout 全体を checker stdin へ渡すため。
class CoutSilencer {
    std::streambuf* old_buf;
    std::ostringstream sink;
public:
    CoutSilencer() : old_buf(std::cout.rdbuf(sink.rdbuf())) {}

    void restore() {
        if (old_buf == nullptr) return;
        std::cout.rdbuf(old_buf);
        old_buf = nullptr;
    }

    ~CoutSilencer() {
        restore();
    }
};
// human_review_end

// human_review_begin: 単一文字列 argv を通常の token 列へ変換するため。
// 責務: "3 2 1" 形式の 1 引数を空白区切り token に分解する。
// 必要な理由: checker/42 互換入力では stack A が 1 つの文字列で渡されることがあるため。
bool split_cli_arg(const std::string& arg, std::vector<std::string>& tokens) {
    std::istringstream iss(arg);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return !tokens.empty();
}
// human_review_end

// human_review_begin: push_swap CLI の各 token を int として検証するため。
// 責務: 1 token が int 範囲内の整数として完全に読めるか検証し、値へ変換する。
// 必要な理由: State 化前に不正な数値入力を Error として扱うため。
bool parse_cli_value(const std::string& token, int& value) {
    if (token.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    long parsed = std::strtol(token.c_str(), &end, 10);
    if (end == token.c_str() || *end != '\0') {
        return false;
    }
    if (errno == ERANGE) {
        return false;
    }
    if (parsed < std::numeric_limits<int>::min() || std::numeric_limits<int>::max() < parsed) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}
// human_review_end

struct ParsedCliInput {
    CliSolveOptions options;
    std::vector<std::string> value_tokens;
};

void append_split_cli_tokens(const std::string& arg, std::vector<std::string>& tokens) {
    std::istringstream iss(arg);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
}

bool parse_positive_cli_option_value(const std::string& value_token, int& value) {
    if (!parse_cli_value(value_token, value)) {
        return false;
    }
    return value > 0;
}

bool parse_cli_key_value_option(
        const std::string& token,
        const std::string& key,
        std::optional<int>& target
) {
    const std::string prefix = key + "=";
    if (token.rfind(prefix, 0) != 0) {
        return false;
    }
    if (target.has_value()) {
        return false;
    }
    int value = 0;
    if (!parse_positive_cli_option_value(token.substr(prefix.size()), value)) {
        return false;
    }
    target = value;
    return true;
}

std::optional<ParsedCliInput> parse_cli_input(int argc, char *argv[]) {
    my_assert(argc > 1);
    std::vector<std::string> tokens;
    for (int i = 1; i < argc; i++) {
        append_split_cli_tokens(argv[i], tokens);
    }
    if (tokens.empty()) {
        return std::nullopt;
    }

    ParsedCliInput parsed;
    for (const std::string& token : tokens) {
        if (token == "--deep") {
            if (parsed.options.deep) {
                return std::nullopt;
            }
            parsed.options.deep = true;
        } else if (token.rfind("--opt-range=", 0) == 0) {
            if (!parse_cli_key_value_option(token, "--opt-range", parsed.options.opt_range)) {
                return std::nullopt;
            }
        } else if (token.rfind("--iter-count=", 0) == 0) {
            if (!parse_cli_key_value_option(token, "--iter-count", parsed.options.iter_count)) {
                return std::nullopt;
            }
        } else if (token.rfind("--", 0) == 0) {
            return std::nullopt;
        } else {
            parsed.value_tokens.push_back(token);
        }
    }

    if (!parsed.options.deep &&
        (parsed.options.opt_range.has_value() || parsed.options.iter_count.has_value())) {
        return std::nullopt;
    }
    if (parsed.value_tokens.empty()) {
        return std::nullopt;
    }
    return parsed;
}

// human_review_begin: nafuka tester と checker 互換 argv の stack A を検証するため。
// 責務: 複数 argv または単一文字列 argv を検証し、int 配列へ変換する。
// 必要な理由: tester/42 形式では stack A の渡され方が複数 argv と単一文字列の両方あり得るため。
std::optional<std::vector<int>> parse_cli_args(const std::vector<std::string>& tokens) {
    std::vector<int> values;
    values.reserve(tokens.size());
    std::set<int> seen;
    for (const std::string& token : tokens) {
        int value = 0;
        if (!parse_cli_value(token, value)) {
            return std::nullopt;
        }
        if (!seen.insert(value).second) {
            return std::nullopt;
        }
        values.push_back(value);
    }
    return values;
}
// human_review_end

// human_review_begin: nafuka tester が渡す argv を既存 solver 用 State に変換するため。
// 責務: 検証済み argv を内部用の 0 始まり圧縮値 State に変換し、失敗時は nullopt を返す。
// 必要な理由: 既存 solver は値の相対順序を 0..N-1 の compressed value として扱うため。
std::optional<State> build_cli_state(const std::vector<std::string>& value_tokens) {
    std::optional<std::vector<int>> values = parse_cli_args(value_tokens);
    if (!values.has_value()) {
        return std::nullopt;
    }
    auto [comp, _] = compress<deque<int>>(*values);
    return State(comp, deque<int>());
}
// human_review_end

// human_review_begin: nafuka tester が checker に渡せる command 形式で出力するため。
// 責務: 最終 command list を checker が読める 1 行 1 command 形式で stdout へ出す。
// 必要な理由: nafuka tester は push_swap stdout をそのまま checker stdin に渡すため。
void print_cli_cmds(const State& optimized_state) {
    const vector<command>& cmds = optimized_state.get_cmd_list().list;
    for (command cmd: cmds) {
        std::cout << cmd_str[static_cast<int>(cmd)] << '\n';
    }
}
// human_review_end

// human_review_begin: 引数あり実行を nafuka tester 形式の push_swap CLI として処理するため。
// 責務: argv 形式の push_swap 入力を N=100/N=500 の既存 owner 処理へ流し、command 列だけを出力する。
// 必要な理由: tester 形式では実験ループではなく 1 入力に対する解だけを返す必要があるため。
int solve_push_swap_cli(int argc, char *argv[]) {
    std::optional<ParsedCliInput> maybe_input = parse_cli_input(argc, argv);
    if (!maybe_input.has_value()) {
        std::cerr << "Error\n";
        return 1;
    }
    const ParsedCliInput& input = maybe_input.value();
    std::optional<State> maybe_initial_state = build_cli_state(input.value_tokens);
    if (!maybe_initial_state.has_value()) {
        std::cerr << "Error\n";
        return 1;
    }
    State initial_state = *maybe_initial_state;
    if (initial_state.current_N <= 0 || initial_state.current_N > 500) {
        std::cerr << "Error\n";
        return 1;
    }
    init(initial_state.current_N);
    set_optimize_range_log_enabled(false);
    set_alns_sum_log(false);
    State optimized_state = [&]() {
        /*std::cout << "cli_debug current_N=" << initial_state.current_N
                  << " deep=" << input.options.deep
                  << " opt_range=";
        if (input.options.opt_range.has_value()) {
            std::cout << input.options.opt_range.value();
        } else {
            std::cout << "default";
        }
        std::cout << " iter_count=";
        if (input.options.iter_count.has_value()) {
            std::cout << input.options.iter_count.value();
        } else {
            std::cout << "default";
        }
        std::cout << '\n';
        std::cout << "cli_debug initial_state=" << initial_state << '\n';
*/
        CoutSilencer silencer;
        if (initial_state.current_N <= 100) {
            if (input.options.deep) {
                silencer.restore();
                std::cout << "cli_debug route=solve_owner_unit_100_deep\n";
                CoutSilencer route_silencer;
                return solve_owner_unit_100_deep<100, 3000, 2>(initial_state, input.options);
            }
            silencer.restore();
            std::cout << "cli_debug route=solve_owner_unit_100\n";
            CoutSilencer route_silencer;
            return solve_owner_unit_100<100, 3000, 2>(initial_state);
        }
        if (input.options.deep) {
            silencer.restore();
            std::cout << "cli_debug route=solve_owner_unit_500_deep\n";
            CoutSilencer route_silencer;
            return solve_owner_unit_500_deep<501, 300, 2>(initial_state, input.options);
        }
        silencer.restore();
        std::cout << "cli_debug route=solve_owner_unit_500\n";
        CoutSilencer route_silencer;
        return solve_owner_unit_500<501, 300, 2>(initial_state);
    }();
    print_cli_cmds(optimized_state);
    return 0;
}
// human_review_end


int main(int argc, char *argv[]) {
    // human_review_begin: 引数あり実行を nafuka tester 形式の push_swap CLI として扱うため。
    if (argc > 1) {
        return solve_push_swap_cli(argc, argv);
    }else{
        //todo 引数が必要だとエラーを出す
        return 0;
    }
    // human_review_end
//    {
//        init(500);
//        init_output(argc, argv);
//        vector<int> perm ={
//                111, 114, 467, 182, 499, 490, 82, 191, 263, 113, 387, 459, 110, 69, 305, 341, 314, 173, 419, 64, 234, 442, 238, 87, 484, 311, 296, 117, 471, 12, 239, 60, 106, 445, 134, 9, 219, 220, 32, 23, 391, 316, 202, 244, 258, 417, 469, 104, 51, 146, 335, 436, 496, 325, 177, 34, 217, 156, 494, 83, 461, 380, 292, 154, 450, 430, 337, 212, 148, 318, 324, 466, 127, 16, 13, 8, 200, 397, 339, 160, 485, 327, 102, 435, 272, 172, 249, 298, 331, 460, 404, 6, 126, 336, 375, 195, 86, 409, 205, 479, 75, 357, 91, 143, 210, 369, 365, 416, 396, 54, 453, 340, 1, 412, 180, 262, 133, 317, 268, 304, 463, 405, 163, 279, 353, 52, 361, 312, 41, 135, 43, 213, 118, 73, 285, 329, 260, 140, 489, 19, 78, 492, 360, 407, 403, 338, 395, 209, 185, 250, 10, 151, 17, 116, 428, 30, 451, 358, 482, 400, 344, 176, 138, 15, 478, 132, 297, 386, 271, 301, 477, 211, 282, 121, 0, 62, 203, 42, 74, 456, 7, 71, 402, 65, 303, 120, 144, 248, 18, 332, 124, 389, 257, 56, 168, 29, 328, 92, 3, 441, 251, 103, 57, 370, 320, 241, 289, 408, 398, 427, 101, 37, 76, 346, 192, 233, 232, 255, 323, 424, 476, 401, 426, 68, 295, 247, 438, 58, 364, 440, 432, 385, 119, 35, 188, 342, 44, 423, 393, 206, 256, 474, 294, 14, 28, 228, 97, 322, 354, 125, 150, 287, 309, 223, 372, 420, 193, 224, 455, 112, 153, 491, 24, 131, 356, 183, 449, 306, 462, 175, 355, 61, 281, 130, 231, 194, 31, 225, 157, 486, 215, 79, 2, 40, 418, 345, 425, 464, 109, 422, 488, 458, 229, 145, 178, 216, 392, 283, 90, 300, 290, 293, 497, 437, 147, 25, 275, 348, 421, 48, 439, 46, 448, 152, 108, 267, 334, 410, 33, 444, 136, 105, 63, 142, 379, 236, 21, 349, 222, 368, 187, 165, 137, 201, 169, 414, 415, 159, 221, 481, 198, 139, 443, 189, 226, 85, 227, 39, 252, 184, 141, 273, 128, 53, 171, 67, 280, 376, 378, 240, 413, 94, 472, 388, 155, 307, 47, 167, 269, 88, 431, 197, 243, 100, 59, 4, 174, 270, 107, 347, 77, 302, 123, 96, 38, 186, 326, 363, 475, 429, 190, 333, 343, 434, 465, 483, 230, 399, 81, 493, 129, 457, 319, 66, 487, 196, 49, 158, 288, 72, 170, 162, 330, 95, 237, 245, 362, 99, 276, 254, 447, 207, 181, 382, 351, 80, 350, 284, 374, 274, 384, 313, 498, 265, 199, 473, 264, 214, 377, 381, 161, 299, 454, 495, 277, 235, 480, 367, 446, 253, 98, 27, 468, 45, 286, 411, 359, 115, 266, 36, 406, 310, 291, 261, 452, 315, 390, 204, 373, 383, 321, 84, 366, 242, 5, 164, 55, 11, 371, 50, 149, 26, 89, 93, 470, 122, 246, 308, 352, 278, 20, 22, 70, 394, 208, 166, 179, 218, 259, 433
//        };
//        State init_state(deque<int>(perm.begin(), perm.end()), deque<int>());
//        vector<command> cmds = {
//                PB, PB, RRA, RRA, PB, RRA, PB, RRR, PB, RA, RR, PB, RR, RR, PB, PB, RA, RRB, PB, RR, RR, PB, RR, RR, RR, RR, PB, RRA, PB, RA, RA, PB, RR, PB, RR, RR, PB, PB, RA, RA, RA, RRB, PB, RA, RA, RA, RA, RR, PB, PB, RA, RR, RR, PB, PB, RR, PB, RRB, RRB, RRR, PB, RA, RA, RA, RA, RR, RR, RR, RR, PB, PB, RA, PB, PB, RR, PB, RB, RR, RR, PB, RRB, PB, RA, RA, RR, PB, RB, RR, PB, PB, RA, RRB, PB, RA, RA, RR, PB, RA, RA, PB, RB, RB, RR, PB, RA, RA, PB, RA, PB, RB, PB, PB, RRB, RRR, PB, RA, RA, RA, PB, RA, RA, PB, RRB, PB, RA, PB, RA, RA, RA, PB, RA, PB, RR, RR, RR, RR, RR, RR, RR, RR, RR, PB, RR, RR, PB, RA, PB, RA, RA, RA, PB, RA, RA, RR, PB, RA, RA, RA, RRB, RRB, PB, RR, RR, PB, PB, PB, RA, RA, RA, RA, RA, RA, RA, RA, RA, RA, RRB, RRB, PB, RR, PB, RRA, PB, RA, RA, RA, RA, RA, RA, RA, RA, RA, RA, RA, PB, RB, RR, RR, RR, RR, PB, RA, PB, RB, PB, RA, RA, PB, RRB, RRB, RRB, RRR, PB, RA, RA, RR, PB, RA, RR, RR, RR, RR, RR, RR, PB, RA, RRB, RRB, PB, PB, RR, PB, RA, RA, PB, RA, PB, RA, PB, RA, RA, RA, RRB, PB, RA, RR, RR, PB, PB, RB, PB, RA, RA, RA, RA, RA, RA, RA, PB, RB, PB, RB, PB, RB, RB, PB, PB, RA, RA, RA, RA, RA, PB, RRB, RRB, RRB, PB, RA, RA, RA, RA, RA, RA, RA, RA, PB, RR, RR, RR, RR, PB, RB, PB, RA, PB, RA, RA, PB, RR, PB, RA, RA, RA, PB, PB, PB, RA, RA, PB, PB, RA, RA, RA, RA, RR, RR, RR, PB, RR, PB, RA, PB, RA, RR, PB, RA, PB, RRB, PB, RR, PB, RR, RR, PB, RA, RA, RA, RA, RA, RA, RA, RA, RA, RR, RR, PB, RR, PB, PB, RR, PB, PB, RB, PB, PB, PB, RA, RA, RA, RA, PB, RA, RA, RA, RA, RA, RA, RA, RA, PB, RA, RA, RA, RA, RA, RA, RA, RA, RA, PB, RA, RA, PB, RA, PB, RA, RR, PB, RA, RR, RR, RR, PB, RR, PB, RA, RR, PB, PB, RR, PB, RR, PB, PB, RR, PB, PB, RA, PB, RR, PB, RA, RA, PB, RA, RA, RA, RA, RR, PB, RR, RR, PB, RA, RA, RRB, RRB, PB, RB, PB, RA, RR, RR, RR, PB, RA, RA, RA, RRB, PB, RA, RA, RA, RA, RRB, PB, RA, RA, RA, RA, PB, RA, RA, RA, RA, PB, RA, RA, RA, RA, RR, PB, RA, RA, PB, RA, RR, PB, RA, PB, RR, PB, RA, PB, PB, RR, RR, PB, RA, RA, RRB, PB, RA, RA, RR, RR, RR, PB, RA, PB, RA, RRB, PB, RA, PB, PB, RR, PB, PB, RA, RA, RA, PB, RA, RA, PB, RRB, PB, RR, RR, RR, PB, RA, RA, RA, RA, PB, RA, RA, PB, RA, RA, RA, RR, PB, PB, RRB, RRR, PB, RA, PB, PB, PB, RR, RR, PB, RRA, PB, RA, RA, RRB, PB, RA, RA, RA, RA, RA, RA, RR, RR, RR, PB, RA, RA, RA, PB, RA, PB, RRA, RRR, RRR, PB, RRB, PB, RA, RR, RR, RR, PB, RRB, PB, PB, RR, RR, RR, RR, RR, PB, RR, RR, PB, PB, RA, RA, RA, RRB, RRB, RRB, RRB, PB, RR, PB, RA, RA, RR, PB, RA, PB, RA, RR, RR, PB, PB, RA, PB, RR, PB, RB, RB, PB, RA, RA, RA, RA, RA, PB, RA, RA, PB, RRB, PB, RA, RA, RR, RR, PB, PB, RA, RRB, PB, RR, RR, PB, RR, PB, PB, RRB, PB, RR, RR, PB, RA, PB, PB, RA, PB, RRB, RRR, PB, RRR, PB, RA, RR, PB, RR, PB, RA, RRB, PB, RR, RR, PB, RA, RRB, PB, RA, PB, RA, RRB, PB, RA, PB, RA, PB, RR, RR, RR, RR, PB, PB, PB, PB, PB, RA, PB, RR, PB, RR, RR, RR, PB, RA, PB, RRB, PB, RRB, RRB, PB, RR, RR, PB, RB, PB, RA, PB, RA, RA, RA, RA, RA, RR, RR, PB, RA, RRB, PB, RR, PB, RB, PB, RA, RA, RA, RA, RA, RA, RR, RR, PB, PB, RA, RA, RA, RA, PB, RA, RA, PB, RA, RA, RA, RA, RA, RA, RA, PB, RA, RR, PB, RA, RA, PB, RA, RA, RA, RA, PB, RRB, RRB, RRB, PB, PB, RA, RR, RR, PB, PB, RR, RR, PB, RB, PB, RA, RA, PB, RA, PB, RR, PB, RA, RA, RA, RA, RR, PB, RA, RA, RA, PB, RRR, PB, RR, RR, RR, PB, RA, RA, RA, RA, PB, PB, RA, RA, RA, RA, RA, RRB, PB, RB, PB, PB, RA, RA, PB, PB, RR, PB, RB, RB, PB, PB, RB, PB, PB, RA, PB, RB, RR, PB, PB, RRB, PB, RR, PB, RR, PB, RR, PB, RR, RR, PB, PB, PB, RRB, PB, RR, PB, PB, RA, RA, PB, RA, PB, RR, RR, RR, PB, RB, PB, RA, PB, RRB, PB, RB, RR, RR, PB, RA, RA, RR, PB, RA, RR, RR, PB, RRB, RRB, RRB, PB, RA, PB, RA, RA, RA, RA, RRB, RRB, PB, RR, PB, RB, PB, PB, PB, RR, PB, RR, RR, PB, PB, PB, PB, RRR, PB, PB, RR, RR, PB, PB, RRB, RRB, RRB, RRB, PB, RB, RB, PB, RR, RR, PB, PB, RRB, RRB, PB, RR, PB, RR, PB, RA, RA, RR, PB, RRB, PB, PB, RRB, PB, RR, PB, RA, PB, PB, RA, RA, RA, PB, PB, RB, RB, RB, RB, RB, PB, RA, PB, RR, PB, RB, RB, RR, PB, RA, RA, RR, PB, RR, PB, RRB, RRB, RRB, RRB, PB, RRA, PB, RR, RR, PB, PB, PB, RR, RR, RR, RR, PB, RB, RB, RR, PB, RA, RA, RA, PB, PB, PB, PB, RRB, RRB, RRB, RRB, RRB, PB, PB, PB, RB, RR, RR, PB, PB, RB, PB, PB, RR, PB, PB, RR, RR, PB, RA, RA, RA, RA, RRB, PB, RA, PB, RR, RR, PB, PB, RR, RR, PB, PB, PB, RB, RR, PB, RRA, PB, RRA, RRR, PB, PB, PB, RR, RR, PB, RB, PB, RA, PB, RA, PB, RR, PB, RR, RR, PB, RA, PB, RB, RB, RB, PB, PB, RA, RA, RA, PB, PB, PB, RRB, RRB, RRR, PB, RB, PB, RR, PB, RB, RB, PB, PB, RRA, PB, PB, RA, PB, RR, PB, PB, PB, PB, RB, RR, PB, RB, PB, RRA, PB, PB, RRR, PB, RA, RR, PB, PB, PB, RA, RR, PB, PB, PB, RB, RB, PB, PB, PB, RR, PB, RA, RR, PB, RR, PB, RA, RRB, PB, RA, PB, RRR, RRR, PB, PB, RR, RR, PB, RRB, PB, RR, RR, PB, PB, RA, PB, RB, PB, RB, PB, PB, RR, PB, PB, PB, RB, RR, PB, RR, RR, PB, PB, RA, PB, PB, RRB, PB, RA, PB, PB, RB, PB, RRA, PB, PB, PB, RR, RR, PB, RB, PB, PB, RRR, PB, PB, PB, PB, SA, RB, RR, RR, PB, RR, PB, RB, RR, PB, PB, RB, RR, PB, RR, PB, RRB, PB, PB, RA, SA, RA, RR, PA, RA, RA, PB, RA, RA, PB, PB, PB, PB, PB, RA, PB, PB, RRB, PB, RRA, SA, RRR, PA, RB, RB, RB, RB, RB, RB, RB, PA, RB, PA, RRA, PA, RRA, PA, PA, RRB, PA, PA, RRB, PA, RB, PA, RRA, PA, RRR, PA, PA, RRR, RRR, PA, RRR, RRR, PB, RRA, RRA, RRA, PA, PA, RRR, PA, PA, RRA, RRA, RRR, RRR, PB, RRB, PA, RRA, RRA, RRA, PB, RRA, SA, RRR, PA, RB, RB, RB, PA, RRR, PB, RRA, RB, PA, RRA, RRA, PB, RRA, RRA, RRA, RB, PB, PB, RRA, RRA, RRA, PB, PB, PB, RRR, RRR, PB, RRA, RRR, RRR, PB, SA, RRA, RRA, RRA, RRA, RRA, PB, PB, RRA, RRA, PB, RRA, RRA, PB, RRA, RRR, PB, PB, RRR, RRR, RRR, RRR, PA, PA, RB, RB, PA, PA, PA, RB, RR, RR, PA, RRA, RB, PA, PA, PA, RRA, RRA, PA, RRA, RB, PA, PA, RRR, RRR, RRR, PA, PA, RRA, RRA, PA, RRA, RRA, RRA, RRR, PA, PA, RRR, PA, PA, RRA, PA, PA, RRA, PA, RRB, PA, RRA, RB, PA, RR, RR, PA, RR, RR, PA, RRA, RRR, RRR, PA, RRA, RB, RB, PA, PA, RRA, PA, RRR, PA, RRB, PA, RRA, PA, PA, RRB, RRR, PA, RRR, PA, PA, RB, PA, PA, RB, RB, RB, PA, RR, RR, PA, RR, PA, RA, RA, RA, RA, PA, RA, RA, RA, RA, RA, PA, RA, RA, PA, RR, PA, RA, RA, RA, RA, RA, RA, PA, RRB, RRB, RRB, RRB, RRB, PA, RA, PA, RA, RA, RA, RA, RRB, PA, RA, RA, RA, RA, RR, PA, RR, RR, PA, RB, RR, RR, RR, PA, RRA, RRA, PA, PA, RB, RR, RR, PA, RRA, RRA, RRA, RRA, RRA, RRA, RRA, RB, PA, PA, PA, PA, PA, PA, RRR, RRR, PA, RRA, PA, RB, RR, PA, RRA, RB, PA, RRR, RRR, RRR, RRR, PA, RRB, PA, RB, PA, RB, RR, RR, PA, RR, RR, PA, RRA, RRA, RRA, RRA, RRA, RRA, PA, PA, RRA, RRA, RRA, RRA, RRR, RRR, RRR, RRR, RRR, PA, RRA, RRA, RRA, RRA, RRR, PA, RRR, PA, RRB, RRB, PA, RRB, RRB, PA, RRA, PA, RRA, RRA, RB, PA, RRA, PA, RA, PA, PA, RB, PA, PA, PA, PA, RB, PA, RRR, RRR, RRR, PA, PA, RRR, PA, PA, RRA, RRA, PA, RR, PA, RB, RR, RR, RR, RR, PA, RR, RR, RR, RR, PA, RA, RA, RA, RA, RA, RA, RR, PA, RRA, PA, RR, RR, RR, PA, RA, RA, RA, RA, RA, PA, RA, RA, RA, RA, RR, PA, PA, RRB, RRR, RRR, RRR, PA, PA, RRB, PA, RRA, RRR, RRR, PA, PA, RB, RB, RR, PA, RB, RB, RB, PA, RRA, PA, PA, RA, RA, RA, RA, RR, PA, RA, RR, PA, RR, RR, PA, RRA, PA, RRA, RRR, PA, PA, RA, RR, RR, PA, RA, RA, RR, PA, RA, RA, PA, RA, RR, RR, RR, RR, RR, RR, PA, PA, RRA, RRA, RRR, PA, RRA, RRA, PA, RRA, RRA, RRR, PA, PA, RRA, RRA, RRA, RRA, RRA, RRA, PA, RB, RB, RB, RB, RB, RR, PA, RRA, RRA, RB, RB, RB, RB, PA, RRA, RRR, PA, PA, RRA, RRA, PA, RRA, RRA, RRA, PA, RRR, RRR, PA, RRR, RRR, RRR, RRR, PA, RRA, RRA, RRA, RRR, RRR, PA, RRA, RRA, RRR, PA, RRR, RRR, RRR, PA, RRB, PA, RRR, PA, PA, RR, PA, RRB, RRB, RRB, RRB, RRB, RRB, RRB, PA, RRB, PA, PA, RRR, RRR, RRR, RRR, PA, PA, RA, PA, RRB, PA, PA, RR, PA, RRA, PA, RRB, RRB, PA, PA, RB, PA, PA, RA, PA, RR, RR, PA, RRB, RRR, RRR, PA, PA, RRA, PA, RRA, PA, RR, PA, RR, PA, PA, PA, RRR, RRR, PA, RB, PA, RR, RR, PA, PA, RA, RR, PA, RA, RR, RR, RR, PA, RR, RR, RR, PA, RB, RR, PA, RRA, PA, RRR, PA, RRA, RRA, RRA, RRA, PA, RRA, PA, RRA, RRR, PA, RRR, PA, PA, RRR, RRR, PA, PA, PA, PA, RA, RR, PA, RB, RR, RR, PA, RA, RA, RR, RR, RR, PA, RB, PA, RA, RA, RR, RR, PA, RB, RR, RR, PA, RA, RA, PA, RRB, RRB, PA, RRB, RRB, RRB, RRB, RRB, RRR, PA, RA, RA, RRB, PA, RA, RA, RA, RA, PA, RA, RA, RR, PA, RA, RA, RA, PA, RA, RR, RR, RR, RR, PA, RA, RA, RRB, RRB, PA, RRB, RRB, PA, RA, RR, RR, PA, RR, RR, PA, RRA, RB, RB, RB, RB, RB, RB, RB, PA, RB, RB, RB, RR, RR, PA, RRA, RRA, RRA, RB, RB, PA, RRA, PA, RB, RR, PA, RRA, RRR, RRR, RRR, RRR, RRR, PA, RRR, RRR, PA, RA, RR, RR, RR, PA, RRA, RB, RB, PA, RRA, PA, RRA, RRA, RB, PA, RRR, RRR, RRR, RRR, RRR, RRR, PA, RRB, RRB, RRB, RRB, RRB, RRB, PA, RRA, PA, RRA, RRA, PA, RB, RB, RR, PA, RRA, RB, RB, RB, PA, RRA, RRR, RRR, PA, RRA, RRA, RRA, RRR, RRR, RRR, PA, RRR, RRR, PA, RRA, RRA, RB, PA, RRA, RRA, RRA, RRA, RRR, RRR, PA, RR, PA, RRB, RRB, RRB, RRB, RRR, RRR, RRR, RRR, PA, RRB, RRB, RRB, RRR, PA, RRB, PA, RRB, RRR, RRR, PA, RRA, PA, PA, RB, RR, RR, RR, PA, PA, RA, RR, PA, RR, PA, PA, RR, PA, RRR, RRR, RRR, RRR, RRR, RRR, PA, RRA, RRA, PA, RRA, RRA, PA, RRA, RRA, PA, RA, RR, PA, RA, RA, RR, PA, RA, RA, PA, RA, RR, RR, RR, PA, RA, RR, PA, RR, PA, RR, PA, RRB, PA, RRR, PA, PA, PA, RRA, RRA, PA, RR, RR, PA, RRA, RB, PA, RB, RB, PA, RB, PA, RRA, RB, PA, PA, RR, RR, RR, RR, PA, RR, RR, PA, RR, RR, PA, RA, RA, RA, RA, RA, RA, RA, RA, RA, RR, PA, RRB, RRB, PA, PA, PA, RRA, PA, RR, RR, PA, RRA, RRA, RRA, RRA, RRA, PA, PA, RB, PA, PA, RB, PA, RR, RR, PA, RR, RR, PA, PA, RR, RR, RR, RR, RR, PA, RB, RB, RB, RR, RR, RR, PA, RA, RR, PA, RB, RR, RR, PA, RB, RB, RB, RB, RR, PA, RRA, RRA, RRA, PA, RRR, RRR, RRR, RRR, PA, RRA, PA, RRR, RRR, RRR, RRR, PA, RRB, RRB, RRR, RRR, PA, RRB, PA, RRR, RRR, RRR, PA, PA, RRR, RRR, RRR, RRR, PA, RRB, PA, RRA, RRR, RRR, RRR, RRR, RRR, PA, RRB, RRB, RRR, PA, RRA, RRA, RRA, PA, RR, PA, RR, PA, RR, RR, RR, PA, RRA, PA, RA, RA, RR, RR, RR, RR, RR, PA, RB, RB, RR, PA, RRA, RRR, RRR, PA, RRA, RRA, RRA, RRR, PA, RRB, RRB, RRR, PA, PA, RRR, RRR, RRR, RRR, RRR, RRR, RRR, PA, RRR, PA, PA, RR, RR, PA, SA, RRA, PA, RRB, RRR, RRR, RRR, PA, RRA, RRR, PA, PA, PA, RRA, RRR, PA, RB, PA, PA, RRA, RRA, RRA, PA, RRB, RRB, RRR, RRR, RRR, PA, RRB, RRB, PA, RA, RRB, RRB, PA, RRB, RRR, RRR, RRR, RRR, RRR, PA, RRB, RRR, PA, RB, PA, PA, PA, PA, RRR, RRR, PA, RRR, PA, RR, PA, RA, PA, RR, PA, PA, RR, PA, RB, PA, RR, RR, RR, RR, RR, RR, PA, RA, RA, RA, RA, RA, RR, RR, RR, PA, PA, RRR, PA, RB, RR, RR, PA, PA, PA, RA, RA, PA, RR, PA, RB, RR, PA, PA, RA, RR, RR, RR, RR, PA, PA, RR, PA, RR, PA, RR, PA, RA, RA, RA, RA, RA, RR, RR, RR, PA, RA, RR, PA, RA, RA, PA, PA, RRB, RRB, RRB, RRB, RRB, RRR, PA, RRB, RRB, RRB, RRR, PA, RRR, RRR, RRR, PA, RRR, PA, RA, RRB, RRB, RRB, PA, RRB, RRR, RRR, PA, RRR, PA, RRA, RRA, PA, RRA, RRA, PA, RRA, PA, RRR, RRR, RRR, PA, RRB, RRB, RRR, RRR, RRR, RRR, PA, PA, PA, PA, RRA, RRR, PA, PA, RRA, RRA, RRA, RRA, PA, RB, RB, PA, RRA, RRA, RRA, PA, RRA, RRR, PA, RA, PA, PA, RRB, RRB, PA, PA, PA, RRA, RRR, RRR, RRR, PA, RB, PA, RB, RR, RR, PA, RB, RB, PA, PA, RRR, PA, RRB, RRR, PA, RRA, RRA, RRA, RRA, PA, RR, PA, RRA, RRR, RRR, PA, PA, RRA, RRR, RRR, PA, RRB, PA, RRR, PB, RRR, RRR, PB, PB, RRB, RRR, PA, RB, RB, RB, PA, RB, PA, RB, RB, RB, RB, PA, RR, RR, PA, PA, PA, PA, RR, RR, RR, RR, RR, PA, RB, RB, PA, RB, RB, RB, RB, RR, PA, RB, RR, RR, PA, RRA, RRA, RRA, RRR, PA, RRA, RRA, PA, RRA, RRA, PA, PA, RB, RR, RR, RR, PA, RB, RB, RR, PA, RR, RR, PA, RB, RB, RB, PA, PA, RRA, RRA, PA, RRA, RRR, PA, RRA, RRA, RRR, RRR, RRR, PA, RRA, RRA, RRR, RRR, PA, RA, PA, PA, RRB, RRR, RRR, PA, RRA, RRA, RRA, PA, RRA, RRR, RRR, RRR, PA, RRA, RRR, RRR, RRR, PA, RB, PA, PA, RB, RB, RB, PA, PA, RR, PA, RRA, RRA, RRA, RRR, PA, PA, RR, PA, PA, RR, PA, RA, PA, RB, RR, PA, PA, RB, RB, RR, PA, RRR, RRR, PA, RB, PA, RRA, RRR, RRR, PA, RRB, RRB, RRR, PA, RRA, PA, RRB, PA, RRB, PA, RRR, PA, PA, PA
//        };
//        const int before_cmd_count = static_cast<int>(cmds.size());
//        for (command cmd: cmds) {
//            init_state.do_cmd(cmd);
//        }
//        optimize_all_range(init_state, 14, 0);
//        write_optimized_range_cmds_result(init_state, before_cmd_count);
//        return 0;
//    }

    bool is_optuna = has_arg(argc, argv, "--optuna");
    bool is_optuna_lahc = has_arg(argc, argv, "--optuna_lahc");
    bool show_alns_log = has_arg(argc, argv, "--alns_log");
    bool show_alns_file_log = has_arg(argc, argv, "--alns_file_log");
    set_alns_accept_params(parse_alns_accept_params(argc, argv));
    //Optuna {
    //Optuna     std::ofstream debug_log("main_entry_debug.txt", std::ios::trunc);
    //Optuna     debug_log << "main:start" << '\n';
    //Optuna }


    init(test_N);//絶対最初に呼ぶ
    //Optuna {
    //Optuna     std::ofstream debug_log("main_entry_debug.txt", std::ios::app);
    //Optuna     debug_log << "main:after_init" << '\n';
    //Optuna }

    if (!is_optuna && !is_optuna_lahc) {
        init_output(argc, argv);
    }
    //Optuna {
    //Optuna     std::ofstream debug_log("main_entry_debug.txt", std::ios::app);
    //Optuna     debug_log << "main:after_init_output" << '\n';
    //Optuna }
    if (is_optuna_lahc) {
        double final_score = run_optuna_lahc_params_case100(show_alns_log, show_alns_file_log);
        cout << "FINAL_SCORE=" << final_score << endl;
        return 0;
    }
    if (is_optuna) {
        GreedyConstructorParams params = parse_greedy_params(argc, argv);
        int final_score = run_optuna_case100(params, show_alns_log, show_alns_file_log);
        cout << "FINAL_SCORE=" << final_score << endl;
        return 0;
    }
//    test_query_common();
//    benchmark_query_ptr_vs_common_query();
//    test_a2b_b2a_range_set_lazy_seg_tree();
    {
        std::ofstream debug_log("main_entry_debug.txt", std::ios::app);
        debug_log << "main:before_test_greedy_queries_constructor" << '\n';
    }
//    test_greedy_queries_constructor<MAX_BUFF>();
    {
        std::ofstream debug_log("main_entry_debug.txt", std::ios::app);
        debug_log << "main:after_test_greedy_queries_constructor" << '\n';
    }
    {
        std::ofstream debug_log("main_entry_debug.txt", std::ios::app);
        debug_log << "main:before_test_repeated_destroy_construct" << '\n';
    }

//    test_owner_array_unit_compare<100, 3000, 2>();
    run_owner_array_unit_100<100, 3000, 2>();
//    run_owner_array_unit_500<501, 300, 2>();
//    run_owner_single_100_multi<100, 3000, 2>();
//    run_any_seed_1case<100, 3000, 2>();
    return 0;
//    test_greedy_queries_constructor<MAX_BUFF, 3>(2, 100000);
//    test_ins2_diff<MAX_BUFF, 3>(2, 100000);
//    test_greedy_queries_constructor_indirect<MAX_BUFF, 3>(3, 100000);
//    test_greedy_queries_constructor_a2b_b2a_indirect<MAX_BUFF, 3>(3, 100000);

    {
        std::ofstream debug_log("main_entry_debug.txt", std::ios::app);
        debug_log << "main:after_test_repeated_destroy_construct" << '\n';
    }
    return 0;
    //bbb
}
