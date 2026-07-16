#pragma once

// This file is included inside GreedyQueriesConstructor.cpp after Helper has
// declared the fixed-B-all-ord entry points.

// 責務: fixed B all_ord 探索回数ログのファイル名を保持する。
// 必要な理由: 重さ調査ログを通常ログと分けて append するため。
static constexpr const char *FIXED_B_COUNT_LOG_PATH = "fixed_b_constructor_count.txt";

// 責務: construct 呼び出し全体で使う fixed B mode を 50% 確率で選ぶ。
// 必要な理由: v ごとではなく、1 回の construct 内の全 v で同じ再構築経路を使うため。
//性能が変わらなかったのでとりあえずオフ
template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::Helper::pick_fixed_b_mode() {
   return false;
    // return my_rand(2) == 0;
}

// 責務: fixed_b_constructor_count.txt へ 1 行 append する。
// 必要な理由: v ごとの候補数と評価回数を実行後に確認できるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::append_fixed_b_log_line(
        const std::string &line
) {
    std::ofstream log_file(FIXED_B_COUNT_LOG_PATH, std::ios::app);
    if (!log_file.is_open()) return;
    log_file << line << '\n';
}

// 責務: v、fixed B all_ord、qcnt、候補数、重い処理の呼び出し回数、best diff を出力する。
// 必要な理由: 想定計算量と実測候補数の差をログだけで比較できるようにするため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::out_fixed_b_log(
        int v,
        AllOrd fixed_b_all_ord,
        const FixedBLogCounter &counter
) {
    std::ostringstream oss;
    oss << "fixed_b_count"
        << " v=" << v
        << " fixed_b_all_ord=" << fixed_b_all_ord.get()
        << " qcnt=" << counter.qcnt
        << " a_all_ord_count=" << counter.a_all_ord_count
        << " pair_count=" << counter.pair_count
        << " eval_count=" << counter.eval_count
        << " rebuild_count=" << counter.rebuild_count
        << " insert_a2b_count=" << counter.insert_a2b_count
        << " insert_b2a_count=" << counter.insert_b2a_count
        << " best_diff=" << counter.best_diff;
    append_fixed_b_log_line(oss.str());
}

// 責務: BOrder 内に TARGET(v) entry が存在するかを返す。
// 必要な理由: A2A-only の v では BOrder に all_ord がなく、get_target_order が assert するため。
template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::Helper::has_b_target_order(
        const YakinamashiState &yst,
        int v
) {
    const my_vector<OrderEntry> &entries = yst.query_order.border.get_order_entries();
    for (const OrderEntry &entry: entries) {
        if (entry.value == v && entry.val_state == ValState::TARGET) return true;
    }
    return false;
}

// 責務: destroy 前 BOrder 内に TARGET(v) entry が存在するかを返す。
// 必要な理由: physical destroy 後の current border では v が消えているため、復元前に snapshot で分岐するため。
template<size_t MAX_N>
bool GreedyQueriesConstructor<MAX_N>::Helper::snapshot_has_b_target_order(
        const PhysicalDestroyContext<MAX_N> &context,
        int v
) {
    const my_vector<OrderEntry> &entries =
            context.erase_order_snapshot.before_erased_order.border.get_order_entries();
    for (const OrderEntry &entry: entries) {
        if (entry.value == v && entry.val_state == ValState::TARGET) return true;
    }
    return false;
}

// 責務: yst コピーへ AOrder 更新、query rebuild、A2B/B2A 挿入を適用し、sum_turn 差分を返す。
// 必要な理由: fixed B all_ord 経路では候補を近似計算せず、実際の再構築結果で best を選ぶため。
template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::Helper::eval_fixed_b_pair(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        int a2b_idx,
        int b2a_idx,
        AllOrd fixed_b_all_ord,
        AllOrd target_a_all_ord
) {
    my_assert(0 <= a2b_idx && a2b_idx <= b2a_idx);
    YakinamashiState eval_yst = yst;
    eval_yst.query_order.aorder.insert_update_shift(v, target_a_all_ord);
    rebuild_queries_for_state(current_st, eval_yst);
    insert_eval_a2b(eval_yst, current_st, v, a2b_idx);
    int shifted_b2a_idx = b2a_idx;
    if (a2b_idx <= b2a_idx) shifted_b2a_idx++;
    insert_eval_b2a(eval_yst, current_st, v, shifted_b2a_idx);
    (void) fixed_b_all_ord;
    return eval_yst.queries_state.sum_turn - yst.queries_state.sum_turn;
}

