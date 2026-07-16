//
// Created by Auto
//

#ifndef UNTITLED_BEAMINITIALIZER_H
#define UNTITLED_BEAMINITIALIZER_H

#include "all_include.h"
#include "State.h"
#include "RingBuffer500.h"
#include "SortedStateA.h"
#include "Legacy/lis_chank/Lis.h"
#include "BeamState.h"
#include "BeamHashHelper.h"
#include "BeamEvaluator.h"

template<int MAX_N>
struct InitStateInfo {
    my_vector<int> is_lis;
    my_vector<int> lis_list;
    State init_state;
    my_vector<int> init_posa;
    my_vector<int> init_sorted_state_a;
    int lis_start_index;
    int lis_start;
    uint64_t id_hash;
    my_bitset<MAX_N> is_lis_swap_v;

    InitStateInfo() : init_state(RingBuffer500(), RingBuffer500(), false), lis_start_index(0), lis_start(0),
                      id_hash(0), is_lis_swap_v(0) {}

    InitStateInfo(const State &state) : init_state(state), lis_start_index(0), lis_start(0), id_hash(0), is_lis_swap_v(0) {}
};

template<int MAX_N>
struct BeamInitResult {
    BeamState<MAX_N> state;
    my_vector<command> init_cmds;
    InitStateInfo<MAX_N> init_state_info;
};

class BeamInitializer {
public:


