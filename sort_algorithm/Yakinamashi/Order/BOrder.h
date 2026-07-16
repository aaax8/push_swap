//
// Created by Auto
//

#ifndef UNTITLED_BORDER_H
#define UNTITLED_BORDER_H

#include "../../../all_include.h"
#include "../../../State.h"
#include "ValState.h"
#include "OrdTypes.h"
#include "../../InitBeam/BeamQuery.h"
#include "OrderBase.h"

class BOrder : public OrderBase {
private:
    my_vector<int> target_v_orders;

    bool has_valid_target_entries(const my_vector<OrderEntry>& entries) const;
    bool has_valid_state() const;
    void update_orders_from_entries(const my_vector<OrderEntry>& order_entries);

public:
    BOrder(int init_N_);
    
    static BOrder build_from_queries(const State& init_state,
                                     const my_vector<BeamQuery>& queries);

    void build_orders_from_queries(const State& init_state,
                                   const my_vector<BeamQuery>& queries);

    AllOrd get_target_order(int value) const {
        my_assert(0 <= value && value < init_N);
        int ord = target_v_orders[value];
        my_assert(ord != -1);
        return AllOrd(ord);
    }

    void update_order(AllOrd prev_all_ord_b, AllOrd new_all_ord_b);
    void swap_all_ord_entries(AllOrd left_all_ord, AllOrd right_all_ord);
    AllOrd update_insert_ord_v(int value, AllOrd new_all_ord_b);
    void process_inserted_btop(State& state, const my_vector<ValState>& val_state_map);
    static BOrder build(const my_vector<command>& sep_cmds,
                        const State& init_state,
                        const my_vector<BeamQuery>& beam_queries);

    void erase_values(const my_vector<int>& values);
    
    void validate_ring_ord(const State &state,
                           const my_vector<ValState> &now_val_state);

    void validate_ord(const my_vector<command> &sep_cmds,
                      const State &init_state,
                      const my_vector<BeamQuery> &beam_queries);

    void initialize_order_entries(const State &init_state);
    static e_a2a_type resolve_exec_a2a_type(
        const my_vector<BeamQuery> &beam_queries,
        int query_idx
    );
    int find_next_ab_idx(
        const my_vector<BeamQuery> &beam_queries,
        int a2a_start
    );
    static void apply_cmd_n(State &state, command cmd, int cnt);
    void process_a2ap_with_grouped_b_rot(const BeamQuery &query,
                                         e_a2a_type a2a_type,
                                         int grouped_b_rot,
                                         command b_cmd,
                                         State &current_state,
                                         my_vector<ValState> &val_state_in_b);
    static void apply_ab_query_with_custom_b_rot(const BeamQuery &query,
                                                 int b_count,
                                                 State &current_state);
    void process_a2a_range_with_ab(
        const my_vector<BeamQuery> &beam_queries,
        int a2a_start,
        int ab_idx,
        State &current_state,
        my_vector<ValState> &val_state_in_b,
        my_vector<int> &grouped_b_rot_buf
    );
    void process_a2a_range_with_final_rot(
        const my_vector<BeamQuery> &beam_queries,
        int a2a_start,
        State &current_state,
        my_vector<ValState> &val_state_in_b
    );

    AllOrd get_all_order(int v, ValState val_state) const {
        (void)v;
        if (val_state == ValState::TARGET) {
            return get_target_order(v);
        }
        my_assert(false);
        return AllOrd(0);
    }
};

#endif //UNTITLED_BORDER_H
