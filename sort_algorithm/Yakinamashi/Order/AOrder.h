//
// Created by Auto
//

#ifndef UNTITLED_AORDER_H
#define UNTITLED_AORDER_H

#include "../../../all_include.h"
#include "../../../State.h"
#include "ValState.h"
#include "OrdTypes.h"
#include "../../InitBeam/BeamQuery.h"
#include "OrderBase.h"
#include "util.h"

// : INIT と TARGET の二層へ整理し、初期固定値は init_v_orders のみで扱う。
class AOrder : public OrderBase {
private:
    my_vector<int> init_v_orders;
    my_vector<int> targeted_v_orders;

    void initialize_order_entries(const State &init_state);

    void append_lis_target_entries_after_init(const my_bitset<MAX_BUFF> &is_lis);

    void update_orders_from_entries();

public:
    AOrder(int init_N_);
    
    static AOrder build_from_queries(const State &init_state,
                                     const my_vector<BeamQuery> &queries,
                                     const my_bitset<MAX_BUFF> &is_lis);

    void build_orders_from_queries(const State &init_state,
                                   const my_vector<BeamQuery> &queries,
                                   const my_bitset<MAX_BUFF> &is_lis);

    my_vector<ValState> initial_value_type(const State &init_state) const;

    void apply_separate_cmds(State &current_state,
                             const my_vector<command> &sep_cmds);

    void apply_beam_queries(State &current_state, const my_vector<BeamQuery> &beam_queries,
                            my_vector<ValState> &val_state_in_a);

    static AOrder build(const my_vector<command> &sep_cmds,
                        const State &init_state,
                        const my_vector<BeamQuery> &beam_queries,
                        const my_bitset<MAX_BUFF> &is_lis);

    void erase_values(const my_vector<int>& values);

    void insert_entry(int index, const OrderEntry& entry);

    AllOrd insert_update_shift(int value, AllOrd new_all_ord_a);

    AllOrd insert_update_no_shift(int value, AllOrd new_all_ord_a);

    void rebase_init_ord_stopgap(int value, AllOrd before_all_ord);

    void swap_all_ord_entries(AllOrd left_all_ord, AllOrd right_all_ord);

    void normalize_min_front();
    void normalize_target_min_front();
    void normalize_target_max_back();
    void normalize_insert_shift(int value);

    void validate_tar_sorted() const;
    void validate_target_entry_order() const;

    void validate_ring_ord(const State &state,
                           const my_vector<ValState> &now_val_state);

    void validate_ord(const my_vector<command> &sep_cmds,
                      const State &init_state,
                      const my_vector<BeamQuery> &beam_queries);

    AllOrd get_init_all_order(int value) const {
        my_assert(0 <= value && value < init_N);
        int ord = init_v_orders[value];
        my_assert(ord != -1);
        return AllOrd(ord);
    }

    AllOrd get_target_order(int value) const {
        my_assert(0 <= value && value < init_N);
        int ord = targeted_v_orders[value];
        my_assert(ord != -1);
        return AllOrd(ord);
    }

    bool has_target_order(int value) const {
        my_assert(0 <= value && value < init_N);
        return targeted_v_orders[value] != -1;
    }

    AllOrd get_init_order(int v) const {
        return get_init_all_order(v);
    }

    AllOrd get_all_order(int v, ValState val_state) const {
        if (val_state == ValState::INIT) {
            return get_init_all_order(v);
        }
        if (val_state == ValState::TARGET) {
            return get_target_order(v);
        }
        my_assert(false);
        return AllOrd(0);
    }
};

#endif //UNTITLED_AORDER_H
