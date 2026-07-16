// YakinamashiQueryConstants.h
// YakinamashiQuery用の定数をまとめる
#pragma once

//YakinamashiQueryの略
namespace YQConstants {
    //
    template<size_t MAX_N>
    constexpr size_t GET_MAX_ALL_ORD_A() {
        return MAX_N * 2 + 5;
    }

    template<size_t MAX_N>
    constexpr size_t GET_MAX_ALL_ORD_B() {
        return MAX_N + 5;
    }
}
