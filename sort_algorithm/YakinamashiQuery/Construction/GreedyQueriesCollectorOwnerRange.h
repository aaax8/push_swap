#pragma once

// This file is included inside GreedyQueriesConstructor<MAX_N>::Helper after
// GreedyQueriesCollector.h. It reuses the existing collector helpers and only
// replaces A2B range ownership with non-overlapping owner ranges.

// 責務: linear all_ord 半開区間、その owner a2b_idx、band 以外の A2B 側 cached score を保持する。
// 必要な理由: 同じ all_ord を複数 a2b_idx が担当しないよう、overlap ごとに score 比較できるようにするため。
// この区間は、band 以外のスコアが同じであることが保証される。
struct OwnedA2BRange {
    OrdRange range;
    int a2b_idx = -1;
    int a2b_insert_turn = 0;
    int direct1_idx = -1;
    int direct1_diff = 0;
    int direct2_idx = -1;
    int direct2_diff = 0;
    int no_band_score = 0;
    int begin_score = 0;
};

// 責務: linear all_ord 半開区間と、band 以外の B2A after cost cache を保持する。
// 必要な理由: B2A 側も OrdRange ごとに limit 判定し、同じ cache の隣接区間だけ matching 前に merge するため。
struct B2ARangeCost {
    OrdRange range;
    int b2a_idx = -1;
    int b2a_insert_turn = 0;
    int direct1_idx = -1;
    int direct1_diff = 0;
    int direct2_idx = -1;
    int direct2_diff = 0;
    int tail_diff = 0;
    int final_rot_diff = 0;
    int no_band_score = 0;
};

// 責務: A2B owner range と B2A accepted range の交差 1 本を保持する。
// 必要な理由: 非連続な同じ pair の range を後で同じ PairRangeTask の vector<OrdRange> に集めるため。
struct PairRangeUnit {
    int a2b_idx = -1;
    int b2a_idx = -1;
    OrdRange range;
};

// 責務: PairRangeUnit 構築時に使う owner overlap 経路を表す。
// 必要な理由: map 経路と array 経路で別々に candidates を作り、best pair 一致を確認するため。
enum class OwnerOverlapSource {
    map,
    array
};

// 責務: owner collector で作る pair task 一時配列の最大要素数を定義する。
// 必要な理由: MAX_QUERY_CNT * MAX_INSERT_EVENT_B 個の PairRangeTask を固定保持するとメモリが大きすぎるため。
static constexpr size_t OWNER_PAIR_TASK_CAPACITY = BASE_PAIR_TASK_CAPACITY;

// 責務: begin_score 以外の owner cache 情報が完全に一致するか返す。
// 必要な理由: band 以外が同じ隣接区間だけを安全に merge するため。
static bool has_same_a2b_owner_cache(const OwnedA2BRange &lhs, const OwnedA2BRange &rhs) {
    return lhs.a2b_idx == rhs.a2b_idx &&
           lhs.a2b_insert_turn == rhs.a2b_insert_turn &&
           lhs.direct1_idx == rhs.direct1_idx &&
           lhs.direct1_diff == rhs.direct1_diff &&
           lhs.direct2_idx == rhs.direct2_idx &&
           lhs.direct2_diff == rhs.direct2_diff &&
           lhs.no_band_score == rhs.no_band_score;
}

// 責務: band 以外の B2A range cache 情報が完全に一致するか返す。
// 必要な理由: B2A limit OK range のうち、同じ after cost cache の隣接区間だけを安全に merge するため。
bool has_same_b2a_range_cache(const B2ARangeCost &lhs, const B2ARangeCost &rhs) {
    return lhs.b2a_idx == rhs.b2a_idx &&
           lhs.b2a_insert_turn == rhs.b2a_insert_turn &&
           lhs.direct1_idx == rhs.direct1_idx &&
           lhs.direct1_diff == rhs.direct1_diff &&
           lhs.direct2_idx == rhs.direct2_idx &&
           lhs.direct2_diff == rhs.direct2_diff &&
           lhs.tail_diff == rhs.tail_diff &&
           lhs.final_rot_diff == rhs.final_rot_diff &&
           lhs.no_band_score == rhs.no_band_score;
}

// 責務: band_bits から [a2b_idx,qcnt) の band 数を数え、direct1/direct2 分を除外する。
// 必要な理由: band 以外を cached した owner score と組み合わせて overlap 比較するため。
static int calc_owner_band_count(
        const OwnedA2BRange &owner,
        int all_ord,
        int qcnt,
        const InsertPairPrecalc &precalc
) {
    my_assert(0 <= all_ord && all_ord < MAX_INSERT_EVENT_B);
    int band_count = pop_cnt(precalc.band_bits[all_ord], owner.a2b_idx, qcnt);
    if (owner.a2b_idx <= owner.direct1_idx && owner.direct1_idx < qcnt &&
        precalc.band_bits[all_ord][owner.direct1_idx]) band_count--;
    if (owner.a2b_idx <= owner.direct2_idx && owner.direct2_idx < qcnt &&
        precalc.band_bits[all_ord][owner.direct2_idx]) band_count--;
    return band_count;
}

// 責務: cached no-band score と指定 all_ord の band count を合算する。
// 必要な理由: 新旧 owner の overlap を begin 代表点で比較するため。
static int calc_owner_score(
        const OwnedA2BRange &owner,
        int all_ord,
        int qcnt,
        const InsertPairPrecalc &precalc
) {
    if (all_ord == owner.range.begin) return owner.begin_score;
    return owner.no_band_score + calc_owner_band_count(owner, all_ord, qcnt, precalc);
}

// 責務: range.begin の all_ord で band を再計算し、begin_score を更新した owner を返す。
// 必要な理由: split 後も begin_score が新しい range.begin の band 込み score を表すようにするため。
static OwnedA2BRange with_owner_range(
        OwnedA2BRange owner,
        const OrdRange &range,
        int qcnt,
        const InsertPairPrecalc &precalc
) {
    owner.range = range;
    owner.begin_score = owner.no_band_score + calc_owner_band_count(owner, range.begin, qcnt, precalc);
    return owner;
}

// 責務: linear range の左端 key で owner range 群を保持する。
// 必要な理由: a2b_idx 全走査を避け、交差する owner range だけを検索できるようにするため。
struct A2BOwnerRangeMapSet {
    std::map<int, OwnedA2BRange> ranges;

    // 責務: 同じ owner cache を持つ左隣 range とだけ 1 本へ merge する。
    // 必要な理由: overlap 処理中に右側の未処理 owner range を先に merge しないようにするため。
    void merge_prev_only(std::map<int, OwnedA2BRange>::iterator it) {
        if (it == ranges.end() || it == ranges.begin()) return;
        auto prev_it = std::prev(it);
        if (prev_it->second.range.end == it->second.range.begin &&
            has_same_a2b_owner_cache(prev_it->second, it->second)) {
            OwnedA2BRange merged = prev_it->second;
            merged.range.end = it->second.range.end;
            ranges.erase(prev_it);
            ranges.erase(it);
            ranges.emplace(merged.range.begin, merged);
        }
    }

