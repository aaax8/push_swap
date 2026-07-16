#pragma once

#include "State.h"
#include "State/YakinamashiState.h"
#include "sort_algorithm/Yakinamashi/Order/AOrder.h"
#include "util.h"
#include <algorithm>

// 責務: planned AOrder、対象 v 集合、construct 用 cost をまとめて保持する。
// 必要な理由: query destroy と physical destroy で同じ AOrder 再配置を使い、fixed-A construct の検証基準を共有するため。
template<size_t MAX_N>
struct AOrderRangePlan {
    AOrder planned_aorder;
    my_vector<OrderEntry> planned_entries;
    my_vector<int> target_vs;
    my_bitset<MAX_N> is_destroyed_v;
    my_vector<std::pair<int, int>> destroyed_vs;
    int picked_target_count = 0;

    // 責務: planned_aorder を initial_n で初期化する。
    // 必要な理由: AOrder が default construct 不可なので plan 生成時にサイズを渡すため。
    explicit AOrderRangePlan(int initial_n) : planned_aorder(initial_n) {}
};

// 責務: TARGET v の circular 区間を選び、INIT の相対順序を保ったまま TARGET をランダム再配置する。
// 必要な理由: A target 固定モードの destroy 前整列を query/physical の両方から同じ仕様で使うため。
template<size_t MAX_N, int MIN_V_CNT, int MAX_V_CNT>
class AOrderRangePlanBuilder {
    static_assert(1 <= MIN_V_CNT, "MIN_V_CNT must be positive");
    static_assert(MIN_V_CNT <= MAX_V_CNT, "MIN_V_CNT must be <= MAX_V_CNT");

    const State& current_st;

public:
    // 責務: plan builder が参照する current_st を保持する。
    // 必要な理由: 対象個数制約を current_N に対して検証するため。
    explicit AOrderRangePlanBuilder(const State& current_st) : current_st(current_st) {}

    // 責務: 対象 TARGET v 群と planned AOrder を作成する。
    // 必要な理由: destroy 前に AOrder を整列し、その結果を query/physical 経路へ渡すため。
    AOrderRangePlan<MAX_N> build_plan(const YakinamashiState<MAX_N>& yst) const {
        const int current_value_count = current_st.current_N;
        my_assert(MIN_V_CNT < current_value_count);
        my_assert(MAX_V_CNT < current_value_count);
        AOrderRangePlan<MAX_N> plan(current_st.initial_N);
        plan.picked_target_count = pick_target_count(current_value_count);
        const my_vector<int> target_entry_vs = collect_target_entry_vs(yst.query_order.aorder);
        my_assert(static_cast<int>(target_entry_vs.size()) == current_value_count);
        plan.target_vs = pick_circular_target_vs(target_entry_vs, plan.picked_target_count);
        for (int v: plan.target_vs) {
            my_assert(0 <= v && v < static_cast<int>(MAX_N));
            my_assert(!plan.is_destroyed_v[v]);
            plan.is_destroyed_v.set(v);
        }
        plan.planned_aorder = build_tar_shuffled_aorder(yst.query_order.aorder, plan.target_vs);
        plan.planned_entries = plan.planned_aorder.get_order_entries();
        validate_plan_seed(yst, plan);
        return plan;
    }

    // 責務: arm に設定された対象個数下限を返す。
    // 必要な理由: ALNS ログにテンプレート設定を出すため。
    int get_min_v_cnt() const { return MIN_V_CNT; }
    // 責務: arm に設定された対象個数上限を返す。
    // 必要な理由: ALNS ログにテンプレート設定を出すため。
    int get_max_v_cnt() const { return MAX_V_CNT; }

private:
    // 責務: テンプレート指定範囲から今回の破壊 target 個数を選ぶ。
    // 必要な理由: MIN/MAX を丸めず、設定不正を assert で検出するため。
    int pick_target_count(int current_value_count) const {
        my_assert(MIN_V_CNT < current_value_count);
        my_assert(MAX_V_CNT < current_value_count);
        return MIN_V_CNT + my_rand(MAX_V_CNT - MIN_V_CNT + 1);
    }

    // 責務: AOrder entry に現れる TARGET v を線形順に集める。
    // 必要な理由: circular block 選択の母列を作るため。
    my_vector<int> collect_target_entry_vs(const AOrder& aorder) const {
        my_vector<int> target_entry_vs;
        const my_vector<OrderEntry>& entries = aorder.get_order_entries();
        int prev_v = -1;
        for (const OrderEntry& entry: entries) {
            if (entry.val_state != ValState::TARGET) continue;
            if (prev_v != -1) my_assert(prev_v < entry.value);
            target_entry_vs.push_back(entry.value);
            prev_v = entry.value;
        }
        return target_entry_vs;
    }

