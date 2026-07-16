//
// Created by Auto
//

#include "BOrder.h"
#include "../../../util.h"
#include "../../YakinamashiQuery/State/PrevA2ARotSum.h"

#ifdef DEBUG
namespace {
    // _BEGIN: Log A2A range replay decisions in BOrder so grouped B rot assignment can be inspected later.
    void debug_log_a2a_range_with_ab_begin(int a2a_start, int ab_idx, const BeamQuery &ab_query, command b_cmd) {
        out("=== BORDER_A2A_RANGE_WITH_AB_BEGIN ===");
        out("a2a_start", a2a_start, "ab_idx", ab_idx, "b_cmd", b_cmd);
        out("ab_query", ab_query);
    }

    void debug_log_a2a_group_assignment(int q_idx,
                                        e_a2a_type a2a_type,
                                        int current_group_idx,
                                        int can_sync,
                                        int used_b_rot,
                                        int rem_b_rot_after,
                                        int grouped_b_rot_now) {
        out("border_group_assign",
            "q_idx", q_idx,
            "a2a_type", get_a2a_type_str(a2a_type),
            "group_idx", current_group_idx,
            "can_sync", can_sync,
            "used_b_rot", used_b_rot,
            "rem_b_rot_after", rem_b_rot_after,
            "grouped_b_rot_now", grouped_b_rot_now);
    }

    void debug_log_a2a_replay_step(int q_idx,
                                   e_a2a_type a2a_type,
                                   int grouped_b_rot,
                                   const BeamQuery &query) {
        out("border_replay_step",
            "q_idx", q_idx,
            "a2a_type", get_a2a_type_str(a2a_type),
            "grouped_b_rot", grouped_b_rot,
            "query", query);
    }

    void debug_log_a2a_range_with_ab_end(int ab_idx, int b_rot_cnt, const BeamQuery &ab_query) {
        out("border_replay_step",
            "q_idx", ab_idx,
            "kind", "ab_tail",
            "b_rot_cnt", b_rot_cnt,
            "query", ab_query);
        out("=== BORDER_A2A_RANGE_WITH_AB_END ===");
    }
    // _END

    // : Add duplicate-target debug helpers so the first corrupting BOrder mutation is visible before rebuild asserts.
    bool try_find_duplicate_target_entry(
        const my_vector<OrderEntry> &entries,
        int n,
        int &dup_value,
        int &first_ord,
        int &second_ord
    ) {
        my_vector<int> seen(n, -1);
        for (int ord = 0; ord < (int)entries.size(); ord++) {
            const OrderEntry &entry = entries[ord];
            if (!is_targeted(entry.val_state)) continue;
            if (seen[entry.value] != -1) {
                dup_value = entry.value;
                first_ord = seen[entry.value];
                second_ord = ord;
                return true;
            }
            seen[entry.value] = ord;
        }
        return false;
    }

    // : Dump the whole BOrder snapshot when duplication appears so the exact corrupt entry order can be reconstructed.
    void debug_log_duplicate_target_entries(
        const char *stage,
        const my_vector<OrderEntry> &entries,
        int n
    ) {
        int dup_value = -1;
        int first_ord = -1;
        int second_ord = -1;
        if (!try_find_duplicate_target_entry(entries, n, dup_value, first_ord, second_ord)) return;
        out("=== BORDER_DUPLICATE_TARGET ===");
        out("stage", stage, "dup_value", dup_value, "first_ord", first_ord, "second_ord", second_ord);
        for (int ord = 0; ord < (int)entries.size(); ord++) {
            out("border_dup_entry",
                "ord", ord,
                "value", entries[ord].value,
                "val_state", entries[ord].val_state);
        }
    }
}
#endif

BOrder::BOrder(int init_N_) : OrderBase(init_N_) {
    target_v_orders.assign(init_N, -1);
}