    // 責務: 空でない segment を begin key で追加し、左隣だけ merge する。
    // 必要な理由: overlap 処理中に右側の未処理 owner range を先に merge しないようにするため。
    void add_segment(const OwnedA2BRange &segment) {
        if (segment.range.begin >= segment.range.end) return;
        auto insert_res = ranges.emplace(segment.range.begin, segment);
        my_assert(insert_res.second);
        merge_prev_only(insert_res.first);
    }

    // 責務: lower_bound と 1 つ前から、range に重なる owner range を返す。
    // 必要な理由: owner 更新で a2b_idx 全走査を避けるため。
    const vec_array<OwnedA2BRange, MAX_INSERT_EVENT_B> &collect_overlaps(const OrdRange &range) const {
        static vec_array<OwnedA2BRange, MAX_INSERT_EVENT_B> overlaps;
        overlaps.clear();
        auto it = ranges.lower_bound(range.begin);
        if (it != ranges.begin()) {
            auto prev_it = std::prev(it);
            if (prev_it->second.range.end > range.begin) overlaps.push_back(prev_it->second);
        }
        while (it != ranges.end() && it->second.range.begin < range.end) {
            overlaps.push_back(it->second);
            ++it;
        }
        return overlaps;
    }

    // 責務: 既存 owner と交差する部分を begin 代表 score で比較し、勝った owner の区間だけ残す。
    // 必要な理由: 同じ all_ord を複数 a2b_idx が担当しない owner range 集合を作るため。
    void insert_range(
            const OwnedA2BRange &new_owner,
            int qcnt,
            const InsertPairPrecalc &precalc
    ) {
        const OrdRange new_range = new_owner.range;
        if (new_range.begin >= new_range.end) return;
        int cursor = new_range.begin;
        auto it = ranges.lower_bound(new_range.begin);
        if (it != ranges.begin()) {
            auto prev_it = std::prev(it);
            if (prev_it->second.range.end > new_range.begin) it = prev_it;
        }
        while (cursor < new_range.end) {
            while (it != ranges.end() && it->second.range.end <= cursor) ++it;
            if (it == ranges.end() || it->second.range.begin >= new_range.end) {
                OwnedA2BRange tail = with_owner_range(
                        new_owner, OrdRange{cursor, new_range.end}, qcnt, precalc);
                add_segment(tail);
                cursor = new_range.end;
                break;
            }
            if (cursor < it->second.range.begin) {
                OwnedA2BRange gap = with_owner_range(
                        new_owner,
                        OrdRange{cursor, std::min(it->second.range.begin, new_range.end)},
                        qcnt,
                        precalc);
                add_segment(gap);
                cursor = gap.range.end;
                continue;
            }
            OwnedA2BRange old_owner = it->second;
            const int overlap_begin = std::max(cursor, old_owner.range.begin);
            const int overlap_end = std::min(new_range.end, old_owner.range.end);
            if (overlap_begin < overlap_end) {
                const int old_score = calc_owner_score(old_owner, overlap_begin, qcnt, precalc);
                const int new_score = calc_owner_score(new_owner, overlap_begin, qcnt, precalc);
                if (new_score < old_score) {
                    if (old_owner.range.begin < overlap_begin) {
                        it->second.range.end = overlap_begin;
                        ++it;
                    } else {
                        it = ranges.erase(it);
                    }
                    OwnedA2BRange mid = with_owner_range(
                            new_owner, OrdRange{overlap_begin, overlap_end}, qcnt, precalc);
                    add_segment(mid);
                    if (overlap_end < old_owner.range.end) {
                        OwnedA2BRange right = with_owner_range(
                                old_owner, OrdRange{overlap_end, old_owner.range.end}, qcnt, precalc);
                        add_segment(right);
                    }
                    it = ranges.lower_bound(overlap_end);
                } else {
                    ++it;
                }
                cursor = overlap_end;
            }
        }
    }
};

// 責務: owner_by_begin に begin 起点の owner を置き、cover_begin で各 all_ord が属する owner begin を引けるようにする。
// 必要な理由: map lower_bound を使わず、長い owner range でも重なり区間を cover_begin から直接見つけるため。
struct A2BOwnerRangeArraySet {
    std::array<OwnedA2BRange, MAX_INSERT_EVENT_B> owner_by_begin{};
    std::array<int, MAX_INSERT_EVENT_B> cover_begin{};
    std::array<bool, MAX_INSERT_EVENT_B> has_owner_begin{};

    // 責務: cover_begin と has_owner_begin を未登録状態へ初期化する。
    // 必要な理由: stack 上に作られる owner set でも未初期化値を overlap 判定に使わないようにするため。
    A2BOwnerRangeArraySet() {
        clear();
    }

    // 責務: 各 all_ord の所属 begin と begin 登録 flag を消す。
    // 必要な理由: constructor と将来の再利用時に owner_by_begin/cover_begin の状態を揃えるため。
    void clear() {
        for (int i = 0; i < static_cast<int>(MAX_INSERT_EVENT_B); i++) {
            cover_begin[i] = -1;
            has_owner_begin[i] = false;
        }
    }

    // 責務: OrdRange が MAX_INSERT_EVENT_B 内の半開区間か assert する。
    // 必要な理由: owner_by_begin/cover_begin の固定配列外アクセスを防ぐため。
    void validate_range(const OrdRange &range) const {
        assert(0 <= range.begin && range.begin <= range.end);
        assert(range.end <= static_cast<int>(MAX_INSERT_EVENT_B));
    }

    // 責務: 指定 range の各 all_ord が属する owner begin を記録する。
    // 必要な理由: collect_overlaps が all_ord から包含 owner を直接引けるようにするため。
    void fill_cover(const OrdRange &range, int begin) {
        validate_range(range);
        for (int all_ord = range.begin; all_ord < range.end; all_ord++) {
            cover_begin[all_ord] = begin;
        }
    }

    // 責務: 指定 range の各 all_ord を未所属に戻す。
    // 必要な理由: owner range の削除や短縮で古い包含情報が残らないようにするため。
    void clear_cover(const OrdRange &range) {
        validate_range(range);
        for (int all_ord = range.begin; all_ord < range.end; all_ord++) {
            cover_begin[all_ord] = -1;
        }
    }

    // 責務: owner_by_begin と has_owner_begin と cover_begin を同時に更新する。
    // 必要な理由: merge 処理から再利用できる最小追加操作を持つため。
    void add_raw_segment(const OwnedA2BRange &segment) {
        validate_range(segment.range);
        if (segment.range.begin >= segment.range.end) return;
        assert(!has_owner_begin[segment.range.begin]);
        owner_by_begin[segment.range.begin] = segment;
        has_owner_begin[segment.range.begin] = true;
        fill_cover(segment.range, segment.range.begin);
    }

