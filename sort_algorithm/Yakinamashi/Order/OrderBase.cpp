//
// Created by Auto
//

#include "OrderBase.h"
#include "../../../util.h"
//x
int OrderBase::find_entry_order(int value, ValState target_val_state, const my_vector<OrderEntry>& order_entries) const {
    for (int i = 0; i < (int) order_entries.size(); i++) {
        if (order_entries[i].value == value && order_entries[i].val_state == target_val_state) {
            return i;
        }
    }
    my_assert(false);
    return -1;
}
//x
void OrderBase::insert_entry_at_position(int position, const OrderEntry& entry, my_vector<OrderEntry>& order_entries) {
    my_assert(0 <= position && position <= (int) order_entries.size());

    order_entries.insert(order_entries.begin() + position, entry);
}
//x
void OrderBase::process_insert_query(const BeamQuery& query, const State& state_after,
                                     const RingBuffer500& stack,
                                     const my_vector<ValState>& val_state_map,
                                     my_vector<OrderEntry>& order_entries) {
    my_assert(!val_state_map.empty());

    int v = query.get_tar_val();

    int insert_pos = -1;
    for (int i = 0; i < stack.size(); i++) {
        if (stack[i] == v) {
            insert_pos = i;
            break;
        }
    }
    my_assert(insert_pos == 0 || insert_pos == 1);

    int stack_size = stack.size();
    int new_position;

    if (stack_size == 1 || stack_size == 2) {
        new_position = 0;
    } else {
        int upper_v = stack[mod(insert_pos - 1, stack_size)];
        int lower_v = stack[mod(insert_pos + 1, stack_size)];

        ValState upper_val_state = val_state_map[upper_v];
        ValState lower_val_state = val_state_map[lower_v];

        my_assert(upper_val_state != ValState::NONE);
        my_assert(lower_val_state != ValState::NONE);

        int upper_order = find_entry_order(upper_v, upper_val_state, order_entries);
        int lower_order = find_entry_order(lower_v, lower_val_state, order_entries);

        my_assert(upper_order >= 0 && lower_order >= 0);

        int circular_distance = (lower_order - upper_order + (int) order_entries.size()) % order_entries.size();
        int half_distance = (circular_distance + 1) / 2;
        new_position = (upper_order + half_distance) % order_entries.size();
    }

    // _BEGIN: 挿入済み entry は ValState::TARGET へ統一する。
    OrderEntry new_entry(v, ValState::TARGET);
    // _END
    insert_entry_at_position(new_position, new_entry, order_entries);
}

void OrderBase::erase_entries_by_values(const my_vector<int>& values) {
    my_bitset<MAX_BUFF> erase_v;
    for (int v: values) {
        my_assert(0 <= v && v < init_N);
        erase_v.set(v);
    }
    my_vector<OrderEntry> kept_entries;
    kept_entries.reserve(order_entries.size());
    for (const OrderEntry& entry: order_entries) {
        my_assert(0 <= entry.value && entry.value < init_N);
        if (erase_v[entry.value]) continue;
        kept_entries.push_back(entry);
    }
    order_entries.swap(kept_entries);
}
