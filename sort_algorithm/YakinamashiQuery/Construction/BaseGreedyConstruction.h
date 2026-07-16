#pragma once
#include "YakinamashiState.h"
#include "State.h"
#include "util.h"
#include <utility>

template<size_t MAX_N>
class BaseGreedyConstruction {
public:
    virtual ~BaseGreedyConstruction() = default;

    // 削除されたvをystに再挿入する
    virtual void construct(
        YakinamashiState<MAX_N>& yst, const my_vector<std::pair<int, int>>& destroyed_vs
    ) = 0;
};