    // 責務: owner_by_begin の登録 flag と cover_begin の所属情報を消す。
    // 必要な理由: overlap 勝敗で旧 owner を置き換えるときに array 状態を同期するため。
    void remove_segment(int begin) {
        assert(0 <= begin && begin < static_cast<int>(MAX_INSERT_EVENT_B));
        assert(has_owner_begin[begin]);
        clear_cover(owner_by_begin[begin].range);
        has_owner_begin[begin] = false;
    }

    // 責務: 短縮された後半の cover_begin を消し、owner range.end を更新する。
    // 必要な理由: 旧 owner の左側だけを残す split を map 経路と同じ意味で表すため。
    void shrink_segment_end(int begin, int end) {
        assert(0 <= begin && begin < static_cast<int>(MAX_INSERT_EVENT_B));
        assert(has_owner_begin[begin]);
        OwnedA2BRange &owner = owner_by_begin[begin];
        assert(owner.range.begin <= end && end <= owner.range.end);
        clear_cover(OrdRange{end, owner.range.end});
        owner.range.end = end;
    }

    // 責務: cover_begin[begin-1] から左隣 owner を引き、cache が同じなら 1 本へまとめる。
    // 必要な理由: map 経路の merge_prev_only と同じ merge 条件を array 経路でも保つため。
    void merge_prev_only(int begin) {
        if (begin <= 0 || !has_owner_begin[begin]) return;
        const int prev_begin = cover_begin[begin - 1];
        if (prev_begin < 0) return;
        assert(has_owner_begin[prev_begin]);
        if (owner_by_begin[prev_begin].range.end == begin &&
            has_same_a2b_owner_cache(owner_by_begin[prev_begin], owner_by_begin[begin])) {
            OwnedA2BRange merged = owner_by_begin[prev_begin];
            merged.range.end = owner_by_begin[begin].range.end;
            remove_segment(prev_begin);
            remove_segment(begin);
            add_raw_segment(merged);
        }
    }

    // 責務: 空でない segment を begin 配列へ追加し、左隣だけ merge する。
    // 必要な理由: map 経路と同じ登録後 merge 条件を array 経路にも適用するため。
    void add_segment(const OwnedA2BRange &segment) {
        if (segment.range.begin >= segment.range.end) return;
        add_raw_segment(segment);
        merge_prev_only(segment.range.begin);
    }

    // 責務: [begin,end) 内で最初に owner が始まる all_ord を返す。
    // 必要な理由: cover_begin が未所属の位置から、次に比較すべき owner range へ進むため。
    int find_next_begin(int begin, int end) const {
        assert(0 <= begin && begin <= end);
        assert(end <= static_cast<int>(MAX_INSERT_EVENT_B));
        for (int all_ord = begin; all_ord < end; all_ord++) {
            if (has_owner_begin[all_ord]) return all_ord;
        }
        return -1;
    }

    // 責務: cover_begin から包含 owner を引き、owner.range.end へジャンプしながら overlap を返す。
    // 必要な理由: 長い owner range でも巻き戻り探索なしで対象 range との重なりを見つけるため。
    const vec_array<OwnedA2BRange, MAX_INSERT_EVENT_B> &collect_overlaps(const OrdRange &range) const {
        static vec_array<OwnedA2BRange, MAX_INSERT_EVENT_B> overlaps;
        validate_range(range);
        overlaps.clear();
        int cursor = range.begin;
        while (cursor < range.end) {
            const int owner_begin = cover_begin[cursor];
            if (owner_begin >= 0) {
                assert(has_owner_begin[owner_begin]);
                const OwnedA2BRange &owner = owner_by_begin[owner_begin];
                overlaps.push_back(owner);
                cursor = std::min(owner.range.end, range.end);
            } else {
                const int next_begin = find_next_begin(cursor + 1, range.end);
                if (next_begin < 0) break;
                cursor = next_begin;
            }
        }
        return overlaps;
    }

    // 責務: owner_by_begin の登録済み要素だけを all_ranges に詰める。
    // 必要な理由: map 経路と登録結果全体が一致するか確認するため。
    const vec_array<OwnedA2BRange, MAX_INSERT_EVENT_B> &collect_all() const {
        static vec_array<OwnedA2BRange, MAX_INSERT_EVENT_B> all_ranges;
        all_ranges.clear();
        for (int begin = 0; begin < static_cast<int>(MAX_INSERT_EVENT_B); begin++) {
            if (has_owner_begin[begin]) all_ranges.push_back(owner_by_begin[begin]);
        }
        return all_ranges;
    }

    // 責務: cover_begin で既存 owner overlap を辿り、map 経路と同じ score 比較・split・merge を行う。
    // 必要な理由: map 経路と同じ owner 集合を owner_by_begin/cover_begin でも構築するため。
    void insert_range(
            const OwnedA2BRange &new_owner,
            int qcnt,
            const InsertPairPrecalc &precalc
    ) {
        const OrdRange new_range = new_owner.range;
        validate_range(new_range);
        if (new_range.begin >= new_range.end) return;
        int cursor = new_range.begin;
        while (cursor < new_range.end) {
            const int old_begin = cover_begin[cursor];
            if (old_begin < 0) {
                const int next_begin = find_next_begin(cursor + 1, new_range.end);
                const int gap_end = next_begin < 0 ? new_range.end : next_begin;
                OwnedA2BRange gap = with_owner_range(
                        new_owner, OrdRange{cursor, gap_end}, qcnt, precalc);
                add_segment(gap);
                cursor = gap_end;
                continue;
            }
            assert(has_owner_begin[old_begin]);
            OwnedA2BRange old_owner = owner_by_begin[old_begin];
            const int overlap_begin = cursor;
            const int overlap_end = std::min(new_range.end, old_owner.range.end);
            const int old_score = calc_owner_score(old_owner, overlap_begin, qcnt, precalc);
            const int new_score = calc_owner_score(new_owner, overlap_begin, qcnt, precalc);
            if (new_score < old_score) {
                if (old_owner.range.begin < overlap_begin) {
                    shrink_segment_end(old_owner.range.begin, overlap_begin);
                } else {
                    remove_segment(old_owner.range.begin);
                }
                OwnedA2BRange mid = with_owner_range(
                        new_owner, OrdRange{overlap_begin, overlap_end}, qcnt, precalc);
                add_segment(mid);
                if (overlap_end < old_owner.range.end) {
                    OwnedA2BRange right = with_owner_range(
                            old_owner, OrdRange{overlap_end, old_owner.range.end}, qcnt, precalc);
                    add_segment(right);
                }
            }
            cursor = overlap_end;
        }
    }
};

