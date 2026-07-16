//x
// filepath: sort_algorithm/YakinamashiQuery/State/YakinamashiState.h
#pragma once

#include "all_include.h"
#include "QueriesState.h"
#include "sort_algorithm/Yakinamashi/Order/AOrder.h"
#include "sort_algorithm/Yakinamashi/Order/BOrder.h"

class State;

//x A/Bの順序情報を値としてまとめる
struct QueryOrder {
    AOrder aorder;
    BOrder border;

    QueryOrder(AOrder init_aorder, BOrder init_border)
            : aorder(std::move(init_aorder)),
              border(std::move(init_border)) {}

    bool operator==(const QueryOrder &rhs) const {
        return aorder.get_all_order_entries() == rhs.aorder.get_all_order_entries() &&
               border.get_all_order_entries() == rhs.border.get_all_order_entries();
    }

    bool operator!=(const QueryOrder &rhs) const {
        return !(*this == rhs);
    }
};

template<size_t MAX_N>
struct EraseOrderSnapshot {
    QueryOrder before_erased_order;
    QueryOrder after_erased_order;
    my_bitset<MAX_N> is_erased_v;

    explicit EraseOrderSnapshot(const QueryOrder& init_order)
            : before_erased_order(init_order),
              after_erased_order(init_order),
              is_erased_v() {}
};

//x 焼きなましで更新される状態を一つに集約する
template<size_t MAX_N>
class YakinamashiState {
public:
    QueryOrder query_order;
    QueriesState queries_state;

    YakinamashiState(my_bitset<MAX_N> init_is_lis,
                     QueryOrder init_query_order,
                     QueriesState init_queries_state)
            :
              query_order(std::move(init_query_order)),
              queries_state(std::move(init_queries_state)) {}

    bool operator==(const YakinamashiState &rhs) const {
        return query_order == rhs.query_order &&
               queries_state == rhs.queries_state;
    }

    bool operator!=(const YakinamashiState &rhs) const {
        return !(*this == rhs);
    }

};

template<size_t MAX_N>
struct PhysicalDestroyContext {
    State& current_st;
    YakinamashiState<MAX_N>& yst;
    EraseOrderSnapshot<MAX_N> erase_order_snapshot;

    PhysicalDestroyContext(State& init_current_st, YakinamashiState<MAX_N>& init_yst)
            : current_st(init_current_st),
              yst(init_yst),
              erase_order_snapshot(init_yst.query_order) {}
};
