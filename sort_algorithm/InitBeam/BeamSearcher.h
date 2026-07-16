#ifndef UNTITLED_BEAMSEARCHER_H
#define UNTITLED_BEAMSEARCHER_H

#include "all_include.h"
#include "State.h"
#include "StateHash.h"
#include "ankerl/unordered_dense.h"
#include "util.h"
#include "BeamEnums.h"
#include "BeamState.h"
#include "BeamQuery.h"
#include "BeamEvaluator.h"
#include "BeamInitializer.h"
#include "BeamOperations.h"
#include "RotateCost.h"
#include "BeamStateUtil.h"
#include "BeamStateRunner.h"
#include "RankingMetrics.h"
#include "BeamHashHelper.h"
#include "BeamQueryFactory.h"
#include "BeamStateValidator.h"
#include "Enums.h"
#include "StateCommandRecorder.h"
#include "A2AHelper.h"
#include "A2AHelper.h"
#include "Command/optimize/OptimizeMatch.h"
#include "../YakinamashiChunk/ChunkSeparator.h"
#include "QueryToCommandBuilder.h"

struct WasNodeInfo {
    BeamPos pos;
    int min_current_turn;
};

template<typename T, typename Compare = std::less<T>>
class KthValueFinder {
public:
    static T find_kth(const my_vector<T> &values, int K, Compare comp = Compare()) {
        my_assert(!values.empty());
        my_assert(K > 0 && K <= (int) values.size());

        my_vector<T> copy = values;
        nth_element(copy.begin(), copy.begin() + K, copy.end(), comp);

        T kth_value = copy[0];
        for (int i = 1; i < K; i++) {
            if (comp(kth_value, copy[i])) {
                kth_value = copy[i];
            }
        }
        return kth_value;
    }
};

//BN, cnt, min_ins, sum_ins, max_ins
//static my_vector<int>



template<int BeamWidth, int KExp, int MAX_N>
class BeamSearcher {
public:
    inline static my_vector<int> ins_cnt = my_vector<int>(MAX_N + 1, 0);
    inline static my_vector<int> min_ins = my_vector<int>(MAX_N + 1, numeric_limits<int>::max());
    inline static my_vector<int> max_ins = my_vector<int>(MAX_N + 1, numeric_limits<int>::min());
    inline static my_vector<my_vector<int>> all_ins = my_vector<my_vector<int> >(MAX_N + 1, my_vector<int>());

    inline static my_vector<long long> sum_ins = my_vector<long long>(MAX_N + 1, 0);

    inline static my_vector<int> ins_b_min = my_vector<int>(MAX_N + 1, 0);
    inline static my_vector<int> ins_b_max = my_vector<int>(MAX_N + 1, 0);
    inline static my_vector<int> ins_b_med = my_vector<int>(MAX_N + 1, 0);
    inline static bool ins_b_med_loaded = false;

    static void load_ins_b_med() {
        if (ins_b_med_loaded) return;
        ins_b_med_loaded = true;


        auto cwd = std::filesystem::current_path();
        auto root = cwd.parent_path(); // cmake-build-debug- の親

        std::filesystem::path file_path = root / "Input" / "ins_b_avg";
        if (!std::filesystem::exists(file_path)) {
            // //Optuna cout << std::filesystem::current_path() << endl;;
            std::filesystem::path alt_path = std::filesystem::current_path() / "Input" / "ins_b_avg";


            if (std::filesystem::exists(alt_path)) {
                file_path = alt_path;
            } else {
                return;
            }
        }

        std::ifstream file(file_path);
        if (!file.is_open()) {
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '/') continue;

            int bn, min_val, max_val;
            double avg;
            int med, cnt;

            std::istringstream iss(line);
            if (iss >> bn >> min_val >> max_val >> avg >> med >> cnt) {
                if (bn >= 0 && bn <= MAX_N) {
                    ins_b_min[bn] = min_val;
                    ins_b_max[bn] = max_val;
                    ins_b_med[bn] = med;
                }
            }
        }
        file.close();
    }

    void record_ins(int BN, int ins) {
        ins_cnt[BN]++;
        sum_ins[BN] += ins;
        min_ins[BN] = std::min(min_ins[BN], ins);
        max_ins[BN] = std::max(max_ins[BN], ins);
        all_ins[BN].push_back(ins);
    }

    void print_avg() {
        out("print_avg");
        out("bn, min, max, avg, cnt");
        for (int bn = 1; bn < MAX_N; bn++) {
            if (ins_cnt[bn] == 0)continue;
            std::sort(all_ins[bn].begin(), all_ins[bn].end());
            out(bn, min_ins[bn], max_ins[bn], (double) sum_ins[bn] / ins_cnt[bn], all_ins[bn][ins_cnt[bn] / 2],
                ins_cnt[bn]);
        }
    }

private:


    my_vector<my_vector<BeamState<MAX_N>>> beams_by_act_cnt;
    my_vector<my_vector<command>> initial_cmds_list;
    my_vector<BeamStateRunner> runners;
    my_vector<InitStateInfo<MAX_N>> init_state_info_list;
    int N;

    ankerl::unordered_dense::map<uint64_t, WasNodeInfo> was_node_by_sh;

    long long total_apply_count = 0;
    long long total_rollback_count = 0;
    long long total_seek_to_count = 0;
//    my_vector<long long> dep_seek_to_count;
//    my_vector<long long> dep_move_amount;
//    my_vector<my_vector<tuple<int, int, int, int>>> dep_move_records;

    vec_array<BeamQuery, 1000> b2a_queries;
    vec_array<BeamQuery, 1000> a2b_queries;
    vec_array<BeamQuery, 1000> a2a_queries;

    inline static long long static_total_apply_count = 0;
    inline static long long static_total_rollback_count = 0;
    inline static long long static_total_seek_to_count = 0;

    class KthSmallestQuery {
        int k;
        int sz = 0;

    public:
        my_vector<int> best_scores;

        explicit KthSmallestQuery(int k) : k(k) {
            best_scores.reserve(k);
        }

        void push(int score) {
            if (sz < k) {
                sz++;
                best_scores.push_back(score);
                // 後ろから挿入ソート
                for (int i = sz - 1; i > 0 && best_scores[i] < best_scores[i - 1]; --i) {
                    std::swap(best_scores[i], best_scores[i - 1]);
                }
                return;
            }

            // すでに k 個ある場合
            if (score >= best_scores[k - 1]) return;

            best_scores[k - 1] = score;
            // 末尾から前にバブルアップ
            for (int i = k - 1; i > 0 && best_scores[i] < best_scores[i - 1]; --i) {
                std::swap(best_scores[i], best_scores[i - 1]);
            }
        }

        int get_kth_score() const {
            my_assert(sz >= k);
            return best_scores[k - 1];
        }

        int size() const {
            my_assert(sz <= k);
            return sz;
        }
    };

    bool is_push_finish(const BeamState<MAX_N> &state) const {
        return state.sorted_a_count == N;
    }

    bool is_finish(const BeamStateRunner &runner, const BeamState<MAX_N> &state) {
        const State &st = runner.get_state();
        if (state.sorted_a_count != N) return false;
        return st.A[0] == 0;
    }

    static int find_zero_pos(const RingBuffer500 &A) {
        for (int i = 0; i < A.size(); i++) {
            if (A[i] == 0) return i;
        }
        my_assert(false);
        return -1;
    }

    void validate_is_finish(const BeamStateRunner &runner, const BeamState<MAX_N> &state) const {
#ifdef DEBUG_VALIDATE
        const State &st = runner.get_state();
        my_assert(st.current_N == N);
        my_assert(state.sorted_a_count == N);
        my_assert(st.B.empty());
        const auto &A = st.A;
        my_assert(A.size() == N);
        int zero_pos = find_zero_pos(A);
        for (int k = 0; k < N; k++) {
            int expected = k;
            int actual = A[(zero_pos + k) % N];
            my_assert(actual == expected);
        }
#endif
    }

    uint64_t calc_hash(const BeamState<MAX_N> &state) const {
        my_assert(state.init_id >= 0 && state.init_id < (int) init_state_info_list.size());
        uint64_t merged = deque_hash::merge_hash(state.a_hash, state.b_hash);
        const InitStateInfo<MAX_N> &init_info = init_state_info_list[state.init_id];
        uint64_t hash = merged ^ init_info.id_hash;

        //a2aならhashがマッチしないようにする(前に連なるa2a全部を見ないといけなくて、状態を同一視できるかの判定が難しいため)
        if (state.last_query.is_a2a()) {
            uint64_t a2a_hash_seed = (uint64_t) my_rand(RAND_MAX) | ((uint64_t) my_rand(RAND_MAX) << 32);
            uint64_t a2a_hash = make_id_hash(a2a_hash_seed);
            hash ^= a2a_hash;
        }

        return hash;
    }

    //@TODO  act_cntで進度を図るのは危険, max_act_cntまでいく事は少ない
    static double linear_noise(int act_cnt, int max_act_cnt, double max_noise) {
        my_assert(act_cnt <= max_act_cnt);
        double t = 1.0 - (double) act_cnt / max_act_cnt; // act_cnt=0 → 1, act_cnt=max → 0
        double u = (double) my_rand(RAND_MAX) / RAND_MAX;   // [0,1)
        return max_noise * t * u;
    }

    void select_beam_outer(my_vector<BeamState<MAX_N>> &candidates, int B, int act_cnt) {
        if (candidates.empty() || B <= 0) {
            my_assert(0);
            return;
        }

        static vec_array<tuple<double, int, int>, max(BeamWidth * KExp * 10, MAX_N)> scores_with_idx;
        scores_with_idx.clear();
//        scores_with_idx.reserve(base_cands_by_allord.size());
        int push_cnt = 0;
        for (int i = 0; i < candidates.size(); i++) {
            auto &st = candidates[i];
            if (st.is_deleted) continue;
            if (is_push_finish(st)) continue;
            scores_with_idx.emplace_back(st.score, my_rand(RAND_MAX), i);
            push_cnt++;
        }
        //上が同じidなら入れる


        if (scores_with_idx.size() <= B) {
            return;
        }

        nth_element(scores_with_idx.begin(), scores_with_idx.begin() + B, scores_with_idx.end());

        for (int del_i = B; del_i < scores_with_idx.size(); del_i++) {
            auto [score, ransuu, idx] = scores_with_idx[del_i];
            auto &st = candidates[idx];
            my_assert (!st.is_deleted);
            st.is_deleted = true;
        }
//        cout << "valid, id_idx_scores" << endl;
//        for (int i = 0; i < id_idx_scores.size(); i++){
//            cout<<"["<<i<<"] = "<<id_idx_scores[i]<<"), ";
//        }
//        cout<<endl;
// #ifdef DEBUG
//         int exist_not_finish_cnt = 0;
//         for (int i = 0; i < base_cands_by_allord.size(); i++) {
//             if (base_cands_by_allord[i].is_deleted)continue;
//             if (is_push_finish(base_cands_by_allord[i]))continue;
//             exist_not_finish_cnt += 1;
//         }
//         my_assert(exist_not_finish_cnt <= B);
// #endif
    }


    BeamPos find_beam_state_pos(const BeamState<MAX_N> &state) const {
        my_assert(state.act_cnt >= 0 && state.act_cnt < (int) beams_by_act_cnt.size());
        for (int i = 0; i < (int) beams_by_act_cnt[state.act_cnt].size(); i++) {
            const BeamState<MAX_N> &st = beams_by_act_cnt[state.act_cnt][i];
            if (st.is_deleted) continue;
            if (st.init_id == state.init_id &&
                st.parent_idx == state.parent_idx &&
                st.last_query == state.last_query) {
                return BeamPos(state.act_cnt, i);
            }
        }
        my_assert(false);
        return BeamPos(-1, -1);
    }

    int get_parent_act_cnt(int act_cnt, const BeamPos &beam_pos) const {
        my_assert(act_cnt > 0);
        int parent_act_cnt = act_cnt - 1;
#ifdef DEBUG
        my_assert(beam_pos.act_cnt == act_cnt);
        my_assert(beam_pos.idx >= 0 && beam_pos.idx < (int) beams_by_act_cnt[beam_pos.act_cnt].size());
        const BeamState<MAX_N> &st = beams_by_act_cnt[beam_pos.act_cnt][beam_pos.idx];
        validate_beam_pos_beam_st(beam_pos, st);
        my_assert (st.parent_idx >= 0);
        my_assert(parent_act_cnt >= 0 && parent_act_cnt < (int) beams_by_act_cnt.size());
        my_assert(st.parent_idx < (int) beams_by_act_cnt[parent_act_cnt].size());
        const BeamState<MAX_N> &parent_st = beams_by_act_cnt[parent_act_cnt][st.parent_idx];
        my_assert(parent_st.act_cnt == parent_act_cnt);
#endif
        return parent_act_cnt;
    }

    int get_parent_act_cnt(int act_cnt, const BeamState<MAX_N> &state) const {
        my_assert(act_cnt > 0);
        int parent_act_cnt = act_cnt - 1;
#ifdef DEBUG
        my_assert(state.act_cnt == act_cnt);
        my_assert(state.parent_idx >= 0);
        my_assert(parent_act_cnt >= 0 && parent_act_cnt < (int) beams_by_act_cnt.size());
        my_assert(state.parent_idx < (int) beams_by_act_cnt[parent_act_cnt].size());
        const BeamState<MAX_N> &parent_st = beams_by_act_cnt[parent_act_cnt][state.parent_idx];
        my_assert(parent_st.act_cnt == parent_act_cnt);
#endif
        return parent_act_cnt;
    }

    void validate_beam_pos_beam_st(const BeamPos &pos, const BeamState<MAX_N> &beam_st) const {
#ifdef DEBUG
        my_assert(pos.act_cnt >= 0 && pos.act_cnt < (int) beams_by_act_cnt.size());
        my_assert(pos.idx >= 0 && pos.idx < (int) beams_by_act_cnt[pos.act_cnt].size());
        my_assert(pos.act_cnt == beam_st.act_cnt);
        const BeamState<MAX_N> &st = beams_by_act_cnt[pos.act_cnt][pos.idx];
        my_assert(st.init_id == beam_st.init_id);
        my_assert(st.parent_idx == beam_st.parent_idx);
        my_assert(st.last_query == beam_st.last_query);
#endif
    }

    static int calc_max_act_cnt(int total_n) {
        return total_n * 2;
    }

    BeamPos get_parent(const BeamPos &beam_pos) {
        my_assert(beam_pos.act_cnt > 0);
        int parent_act_cnt = get_parent_act_cnt(beam_pos.act_cnt, beam_pos);
        return BeamPos(parent_act_cnt, beams_by_act_cnt[beam_pos.act_cnt][beam_pos.idx].parent_idx);
    }

