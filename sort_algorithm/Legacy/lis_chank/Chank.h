//
//

#ifndef UNTITLED_CHANK_H
#define UNTITLED_CHANK_H
// human_review_begin: 公開用コピー単体で include が解決できるようにローカル参照へ変更する。
#include "all_include.h"
#include "util.h"
// human_review_end


template<size_t ARG_N>
class ChankInfo {
public:
    const int init_id = -1;
    array<int, ARG_N> chank_ids;
    vector<int> chank_sizes;
    int size;
    vector<vector<int>> chank_vals;

    void init_ids() {
        for (int i = 0; i < ARG_N; i++) {
            chank_ids[i] = init_id;
        }
    }

    template<class List>
//chank_num個に分ける
//(i) := 何番目のchankに入るかをもつ
    void set_chank_id(const List &list) {
        auto set_id_by_sorted = [&](const auto &sorted) {
            int was_chank_sum = 0;
            for (int chank_id = 0; chank_id < chank_sizes.size(); chank_id++) {
                for (int i = 0; i < chank_sizes[chank_id]; i++) {
                    chank_ids[sorted[was_chank_sum + i]] = chank_id;
                }
                was_chank_sum += chank_sizes[chank_id];
            }
        };
        init_ids();
        set_id_by_sorted(get_sorted(list));
    }

    void validate_val(int val) {
        my_assert(0 <= val && val < ARG_N);
    }

    template<class List>
    void validate_arg(const List &list) {
        auto validate_vals = [&]() {
            for (auto &v: list) {
                validate_val(v);
            }
        };
        auto validate_chank = [&]() {
            for (auto &sz: chank_sizes) {
                //0を許す
                //基本チャンク3つだけど、入力によっては2つになるとか　良いと思うから
                my_assert(0 <= sz && sz <= list.size());
            }
            my_assert(list.size() == sum(chank_sizes));
        };
        validate_vals();
        validate_chank();
    }

public:
    template<class List>
    ChankInfo(const List &list, const vector<int> &chank_sizes) : chank_sizes(chank_sizes) {
        validate_arg(list);
        set_chank_id(list);
        size = sum(chank_sizes);
        chank_vals.resize(chank_sizes.size());
        for(auto& v : list){
            chank_vals[get_chank_id(v)].push_back(v);
        }
        for (int i = 0; i < chank_sizes.size(); i++){
            std::sort(chank_vals[i].begin(), chank_vals[i].end());
        }
    }

    int contains_cnt() {
        return size;
    }

    int chank_size(int chank_id) {
        return chank_sizes[chank_id];
    }

    int chak_id_cnt() {
        return chank_sizes.size();
    }

    bool belong_chank(int val) {
        return chank_ids[val] != init_id;
    }

    int get_chank_id(int val) {
        validate_val(val);
        //chankに含まれている物でないといけない
        my_assert(belong_chank(val));
        return chank_ids[val];
    }

    void print() {
        out("chank_ids");
        out_with_idx(chank_ids, init_id);
        for (int i = 0; i < chank_vals.size(); i++){
            out("id", i);
            out(chank_vals[i]);
        }
    }
};


void test_get_chank();



#endif //UNTITLED_CHANK_H