// 責務: 指定された owner overlap source だけへ登録し、その経路の overlap を返す。
// 必要な理由: 同じ seed 条件で map 経路と array 経路を別々に実行し、速度と最終 score を比較するため。
struct A2BOwnerRangeSet {
    A2BOwnerRangeMapSet map_set;
    A2BOwnerRangeArraySet array_set;
    OwnerOverlapSource overlap_source = OwnerOverlapSource::map;

    explicit A2BOwnerRangeSet(OwnerOverlapSource init_overlap_source) :
            overlap_source(init_overlap_source) {
    }

    // 責務: overlap_source に応じて map_set または array_set の片方へ new_owner を登録する。
    // 必要な理由: map 経路と array 経路を別実行にし、速度差を測れるようにするため。
    void insert_range(
            const OwnedA2BRange &new_owner,
            int qcnt,
            const InsertPairPrecalc &precalc
    ) {
        if (overlap_source == OwnerOverlapSource::map) {
            map_set.insert_range(new_owner, qcnt, precalc);
        } else {
            array_set.insert_range(new_owner, qcnt, precalc);
        }
    }

    // 責務: overlap_source に応じて map_set または array_set の collect_overlaps を返す。
    // 必要な理由: map 経路と array 経路を同じ collector 形状で別々に実行するため。
    const vec_array<OwnedA2BRange, MAX_INSERT_EVENT_B> &collect_overlaps(const OrdRange &range) const {
        if (overlap_source == OwnerOverlapSource::map) {
            return map_set.collect_overlaps(range);
        }
        return array_set.collect_overlaps(range);
    }
};

// 責務: 登録予定の A2B owner range を長さ 1 の range 群に分解して owner 集合へ登録する。
// 必要な理由: overlap 先頭 1 点だけで既存 owner/new owner の勝敗を決める粗さがスコア低下原因かを切り分けるため。
void insert_a2b_owner_unit_ranges_for_test(
        A2BOwnerRangeSet &owner_ranges,
        const OwnedA2BRange &owner,
        int qcnt,
        const InsertPairPrecalc &precalc
) {
    for (int all_ord = owner.range.begin; all_ord < owner.range.end; all_ord++) {
        if (G::use_owner_even_unit && (all_ord & 1)) continue;
        OwnedA2BRange unit_owner =
                with_owner_range(owner, OrdRange{all_ord, all_ord + 1}, qcnt, precalc);
        owner_ranges.insert_range(unit_owner, qcnt, precalc);
    }
}

// 責務: 指定 a2b_idx/all_ord の A2B insert、direct1/direct2、fixed indirect no-band を計算して owner range 情報へ詰める。
// 必要な理由: owner 比較時に band 以外を再計算せず、区間内で一定な score として使うため。
OwnedA2BRange build_owned_a2b_range(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        const OrdRange &range,
        const InsertPairPrecalc &precalc
) {
    const int n = yst.queries_state.current_N;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const AllOrd all_ord_b(range.begin);
    DebugIns2Case case_info{a2b_idx, qcnt, all_ord_b.get()};
    DebugIns2A2BPlan a2b_plan = build_a2b_plan(
            v, yst, all_ord_b,
            precalc.exist_stack_a_ords,
            precalc.exist_stack_b_ords,
            precalc.prev_a2a_ranges,
            precalc.base_ra_prefix_sum_A2A,
            precalc.base_rra_prefix_sum_A2A,
            a2b_idx,
            n
    );
    OwnedA2BRange owner;
    owner.range = range;
    owner.a2b_idx = a2b_idx;
    owner.a2b_insert_turn = a2b_plan.bri.ins_turn;
    owner.direct1_idx = normalize_a2b_direct1_idx(a2b_idx, precalc, qcnt);
    owner.direct2_idx = normalize_a2b_direct2_idx(owner.direct1_idx, a2b_idx, precalc, qcnt);
    owner.direct1_diff = calc_a2b_direct1_diff(
            owner.direct1_idx, yst, init_st, yst.queries_state.queries, case_info, a2b_plan, n);
    QueryOrdEffect a2b_effect;
    a2b_effect.a_effect.set_del(a2b_plan.tar_all_ord_a);
    a2b_effect.b_effect.set_add(a2b_plan.tar_all_ord_b);
    a2b_effect.diff_stack_a_cnt = -1;
    owner.direct2_diff = calc_a2b_direct2_diff(
            owner.direct1_idx, owner.direct2_idx, yst, init_st, yst.queries_state.queries,
            case_info, a2b_plan, n, precalc.next_ab_idxs, a2b_effect,
            precalc.indirect_a2b_ra_prefix_sum_A2A,
            precalc.indirect_a2b_rra_prefix_sum_A2A,
            precalc);
    int fixed_no_band = calc_debug_mid_indirect_sum_without_band(
            precalc.fixed_indirect_a2b_prefix_sum,
            precalc.a2b_band_ranges,
            case_info,
            owner.direct1_idx,
            owner.direct2_idx);
    owner.no_band_score =
            owner.a2b_insert_turn + owner.direct1_diff + owner.direct2_diff + fixed_no_band;
    owner.begin_score = owner.no_band_score + calc_owner_band_count(owner, range.begin, qcnt, precalc);
    return owner;
}

// 責務: circular range と entry/direct 点から band 以外の score が一定な OrdRange 列を作る。
// 必要な理由: limit 判定と owner 比較を小区間単位で行えるようにするため。
const vec_array<OrdRange, MAX_INSERT_EVENT_B> &build_a2b_owner_split_ranges(
        const BAllOrdRange &provisional_b_range,
        const EntryPointArray &entry_points
) {
    static vec_array<OrdRange, MAX_INSERT_EVENT_B> result;
    result.clear();
    LinearRangeArray linear_ranges = split_circular_range(provisional_b_range.range);
    for (const OrdRange &linear_range: linear_ranges) {
        EntryPointArray points = collect_entry_point_values_in_linear_range(entry_points, linear_range);
        append_ranges_by_points(result, linear_range, points);
    }
    return result;
}

// 責務: A2B insert と direct1/direct2 の合計が A2B limit 以下か返す。
// 必要な理由: 既存 Collector と同じ A2B 単体 limit で owner range 登録を絞るため。
bool owner_range_passes_a2b_limit(
        const OwnedA2BRange &owner,
        const A2BB2ACollectWorkspace &workspace
) {
    const int a2b_insert_direct =
            owner.a2b_insert_turn + owner.direct1_diff + owner.direct2_diff;
    return a2b_insert_direct <= workspace.limits.a2b_ins_direct_lim;
}

