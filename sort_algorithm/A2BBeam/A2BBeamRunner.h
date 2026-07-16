#ifndef UNTITLED_A2BBEAMRUNNER_H
#define UNTITLED_A2BBEAMRUNNER_H

#include "UnderMaxSortPredictor.h"
#include "Command/SyncCommand.h"
#include "State.h"
#include "all_include.h"
#include "util.h"

template<size_t MAX_N>
class A2BBeamRunner {
private:
    static constexpr int rot_limit = 5;
    static constexpr int inf = 1 << 30;
    static constexpr int log_node_limit = 10;
    static constexpr double a2b_start_weight = 1.5;
    static constexpr double a2b_finish_weight = 1.5;
    static constexpr double b2a_start_weight = 0.67;
    static constexpr double b2a_finish_weight = 0.67;

    // 責務: 1 つの A2B 候補の位置、回転方法、手数評価を保持する。
    // 必要な理由: greedy と同じ候補生成・command 生成を beam node 展開で使うため。
    struct BeamMove {
        int a_idx;
        int b_idx;
        command a_cmd;
        command b_cmd;
        int move_turn;
        double predict_all_turn;
    };

public:
    // 責務: A2B beam 終了後の state、実コマンド列、累積手数、最終予測 score を保持する。
    // 必要な理由: A が空になるまで beam で選んだ結果と、出力した command をテストで確認するため。
    struct BeamResult {
        State state;
        cmd_list_t cmds;
        int spent_turn;
        double predict_all_turn;
    };

private:
    // 責務: 1 つの beam node として state、累積 command、累積手数、score、直前 move を保持する。
    // 必要な理由: 親 pointer を使わず、各 depth で node をコピーして単純に beam search するため。
    struct BeamNode {
        State state;
        cmd_list_t cmds;
        int spent_turn;
        double predict_all_turn;
        BeamMove last_move;
        bool has_last_move;
    };

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

    // 責務: BeamMove が指定する A 側 command の実行回数を返す。
    // 必要な理由: BeamMove に a_dis を持たせず、command 生成時に必要な距離だけ計算するため。
    static int calc_a_dis(const BeamMove &move, int a_size) {
        if (move.a_cmd == RA) {
            return move.a_idx;
        }
        my_assert(move.a_cmd == RRA);
        return rev_dis(a_size, move.a_idx);
    }

    // 責務: BeamMove が指定する B 側 command の実行回数を返す。
    // 必要な理由: BeamMove に b_dis を持たせず、command 生成時に必要な距離だけ計算するため。
    static int calc_b_dis(const BeamMove &move, int b_size) {
        if (b_size == 0) {
            return 0;
        }
        if (move.b_cmd == RB) {
            return move.b_idx;
        }
        my_assert(move.b_cmd == RRB);
        return rev_dis(b_size, move.b_idx);
    }

