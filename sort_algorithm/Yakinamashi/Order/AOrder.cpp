//
// Created by Auto
//

#include "AOrder.h"
#include "../../../util.h"
#include "ValState.h"

// 責務: AOrder の entry 列を index:value:state の形で debug 出力する。
static void debug_log_aorder_entries(const char *tag,
                                     int break_idx,
                                     int prev_target_v,
                                     int current_target_v,
                                     const my_vector<OrderEntry> &entries) {
#ifdef DEBUG
    out("=== AORDER_TARGET_ENTRY_ORDER_FAILURE ===");
    out("tag", tag,
        "break_idx", break_idx,
        "prev_target_v", prev_target_v,
        "current_target_v", current_target_v,
        "entry_count", static_cast<int>(entries.size()));
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        out("aorder_entry",
            "idx", i,
            "value", entries[i].value,
            "val_state", entries[i].val_state);
    }
#else
    (void)tag;
    (void)break_idx;
    (void)prev_target_v;
    (void)current_target_v;
    (void)entries;
#endif
}

AOrder::AOrder(int init_N_) : OrderBase(init_N_) {
    init_v_orders.assign(init_N, -1);
    targeted_v_orders.assign(init_N, -1);
}

void AOrder::initialize_order_entries(const State &init_state) {
    order_entries.clear();
    order_entries.reserve(init_N);
    for (int i = 0; i < init_state.A.size(); i++) {
        order_entries.emplace_back(init_state.A[i], ValState::INIT);
    }
}

// 責務: 現在存在する最小 value の TARGET entry、なければ INIT entry を先頭へ回す。
void AOrder::normalize_min_front() {
    if (order_entries.empty()) return;
    int base_v = order_entries[0].value;
    for (const OrderEntry &entry: order_entries) {
        chmi(base_v, entry.value);
    }
    int base_init_idx = -1;
    int base_target_idx = -1;
    for (int i = 0; i < (int) order_entries.size(); i++) {
        if (order_entries[i].value != base_v) continue;
        if (order_entries[i].val_state == ValState::INIT) base_init_idx = i;
        if (order_entries[i].val_state == ValState::TARGET) base_target_idx = i;
    }
    int start_idx = base_target_idx != -1 ? base_target_idx : base_init_idx;
    if (start_idx == -1) return;
    my_assert(0 <= start_idx && start_idx < (int) order_entries.size());
    if (start_idx != 0) {
        std::rotate(order_entries.begin(), order_entries.begin() + start_idx, order_entries.end());
    }
    update_orders_from_entries();
    validate_tar_sorted();
    validate_target_entry_order();
}

// 責務: TARGET entry の最小 value を先頭へ rotate し、order index を更新する。
// 必要な理由: new max 挿入時は target min を先頭にして、max の後ろ側 slot を広く扱える状態にするため。
void AOrder::normalize_target_min_front() {
    if (order_entries.empty()) return;
    int target_idx = -1;
    int target_v = numeric_limits<int>::max();
    for (int i = 0; i < (int) order_entries.size(); i++) {
        const OrderEntry &entry = order_entries[i];
        if (entry.val_state != ValState::TARGET) continue;
        if (chmi(target_v, entry.value)) target_idx = i;
    }
    if (target_idx == -1) {
        normalize_min_front();
        return;
    }
    if (target_idx != 0) {
        std::rotate(order_entries.begin(), order_entries.begin() + target_idx, order_entries.end());
    }
    update_orders_from_entries();
    validate_target_entry_order();
}

// 責務: TARGET entry の最大 value が末尾になるように rotate し、order index を更新する。
// 必要な理由: new min 挿入時は target min の前にできるだけ多くの INIT slot を残せる状態にするため。
void AOrder::normalize_target_max_back() {
    if (order_entries.empty()) return;
    int target_idx = -1;
    int target_v = numeric_limits<int>::min();
    for (int i = 0; i < (int) order_entries.size(); i++) {
        const OrderEntry &entry = order_entries[i];
        if (entry.val_state != ValState::TARGET) continue;
        if (chma(target_v, entry.value)) target_idx = i;
    }
    if (target_idx == -1) {
        normalize_min_front();
        return;
    }
    const int start_idx = target_idx + 1;
    if (start_idx < (int) order_entries.size()) {
        std::rotate(order_entries.begin(), order_entries.begin() + start_idx, order_entries.end());
    }
    update_orders_from_entries();
    validate_target_entry_order();
}

