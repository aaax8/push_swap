//
// Created by Auto
//

#ifndef UNTITLED_CIRCULARRANGE_H
#define UNTITLED_CIRCULARRANGE_H

#include "../../all_include.h"
#include "../../util.h"

namespace ChunkSeparator {
    //x
    // 円環上の範囲を扱うクラス
    // 範囲は [range_start, range_end) の形式で、昇順で扱う
    // 折り返しを考慮（例: [480, 20) は 480→499→0→19）
    //o
    // 円環上の値を足し算する（modを使わず、引き算/足し算でNを調整）
    inline int add_circular(int a, int delta, int N) {
        my_assert(0 <= a && a < N);
        int result = a + delta;
        if (result >= N) {
            result -= N;
            my_assert(0 <= result && result < N);
        } else if (result < 0) {
            result += N;
            my_assert(0 <= result && result < N);
        }
        return result;
    }

    class CircularRange {
    private:
        int range_start;
        int range_size;
        int state_N;

        //o
        void validate_params(int start, int size, int n) const {
            #ifdef DEBUG
            if(n==0){
                my_assert(start==0 && size==0);
                return;
            }
            my_assert(0 <= n);
            my_assert(0 <= start && start < n);
            my_assert(0 <= size && size <= n);
             #endif
        }

        //o
        void validate_value(int value) const {
            my_assert(0 <= value && value < state_N);
        }

        //o
        // 範囲が折り返しているかどうか判定
        bool is_wrapping() const {
            return range_start + range_size > state_N;
        }

    public:
        //o
        CircularRange(int start, int size, int n)
                : range_start(start), range_size(size), state_N(n) {
            validate_params(range_start, range_size, state_N);
        }

        //o
        // 値が範囲に含まれるか判定（折り返し対応）
        bool contains(int value) const {
            validate_value(value);

            if (range_size == 0) {
                return false;
            }

            if (range_size == state_N) {
                return true;
            }

            if (is_wrapping()) {
                // 折り返しの場合: [start, N) または [0, end)
                int end = get_end();
                return (range_start <= value) || (value < end);
            } else {
                // 折り返しなしの場合: [start, end)
                return range_start <= value && value < range_start + range_size;
            }
        }

        //o
        int size() const {
            return range_size;
        }

        //o
        bool is_empty() const {
            return range_size == 0;
        }

        //o
        bool is_full() const {
            return range_size == state_N;
        }

        //o
        int get_start() const { return range_start; }

        //o
        int get_end() const {
            int end = range_start + range_size;
            if (end >= state_N) {
                return end - state_N;
            } else {
                return end;
            }
        }
        int get_N()const{
            return state_N;
        }
    };

    //x CircularRange を文字列化
    inline std::string to_string(const CircularRange &range) {
        std::stringstream ss;
        ss << "[" << range.get_start() << ", " << range.get_end() << ")";
        ss << ", size = " << range.size();
        ss << ", N = " << range.get_N();
        return ss.str();
    }

    //二つの範囲が重なるか
    //oo
    inline bool circular_ranges_overlap_nieve(const CircularRange &r1, const CircularRange &r2) {
        //r1とr2が重なるかどうか判定
        int r1_start = r1.get_start();
        int r1_end = r1.get_end();
        int r2_start = r2.get_start();
        int r2_end = r2.get_end();

        for(int loop_i = 0 ; loop_i < r1.size() ; loop_i++){
            int r1_v = ChunkSeparator::add_circular(r1_start, loop_i, r1.get_N());
            if(r2.contains(r1_v)){
                return true;
            }
        }
        return false;
    }
    //circular_ranges_overlap_nieveのテスト
    inline void test_circular_ranges_overlap_nieve() {
        {
            CircularRange r1(480, 40, 500); // 480-499,0-19
            CircularRange r2(10, 20, 500);  // 10-29
            my_assert(circular_ranges_overlap_nieve(r1, r2) == true);
        }
        {
            CircularRange r1(100, 50, 500); // 100-149
            CircularRange r2(140, 20, 500); // 140-159
            my_assert(circular_ranges_overlap_nieve(r1, r2) == true);
        }
        {
            CircularRange r1(200, 30, 500); // 200-229
            CircularRange r2(230, 20, 500); // 230-249
            my_assert(circular_ranges_overlap_nieve(r1, r2) == false);
        }
        {
            CircularRange r1(450, 60, 500); // 450-499,0-9
            CircularRange r2(5, 10, 500);   // 5-14
            my_assert(circular_ranges_overlap_nieve(r1, r2) == true);
        }
        {
            CircularRange r1(30, 60, 500); // 30-89
            CircularRange r2(450, 90, 500);   // 450~499, 0~39
            my_assert(circular_ranges_overlap_nieve(r1, r2) == true);
        }
        {
            CircularRange r1(0, 0, 500);   // empty
            CircularRange r2(100, 50, 500); // 100-149
            my_assert(circular_ranges_overlap_nieve(r1, r2) == false);
        }
        {
            CircularRange r1(0, 500, 500);   // full
            CircularRange r2(250, 100, 500); // 250-349
            my_assert(circular_ranges_overlap_nieve(r1, r2) == true);
        }
    }
}

#endif //UNTITLED_CIRCULARRANGE_H