    // 責務: TARGET v 列から円環連続な対象 v 群を選ぶ。
    // 必要な理由: AOrder entry 上の circular 区間 destroy を実現するため。
    my_vector<int> pick_circular_target_vs(
            const my_vector<int>& target_entry_vs,
            int picked_target_count
    ) const {
        my_assert(!target_entry_vs.empty());
        my_assert(1 <= picked_target_count && picked_target_count < static_cast<int>(target_entry_vs.size()));
        const int start = my_rand(static_cast<int>(target_entry_vs.size()));
        my_vector<int> target_vs;
        target_vs.reserve(picked_target_count);
        for (int i = 0; i < picked_target_count; i++) {
            target_vs.push_back(target_entry_vs[(start + i) % static_cast<int>(target_entry_vs.size())]);
        }
        return target_vs;
    }

    // 責務: 指定 v の TARGET entry 位置を返す。
    // 必要な理由: 選択 block の開始・終了位置を entry 列上で求めるため。
    int find_target_pos(const my_vector<OrderEntry>& entries, int v) const {
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            if (entries[i].value == v && entries[i].val_state == ValState::TARGET) return i;
        }
        my_assert(false);
        return -1;
    }

    // 責務: start から end 直前までの円環 index 列を作る。
    // 必要な理由: wrap する target block を線形 segment として置き換えるため。
    my_vector<int> build_circular_positions(int entry_count, int start_pos, int end_pos_exclusive) const {
        my_vector<int> positions;
        int pos = start_pos;
        while (true) {
            positions.push_back(pos);
            pos++;
            if (pos == entry_count) pos = 0;
            if (pos == end_pos_exclusive) break;
        }
        return positions;
    }

    // 責務: segment 内で TARGET を置く slot を重複なしでランダム選択する。
    // 必要な理由: INIT の隙間への TARGET 再配置を偏りにくい組み合わせ選択にするため。
    my_vector<int> pick_target_slots(int total_slot_count, int target_count) const {
        my_vector<int> slots(total_slot_count);
        for (int i = 0; i < total_slot_count; i++) slots[i] = i;
        for (int i = total_slot_count - 1; i > 0; i--) {
            const int j = my_rand(i + 1);
            std::swap(slots[i], slots[j]);
        }
        slots.erase(slots.begin() + target_count, slots.end());
        std::sort(slots.begin(), slots.end());
        return slots;
    }

    // 責務: 選択 TARGET block を INIT 相対順序固定のままランダム再配置した entry 列を作る。
    // 必要な理由: planned AOrder の元になる entry 列を、既存 AOrder を壊さずに計算するため。
    my_vector<OrderEntry> build_replaced_entries(
            const my_vector<OrderEntry>& entries,
            const my_vector<int>& target_vs
    ) const {
        my_bitset<MAX_N> is_target_v;
        for (int v: target_vs) is_target_v.set(v);
        const int entry_count = static_cast<int>(entries.size());
        const int start_pos = find_target_pos(entries, target_vs.front());
        const int last_pos = find_target_pos(entries, target_vs.back());
        const int end_pos_exclusive = (last_pos + 1) % entry_count;
        const my_vector<int> circular_positions = build_circular_positions(entry_count, start_pos, end_pos_exclusive);

        my_vector<OrderEntry> init_entries;
        for (int pos: circular_positions) {
            const OrderEntry& entry = entries[pos];
            if (entry.val_state == ValState::TARGET && is_target_v[entry.value]) continue;
            my_assert(entry.val_state == ValState::INIT);
            init_entries.push_back(entry);
        }

        const int target_count = static_cast<int>(target_vs.size());
        const int total_slot_count = static_cast<int>(init_entries.size()) + target_count;
        const my_vector<int> target_slots = pick_target_slots(total_slot_count, target_count);
        my_vector<OrderEntry> new_segment;
        new_segment.reserve(total_slot_count);
        int init_ptr = 0;
        int target_ptr = 0;
        for (int slot = 0; slot < total_slot_count; slot++) {
            if (target_ptr < target_count && target_slots[target_ptr] == slot) {
                new_segment.emplace_back(target_vs[target_ptr], ValState::TARGET);
                target_ptr++;
            } else {
                my_assert(init_ptr < static_cast<int>(init_entries.size()));
                new_segment.push_back(init_entries[init_ptr]);
                init_ptr++;
            }
        }
        my_assert(init_ptr == static_cast<int>(init_entries.size()));
        my_assert(target_ptr == target_count);

        my_vector<OrderEntry> replaced = entries;
        my_assert(circular_positions.size() == new_segment.size());
        for (int i = 0; i < static_cast<int>(circular_positions.size()); i++) {
            replaced[circular_positions[i]] = new_segment[i];
        }
        return normalize_entries_min_front(replaced);
    }

    // 責務: AOrder::normalize_min_front と同じ基準で entry 列を rotate する。
    // 必要な理由: planned entry 列を AOrder の通常形へ揃えて validate しやすくするため。
    my_vector<OrderEntry> normalize_entries_min_front(my_vector<OrderEntry> entries) const {
        if (entries.empty()) return entries;
        int base_v = entries[0].value;
        for (const OrderEntry& entry: entries) chmi(base_v, entry.value);
        int base_init_idx = -1;
        int base_target_idx = -1;
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            if (entries[i].value != base_v) continue;
            if (entries[i].val_state == ValState::INIT) base_init_idx = i;
            if (entries[i].val_state == ValState::TARGET) base_target_idx = i;
        }
        const int start_idx = base_target_idx != -1 ? base_target_idx : base_init_idx;
        my_assert(0 <= start_idx && start_idx < static_cast<int>(entries.size()));
        if (start_idx != 0) {
            std::rotate(entries.begin(), entries.begin() + start_idx, entries.end());
        }
        return entries;
    }

    // 責務: 2 つの entry 列を同じ normalize 規則へ通してから比較する。
    // 必要な理由: planned_entries は index 0 固定ではなく、normalize 後に同じなら同じ plan と扱うため。
    bool entries_equal_normalized(
            my_vector<OrderEntry> lhs,
            my_vector<OrderEntry> rhs
    ) const {
        return normalize_entries_min_front(std::move(lhs)) == normalize_entries_min_front(std::move(rhs));
    }

    // 責務: entry 列から target_entry と完全一致する位置を返す。
    // 必要な理由: value だけではなく ValState まで含めた exact entry を anchor として使うため。
    int find_entry_pos(
            const my_vector<OrderEntry>& entries,
            const OrderEntry& target_entry
    ) const {
        for (int i = 0; i < static_cast<int>(entries.size()); i++) {
            if (entries[i] == target_entry) return i;
        }
        return -1;
    }

    // 責務: plan_pos の entry を現在 AOrder に入れる位置を、plan 上の直前 existing exact entry の直後として返す。
    // 必要な理由: planned_entries の線形 index ではなく、円環上の相対順序に従って entry を挿入するため。
    int find_plan_insert_pos(
            const my_vector<OrderEntry>& plan_entries,
            const my_vector<OrderEntry>& current_entries,
            int plan_pos
    ) const {
        my_assert(!plan_entries.empty());
        my_assert(0 <= plan_pos && plan_pos < static_cast<int>(plan_entries.size()));
        int prev_pos = plan_pos;
        for (int step = 0; step < static_cast<int>(plan_entries.size()) - 1; step++) {
            prev_pos--;
            if (prev_pos < 0) prev_pos = static_cast<int>(plan_entries.size()) - 1;
            const int current_pos = find_entry_pos(current_entries, plan_entries[prev_pos]);
            if (current_pos != -1) return current_pos + 1;
        }
        my_assert(false);
        return 0;
    }

    // 責務: base AOrder から TARGET 再配置済みの planned AOrder を作る。
    // 必要な理由: target shuffle plan 専用処理であることを名前と処理単位から明確にするため。
    AOrder build_tar_shuffled_aorder(
            const AOrder& base_aorder,
            const my_vector<int>& target_vs
    ) const {
        const my_vector<OrderEntry> planned_entries =
                build_replaced_entries(base_aorder.get_order_entries(), target_vs);
        my_bitset<MAX_N> is_target_v;
        for (int v: target_vs) is_target_v.set(v);
        AOrder planned = base_aorder;
        planned.erase_values(target_vs);
        for (int i = 0; i < static_cast<int>(planned_entries.size()); i++) {
            const OrderEntry& entry = planned_entries[i];
            if (!is_target_v[entry.value]) continue;
            const int insert_pos = find_plan_insert_pos(planned_entries, planned.get_order_entries(), i);
            planned.insert_entry(insert_pos, entry);
        }
        planned.normalize_min_front();
        planned.validate_target_entry_order();
        my_assert(entries_equal_normalized(planned.get_order_entries(), planned_entries));
        return planned;
    }

    // 責務: 作成した plan の対象数・重複・entry 順序を DEBUG 時に検証する。
    // 必要な理由: circular block 選択と planned AOrder 作成の仕様違反を早期に検出するため。
    void validate_plan_seed(
            const YakinamashiState<MAX_N>& yst,
            const AOrderRangePlan<MAX_N>& plan
    ) const {
#ifdef DEBUG
        my_assert(plan.picked_target_count == static_cast<int>(plan.target_vs.size()));
        my_assert(MIN_V_CNT <= plan.picked_target_count);
        my_assert(plan.picked_target_count <= MAX_V_CNT);
        my_assert(MAX_V_CNT < current_st.current_N);
        my_assert(entries_equal_normalized(plan.planned_entries, plan.planned_aorder.get_order_entries()));
        my_vector<int> seen(current_st.initial_N, 0);
        for (int v: plan.target_vs) {
            my_assert(0 <= v && v < current_st.initial_N);
            seen[v]++;
            my_assert(seen[v] == 1);
            my_assert(plan.is_destroyed_v[v]);
            my_assert(yst.query_order.aorder.has_target_order(v));
            my_assert(plan.planned_aorder.has_target_order(v));
        }
        int prev_target_v = -1;
        int target_entry_count = 0;
        for (const OrderEntry& entry: plan.planned_entries) {
            if (entry.val_state != ValState::TARGET) continue;
            target_entry_count++;
            if (prev_target_v != -1) my_assert(prev_target_v < entry.value);
            prev_target_v = entry.value;
        }
        my_assert(target_entry_count == current_st.current_N);
#else
        (void) yst;
        (void) plan;
#endif
    }
};

