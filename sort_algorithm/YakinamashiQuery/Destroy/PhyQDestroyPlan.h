#pragma once

#include "util.h"

// 責務: PHY_Q は physical destroy と query destroy を v ごとに分ける arm であり、その対象 v と physical 側 v を保持する。
// 必要な理由: destroy 後の query 削除集合と、construct 時の physical/query 分岐を同じ情報源から扱うため。
template<size_t MAX_N>
struct PhyQDestroyPlan {
    my_vector<std::pair<int, int>> destroyed_vs;
    my_bitset<MAX_N> is_physical_v;
};