// : Centralize BOrder invariants so every mutation site can stop at the first duplicate or target_v_orders drift.
bool BOrder::has_valid_target_entries(const my_vector<OrderEntry> &entries) const {
    my_vector<int> seen(init_N, -1);
    for (int ord = 0; ord < (int)entries.size(); ord++) {
        const OrderEntry &entry = entries[ord];
        if (!is_targeted(entry.val_state)) return false;
        if (!(0 <= entry.value && entry.value < init_N)) return false;
        if (seen[entry.value] != -1) return false;
        seen[entry.value] = ord;
    }
    return true;
}

// : Check both entry uniqueness and target_v_orders sync from one source of truth before later B-side ord code runs.
bool BOrder::has_valid_state() const {
    if (!has_valid_target_entries(order_entries)) return false;
    if ((int)target_v_orders.size() != init_N) return false;
    my_vector<int> expected_orders(init_N, -1);
    for (int ord = 0; ord < (int)order_entries.size(); ord++) {
        expected_orders[order_entries[ord].value] = ord;
    }
    for (int v = 0; v < init_N; v++) {
        if (target_v_orders[v] != expected_orders[v]) return false;
    }
    return true;
}

void BOrder::update_order(AllOrd prev_all_ord_b, AllOrd new_all_ord_b) {
    int prev_idx = prev_all_ord_b.get();
    int new_idx = new_all_ord_b.get();
    my_assert(0 <= prev_idx && prev_idx < (int)order_entries.size());
    my_assert(0 <= new_idx && new_idx < (int)order_entries.size());
    if (prev_idx == new_idx) return;
    OrderEntry moved_entry = order_entries[prev_idx];
    if (prev_idx < new_idx) {
        for (int i = prev_idx; i < new_idx; i++) order_entries[i] = order_entries[i + 1];
    } else {
        for (int i = prev_idx; i > new_idx; i--) order_entries[i] = order_entries[i - 1];
    }
    order_entries[new_idx] = moved_entry;
    int update_begin = std::min(prev_idx, new_idx);
    int update_end = std::max(prev_idx, new_idx);
    for (int i = update_begin; i <= update_end; i++) {
        int v = order_entries[i].value;
        my_assert(0 <= v && v < init_N);
        target_v_orders[v] = i;
    }
    my_assert(has_valid_state());
}

// 責務: 指定された 2 つの B all_ord entry を交換し、value から all_ord を引く補助配列も同時に同期する。
// 必要な理由: beam の追加 sb/ss は物理 top2 と all_ord 割当を同時に入れ替えるため、order_entries だけの交換では後続 OrdCalc が壊れる。
void BOrder::swap_all_ord_entries(AllOrd left_all_ord, AllOrd right_all_ord) {
    const int left = left_all_ord.get();
    const int right = right_all_ord.get();
    my_assert(0 <= left && left < static_cast<int>(order_entries.size()));
    my_assert(0 <= right && right < static_cast<int>(order_entries.size()));
    if (left == right) return;
    std::swap(order_entries[left], order_entries[right]);
    const OrderEntry &left_entry = order_entries[left];
    const OrderEntry &right_entry = order_entries[right];
    my_assert(left_entry.val_state == ValState::TARGET);
    my_assert(right_entry.val_state == ValState::TARGET);
    my_assert(0 <= left_entry.value && left_entry.value < init_N);
    my_assert(0 <= right_entry.value && right_entry.value < init_N);
    target_v_orders[left_entry.value] = left;
    target_v_orders[right_entry.value] = right;
    my_assert(get_target_order(left_entry.value).get() == left);
    my_assert(get_target_order(right_entry.value).get() == right);
    my_assert(has_valid_state());
}