    template<int MAX_N>
    static my_vector<BeamInitResult<MAX_N>> generate_initial_results(const State &init_state) {
        my_assert(init_state.initial_N <= MAX_N);
        my_vector<BeamInitResult<MAX_N>> initial_results;
        RingBuffer500 temp_A = init_state.A;
        deque<int> dq_A = to_dq(temp_A);

        my_vector<pair<int, my_vector<int>>> ring_lis_candidates;

        std::set<my_vector<int>> was_lis;
        for (int start_i = 0; start_i < dq_A.size(); start_i++) {
            deque<int> rotated_dq = dq_A;
            for (int k = 0; k < start_i; k++) {
                rotated_dq.push_back(rotated_dq.front());
                rotated_dq.pop_front();
            }

            //roted_dq[0]がlisの中に含まれる
            my_vector<int> lis = get_lis_ring_use_a0(rotated_dq);
            if (was_lis.count(lis)) {
                continue;
            }
            was_lis.insert(lis);
            my_assert(is_sorted(lis.begin(), lis.end()));
            my_assert(std::find(lis.begin(), lis.end(), rotated_dq[0]) != lis.end());
            ring_lis_candidates.emplace_back(start_i, lis);
        }
        cout << "lis_cnt "<< ring_lis_candidates.size() << endl;

        for (const auto &[start_i, lis]: ring_lis_candidates) {
            int id = (int) initial_results.size();
            InitStateInfo<MAX_N> init_state_info;
            init_state_info.lis_list = lis;
            init_state_info.is_lis.resize(init_state.initial_N, 0);
            for (int v: lis) {
                my_assert(v >= 0 && v < init_state.initial_N);
                init_state_info.is_lis[v] = 1;
            }
            init_state_info.init_state = init_state;
            init_state_info.lis_start_index = start_i;
            init_state_info.lis_start = 0;
            init_state_info.id_hash = make_id_hash(id);
            init_state_info.init_posa.resize(init_state.initial_N);
            for (int i = 0; i < (int) init_state.A.size(); i++) {
                int value = init_state.A[i];
                my_assert(value >= 0 && value < init_state.initial_N);
                init_state_info.init_posa[value] = i;
            }
            
            init_state_info.is_lis_swap_v.reset();
            for (int i = 0; i < (int) init_state.A.size(); i++) {
                int v = init_state.A[i];
                if (init_state_info.is_lis[v]) {
                    continue;
                }
                
                int prev_lis_value = -1;
                for (int j = 1; j < (int) init_state.A.size(); j++) {
                    int prev_i = (i - j + init_state.A.size()) % init_state.A.size();
                    int prev_v = init_state.A[prev_i];
                    if (init_state_info.is_lis[prev_v]) {
                        prev_lis_value = prev_v;
                        break;
                    }
                }
                
                int next_lis_value = -1;
                for (int j = 1; j < (int) init_state.A.size(); j++) {
                    int next_i = (i + j) % init_state.A.size();
                    int next_v = init_state.A[next_i];
                    if (init_state_info.is_lis[next_v]) {
                        next_lis_value = next_v;
                        break;
                    }
                }
                my_assert(prev_lis_value != -1 && next_lis_value != -1);
                    int prev_lis_idx = -1;
                    int next_lis_idx = -1;
                    for (int k = 0; k < (int) lis.size(); k++) {
                        if (lis[k] == prev_lis_value) {
                            prev_lis_idx = k;
                        }
                        if (lis[k] == next_lis_value) {
                            next_lis_idx = k;
                        }
                    }
                    my_assert(prev_lis_idx != -1 && next_lis_idx != -1);
                    
                 int prev_prev_idx = (prev_lis_idx - 1 + lis.size()) % lis.size();
                 int next_next_idx = (next_lis_idx + 1) % lis.size();
                 my_assert(!is_sorted_of_ring(lis[prev_lis_idx], v, lis[next_lis_idx]));
                 if(is_sorted_of_ring(lis[prev_prev_idx], v, lis[prev_lis_idx])){
                    init_state_info.is_lis_swap_v[v] = true;
                 }
                 if(is_sorted_of_ring(lis[next_lis_idx], v, lis[next_next_idx])){
                    init_state_info.is_lis_swap_v[v] = true;
                 }
            }

            State st(init_state.A, init_state.B, false);
            my_vector<command> cmds;
            int sorted_a_count = (int) lis.size();
            int initial_turn = 0;
            int state_n = init_state.current_N;
            double sum = 0.0;
            my_assert(st.B.size() == 0);
            for (int i = 0; i < (int) st.A.size(); i++) {
                int av = st.A[i];
                if (init_state_info.is_lis[av]) {
                    continue;
                }
                sum += min(av, state_n - av);
            }
            my_assert(lis.size());
            int min_a_pos = get_pos(st.A, lis[0]);
            my_assert(min_a_pos >= 0);
            int min_a_v = lis[0];

            StateHash sh(st);
            uint64_t a_hash = sh.A.all_hash;
            uint64_t b_hash = sh.B.all_hash;
            my_bitset<MAX_N> is_sorted_v;
            for (int v: lis) {
                is_sorted_v[v] = true;
            }
            my_bitset<MAX_N> a2b_skipped_vals;
            BeamState<MAX_N> state(-1, initial_turn, sorted_a_count, -1, BeamQuery(), 0.0, sum,
                                   0, -1, min_a_pos, min_a_v, a_hash, b_hash, is_sorted_v, a2b_skipped_vals);
            SortedStateA sorted_state_a = create_sorted_state_a(st, state);
            init_state_info.init_sorted_state_a.clear();
            init_state_info.init_sorted_state_a.reserve(sorted_state_a.A.size());
            for (int i = 0; i < sorted_state_a.A.size(); i++) {
                init_state_info.init_sorted_state_a.push_back(sorted_state_a.A[i]);
            }
            initial_results.push_back({state, cmds, init_state_info});
        }

        return initial_results;
    }

//    template<int MAX_N>
//    static my_vector<BeamState<MAX_N>> generate_initial_states(const State &init_state) {
//        auto res = generate_initial_results<MAX_N>(init_state);
//        my_vector<BeamState<MAX_N>> out;
//        out.reserve(res.size());
//        for (auto &r: res) out.push_back(r.state);
//        return out;
//    }
};

#endif //UNTITLED_BEAMINITIALIZER_H

