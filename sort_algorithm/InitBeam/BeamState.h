//
// Created by Auto
//

#ifndef UNTITLED_BEAMSTATE_H
#define UNTITLED_BEAMSTATE_H

#include "all_include.h"
#include "BeamQuery.h"
#include "RingBuffer500.h"

//Runnerに持たせられる情報多そう
//rとしたものは持たせられそう
template<int MAX_N>
struct BeamState {
    int init_id;
    const int first_init_id;
    int parent_idx;
    BeamQuery last_query;
    double score;
    int current_turn;//r
    int sorted_a_count;//r
    double yet_sorted_dis_to_0_sum;//r
    int act_cnt;//遷移回数（beams_by_act_cntの添え字として使用）
    int cur_idx;//beams_by_act_cnt[act_cnt][cur_idx]が自分自身を指す
    int min_a_pos;//aの中で最小の値の位置
    int min_a_v;//infで初期化
    bool is_deleted;//削除済みかどうか
    uint64_t a_hash;
    uint64_t b_hash;
    my_bitset<MAX_N> is_sorted_v;
    my_bitset<MAX_N> a2b_skipped_vals;
private:
    //後から代入する
    double a_dis_sum;//Aの各要素の、正しい位置との距離の合計、sum(上限の距離とのmin)

public:
    void set_a_dis_sum(double sum) {
        a_dis_sum = sum;
    }

    double get_a_dis_sum() const {
        my_assert(a_dis_sum >= 0);
        return a_dis_sum;
    }

    BeamState() : init_id(-1), first_init_id(-1), parent_idx(-1), last_query(), score(0.0),
                  current_turn(0), sorted_a_count(0), yet_sorted_dis_to_0_sum(0.0), act_cnt(0),
                  cur_idx(-1), min_a_pos(-1), min_a_v(numeric_limits<int>::max()),
                  is_deleted(false), a_hash(0), b_hash(0), is_sorted_v(), a2b_skipped_vals(), a_dis_sum(inf) {

    }


    //a_dis_sumを後で代入
    BeamState(int init_id, int current_turn, int sorted_a_count,
              int parent_idx,
              const BeamQuery &last_query, double score, double sum,
              int act_cnt,
              int cur_idx,
              int min_a_pos,
              int min_a_v,
              uint64_t a_hash,
              uint64_t b_hash,
              const my_bitset<MAX_N> &is_sorted_v,
              const my_bitset<MAX_N> &a2b_skipped_vals
    )
            : init_id(init_id), first_init_id(init_id), parent_idx(parent_idx),
              last_query(last_query), score(score), current_turn(current_turn),
              sorted_a_count(sorted_a_count), yet_sorted_dis_to_0_sum(sum),
              act_cnt(act_cnt), cur_idx(cur_idx),
              min_a_pos(min_a_pos), min_a_v(min_a_v),
              is_deleted(false), a_hash(a_hash), b_hash(b_hash),
              is_sorted_v(is_sorted_v), a2b_skipped_vals(a2b_skipped_vals), a_dis_sum(inf) {


    }

    BeamState &operator=(const BeamState &other) {
        if (this != &other) {
            init_id = other.init_id;
            const_cast<int &>(first_init_id) = other.first_init_id;
            parent_idx = other.parent_idx;
            last_query = other.last_query;
            score = other.score;
            current_turn = other.current_turn;
            sorted_a_count = other.sorted_a_count;
            yet_sorted_dis_to_0_sum = other.yet_sorted_dis_to_0_sum;
            act_cnt = other.act_cnt;
            cur_idx = other.cur_idx;
            min_a_pos = other.min_a_pos;
            min_a_v = other.min_a_v;
            is_deleted = other.is_deleted;
            a_hash = other.a_hash;
            b_hash = other.b_hash;
            is_sorted_v = other.is_sorted_v;
            a2b_skipped_vals = other.a2b_skipped_vals;
            a_dis_sum = other.a_dis_sum;
        }
        return *this;
    }
};

struct BeamPos {
    int act_cnt;
    int idx;

    BeamPos() : act_cnt(0), idx(0) {}

    BeamPos(int a, int i) : act_cnt(a), idx(i) {}

    bool operator==(const BeamPos &other) const {
        return act_cnt == other.act_cnt && idx == other.idx;
    }

    bool operator!=(const BeamPos &other) const {
        return !(*this == other);
    }
};

template<int MAX_N>
inline ostream &operator<<(ostream &os, const BeamState<MAX_N> &st) {
    os << "{init_id:" << st.init_id
       << ", parent_idx:" << st.parent_idx
       << ", current_turn:" << st.current_turn
       << ", sorted_a_count:" << st.sorted_a_count
       << ", score:" << st.score
       << ", yet_sorted_dis_to_0_sum:" << st.yet_sorted_dis_to_0_sum
       << ", act_cnt:" << st.act_cnt
       << ", min_a_pos:" << st.min_a_pos
       << ", min_a_v:" << st.min_a_v
       << ", is_deleted:" << st.is_deleted
       << ", a_hash:" << st.a_hash
       << ", b_hash:" << st.b_hash
       << ", last_query:" << st.last_query << "}";
    return os;
}

#endif //UNTITLED_BEAMSTATE_H

