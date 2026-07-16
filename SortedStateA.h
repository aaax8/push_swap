//
// Created by Auto
//

#ifndef UNTITLED_SORTEDSTATEA_H
#define UNTITLED_SORTEDSTATEA_H

#include "RingBuffer500.h"
#include "State.h"
#include "all_include.h"
#include "util.h"


template<int MAX_N>
class BeamState;

class BeamStateRunner;

class SortedStateA {
public:
    RingBuffer500 A;
    int N;
    int min_a_pos;

    SortedStateA() : N(0), min_a_pos(-1) {}

    void validate_min_a_pos() const{
#ifdef DEBUG
        for (int i = 0; i < A.size(); i++) {
            my_assert(A[min_a_pos] <= A[i]);
        }
#endif
    }

    void validate_sorted(int min_a_pos) {
#ifdef DEBUG
        for (int dis = 0; dis + 1 < A.size(); dis++) {
            int cur = mod(min_a_pos + dis, A.size());
            int nex = mod(min_a_pos + dis + 1, A.size());
            my_assert(A[cur] < A[nex]);
        }
#endif
    }

    SortedStateA(const RingBuffer500 &A, int min_a_pos) : A(A), N(A.size()), min_a_pos(min_a_pos) {
        validate_min_a_pos();
        validate_sorted(min_a_pos);
    }

    SortedStateA(const SortedStateA &rhs) : A(rhs.A), N(rhs.N), min_a_pos(rhs.min_a_pos) {}

    SortedStateA &operator=(const SortedStateA &rhs) {
        A = rhs.A;
        N = rhs.N;
        min_a_pos = rhs.min_a_pos;
        return *this;
    }

    bool operator==(const SortedStateA &rhs) const {
        return A == rhs.A && N == rhs.N && min_a_pos == rhs.min_a_pos;
    }

    int size() const { return A.size(); }

    void print() const {
        out("A");
        A.print();
    }

    void ra() {
        my_assert(A.size() >= 2);
        A.push_back(A[0]);
        A.pop_front();
        if (min_a_pos == 0) {
            min_a_pos = A.size() - 1;
        } else {
            min_a_pos--;
        }
        validate_min_a_pos();
    }

    void rra() {
        my_assert(A.size() >= 2);
        A.push_front(A.back());
        A.pop_back();
        if (min_a_pos == A.size() - 1) {
            min_a_pos = 0;
        } else {
            min_a_pos++;
        }
        validate_min_a_pos();
    }

    int calc_min_v() const{
        return A[min_a_pos];
    }

    void pa(int v) {
        int min_v = calc_min_v();
        if (min_v > v) {
            min_a_pos = 0;
        } else {
            min_a_pos++;
        }
        A.push_front(v);
        N++;
        my_assert(A[0] == v);
#ifdef DEBUG
        if(A.size() > 2){
            my_assert(is_sorted_of_ring(A.back(), A[0], A[1]));
        }
#endif
        validate_min_a_pos();
    }

    void pb() {
        if (min_a_pos == 0) { ;
        } else {
            min_a_pos--;
        }
        my_assert(A.size() > 0);
        A.pop_front();
        N--;
        my_assert(N > 0);//N==0のときのmin_a_posは未定義なので
        validate_min_a_pos();
    }

    template<int MAX_N>
    void if_sorted_ra(const State &st, const bitset<MAX_N> &is_sorted_v);

    template<int MAX_N>
    void if_sorted_rra(const State &st, const bitset<MAX_N> &is_sorted_v);
};

#include "sort_algorithm/InitBeam/BeamState.h"

template<int MAX_N>
inline SortedStateA create_sorted_state_a(const State &st,
                                   const BeamState<MAX_N> &beam_st) {
    RingBuffer500 sorted_a;
    int min_i = -1;
    int min_v = numeric_limits<int>::max();
    for (int i = 0; i < st.A.size(); ++i) {
        int v = st.A[i];
        if (beam_st.is_sorted_v[v]) {
            if (chmi(min_v, v)) {
                min_i = sorted_a.size();
            }
            sorted_a.push_back(v);
        }
    }
    my_assert(min_i != -1);
    return SortedStateA(sorted_a, min_i);
}

template<int MAX_N>
inline void validate_is_sorted_v(const State &st, const bitset<MAX_N> &is_sorted_v) {
#ifdef DEBUG
    vector<int> lis;
    for (int v = 0; v < MAX_N; v++) {
        if (is_sorted_v[v]) {
            lis.push_back(v);
        }
    }
    my_assert(lis.size() > 1);//ある程度ないと動かないかも
    int start = get_pos(st.A, lis[0]);
    int lis_i = 0;
    for (int dis = 0; dis < st.A.size(); ++dis) {
        int i = mod(start + dis, st.A.size());
        if (st.A[i] == lis[lis_i]) {
            lis_i++;
        }

        if (lis_i == lis.size())break;
    }
    my_assert(lis_i == lis.size());
    auto get_prev_lis_v = [&](int i) -> int {
        for (int dis = 1; dis < st.A.size(); ++dis) {
            int j = mod(i - dis, st.A.size());
            if (is_sorted_v[st.A[j]]) {
                return st.A[j];
            }
        }
        my_assert(false);
        return -1;
    };
    auto get_nex_lis_v = [&](int i) -> int {
        for (int dis = 0; dis < st.A.size(); ++dis) {
            int j = mod(i + dis, st.A.size());
            if (is_sorted_v[st.A[j]]) {
                return st.A[j];
            }
        }
        my_assert(false);
        return -1;
    };

    //増やせない事の確認
    for (int i = 0; i < st.A.size(); ++i) {
        if (is_sorted_v[st.A[i]]) {
            continue;
        }
        int prev_lis_v = get_prev_lis_v(i);
        int next_lis_v = get_nex_lis_v(i);
        my_assert(!is_sorted_of_ring(prev_lis_v, st.A[i], next_lis_v));
    }
#endif
}

template<int MAX_N>
inline void SortedStateA::if_sorted_ra(const State &st,
                                const bitset<MAX_N> &is_sorted_v) {
    validate_is_sorted_v<MAX_N>(st, is_sorted_v);
    my_assert(st.A.size() >= 2);
    if (is_sorted_v[st.A[0]]) {
        ra();
    }
}

template<int MAX_N>
inline void SortedStateA::if_sorted_rra(const State &st,
                                 const bitset<MAX_N> &is_sorted_v) {
    validate_is_sorted_v<MAX_N>(st, is_sorted_v);
    my_assert(st.A.size() >= 2);
    int back_idx = st.A.size() - 1;
    if (is_sorted_v[st.A[back_idx]]) {
        rra();
    }
}

#endif // UNTITLED_SORTEDSTATEA_H