// 責務: range.begin の all_ord について [b2a_idx,qcnt) の band 数を数え、direct1/direct2 分を除外する。
// 必要な理由: B2A range limit 判定を Fenwick なしの本番経路で高速に行うため。
int calc_b2a_range_band_count(
        const B2ARangeCost &range_cost,
        int qcnt,
        const InsertPairPrecalc &precalc
) {
    const int all_ord = range_cost.range.begin;
    my_assert(0 <= all_ord && all_ord < MAX_INSERT_EVENT_B);
    int band_count = pop_cnt(precalc.band_bits[all_ord], range_cost.b2a_idx, qcnt);
    if (range_cost.b2a_idx <= range_cost.direct1_idx && range_cost.direct1_idx < qcnt &&
        precalc.band_bits[all_ord][range_cost.direct1_idx]) band_count--;
    if (range_cost.b2a_idx <= range_cost.direct2_idx && range_cost.direct2_idx < qcnt &&
        precalc.band_bits[all_ord][range_cost.direct2_idx]) band_count--;
    return band_count;
}

// 責務: cached no-band score と range.begin の direct 除外込み band count を合算する。
// 必要な理由: B2A OrdRange ごとの採用可否を既存 after cost と同じ意味で判定するため。
int calc_b2a_range_total(
        const B2ARangeCost &range_cost,
        int qcnt,
        const InsertPairPrecalc &precalc
) {
    return range_cost.no_band_score + calc_b2a_range_band_count(range_cost, qcnt, precalc);
}

// 責務: FIXED_A2B_IDX=0 の仮 A2B を前提に、B2A insert/direct/tail/final の no-band cost を詰める。
// 必要な理由: B2A range 単位 limit 判定と隣接 merge 判定に同じ cache を使うため。
B2ARangeCost build_b2a_range_cost(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int b2a_idx,
        const OrdRange &range,
        const InsertPairPrecalc &precalc
) {
    constexpr int FIXED_A2B_IDX = 0;
    const int n = yst.queries_state.current_N;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const AllOrd all_ord_b(range.begin);
    DebugIns2Case case_info{FIXED_A2B_IDX, b2a_idx, all_ord_b.get()};
    DebugIns2A2BPlan a2b_plan = build_a2b_plan(
            v, yst, all_ord_b,
            precalc.exist_stack_a_ords,
            precalc.exist_stack_b_ords,
            precalc.prev_a2a_ranges,
            precalc.base_ra_prefix_sum_A2A,
            precalc.base_rra_prefix_sum_A2A,
            FIXED_A2B_IDX,
            n
    );
    DebugIns2B2APlan b2a_plan = build_b2a_plan(
            v, yst, init_st, case_info, a2b_plan,
            precalc.exist_stack_a_ords,
            precalc.exist_stack_b_ords,
            precalc.prev_a2a_ranges,
            precalc.indirect_a2b_ra_prefix_sum_A2A,
            precalc.indirect_a2b_rra_prefix_sum_A2A,
            n,
            precalc
    );
    QueryOrdEffect net_effect = build_a2b_b2a_net_exist_effect(a2b_plan, b2a_plan);

    B2ARangeCost range_cost;
    range_cost.range = range;
    range_cost.b2a_idx = b2a_idx;
    range_cost.b2a_insert_turn = b2a_plan.bri.ins_turn;
    range_cost.direct1_idx = normalize_b2a_direct1_idx(b2a_idx, precalc, qcnt);
    range_cost.direct2_idx = normalize_b2a_direct2_idx(range_cost.direct1_idx, b2a_idx, precalc, qcnt);
    range_cost.tail_diff = calc_tail_indirect_sum(
            precalc.fixed_indirect_a2b_b2a_prefix_sum,
            case_info,
            range_cost.direct1_idx,
            range_cost.direct2_idx,
            qcnt);
    const bool use_direct_final_rot = precalc.next_a2b_b2a_effected_valid_q_idx[b2a_idx] == qcnt;
    range_cost.final_rot_diff =
            use_direct_final_rot ? precalc.direct_final_rot_diff : precalc.indirect_final_rot_diff;
    range_cost.direct1_diff = calc_b2a_direct1_diff(
            range_cost.direct1_idx, yst, init_st, yst.queries_state.queries, case_info, b2a_plan,
            net_effect, n);
    range_cost.direct2_diff = calc_b2a_direct2_diff(
            precalc, range_cost.direct1_idx, range_cost.direct2_idx, yst, init_st,
            yst.queries_state.queries, case_info, b2a_plan, n, precalc.next_ab_idxs, net_effect,
            precalc.indirect_a2b_b2a_ra_prefix_sum_A2A,
            precalc.indirect_a2b_b2a_rra_prefix_sum_A2A);
    range_cost.no_band_score =
            range_cost.b2a_insert_turn + range_cost.tail_diff + range_cost.final_rot_diff +
            range_cost.direct1_diff + range_cost.direct2_diff;
    return range_cost;
}

// 責務: DEBUG 中に同じ range.begin all_ord の legacy total と高速 total が一致することを検証する。
// 必要な理由: band_bits 経路への変更で direct band 除外や after cost の意味がずれていないことを保証するため。
void assert_b2a_range_total_matches_legacy(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const B2ARangeCost &range_cost,
        const BandRangeFenwick &active_bands,
        const InsertPairPrecalc &precalc
) {
#ifdef DEBUG
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    const int fast_total = calc_b2a_range_total(range_cost, qcnt, precalc);
    B2AAfterCost legacy_cost = calc_b2a_after_cost(
            yst, init_st, v, range_cost.b2a_idx, AllOrd(range_cost.range.begin), active_bands, precalc);
    my_assert(fast_total == legacy_cost.total);
#else
    (void) yst;
    (void) init_st;
    (void) v;
    (void) range_cost;
    (void) active_bands;
    (void) precalc;
#endif
}

// 責務: circular range と B2A entry point から B2A no-band score が一定な OrdRange 列を作る。
// 必要な理由: B2A limit 判定を OrdRange 単位で行い、NG 区間を matching から除外するため。
const vec_array<OrdRange, MAX_INSERT_EVENT_B> &build_b2a_split_ranges(
        const BAllOrdRange &provisional_range,
        const EntryPointArray &entry_points
) {
    static vec_array<OrdRange, MAX_INSERT_EVENT_B> result;
    result.clear();
    LinearRangeArray linear_ranges = split_circular_range(provisional_range.range);
    for (const OrdRange &linear_range: linear_ranges) {
        EntryPointArray points = collect_entry_point_values_in_linear_range(entry_points, linear_range);
        append_ranges_by_points(result, linear_range, points);
    }
    return result;
}