//登録できたall_ordを返す
//指定したordの直前に挿入する
AllOrd BOrder::update_insert_ord_v(int value, AllOrd new_all_ord_b) {
    my_assert(0 <= value && value < init_N);
    int prev_ord = target_v_orders[value];
    if (prev_ord != -1) {
        int target = new_all_ord_b.get();
        if (prev_ord < target) target--;
        update_order(AllOrd(prev_ord), AllOrd(target));
        return AllOrd(target);
    }
    my_assert(new_all_ord_b.get() <= (int)order_entries.size());
    insert_entry_at_position(new_all_ord_b.get(), OrderEntry(value, ValState::TARGET), order_entries);
    my_assert(has_valid_target_entries(order_entries));
    // _BEGIN: Probe the construct-time insert path immediately so duplicate TARGET values are reported at their first mutation site.
#ifdef DEBUG
    debug_log_duplicate_target_entries("update_insert_ord_v.insert", order_entries, init_N);
    out("border_dup_context", "value", value, "new_all_ord_b", new_all_ord_b.get());
#endif
    // _END
    update_orders_from_entries(order_entries);
    return new_all_ord_b;
}

void BOrder::update_orders_from_entries(const my_vector<OrderEntry> &order_entries) {
    // _BEGIN: Emit the rebuild input once more at assert time so late callers still reveal the duplicate snapshot.
#ifdef DEBUG
    debug_log_duplicate_target_entries("update_orders_from_entries.begin", order_entries, init_N);
#endif
    // _END
    target_v_orders.assign(init_N, -1);
    for (int i = 0; i < (int) order_entries.size(); i++) {
        if (is_targeted(order_entries[i].val_state)) {
            auto &target = target_v_orders[order_entries[i].value];
            // _BEGIN: Keep the conflicting ord pair visible right before the uniqueness assert fires.
#ifdef DEBUG
            if (target != -1) {
                out("border_dup_assert_context",
                    "value", order_entries[i].value,
                    "first_ord", target,
                    "second_ord", i);
            }
#endif
            // _END
            my_assert(target == -1);
            target = i;
        } else {
            my_assert(false);
        }
    }
    my_assert(has_valid_state());
}

void BOrder::build_orders_from_queries(const State &init_state,
                                       const my_vector<BeamQuery> &queries) {
    my_vector<OrderEntry> order_entries;
    my_vector<OrderEntry> previous_entries;
    my_vector<ValState> val_state_in_b(init_N, ValState::NONE);
    State current_state = init_state;
    for (int i = 0; i < (int) queries.size(); i++) {
        const BeamQuery &query = queries[i];
        my_assert(query.is_valid_query());
        apply_query_to_state(current_state, query, queries, i);
        if (query.get_push_type() == e_push_type::a2b) {
            process_insert_query(query, current_state, current_state.B, val_state_in_b, order_entries);
            my_assert(has_valid_target_entries(order_entries));
            // _BEGIN: Check the simple A2B replay path so build_from_queries can report the first duplicate-producing query.
#ifdef DEBUG
            debug_log_duplicate_target_entries("build_orders_from_queries.a2b", order_entries, init_N);
            out("border_dup_context", "q_idx", i, "query", query);
#endif
            // _END
            val_state_in_b[query.get_tar_val()] = ValState::TARGET;
        } else if (current_state.B.size() == 0 && order_entries.size() > 0) {
            previous_entries.insert(previous_entries.begin(), order_entries.begin(), order_entries.end());
            my_assert(has_valid_target_entries(previous_entries));
            order_entries.clear();
        }
    }
    previous_entries.insert(previous_entries.begin(), order_entries.begin(), order_entries.end());
    my_assert(has_valid_target_entries(previous_entries));
    this->order_entries = previous_entries;
    update_orders_from_entries(this->order_entries);
}

BOrder BOrder::build_from_queries(const State &init_state,
                                  const my_vector<BeamQuery> &queries) {
    BOrder border(init_state.initial_N);
    border.build_orders_from_queries(init_state, queries);
    return border;
}

