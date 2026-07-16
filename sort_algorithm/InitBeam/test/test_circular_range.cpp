//
// Created by Auto
//

#include "../CircularRange.h"
#include "util.h"
#include "all_include.h"

using namespace ChunkSeparator;

void test_circular_range_basic() {
    out("=== test_circular_range_basic ===");
    
    constexpr int N = 100;
    
    // [10, 50) の範囲 (size=40)
    CircularRange range(10, 40, N);
    
    // size() のテスト
    my_assert(range.size() == 40);
    out("size:", range.size(), "OK");
    
    // contains() のテスト - 範囲内
    my_assert(range.contains(10) == true);
    my_assert(range.contains(25) == true);
    my_assert(range.contains(49) == true);
    out("contains (within range): OK");
    
    // contains() のテスト - 範囲外
    my_assert(range.contains(9) == false);
    my_assert(range.contains(50) == false);
    my_assert(range.contains(99) == false);
    my_assert(range.contains(0) == false);
    out("contains (outside range): OK");
    
    // get_start(), get_end() のテスト
    my_assert(range.get_start() == 10);
    my_assert(range.get_end() == 50);
    out("get_start/get_end: OK");
    
    out("test_circular_range_basic: PASSED");
}

void test_circular_range_edge_cases() {
    out("=== test_circular_range_edge_cases ===");
    
    constexpr int N = 100;
    
    // 単一要素の範囲 [5, 6) (size=1)
    CircularRange single_range(5, 1, N);
    my_assert(single_range.size() == 1);
    my_assert(single_range.contains(5) == true);
    my_assert(single_range.contains(6) == false);
    out("single element range: OK");
    
    // 範囲の開始が0の場合 [0, 10)
    CircularRange start_zero_range(0, 10, N);
    my_assert(start_zero_range.size() == 10);
    my_assert(start_zero_range.contains(0) == true);
    my_assert(start_zero_range.contains(9) == true);
    my_assert(start_zero_range.contains(10) == false);
    out("start at zero: OK");
    //
    // 範囲の終了がNの場合 [90, 100) (size=10)
    CircularRange end_max_range(90, 10, N);
    my_assert(end_max_range.size() == 10);
    my_assert(end_max_range.contains(90) == true);
    my_assert(end_max_range.contains(99) == true);
    out("end at N: OK");
    
    out("test_circular_range_edge_cases: PASSED");
}

void test_circular_range_wrapping() {
    out("=== test_circular_range_wrapping ===");
    
    constexpr int N = 100;
    
    // 折り返し範囲 [480, 20) -> 480→499→0→19 (40個)
    // ただしN=100なので、[80, 20) でテスト (size=40)
    CircularRange wrapping_range(80, 40, N);
    my_assert(wrapping_range.size() == 40); // (100-80) + 20 = 40
    out("wrapping range size:", wrapping_range.size());
    
    // 範囲内のテスト - [80, 100)
    my_assert(wrapping_range.contains(80) == true);
    my_assert(wrapping_range.contains(90) == true);
    my_assert(wrapping_range.contains(99) == true);
    out("wrapping range contains [80, 100): OK");
    
    // 範囲内のテスト - [0, 20)
    my_assert(wrapping_range.contains(0) == true);
    my_assert(wrapping_range.contains(10) == true);
    my_assert(wrapping_range.contains(19) == true);
    my_assert(wrapping_range.contains(20) == false);
    out("wrapping range contains [0, 20): OK");
    
    // 範囲外のテスト
    my_assert(wrapping_range.contains(79) == false);
    my_assert(wrapping_range.contains(20) == false);
    my_assert(wrapping_range.contains(50) == false);
    out("wrapping range excludes [20, 80): OK");
    
    // 折り返し範囲 [95, 5) -> 95→99→0→4 (10個) (size=10)
    CircularRange wrapping_small_range(95, 10, N);
    my_assert(wrapping_small_range.size() == 10); // (100-95) + 5 = 10
    my_assert(wrapping_small_range.contains(95) == true);
    my_assert(wrapping_small_range.contains(99) == true);
    my_assert(wrapping_small_range.contains(0) == true);
    my_assert(wrapping_small_range.contains(4) == true);
    my_assert(wrapping_small_range.contains(5) == false);
    my_assert(wrapping_small_range.contains(94) == false);
    out("wrapping small range: OK");
    
    out("test_circular_range_wrapping: PASSED");
}

