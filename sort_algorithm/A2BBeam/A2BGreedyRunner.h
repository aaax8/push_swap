#ifndef UNTITLED_A2BGREEDYRUNNER_H
#define UNTITLED_A2BGREEDYRUNNER_H

#include "UnderMaxSortPredictor.h"
#include "Command/SyncCommand.h"
#include "State.h"
#include "all_include.h"
#include "util.h"

template<size_t MAX_N>
class A2BGreedyRunner {
private:
    static constexpr int rot_limit = 6;
    static constexpr int inf = 1 << 30;
    static constexpr int log_candidate_limit = 10;
    static constexpr double start_predict_weight = 0.7;
    static constexpr double finish_predict_weight = 1.0;

    // 責務: 1 つの A2B 候補の位置、回転方法、手数評価を保持する。
    // 必要な理由: 候補評価、採用候補の適用、debug log の出力を同じ情報で行うため。
    struct GreedyMove {
        int a_idx;
        int b_idx;
        command a_cmd;
        command b_cmd;
        int move_turn;
        double predict_all_turn;
    };

public:
    // 責務: A2B greedy 終了後の state、実コマンド列、累積手数、最終予測手数を保持する。
    // 必要な理由: A が空になるまで選んだ結果と、出力した command をテストで確認するため。
    struct GreedyResult {
        State state;
        cmd_list_t cmds;
        int spent_turn;
        double predict_all_turn;
    };

private:
    // 責務: 初期 State が A のみ、B 空、値が 0..N-1 の重複なしであることを確認する。
    // 必要な理由: UnderMaxSortPredictor が 0..N-1 の値を前提にしているため。
    static void assert_initial_state(int N, const State &st) {
        my_assert(N > 0);
        my_assert(st.A.size() == N);
        my_assert(st.B.empty());
#ifdef DEBUG
        my_vector<int> seen(N);
        for (int i = 0; i < st.A.size(); i++) {
            my_assert(0 <= st.A[i] && st.A[i] < N);
            my_assert(!seen[st.A[i]]);
            seen[st.A[i]] = 1;
        }
        for (int v = 0; v < N; v++) {
            my_assert(seen[v]);
        }
#endif
    }

    // 責務: idx を top にする reverse rotate 距離を返す。
    // 必要な理由: RRA/RRB 候補の idx と rotate 距離を相互変換するため。
    static int rev_dis(int size, int idx) {
        my_assert(0 <= idx && idx < size);
        if (idx == 0) {
            return 0;
        }
        return size - idx;
    }

    // 責務: GreedyMove が指定する A 側 command の実行回数を返す。
    // 必要な理由: GreedyMove に a_dis を持たせず、command 生成時に必要な距離だけ計算するため。
    static int calc_a_dis(const GreedyMove &move, int a_size) {
        if (move.a_cmd == RA) {
            return move.a_idx;
        }
        my_assert(move.a_cmd == RRA);
        return rev_dis(a_size, move.a_idx);
    }

    // 責務: GreedyMove が指定する B 側 command の実行回数を返す。
    // 必要な理由: GreedyMove に b_dis を持たせず、command 生成時に必要な距離だけ計算するため。
    static int calc_b_dis(const GreedyMove &move, int b_size) {
        if (b_size == 0) {
            return 0;
        }
        if (move.b_cmd == RB) {
            return move.b_idx;
        }
        my_assert(move.b_cmd == RRB);
        return rev_dis(b_size, move.b_idx);
    }

