//
// Created by Auto
//

#ifndef UNTITLED_ORDERBASE_H
#define UNTITLED_ORDERBASE_H

#include "../../../all_include.h"
#include "../../../State.h"
#include "../../../RingBuffer500.h"
#include "ValState.h"
#include "../../InitBeam/BeamQuery.h"

class OrderBase {
protected:
    int init_N;
    my_vector<OrderEntry> order_entries; // order順に並んだエントリ（インデックスがorder）

    int find_entry_order(int value, ValState target_val_state, const my_vector<OrderEntry>& order_entries) const;
    
    void insert_entry_at_position(int position, const OrderEntry& entry, my_vector<OrderEntry>& order_entries);

    void process_insert_query(const BeamQuery& query, const State& state_after,
                             const RingBuffer500& stack,
                             const my_vector<ValState>& val_state_map,
                             my_vector<OrderEntry>& order_entries);

    void erase_entries_by_values(const my_vector<int>& values);
    
public:
    OrderBase(int init_N_) : init_N(init_N_) {
        order_entries.clear();
    }

    const my_vector<OrderEntry>& get_all_order_entries() const {
        return order_entries;
    }

    const my_vector<OrderEntry>& get_order_entries() const {
        return order_entries;
    }

    int get_all_order_entries_size() const {
        return order_entries.size();
    }
};

#endif //UNTITLED_ORDERBASE_H