void BOrder::validate_ring_ord(const State &state,
                               const my_vector<ValState> &now_val_state){
    my_vector<int> b_ords;
    int min_ord = numeric_limits<int>::max();
    int min_pos = -1;
    for (int i = 0; i < state.B.size(); i++) {
        ValState val_state = now_val_state[state.B[i]];
        int ord = get_all_order(state.B[i], val_state).get();
        b_ords.push_back(ord);
        if (chmi(min_ord, ord)) {
            min_pos = i;
        }
    }
    int prev_v = -1;
    for (int loop_i = 0; loop_i < state.B.size(); loop_i++) {
        int i = mod(loop_i + min_pos, state.B.size());
        int ord = b_ords[i];
        if(prev_v >= ord){
            my_assert(false);
        }
        prev_v = ord;
    }
}

void BOrder::validate_ord(const my_vector<command> &sep_cmds,
                          const State &init_state,
                          const my_vector<BeamQuery> &beam_queries){
    //なぜかa2apの挿入を子の中でもやっていて壊れているので一旦消す
//#ifndef DEBUG
//    return;
//#endif
//    State state(init_state);
//    my_assert(state.B.size()==0);
//    my_vector<ValState> now_val_state(init_state.initial_N, ValState::NONE);
//    my_vector<int> grouped_b_rot_buf(beam_queries.size(), 0);
//    validate_ring_ord(state, now_val_state);
//    for (auto &cmd: sep_cmds) {
//        my_assert(cmd != command::PA);
//        state.do_cmd(cmd);
//        if(cmd==command::PB){
//            now_val_state[state.B[0]] = ValState::TARGET;
//        }
//        validate_ring_ord(state, now_val_state);
//    }
//    // _BEGIN: validator も build と同じ A2A-range replay を通し、A2AP 用 BOrder 経路を検証対象に含める。
//    int i = 0;
//    while (i < (int)beam_queries.size()) {
//        const BeamQuery &query = beam_queries[i];
//        my_assert(query.is_valid_query());
//        if (query.is_a2a()) {
//            int ab_idx = find_next_ab_idx(beam_queries, i);
//            if (ab_idx == -1) {
//                process_a2a_range_with_final_rot(
//                    beam_queries, i, state, now_val_state
//                );
//                validate_ring_ord(state, now_val_state);
//                break;
//            }
//            process_a2a_range_with_ab(
//                beam_queries, i, ab_idx, state, now_val_state, grouped_b_rot_buf
//            );
//            validate_ring_ord(state, now_val_state);
//            i = ab_idx + 1;
//            continue;
//        }
//        apply_query_to_state(state, query, beam_queries, i);
//        if (query.get_push_type() == e_push_type::a2b) {
//            now_val_state[query.get_tar_val()] = ValState::TARGET;
//        }
//        validate_ring_ord(state, now_val_state);
//        i++;
//    }
//    // _END
}