// 責務: 共通 planner を query destroy 用の型として保持する。
// 必要な理由: ALNS の overload 解決で query/physical 経路をフラグなしに分けるため。
template<size_t MAX_N, int MIN_V_CNT, int MAX_V_CNT>
class AOrderRangeQueryDestroyer {
    AOrderRangePlanBuilder<MAX_N, MIN_V_CNT, MAX_V_CNT> planner;

public:
    // 責務: query destroy wrapper が参照する planner を初期化する。
    // 必要な理由: ALNS arm から common planner を使うため。
    explicit AOrderRangeQueryDestroyer(const State& current_st) : planner(current_st) {}

    // 責務: query destroy 用に planned AOrder plan を作る。
    // 必要な理由: 型で query 経路へ流しつつ、plan 作成は common 化するため。
    AOrderRangePlan<MAX_N> build_plan(const YakinamashiState<MAX_N>& yst) const {
        return planner.build_plan(yst);
    }

    // 責務: query destroy arm の対象個数下限を返す。
    // 必要な理由: ログ出力で arm 設定を確認するため。
    int get_min_v_cnt() const { return planner.get_min_v_cnt(); }
    // 責務: query destroy arm の対象個数上限を返す。
    // 必要な理由: ログ出力で arm 設定を確認するため。
    int get_max_v_cnt() const { return planner.get_max_v_cnt(); }
};