// 責務: B2A range を分割し、begin all_ord の total が limit 以下の range だけを残して隣接 merge する。
// 必要な理由: limit NG の B2A 区間を A2B owner range との matching に進ませないため。
const vec_array<B2ARangeCost, MAX_INSERT_EVENT_B> &build_b2a_accepted_ranges(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int b2a_idx,
        const AbIdxRange &ab_range,
        const QueryScanState &scan_state,
        const A2BB2ACollectWorkspace &workspace,
        const BandRangeFenwick &active_bands,
        const InsertPairPrecalc &precalc
) {
    static vec_array<B2ARangeCost, MAX_INSERT_EVENT_B> accepted;
    accepted.clear();
    const BIdxRange &b_idx_range = ab_range.b_idx_range;
    if (b_idx_range.range.is_empty()) my_assert(b_idx_range.means_full);
    BAllOrdRange provisional_range =
            make_previsional_b_ord_range(yst, scan_state, b_idx_range, workspace.b_all_ord_size);
    if (provisional_range.range.is_empty()) return accepted;
    EntryPointArray entry_points =
            collect_b2a_entry_points(yst, scan_state, b2a_idx, b_idx_range, provisional_range, precalc);
    const vec_array<OrdRange, MAX_INSERT_EVENT_B> &split_ranges =
            build_b2a_split_ranges(provisional_range, entry_points);
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    for (const OrdRange &range: split_ranges) {
        if (range.begin >= range.end) continue;
        B2ARangeCost range_cost = build_b2a_range_cost(yst, init_st, v, b2a_idx, range, precalc);
        assert_b2a_range_total_matches_legacy(yst, init_st, v, range_cost, active_bands, precalc);
        const int total = calc_b2a_range_total(range_cost, qcnt, precalc);
        if (total > workspace.limits.b2a_after_lim) continue;
        if (!accepted.empty() &&
            accepted.back().range.end == range_cost.range.begin &&
            has_same_b2a_range_cache(accepted.back(), range_cost)) {
            accepted.back().range.end = range_cost.range.end;
        } else {
            accepted.push_back(range_cost);
        }
    }
    return accepted;
}

// 責務: A2B range を entry/direct 点で分割し、limit を通った小区間だけ owner 集合へ登録する。
// 必要な理由: A2B all_ord owner を重複なしに保ちながら、後続 B2A merge に使うため。
void collect_and_register_a2b_owner_ranges(
        A2BOwnerRangeSet &owner_ranges,
        const A2BB2ACollectWorkspace &workspace,
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        const AbIdxRange &a2b_ab_range,
        const QueryScanState &scan_state,
        const InsertPairPrecalc &precalc
) {
    if (!contains_init_v_in_a_range(yst, scan_state, v, a2b_ab_range)) return;
    const BIdxRange &b_idx_range = a2b_ab_range.b_idx_range;
    if (b_idx_range.range.is_empty()) my_assert(b_idx_range.means_full);
    BAllOrdRange provisional_b_range =
            make_previsional_b_ord_range(yst, scan_state, b_idx_range, workspace.b_all_ord_size);
    if (provisional_b_range.range.is_empty()) return;
    EntryPointArray entry_points = collect_a2b_entry_points(
            yst, scan_state, a2b_idx, b_idx_range, provisional_b_range, precalc);
    const vec_array<OrdRange, MAX_INSERT_EVENT_B> &split_ranges =
            build_a2b_owner_split_ranges(provisional_b_range, entry_points);
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    bool has_pending_owner = false;
    OwnedA2BRange pending_owner;
    for (const OrdRange &range: split_ranges) {
        if (range.begin >= range.end) continue;
        if (G::use_owner_even_unit && range.end == range.begin + 1 && (range.begin & 1)) continue;
        OwnedA2BRange owner =
                build_owned_a2b_range(yst, init_st, v, a2b_idx, range, precalc);
        if (!owner_range_passes_a2b_limit(owner, workspace)) continue;
        if (has_pending_owner &&
            pending_owner.range.end == owner.range.begin &&
            has_same_a2b_owner_cache(pending_owner, owner)) {
            pending_owner.range.end = owner.range.end;
        } else {
            if (has_pending_owner) {
                if (G::use_owner_unit_range) {
                    insert_a2b_owner_unit_ranges_for_test(owner_ranges, pending_owner, qcnt, precalc);
                } else {
                    owner_ranges.insert_range(pending_owner, qcnt, precalc);
                }
            }
            pending_owner = owner;
            has_pending_owner = true;
        }
    }
    if (has_pending_owner) {
        if (G::use_owner_unit_range) {
            insert_a2b_owner_unit_ranges_for_test(owner_ranges, pending_owner, qcnt, precalc);
        } else {
            owner_ranges.insert_range(pending_owner, qcnt, precalc);
        }
    }
}

// 責務: 指定 B2A range と交差する A2B owner ranges を PairRangeUnit として out に追加する。
// 必要な理由: 非連続な同じ pair の overlap を後で同じ PairRangeTask にまとめるため。
void append_pair_range_units_for_b2a_range(
        const A2BOwnerRangeSet &owner_ranges,
        const B2ARangeCost &b2a_range,
        vec_array<PairRangeUnit, MAX_QUERY_CNT * MAX_INSERT_EVENT_B + 100> &out_units
) {
    const vec_array<OwnedA2BRange, MAX_INSERT_EVENT_B> &overlapping_owners =
            owner_ranges.collect_overlaps(b2a_range.range);
    for (const OwnedA2BRange &owner: overlapping_owners) {
        OrdRange overlap{
                std::max(owner.range.begin, b2a_range.range.begin),
                std::min(owner.range.end, b2a_range.range.end)
        };
        if (overlap.begin >= overlap.end) continue;
        out_units.push_back(PairRangeUnit{owner.a2b_idx, b2a_range.b2a_idx, overlap});
    }
}

// 責務: PairRangeUnit を sort し、同じ pair の OrdRange を 1 つの PairRangeTask に集める。
// 必要な理由: Mo 評価を既存通り pair 単位に保ち、非連続 range を同じ pair として扱うため。
void append_pair_tasks_from_units(
        vec_array<PairRangeUnit, MAX_QUERY_CNT * MAX_INSERT_EVENT_B + 100> &units,
        BasePairCandidates &out
) {
    std::sort(units.begin(), units.end(), [](const PairRangeUnit &lhs, const PairRangeUnit &rhs) {
        if (lhs.a2b_idx != rhs.a2b_idx) return lhs.a2b_idx < rhs.a2b_idx;
        if (lhs.b2a_idx != rhs.b2a_idx) return lhs.b2a_idx < rhs.b2a_idx;
        if (lhs.range.begin != rhs.range.begin) return lhs.range.begin < rhs.range.begin;
        return lhs.range.end < rhs.range.end;
    });
    bool has_task = false;
    for (const PairRangeUnit &unit: units) {
        if (unit.range.begin >= unit.range.end) continue;
        if (!has_task ||
            out.pair_tasks.back().a2b_idx != unit.a2b_idx ||
            out.pair_tasks.back().b2a_idx != unit.b2a_idx) {
            PairRangeTask task;
            task.a2b_idx = unit.a2b_idx;
            task.b2a_idx = unit.b2a_idx;
            task.range_begin = static_cast<int>(out.pair_ranges.size());
            task.range_count = 0;
            out.pair_tasks.push_back(task);
            has_task = true;
        }
        out.pair_ranges.push_back(unit.range);
        out.pair_tasks.back().range_count++;
    }
}