void BOrder::process_inserted_btop(State& state, const my_vector<ValState>& val_state_map){
    my_assert(!val_state_map.empty());
    auto& stack = state.B;
    int stack_size = stack.size();
    int new_position;
    if (stack_size == 1 || stack_size == 2) {
        new_position = 0;
    } else {
        int upper_v = stack[mod(-1, stack_size)];
        int lower_v = stack[1];
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
    insert_entry_at_position(new_position, OrderEntry(stack[0], ValState::TARGET), order_entries);
    my_assert(has_valid_target_entries(order_entries));
    // _BEGIN: Check sep-cmd PB insertion immediately because duplicates here would predate every beam replay path.
#ifdef DEBUG
    debug_log_duplicate_target_entries("process_inserted_btop", order_entries, init_N);
    out("border_dup_context", "inserted_v", stack[0], "new_position", new_position);
#endif
    // _END
}

void BOrder::initialize_order_entries(const State &init_state) {
    order_entries.clear();
    order_entries.reserve(init_N);
    my_assert(init_state.B.size()==0);
    my_assert(has_valid_state());
}

e_a2a_type BOrder::resolve_exec_a2a_type(
    const my_vector<BeamQuery> &beam_queries,
    int query_idx
) {
    my_assert(0 <= query_idx && query_idx < (int)beam_queries.size());
    const BeamQuery &query = beam_queries[query_idx];
    my_assert(query.is_valid_query());
    my_assert(query.is_a2a());
    if (query_idx + 1 < (int)beam_queries.size()) {
        const BeamQuery &next_query = beam_queries[query_idx + 1];
        my_assert(next_query.is_valid_query());
        return next_query.get_determined_prev_a2a_type();
    }
    return query.get_cur_default_a2a_type();
}

int BOrder::find_next_ab_idx(
    const my_vector<BeamQuery> &beam_queries,
    int a2a_start
) {
    my_assert(0 <= a2a_start && a2a_start < (int)beam_queries.size());
    my_assert(beam_queries[a2a_start].is_valid_query());
    my_assert(beam_queries[a2a_start].is_a2a());
    for (int i = a2a_start + 1; i < (int)beam_queries.size(); i++) {
        if (!beam_queries[i].is_a2a()) {
            return i;
        }
    }
    return -1;
}

void BOrder::apply_cmd_n(State &state, command cmd, int cnt) {
    for (int i = 0; i < cnt; i++) {
        if (cmd == RA) state.ra();
        else if (cmd == RRA) state.rra();
        else if (cmd == RB) state.rb();
        else if (cmd == RRB) state.rrb();
        else my_assert(false);
    }
}

void BOrder::process_a2ap_with_grouped_b_rot(const BeamQuery &query,
                                             e_a2a_type a2a_type,
                                             int grouped_b_rot,
                                             command b_cmd,
                                             State &current_state,
                                             my_vector<ValState> &val_state_in_b) {
    my_assert(a2a_type == e_a2a_type::push_rot);
    auto [cmd1, cnt1] = query.get_cmd1_cnt1_(a2a_type, query.get_a_size());
    apply_cmd_n(current_state, cmd1, cnt1);
    apply_cmd_n(current_state, b_cmd, grouped_b_rot);
    current_state.pb();
    process_insert_query(query, current_state, current_state.B, val_state_in_b, order_entries);
    my_assert(has_valid_target_entries(order_entries));
    // _BEGIN: Probe push_rot replay right after PB because this path is the current prime suspect for duplicate BOrder entries.
#ifdef DEBUG
    debug_log_duplicate_target_entries("process_a2ap_with_grouped_b_rot.pb", order_entries, init_N);
    out("border_dup_context", "grouped_b_rot", grouped_b_rot, "query", query);
#endif
    // _END
    val_state_in_b[query.get_tar_val()] = ValState::TARGET;
    int flip_dis_dir = query.get_flip_dis_dir();
    apply_cmd_n(current_state, flip_dis_dir > 0 ? RA : RRA, abs(flip_dis_dir));
    current_state.pa();
}

void BOrder::apply_ab_query_with_custom_b_rot(const BeamQuery &query,
                                              int b_count,
                                              State &current_state) {
    apply_cmd_n(current_state, query.get_a_cmd(), query.get_a_count());
    apply_cmd_n(current_state, query.get_b_cmd(), b_count);
    if (query.get_push_type() == e_push_type::a2b) current_state.pb();
    else {
        my_assert(query.get_push_type() == e_push_type::b2a);
        current_state.pa();
    }
}

void BOrder::process_a2a_range_with_ab(
    const my_vector<BeamQuery> &beam_queries,
    int a2a_start,
    int ab_idx,
    State &current_state,
    my_vector<ValState> &val_state_in_b,
    my_vector<int> &grouped_b_rot_buf
) {
    my_assert(0 <= a2a_start && a2a_start < ab_idx);
    my_assert(ab_idx < (int)beam_queries.size());
    my_assert(beam_queries[ab_idx].is_valid_query());
    my_assert(beam_queries[ab_idx].is_a2b_b2a());
    const BeamQuery &ab_query = beam_queries[ab_idx];
    command b_cmd = ab_query.get_b_cmd();
    my_assert(b_cmd == RB || b_cmd == RRB);
    int rem_b_rot = ab_query.get_b_count();
    int current_group_idx = ab_idx;
#ifdef DEBUG
    // _BEGIN: Emit grouped B rot assignment inputs before replay so one failing range can be reconstructed from output.
    debug_log_a2a_range_with_ab_begin(a2a_start, ab_idx, ab_query, b_cmd);
    // _END
#endif
    my_assert((int)grouped_b_rot_buf.size() == (int)beam_queries.size());
    for (int i = ab_idx - 1; i >= a2a_start; i--) {
        const BeamQuery &query = beam_queries[i];
        my_assert(query.is_valid_query());
        my_assert(query.is_a2a());
        e_a2a_type a2a_type = resolve_exec_a2a_type(beam_queries, i);
        if (a2a_type == e_a2a_type::push_rot) {
            current_group_idx = i;
        }
        auto [cmd1, cnt1] = query.get_cmd1_cnt1_(a2a_type, query.get_a_size());
        PrevA2ARotSum sync_sum = PrevA2ARotSumCalc::calc_a2a_sync_rot_sum(
            cmd1, cnt1, query.get_flip_dis_dir(), a2a_type
        );
        int can_sync = (b_cmd == RB) ? sync_sum.ra_sum : sync_sum.rra_sum;
        int used_b_rot = std::min(rem_b_rot, can_sync);
        grouped_b_rot_buf[current_group_idx] += used_b_rot;
        rem_b_rot -= used_b_rot;
#ifdef DEBUG
        // _BEGIN: Record backward grouping decisions to see which A2A absorbed each B rotation.
        debug_log_a2a_group_assignment(
            i, a2a_type, current_group_idx, can_sync, used_b_rot, rem_b_rot, grouped_b_rot_buf[current_group_idx]
        );
        // _END
#endif
    }
    // _BEGIN: Keep the unsynced tail B rotations on the AB query so grouped replay preserves the original total B count.
    grouped_b_rot_buf[ab_idx] += rem_b_rot;
    // _END
    for (int i = a2a_start; i < ab_idx; i++) {
        const BeamQuery &query = beam_queries[i];
        e_a2a_type a2a_type = resolve_exec_a2a_type(beam_queries, i);
#ifdef DEBUG
        // _BEGIN: Record forward replay inputs so the executed A2A range can be inspected after a failure.
        debug_log_a2a_replay_step(i, a2a_type, grouped_b_rot_buf[i], query);
        // _END
#endif
        if (a2a_type == e_a2a_type::swap_rot) {
            apply_query_to_state(current_state, query, beam_queries, i);
        } else {
            process_a2ap_with_grouped_b_rot(
                query, a2a_type, grouped_b_rot_buf[i], b_cmd, current_state, val_state_in_b
            );
            grouped_b_rot_buf[i] = 0;
        }
    }
#ifdef DEBUG
    // _BEGIN: Log the tail AB replay with the final remaining grouped B rotation count.
    debug_log_a2a_range_with_ab_end(ab_idx, grouped_b_rot_buf[ab_idx], ab_query);
    // _END
#endif
    apply_ab_query_with_custom_b_rot(ab_query, grouped_b_rot_buf[ab_idx], current_state);
    if (ab_query.get_push_type() == e_push_type::a2b) {
        process_insert_query(ab_query, current_state, current_state.B, val_state_in_b, order_entries);
        my_assert(has_valid_target_entries(order_entries));
        // _BEGIN: Probe the AB-tail A2B insertion so grouped-range replay can be distinguished from the push_rot insertion path.
#ifdef DEBUG
        debug_log_duplicate_target_entries("process_a2a_range_with_ab.ab_tail_a2b", order_entries, init_N);
        out("border_dup_context", "ab_idx", ab_idx, "b_rot_cnt", grouped_b_rot_buf[ab_idx], "query", ab_query);
#endif
        // _END
        val_state_in_b[ab_query.get_tar_val()] = ValState::TARGET;
    }
    grouped_b_rot_buf[ab_idx] = 0;
}

void BOrder::process_a2a_range_with_final_rot(
    const my_vector<BeamQuery> &beam_queries,
    int a2a_start,
    State &current_state,
    my_vector<ValState> &val_state_in_b
) {
    my_assert(0 <= a2a_start && a2a_start < (int)beam_queries.size());
    for (int i = a2a_start; i < (int)beam_queries.size(); i++) {
        const BeamQuery &query = beam_queries[i];
        my_assert(query.is_valid_query());
        my_assert(query.is_a2a());
        e_a2a_type a2a_type = resolve_exec_a2a_type(beam_queries, i);
        if (a2a_type == e_a2a_type::swap_rot) {
            apply_query_to_state(current_state, query, beam_queries, i);
        } else {
            process_a2ap_with_grouped_b_rot(
                query, a2a_type, 0, RB, current_state, val_state_in_b
            );
        }
    }
}

BOrder BOrder::build(const my_vector<command>& sep_cmds,
                     const State& init_state,
                     const my_vector<BeamQuery>& beam_queries) {
    my_assert(init_state.initial_N > 0);
    BOrder border(init_state.initial_N);
    border.initialize_order_entries(init_state);
    my_vector<ValState> val_state_in_b(init_state.initial_N, ValState::NONE);
    my_vector<int> grouped_b_rot_buf(beam_queries.size(), 0);
    State current_state = init_state;
    for(auto& cmd : sep_cmds){
        my_assert(cmd != command::PA && cmd != command::NO_COMMAND);
        current_state.do_cmd(cmd);
        if(cmd == command::PB){
            border.process_inserted_btop(current_state, val_state_in_b);
            val_state_in_b[current_state.B.front()] = ValState::TARGET;
        }
    }
    int i = 0;
    while (i < (int)beam_queries.size()) {
        const BeamQuery &query = beam_queries[i];
        my_assert(query.is_valid_query());
        if (query.is_a2a()) {
            int ab_idx = border.find_next_ab_idx(beam_queries, i);
            if (ab_idx == -1) {
                border.process_a2a_range_with_final_rot(
                    beam_queries, i, current_state, val_state_in_b
                );
                break;
            }
            border.process_a2a_range_with_ab(
                beam_queries, i, ab_idx, current_state, val_state_in_b, grouped_b_rot_buf
            );
            i = ab_idx + 1;
            continue;
        }
        apply_query_to_state(current_state, query, beam_queries, i);
        if (query.get_push_type() == e_push_type::a2b) {
            border.process_insert_query(query, current_state, current_state.B, val_state_in_b, border.order_entries);
            my_assert(border.has_valid_target_entries(border.order_entries));
            // _BEGIN: Check the direct beam A2B path so BOrder::build identifies the first duplicate-producing query in normal replay too.
#ifdef DEBUG
            debug_log_duplicate_target_entries("build.a2b", border.order_entries, border.init_N);
            out("border_dup_context", "q_idx", i, "query", query);
#endif
            // _END
            val_state_in_b[query.get_tar_val()] = ValState::TARGET;
        }
        i++;
    }
    border.update_orders_from_entries(border.order_entries);
//    my_assert(border.has_valid_state());
//    border.validate_ord(sep_cmds, init_state, beam_queries);
    my_assert(border.has_valid_state());
    return border;
}

void BOrder::erase_values(const my_vector<int>& values) {
    erase_entries_by_values(values);
    update_orders_from_entries(order_entries);
}