// 責務: 共通 planner を physical destroy 用の型として保持する。
// 必要な理由: ALNS の overload 解決で query/physical 経路をフラグなしに分けるため。
template<size_t MAX_N, int MIN_V_CNT, int MAX_V_CNT>
class AOrderRangePhysicalDestroyer {
    AOrderRangePlanBuilder<MAX_N, MIN_V_CNT, MAX_V_CNT> planner;

public:
    // 責務: physical destroy wrapper が参照する planner を初期化する。
    // 必要な理由: ALNS arm から common planner を使うため。
    explicit AOrderRangePhysicalDestroyer(const State& current_st) : planner(current_st) {}

    // 責務: physical destroy 用に planned AOrder plan を作る。
    // 必要な理由: 型で physical 経路へ流しつつ、plan 作成は common 化するため。
    AOrderRangePlan<MAX_N> build_plan(const YakinamashiState<MAX_N>& yst) const {
        return planner.build_plan(yst);
    }

    // 責務: physical destroy arm の対象個数下限を返す。
    // 必要な理由: ログ出力で arm 設定を確認するため。
    int get_min_v_cnt() const { return planner.get_min_v_cnt(); }
    // 責務: physical destroy arm の対象個数上限を返す。
    // 必要な理由: ログ出力で arm 設定を確認するため。
    int get_max_v_cnt() const { return planner.get_max_v_cnt(); }
};