    // 責務: GreedyMove を実行する command 列を作る。
    // 必要な理由: State 更新と最終 command 出力が同じ操作列を共有できるようにするため。
    static cmd_list_t build_move_cmds(const GreedyMove &move, int a_size, int b_size) {
        int a_dis = calc_a_dis(move, a_size);
        int b_dis = calc_b_dis(move, b_size);
        cmd_list_t cmds;
        if (can_sync_cmd(move.a_cmd, move.b_cmd)) {
            int sync_len = min(a_dis, b_dis);
            command sync_cmd_v = sync_cmd(move.a_cmd, move.b_cmd);
            for (int i = 0; i < sync_len; i++) {
                cmds.add(sync_cmd_v);
            }
            for (int i = sync_len; i < a_dis; i++) {
                cmds.add(move.a_cmd);
            }
            for (int i = sync_len; i < b_dis; i++) {
                cmds.add(move.b_cmd);
            }
        } else {
            for (int i = 0; i < a_dis; i++) {
                cmds.add(move.a_cmd);
            }
            for (int i = 0; i < b_dis; i++) {
                cmds.add(move.b_cmd);
            }
        }
        cmds.add(PB);
        my_assert(cmds.list.size() == move.move_turn);
        return cmds;
    }

    // 責務: cmd_list_t の全 command を State に順に適用する。
    // 必要な理由: 仮評価と実適用で build_move_cmds の結果を同じように反映するため。
    static void apply_cmds(State &st, const cmd_list_t &cmds) {
        for (int i = 0; i < cmds.list.size(); i++) {
            st.do_cmd(cmds.list[i]);
        }
    }

    // 責務: src の command を dst へ末尾追加する。
    // 必要な理由: State の command tracking に依存せず、runner の最終 command を保存するため。
    static void append_cmds(cmd_list_t &dst, const cmd_list_t &src) {
        for (int i = 0; i < src.list.size(); i++) {
            dst.add(src.list[i]);
        }
    }

    // 責務: push 済み個数に応じて rest_predict に掛ける重みを返す。
    // 必要な理由: 序盤は予測手数を重く見て、N-1 個 push 済みの時点で重みを 1.0 にするため。
    static double calc_predict_weight(int N, int pushed_count) {
        if (N <= 1) {
            return finish_predict_weight;
        }
        int clamped_pushed = min(pushed_count, N - 1);
        double rate = (double) clamped_pushed / (double) (N - 1);
        return start_predict_weight + (finish_predict_weight - start_predict_weight) * rate;
    }

    // 責務: move 適用後の仮 state に predictor を呼び、predict_all_turn を埋めた候補を返す。
    // 必要な理由: spent_turn + move_turn + predict_turn * weight で greedy 選択するため。
    static GreedyMove eval_move(int N, const State &st, GreedyMove move, int spent_turn) {
        cmd_list_t move_cmds = build_move_cmds(move, st.A.size(), st.B.size());
        State next_st(st);
        apply_cmds(next_st, move_cmds);
        int rest_predict = UnderMaxSortPredictor<MAX_N>::predict_turn(N, next_st.A, next_st.B);
        double predict_weight = calc_predict_weight(N, next_st.B.size());
        move.predict_all_turn = (double) spent_turn + (double) move.move_turn + (double) rest_predict * predict_weight;
        return move;
    }

    // 責務: rotate 手数が 6 以下の候補を評価して candidates に追加する。
    // 必要な理由: choose_move の候補列挙を直接生成方式にして、不要な全走査を避けるため。
    static void add_candidate(int N, const State &st, int spent_turn, my_vector<GreedyMove> &candidates,
                              int a_idx, int b_idx, command a_cmd, command b_cmd, int rot_turn) {
        my_assert(rot_turn <= rot_limit);
        GreedyMove move{a_idx, b_idx, a_cmd, b_cmd, rot_turn + 1, (double) inf};
        candidates.push_back(eval_move(N, st, move, spent_turn));
    }