// 責務: value が TARGET min なら max-back、TARGET max なら min-front、それ以外なら既存 min-front normalize を実行する。
// 必要な理由: physical destroyed 側の shift で、new min/new max の可動範囲をそれぞれ広く取るため。
void AOrder::normalize_insert_shift(int value) {
    int min_target_v = numeric_limits<int>::max();
    int max_target_v = numeric_limits<int>::min();
    bool has_target = false;
    bool has_value_target = false;
    for (const OrderEntry &entry: order_entries) {
        if (entry.val_state != ValState::TARGET) continue;
        has_target = true;
        if (entry.value == value) has_value_target = true;
        chmi(min_target_v, entry.value);
        chma(max_target_v, entry.value);
    }
    if (!has_target) {
        normalize_min_front();
        return;
    }
    my_assert(has_value_target);
    if (value == min_target_v) {
        normalize_target_max_back();
        return;
    }
    if (value == max_target_v) {
        normalize_target_min_front();
        return;
    }
    normalize_min_front();
}

// 責務: 現存する最小 value の TARGET entry、なければ INIT entry が先頭にあることを検証する。
void AOrder::validate_tar_sorted() const {
    #ifndef DEBUG 
    return;
    #endif
    if (order_entries.empty()) return;
    int base_v = order_entries[0].value;
    for (const OrderEntry &entry: order_entries) {
        chmi(base_v, entry.value);
    }
    int base_init_idx = -1;
    int base_target_idx = -1;
    for (int i = 0; i < (int) order_entries.size(); i++) {
        if (order_entries[i].value != base_v) continue;
        if (order_entries[i].val_state == ValState::INIT) base_init_idx = i;
        if (order_entries[i].val_state == ValState::TARGET) base_target_idx = i;
    }
    if (base_target_idx != -1) {
        my_assert(base_target_idx == 0);
        return;
    }
    if (base_init_idx != -1) {
        my_assert(base_init_idx == 0);
    }
}

// 責務: TARGET entry だけを entry 出現順に見て value が厳密昇順であることを検証する。
// 必要な理由: TARGET min value が TARGET 内の最前、max value が TARGET 内の最後という前提崩れを早期検出するため。
void AOrder::validate_target_entry_order() const {
#ifndef DEBUG
    return;
#endif
    int prev_target_v = -1;
    int target_count = 0;
    for (int i = 0; i < static_cast<int>(order_entries.size()); i++) {
        const OrderEntry &entry = order_entries[i];
        if (entry.val_state != ValState::TARGET) continue;
        target_count++;
        if (prev_target_v >= entry.value) {
            debug_log_aorder_entries("target_order_break", i, prev_target_v, entry.value, order_entries);
            my_assert(false);
        }
        prev_target_v = entry.value;
    }
    (void)target_count;
}

void AOrder::append_lis_target_entries_after_init(const my_bitset<MAX_BUFF> &is_lis) {
    my_vector<OrderEntry> new_entries;
    new_entries.reserve(order_entries.size() + init_N);
#ifdef DEBUG
    my_bitset<MAX_BUFF> appended_lis_target;
#endif
    for (const OrderEntry &entry: order_entries) {
        new_entries.push_back(entry);
        int v = entry.value;
        my_assert(0 <= v && v < init_N);
        if (!is_lis[v]) continue;
        my_assert(!appended_lis_target[v]);
        new_entries.emplace_back(v, ValState::TARGET);
#ifdef DEBUG
        appended_lis_target.set(v);
#endif
    }
#ifdef DEBUG
    for (int v = 0; v < init_N; v++) {
        if (is_lis[v]) {
            my_assert(appended_lis_target[v]);
        }
    }
#endif
    order_entries.swap(new_entries);
}

void AOrder::update_orders_from_entries() {
    init_v_orders.assign(init_N, -1);
    targeted_v_orders.assign(init_N, -1);
    for (int i = 0; i < (int) order_entries.size(); i++) {
        my_assert(order_entries[i].val_state != ValState::NONE);
        int v = order_entries[i].value;
        if (is_init(order_entries[i].val_state)) {
            auto &target = init_v_orders[v];
            my_assert(target == -1);
            target = i;
        }
        if (is_targeted(order_entries[i].val_state)) {
            auto &target = targeted_v_orders[v];
            my_assert(target == -1);
            target = i;
        }
    }
}

void AOrder::build_orders_from_queries(const State &init_state,
                                       const my_vector<BeamQuery> &queries,
                                       const my_bitset<MAX_BUFF> &is_lis) {
    initialize_order_entries(init_state);
    my_vector<ValState> val_state_in_a = initial_value_type(init_state);
    State current_state = init_state;
    for (int i = 0; i < (int) queries.size(); i++) {
        const BeamQuery &query = queries[i];
        my_assert(query.is_valid_query());
        apply_query_to_state(current_state, query, queries, i);
        my_assert(current_state.A.size() != 0);
        if (query.is_a2a() || query.get_push_type() == e_push_type::b2a) {
            process_insert_query(query, current_state, current_state.A, val_state_in_a, order_entries);
            val_state_in_a[query.get_tar_val()] = ValState::TARGET;
        }
    }
    append_lis_target_entries_after_init(is_lis);
    normalize_min_front();
}

