#pragma once
#include "YakinamashiState.h"
#include "util.h"
#include <utility>

template<size_t MAX_N>
class BaseDestroy {
public:
    virtual ~BaseDestroy() = default;

    // ystを破壊し、削除したvのリストを返す
    virtual my_vector<std::pair<int, int>> destroy(YakinamashiState<MAX_N>& yst) = 0;
};