//    my_vector<BeamQuery> get_query_sequence(const BeamState &state) const {
//        my_vector<BeamQuery> queries;
//        BeamState cur = state;
//        BeamPos cur_pos(state.sorted_a_count, -1);
//        for (int i = 0; i < (int) beams_by_act_cnt[cur_pos.dep].size(); i++) {
//            if (beams_by_act_cnt[cur_pos.dep][i].init_id == state.init_id &&
//                beams_by_act_cnt[cur_pos.dep][i].parent_idx == state.parent_idx &&
//                beams_by_act_cnt[cur_pos.dep][i].last_query == state.last_query) {
//                cur_pos.idx = i;
//                break;
//            }
//        }
//        my_assert(cur_pos.idx >= 0);
//        while (cur.sorted_a_count > cur.lis_length && cur.parent_idx >= 0) {
//            queries.push_back(cur.last_query);
//            int parent_dep = cur_pos.dep - 1;
//            my_assert(parent_dep >= 0 && parent_dep < (int) beams_by_act_cnt.size());
//            my_assert(cur.parent_idx < (int) beams_by_act_cnt[parent_dep].size());
//            cur = beams_by_act_cnt[parent_dep][cur.parent_idx];
//            cur_pos = BeamPos(parent_dep, cur.parent_idx);
//        }
//        reverse(queries.begin(), queries.end());
//        return queries;
//    }

    int seek_to(const BeamPos &target_pos) {
        my_assert(target_pos.act_cnt < (int) beams_by_act_cnt.size());
        my_assert(target_pos.idx < (int) beams_by_act_cnt[target_pos.act_cnt].size());
        const BeamState<MAX_N> &target_state = beams_by_act_cnt[target_pos.act_cnt][target_pos.idx];
        validate_beam_pos_beam_st(target_pos, target_state);
        my_assert(target_state.init_id >= 0 && target_state.init_id < (int) runners.size());
        BeamStateRunner &runner = runners[target_state.init_id];
        auto start_runner_pos = runner.get_pos();
        if (start_runner_pos == target_pos) {
            my_assert(measure_seek_to_move_amount(start_runner_pos, target_pos) == 0);
            return 0;
        }

        //この前提で、runnerのidを割り振る
//        my_assert(abs(start_runner_pos.act_cnt - target_pos.act_cnt) <= 1);
        BeamPos move_goal_post = target_pos;
        my_assert(MAX_N < 600);
        static vec_array<BeamPos, 600> lca_to_target;
        lca_to_target.clear();
        int apply_count = 0;
        int rollback_count = 0;
        auto move_parent_runner = [&]() {
            BeamPos cur_pos = runner.get_pos();
            auto &cur_beam_st = beams_by_act_cnt[cur_pos.act_cnt][cur_pos.idx];
            //todo 知識の散逸 act_cntの前が-1なのは現状の仕様に依存している
            auto &prev_beam_st = beams_by_act_cnt[cur_beam_st.act_cnt - 1][cur_beam_st.parent_idx];
            const BeamQuery &prev_q = prev_beam_st.last_query;
            runner.rollback_query<MAX_N>(beams_by_act_cnt, prev_q);
            rollback_count++;
        };
        auto move_parent_goal = [&]() {
            lca_to_target.emplace_back(move_goal_post);//reverseする
            move_goal_post = get_parent(move_goal_post);
        };
        while (runner.get_pos().act_cnt > move_goal_post.act_cnt) {
            move_parent_runner();
        }
        while (move_goal_post.act_cnt > runner.get_pos().act_cnt) {
            move_parent_goal();
        }
        while (runner.get_pos() != move_goal_post) {
            move_parent_runner();
            move_parent_goal();
        }
        std::reverse(lca_to_target.begin(), lca_to_target.end());
        for (const BeamPos &next_pos: lca_to_target) {
            my_assert(next_pos.act_cnt > 0);
            my_assert(next_pos.idx < (int) beams_by_act_cnt[next_pos.act_cnt].size());
            const BeamState<MAX_N> &st = beams_by_act_cnt[next_pos.act_cnt][next_pos.idx];
            my_assert(st.parent_idx >= 0);
            int parent_act_cnt = next_pos.act_cnt - 1;
            my_assert(parent_act_cnt >= 0 && parent_act_cnt < (int) beams_by_act_cnt.size());
            my_assert(st.parent_idx < (int) beams_by_act_cnt[parent_act_cnt].size());
            const BeamState<MAX_N> &parent_st = beams_by_act_cnt[parent_act_cnt][st.parent_idx];
            runner.apply_query(st.last_query, next_pos, beams_by_act_cnt, parent_st.last_query);
            runner.validate_sorted_st(beams_by_act_cnt);
            apply_count++;
        }
        my_assert(runner.get_pos() == target_pos);

        total_seek_to_count++;
        total_apply_count += apply_count;
        total_rollback_count += rollback_count;

        int move_amount = apply_count + rollback_count;
//        cout << "seek id " << target_state.init_id << " fdep " << start_runner_pos.dep << " fidx "
//             << start_runner_pos.idx << " tdep " << target_pos.dep << " tidx "
//             << target_pos.idx << " amount " << move_amount << endl;

//        if (target_pos.dep < (int) dep_seek_to_count.size()) {
//            dep_seek_to_count[target_pos.dep]++;
//            dep_move_amount[target_pos.dep] += move_amount;
//            if (target_pos.dep < (int) dep_move_records.size()) {
//                dep_move_records[target_pos.dep].emplace_back(start_runner_pos.dep, start_runner_pos.idx,
//                                                              target_pos.idx, move_amount);
//            }
//        }
        my_assert(measure_seek_to_move_amount(start_runner_pos, target_pos) == move_amount);
        return move_amount;
    }

    int measure_seek_to_move_amount(const BeamPos &current_pos, const BeamPos &target_pos) {
//        BeamPos current_pos = runners[beams_by_act_cnt[target_pos.act_cnt][target_pos.idx].init_id].get_pos();
        if (current_pos == target_pos) {
            return 0;
        }
        my_assert(target_pos.act_cnt < (int) beams_by_act_cnt.size());
        my_assert(target_pos.idx < (int) beams_by_act_cnt[target_pos.act_cnt].size());
        my_assert(current_pos.act_cnt >= 0 && current_pos.act_cnt < (int) beams_by_act_cnt.size());
        my_assert(current_pos.idx >= 0 && current_pos.idx < (int) beams_by_act_cnt[current_pos.act_cnt].size());

        BeamPos cur_pos = current_pos;
        BeamPos move_goal_post = target_pos;

        int apply_count = 0;
        int rollback_count = 0;

        auto move_parent_cur = [&]() {
            my_assert(cur_pos.act_cnt > 0);
            cur_pos = get_parent(cur_pos);
            rollback_count++;
        };
        auto move_parent_goal = [&]() {
            move_goal_post = get_parent(move_goal_post);
            apply_count++;
        };

        while (cur_pos.act_cnt > move_goal_post.act_cnt) {
            move_parent_cur();
        }
        while (move_goal_post.act_cnt > cur_pos.act_cnt) {
            move_parent_goal();
        }
        while (cur_pos != move_goal_post) {
            move_parent_cur();
            move_parent_goal();
        }


        return apply_count + rollback_count;
    }

    bool is_garbage_a(int v, const BeamState<MAX_N> &st) const {
        return !st.is_sorted_v[v];
    }

    void validate_state_and_min_a_pos_min_a_v(const State &st, int new_min_a_pos, int new_min_a_v,
                                              const BeamState<MAX_N> &beam_st) {
#ifdef DEBUG_VALIDATE
        int min_v = numeric_limits<int>::max();
        int min_i = -1;
        for (int i = 0; i < st.A.size(); i++) {
            if (is_garbage_a(st.A[i], beam_st)) {
                continue;
            }
            if (chmi(min_v, st.A[i])) {
                min_v = st.A[i];
                min_i = i;
            }
        }

        my_assert(min_i == new_min_a_pos);
        my_assert(min_v == new_min_a_v);
#endif
    }

    pair<int, int>
    get_new_min_a_pos_and_v(const BeamStateRunner &runner, int min_a_v, const BeamQuery &query) const {
        const State &st = runner.get_state();

        int new_min_a_v = min_a_v;
        if ((query.get_push_type() == e_push_type::b2a || query.get_push_type() == e_push_type::a2a) &&
            query.get_tar_val() < min_a_v) {
            new_min_a_v = query.get_tar_val();
        }

        const StackWithPos &stack_pos_a = runner.get_stack_a_pos();
        int new_min_a_pos = stack_pos_a.position(new_min_a_v);
        my_assert(0 <= new_min_a_pos && new_min_a_pos < st.A.size());

        return {new_min_a_pos, new_min_a_v};
    }

    double calc_new_yet_sorted_v_sum(const BeamState<MAX_N> &parent_state, const BeamQuery &query) {
        if (query.get_push_type() == e_push_type::b2a) {
            const State &st = runners[parent_state.init_id].get_state();
            my_assert(st.current_N == N);
            return parent_state.yet_sorted_dis_to_0_sum - min(query.get_tar_val(), N - query.get_tar_val());
        } else if (query.get_push_type() == e_push_type::a2b) {
            return parent_state.yet_sorted_dis_to_0_sum;
        } else if (query.get_push_type() == e_push_type::a2a) {
            const State &st = runners[parent_state.init_id].get_state();
            my_assert(st.current_N == N);
            return parent_state.yet_sorted_dis_to_0_sum - min(query.get_tar_val(), N - query.get_tar_val());
        } else {
            my_assert(false);
            return -1;
        }
    }

    my_vector<BeamQuery> get_prev_a2a_queries(const BeamState<MAX_N> &beam_st) const {
        my_vector<BeamQuery> prev_a2a_queries;
        BeamPos cur_pos(beam_st.act_cnt, beam_st.cur_idx);

        while (cur_pos.act_cnt > 0) {
            const BeamState<MAX_N> &cur_beam_st = beams_by_act_cnt[cur_pos.act_cnt][cur_pos.idx];
            const BeamQuery &last_query = cur_beam_st.last_query;
            if (last_query.is_a2a()) {
                prev_a2a_queries.push_back(last_query);
            } else {
                break;
            }

            int parent_act_cnt = get_parent_act_cnt(cur_pos.act_cnt, cur_pos);
            if (parent_act_cnt < 0 || cur_beam_st.parent_idx < 0) {
                break;
            }
            cur_pos = BeamPos(parent_act_cnt, cur_beam_st.parent_idx);
        }

        reverse(prev_a2a_queries.begin(), prev_a2a_queries.end());
        return prev_a2a_queries;
    }


    pair<uint64_t, uint64_t> calc_new_hash(const BeamState<MAX_N> &parent_state, const BeamQuery &query,
                                           BeamStateRunner &runner, State &st) {

        //a2aはタイプによって終わった時の状態が違うので、prev_a2aを変更する場合、そのずれを修正する必要がある
#ifdef DEBUG
        StateHash sh(st);
#endif
        uint64_t new_a_hash = parent_state.a_hash;
        uint64_t new_b_hash = parent_state.b_hash;
        StateCommandRecorder<get_state_cmd_recorder_size<MAX_N>()> st_cmd_rec(st);
        int prev_a2a_diff_ra = 0, prev_a2a_diff_rra = 0;
        auto &prev_q = parent_state.last_query;
//        inline pair<int, int> change_type_diff(const State &state, const BeamQuery &a2a_query, e_a2a_type new_type) {
        if (prev_q.is_valid_query() && prev_q.get_push_type() == e_push_type::a2a) {
            tie(prev_a2a_diff_ra, prev_a2a_diff_rra) = change_type_diff(st, prev_q,
                                                                        query.get_determined_prev_a2a_type());
            my_assert(prev_a2a_diff_ra >= 0 && prev_a2a_diff_rra >= 0);
            if (prev_a2a_diff_ra > 0) {
                my_assert(prev_a2a_diff_ra == 1);
                tie(new_a_hash, new_b_hash) = StateHash::get_do_cmd_nth_hash(new_a_hash, new_b_hash, RA, 1, st);
                st_cmd_rec.do_cmd(RA);
#ifdef DEBUG
                sh.do_cmd(RA);
#endif
            }
            if (prev_a2a_diff_rra > 0) {
                my_assert(prev_a2a_diff_rra == 1);
                tie(new_a_hash, new_b_hash) = StateHash::get_do_cmd_nth_hash(new_a_hash, new_b_hash, RRA, 1, st);
                st_cmd_rec.do_cmd(RRA);
#ifdef DEBUG
                sh.do_cmd(RRA);
#endif
            }
        }
        if (query.is_a2b_b2a()) {
            if (query.get_a_count() > 0) {
                tie(new_a_hash, new_b_hash) = StateHash::get_do_cmd_nth_hash(new_a_hash, new_b_hash, query.get_a_cmd(),
                                                                             query.get_a_count(), st);
#ifdef DEBUG
                for (int i = 0; i < query.get_a_count(); i++) {
                    sh.do_cmd(query.get_a_cmd());
                }
                my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
            }

            if (query.get_b_count() > 0) {
                tie(new_a_hash, new_b_hash) = StateHash::get_do_cmd_nth_hash(new_a_hash, new_b_hash, query.get_b_cmd(),
                                                                             query.get_b_count(), st);
#ifdef DEBUG
                for (int i = 0; i < query.get_b_count(); i++) {
                    sh.do_cmd(query.get_b_cmd());
                }
                my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
            }
            if (query.get_push_type() == e_push_type::b2a) {
                int push_b_i;
                if (query.get_b_count() == 0) {
                    push_b_i = 0;
                } else if (query.get_b_cmd() == RB) {
                    push_b_i = query.get_b_count();
                } else if (query.get_b_cmd() == RRB) {
                    push_b_i = st.B.size() - query.get_b_count();
                } else {
                    my_assert(0);
                }
                my_assert(0 <= push_b_i && push_b_i < st.B.size());
                tie(new_a_hash, new_b_hash) = StateHash::apply_command_to_hash_PA(new_a_hash, new_b_hash,
                                                                                  st.B[push_b_i],
                                                                                  st.A.size(), st.B.size());
            } else if (query.get_push_type() == e_push_type::a2b) {
                int push_a_i;
                if (query.get_a_count() == 0) {
                    push_a_i = 0;
                } else if (query.get_a_cmd() == RA) {
                    push_a_i = query.get_a_count();
                } else if (query.get_a_cmd() == RRA) {
                    push_a_i = st.A.size() - query.get_a_count();
                } else {
                    my_assert(0);
                }
                my_assert(0 <= push_a_i && push_a_i < st.A.size());
                tie(new_a_hash, new_b_hash) = StateHash::apply_command_to_hash_PB(new_a_hash, new_b_hash,
                                                                                  st.A[push_a_i],
                                                                                  st.A.size(), st.B.size());
            } else if (query.get_push_type() == e_push_type::a2a) {
                my_assert(false);
            } else {
                my_assert(false);
            }
#ifdef DEBUG
            if (query.get_push_type() == e_push_type::b2a) {
                sh.do_cmd(PA);
            } else if (query.get_push_type() == e_push_type::a2b) {
                sh.do_cmd(PB);
            } else if (query.get_push_type() == e_push_type::a2a) {
                my_assert(false);
            } else {
                my_assert(false);
            }
            my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
        } else if (query.is_a2a()) {
            my_assert(!prev_q.is_valid_query() || !prev_q.is_a2a() ||
                      query.get_determined_prev_a2a_type() == prev_q.get_cur_default_a2a_type());
            auto [cmd1, q_cnt1] = query.get_cmd1_cnt1_(query.get_cur_default_a2a_type(), query.get_a_size());
            if (cmd1 == RA) {
                tie(new_a_hash, new_b_hash) = StateHash::get_do_cmd_nth_hash(new_a_hash, new_b_hash, RA, q_cnt1, st);
                for (int _ = 0; _ < q_cnt1; _++) {
                    st_cmd_rec.do_cmd(RA);
                }
#ifdef DEBUG
                for (int _ = 0; _ < q_cnt1; _++) {
                    sh.do_cmd(RA);
                }
                my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
            } else if (cmd1 == RRA) {
                tie(new_a_hash, new_b_hash) = StateHash::get_do_cmd_nth_hash(new_a_hash, new_b_hash, RRA, q_cnt1, st);
                for (int _ = 0; _ < q_cnt1; _++) {
                    st_cmd_rec.do_cmd(RRA);
                }
#ifdef DEBUG
                for (int _ = 0; _ < q_cnt1; _++) {
                    sh.do_cmd(RRA);
                }
                my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
            } else {
                my_assert(false);
            }
            int flip_dis_dir = query.get_flip_dis_dir();
            my_assert(flip_dis_dir != 0);
            int abs_flip_dis_dir = abs(flip_dis_dir);


            if (query.get_cur_default_a2a_type() == e_a2a_type::swap_rot &&
                flip_dis_dir < 0
                    ) {
                //ra 1かい
                tie(new_a_hash, new_b_hash) = StateHash::get_do_cmd_nth_hash(new_a_hash, new_b_hash, RA, 1, st);
                st_cmd_rec.do_cmd(RA);
#ifdef DEBUG
                sh.do_cmd(RA);
                my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
            }
            tie(new_a_hash, new_b_hash) = StateHash::apply_command_to_hash_PB(new_a_hash, new_b_hash, st.A[0],
                                                                              st.A.size(),
                                                                              st.B.size());
            st_cmd_rec.do_cmd(PB);
#ifdef DEBUG
            sh.do_cmd(PB);
            my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
            if (flip_dis_dir > 0) {
                //raをabs_flip回
                tie(new_a_hash, new_b_hash) = StateHash::get_do_cmd_nth_hash(new_a_hash, new_b_hash, RA,
                                                                             abs_flip_dis_dir,
                                                                             st);
                for (int _ = 0; _ < abs_flip_dis_dir; _++) {
                    st_cmd_rec.do_cmd(RA);
                }
#ifdef DEBUG
                for (int _ = 0; _ < abs_flip_dis_dir; _++) {
                    sh.do_cmd(RA);
                }
                my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
            } else {
                my_assert(flip_dis_dir < 0);
                //rraをabs_flip回
                tie(new_a_hash, new_b_hash) = StateHash::get_do_cmd_nth_hash(new_a_hash, new_b_hash, RRA,
                                                                             abs_flip_dis_dir,
                                                                             st);
                for (int _ = 0; _ < abs_flip_dis_dir; _++) {
                    st_cmd_rec.do_cmd(RRA);
                }
#ifdef DEBUG
                for (int _ = 0; _ < abs_flip_dis_dir; _++) {
                    sh.do_cmd(RRA);
                }
                my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
            }
            //pa
            tie(new_a_hash, new_b_hash) = StateHash::apply_command_to_hash_PA(new_a_hash, new_b_hash, st.B[0],
                                                                              st.A.size(),
                                                                              st.B.size());
            st_cmd_rec.do_cmd(PA);
#ifdef DEBUG
            sh.do_cmd(PA);
            my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
            if (query.get_cur_default_a2a_type() == e_a2a_type::swap_rot &&
                flip_dis_dir > 0
                    ) {
                //rra 1回
                tie(new_a_hash, new_b_hash) = StateHash::get_do_cmd_nth_hash(new_a_hash, new_b_hash, RRA, 1, st);
                st_cmd_rec.do_cmd(RRA);
#ifdef DEBUG
                sh.do_cmd(RRA);
                my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
            }
#ifdef DEBUG
            StateHash sh_by_st(st);
            my_assert(sh_by_st.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
            my_assert(sh.A.all_hash == new_a_hash && sh.B.all_hash == new_b_hash);
#endif
        }
        st_cmd_rec.revert_all();


        return {new_a_hash, new_b_hash};
    }

    void add_applied_query(const BeamState<MAX_N> &parent_state, const BeamQuery &query,
                           int parent_act_cnt, int parent_pos_in_turn) {
        //a2aはタイプによって終わった時の状態が違うので、prev_a2aを変更する場合、そのずれを修正する必要がある
        my_assert(parent_state.init_id >= 0 && parent_state.init_id < (int) runners.size());
        BeamStateRunner &runner = runners[parent_state.init_id];

        BeamPos parent_pos(parent_act_cnt, parent_pos_in_turn);
        seek_to(parent_pos);
        my_assert(runner.get_pos() == parent_pos);
#ifdef DEBUG
        SortedStateA sorted_st_a_bef = runner.get_sorted_state_a();
        State st_bef = runner.get_state();
        StateHash sh_bef(st_bef);
        StateHash sh(st_bef);
        my_assert(sh_bef.A.all_hash == parent_state.a_hash);
        my_assert(sh_bef.B.all_hash == parent_state.b_hash);

        auto &prev_q = parent_state.last_query;
        if (prev_q.is_valid_query()) {
            bool prev_is_a2a = prev_q.is_a2a();
            if (prev_is_a2a) {
                my_assert(query.get_determined_prev_a2a_type() != e_a2a_type::not_a2a);
            } else {
                my_assert(query.get_determined_prev_a2a_type() == e_a2a_type::not_a2a);
            }
        } else {
            my_assert(query.get_determined_prev_a2a_type() == e_a2a_type::not_a2a);
        }
#endif
        int next_sorted_a_count;
        if (query.get_push_type() == e_push_type::b2a) {
            const State &st_before = runner.get_state();
            next_sorted_a_count = parent_state.sorted_a_count + 1;
        } else if (query.get_push_type() == e_push_type::a2b) {
            next_sorted_a_count = parent_state.sorted_a_count;
        } else if (query.get_push_type() == e_push_type::a2a) {
            next_sorted_a_count = parent_state.sorted_a_count + 1;
        } else {
            my_assert(false);
        }
        State &st = runner.get_state();
        auto [new_a_hash, new_b_hash] = calc_new_hash(parent_state, query, runner, st);
#ifdef DEBUG
        my_assert(st.A == st_bef.A && st.B == st_bef.B);
#endif
        int next_act_cnt = parent_act_cnt + 1;
        int next_idx = (int) beams_by_act_cnt[next_act_cnt].size();
        BeamPos next_pos(next_act_cnt, next_idx);
        runner.apply_query(query, next_pos, beams_by_act_cnt, parent_state.last_query);

        const State &st_after = runner.get_state();
#ifdef DEBUG
        {
            StateHash sh_by_runner(runner.get_state());
            my_assert(sh_by_runner.A.all_hash == new_a_hash &&
                      sh_by_runner.B.all_hash == new_b_hash
            );
            //だめだったら、calc_new_hashをもう一回呼んで、stateが最終的にどうなるか比較する
        };
#endif
        my_assert(st_after.current_N <= MAX_N);
        double next_sum = calc_new_yet_sorted_v_sum(parent_state, query);
        const InitStateInfo<MAX_N> &init_info = init_state_info_list[parent_state.init_id];
        my_bitset<MAX_N> next_is_sorted_v = parent_state.is_sorted_v;
        if (query.get_push_type() == e_push_type::b2a) {
            next_is_sorted_v[query.get_tar_val()] = true;
        } else if (query.get_push_type() == e_push_type::a2b) {
            // a2bの場合はnext_is_sorted_vは変化しない
        } else if (query.get_push_type() == e_push_type::a2a) {
            next_is_sorted_v[query.get_tar_val()] = true;
        } else {
            my_assert(0);
        }
        my_bitset<MAX_N> next_a2b_skipped_vals = parent_state.a2b_skipped_vals;
        if (query.get_push_type() == e_push_type::a2b) {
            const my_vector<int> &skip_vals = query.get_a2b_skip_vals();
            for (int v: skip_vals) {
                next_a2b_skipped_vals[v] = true;
            }
        }
        auto [new_min_a_pos, new_min_a_v] = get_new_min_a_pos_and_v(runner, parent_state.min_a_v, query);
        BeamState<MAX_N> next_state(parent_state.init_id,
                                    parent_state.current_turn + query.get_ins_turn_with_prev_diff(),
                                    next_sorted_a_count,
                                    parent_pos_in_turn, query, 0.0, next_sum,
                                    next_act_cnt, next_idx,
                                    new_min_a_pos, new_min_a_v,
                                    new_a_hash, new_b_hash,
                                    next_is_sorted_v,
                                    next_a2b_skipped_vals);
//        next_state.set_a_dis_sum(calc_a_dis_sum(next_state, runner));
        validate_state_and_min_a_pos_min_a_v(st_after, new_min_a_pos, new_min_a_v, next_state);

        int lis_length = (int) init_state_info_list[parent_state.init_id].lis_list.size();

//        int ins_all_b_cost = calc_ins_all_b_cost(runner.get_state(), runner, next_state);
        next_state.score = BeamEvaluator::evaluate_rough<MAX_N>(runner, next_state,
                                                                lis_length/*, ins_all_b_cost,  ins_b_min, ins_b_med, ins_b_max*/);
        beams_by_act_cnt[next_act_cnt].push_back(next_state);
        beams_by_act_cnt[next_act_cnt][next_idx].cur_idx = next_idx;
        runner.validate_sorted_st(beams_by_act_cnt);//beam_by_act_cntにstateが追加されないと呼べないのでここで呼ぶ
        validate_beam_pos_beam_st(next_pos, next_state);


//        int get_queries(const BeamPos &tar_pos, int K, int &move_amount) {
//        int _;
//        get_queries(next_pos, 1, _);
//        next_state.score =next_state.score-3.5 + queries[0].ins_turn;

        runner.rollback_query(beams_by_act_cnt, parent_state.last_query);
        my_assert(runner.get_pos() == parent_pos);

        uint64_t next_hash = calc_hash(next_state);
        auto it = was_node_by_sh.find(next_hash);
        if (it != was_node_by_sh.end()) {
            // out("add_applied_query: hash found in was_node_by_sh");
            const WasNodeInfo &already_info = it->second;
            const BeamPos &already_pos = already_info.pos;
            BeamState<MAX_N> &already_st = beams_by_act_cnt[already_pos.act_cnt][already_pos.idx];
            my_assert(already_st.init_id >= 0 && already_st.init_id < (int) runners.size());

            //a2b, b2aの違うコマンドによって、同じ状態になる事がある
//            if (already_st.last_query.tar_val != next_state.last_query.tar_val) {
//                seek_to(BeamPos(next_state.act_cnt - 1, next_state.parent_idx));
//                State prev_next_st = runners[next_state.init_id].get_state();
//                seek_to(BeamPos(already_st.act_cnt-1, already_st.parent_idx));
//                State prev_already_st = runners[already_st.init_id].get_state();
//                ;
////                prev_next_st =
//            }

            int current_turn = next_state.current_turn;
            int min_current_turn = already_info.min_current_turn;
            // out("add_applied_query: current_turn=", current_turn, " min_current_turn=", min_current_turn);

            //同じ手数だったら見ない(以前の細いビームサーチで弾かれたという事は、仮に実はそれが強かったとしても、どちらにせよ細いビームサーチでそれを測る事は難しい)
            //外側の場合その限りではないが、簡単のために改善された場合だけ見る
            if (current_turn < min_current_turn) {
                // out("add_applied_query: branch: current_turn <= min_current_turn, marking old node as deleted");
                already_st.is_deleted = true;
                it->second.pos = next_pos;
                it->second.min_current_turn = current_turn;
            } else {
                // out("add_applied_query: branch: current_turn > min_current_turn, popping back new node");
                beams_by_act_cnt[next_act_cnt].pop_back();
            }
            return;
        }
        // out("add_applied_query: hash not found in was_node_by_sh, adding new node");
        WasNodeInfo new_info;
        new_info.pos = next_pos;
        new_info.min_current_turn = next_state.current_turn;
        was_node_by_sh[next_hash] = new_info;
    }


public:

    BeamSearcher() : a2b_queries(), b2a_queries(), a2a_queries() {
        load_ins_b_med();
    }

    const my_vector<my_vector<BeamState<MAX_N>>> &get_beam_by_sorted_a_cnt() const { return beams_by_act_cnt; }

    const my_vector<my_vector<command>> &get_initial_cmds_list() const { return initial_cmds_list; }

    const State &get_state(const BeamState<MAX_N> &state) {
        my_assert(state.init_id >= 0 && state.init_id < (int) runners.size());
        BeamPos pos = find_beam_state_pos(state);
        validate_beam_pos_beam_st(pos, state);
        seek_to(pos);
        return runners[state.init_id].get_state();
    }

    //nex_i, part_cmds, ra_sum, rra_sum
    static tuple<int, my_vector<command>, int, int>
    beam_queries_to_a2a_commands(const State &init_st, const my_vector<BeamQuery> &qs, int cur_qi) {
        int ra_sum = 0;
        int rra_sum = 0;
        my_vector<command> part_cmds;
        my_assert(cur_qi < qs.size());
        for (; cur_qi < qs.size(); cur_qi++) {
            auto &q = qs[cur_qi];
            if (!q.is_a2a()) {
                break;
            }
            auto add_ra_rra_sum = [&](command cmd) {
                if (cmd == RA) {
                    ra_sum += 1;
                } else if (cmd == RRA) {
                    rra_sum += 1;
                } else {
                    my_assert(false);
                }
            };
            auto add_cmds_part = [&](e_a2a_type new_a2a_type) {
                my_vector<command> cmds_;
                auto [cmd1, cnt1] = q.get_cmd1_cnt1_(new_a2a_type, q.get_a_size());
                int flip_dis_dir = q.get_flip_dis_dir();
                for (int _ = 0; _ < cnt1; _++) {
                    cmds_.push_back(cmd1);
                    add_ra_rra_sum(cmd1);
                }
                int abs_flip_dis_dir = abs(flip_dis_dir);
                if (new_a2a_type == e_a2a_type::swap_rot) {
                    if (flip_dis_dir > 0) {
                        cmds_.push_back(SA);
                        for (int _ = 0; _ < abs_flip_dis_dir - 1; _++) {
                            add_ra_rra_sum(RA);
                            cmds_.push_back(RA);
                            cmds_.push_back(SA);
                        }
                    } else {
                        cmds_.push_back(SA);
                        for (int _ = 0; _ < abs_flip_dis_dir - 1; _++) {
                            add_ra_rra_sum(RRA);
                            cmds_.push_back(RRA);
                            cmds_.push_back(SA);
                        }
                    }
                } else if (new_a2a_type == e_a2a_type::push_rot) {
                    cmds_.push_back(PB);
                    if (flip_dis_dir > 0) {
                        for (int _ = 0; _ < abs_flip_dis_dir; _++) {
                            cmds_.push_back(RA);
                        }
                    } else {
                        for (int _ = 0; _ < abs_flip_dis_dir; _++) {
                            cmds_.push_back(RRA);
                        }
                    }
                    cmds_.push_back(PA);
                } else {
                    my_assert(false);
                }
                part_cmds.insert(part_cmds.end(), cmds_.begin(), cmds_.end());
            };
            e_a2a_type new_a2a_type = q.get_cur_default_a2a_type();
            if (cur_qi + 1 < qs.size()) {
                const auto &next_q = qs[cur_qi + 1];
                new_a2a_type = next_q.get_determined_prev_a2a_type();
            }
            add_cmds_part(new_a2a_type);
        }
        return {cur_qi, part_cmds, ra_sum, rra_sum};
    }


    //a2bやb2aの時、直前にa2aが連なっている時に
    //swap_rotなら全範囲
    //push_rotなら、cmd1の部分とsyncできる
    //たとえばpush_rotで、ra*4, pb, rra*2, paという時に
    //次にa2b等がある時に, 最初のra*4とsync出来る
    static my_vector<command> beam_queries_to_commands(const State &init_st, const my_vector<BeamQuery> &qs) {
        CommandBuildState build_st(init_st);
        QueryToCommandBuilder builder(build_st);
        int predict_cmd_sum = 0;
        for (int i = 0; i < qs.size(); i++) {
            int pre_cmd_size = build_st.cmds.size();
            predict_cmd_sum += qs[i].get_ins_turn_with_prev_diff();
            if (qs[i].get_push_type() == e_push_type::a2a) {
                e_a2a_type a2a_type = qs[i].get_cur_default_a2a_type();
                if (i + 1 < qs.size()) {
                    a2a_type = qs[i + 1].get_determined_prev_a2a_type();
                }
                auto [cmd1, cnt1] = qs[i].get_cmd1_cnt1_(a2a_type, qs[i].get_a_size());
                int flip_dis_dir = qs[i].get_flip_dis_dir();
                my_assert(flip_dis_dir != 0);
                builder.add_a2a(a2a_type, cmd1, cnt1, flip_dis_dir);
                //a2a_typeが後ろによって決められる都合上、a2aのコマンド数はvalidateできない
            } else {
                builder.add_a2b_b2a(qs[i].get_push_type(), qs[i].get_a_cmd(), qs[i].get_b_cmd(), qs[i].get_a_count(),
                                    qs[i].get_b_count());
                my_assert(predict_cmd_sum == build_st.cmds.size());
            }
        }
        return build_st.cmds;
    }

    static my_vector<command> past_beam_queries_to_commands(const State &init_st, const my_vector<BeamQuery> &qs) {
//        my_vector<command> prev_cmds_default;
//        my_vector<command> prev_cmds_next_det;
//        my_vector<command> cur_cmds_default;//default, next_ter
//        my_vector<command> cur_cmds_next_det;//default, next_ter
//        State st(init_st);
//
        State st(init_st);
        my_vector<command> cmds;
        int predict_cmd_sum = 0;
        auto add_a2b_b2a = [&](e_push_type push_type, command a_cmd, command b_cmd, int a_cnt, int b_cnt) {
            my_assert(a_cnt >= 0 && b_cnt >= 0);
            if ((a_cmd == RA && b_cmd == RB) || (a_cmd == RRA && b_cmd == RRB)) {
                command both_cmd = (a_cmd == RA) ? RR : RRR;
                int common = min(a_cnt, b_cnt);
                int rem_a = a_cnt - common;
                int rem_b = b_cnt - common;
                for (int i = 0; i < rem_a; i++) cmds.push_back(a_cmd);
                for (int i = 0; i < rem_b; i++) cmds.push_back(b_cmd);
                for (int i = 0; i < common; i++) cmds.push_back(both_cmd);
            } else {
                if (a_cmd == RA) for (int i = 0; i < a_cnt; i++) cmds.push_back(RA);
                else if (a_cmd == RRA) for (int i = 0; i < a_cnt; i++) cmds.push_back(RRA);
                if (b_cmd == RB) for (int i = 0; i < b_cnt; i++) cmds.push_back(RB);
                else if (b_cmd == RRB) for (int i = 0; i < b_cnt; i++) cmds.push_back(RRB);
            }
            if (push_type == e_push_type::a2b) {
                cmds.push_back(PB);
            } else if (push_type == e_push_type::b2a) {
                cmds.push_back(PA);
            } else {
                my_assert(false);
            }
        };
        for (int i = 0; i < qs.size();) {
//a2aじゃない次
            auto [nex_i, part_cmds, ra_sum, rra_sum] = beam_queries_to_a2a_commands(init_st, qs, i);
            my_assert(nex_i == qs.size() || qs[nex_i].get_push_type() != e_push_type::a2a);
            if (nex_i == qs.size()) {
                cmds.insert(cmds.end(), part_cmds.begin(), part_cmds.end());
                i = nex_i;
//            } else if (qs[nex_i].get_push_type() == e_push_type::a2b) {
//                cmds.insert(cmds.end(), part_cmds.begin(), part_cmds.end());
//                auto &nex_q = qs[nex_i];
//                add_a2b_b2a(nex_q.get_push_type(), nex_q.get_a_cmd(), nex_q.get_b_cmd(), nex_q.get_a_count(),
//                            nex_q.get_b_count());
//                i = nex_i + 1;
            } else {
                auto &nex_q = qs[nex_i];
                my_assert(nex_q.is_a2b_b2a());
                command a_cmd = nex_q.get_a_cmd();
                command b_cmd = nex_q.get_b_cmd();
                int a_cnt = nex_q.get_a_count();
                int b_cnt = nex_q.get_b_count();
                int sync_b_cnt;
                if (b_cmd == RB) {
                    sync_b_cnt = min(ra_sum, b_cnt);
                } else {
                    my_assert(b_cmd == RRB);
                    sync_b_cnt = min(rra_sum, b_cnt);
                }
                int sync_b_rem = sync_b_cnt;
                for (auto &cmd: part_cmds) {
                    if (b_cmd == RB) {
                        if (cmd == RA && sync_b_rem > 0) {
                            cmds.push_back(RR);
                            sync_b_rem--;
                        } else {
                            cmds.push_back(cmd);
                        }
                    } else {
                        my_assert(b_cmd == RRB);
                        if (cmd == RRA && sync_b_rem > 0) {
                            cmds.push_back(RRR);
                            sync_b_rem--;
                        } else {
                            cmds.push_back(cmd);
                        }
                    }
                }
                add_a2b_b2a(nex_q.get_push_type(), a_cmd, b_cmd, a_cnt, b_cnt - sync_b_cnt);
                i = nex_i + 1;
            }
        }
        for (auto &cmd: cmds) {
            st.do_cmd(cmd);
        }
        st.print();
        return cmds;
    }

//    static my_vector<command> beam_queries_to_commands(const State &init_st, const my_vector<BeamQuery> &qs) {
//        my_vector<command> prev_cmds_default;
//        my_vector<command> prev_cmds_next_det;
//        my_vector<command> cur_cmds_default;//default, next_ter
//        my_vector<command> cur_cmds_next_det;//default, next_ter
//        State st(init_st);
//
//        int predict_cmd_sum = 0;
//        //a2b, b2aまで追加
//        for (int i = 0; i < qs.size(); i++) {
//            const auto &q = qs[i];
//            my_vector<command> cmds_part;
//            predict_cmd_sum += q.get_ins_turn_with_prev_diff();
//
//            int rot_count = 0;
//            if (q.is_a2b_b2a()) {
//                if ((q.get_a_cmd() == RA && q.get_b_cmd() == RB) || (q.get_a_cmd() == RRA && q.get_b_cmd() == RRB)) {
//                    command both_cmd = (q.get_a_cmd() == RA) ? RR : RRR;
//                    int common = min(q.get_a_count(), q.get_b_count());
//                    int rem_a = q.get_a_count() - common;
//                    int rem_b = q.get_b_count() - common;
//                    for (int i = 0; i < rem_a; i++) cmds_part.push_back(q.get_a_cmd());
//                    for (int i = 0; i < rem_b; i++) cmds_part.push_back(q.get_b_cmd());
//                    for (int i = 0; i < common; i++) cmds_part.push_back(both_cmd);
//                    rot_count = max(q.get_a_count(), q.get_b_count());
//                } else {
//                    if (q.get_a_cmd() == RA) for (int i = 0; i < q.get_a_count(); i++) cmds_part.push_back(RA);
//                    else if (q.get_a_cmd() == RRA) for (int i = 0; i < q.get_a_count(); i++) cmds_part.push_back(RRA);
//                    if (q.get_b_cmd() == RB) for (int i = 0; i < q.get_b_count(); i++) cmds_part.push_back(RB);
//                    else if (q.get_b_cmd() == RRB) for (int i = 0; i < q.get_b_count(); i++) cmds_part.push_back(RRB);
//                    rot_count = q.get_a_count() + q.get_b_count();
//                }
//                if (q.get_push_type() == e_push_type::a2b) {
//                    cmds_part.push_back(PB);
//                } else if (q.get_push_type() == e_push_type::b2a) {
//                    cmds_part.push_back(PA);
//                } else {
//                    my_assert(false);
//                }
//
////                out("optimized_len", optimized_cmds.size());
//                out("predict_cmd_sum", predict_cmd_sum);
//                cur_cmds_default.insert(cur_cmds_default.end(), cmds_part.begin(), cmds_part.end());
//                cur_cmds_next_det.insert(cur_cmds_next_det.end(), cmds_part.begin(), cmds_part.end());
//
//                ns_optimize_cmd::optimize_match_c mt;
//                cur_cmds_default = mt.optimize_sync(cur_cmds_default);
//                cur_cmds_next_det = mt.optimize_sync(cur_cmds_next_det);
//            } else if (q.is_a2a()) {
//                auto get_cmds_part = [&](e_a2a_type new_a2a_type) {
//                    my_vector<command> cmds_;
//                    auto [cmd1, cnt1] = q.get_cmd1_cnt1_(new_a2a_type, q.get_a_size());
//                    int flip_dis_dir = q.get_flip_dis_dir();
//                    for (int _ = 0; _ < cnt1; _++) {
//                        cmds_.push_back(cmd1);
//                    }
//                    int abs_flip_dis_dir = abs(flip_dis_dir);
//                    if (new_a2a_type == e_a2a_type::swap_rot) {
//                        if (flip_dis_dir > 0) {
//                            cmds_.push_back(SA);
//                            for (int _ = 0; _ < abs_flip_dis_dir - 1; _++) {
//                                cmds_.push_back(RA);
//                                cmds_.push_back(SA);
//                            }
//                        } else {
//                            cmds_.push_back(SA);
//                            for (int _ = 0; _ < abs_flip_dis_dir - 1; _++) {
//                                cmds_.push_back(RRA);
//                                cmds_.push_back(SA);
//                            }
//                        }
//                    } else if (new_a2a_type == e_a2a_type::push_rot) {
//                        cmds_.push_back(PB);
//                        if (flip_dis_dir > 0) {
//                            for (int _ = 0; _ < abs_flip_dis_dir; _++) {
//                                cmds_.push_back(RA);
//                            }
//                        } else {
//                            for (int _ = 0; _ < abs_flip_dis_dir; _++) {
//                                cmds_.push_back(RRA);
//                            }
//                        }
//                        cmds_.push_back(PA);
//                    } else {
//                        my_assert(false);
//                    }
//                    return cmds_;
//                };
//
//                e_a2a_type new_a2a_type = q.get_cur_default_a2a_type();
//                if (i + 1 < qs.size()) {
//                    const auto &next_q = qs[i + 1];
//                    new_a2a_type = next_q.get_determined_prev_a2a_type();
//                }
//                auto default_part_cmds = get_cmds_part(q.get_cur_default_a2a_type());
//                cmds_part = get_cmds_part(new_a2a_type);
//                out("qi", i);
//                out("new_a2a", new_a2a_type == e_a2a_type::push_rot ? "push_rot" : new_a2a_type == e_a2a_type::swap_rot
//                                                                                   ? "swap_rot" : "not");
//                out("default_part_cmds");
//                out(default_part_cmds);
//                out("cmds_part");
//                out(cmds_part);
//                cur_cmds_next_det.insert(cur_cmds_next_det.end(), cmds_part.begin(), cmds_part.end());
//                cur_cmds_default.insert(cur_cmds_default.end(), default_part_cmds.begin(), default_part_cmds.end());
//
//
//                ns_optimize_cmd::optimize_match_c mt;
//                cur_cmds_default = mt.optimize_sync(cur_cmds_default);
//                cur_cmds_next_det = mt.optimize_sync(cur_cmds_next_det);
//            } else {
//                my_assert(false);
//            }
//            out("----------------");
//            {
//                //-----------
//                ns_optimize_cmd::optimize_match_c mt;
//                my_vector<command> optimized_cmds = mt.optimize_sync(cur_cmds_default);
//                my_assert((int) optimized_cmds.size() == predict_cmd_sum);
//
//                out("optimized_len", optimized_cmds.size());
//                out("predict_cmd_sum", predict_cmd_sum);
//                if (optimized_cmds.size() != predict_cmd_sum) {
//                    out("error!!!!!!!");
//                }
//
//            }
//            out(q);
//            out(cmds_part);
//
//            prev_cmds_default = cur_cmds_default;
//            prev_cmds_next_det = cur_cmds_next_det;
//
//            State st_new(init_st);
//            for (auto &cmd: cur_cmds_next_det) {
//                st_new.do_cmd(cmd);
//            }
//            st = st_new;
//        }
//        return cur_cmds_next_det;
//    }

    void validate_reconstruct() {
        auto f = [&](const BeamState<MAX_N> &target) {
            my_vector<BeamQuery> rev_qs;
            BeamState<MAX_N> cur_beam_st = target;
            BeamPos cur_pos = find_beam_state_pos(target);
            my_vector<BeamPos> beam_poss;
            beam_poss.push_back(cur_pos);
            while (cur_pos.act_cnt > 0) {
                rev_qs.push_back(cur_beam_st.last_query);
                int parent_act_cnt = get_parent_act_cnt(cur_beam_st.act_cnt, cur_beam_st);
                my_assert(parent_act_cnt >= 0 && parent_act_cnt < (int) beams_by_act_cnt.size());
                my_assert(cur_beam_st.parent_idx < (int) beams_by_act_cnt[parent_act_cnt].size());
                cur_pos = BeamPos(parent_act_cnt, cur_beam_st.parent_idx);
                cur_beam_st = beams_by_act_cnt[parent_act_cnt][cur_beam_st.parent_idx];
                beam_poss.push_back(cur_pos);
                if (cur_pos.idx >= 0)
                    validate_beam_pos_beam_st(cur_pos, cur_beam_st);
            }
        };
        for (int act_cnt = 0; act_cnt < beams_by_act_cnt.size(); act_cnt++) {
            for (int idx = 0; idx < beams_by_act_cnt[act_cnt].size(); idx++) {
                f(beams_by_act_cnt[act_cnt][idx]);
            }
        }
    }

    tuple<my_vector<BeamQuery>, my_vector<command>>
    reconstruct_commands(const State &init_st, const BeamState<MAX_N> &target) {
        my_vector<BeamQuery> rev_qs;
        BeamState<MAX_N> cur_beam_st = target;
        BeamPos cur_pos = find_beam_state_pos(target);
        validate_beam_pos_beam_st(cur_pos, cur_beam_st);
        my_vector<BeamPos> pos_path;
        while (cur_pos.act_cnt > 0) {
            pos_path.push_back(cur_pos);
            rev_qs.push_back(cur_beam_st.last_query);
            int parent_act_cnt = get_parent_act_cnt(cur_beam_st.act_cnt, cur_beam_st);
            my_assert(parent_act_cnt >= 0 && parent_act_cnt < (int) beams_by_act_cnt.size());
            my_assert(cur_beam_st.parent_idx < (int) beams_by_act_cnt[parent_act_cnt].size());
            cur_pos = BeamPos(parent_act_cnt, cur_beam_st.parent_idx);
            cur_beam_st = beams_by_act_cnt[parent_act_cnt][cur_beam_st.parent_idx];
            validate_beam_pos_beam_st(cur_pos, cur_beam_st);
        }
        pos_path.push_back(cur_pos);
        std::reverse(pos_path.begin(), pos_path.end());

        reverse(rev_qs.begin(), rev_qs.end());
        my_assert(cur_beam_st.init_id >= 0 && cur_beam_st.init_id < (int) initial_cmds_list.size());
        my_vector<command> cmds = beam_queries_to_commands(init_st, rev_qs);

        //cmdズレるけど放置
        my_assert(target.current_turn == cmds.size());
        int ra_dis = target.min_a_pos;
        int rra_dis = N - target.min_a_pos;
        if (ra_dis < rra_dis) {
            for (int _ = 0; _ < ra_dis; _++) {
                cmds.push_back(RA);
            }
        } else {
            for (int _ = 0; _ < rra_dis; _++) {
                cmds.push_back(RRA);
            }
        }
//#ifdef DEBUG
        State st(init_st);
        for (auto cmd: cmds) {
            st.do_cmd(cmd);
        }
        if (st.A.size() != N) {
            assert_catch();
        }
        my_vector<State> states;
        for (int i = 0; i < N; i++) {
            if (st.A[i] != i) {
                cout << "in error" << endl;
                State st2(init_st);
                for (int turn = 0; turn < cmds.size(); turn++) {
                    out("turn", turn);
                    st2.print();
                    st2.do_cmd(cmds[turn]);
                    states.push_back(st2);
                }

                int error_v = st.A[i];
                for (auto &q: rev_qs) {
                    if (q.get_tar_val() == error_v) {
                        assert_catch();
                    }
                }
            }
            if (st.A[i] != i) {
                assert_catch();
            }
        }
//#endif
        return {rev_qs, cmds};
    }

    static int get_ra_dis(const State &st, int a_pos) {
        my_assert(0 <= a_pos && a_pos < st.A.size());
        return a_pos;
    }

    static int get_rra_dis(const State &st, int a_pos) {
        my_assert(0 <= a_pos && a_pos < st.A.size());
        if (a_pos == 0) {
            return 0;
        } else {
            return st.A.size() - a_pos;
        }
    }

    static int get_rb_dis(const State &st, int b_pos) {
        my_assert(b_pos == 0 || (0 <= b_pos && b_pos < st.B.size()));
        return b_pos;
    }

    static int get_rrb_dis(const State &st, int b_pos) {
        my_assert(b_pos == 0 || (0 <= b_pos && b_pos < st.B.size()));
        if (b_pos == 0) {
            return 0;
        } else {
            return st.B.size() - b_pos;
        }
    }

    static tuple<command, command, int, int> calc_optimal_a2b_rot(const State &st, int a_pos, int b_pos) {
        my_assert(0 <= a_pos && a_pos < st.A.size());
        my_assert(b_pos == 0 || (0 <= b_pos && b_pos < st.B.size()));

        int ra_dis = get_ra_dis(st, a_pos);
        int rra_dis;
        if (ra_dis == 0) {
            rra_dis = MAX_N * 10;
        } else {
            rra_dis = st.A.size() - ra_dis;
        }
        int rb_dis = get_rb_dis(st, b_pos);
        int rrb_dis;
        if (rb_dis == 0) {
            rrb_dis = MAX_N * 10;
        } else {
            rrb_dis = st.B.size() - rb_dis;
        }

        int cost_ra_rb = calc_joint_rot_cost(ra_dis, rb_dis, true);
        int cost_rra_rrb = calc_joint_rot_cost(rra_dis, rrb_dis, true);
        int cost_ra_rrb = calc_joint_rot_cost(ra_dis, rrb_dis, false);
        int cost_rra_rb = calc_joint_rot_cost(rra_dis, rb_dis, false);

        int min_cost = cost_ra_rb;
        min_cost = min(min_cost, cost_rra_rrb);
        min_cost = min(min_cost, cost_ra_rrb);
        min_cost = min(min_cost, cost_rra_rb);

        command a_cmd;
        command b_cmd;
        int a_cnt;
        int b_cnt;
        if (min_cost == cost_ra_rb) {
            a_cmd = RA;
            b_cmd = RB;
            a_cnt = ra_dis;
            b_cnt = rb_dis;
        } else if (min_cost == cost_rra_rrb) {
            a_cmd = RRA;
            b_cmd = RRB;
            a_cnt = rra_dis;
            b_cnt = rrb_dis;
        } else if (min_cost == cost_ra_rrb) {
            a_cmd = RA;
            b_cmd = RRB;
            a_cnt = ra_dis;
            b_cnt = rrb_dis;
        } else {
            a_cmd = RRA;
            b_cmd = RB;
            a_cnt = rra_dis;
            b_cnt = rb_dis;
        }
        return {a_cmd, b_cmd, a_cnt, b_cnt};
    }

//todo リファクタここじゃないばしょにあるべき
    static int calc_ins_turn(bool can_sync, int a_cnt, int b_cnt) {
        if (can_sync) {
            return max(a_cnt, b_cnt) + 1;
        } else {
            return a_cnt + b_cnt + 1;
        }
    }

//todo リファクタここじゃないばしょにあるべき
    static int calc_ins_turn(command a_cmd, command b_cmd, int a_cnt, int b_cnt) {
        bool can_sync = can_sync_cmd(a_cmd, b_cmd);
        return calc_ins_turn(can_sync, a_cnt, b_cnt);
    }

//Verified by test_set_cur_connect_info
//@COMMENTED_OUT: まだどういう形で使うか決まってないのでコメントアウト
/*
static void set_cur_connect_info(const BeamQuery &prev_q, BeamQuery &cur_q) {
    my_assert(prev_q.cmd1 == RA || prev_q.cmd1 == RRA);
    my_assert(cur_q.cmd1 == RA || cur_q.cmd1 == RRA);
    my_assert(prev_q.cmd2 == RB || prev_q.cmd2 == RRB);
    my_assert(cur_q.cmd2 == RB || cur_q.cmd2 == RRB);
    my_assert(prev_q.tar_val >= 0);
    int new_pre_a_cnt = prev_q.connect_info.new_a_count;
    int new_pre_b_cnt = prev_q.connect_info.new_b_count;
    cur_q.connect_info.new_a_count = cur_q.amount1;
    cur_q.connect_info.new_b_count = cur_q.b_count;

    if (prev_q.cmd1 == command::RA && prev_q.amount1 > 0
        && cur_q.cmd1 == command::RRA && cur_q.amount1 > 0) {
        //前にsa
        new_pre_a_cnt--;
        my_assert(new_pre_a_cnt >= 0);
        cur_q.connect_info.new_a_count--;
        cur_q.connect_info.make_cur_sa = true;
    }
    if (prev_q.cmd2 == command::RB && prev_q.b_count > 0
        && cur_q.cmd2 == command::RRB && cur_q.b_count > 0) {
        new_pre_b_cnt--;
        my_assert(new_pre_b_cnt >= 0);
        cur_q.connect_info.new_b_count--;
        cur_q.connect_info.make_pre_sb = true;
    }
    bool can_sync_prev = can_sync_cmd(prev_q.cmd1, prev_q.cmd2);
    //前のsb, saが連結されるか
    bool valid_ss = false;
    if (cur_q.connect_info.make_pre_sb
        && prev_q.connect_info.make_cur_sa) {
        if (!can_sync_prev) {
            valid_ss = true;
        } else if (new_pre_a_cnt == 0 || new_pre_b_cnt == 0) {
            valid_ss = true;
        } else {
            valid_ss = false;
        }
    }
    int new_pre_ins_turn;
    new_pre_ins_turn = calc_ins_turn(can_sync_prev, new_pre_a_cnt, new_pre_b_cnt);
    new_pre_ins_turn += cur_q.connect_info.make_pre_sb + prev_q.connect_info.make_cur_sa - valid_ss;

    cur_q.connect_info.prev_diff_turn = new_pre_ins_turn - prev_q.connect_info.new_cur_turn;
    cur_q.connect_info.new_cur_turn =
            cur_q.connect_info.make_cur_sa +
            calc_ins_turn(cur_q.cmd1, cur_q.cmd2, cur_q.connect_info.new_a_count, cur_q.connect_info.new_b_count);
}
*/

    PrevA2AInfo create_a2a_prev_info(const BeamState<MAX_N> &st, const State &state) {
        my_vector<BeamQuery> prev_a2a_queries = get_prev_a2a_queries(st);
        int if_push_prev_diff = 0;
        int if_swap_prev_diff = 0;
        //最後に何を選ぶか
        int swap_prev_ra_sum = 0;
        int push_prev_ra_sum = 0;
        int swap_prev_rra_sum = 0;
        int push_prev_rra_sum = 0;
        bool prev_is_a2a = !prev_a2a_queries.empty();
        int if_push_last_a_pos_diff = 0;
        int if_swap_last_a_pos_diff = 0;

        if (!prev_a2a_queries.empty()) {
            auto &prev_q = prev_a2a_queries.back();
            my_assert (prev_q.get_push_type() == e_push_type::a2a);
            bool if_swap_v_next = is_if_swap_v_next(prev_q);
            if (prev_q.get_cur_default_a2a_type() == e_a2a_type::push_rot) {
                if_push_prev_diff = 0;
                if_swap_prev_diff =
                        calc_a2a_ins_turn_with_type(prev_q, e_a2a_type::swap_rot) - prev_q.get_ins_turn_simple();
                if_push_last_a_pos_diff = 0;
                if_swap_last_a_pos_diff = if_swap_v_next ? 1 : 0;
            } else if (prev_q.get_cur_default_a2a_type() == e_a2a_type::swap_rot) {
                if_swap_prev_diff = 0;
                if_push_prev_diff =
                        calc_a2a_ins_turn_with_type(prev_q, e_a2a_type::push_rot) - prev_q.get_ins_turn_simple();
                if_push_last_a_pos_diff = if_swap_v_next ? -1 : 0;
                if_swap_last_a_pos_diff = 0;
            } else {
                my_assert(false);
            }

            auto get_ra_rra_add = [&](BeamQuery &q, e_a2a_type a2a_type) -> pair<int, int> {
                auto [cmd1, cnt1] = q.get_cmd1_cnt1_(a2a_type, q.get_a_size());
                int ra_add = 0;
                int rra_add = 0;
                if (cmd1 == RA) {
                    ra_add += cnt1;
                } else if (cmd1 == RRA) {
                    rra_add += cnt1;
                } else {
                    my_assert(false);
                }
                int flip = q.get_flip_dis_dir();
                int abs_flip = abs(flip);
                my_assert(flip != 0);
                if (a2a_type == e_a2a_type::swap_rot) {
                    if (flip > 0) {
                        ra_add += abs_flip - 1;
                    } else {
                        rra_add += abs_flip - 1;
                    }
                } else if (a2a_type == e_a2a_type::push_rot) { ;//足さない
                } else {
                    my_assert(false);
                }
                return {ra_add, rra_add};
            };
            for (int i = 0; i + 1 < prev_a2a_queries.size(); i++) {
                auto &q = prev_a2a_queries[i];
                auto [ra_add, rra_add] = get_ra_rra_add(q, q.get_cur_default_a2a_type());
                push_prev_ra_sum += ra_add;
                push_prev_rra_sum += rra_add;
                swap_prev_ra_sum += ra_add;
                swap_prev_rra_sum += rra_add;
            }
            auto [push_ra_add, push_rra_add] = get_ra_rra_add(prev_a2a_queries.back(), e_a2a_type::push_rot);
            auto [swap_ra_add, swap_rra_add] = get_ra_rra_add(prev_a2a_queries.back(), e_a2a_type::swap_rot);
            push_prev_ra_sum += push_ra_add;
            push_prev_rra_sum += push_rra_add;
            swap_prev_ra_sum += swap_ra_add;
            swap_prev_rra_sum += swap_rra_add;
        }

        return PrevA2AInfo(if_push_prev_diff, push_prev_ra_sum, push_prev_rra_sum,
                           if_swap_prev_diff, swap_prev_ra_sum, swap_prev_rra_sum, prev_is_a2a,
                           if_push_last_a_pos_diff, if_swap_last_a_pos_diff);
    }

    struct RotateDis {
        int min_ra_dis;
        int max_ra_dis;
        int min_rra_dis;
        int max_rra_dis;
    };

    static RotateDis calc_rotate_dis(int a_pos_first, int a_pos_last, int a_size) {
        RotateDis result;
        if (a_pos_first <= a_pos_last) {
            result.min_ra_dis = a_pos_first;
            result.max_ra_dis = a_pos_last;
        } else {
            result.min_ra_dis = 0;
            result.max_ra_dis = a_pos_last;
        }

        if (a_pos_first <= a_pos_last) {
            if (a_pos_first == 0) {
                result.min_rra_dis = 0;
                result.max_rra_dis = 0;
            } else {
                result.min_rra_dis = a_size - a_pos_last;
                result.max_rra_dis = a_size - a_pos_first;
            }
        } else {
            result.min_rra_dis = 0;
            result.max_rra_dis = a_size - a_pos_first;
        }
        return result;
    }

//[first, last]
    static my_vector<BeamQuery>
    build_b2a_queries(const State &st, int default_a_pos_first, int default_a_pos_last, int b_pos,
                      const PrevA2AInfo &prev_info, const my_vector<BeamQuery> &prev_a2a_queries) {

        if (!prev_info.is_prev_a2a()) {
            RotateDis rotate_dis = calc_rotate_dis(default_a_pos_first, default_a_pos_last, st.A.size());
            int min_ra_dis = rotate_dis.min_ra_dis;
            int max_ra_dis = rotate_dis.max_ra_dis;
            int min_rra_dis = rotate_dis.min_rra_dis;
            int max_rra_dis = rotate_dis.max_rra_dis;
            int rb_dis = get_rb_dis(st, b_pos);
            int rrb_dis = get_rrb_dis(st, b_pos);

            int cost_ra_rb = calc_joint_rot_cost(min_ra_dis, rb_dis, true);
            int cost_rra_rrb = calc_joint_rot_cost(min_rra_dis, rrb_dis, true);
            int cost_ra_rrb = calc_joint_rot_cost(min_ra_dis, rrb_dis, false);
            int cost_rra_rb = calc_joint_rot_cost(min_rra_dis, rb_dis, false);

            int min_turn = cost_ra_rb;
            min_turn = min(min_turn, cost_rra_rrb);
            min_turn = min(min_turn, cost_ra_rrb);
            min_turn = min(min_turn, cost_rra_rb);
            min_turn += 1;

            command a_cmd, b_cmd;
            int a_count_cl = 0;
            int a_count_cr = 0;
            int b_count = 0;
            if (min_turn == cost_ra_rb + 1) {
                a_cmd = RA;
                b_cmd = RB;
                a_count_cl = min_ra_dis;
                if (min_ra_dis >= rb_dis) {
                    a_count_cr = a_count_cl;
                } else {
                    a_count_cr = min(max_ra_dis, rb_dis);
                }
                b_count = rb_dis;
            } else if (min_turn == cost_rra_rrb + 1) {
                a_cmd = RRA;
                b_cmd = RRB;
                a_count_cl = min_rra_dis;
                if (min_rra_dis >= rrb_dis) {
                    a_count_cr = a_count_cl;
                } else {
                    a_count_cr = min(max_rra_dis, rrb_dis);
                }
                b_count = rrb_dis;
            } else if (min_turn == cost_ra_rrb + 1) {
                a_cmd = RA;
                b_cmd = RRB;
                a_count_cl = min_ra_dis;
                a_count_cr = min_ra_dis;
                b_count = rrb_dis;
            } else {
                a_cmd = RRA;
                b_cmd = RB;
                a_count_cl = min_rra_dis;
                a_count_cr = min_rra_dis;
                b_count = rb_dis;
            }
            my_vector<BeamQuery> ret_queries;
            for (int a_count = a_count_cl; a_count <= a_count_cr; a_count++) {
                BeamQuery q(st.B[b_pos], min_turn, a_count, b_count, a_cmd, b_cmd, e_push_type::b2a,
                            e_a2a_type::not_a2a, 0, e_a2a_type::not_a2a, st.A.size());
                validate_ins_turn(q, prev_a2a_queries, prev_info);
                ret_queries.emplace_back(q);
            }
            return ret_queries;
        } else {
            int if_push_a_pos_first = mod(default_a_pos_first + prev_info.if_push_last_a_pos_diff, st.A.size());
            int if_push_a_pos_last = mod(default_a_pos_last + prev_info.if_push_last_a_pos_diff, st.A.size());
            int if_swap_a_pos_first = mod(default_a_pos_first + prev_info.if_swap_last_a_pos_diff, st.A.size());
            int if_swap_a_pos_last = mod(default_a_pos_last + prev_info.if_swap_last_a_pos_diff, st.A.size());

            RotateDis if_push_rotate_dis = calc_rotate_dis(if_push_a_pos_first, if_push_a_pos_last, st.A.size());
            RotateDis if_swap_rotate_dis = calc_rotate_dis(if_swap_a_pos_first, if_swap_a_pos_last, st.A.size());

            int rb_dis = get_rb_dis(st, b_pos);
            int rrb_dis = get_rrb_dis(st, b_pos);
            int prev_push_rem_rb = u0(rb_dis - prev_info.push_prev_ra_sum);
            int prev_push_rem_rrb = u0(rrb_dis - prev_info.push_prev_rra_sum);
            int prev_swap_rem_rb = u0(rb_dis - prev_info.swap_prev_ra_sum);
            int prev_swap_rem_rrb = u0(rrb_dis - prev_info.swap_prev_rra_sum);

            enum e_best_type {
                prev_push_ra_rb_,
                prev_push_rra_rrb_,
                prev_swap_ra_rb_,
                prev_swap_rra_rrb_,
                prev_push_ra_rrb_,
                prev_push_rra_rb_,
                prev_swap_ra_rrb_,
                prev_swap_rra_rb_,
            };
            e_best_type best_type;
            //このクエリを使う事による、手数の差分
            int prev_push_ra_rb = prev_info.if_push_prev_diff +
                                  calc_joint_rot_cost(if_push_rotate_dis.min_ra_dis, prev_push_rem_rb, true);
            int prev_push_rra_rrb =
                    prev_info.if_push_prev_diff +
                    calc_joint_rot_cost(if_push_rotate_dis.min_rra_dis, prev_push_rem_rrb, true);
            int prev_swap_ra_rb = prev_info.if_swap_prev_diff +
                                  calc_joint_rot_cost(if_swap_rotate_dis.min_ra_dis, prev_swap_rem_rb, true);
            int prev_swap_rra_rrb =
                    prev_info.if_swap_prev_diff +
                    calc_joint_rot_cost(if_swap_rotate_dis.min_rra_dis, prev_swap_rem_rrb, true);
            int prev_push_ra_rrb =
                    prev_info.if_push_prev_diff +
                    calc_joint_rot_cost(if_push_rotate_dis.min_ra_dis, prev_push_rem_rrb, false);
            int prev_push_rra_rb =
                    prev_info.if_push_prev_diff +
                    calc_joint_rot_cost(if_push_rotate_dis.min_rra_dis, prev_push_rem_rb, false);
            int prev_swap_ra_rrb =
                    prev_info.if_swap_prev_diff +
                    calc_joint_rot_cost(if_swap_rotate_dis.min_ra_dis, prev_swap_rem_rrb, false);
            int prev_swap_rra_rb =
                    prev_info.if_swap_prev_diff +
                    calc_joint_rot_cost(if_swap_rotate_dis.min_rra_dis, prev_swap_rem_rb, false);

            best_type = prev_push_ra_rb_;
            int min_cost = prev_push_ra_rb;
            if (chmi(min_cost, prev_push_rra_rrb)) {
                best_type = prev_push_rra_rrb_;
            }
            if (chmi(min_cost, prev_swap_ra_rb)) {
                best_type = prev_swap_ra_rb_;
            }
            if (chmi(min_cost, prev_swap_rra_rrb)) {
                best_type = prev_swap_rra_rrb_;
            }
            if (chmi(min_cost, prev_push_ra_rrb)) {
                best_type = prev_push_ra_rrb_;
            }
            if (chmi(min_cost, prev_push_rra_rb)) {
                best_type = prev_push_rra_rb_;
            }
            if (chmi(min_cost, prev_swap_ra_rrb)) {
                best_type = prev_swap_ra_rrb_;
            }
            if (chmi(min_cost, prev_swap_rra_rb)) {
                best_type = prev_swap_rra_rb_;
            }

            e_a2a_type best_prev_a2a_type;
            command best_a_cmd, best_b_cmd;
            int prev_a2a_ins_turn_diff = 0;

            if (best_type == prev_push_ra_rb_ || best_type == prev_push_rra_rrb_ ||
                best_type == prev_push_ra_rrb_ || best_type == prev_push_rra_rb_) {
                best_prev_a2a_type = e_a2a_type::push_rot;
                prev_a2a_ins_turn_diff = prev_info.if_push_prev_diff;
            } else {
                best_prev_a2a_type = e_a2a_type::swap_rot;
                prev_a2a_ins_turn_diff = prev_info.if_swap_prev_diff;
            }

            if (best_type == prev_push_ra_rb_ || best_type == prev_swap_ra_rb_) {
                best_a_cmd = RA;
                best_b_cmd = RB;
            } else if (best_type == prev_push_rra_rrb_ || best_type == prev_swap_rra_rrb_) {
                best_a_cmd = RRA;
                best_b_cmd = RRB;
            } else if (best_type == prev_push_ra_rrb_ || best_type == prev_swap_ra_rrb_) {
                best_a_cmd = RA;
                best_b_cmd = RRB;
            } else {
                best_a_cmd = RRA;
                best_b_cmd = RB;
            }

            int min_turn = min_cost + 1;
            int a_count_cl = 0;
            int a_count_cr = 0;
            int b_count = 0;

            RotateDis best_rotate_dis = (best_prev_a2a_type == e_a2a_type::push_rot) ? if_push_rotate_dis
                                                                                     : if_swap_rotate_dis;
            if (best_a_cmd == RA && best_b_cmd == RB) {
                int rem_rb = (best_prev_a2a_type == e_a2a_type::push_rot) ? prev_push_rem_rb : prev_swap_rem_rb;
                a_count_cl = best_rotate_dis.min_ra_dis;
                if (best_rotate_dis.min_ra_dis >= rem_rb) {
                    a_count_cr = a_count_cl;
                } else {
                    a_count_cr = min(best_rotate_dis.max_ra_dis, rem_rb);
                }
                b_count = rb_dis;
            } else if (best_a_cmd == RRA && best_b_cmd == RRB) {
                int rem_rrb = (best_prev_a2a_type == e_a2a_type::push_rot) ? prev_push_rem_rrb : prev_swap_rem_rrb;
                a_count_cl = best_rotate_dis.min_rra_dis;
                if (best_rotate_dis.min_rra_dis >= rem_rrb) {
                    a_count_cr = a_count_cl;
                } else {
                    a_count_cr = min(best_rotate_dis.max_rra_dis, rem_rrb);
                }
                b_count = rrb_dis;
            } else if (best_a_cmd == RA && best_b_cmd == RRB) {
                int rem_rrb = (best_prev_a2a_type == e_a2a_type::push_rot) ? prev_push_rem_rrb : prev_swap_rem_rrb;
                a_count_cl = best_rotate_dis.min_ra_dis;
                a_count_cr = best_rotate_dis.min_ra_dis;
                b_count = rrb_dis;
            } else {
                int rem_rb = (best_prev_a2a_type == e_a2a_type::push_rot) ? prev_push_rem_rb : prev_swap_rem_rb;
                a_count_cl = best_rotate_dis.min_rra_dis;
                a_count_cr = best_rotate_dis.min_rra_dis;
                b_count = rb_dis;
            }

            my_vector<BeamQuery> ret_queries;
            for (int a_count = a_count_cl; a_count <= a_count_cr; a_count++) {
                BeamQuery q(st.B[b_pos], min_turn, a_count, b_count,
                            best_a_cmd, best_b_cmd, e_push_type::b2a,
                            best_prev_a2a_type, prev_a2a_ins_turn_diff, e_a2a_type::not_a2a, st.A.size());
                validate_ins_turn(q, prev_a2a_queries, prev_info);
                ret_queries.emplace_back(q);
            }
            return ret_queries;
        }
    }


private:

    static int get_a_pos_impl(const RingBuffer500 &A, int b_value, int al, int ar) {
        //(al, ar]
        int ok = ar;
        int ng = al;
        while (ok - ng > 1) {
            int mid = (ng + ok) >> 1;
            my_assert(mid != ar);
            if (A[mid] > b_value) {
                ok = mid;
            } else {
                ng = mid;
            }
        }
        return ok;
    }

    static int get_a_pos_for_b_pos(const State &state, const BeamState<MAX_N> &st, int b_pos) {
        int b_value = state.B[b_pos];
        int a_min_pos = st.min_a_pos;
        int res_apos;
        if (b_value < st.min_a_v) {
            res_apos = st.min_a_pos;
        } else if (a_min_pos == 0) {
            res_apos = get_a_pos_impl(state.A, b_value, -1, state.A.size());
            if (res_apos == state.A.size()) {
                res_apos = 0;
            }
        } else {
            if (b_value > state.A[0]) {
                res_apos = get_a_pos_impl(state.A, b_value, 0, a_min_pos);
            } else {
                res_apos = get_a_pos_impl(state.A, b_value, a_min_pos, state.A.size());
                if (res_apos == state.A.size()) {
                    res_apos = 0;
                }
            }
        }
        return res_apos;
    }

    static int
    fast_ret__get_a_pos_for_b_pos(BeamStateRunner &runner, const State &state, const BeamState<MAX_N> &st, int b_pos,
                                  int lim_ra, int lim_rra) {
        int b_value = state.B[b_pos];
        int a_min_pos = st.min_a_pos;
        int res_apos_ok;
        if (b_value < st.min_a_v) {
            res_apos_ok = st.min_a_pos;
        } else if (a_min_pos == 0) {
            my_assert(lim_ra < state.A.size());
            int al, ar;//arはok, alはng
            bool ok_ra;
            my_assert(state.A[lim_ra] != b_value);
            if (state.A[lim_ra] < b_value) {
                ok_ra = false;
                ar = state.A.size();
            } else {
                ok_ra = true;
                ar = lim_ra;
            }
            bool ok_rra;
            my_assert(lim_rra < state.A.size());
            if (b_value < state.A[state.A.size() - lim_rra - 1]) {
                ok_rra = false;
                al = -1;
            } else {
                ok_rra = true;
                al = state.A.size() - lim_rra - 1;
            }
            if (!ok_ra && !ok_rra) {
                return -1;
            }
            res_apos_ok = get_a_pos_impl(state.A, b_value, al, ar);
            if (res_apos_ok == state.A.size()) {
                res_apos_ok = 0;
            }
        } else {
            my_assert(state.A.size() > 1 && state.A[state.A.size() - 1] < state.A[0]);
            if (state.A[state.A.size() - 1] < b_value && b_value < state.A[0]) {
                return 0;
            } else if (b_value > state.A[0]) {
                my_assert(lim_ra < state.A.size());
                int al, ar;//arはok, alはng
                bool ok_ra;
                my_assert(state.A[lim_ra] != b_value);
                if (lim_ra >= a_min_pos || state.A[lim_ra] >= b_value) {
                    ar = min(a_min_pos, lim_ra);
                    ok_ra = true;
                } else {
                    ar = a_min_pos;
                    ok_ra = false;
                }

                bool ok_rra;
                my_assert(lim_rra < state.A.size());
                if ((a_min_pos < state.A.size() - lim_rra) || b_value < state.A[state.A.size() - lim_rra - 1]) {
                    al = 0;
                    ok_rra = false;
                } else {
                    al = state.A.size() - lim_rra - 1;
                    ok_rra = true;
                }
                if (!ok_ra && !ok_rra) {
                    return -1;
                }
                res_apos_ok = get_a_pos_impl(state.A, b_value, al, ar);
//                res_apos_ok = get_a_pos_impl(state.A, b_value, al, ar);
            } else {
                my_assert(lim_ra < state.A.size());
                int al, ar_ok, ar_bad;
                bool ok_ra;
                my_assert(state.A[lim_ra] != b_value);
                if (lim_ra < a_min_pos || state.A[lim_ra] < b_value) {
                    ar_ok = state.A.size();
                    ar_bad = state.A.size();
                    ok_ra = false;
                } else {
                    ar_bad = min(state.A.size(), lim_ra);
                    ar_ok = state.A.size();
                    ok_ra = true;
                }

                bool ok_rra;
                my_assert(lim_rra < state.A.size());
                if (a_min_pos < state.A.size() - lim_rra && b_value < state.A[state.A.size() - lim_rra - 1]) {
                    al = a_min_pos;
                    ok_rra = false;
                } else {
                    al = max(a_min_pos, state.A.size() - lim_rra - 1);
                    ok_rra = true;
                }
                if (!ok_ra && !ok_rra) {
                    return -1;
                }
//                res_apos_ok = get_a_pos_impl(state.A, b_value, a_min_pos, state.A.size());
                res_apos_ok = get_a_pos_impl(state.A, b_value, al, ar_ok);
                int res_apos_bad = get_a_pos_impl(state.A, b_value, al, ar_bad);
                if (res_apos_ok == state.A.size()) {
                    res_apos_ok = 0;
                }
                if (res_apos_bad == state.A.size()) {
                    res_apos_bad = 0;
                }
                my_assert(res_apos_ok == res_apos_bad);
            }
        }
        int ra_dis = get_ra_dis(state, res_apos_ok);
        int rra_dis = get_rra_dis(state, res_apos_ok);
        if (lim_ra < ra_dis && lim_rra < rra_dis) {
            return -1;
        }
        return res_apos_ok;
    }


//    void sorted__process_query_candidate(BeamStateRunner &runner, const State &state, const BeamState<MAX_N> &st,
//                                         int b_pos, command cmd2, int b_cnt, KthSmallestQuery &kth_smallest_q,
//                                         array<BeamQuery, 1000> &queries, int &q_i, int threshold) {
//        // int lim_ra, lim_rra;
//        // if()
//        int a_pos = has_dust_get_a_pos_abv(runner, state, st, b_pos);
//        queries[q_i++] = (build_single_query(runner, a_pos, b_pos, cmd2, b_cnt, e_push_type::b2a));
////        queries[q_i++] = (build_b2a_queries(runner, a_pos, b_pos, e_push_type::b2a));
//        kth_smallest_q.push(queries[q_i - 1].ins_turn);
//    }


//挿入位置として有効な[l, r]を返す
//円環上の位置なのでl>rであることがある
//vがある位置はAでもBでもいい
    static std::pair<int, int>
    has_dust_get_a_pos_abv(const SortedStateA &sorted_a, const StackWithPos &stack_pos_a,
                           const State &state,/* const BeamState<MAX_N> &st,*/
                           int val) {
//        int b_value = state.B[b_pos];
//        auto &sorted_a = runner.get_sorted_state_a();
        sorted_a.validate_min_a_pos();
        int S_min_a_pos = sorted_a.min_a_pos;
        int S_res_r_apos;
        int S_min_a_v = sorted_a.calc_min_v();
        if (val < S_min_a_v) {
            S_res_r_apos = S_min_a_pos;
        } else if (S_min_a_pos == 0) {
            S_res_r_apos = get_a_pos_impl(sorted_a.A, val, -1, sorted_a.A.size());
            if (S_res_r_apos == sorted_a.A.size()) {
                S_res_r_apos = 0;
            }
        } else {
            if (val > sorted_a.A[0]) {
                S_res_r_apos = get_a_pos_impl(sorted_a.A, val, 0, S_min_a_pos);
            } else {
                S_res_r_apos = get_a_pos_impl(sorted_a.A, val, S_min_a_pos, sorted_a.A.size());
                if (S_res_r_apos == sorted_a.A.size()) {
                    S_res_r_apos = 0;
                }
            }
        }
//        return S_res_r_apos;
        my_assert(sorted_a.A.size() > 0);
        my_assert(state.A.size() > 0);
        int r_tar_a_v = sorted_a.A[S_res_r_apos];
        int S_res_l_apos;
        if (S_res_r_apos == 0) {
            S_res_l_apos = sorted_a.A.size() - 1;
        } else {
            S_res_l_apos = S_res_r_apos - 1;
        }
        int open_l_tar_a_v = sorted_a.A[S_res_l_apos];
        int r = stack_pos_a.position(r_tar_a_v);
        int open_l = stack_pos_a.position(open_l_tar_a_v);
        int l;
        if (open_l == state.A.size() - 1) {
            l = 0;
        } else {
            l = open_l + 1;
        }
        my_assert(0 <= l && l < state.A.size());
        my_assert(0 <= r && r < state.A.size());
        return {l, r};
    }

    void set_a2a_queries(BeamStateRunner &runner, const State &state, const BeamState<MAX_N> &st,
                         int a_pos, command a_cmd, int a_cnt, KthSmallestQuery &kth_smallest_q,
                         vec_array<BeamQuery, 1000> &queries, int lim_ins_turn) {
//        //閉区間
        auto [sorted_a_pos_l, sorted_a_pos_r] = has_dust_get_a_pos_abv(runner.get_sorted_state_a(),
                                                                       runner.get_stack_a_pos(), state, /*st,*/
                                                                       state.A[a_pos]);
#ifdef DEBUG
        //ソート済みでない事を確認
        my_assert(sorted_a_pos_l != a_pos && sorted_a_pos_r != a_pos);
        if (sorted_a_pos_l != sorted_a_pos_r) {
            my_assert(!is_sorted_of_ring(sorted_a_pos_l, a_pos, sorted_a_pos_r));
        }
#endif
        BeamQuery q = BeamQueryFactory::build_a2a(state.A[a_pos], a_pos, sorted_a_pos_l, sorted_a_pos_r, st.last_query,
                                                  state.A.size());
        my_assert(q.get_a_size() > 0);
        my_assert(q.get_push_type() == e_push_type::a2a);
        my_assert(q.get_cur_default_a2a_type() == e_a2a_type::push_rot ||
                  q.get_cur_default_a2a_type() == e_a2a_type::swap_rot);
        validate_ins_turn(q);
        queries.emplace_back(q);
        kth_smallest_q.push(q.get_ins_turn_with_prev_diff());
    }


    void set_b2a_queries(BeamStateRunner &runner, const State &state, const BeamState<MAX_N> &st,
                         int b_pos, KthSmallestQuery &kth_smallest_q,
                         vec_array<BeamQuery, 1000> &queries,
                         const PrevA2AInfo &prev_info) {
        auto [default_sorted_a_pos_first, default_sorted_a_pos_last] = has_dust_get_a_pos_abv(
                runner.get_sorted_state_a(),
                runner.get_stack_a_pos(), state,
                state.B[b_pos]);
        my_vector<BeamQuery> prev_a2a_queries = get_prev_a2a_queries(st);
        my_vector<BeamQuery> part_queries = build_b2a_queries(runner.get_state(), default_sorted_a_pos_first,
                                                              default_sorted_a_pos_last,
                                                              b_pos,
                                                              prev_info, prev_a2a_queries);
        auto &st_watch = state;
        for (auto &q: part_queries) {
//            a_count_cr = min(st.A.size() - a_pos_first, rrb_dis);
            int ins_turn = q.get_ins_turn_with_prev_diff();
            queries.emplace_back(q);
            kth_smallest_q.push(ins_turn);
        }
    }


    my_vector<BeamQuery> collect_top_k_queries(KthSmallestQuery &kth_smallest_q,
                                               const my_vector<BeamQuery> &queries_with_scores,
                                               int K) {
        if (kth_smallest_q.size() < K) {
            my_vector<BeamQuery> kth_smallest_queries;
            for (const auto &q: queries_with_scores) {
                kth_smallest_queries.emplace_back(q);
            }
            return kth_smallest_queries;
        }
        int threshold_score = kth_smallest_q.get_kth_score();
        int allow_max_score_cnt = 0;
        for (int i = K - 1; i >= 0; i--) {
            int s = kth_smallest_q.best_scores[i];
            if (s == kth_smallest_q.get_kth_score()) {
                allow_max_score_cnt++;
            } else {
                break;
            }
        }
        my_assert(allow_max_score_cnt);

        my_vector<BeamQuery> kth_smallest_queries;
        for (const auto &q: queries_with_scores) {
            if (q.get_ins_turn_with_prev_diff() < threshold_score) {
                kth_smallest_queries.push_back(q);
            } else if (q.get_ins_turn_with_prev_diff() == threshold_score/* && allow_max_score_cnt*/) {
                kth_smallest_queries.push_back(q);
            }
        }
        return kth_smallest_queries;
    }


    int collect_top_k_queries2(KthSmallestQuery &kth_smallest_q,
                               vec_array<BeamQuery, 1000> &queries,
                               int K) {
        // human_review_begin: B2A 同率候補が rare case で増えすぎる場合だけ残す候補数を抑えるため。
        constexpr int QUERY_KEEP_LIMIT = 5;
        // human_review_end
        int qcnt = (int) queries.size();
        if (qcnt <= K) {
            return qcnt;
        }
        int threshold_score = kth_smallest_q.get_kth_score();
        auto it = std::partition(
                queries.begin(),
                queries.end(),
                [&](const BeamQuery &q) {
                    return q.get_ins_turn_with_prev_diff() <= threshold_score;
                }
        );
        // human_review_begin: threshold 以下の候補が固定上限を超える時だけ score 昇順で先頭側を残すため。
        int keep_count = (int) (it - queries.begin());
        if (keep_count > QUERY_KEEP_LIMIT) {
            std::sort(queries.begin(), it, [](const BeamQuery &lhs, const BeamQuery &rhs) {
                return lhs.get_ins_turn_with_prev_diff() < rhs.get_ins_turn_with_prev_diff();
            });
            keep_count = QUERY_KEEP_LIMIT;
        }
        queries.resize_small(keep_count);
        // human_review_end
        return (int) (queries.size());
    }

public:

    int get_b2a_queries(const BeamPos &tar_pos, int K, int &move_amount, vec_array<BeamQuery, 1000> &b2a_queries) {
        b2a_queries.clear();
        my_assert(tar_pos.act_cnt < (int) beams_by_act_cnt.size());
        my_assert(tar_pos.idx < (int) beams_by_act_cnt[tar_pos.act_cnt].size());
        const BeamState<MAX_N> &st = beams_by_act_cnt[tar_pos.act_cnt][tar_pos.idx];
        my_assert(st.init_id >= 0 && st.init_id < (int) runners.size());
        BeamStateRunner &runner = runners[st.init_id];
        my_assert(runner.get_pos() == tar_pos);

        const State &state = runner.get_state();
        my_assert(!state.B.empty());
        my_assert(K > 0);

        KthSmallestQuery kth_smallest_q(K);
        int threshold = inf;
        PrevA2AInfo prev_info = create_a2a_prev_info(st, state);

        bool rb_limit_ok = true;
        bool rrb_limit_ok = true;
        for (int dis = 0; dis < (int) state.B.size(); dis++) {
            if (!rb_limit_ok && !rrb_limit_ok) {
                break;
            }
            if (kth_smallest_q.size() >= K) {
                threshold = kth_smallest_q.get_kth_score();
                if (threshold < dis + 1) {
                    break;
                }
            }
            //raで見るターゲット
            int rb_i = dis;
            //rraで見るターゲット
            int rrb_i = state.B.size() - dis;
            if (rb_limit_ok) {
                //push
                int possible_min_ins = numeric_limits<int>::max();
                //push
                int possible_min_ins_push = 1;//push
                possible_min_ins_push += prev_info.if_push_prev_diff;
                possible_min_ins_push += u0(dis - prev_info.push_prev_ra_sum);
                chmi(possible_min_ins, possible_min_ins_push);

                int possible_min_ins_swap_rot = 1;//push
                possible_min_ins_swap_rot += prev_info.if_swap_prev_diff;
                possible_min_ins_swap_rot += u0(dis - prev_info.swap_prev_ra_sum);
                chmi(possible_min_ins, possible_min_ins_swap_rot);
                if (possible_min_ins <= threshold) {
                    set_b2a_queries(runner, state, st, rb_i, kth_smallest_q, b2a_queries, prev_info);
                } else {
                    rb_limit_ok = false;
                }
            }
            if (rrb_limit_ok) {
                if (dis > 0) {
                    int possible_min_ins = numeric_limits<int>::max();
                    int possible_min_ins_push = 1;
                    possible_min_ins_push += prev_info.if_push_prev_diff;
                    possible_min_ins_push += u0(dis - prev_info.push_prev_rra_sum);
                    chmi(possible_min_ins, possible_min_ins_push);

                    int possible_min_ins_swap_rot = 1;
                    possible_min_ins_swap_rot += prev_info.if_swap_prev_diff;
                    possible_min_ins_swap_rot += u0(dis - prev_info.swap_prev_rra_sum);
                    chmi(possible_min_ins, possible_min_ins_swap_rot);
                    if (possible_min_ins <= threshold) {
                        set_b2a_queries(runner, state, st, rrb_i, kth_smallest_q, b2a_queries, prev_info);
                    } else {
                        rrb_limit_ok = false;
                    }
                }
            }
        }
        return collect_top_k_queries2(kth_smallest_q, b2a_queries, K);
    }

    static int calc_a_garbage_cnt(BeamState<MAX_N> &beam_st, State &stack_st) {
        my_assert(beam_st.sorted_a_count <= stack_st.A.size());
        int AN = stack_st.A.size();
        return AN - beam_st.sorted_a_count;
    }

    bool chk_has_garbage_a_except_swap(const BeamState<MAX_N> &beam_st, const State &stack_st) {
        const InitStateInfo<MAX_N> &init_info = init_state_info_list[beam_st.init_id];
        for (int i = 0; i < stack_st.A.size(); i++) {
            if (init_info.is_lis_swap_v[stack_st.A[i]]) {
                continue;
            }
            if (is_garbage_a(stack_st.A[i], beam_st)) {
                return true;
            }
        }
        return false;
    }

    bool chk_has_garbage_a_except_swap_and_skip(const BeamState<MAX_N> &beam_st, const State &stack_st) {
        const InitStateInfo<MAX_N> &init_info = init_state_info_list[beam_st.init_id];
        for (int i = 0; i < stack_st.A.size(); i++) {
            if (init_info.is_lis_swap_v[stack_st.A[i]]) {
                continue;
            }
            if (beam_st.a2b_skipped_vals[stack_st.A[i]]) {
                continue;
            }
            if (is_garbage_a(stack_st.A[i], beam_st)) {
                return true;
            }
        }
        return false;
    }

    bool chk_has_garbage_in_a(const BeamState<MAX_N> &beam_st, const State &stack_st) {
        my_assert(beam_st.sorted_a_count <= stack_st.A.size());
        bool res = beam_st.sorted_a_count < stack_st.A.size();
        return res;
    }

    bool is_only_a2b_mode(const BeamState<MAX_N> &beam_st, const State &stack_st) {
        const InitStateInfo<MAX_N> &init_info = init_state_info_list[beam_st.init_id];
        for (int i = 0; i < stack_st.A.size(); i++) {
            int v = stack_st.A[i];
            if (beam_st.is_sorted_v[v]) {
                continue;
            }
            if (init_info.is_lis_swap_v[v]) {
                continue;
            }
            return true;
        }
        return false;
    }

    int get_a2a_queries(const BeamPos &tar_pos, int K, int &move_amount) {
        my_assert(K == 1);
        a2a_queries.clear();
        my_assert(tar_pos.act_cnt < (int) beams_by_act_cnt.size());
        my_assert(tar_pos.idx < (int) beams_by_act_cnt[tar_pos.act_cnt].size());
        const BeamState<MAX_N> &st = beams_by_act_cnt[tar_pos.act_cnt][tar_pos.idx];
        my_assert(st.init_id >= 0 && st.init_id < (int) runners.size());
        BeamStateRunner &runner = runners[st.init_id];
        my_assert(runner.get_pos() == tar_pos);

        const State &state = runner.get_state();
        my_assert(K > 0);

        KthSmallestQuery kth_smallest_q(K);
        int threshold = inf;
        //0, 1, -1, -2, -3...

        auto add_query = [&](int i) {
            my_assert(0 <= i && i < state.current_N);
            int dis;
            command a_cmd;
            if (i < state.current_N - i) {
                a_cmd = RA;
                dis = i;
            } else {
                a_cmd = RRA;
                dis = state.current_N - i;
            }
            if (!st.is_sorted_v[state.A[i]]) {
                set_a2a_queries(runner, state, st, i, a_cmd, dis, kth_smallest_q, a2a_queries, threshold);
                return true;
            }
            return false;
        };

        //クエリが1つ前提のコード
        if (add_query(0)) {
            return 1;
        }
        if (add_query(1)) {
            return 1;
        }
        for (int i = state.A.size() - 1; i > 1; i--) {
            if (add_query(i)) {
                return 1;
            }
        }
        return 0;

//        for (int dis = 0; dis < (int) state.A.size(); dis++) {
//            if (kth_smallest_q.size() >= K) {
//                threshold = kth_smallest_q.get_kth_score();
//                //disの一歩手前でsaするみたいのがあるからdis
//                if (threshold < dis/*+ 1*/) {
//                    break;
//                }
//            }
//            int i = dis;
//            int j = state.A.size() - dis;
//            if (i > j) break;
//
//            if (!st.is_sorted_v[state.A[i]]) {
//                set_a2a_queries(runner, state, st, i, RA, dis, kth_smallest_q, a2a_queries, threshold);
//            }
//            if (i == j) {
//                break;
//            }
//            if (dis > 0 && !st.is_sorted_v[state.A[j]]) {
//                set_a2a_queries(runner, state, st, j, RRA, dis, kth_smallest_q, a2a_queries, threshold);
//            }
//        }
        return collect_top_k_queries2(kth_smallest_q, a2a_queries, K);
    }

    int init_sorted_mid_v(const BeamState<MAX_N> &beam_st) {
        auto &init_sorted_a = init_state_info_list[beam_st.init_id].init_sorted_state_a;
        int mid_v = init_sorted_a[init_sorted_a.size() / 2];
        return mid_v;
    }

    bool is_btm_chank_b(const BeamState<MAX_N> &beam_st, const State &st, int v) {
        int mid_v = init_sorted_mid_v(beam_st);
        int dis_by_mid_v;
        if (mid_v <= v) {
            dis_by_mid_v = v - mid_v;
        } else {
            dis_by_mid_v = st.current_N - mid_v + v;
        }
        return !(dis_by_mid_v < st.current_N / 2);
    }

    void validate_B_all_chank_loop(const BeamState<MAX_N> &beam_st, const State &st, const BeamStateRunner &runner,
                                   int loop_cnt) {
#ifdef DEBUG
        for (int i = 0; i < st.B.size(); i++) {
            if (!is_a2b_tar_v(st.B[i], beam_st, runner)) {
                continue;
            }
            ChankInfo chank_info = get_chank_info(st.current_N, beam_st, st.B[i], runner);
            my_assert(chank_info.loop_cnt == loop_cnt);
        }
#endif
    }

    //返り値をiとして、iの上に挿入する
    //stのAの状態はズレている可能性があることに注意
    //Bはズレないので現状添え字を使ってしまっているが本当は良くない
    int
    calc_a2b_b_pos(const BeamState<MAX_N> &beam_st, const State &st, int a_pos, int av, const BeamStateRunner &runner) {
        ChankInfo v_chank_info = get_chank_info(st.current_N, beam_st, av, runner);
        my_assert(v_chank_info.loop_cnt < 2);//あとで一般化する
        auto b_top_chank_info_cnt = [&](const ChankInfo &target_info) {
            int cnt = 0;
            for (int i = 0; i < st.B.size(); i++) {
                ChankInfo cur_info = get_chank_info(st.current_N, beam_st, st.B[i], runner);
                if (cur_info != target_info) {
                    break;
                }
                cnt++;
            }
            return cnt;
        };
        if (st.B.size() == 0) {
            return 0;
        } else if (v_chank_info.loop_cnt == 0) {
            //全要素が0番目
            validate_B_all_chank_loop(beam_st, st, runner, 0);
            int top_must_btm_cnt = b_top_chank_info_cnt(ChankInfo(0, e_chank_pos::btm_chank));
            if (top_must_btm_cnt == st.B.size()) {
                return 0;
            } else if (v_chank_info.pos == e_chank_pos::top_chank) {
                return top_must_btm_cnt;
            } else {
                return min(top_must_btm_cnt, a_pos);
            }
        } else {
            my_assert(v_chank_info.loop_cnt == 1);
            ChankInfo b_top_v_chank_info = get_chank_info(st.current_N, beam_st, st.B[0], runner);
            if (b_top_v_chank_info == ChankInfo(0, e_chank_pos::top_chank)) {
                return 0;
            } else if (b_top_v_chank_info == ChankInfo(0, e_chank_pos::btm_chank)) {
                validate_B_all_chank_loop(beam_st, st, runner, 0);
                int top2_cnt = b_top_chank_info_cnt(ChankInfo(0, e_chank_pos::btm_chank));
                //1が一つもない場合
                if (top2_cnt == st.B.size()) {
                    my_assert(false);//基本的にないはずなのでassertつける 後で消してもいい
                    return 0;
                } else {
                    return top2_cnt;
                }
            } else if (b_top_v_chank_info == ChankInfo(1, e_chank_pos::top_chank)) {
                return 0;
            } else if (b_top_v_chank_info == ChankInfo(1, e_chank_pos::btm_chank)) {
                int top3_cnt = b_top_chank_info_cnt(ChankInfo(1, e_chank_pos::btm_chank));
                //chank, 1, 2が存在しない場合という事になるのでおかしいけど
                if (top3_cnt == st.B.size()) {
                    my_assert(false);
                    return 0;
                } else {
                    if (v_chank_info == ChankInfo(1, e_chank_pos::top_chank)) {
                        return top3_cnt;
                    } else {
                        my_assert(v_chank_info == ChankInfo(1, e_chank_pos::btm_chank));
                        return min(top3_cnt, a_pos);
                    }
                }
            } else {
                my_assert(false);
                return -1;
            }
        }
    }

    bool is_must_skip_v(int v, const BeamState<MAX_N> &beam_st, const BeamStateRunner &runner) {
        const InitStateInfo<MAX_N> &init_info = init_state_info_list[beam_st.init_id];
        if (init_info.is_lis_swap_v[v]) {
            return true;
        }
        return false;
    }

    bool is_skip_val2(const BeamState<MAX_N> &beam_st, const State &state, const BeamStateRunner &runner, int v) {
//        if (N < 500) {
        return false;
//        }
        int v_pos = runner.get_stack_a_pos().position(v);
        int prev_sorted_v = -1;
        for (int dis = 1; dis < state.A.size(); dis++) {
            int i = mod(v_pos - dis, state.A.size());
            int v = state.A[i];
            if (beam_st.is_sorted_v[v]) {
                prev_sorted_v = v;
                break;
            }
        }
        my_assert(prev_sorted_v != -1);
        //上へ持っていく物だけskipしたい
        if (prev_sorted_v < v) {
            return false;
        }
        if (prev_sorted_v - v <= 10) {
            return true;
        } else {
            return false;
        }
//        int sorted_prev_i;
//        auto &sorted_a = runner.get_sorted_state_a();
//        for (int i = 0; i < sorted_a.A.size(); i++) {
//            if (sorted_a.A[i] == prev_sorted_v) {
//                sorted_prev_i = i;
//                break;
//            }
//        }
//        //既にソートされてない事の確認
//        {
//            int sorted_nex_i = mod(sorted_prev_i + 1, sorted_a.A.size());
//            my_assert(!is_sorted_of_ring(sorted_a[sorted_prev_i], v, sorted_a[sorted_nex_i]));
//        }

    }

    //降順に並べる
//    enum e_a2b_phase {
//        chank12, chank03, fin
//    };

    enum e_chank_pos {
        top_chank, btm_chank
    };

    struct ChankInfo {
        int loop_cnt;
        e_chank_pos pos;

        ChankInfo() : loop_cnt(0), pos(top_chank) {}

        ChankInfo(int loop_cnt_, e_chank_pos is_top_btm_)
                : loop_cnt(loop_cnt_), pos(is_top_btm_) {}

        bool operator==(const ChankInfo &other) const {
            return loop_cnt == other.loop_cnt && pos == other.pos;
        }

        bool operator!=(const ChankInfo &other) const {
            return !(*this == other);
        }
    };

    ChankInfo get_chank_info(int N, const BeamState<MAX_N> &beam_st, int v, const BeamStateRunner &runner) {
        my_assert(is_a2b_tar_v(v, beam_st, runner));
        int mid_v = init_sorted_mid_v(beam_st);
        int dis_by_mid_v;
        if (mid_v <= v) {
            dis_by_mid_v = v - mid_v;
        } else {
            dis_by_mid_v = N + v - mid_v;
        }
        my_assert(CHANK_CNT > 0 && CHANK_CNT % 2 == 0);
        int chank_range = N / CHANK_CNT;
        int inc_chank_id = dis_by_mid_v / chank_range;
        if (inc_chank_id >= CHANK_CNT) {
            inc_chank_id = CHANK_CNT - 1;
        }
        int chank_ord_of_top = mod(CHANK_CNT / 2 + inc_chank_id, CHANK_CNT);
        if (CHANK_CNT / 2 <= chank_ord_of_top) {
            int loop_cnt = chank_ord_of_top - CHANK_CNT / 2;
//            return ChankInfo(loop_cnt, e_chank_pos::btm_chank);
            return ChankInfo(loop_cnt, e_chank_pos::top_chank);
        } else {
            int loop_cnt = (CHANK_CNT / 2 - 1) - chank_ord_of_top;
//            return ChankInfo(loop_cnt, e_chank_pos::top_chank);
            return ChankInfo(loop_cnt, e_chank_pos::btm_chank);
            //降順の方が微妙に良かった N=500の手数が17手ほど
        }
    }

    //0周めなら0,
    int get_now_a2b_chank_loop_cnt(const BeamState<MAX_N> &beam_st, BeamStateRunner &runner) {
        auto &stack_st = runner.get_state();
        const InitStateInfo<MAX_N> &init_info = init_state_info_list[beam_st.init_id];
        int min_loop_cnt = numeric_limits<int>::max();
//        bool exist03 = false;
        for (int i = 0; i < stack_st.A.size(); i++) {
            if (init_info.is_lis_swap_v[stack_st.A[i]]) {
                continue;
            }
            if (is_garbage_a(stack_st.A[i], beam_st)) {
                ChankInfo chank_info = get_chank_info(stack_st.current_N, beam_st, stack_st.A[i], runner);
                my_assert(CHANK_CNT % 2 == 0);
                my_assert(0 <= chank_info.loop_cnt && chank_info.loop_cnt < CHANK_CNT / 2);
                if (chank_info.loop_cnt == 0) {
                    return 0;
                }
                chmi(min_loop_cnt, chank_info.loop_cnt);
            }
        }
        return min_loop_cnt;
    }

    bool is_target_chank_v(int N, int v, const BeamState<MAX_N> &beam_st, int now_chank_loop,
                           const BeamStateRunner &runner) {
        my_assert(CHANK_CNT % 2 == 0);
        my_assert(now_chank_loop < CHANK_CNT / 2);
        ChankInfo chank_info = get_chank_info(N, beam_st, v, runner);
        return chank_info.loop_cnt == now_chank_loop;
    }

    bool is_a2b_tar_v(int v, const BeamState<MAX_N> &st, const BeamStateRunner &runner) {
        if (is_garbage_a(v, st) && !is_must_skip_v(v, st, runner)) {
            return true;
        } else {
            return false;
        }
    }

//raでたどり着ける一番上のゴミを返す
    int get_a2b_queries(const BeamPos &tar_pos, int K, int &move_amount, int dir, const PrevA2AInfo &prev_info) {
        a2b_queries.clear();
        my_assert(tar_pos.act_cnt < (int) beams_by_act_cnt.size());
        my_assert(tar_pos.idx < (int) beams_by_act_cnt[tar_pos.act_cnt].size());
        const BeamState<MAX_N> &st = beams_by_act_cnt[tar_pos.act_cnt][tar_pos.idx];
        my_assert(st.init_id >= 0 && st.init_id < (int) runners.size());
        BeamStateRunner &runner = runners[st.init_id];
        my_assert(runner.get_pos() == tar_pos);

        const State &state = runner.get_state();
        my_assert(!state.A.empty());
        my_assert(K > 0);
//        my_assert(chk_has_garbage_in_a(st, state));
        if (!chk_has_garbage_in_a(st, state)) {
            return 0;
        }
        auto &sorted_a = runner.get_sorted_state_a();
        int if_swap_a_pos_diff = prev_info.if_swap_last_a_pos_diff;
        int if_push_a_pos_diff = prev_info.if_push_last_a_pos_diff;
        my_assert(a2b_queries.size() == 0);
//        my_vector<int> skipped_vals_if_prev_push;
//        my_vector<int> skipped_vals_if_prev_swap;
//        my_vector<int> skipped_vals_if_prev_not_a2a;

        int now_chank_loop = get_now_a2b_chank_loop_cnt(st, runner);
        for (int dis = 0; dis < state.A.size(); dis++) {
            //現状のstにおけるa_posではなく、prevをprev_a2a_typeにした時のa_pos
            auto add_query = [&](int a_pos, int v, e_a2a_type prev_a2a_type) {
//                bool is_target_chank_v(int N, int v, const BeamState<MAX_N> &beam_st, e_a2b_phase a2b_phase, const BeamStateRunner& runner) {
                my_assert(is_target_chank_v(state.current_N, v, st, now_chank_loop, runner));
                int b_pos = calc_a2b_b_pos(st, state, a_pos, v, runner);
                auto [a_cmd, b_cmd, a_cnt_orign, b_cnt_orign] = calc_optimal_a2b_rot(state, a_pos, b_pos);
                int b_cnt_rem;
                if (b_cmd == RB) {
                    if (prev_a2a_type == e_a2a_type::push_rot) {
                        b_cnt_rem = u0(b_cnt_orign - prev_info.push_prev_ra_sum);
                    } else if (prev_a2a_type == e_a2a_type::swap_rot) {
                        b_cnt_rem = u0(b_cnt_orign - prev_info.swap_prev_ra_sum);
                    } else {
                        b_cnt_rem = b_cnt_orign;
                    }
                } else if (b_cmd == RRB) {
                    if (prev_a2a_type == e_a2a_type::push_rot) {
                        b_cnt_rem = u0(b_cnt_orign - prev_info.push_prev_rra_sum);
                    } else if (prev_a2a_type == e_a2a_type::swap_rot) {
                        b_cnt_rem = u0(b_cnt_orign - prev_info.swap_prev_rra_sum);
                    } else {
                        b_cnt_rem = b_cnt_orign;
                    }
                } else {
                    my_assert(false);
                }

                int ins_turn = calc_ins_turn(a_cmd, b_cmd, a_cnt_orign, b_cnt_rem);
                int prev_a2a_ins_turn_diff = 0;
                if (prev_a2a_type == e_a2a_type::push_rot) {
                    prev_a2a_ins_turn_diff = prev_info.if_push_prev_diff;
                } else if (prev_a2a_type == e_a2a_type::swap_rot) {
                    prev_a2a_ins_turn_diff = prev_info.if_swap_prev_diff;
                }
                int ins_turn_with_prev_diff = ins_turn + prev_a2a_ins_turn_diff;

                a2b_queries.emplace_back(v,
                                         ins_turn_with_prev_diff,
                                         a_cnt_orign,
                                         b_cnt_orign,
                                         a_cmd,
                                         b_cmd,
                                         e_push_type::a2b,
                                         prev_a2a_type,
                                         prev_a2a_ins_turn_diff,
                                         e_a2a_type::not_a2a,
                                         state.A.size());
//                if (prev_a2a_type == e_a2a_type::push_rot) {
//                    a2b_queries.back().set_a2b_skip_vals(skipped_vals_if_prev_push);
//                } else if (prev_a2a_type == e_a2a_type::swap_rot) {
//                    a2b_queries.back().set_a2b_skip_vals(skipped_vals_if_prev_swap);
//                } else {
//                    a2b_queries.back().set_a2b_skip_vals(skipped_vals_if_prev_not_a2a);
//                    my_assert(false);
//                }
#ifdef DEBUG
                my_vector<BeamQuery> prev_a2a_queries = get_prev_a2a_queries(st);
                validate_ins_turn(a2b_queries.back(), prev_a2a_queries, prev_info);
#endif
            };
            int prev_q_cnt = a2b_queries.size();
            int default_a_pos = mod(dis * dir, state.A.size());
//            if (!is_garbage_a(v, st)) {
//                return;
//            }
//            if (is_must_skip_v(v, st, runner)) {
//                return;
//            }
            if (!prev_info.is_prev_a2a()) {
                int v = state.A[default_a_pos];
                if (is_a2b_tar_v(v, st, runner)) {
                    bool is_target_chank_v_ = is_target_chank_v(N, v, st, now_chank_loop, runner);
                    if (is_target_chank_v_) {
                        add_query(default_a_pos, v, e_a2a_type::not_a2a);
                    } else {
//                    skipped_vals_if_prev_not_a2a.push_back(v);
                    }
                }
            } else {
                int if_push_a_pos_on_this_st = mod(default_a_pos - if_push_a_pos_diff, state.A.size());
                int if_push_v = state.A[if_push_a_pos_on_this_st];
                if (is_a2b_tar_v(if_push_v, st, runner)) {
                    bool if_push_is_target_chank_v_ = is_target_chank_v(N, if_push_v, st, now_chank_loop, runner);
                    if (if_push_is_target_chank_v_) {
                        add_query(default_a_pos, if_push_v, e_a2a_type::push_rot);
                    } else {
//                    skipped_vals_if_prev_push.push_back(if_push_v);
                    }
                }

                int if_swap_a_pos_on_this_st = mod(default_a_pos - if_swap_a_pos_diff, state.A.size());
                int if_swap_v = state.A[if_swap_a_pos_on_this_st];

                if (is_a2b_tar_v(if_swap_v, st, runner)) {
                    bool if_swap_is_target_chank_v_ = is_target_chank_v(N, if_swap_v, st, now_chank_loop, runner);
                    if (if_swap_is_target_chank_v_) {
                        add_query(default_a_pos, if_swap_v, e_a2a_type::swap_rot);
                    } else {
//                    skipped_vals_if_prev_push.push_back(if_swap_v);
                    }
                }
            }
            int aft_q_cnt = a2b_queries.size();
            int inc_q_cnt = a2b_queries.size() - prev_q_cnt;
            my_assert(0 <= inc_q_cnt && inc_q_cnt <= 2);
            if (inc_q_cnt == 2) {
                if (a2b_queries[aft_q_cnt - 1].get_ins_turn_with_prev_diff() <
                    a2b_queries[aft_q_cnt - 2].get_ins_turn_with_prev_diff()) {
                    a2b_queries[aft_q_cnt - 2] = a2b_queries[aft_q_cnt - 1];
                }
                a2b_queries.pop_back();
            }
            my_assert(a2b_queries.size() <= 2);
            if (a2b_queries.size() == 1) {
                return 1;
            }
        }
        return a2b_queries.size();
    }


    int get_queries(const BeamPos &tar_pos, int K, int &move_amount) {
        b2a_queries.clear();
        a2b_queries.clear();
        a2a_queries.clear();
        move_amount = seek_to(tar_pos);
        my_assert(tar_pos.act_cnt < (int) beams_by_act_cnt.size());
        my_assert(tar_pos.idx < (int) beams_by_act_cnt[tar_pos.act_cnt].size());
        const BeamState<MAX_N> &beam_st = beams_by_act_cnt[tar_pos.act_cnt][tar_pos.idx];
        my_assert(beam_st.init_id >= 0 && beam_st.init_id < (int) runners.size());
        BeamStateRunner &runner = runners[beam_st.init_id];
        my_assert(runner.get_pos() == tar_pos);

        const State &stack_st = runner.get_state();
        const InitStateInfo<MAX_N> &init_info = init_state_info_list[beam_st.init_id];

        PrevA2AInfo prev_info = create_a2a_prev_info(beam_st, stack_st);

        //ゴミが残ってたらtrue
        bool has_garbage_a_except_swap = chk_has_garbage_a_except_swap(beam_st, stack_st);
        int cnt = 0;
        if (has_garbage_a_except_swap) {
            cnt += get_a2b_queries(tar_pos, K, move_amount, 1, prev_info);
//            if (stack_st.B.size() > 0) {
//                cnt += get_b2a_queries(tar_pos, K, move_amount, b2a_queries);
//            }
        } else {
//            cnt += get_a2b_queries(tar_pos, K, move_amount, -1, prev_info);
//            cnt += get_a2b_queries(tar_pos, K, move_amount, 1, prev_info);
//            cnt += get_a2b_queries(tar_pos, K, move_amount, -1, prev_info);
            //swapのやつだけあるなら
            if (chk_has_garbage_in_a(beam_st, stack_st)) {
                cnt += get_a2a_queries(tar_pos, 1, move_amount);
            }
            if (stack_st.B.size() > 0) {
                cnt += get_b2a_queries(tar_pos, K, move_amount, b2a_queries);
            }
        }
        my_assert(cnt > 0);
        my_assert(cnt == a2b_queries.size() + b2a_queries.size() + a2a_queries.size());
        return cnt;

    }

    //良く分からないので消す
//     void validate_is_sorted_is_true(const BeamState<MAX_N>& beam_st){
// #ifndef DEBUG
//         return;
// #endif
//         seek_to(BeamPos( beam_st.act_cnt, beam_st.cur_idx));
//         const State& st = runners[beam_st.init_id].get_state();
//         my_vector<int> sorted_as;
//         for (int i = 0; i < st.A.size(); i++){
//             if(beam_st.is_sorted_v[st.A[i]]){
//                 sorted_as.push_back(st.A[i]);
//             }
//         }
//         validate_ring_sorted(sorted_as);
//     }

    void beam_loop(int max_act_cnt, int &query_cnt, int &query_sum) {
        for (int act_cnt = 0; act_cnt < max_act_cnt; act_cnt++) {
            auto &beam = beams_by_act_cnt[act_cnt];
            if (beam.empty()) continue;
            was_node_by_sh.clear();
            select_beam_outer(beam, BeamWidth, act_cnt);
            my_assert(!beam.empty());

            for (int idx = (int) beam.size() - 1; idx >= 0; idx--) {
                const auto &S = beam[idx];
                // validate_is_sorted_is_true(S);

                if (S.is_deleted) continue;
                if (is_push_finish(S)) continue;
                my_assert(S.init_id >= 0 && S.init_id < (int) runners.size());
                BeamPos target_pos(act_cnt, idx);
                BeamState<MAX_N> &target_st = beams_by_act_cnt[act_cnt][idx];

                int target_id = target_st.init_id;
                int seek_move_amount;
                int query_size = get_queries(target_pos, KExp, seek_move_amount);
                query_sum += query_size;
                query_cnt++;
//                if (a2b_queries.size()) {
                for (int qi = 0; qi < a2b_queries.size(); qi++) {
                    add_applied_query(S, a2b_queries[qi], act_cnt, idx);
                }
//                } else {
                seek_to(target_pos);
                for (int qi = 0; qi < b2a_queries.size(); qi++) {
                    add_applied_query(S, b2a_queries[qi], act_cnt, idx);
                }
                for (int qi = 0; qi < a2a_queries.size(); qi++) {
                    add_applied_query(S, a2a_queries[qi], act_cnt, idx);
                }
//                }
            }
        }
    }

    int calc_a2a_by_sara(int dis) {
        return dis * 2 - 1;
    };

    auto calc_a2a_by_push(int dis) {
        return dis + 2;
    };

//beam_stはa_dis_sum以外はセットされている
//    double calc_a_dis_sum(const BeamState<MAX_N> &beam_st, const BeamStateRunner &runner) {
//        double a_dis_sum = 0;
//        const State &st = runner.get_state();
//        const SortedStateA &sorted_st_a = runner.get_sorted_state_a();
//        const StackWithPos &stack_pos_a = runner.get_stack_a_pos();
//        const auto &is_sorted_v = beam_st.is_sorted_v;
//        auto chk_valid_a_pos = [&](int ai) {
//            return 0 <= ai && ai < st.A.size();
//        };
//        //fromの物を、toの上に持っていく
//        //追い越す相手の個数
//        auto ring_dis_up = [&](int from, int to) {
//            my_assert(chk_valid_a_pos(from));
//            my_assert(chk_valid_a_pos(to));
//            my_assert(from != to);
//            if (to < from) {
//                return from - to;
//            } else {
//                return from + (int) st.A.size() - to;
//            };
//        };
//        auto ring_dis_down = [&](int from, int to) {
//            my_assert(chk_valid_a_pos(from));
//            my_assert(chk_valid_a_pos(to));
//            my_assert(from != to);
//            if (from < to) {
//                return to - from - 1;
//            } else {
//                return ((int) st.A.size() - 1 - from) + to;
//            }
//        };
//        double lim = BeamEvaluator::get_a_dis_lim(st.current_N);
//        int a_garbage_cnt = 0;
//        for (int i = 0; i < st.A.size(); i++) {
//            if (is_sorted_v[st.A[i]]) {
//                continue;
//            }
//            a_garbage_cnt++;
//            auto [to_first, to_last] = has_dust_get_a_pos_abv(sorted_st_a, stack_pos_a, st, st.A[i]);
//            my_assert(to_first == to_last || !is_sorted_of_ring(to_first, i, to_last));
//            //追い越す個数
//
//            auto calc_ins_min = [&](int dis) {
//                return min(calc_a2a_by_sara(dis), calc_a2a_by_push(dis));
//            };
//            //上に移動するなら、to_lastに行けばいい
//            int dis_up = ring_dis_up(i, to_last);
//            int dis_down = ring_dis_down(i, to_first);
//            int ins_min = min(calc_ins_min(dis_up), calc_ins_min(dis_down));
//            double res_dis = min((double) ins_min, lim);
//            a_dis_sum += res_dis;
//        }
////        out("gar4.75", a_garbage_cnt * 4.75, "a_dis_sum", a_dis_sum);
//        my_assert(a_garbage_cnt <= a_dis_sum + 0.0001 && a_dis_sum <= a_garbage_cnt * lim + 0.0001);
//        return a_dis_sum;
//    }
//
    int CHANK_CNT;

    BeamState<MAX_N> search(const State &init_state, int CHANK_CNT) {
        this->CHANK_CNT = CHANK_CNT;
        my_assert(CHANK_CNT > 0 && CHANK_CNT % 2 == 0);
        my_assert(init_state.current_N <= MAX_N);
        N = init_state.current_N;
        my_vector<BeamInitResult<MAX_N>> initial_results = BeamInitializer::generate_initial_results<MAX_N>(
                init_state);;

        my_assert(!initial_results.empty());

        beams_by_act_cnt.clear();
        beams_by_act_cnt.resize(calc_max_act_cnt(N));
        initial_cmds_list.clear();
        initial_cmds_list.resize(initial_results.size());
        runners.clear();
        runners.reserve(initial_results.size());
        init_state_info_list.clear();
        init_state_info_list.reserve(initial_results.size());

        total_apply_count = 0;
        total_rollback_count = 0;
        total_seek_to_count = 0;
//        dep_seek_to_count.clear();
//        dep_seek_to_count.resize(total_n + 1, 0);
//        dep_move_amount.clear();
//        dep_move_amount.resize(total_n + 1, 0);
//        dep_move_records.clear();
//        dep_move_records.resize(total_n + 1);

        int query_cnt = 0;
        int query_sum = 0;
        for (int i = 0; i < (int) initial_results.size(); i++) {
            State base_state(init_state.A, init_state.B, false);
            StateHash base_hash(base_state);
            uint64_t a_hash = base_hash.A.all_hash;
            uint64_t b_hash = base_hash.B.all_hash;
            BeamState<MAX_N> state = initial_results[i].state;
            state.init_id = i;
            state.act_cnt = 0;
            state.a_hash = a_hash;
            state.b_hash = b_hash;
            int act_cnt = 0;
            beams_by_act_cnt[act_cnt].push_back(state);
            initial_cmds_list[i] = initial_results[i].init_cmds;
            init_state_info_list.push_back(initial_results[i].init_state_info);
            int idx = (int) beams_by_act_cnt[act_cnt].size() - 1;
            beams_by_act_cnt[act_cnt][idx].cur_idx = idx;
            BeamPos pos = BeamPos(act_cnt, idx);
            runners.emplace_back(base_state, pos, state);
//            double a_dis_sum = calc_a_dis_sum(beams_by_act_cnt[act_cnt].back(), runners.back());
//            beams_by_act_cnt[act_cnt].back().set_a_dis_sum(a_dis_sum);
        }
        int max_act_cnt = calc_max_act_cnt(N);
        beam_loop(max_act_cnt, query_cnt, query_sum);


        cout << "runner_cnt" << runners.size() << endl;
        cout << "avg" << double(query_sum) / query_cnt << endl;
//        out("dep=", total_n - 1, " seek_to_count=", dep_seek_to_count[total_n - 1], " move_amount=",
//            dep_move_amount[total_n - 1]);

        int inf = numeric_limits<int>::max();
        int min_turn = inf;
        BeamState<MAX_N> best_state;
        for (int act_cnt = 0; act_cnt < max_act_cnt; act_cnt++) {
            const auto &beam = beams_by_act_cnt[act_cnt];
            for (int idx = 0; idx < (int) beam.size(); idx++) {
                const auto &S = beam[idx];
                if (S.is_deleted) continue;
                if (!is_push_finish(S)) continue;
                my_assert(S.init_id >= 0 && S.init_id < (int) runners.size());
                BeamPos pos(act_cnt, idx);
                seek_to(pos);
                validate_is_finish(runners[S.init_id], S);
                //todo とりあえずdoubleからintで...
                if (chmi(min_turn, BeamEvaluator::evaluaet_finish<MAX_N>(runners[S.init_id], S))) {
                    best_state = S;
                }
            }
        }
        static_total_seek_to_count += total_seek_to_count;
        static_total_apply_count += total_apply_count;
        static_total_rollback_count += total_rollback_count;

        my_assert(min_turn != inf);
        return best_state;
    }

    //x
    template<size_t TREE_MAX_N>
    InitStateInfo<MAX_N> create_init_state_info_for_finished_a2b(
            const State &init_state,
            const my_bitset<MAX_N> &is_lis
    ) {
        InitStateInfo<MAX_N> init_state_info;
        init_state_info.init_state = init_state;
        init_state_info.lis_start_index = -1;
        init_state_info.lis_start = -1;
        init_state_info.id_hash = make_id_hash(0);

        // is_lisからlis_listを小さい順に構築
        my_vector<int> lis_list;
        for (int v = 0; v < N; v++) {
            if (is_lis[v]) {
                lis_list.push_back(v);
            }
        }
        init_state_info.lis_list = lis_list;

        // is_lisをコピー
        init_state_info.is_lis.resize(N, 0);
        for (int v = 0; v < N; v++) {
            if (is_lis[v]) {
                my_assert(0 <= v && v < N);
                init_state_info.is_lis[v] = 1;
            }
        }

        init_state_info.init_posa.resize(N, -1);
        for (int i = 0; i < init_state.A.size(); i++) {
            int value = init_state.A[i];
            my_assert(0 <= value && value < N);
            init_state_info.init_posa[value] = i;
        }

        init_state_info.is_lis_swap_v.reset();
        for (int i = 0; i < init_state.A.size(); i++) {
            int v = init_state.A[i];
            if (!init_state_info.is_lis[v]) {
                init_state_info.is_lis_swap_v[v] = true;
            }
        }

        init_state_info.init_sorted_state_a = lis_list;

#ifdef DEBUG
        my_assert(init_state_info.init_sorted_state_a.size() == lis_list.size());
        if (lis_list.size() > 2) {
            for (size_t i = 0; i < lis_list.size(); i++) {
                int v_prev = lis_list[(i - 1 + lis_list.size()) % lis_list.size()];
                int v = lis_list[i];
                int v_next = lis_list[(i + 1) % lis_list.size()];
                my_assert(is_sorted_of_ring(v_prev, v, v_next));
            }
        }
#endif

        return init_state_info;
    }

    //o
    void initialize_beam_state_for_finished_a2b(
            const State &base_state,
            const StateHash &base_hash,
            const InitStateInfo<MAX_N> &init_state_info,
            BeamState<MAX_N> &state,
            const my_bitset<MAX_N> &is_lis
    ) {
        state.init_id = 0;
        state.act_cnt = 0;
        state.sorted_a_count = (int) init_state_info.lis_list.size();
        state.a_hash = base_hash.A.all_hash;
        state.b_hash = base_hash.B.all_hash;
        state.cur_idx = 0;
        state.current_turn = 0;
        state.score = 0.0;
        state.yet_sorted_dis_to_0_sum = 0.0;
        state.parent_idx = -1;

        state.is_sorted_v = is_lis;
        for (int v = 0; v < N; v++) {
            if (init_state_info.is_lis[v]) {
                state.min_a_v = v;
                break;
            }
        }
        state.min_a_pos = init_state_info.init_posa[state.min_a_v];

        state.is_deleted = false;
    }

    //o
    void initialize_beam_searcher_for_finished_a2b(
            const State &base_state,
            const BeamState<MAX_N> &state,
            const InitStateInfo<MAX_N> &init_state_info
    ) {
        beams_by_act_cnt.clear();
        beams_by_act_cnt.resize(calc_max_act_cnt(N));
        beams_by_act_cnt[0].push_back(state);
        beams_by_act_cnt[0][0].cur_idx = 0;

        initial_cmds_list.clear();
        initial_cmds_list.resize(1);
        initial_cmds_list[0] = {};

        runners.clear();
        runners.reserve(1);
        BeamPos pos(0, 0);
        runners.emplace_back(base_state, pos, state);

        init_state_info_list.clear();
        init_state_info_list.reserve(1);
        init_state_info_list.push_back(init_state_info);

        total_apply_count = 0;
        total_rollback_count = 0;
        total_seek_to_count = 0;
    }

    //x
    template<size_t TREE_MAX_N>
    BeamState<MAX_N> search_finished_a2b(
            const State &init_state,
            const my_bitset<MAX_N> &is_lis,
            const YakinamashiChunk::ChunkTree<TREE_MAX_N> &tree
    ) {
        my_assert(init_state.current_N <= MAX_N);
        N = init_state.current_N;

        InitStateInfo<MAX_N> init_state_info = create_init_state_info_for_finished_a2b<TREE_MAX_N>(init_state, is_lis);

        State base_state(init_state.A, init_state.B, false);
        StateHash base_hash(base_state);
        BeamState<MAX_N> state;
        initialize_beam_state_for_finished_a2b(base_state, base_hash, init_state_info, state, is_lis);
        initialize_beam_searcher_for_finished_a2b(base_state, state, init_state_info);

        int max_act_cnt = calc_max_act_cnt(N);
        int query_cnt = 0;
        int query_sum = 0;
        beam_loop(max_act_cnt, query_cnt, query_sum);

        int min_turn = numeric_limits<int>::max();
        BeamState<MAX_N> best_state;

        for (int act_cnt = 0; act_cnt < max_act_cnt; act_cnt++) {
            const auto &beam = beams_by_act_cnt[act_cnt];
            for (int idx = 0; idx < (int) beam.size(); idx++) {
                const auto &S = beam[idx];
                if (!is_push_finish(S)) continue;
                // Align runner state with the finished beam state before validation
                BeamPos pos(act_cnt, idx);
                seek_to(pos);
                validate_is_finish(runners[S.init_id], S);
                if (chmi(min_turn, BeamEvaluator::evaluaet_finish<MAX_N>(runners[S.init_id], S))) {
                    best_state = S;
                }
            }
            if (min_turn != numeric_limits<int>::max()) break;
        }

        my_assert(min_turn != numeric_limits<int>::max());
        return best_state;
    }

};

#endif //UNTITLED_BEAMSEARCHER_H