AOrder AOrder::build_from_queries(const State &init_state,
                                  const my_vector<BeamQuery> &queries,
                                  const my_bitset<MAX_BUFF> &is_lis) {
    AOrder aorder(init_state.initial_N);
    aorder.build_orders_from_queries(init_state, queries, is_lis);
    return aorder;
}

my_vector<ValState> AOrder::initial_value_type(const State &init_state) const {
    my_vector<ValState> val_state_in_a(init_N, ValState::NONE);
    for (int i = 0; i < init_state.A.size(); i++) {
        val_state_in_a[init_state.A[i]] = ValState::INIT;
    }
    return val_state_in_a;
}

void AOrder::apply_separate_cmds(State &current_state,
                                 const my_vector<command> &sep_cmds) {
    for (auto &cmd: sep_cmds) {
        my_assert(cmd != command::PA && cmd != command::NO_COMMAND);
        current_state.do_cmd(cmd);
    }
}

void AOrder::apply_beam_queries(State &current_state, const my_vector<BeamQuery> &beam_queries,
                                my_vector<ValState> &val_state_in_a) {
    for (int i = 0; i < (int) beam_queries.size(); i++) {
        const BeamQuery &query = beam_queries[i];
        my_assert(query.is_valid_query());
        apply_query_to_state(current_state, query, beam_queries, i);
        my_assert(current_state.A.size() != 0);
        if (query.is_a2a() || query.get_push_type() == e_push_type::b2a) {
            process_insert_query(query, current_state, current_state.A, val_state_in_a, order_entries);
            val_state_in_a[query.get_tar_val()] = ValState::TARGET;
        }
    }
}

void AOrder::validate_ring_ord(const State &state,
                               const my_vector<ValState> &now_val_state) {
    my_vector<int> a_ords;
    int min_ord = numeric_limits<int>::max();
    int min_pos = -1;
    for (int i = 0; i < state.A.size(); i++) {
        ValState val_state = now_val_state[state.A[i]];
        int ord = get_all_order(state.A[i], val_state).get();
        a_ords.push_back(ord);
        if (chmi(min_ord, ord)) {
            min_pos = i;
        }
    }
    int prev_v = -1;
    for (int loop_i = 0; loop_i < state.A.size(); loop_i++) {
        int i = mod(loop_i + min_pos, state.A.size());
        int ord = a_ords[i];
        if(prev_v >= ord){
            my_assert(false);
        }
        prev_v = ord;
    }
}

void AOrder::validate_ord(const my_vector<command> &sep_cmds,
                          const State &init_state,
                          const my_vector<BeamQuery> &beam_queries) {
#ifndef DEBUG
    return;
#endif
    State state(init_state);
    auto now_val_state = initial_value_type(init_state);
    validate_ring_ord(state, now_val_state);
    for (auto &cmd: sep_cmds) {
        my_assert(cmd != command::PA);
        state.do_cmd(cmd);
        validate_ring_ord(state, now_val_state);
    }
    state.print();
    for (int i = 0; i < (int) beam_queries.size(); i++) {
        const BeamQuery &query = beam_queries[i];
        my_assert(query.is_valid_query());
        apply_query_to_state(state, query, beam_queries, i);
        my_assert(state.A.size() != 0);
        if (query.is_a2a() || query.get_push_type() == e_push_type::b2a) {
            now_val_state[query.get_tar_val()] = ValState::TARGET;
        }
        validate_ring_ord(state, now_val_state);
        state.print();
    }
}

AOrder AOrder::build(const my_vector<command> &sep_cmds,
                     const State &init_state,
                     const my_vector<BeamQuery> &beam_queries,
                     const my_bitset<MAX_BUFF> &is_lis) {
    my_assert(init_state.initial_N > 0);
    AOrder aorder(init_state.initial_N);
    aorder.initialize_order_entries(init_state);
    auto val_state_in_a = aorder.initial_value_type(init_state);
    State current_state = init_state;
    aorder.apply_separate_cmds(current_state, sep_cmds);
    aorder.apply_beam_queries(current_state, beam_queries, val_state_in_a);
    aorder.append_lis_target_entries_after_init(is_lis);
    aorder.normalize_min_front();
    aorder.validate_ord(sep_cmds, init_state, beam_queries);
    return aorder;
}

void AOrder::erase_values(const my_vector<int>& values) {
    erase_entries_by_values(values);
    update_orders_from_entries();
    validate_target_entry_order();
}

void AOrder::insert_entry(int index, const OrderEntry& entry) {
    insert_entry_at_position(index, entry, order_entries);
    update_orders_from_entries();
}