// 責務: A2B 末尾以外の全 a2b_idx、全 b2a_idx、全有効 target A all_ord を実評価し、best を返す。
// 必要な理由: 既存 Collector とは別に、B all_ord 固定の naive 探索経路を持つため。
template<size_t MAX_N>
typename GreedyQueriesConstructor<MAX_N>::Helper::FixedBPairResult
GreedyQueriesConstructor<MAX_N>::Helper::collect_fixed_b_pair(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        AllOrd fixed_b_all_ord
) {
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    InsertRangeSet a_all_ord_ranges = build_aorder_v_ranges(yst, current_st, v);
    FixedBPairResult result;
    FixedBLogCounter counter;
    counter.qcnt = qcnt;
    for_each_all_ord_in_ranges(a_all_ord_ranges, [&](AllOrd) {
        counter.a_all_ord_count++;
    });
    for (int a2b_idx = 0; a2b_idx < qcnt; a2b_idx++) {
        for (int b2a_idx = a2b_idx; b2a_idx <= qcnt; b2a_idx++) {
            counter.pair_count++;
            for_each_all_ord_in_ranges(a_all_ord_ranges, [&](AllOrd target_a_all_ord) {
                counter.eval_count++;
                counter.rebuild_count++;
                counter.insert_a2b_count++;
                counter.insert_b2a_count++;
                const int diff = eval_fixed_b_pair(
                        yst, current_st, v, a2b_idx, b2a_idx, fixed_b_all_ord, target_a_all_ord);
                if (result.found && result.pair.diff <= diff) return;
                result.found = true;
                result.pair = ScoredPair{PairQidx{a2b_idx, b2a_idx}, fixed_b_all_ord, diff};
                result.pair.target_a_all_ord = target_a_all_ord;
                result.pair.has_target_a = true;
                counter.best_diff = diff;
            });
        }
    }
    out_fixed_b_log(v, fixed_b_all_ord, counter);
    return result;
}

// 責務: FixedBPairResult の ScoredPair を検証し、apply_insert_pair_plan で実状態へ反映する。
// 必要な理由: AOrder 更新、query rebuild、A2B/B2A 挿入を既存 pair 経路と同じ手順に揃えるため。
template<size_t MAX_N>
void GreedyQueriesConstructor<MAX_N>::Helper::apply_fixed_b_pair(
        YakinamashiState &yst,
        const State &current_st,
        int v,
        const FixedBPairResult &result
) {
    my_assert(result.found);
    my_assert(result.pair.has_target_a);
    apply_insert_pair_plan(yst, current_st, v, result.pair);
}

// 責務: destroy 前 border で v より前にあった現在 entry の最後の直後を返す。
// 必要な理由: B all_ord を固定する前に、元 border の相対順序に反しない位置へ v を戻すため。
template<size_t MAX_N>
int GreedyQueriesConstructor<MAX_N>::Helper::find_restore_b_pos(
        const PhysicalDestroyContext<MAX_N> &context,
        int v
) {
    const my_vector<OrderEntry> &before_entries =
            context.erase_order_snapshot.before_erased_order.border.get_order_entries();
    const my_vector<OrderEntry> &current_entries =
            context.yst.query_order.border.get_order_entries();
    int restore_pos = 0;
    bool found_v = false;
    for (const OrderEntry &entry: before_entries) {
        if (entry.value == v && entry.val_state == ValState::TARGET) {
            found_v = true;
            break;
        }
        const int current_pos =
                find_optional_entry_pos_by_value_state(current_entries, entry.value, ValState::TARGET);
        if (current_pos != -1) restore_pos = current_pos + 1;
    }
    my_assert(found_v);
    my_assert(0 <= restore_pos && restore_pos <= static_cast<int>(current_entries.size()));
    return restore_pos;
}

// 責務: 消えている TARGET(v) を元 border 相対順序に反しない位置へ挿入し、その all_ord を返す。
// 必要な理由: fixed B all_ord 評価の前提として、BOrder に v の TARGET entry が必要なため。
template<size_t MAX_N>
AllOrd GreedyQueriesConstructor<MAX_N>::Helper::restore_fixed_b_order(
        PhysicalDestroyContext<MAX_N> &context,
        int v
) {
    if (!context.erase_order_snapshot.is_erased_v[v]) {
        return context.yst.query_order.border.get_target_order(v);
    }
    const int restore_pos = find_restore_b_pos(context, v);
    return context.yst.query_order.border.update_insert_ord_v(v, AllOrd(restore_pos));
}
