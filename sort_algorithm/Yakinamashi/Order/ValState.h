//
// Created by Auto
//

#ifndef UNTITLED_VALSTATE_H
#define UNTITLED_VALSTATE_H

#include "../../../all_include.h"
#include "util.h"

// : A 側の初期固定値は is_lis で管理し、ValState から INIT_SORTED を外して TARGET へ寄せる。
enum class ValState {
    NONE,
    INIT,
    TARGET
};

inline bool is_init(ValState val_state) {
    return val_state == ValState::INIT;
}

inline bool is_targeted(ValState val_state) {
    return val_state == ValState::TARGET;
}

struct OrderEntry {
    int value;
    ValState val_state;

    OrderEntry() : value(-1), val_state(ValState::INIT) {}

    OrderEntry(int v, ValState val_state) : value(v), val_state(val_state) {}

    bool operator==(const OrderEntry& other) const {
        return value == other.value && val_state == other.val_state;
    }

    bool operator!=(const OrderEntry& other) const {
        return !(*this == other);
    }
};

inline ostream& operator<<(ostream& os, const ValState& val_state) {
    switch (val_state) {
        case ValState::NONE:
            os << "NONE";
            break;
        case ValState::INIT:
            os << "INIT";
            break;
        case ValState::TARGET:
            os << "TARGET";
            break;
        default:
            my_assert(false);
            os << "UNKNOWN_VAL_STATE";
            break;
    }
    return os;
}

inline ostream& operator<<(ostream& os, const OrderEntry& entry) {
    os << "{value:" << entry.value << ", val_state:" << entry.val_state << "}";
    return os;
}

inline void deb_val_st(const my_vector<ValState>& val_states) {
    std::ostringstream oss;
    for (int i = 0; i < val_states.size(); i++) {
        if (i > 0) {
            oss << ",  ";
        }
        oss << i << ":" << val_states[i];
    }
    out(oss.str());
}

#endif //UNTITLED_VALSTATE_H