    // 責務: 現在 state の全候補を作り、評価済み候補列を返す。
    // 必要な理由: rot_turn 外側全走査や一時 bases を使わず候補を生成するため。
    static my_vector<GreedyMove> build_candidates(int N, const State &st, int spent_turn) {
        my_vector<GreedyMove> candidates;
        int a_size = st.A.size();
        int b_size = st.B.size();
        int b_forward_limit = 0;
        if (b_size > 0) {
            b_forward_limit = min(rot_limit, b_size - 1);
        }

        for (int a_dis = 0; a_dis <= min(rot_limit, a_size - 1); a_dis++) {
            for (int b_dis = 0; b_dis <= b_forward_limit; b_dis++) {
                int rot_turn = max(a_dis, b_dis);
                add_candidate(N, st, spent_turn, candidates, a_dis, b_dis, RA, RB, rot_turn);
            }
        }
        if (b_size > 0) {
            for (int a_dis = 0; a_dis <= min(rot_limit, a_size - 1); a_dis++) {
                for (int b_dis = 1; b_dis <= min(rot_limit - a_dis, b_size - 1); b_dis++) {
                    add_candidate(N, st, spent_turn, candidates, a_dis, b_size - b_dis, RA, RRB, a_dis + b_dis);
                }
            }
        }
        for (int a_dis = 1; a_dis <= min(rot_limit, a_size - 1); a_dis++) {
            for (int b_dis = 0; b_dis <= min(rot_limit - a_dis, b_forward_limit); b_dis++) {
                add_candidate(N, st, spent_turn, candidates, a_size - a_dis, b_dis, RRA, RB, a_dis + b_dis);
            }
        }
        if (b_size > 0) {
            for (int a_dis = 1; a_dis <= min(rot_limit, a_size - 1); a_dis++) {
                for (int b_dis = 1; b_dis <= min(rot_limit, b_size - 1); b_dis++) {
                    int rot_turn = max(a_dis, b_dis);
                    add_candidate(N, st, spent_turn, candidates, a_size - a_dis, b_size - b_dis,
                                  RRA, RRB, rot_turn);
                }
            }
        }
        my_assert(!candidates.empty());
        return candidates;
    }

    // 責務: 評価済み候補列から predict_all_turn が最小の候補を返す。
    // 必要な理由: 同点は候補生成順で先に出たものを採用するため。
    static GreedyMove choose_move(const my_vector<GreedyMove> &candidates) {
        int best_idx = 0;
        for (int i = 1; i < candidates.size(); i++) {
            if (candidates[i].predict_all_turn < candidates[best_idx].predict_all_turn) {
                best_idx = i;
            }
        }
        return candidates[best_idx];
    }

    // 責務: RingBuffer500 を label=[v0,v1,...] 形式で出力する。
    // 必要な理由: 各 step の開始・終了 state をログで確認するため。
    static void write_ring(std::ostream &os, const char *label, const RingBuffer500 &values) {
        os << label << "=[";
        for (int i = 0; i < values.size(); i++) {
            if (i > 0) {
                os << ",";
            }
            os << values[i];
        }
        os << "]";
    }

    // 責務: GreedyMove を 1 行で出力する。
    // 必要な理由: 候補評価と採用候補を同じ形式で確認するため。
    static void write_move(std::ostream &os, const char *label, const GreedyMove &move) {
        os << label
           << " a_idx=" << move.a_idx
           << " b_idx=" << move.b_idx
           << " a_cmd=" << move.a_cmd
           << " b_cmd=" << move.b_cmd
           << " move_turn=" << move.move_turn
           << " predict_all_turn=" << move.predict_all_turn
           << "\n";
    }