    // 責務: BeamMove を実行する command 列を作る。
    // 必要な理由: State 更新と最終 command 出力が同じ操作列を共有できるようにするため。
    static cmd_list_t build_move_cmds(const BeamMove &move, int a_size, int b_size) {
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
    // 必要な理由: State の command tracking に依存せず、runner の node command を保存するため。
    static void append_cmds(cmd_list_t &dst, const cmd_list_t &src) {
        for (int i = 0; i < src.list.size(); i++) {
            dst.add(src.list[i]);
        }
    }

    // 責務: push 済み個数に応じて A2B 側 predict に掛ける重みを返す。
    // 必要な理由: A2B 側の残り予測を B2A 側とは別係数で beam score に反映するため。
    static double calc_a2b_weight(int N, int pushed_count) {
        if (N <= 1) {
            return a2b_finish_weight;
        }
        int clamped_pushed = min(pushed_count, N - 1);
        double rate = (double) clamped_pushed / (double) (N - 1);
        return a2b_start_weight + (a2b_finish_weight - a2b_start_weight) * rate;
    }

    // 責務: push 済み個数に応じて B2A 側 predict に掛ける重みを返す。
    // 必要な理由: B2A 側の残り予測を A2B 側とは別係数で beam score に反映するため。
    static double calc_b2a_weight(int N, int pushed_count) {
        if (N <= 1) {
            return b2a_finish_weight;
        }
        int clamped_pushed = min(pushed_count, N - 1);
        double rate = (double) clamped_pushed / (double) (N - 1);
        return b2a_start_weight + (b2a_finish_weight - b2a_start_weight) * rate;
    }

    // 責務: move 適用後の子 node を作り、predict_all_turn を埋めて返す。
    // 必要な理由: spent_turn + A2B/B2A 別係数つき predict で beam を枝刈りするため。
    static BeamNode eval_move(int N, const BeamNode &node, BeamMove move) {
        cmd_list_t move_cmds = build_move_cmds(move, node.state.A.size(), node.state.B.size());
        BeamNode next_node{State(node.state), cmd_list_t(node.cmds), node.spent_turn, (double) inf, move, true};
        apply_cmds(next_node.state, move_cmds);
        append_cmds(next_node.cmds, move_cmds);
        next_node.spent_turn += move.move_turn;
        auto rest_predict = UnderMaxSortPredictor<MAX_N>::predict_detail(N, next_node.state.A, next_node.state.B);
        double a2b_weight = calc_a2b_weight(N, next_node.state.B.size());
        double b2a_weight = calc_b2a_weight(N, next_node.state.B.size());
        next_node.predict_all_turn =
                (double) next_node.spent_turn
                + (double) (rest_predict.a2b_cost + rest_predict.push_over_max_cost) * a2b_weight
                + (double) rest_predict.b2a_cost * b2a_weight;
        next_node.last_move.predict_all_turn = next_node.predict_all_turn;
        my_assert(next_node.spent_turn == next_node.cmds.list.size());
        return next_node;
    }

    // 責務: rotate 手数が 6 以下の候補を評価して candidates に追加する。
    // 必要な理由: greedy と同じ候補列挙を beam の各 node から使うため。
    static void add_candidate(int N, const BeamNode &node, my_vector<BeamNode> &candidates,
                              int a_idx, int b_idx, command a_cmd, command b_cmd, int rot_turn) {
        my_assert(rot_turn <= rot_limit);
        BeamMove move{a_idx, b_idx, a_cmd, b_cmd, rot_turn + 1, (double) inf};
        candidates.push_back(eval_move(N, node, move));
    }

    // 責務: 現在 node から到達できる全候補 node を candidates に追加する。
    // 必要な理由: greedy と同じ候補列挙のまま、beam search で複数 node を展開するため。
    static void append_candidates(int N, const BeamNode &node, my_vector<BeamNode> &candidates) {
        int a_size = node.state.A.size();
        int b_size = node.state.B.size();
        int b_forward_limit = 0;
        if (b_size > 0) {
            b_forward_limit = min(rot_limit, b_size - 1);
        }

        for (int a_dis = 0; a_dis <= min(rot_limit, a_size - 1); a_dis++) {
            for (int b_dis = 0; b_dis <= b_forward_limit; b_dis++) {
                int rot_turn = max(a_dis, b_dis);
                add_candidate(N, node, candidates, a_dis, b_dis, RA, RB, rot_turn);
            }
        }
        if (b_size > 0) {
            for (int a_dis = 0; a_dis <= min(rot_limit, a_size - 1); a_dis++) {
                for (int b_dis = 1; b_dis <= min(rot_limit - a_dis, b_size - 1); b_dis++) {
                    add_candidate(N, node, candidates, a_dis, b_size - b_dis, RA, RRB, a_dis + b_dis);
                }
            }
        }
        for (int a_dis = 1; a_dis <= min(rot_limit, a_size - 1); a_dis++) {
            for (int b_dis = 0; b_dis <= min(rot_limit - a_dis, b_forward_limit); b_dis++) {
                add_candidate(N, node, candidates, a_size - a_dis, b_dis, RRA, RB, a_dis + b_dis);
            }
        }
        if (b_size > 0) {
            for (int a_dis = 1; a_dis <= min(rot_limit, a_size - 1); a_dis++) {
                for (int b_dis = 1; b_dis <= min(rot_limit, b_size - 1); b_dis++) {
                    int rot_turn = max(a_dis, b_dis);
                    add_candidate(N, node, candidates, a_size - a_dis, b_size - b_dis,
                                  RRA, RRB, rot_turn);
                }
            }
        }
    }

    // 責務: 候補 node を score 昇順に並べ、上位 beam_width 個だけ返す。
    // 必要な理由: 各 depth で beam search の幅を固定するため。
    static my_vector<BeamNode> select_next_beam(my_vector<BeamNode> &candidates, int beam_width) {
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const BeamNode &lhs, const BeamNode &rhs) {
                             return lhs.predict_all_turn < rhs.predict_all_turn;
                         });
        int take_count = min(beam_width, (int) candidates.size());
        my_vector<BeamNode> next_beam;
        next_beam.reserve(take_count);
        for (int i = 0; i < take_count; i++) {
            next_beam.push_back(candidates[i]);
        }
        return next_beam;
    }

    // 責務: RingBuffer500 を label=[v0,v1,...] 形式で出力する。
    // 必要な理由: 各 depth の上位 node 状態をログで確認するため。
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

    // 責務: BeamMove を 1 行で出力する。
    // 必要な理由: 各 depth の上位 node がどの move で作られたか確認するため。
    static void write_move(std::ostream &os, const BeamMove &move) {
        os << "a_idx=" << move.a_idx
           << " b_idx=" << move.b_idx
           << " a_cmd=" << move.a_cmd
           << " b_cmd=" << move.b_cmd
           << " move_turn=" << move.move_turn
           << " predict_all_turn=" << move.predict_all_turn;
    }

    // 責務: depth、beam 数、上位 node の score、state、直前 move を出力する。
    // 必要な理由: beam がどの候補を残しているかを追えるようにするため。
    static void log_beam(std::ostream &os, int depth, const my_vector<BeamNode> &beam) {
        os << "a2b_beam_depth"
           << "\n  depth=" << depth
           << "\n  beam_size=" << beam.size()
           << "\n";
        int log_count = min(log_node_limit, (int) beam.size());
        for (int i = 0; i < log_count; i++) {
            const BeamNode &node = beam[i];
            os << "  node_rank=" << i
               << " spent_turn=" << node.spent_turn
               << " predict_all_turn=" << node.predict_all_turn;
            if (node.has_last_move) {
                os << " last_move ";
                write_move(os, node.last_move);
            }
            os << "\n    ";
            write_ring(os, "a", node.state.A);
            os << "\n    ";
            write_ring(os, "b", node.state.B);
            os << "\n";
        }
        os << "\n";
        os.flush();
    }

    // 責務: コマンド列を 1 行のカンマ+半角スペース区切りで OutPut/a2b_beam_runner_commands.txt に出力する。
    // 必要な理由: 答えが求まった時に、ログとは別ファイルで command を確認するため。
    static void write_commands(const cmd_list_t &cmds) {
        std::filesystem::create_directories(output_dir);
        const std::filesystem::path path = output_dir / "a2b_beam_runner_commands.txt";
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

    // 責務: OutPut/a2b_beam_runner_debug.txt を truncate で開く。
    // 必要な理由: 各 depth の上位 node を command 出力とは別に保存するため。
    static std::ofstream open_debug_log() {
        std::filesystem::create_directories(output_dir);
        const std::filesystem::path path = output_dir / "a2b_beam_runner_debug.txt";
        std::ofstream os(path, std::ios::trunc);
        if (!os.is_open()) {
            std::cerr << "failed to open " << path << std::endl;
            std::abort();
        }
        return os;
    }

public:
    // 責務: 初期 State から A が空になる depth まで beam search し、最終 command と debug log を出力する。
    // 必要な理由: greedy の best 1 個だけでなく、上位 beam_width 個を残して A2B 構築を試せるようにするため。
    static BeamResult run(int N, const State &initial_state, int beam_width) {
        assert_initial_state(N, initial_state);
        my_assert(beam_width > 0);
        BeamMove empty_move{0, 0, RA, RB, 0, (double) inf};
        BeamNode initial_node{State(initial_state), cmd_list_t(), 0, (double) inf, empty_move, false};
        my_vector<BeamNode> beam;
        beam.push_back(initial_node);
        std::ofstream debug_os = open_debug_log();
        log_beam(debug_os, 0, beam);
        for (int depth = 0; depth < N; depth++) {
            my_vector<BeamNode> candidates;
            for (int i = 0; i < beam.size(); i++) {
                append_candidates(N, beam[i], candidates);
            }
            my_assert(!candidates.empty());
            beam = select_next_beam(candidates, beam_width);
            log_beam(debug_os, depth + 1, beam);
        }
        my_assert(!beam.empty());
        BeamNode best_node = beam[0];
        my_assert(best_node.state.A.empty());
        write_commands(best_node.cmds);
        return {best_node.state, best_node.cmds, best_node.spent_turn, best_node.predict_all_turn};
    }
};

#endif //UNTITLED_A2BBEAMRUNNER_H