AllOrd AOrder::insert_update_shift(int value, AllOrd new_all_ord_a) {
    my_assert(0 <= value && value < init_N);
    int target = new_all_ord_a.get();
    my_assert(0 <= target && target <= (int) order_entries.size());
    int prev_idx = -1;
    for (int i = 0; i < (int) order_entries.size(); i++) {
        if (order_entries[i].value == value && order_entries[i].val_state == ValState::TARGET) {
            prev_idx = i;
            break;
        }
    }
    if (prev_idx != -1) {
        OrderEntry moved_entry = order_entries[prev_idx];
        order_entries.erase(order_entries.begin() + prev_idx);
        if (prev_idx < target) target--;
        my_assert(0 <= target && target <= (int) order_entries.size());
        order_entries.insert(order_entries.begin() + target, moved_entry);
    } else {
        insert_entry_at_position(target, OrderEntry(value, ValState::TARGET), order_entries);
    }
    update_orders_from_entries();
    normalize_insert_shift(value);
    validate_target_entry_order();
    return new_all_ord_a;
}

// 責務: TARGET entry を指定 AllOrd 位置へ追加または移動し、normalize_min_front は呼ばずに order index だけ更新する。
// 必要な理由: A2A fast 評価は normalize なし effect 座標で差分を計算するため、採用時の AOrder 更新も同じ座標に揃える必要がある。
AllOrd AOrder::insert_update_no_shift(int value, AllOrd new_all_ord_a) {
    my_assert(0 <= value && value < init_N);
    int target = new_all_ord_a.get();
    my_assert(0 <= target && target <= (int) order_entries.size());
    int prev_idx = -1;
    for (int i = 0; i < (int) order_entries.size(); i++) {
        if (order_entries[i].value == value && order_entries[i].val_state == ValState::TARGET) {
            prev_idx = i;
            break;
        }
    }
    if (prev_idx != -1) {
        OrderEntry moved_entry = order_entries[prev_idx];
        order_entries.erase(order_entries.begin() + prev_idx);
        if (prev_idx < target) target--;
        my_assert(0 <= target && target <= (int) order_entries.size());
        order_entries.insert(order_entries.begin() + target, moved_entry);
    } else {
        insert_entry_at_position(target, OrderEntry(value, ValState::TARGET), order_entries);
    }
    update_orders_from_entries();
    validate_target_entry_order();
    return new_all_ord_a;
}

// 責務: 指定 value の INIT entry を before_all_ord の直前へ移動し、order index を同期する。
// 必要な理由: A2A 途中の物理位置を rebuild の開始位置として扱うため。
void AOrder::rebase_init_ord_stopgap(int value, AllOrd before_all_ord) {
    my_assert(0 <= value && value < init_N);
    int target = before_all_ord.get();
    my_assert(0 <= target && target < static_cast<int>(order_entries.size()));
    int prev_idx = -1;
    for (int i = 0; i < static_cast<int>(order_entries.size()); i++) {
        if (order_entries[i].value == value && order_entries[i].val_state == ValState::INIT) {
            prev_idx = i;
            break;
        }
    }
    my_assert(prev_idx != -1);
    my_assert(prev_idx != target);
    OrderEntry moved_entry = order_entries[prev_idx];
    order_entries.erase(order_entries.begin() + prev_idx);
    if (prev_idx < target) target--;
    my_assert(0 <= target && target <= static_cast<int>(order_entries.size()));
    order_entries.insert(order_entries.begin() + target, moved_entry);
    update_orders_from_entries();
    validate_target_entry_order();
}

// 責務: 指定された 2 つの A all_ord entry を交換し、value/state から all_ord を引く補助配列も同時に同期する。
// 必要な理由: beam の追加 sa/ss は物理 top2 と all_ord 割当を同時に入れ替えるため、order_entries だけの交換では後続 OrdCalc が壊れる。
void AOrder::swap_all_ord_entries(AllOrd left_all_ord, AllOrd right_all_ord) {
    const int left = left_all_ord.get();
    const int right = right_all_ord.get();
    my_assert(0 <= left && left < static_cast<int>(order_entries.size()));
    my_assert(0 <= right && right < static_cast<int>(order_entries.size()));
    if (left == right) return;
    std::swap(order_entries[left], order_entries[right]);
    auto update_one = [&](int idx) {
        const OrderEntry &entry = order_entries[idx];
        my_assert(0 <= entry.value && entry.value < init_N);
        if (entry.val_state == ValState::INIT) {
            init_v_orders[entry.value] = idx;
            return;
        }
        my_assert(entry.val_state == ValState::TARGET);
        targeted_v_orders[entry.value] = idx;
    };
    update_one(left);
    update_one(right);
    my_assert(get_all_order(order_entries[left].value, order_entries[left].val_state).get() == left);
    my_assert(get_all_order(order_entries[right].value, order_entries[right].val_state).get() == right);
    validate_target_entry_order();
}