    // 責務: step 開始 state、上位候補、採用候補、step 終了 state を出力する。
    // 必要な理由: greedy が何を比較して何を選んだかを追えるようにするため。
    static void log_step(std::ostream &os, int step, int spent_before, const State &before_st,
                         const my_vector<GreedyMove> &candidates, const GreedyMove &chosen,
                         int spent_after, const State &after_st) {
        os << "a2b_greedy_step"
           << "\n  step=" << step
           << "\n  spent_before=" << spent_before
           << "\n  ";
        write_ring(os, "a_before", before_st.A);
        os << "\n  ";
        write_ring(os, "b_before", before_st.B);
        os << "\n";

        my_vector<GreedyMove> sorted_candidates = candidates;
        std::stable_sort(sorted_candidates.begin(), sorted_candidates.end(),
                         [](const GreedyMove &lhs, const GreedyMove &rhs) {
                             return lhs.predict_all_turn < rhs.predict_all_turn;
                         });
        int log_count = min(log_candidate_limit, (int) sorted_candidates.size());
        for (int i = 0; i < log_count; i++) {
            os << "  candidate_rank=" << i << " ";
            write_move(os, "move", sorted_candidates[i]);
        }
        os << "  ";
        write_move(os, "chosen", chosen);
        os << "  spent_after=" << spent_after
           << "\n  ";
        write_ring(os, "a_after", after_st.A);
        os << "\n  ";
        write_ring(os, "b_after", after_st.B);
        os << "\n\n";
        os.flush();
    }

    // 責務: コマンド列を 1 行のカンマ+半角スペース区切りで OutPut/a2b_greedy_runner_commands.txt に出力する。
    // 必要な理由: 答えが求まった時に、ログとは別ファイルで command を確認するため。
    static void write_commands(const cmd_list_t &cmds) {
        std::filesystem::create_directories(output_dir);
        const std::filesystem::path path = output_dir / "a2b_greedy_runner_commands.txt";
        std::ofstream os(path, std::ios::trunc);
        if (!os.is_open()) {
            std::cerr << "failed to open " << path << std::endl;
            std::abort();
        }
        for (int i = 0; i < cmds.list.size(); i++) {
            if (i > 0) {
                os << ", ";
            }
            os << cmds.list[i];
        }
        os << "\n";
    }

    // 責務: OutPut/a2b_greedy_runner_debug.txt を truncate で開く。
    // 必要な理由: 各 step の候補評価と採用結果を command 出力とは別に保存するため。
    static std::ofstream open_debug_log() {
        std::filesystem::create_directories(output_dir);
        const std::filesystem::path path = output_dir / "a2b_greedy_runner_debug.txt";
        std::ofstream os(path, std::ios::trunc);
        if (!os.is_open()) {
            std::cerr << "failed to open " << path << std::endl;
            std::abort();
        }
        return os;
    }

public:
    // 責務: 初期 State から A が空になるまで greedy に pb し、最終 command と debug log を出力する。
    // 必要な理由: UnderMaxSortPredictor を使った貪欲 A2B 構築を単独で試せるようにするため。
    static GreedyResult run(int N, const State &initial_state) {
        assert_initial_state(N, initial_state);
        State st(initial_state);
        int spent_turn = 0;
        int step = 0;
        cmd_list_t cmds;
        std::ofstream debug_os = open_debug_log();
        while (!st.A.empty()) {
            State before_st(st);
            int spent_before = spent_turn;
            my_vector<GreedyMove> candidates = build_candidates(N, st, spent_turn);
            GreedyMove move = choose_move(candidates);
            cmd_list_t move_cmds = build_move_cmds(move, st.A.size(), st.B.size());
            apply_cmds(st, move_cmds);
            append_cmds(cmds, move_cmds);
            spent_turn += move.move_turn;
            my_assert(spent_turn == cmds.list.size());
            log_step(debug_os, step, spent_before, before_st, candidates, move, spent_turn, st);
            step++;
        }
        int final_rest_predict = UnderMaxSortPredictor<MAX_N>::predict_turn(N, st.A, st.B);
        double final_predict_weight = calc_predict_weight(N, st.B.size());
        double final_predict_all_turn = (double) spent_turn + (double) final_rest_predict * final_predict_weight;
        write_commands(cmds);
        return {st, cmds, spent_turn, final_predict_all_turn};
    }
};

#endif //UNTITLED_A2BGREEDYRUNNER_H
