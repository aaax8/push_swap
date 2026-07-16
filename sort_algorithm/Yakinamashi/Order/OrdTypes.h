//x
// sort_algorithm/Yakinamashi/Order/OrdTypes.h
#pragma once

#include "../../../util.h"

//(v, vのOrderType)の組に対して割り当てられる値
//この値が小さい順に、stackで円環上に昇順に並ぶ
//それらは相異なる
//例えば同じvについても
// ・AにおけるINIT状態のvのOrder
// ・AにPUSHEDされた状態のvのOrder
//など、上の二つは異なるOrder
//stackにおける各値は、そのOrderTypeによってAllOrdが決定され
//YakinamashiQueryにおいては、stackから取得できるAllOrd列は、常に円環上で昇順に並ぶことが保証される

struct RelOrd;
struct OrdPos;

inline void validate_RelOrd(int rel_ord_v, int stack_size);
inline void validate_OrdPos(int ord_pos_v, int stack_size);
inline void validate_RelOrd(const RelOrd& rel_ord, int stack_size);
inline void validate_OrdPos(const OrdPos& ord_pos, int stack_size);

struct AllOrd {
    int value;

    explicit constexpr AllOrd(int v) : value(v) {
        //-1で使うところがある
//        my_assert(v >= 0);
    }

    constexpr int get() const { return value; }

    constexpr bool operator==(AllOrd other) const { return value == other.value; }
    constexpr bool operator!=(AllOrd other) const { return value != other.value; }
    constexpr bool operator<(AllOrd other) const { return value < other.value; }
    constexpr bool operator<=(AllOrd other) const { return value <= other.value; }
    constexpr bool operator>(AllOrd other) const { return value > other.value; }
    constexpr bool operator>=(AllOrd other) const { return value >= other.value; }

    constexpr bool operator==(int other) const { return value == other; }
    constexpr bool operator!=(int other) const { return value != other; }
    constexpr bool operator<(int other) const { return value < other; }
    constexpr bool operator<=(int other) const { return value <= other; }
    constexpr bool operator>(int other) const { return value > other; }
    constexpr bool operator>=(int other) const { return value >= other; }
};

//stackから取得できるAllOrd列を座標圧縮した物
//sizeNのstackに並ぶRelOrd列は、常に0~N-1が円環上に昇順に並ぶことが保証される
struct RelOrd{
    int value;
    RelOrd(){}
    explicit constexpr RelOrd(int v) : value(v) {
        my_assert(v >= 0);
    }

    constexpr int get() const { return value; }
    constexpr bool operator==(RelOrd other) const { return value == other.value; }
    constexpr bool operator!=(RelOrd other) const { return value != other.value; }
    constexpr bool operator<(RelOrd other) const { return value < other.value; }
    constexpr bool operator<=(RelOrd other) const { return value <= other.value; }
    constexpr bool operator>(RelOrd other) const { return value > other.value; }
    constexpr bool operator>=(RelOrd other) const { return value >= other.value; }
};


//範囲 : [0, stack.size())
//stackのposを表す
struct OrdPos {
    int value;

    // Responsibility: create an unused sentinel OrdPos for container storage.
    // Reason: vec_array<OrdPos, N> needs default construction for unused slots without changing explicit valid OrdPos construction.
    constexpr OrdPos() : value(-1) {}

    explicit constexpr OrdPos(int v) : value(v) {
        my_assert(v >= 0);
    }

    constexpr int get() const { return value; }

    // 比較演算子（RelOrd同士）
    constexpr bool operator==(OrdPos other) const { return value == other.value; }
    constexpr bool operator!=(OrdPos other) const { return value != other.value; }
    constexpr bool operator<(OrdPos other) const { return value < other.value; }
    constexpr bool operator<=(OrdPos other) const { return value <= other.value; }
    constexpr bool operator>(OrdPos other) const { return value > other.value; }
    constexpr bool operator>=(OrdPos other) const { return value >= other.value; }

    // 比較演算子（intとの比較）
    // constexpr bool operator==(int other) const { return value == other; }
    // constexpr bool operator!=(int other) const { return value != other; }
    // constexpr bool operator<(int other) const { return value < other; }
    // constexpr bool operator<=(int other) const { return value <= other; }
    // constexpr bool operator>(int other) const { return value > other; }
    // constexpr bool operator>=(int other) const { return value >= other; }
};



// サイズ保証（ゼロコスト抽象化の確認）
static_assert(sizeof(AllOrd) == sizeof(int), "AllOrd must be same size as int");
static_assert(sizeof(OrdPos) == sizeof(int), "OrdPos must be same size as int");
static_assert(alignof(AllOrd) == alignof(int), "AllOrd must have same alignment as int");
static_assert(alignof(OrdPos) == alignof(int), "OrdPos must have same alignment as int");

inline OrdPos to_ord_pos(const RelOrd &rel_ord, int stack_size){
    if(rel_ord.get() == stack_size){
        return OrdPos(0);
    }else{
        return OrdPos(rel_ord.get());
    }
}

inline RelOrd to_rel_ord(const OrdPos &ord_pos){
    return RelOrd(ord_pos.get());
}



inline void validate_RelOrd(int rel_ord_v, int stack_size){
    my_assert(0 <= rel_ord_v && rel_ord_v <= stack_size);
}

inline void validate_RelOrd(const RelOrd& rel_ord, int stack_size){
    validate_RelOrd(rel_ord.get(), stack_size);
}

inline void validate_OrdPos(int ord_pos_v, int stack_size){
    my_assert((ord_pos_v==0)|| (0 <= ord_pos_v && ord_pos_v < stack_size));
}

inline void validate_OrdPos(const OrdPos& ord_pos, int stack_size){
    validate_OrdPos(ord_pos.get(), stack_size);
}



template <typename T>
concept OrdPos_or_RelOrd = is_same_v<T, OrdPos> || is_same_v<T, RelOrd>;