// 責務: B2A range 単位 limit を通った区間だけを A2B owner range と matching し、overlap を out に追加する。
// 必要な理由: limit NG の B2A 区間を候補化せず、同じ pair の range は後でまとめられるようにするため。
bool collect_b2a_candidates_owner_range(
        const A2BOwnerRangeSet &owner_ranges,
        A2BB2ACollectWorkspace &workspace,
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int b2a_idx,
        const QueryScanState &scan_state,
        const AbIdxRange &ab_range,
        const BandRangeFenwick &active_bands,
        const InsertPairPrecalc &precalc,
        vec_array<PairRangeUnit, MAX_QUERY_CNT * MAX_INSERT_EVENT_B + 100> &out_units,
        int &candidate_num
) {
    if (!contains_pushed_v_in_a_range(yst, scan_state, v, ab_range)) return false;
    const vec_array<B2ARangeCost, MAX_INSERT_EVENT_B> &accepted_ranges = build_b2a_accepted_ranges(
            yst, init_st, v, b2a_idx, ab_range, scan_state, workspace, active_bands, precalc);
    const int before_size = static_cast<int>(out_units.size());
    for (const B2ARangeCost &b2a_range: accepted_ranges) {
        append_pair_range_units_for_b2a_range(owner_ranges, b2a_range, out_units);
    }
    const int added = static_cast<int>(out_units.size()) - before_size;
    candidate_num += added;
    return added > 0;
}

// 責務: A2B owner range を非重複に更新し、B2A accepted range との overlap を PairRangeTask へまとめる。
// 必要な理由: 既存 Collector を残したまま、B2A range 単位 limit と A2B owner range を組み合わせつつ next_ab_idxs の vec_array 化に追従するため。
bool collect_candidates_for_range_ext_owner_range(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const RangeExt &range_ext,
        const QueryBoundaryArray &next_ab_idxs,
        const my_bitset<MAX_N> &is_never_construct_v,
        BasePairCandidates &out,
        int &candidate_num,
        const InsertPairPrecalc &precalc,
        A2BB2ACollectLimits limits,
        OwnerOverlapSource overlap_source
) {
    if (G::is_timing_log_enabled()) {
        std::ostringstream ss;
        ss << "stage=owner_range_ext_begin"
           << " v=" << v
           << " qcnt=" << yst.queries_state.queries.size()
           << " from_a=" << range_ext.from_ext_a
           << " to_a=" << range_ext.to_ext_a
           << " from_b=" << range_ext.from_ext_b
           << " to_b=" << range_ext.to_ext_b
           << " a2b_lim=" << limits.a2b_ins_direct_lim
           << " b2a_lim=" << limits.b2a_after_lim
           << " source=" << (overlap_source == OwnerOverlapSource::array ? "array" : "map");
        G::write_timing_log(ss.str());
    }
    my_vector<AbIdxRange> query_ab_ranges =
            make_ab_idx_ranges(yst, init_st, range_ext, is_never_construct_v, next_ab_idxs);
    QueriesCalculator qs_calc(init_st.current_N, yst.queries_state.queries);
    QueryScanState scan_state = init_scan_state(yst, init_st);
    int query_count = static_cast<int>(yst.queries_state.queries.size());
    A2BB2ACollectWorkspace workspace;
    init_a2b_b2a_collect_workspace(
            workspace,
            query_count,
            yst.query_order.border.get_all_order_entries_size(),
            limits
    );
    A2BOwnerRangeSet owner_ranges(overlap_source);
    static vec_array<PairRangeUnit, MAX_QUERY_CNT * MAX_INSERT_EVENT_B + 100> pair_units;
    pair_units.clear();
    BandRangeFenwick active_bands;
    const int b_insert_slot_count = calc_insert_slot_count(workspace.b_all_ord_size);
    active_bands.init(b_insert_slot_count);
    for (const BandRangeInfo &band: precalc.a2b_band_ranges) {
        add_band_range_to_fenwick(active_bands, band, 1);
    }
    bool appended = false;
    for (int q_idx = 0; q_idx < query_count; q_idx++) {
        const QueryCommonRI &qri = yst.queries_state.queries[q_idx];
        const AbIdxRange &ab_range = query_ab_ranges[q_idx];
        if (G::is_timing_log_enabled()) {
            std::ostringstream ss;
            ss << "stage=owner_q_b2a_begin"
               << " v=" << v
               << " q_idx=" << q_idx
               << " qcnt=" << query_count
               << " candidate_num=" << candidate_num
               << " pair_units=" << pair_units.size();
            G::write_timing_log(ss.str());
        }
        const auto timing_b2a_start = std::chrono::steady_clock::now();
        if (collect_b2a_candidates_owner_range(
                owner_ranges, workspace, yst, init_st, v, q_idx, scan_state, ab_range,
                active_bands, precalc, pair_units, candidate_num)) {
            appended = true;
        }
        if (G::is_timing_log_enabled()) {
            const auto timing_b2a_end = std::chrono::steady_clock::now();
            const auto elapsed_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(timing_b2a_end - timing_b2a_start).count();
            std::ostringstream ss;
            ss << "stage=owner_q_b2a_end"
               << " v=" << v
               << " q_idx=" << q_idx
               << " qcnt=" << query_count
               << " candidate_num=" << candidate_num
               << " pair_units=" << pair_units.size()
               << " elapsed_ms=" << elapsed_ms;
            G::write_timing_log(ss.str());
        }
        add_band_range_to_fenwick(active_bands, precalc.a2b_band_ranges[q_idx], -1);
        if (G::is_timing_log_enabled()) {
            std::ostringstream ss;
            ss << "stage=owner_q_a2b_register_begin"
               << " v=" << v
               << " q_idx=" << q_idx
               << " qcnt=" << query_count
               << " pair_units=" << pair_units.size();
            G::write_timing_log(ss.str());
        }
        const auto timing_a2b_start = std::chrono::steady_clock::now();
        collect_and_register_a2b_owner_ranges(
                owner_ranges, workspace, yst, init_st, v, q_idx, ab_range, scan_state, precalc);
        if (G::is_timing_log_enabled()) {
            const auto timing_a2b_end = std::chrono::steady_clock::now();
            const auto elapsed_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(timing_a2b_end - timing_a2b_start).count();
            std::ostringstream ss;
            ss << "stage=owner_q_a2b_register_end"
               << " v=" << v
               << " q_idx=" << q_idx
               << " qcnt=" << query_count
               << " pair_units=" << pair_units.size()
               << " elapsed_ms=" << elapsed_ms;
            G::write_timing_log(ss.str());
        }
        advance_scan_state(yst, scan_state, qs_calc, q_idx, qri);
    }
    const int final_b2a_idx = query_count;
    const AbIdxRange &final_ab_range = query_ab_ranges[final_b2a_idx];
    if (collect_b2a_candidates_owner_range(
            owner_ranges, workspace, yst, init_st, v, final_b2a_idx, scan_state, final_ab_range,
            active_bands, precalc, pair_units, candidate_num)) {
        appended = true;
    }
    if (G::is_timing_log_enabled()) {
        std::ostringstream ss;
        ss << "stage=owner_final_b2a_end"
           << " v=" << v
           << " q_idx=" << final_b2a_idx
           << " qcnt=" << query_count
           << " candidate_num=" << candidate_num
           << " pair_units=" << pair_units.size();
        G::write_timing_log(ss.str());
    }
    // 責務: 同じ pair の OrdRange を 1 つの PairRangeTask に集め、range 自体は結合しない。
    // 必要な理由: PairRangeUnit を Mo 単位にせず、既存の pair 単位評価を維持するため。
    append_pair_tasks_from_units(pair_units, out);
    out.ord_range_total_count += static_cast<int>(pair_units.size());
    if (G::is_timing_log_enabled()) {
        std::ostringstream ss;
        ss << "stage=owner_range_ext_end"
           << " v=" << v
           << " appended=" << appended
           << " candidate_num=" << candidate_num
           << " pair_units=" << pair_units.size()
           << " pair_tasks=" << out.pair_tasks.size()
           << " pair_ranges=" << out.pair_ranges.size()
           << " ord_range_total=" << out.ord_range_total_count;
        G::write_timing_log(ss.str());
    }
    return appended;
}

// 責務: range_ext retry を行いながら owner range 版の BasePairCandidates を返す。
// 必要な理由: 既存 collect_candidates を残したまま construct 経路から新 Collector を呼べるようにするため。
// 責務: owner range 経路で 1 つの v に対する pair candidate を列挙する。
// 必要な理由: owner range 経路でも通常 collector と同じ A/B 別 ext 条件で候補を作るため。
void collect_candidates_owner_range(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const InsertPairPrecalc &precalc,
        const GreedyConstructorParams &params,
        const my_bitset<MAX_N> &is_never_construct_v,
        BasePairCandidates &out
        ,
        OwnerOverlapSource overlap_source = OwnerOverlapSource::map
) {
    constexpr int OWNER_RETRY_LIMIT = 3;
    RangeExt range_ext = RangeExt{params.from_ext_a, params.to_ext_a, params.from_ext_b, params.to_ext_b};
    A2BB2ACollectLimits current_limits = A2BB2ACollectLimits{params.a2b_lim, params.b2a_lim};
    auto next_ab_idxs = build_next_ab_idxs(yst);
    int candidate_num = 0;
    int retry_count = 0;
    if (G::is_timing_log_enabled()) {
        std::ostringstream ss;
        ss << "stage=owner_collect_begin"
           << " v=" << v
           << " qcnt=" << yst.queries_state.queries.size()
           << " from_a=" << range_ext.from_ext_a
           << " to_a=" << range_ext.to_ext_a
           << " from_b=" << range_ext.from_ext_b
           << " to_b=" << range_ext.to_ext_b
           << " a2b_lim=" << current_limits.a2b_ins_direct_lim
           << " b2a_lim=" << current_limits.b2a_after_lim
           << " source=" << (overlap_source == OwnerOverlapSource::array ? "array" : "map");
        G::write_timing_log(ss.str());
    }
    while (true) {
        if (G::is_timing_log_enabled()) {
            std::ostringstream ss;
            ss << "stage=owner_retry_begin"
               << " v=" << v
               << " retry=" << retry_count
               << " candidate_num=" << candidate_num
               << " from_a=" << range_ext.from_ext_a
               << " to_a=" << range_ext.to_ext_a
               << " from_b=" << range_ext.from_ext_b
               << " to_b=" << range_ext.to_ext_b
               << " a2b_lim=" << current_limits.a2b_ins_direct_lim
               << " b2a_lim=" << current_limits.b2a_after_lim;
            G::write_timing_log(ss.str());
        }
        const auto timing_retry_start = std::chrono::steady_clock::now();
        bool appended = collect_candidates_for_range_ext_owner_range(
            yst, init_st, v, range_ext, next_ab_idxs, is_never_construct_v,
            out, candidate_num,
            precalc, current_limits, overlap_source);
        if (G::is_timing_log_enabled()) {
            const auto timing_retry_end = std::chrono::steady_clock::now();
            const auto elapsed_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(timing_retry_end - timing_retry_start).count();
            std::ostringstream ss;
            ss << "stage=owner_retry_end"
               << " v=" << v
               << " retry=" << retry_count
               << " appended=" << appended
               << " candidate_num=" << candidate_num
               << " pair_tasks=" << out.pair_tasks.size()
               << " pair_ranges=" << out.pair_ranges.size()
               << " ord_range_total=" << out.ord_range_total_count
               << " elapsed_ms=" << elapsed_ms;
            G::write_timing_log(ss.str());
        }
        if (appended) break;
        if (retry_count + 1 >= OWNER_RETRY_LIMIT) break;
        if (!can_retry_with_larger_ext(range_ext, init_st.current_N)) break;
        range_ext = RangeExt{
            range_ext.from_ext_a * 2,
            range_ext.to_ext_a * 2,
            range_ext.from_ext_b * 2,
            range_ext.to_ext_b * 2
        };
        current_limits.a2b_ins_direct_lim *= 2;
        current_limits.b2a_after_lim *= 2;
        retry_count++;
        if (G::is_timing_log_enabled()) {
            std::ostringstream ss;
            ss << "stage=owner_retry_widen"
               << " v=" << v
               << " retry=" << retry_count
               << " from_a=" << range_ext.from_ext_a
               << " to_a=" << range_ext.to_ext_a
               << " from_b=" << range_ext.from_ext_b
               << " to_b=" << range_ext.to_ext_b
               << " a2b_lim=" << current_limits.a2b_ins_direct_lim
               << " b2a_lim=" << current_limits.b2a_after_lim;
            G::write_timing_log(ss.str());
        }
    }
    if (G::is_timing_log_enabled()) {
        std::ostringstream ss;
        ss << "stage=owner_collect_end"
           << " v=" << v
           << " retry_count=" << retry_count
           << " candidate_num=" << candidate_num
           << " pair_tasks=" << out.pair_tasks.size()
           << " pair_ranges=" << out.pair_ranges.size()
           << " ord_range_total=" << out.ord_range_total_count;
        G::write_timing_log(ss.str());
    }
}
