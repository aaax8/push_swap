#pragma once

// This file is included inside GreedyQueriesConstructor<MAX_N>::Helper.
// It owns candidate enumeration data and flow; InsertPairPrecalc and detailed
// score/diff calculation stay in GreedyQueriesConstructor.cpp.

// 責務: A2B/B2A の query index pair だけを保持する。
struct PairQidx {
    int a2b_idx = -1;
    int b2a_idx = -1;
};

// 責務: owner range collector が作る task と range の最大要素数を定義する。
// 必要な理由: PairRangeTask 内に range 配列を内蔵せず、flat arena で静的領域を小さく保つため。
static constexpr size_t BASE_PAIR_TASK_CAPACITY = MAX_QUERY_CNT * MAX_INSERT_EVENT_B + 100;
static constexpr size_t BASE_PAIR_RANGE_CAPACITY = MAX_QUERY_CNT * MAX_INSERT_EVENT_B + 100;
// 責務: all_ord split point の固定保持容量を定義する。
// 必要な理由: entry point 系の一時 vector を固定配列化し、容量不足時は vec_array の assert で止めるため。
using EntryPointArray = vec_array<int, MAX_INSERT_EVENT_B>;
// 責務: A2B/B2A entry が持つ valid all_ord を workspace 単位の arena へ集約する容量を定義する。
// 必要な理由: RangeEntry ごとの vector 確保を避けつつ、A2B/B2A 両方の entry 点を保持するため。
static constexpr size_t VALID_ALL_ORD_CAPACITY = (MAX_QUERY_CNT * 2 + 1) * MAX_INSERT_EVENT_B;
using ValidAllOrdArena = vec_array<int, VALID_ALL_ORD_CAPACITY>;

// 責務: collect workspace が使う valid all_ord arena の static 実体を返す。
// 必要な理由: A2BB2ACollectWorkspace はローカル変数でも作られるため、大きい固定配列を値メンバにして stack を増やさないようにするため。
static ValidAllOrdArena &valid_all_ord_workspace() {
    static ValidAllOrdArena arena;
    return arena;
}

using InsertPairCandidatesByAllOrd = std::array<my_vector<PairQidx>, MAX_INSERT_SLOT_B>;

struct RangeExt {
    // 責務: A 側と B 側で別々に使う from/to 拡張幅を保持する。
    // 必要な理由: query destroy 後の A 側 range と B 側 range を独立に調整するため。
    int from_ext_a;
    int to_ext_a;
    int from_ext_b;
    int to_ext_b;
};

struct AIdxRange {
    CircularRange range = CircularRange(0, 0, 0);
};

struct BIdxRange {
    CircularRange range = CircularRange(0, 0, 0);
    bool means_full = false;
};

struct BAllOrdRange {
    CircularRange range = CircularRange(0, 0, 0);
};

//見ている、stack上のidx範囲
struct AbIdxRange {
    AIdxRange a_idx_range;
    BIdxRange b_idx_range;
};

struct CommonBRangeSet {
    int count = 0;
    std::array<CircularRange, 2> ranges = {
            CircularRange(0, 0, 0),
            CircularRange(0, 0, 0)
    };
};

// 責務: all_ord の linear 半開区間 [begin, end) を保持する。
// 必要な理由: Mo 評価用 segtree は circular range を直接扱わず、linear 半開区間だけを query するため。
struct OrdRange {
    int begin = 0;
    int end = 0;
};
// 責務: circular range を linear range に分けた最大 2 本を固定保持する。
// 必要な理由: split_circular_range の戻り値 vector をなくし、最大本数が 2 であることを型で表すため。
using LinearRangeArray = vec_array<OrdRange, 2>;

// 責務: 1 つの A2B/B2A pair と、その pair で評価する all_ord 半開区間列を保持する。
// 必要な理由: direct band 除外と calc_diff_without_band を pair 単位で 1 回にまとめるため。
struct PairRangeTask {
    int a2b_idx = -1;
    int b2a_idx = -1;
    int range_begin = 0;
    int range_count = 0;
};

// 責務: base で出現した pair と、その common range と評価済み all_ord を保持する。
struct BasePairInfo {
    PairQidx pair_qidx;
    CommonBRangeSet common_ranges;
    my_vector<int> base_all_ords;
};

// 責務: base base_cands_by_allord の all_ord bucket と pair 単位情報を保持する。
struct BasePairCandidates {
    InsertPairCandidatesByAllOrd base_cands_by_allord{};
    my_vector<BasePairInfo> base_pair_infos;
    vec_array<PairRangeTask, BASE_PAIR_TASK_CAPACITY> pair_tasks;
    vec_array<OrdRange, BASE_PAIR_RANGE_CAPACITY> pair_ranges;
    int ord_range_total_count = 0;

    // 責務: 候補保持配列の size だけを初期化し、次の候補収集に使える状態へ戻す。
    // 必要な理由: BasePairCandidates を static 確保しつつ、各 v の候補収集を独立させるため。
    void clear() {
        for (auto &bucket: base_cands_by_allord) bucket.clear();
        base_pair_infos.clear();
        pair_tasks.clear();
        pair_ranges.clear();
        ord_range_total_count = 0;
    }
};

// 責務: legacy/debug 比較用の候補 bucket と pair 情報を保持する。
// 必要な理由: legacy check は速度対象外なので、巨大な固定配列を持たず通常 owner 経路の静的領域を減らすため。
struct LegacyBasePairCandidates {
    InsertPairCandidatesByAllOrd base_cands_by_allord{};
    my_vector<BasePairInfo> base_pair_infos;
    my_vector<PairRangeTask> pair_tasks;
    my_vector<OrdRange> pair_ranges;
    int ord_range_total_count = 0;

    void clear() {
        for (auto &bucket: base_cands_by_allord) bucket.clear();
        base_pair_infos.clear();
        pair_tasks.clear();
        pair_ranges.clear();
        ord_range_total_count = 0;
    }
};

// 責務: 通常の候補収集で使い回す BasePairCandidates 参照を返す。
// 必要な理由: 固定容量化した BasePairCandidates を複数箇所で static 宣言してメモリを重複させないため。
BasePairCandidates &base_pair_candidates_workspace() {
    static BasePairCandidates candidates;
    return candidates;
}

// 責務: legacy check で通常候補と同時に参照できる BasePairCandidates 参照を返す。
// 必要な理由: new candidates を保持したまま legacy candidates を構築し、かつ固定容量領域を使い回すため。
LegacyBasePairCandidates &legacy_pair_candidates_workspace() {
    static LegacyBasePairCandidates candidates;
    return candidates;
}

// 責務: A2A を挿入する q_idx、shift 前の AOrder 挿入位置、sum_turn 差分を保持する。
struct A2ACandidate {
    bool found = false;
    int q_idx = -1;
    AllOrd a_all_ord = AllOrd(0);
    int diff = INT_MAX;
};

struct A2BB2ACollectLimits {
    int a2b_ins_direct_lim;
    int b2a_after_lim;
};

void widen_collect_limits(A2BB2ACollectLimits &limits) {
    limits.a2b_ins_direct_lim++;
    limits.b2a_after_lim++;
}

// 責務: arena 内の連続した AllOrd 範囲を range-for と index で読めるようにする。
// 必要な理由: RangeEntry から vector を外しても、既存の読み取り順序を変えずに参照するため。
struct AllOrdArenaView {
    const int *values = nullptr;
    int count = 0;

    struct Iterator {
        const int *ptr = nullptr;

        AllOrd operator*() const { return AllOrd(*ptr); }
        Iterator &operator++() {
            ptr++;
            return *this;
        }
        bool operator!=(const Iterator &other) const { return ptr != other.ptr; }
    };

    Iterator begin() const { return Iterator{values}; }
    Iterator end() const { return Iterator{count == 0 ? values : values + count}; }
    size_t size() const { return static_cast<size_t>(count); }
    bool empty() const { return count == 0; }
    AllOrd operator[](int idx) const {
        my_assert(0 <= idx && idx < count);
        return AllOrd(values[idx]);
    }
    AllOrd front() const {
        my_assert(0 < count);
        return AllOrd(values[0]);
    }
    AllOrd back() const {
        my_assert(0 < count);
        return AllOrd(values[count - 1]);
    }
};

struct RangeEntry {
    bool exists = false;
    BAllOrdRange registered_range;
    // 責務: registered_range.range の円環順序で並ぶ valid all_ord の arena 範囲を保持する。
    // 必要な理由: pair 作成時に A2B/B2A の点列を sort せず O(k) merge しつつ、RangeEntry ごとの vector 確保をなくすため。
    int valid_begin = 0;
    int valid_count = 0;
    const ValidAllOrdArena *valid_arena = nullptr;
    bool valid_empty_zero = false;

    void clear() {
        exists = false;
        registered_range = BAllOrdRange{};
        valid_begin = 0;
        valid_count = 0;
        valid_empty_zero = false;
    }

    // 責務: この entry が保持する valid all_ord 範囲を読み取り view として返す。
    // 必要な理由: 外部 arena 化後も、呼び出し側が vector と同じ順序で valid all_ord を走査できるようにするため。
    AllOrdArenaView valid_all_ords() const {
        if (valid_count == 0) return AllOrdArenaView{};
        my_assert(valid_arena != nullptr);
        return AllOrdArenaView{valid_arena->begin() + valid_begin, valid_count};
    }
};

struct PairCostLog {
    int a2b_insert_turn = 0;
    int a2b_direct1_idx = -1;
    int a2b_direct1_diff = 0;
    int a2b_direct2_idx = -1;
    int a2b_direct2_diff = 0;
    int a2b_total = 0;
    int b2a_insert_turn = 0;
    int b2a_direct1_idx = -1;
    int b2a_direct1_diff = 0;
    int b2a_direct2_idx = -1;
    int b2a_direct2_diff = 0;
    int b2a_total = 0;
    int b2a_tail_diff = 0;
    int b2a_final_rot_diff = 0;
    int b2a_band_count = 0;
    int b2a_after_total = 0;
};

struct BandRangeFenwick {
    vector<int> bit;
    int size = 0;

    void init(int init_size) {
        my_assert(0 <= init_size);
        size = init_size;
        bit.assign(size + 2, 0);
    }

    void add_point(int idx, int delta) {
        my_assert(0 <= idx && idx <= size);
        for (int pos = idx + 1; pos < static_cast<int>(bit.size()); pos += pos & -pos) {
            bit[pos] += delta;
        }
    }

    void add_linear_range(int left, int right, int delta) {
        my_assert(0 <= left && left <= right && right <= size);
        if (left == right) return;
        add_point(left, delta);
        add_point(right, -delta);
    }

    void add_circular_range(const CircularRange &range, int delta) {
        if (range.is_empty()) return;
        my_assert(range.get_N() == size);
        if (range.is_full()) {
            add_linear_range(0, size, delta);
            return;
        }
        const int start = range.get_start();
        const int end = range.get_end();
        if (start < end) {
            add_linear_range(start, end, delta);
            return;
        }
        add_linear_range(start, size, delta);
        add_linear_range(0, end, delta);
    }

    int point_query(int idx) const {
        my_assert(0 <= idx && idx < size);
        int sum = 0;
        for (int pos = idx + 1; pos > 0; pos -= pos & -pos) {
            sum += bit[pos];
        }
        return sum;
    }
};

struct B2AAfterCost {
    int insert_turn = 0;
    int tail_diff = 0;
    int final_rot_diff = 0;
    int direct1_idx = -1;
    int direct1_diff = 0;
    int direct2_idx = -1;
    int direct2_diff = 0;
    int band_count = 0;
    int total = 0;
};

// 責務: segtree node/query 結果として、最小 band 差分とそれを満たす all_ord を保持する。
// 必要な理由: range 内の最小 band 合計と復元用 all_ord を同時に返すため。
struct BandMinNode {
    int min_band_diff = INT_MAX;
    int min_all_ord = -1;
};

// 責務: CircularRange を OrdRange の列へ分割し、wrap する場合は [start,N), [0,end) に分ける。
// 必要な理由: BandMinSeg は circular range を直接扱わないため。
LinearRangeArray split_circular_range(const CircularRange &range) {
    LinearRangeArray ranges;
    if (range.is_empty()) return ranges;
    const int n = range.get_N();
    my_assert(0 < n);
    if (range.is_full()) {
        ranges.push_back(OrdRange{0, n});
        return ranges;
    }
    const int begin = range.get_start();
    const int end = range.get_end();
    if (begin < end) {
        ranges.push_back(OrdRange{begin, end});
        return ranges;
    }
    ranges.push_back(OrdRange{begin, n});
    if (0 < end) ranges.push_back(OrdRange{0, end});
    return ranges;
}

struct BandMinSeg {
    int point_count = 0;
    int size = 1;
    int log = 0;
    vector<BandMinNode> data;
    vector<int> lazy;

    static BandMinNode better_node(const BandMinNode &lhs, const BandMinNode &rhs) {
        if (lhs.min_band_diff != rhs.min_band_diff) {
            return lhs.min_band_diff < rhs.min_band_diff ? lhs : rhs;
        }
        if (lhs.min_all_ord == -1) return rhs;
        if (rhs.min_all_ord == -1) return lhs;
        return lhs.min_all_ord <= rhs.min_all_ord ? lhs : rhs;
    }

    void init(int n) {
        my_assert(0 <= n);
        point_count = n;
        size = 1;
        log = 0;
        while (size < point_count) {
            size <<= 1;
            log++;
        }
        data.assign(size * 2, BandMinNode{INT_MAX, -1});
        lazy.assign(size * 2, 0);
        for (int idx = 0; idx < size; idx++) {
            if (idx < point_count) data[size + idx] = BandMinNode{0, idx};
            else data[size + idx] = BandMinNode{INT_MAX, -1};
        }
        for (int node = size - 1; node >= 1; node--) pull(node);
    }

    void apply_node(int node, int delta) {
        if (data[node].min_all_ord != -1) data[node].min_band_diff += delta;
        lazy[node] += delta;
    }

    void push(int node) {
        if (lazy[node] == 0) return;
        apply_node(node << 1, lazy[node]);
        apply_node(node << 1 | 1, lazy[node]);
        lazy[node] = 0;
    }

    void pull(int node) {
        data[node] = better_node(data[node << 1], data[node << 1 | 1]);
    }

    void add_range(int begin, int end, int delta) {
        my_assert(0 <= begin && begin <= end && end <= point_count);
        if (begin == end) return;
        int left = begin + size;
        int right = end + size;
        int left0 = left;
        int right0 = right;
        for (int i = log; i >= 1; i--) {
            if (((left0 >> i) << i) != left0) push(left0 >> i);
            if (((right0 >> i) << i) != right0) push((right0 - 1) >> i);
        }
        while (left < right) {
            if (left & 1) apply_node(left++, delta);
            if (right & 1) apply_node(--right, delta);
            left >>= 1;
            right >>= 1;
        }
        for (int i = 1; i <= log; i++) {
            if (((left0 >> i) << i) != left0) pull(left0 >> i);
            if (((right0 >> i) << i) != right0) pull((right0 - 1) >> i);
        }
    }

    BandMinNode query_range(int begin, int end) {
        my_assert(0 <= begin && begin <= end && end <= point_count);
        if (begin == end) return BandMinNode{};
        int left = begin + size;
        int right = end + size;
        int left0 = left;
        int right0 = right;
        for (int i = log; i >= 1; i--) {
            if (((left0 >> i) << i) != left0) push(left0 >> i);
            if (((right0 >> i) << i) != right0) push((right0 - 1) >> i);
        }
        BandMinNode left_res;
        BandMinNode right_res;
        while (left < right) {
            if (left & 1) left_res = better_node(left_res, data[left++]);
            if (right & 1) right_res = better_node(data[--right], right_res);
            left >>= 1;
            right >>= 1;
        }
        return better_node(left_res, right_res);
    }
};

// 責務: band が存在する場合だけ circular range を linear OrdRange へ分割し、それぞれへ delta を加算する。
// 必要な理由: Mo の q_idx 追加削除と direct band 除外を同じ range add 処理で扱うため。
inline void add_band_range_to_min_seg(BandMinSeg &seg, const BandRangeInfo &band, int delta) {
    if (!band.has_band) return;
    LinearRangeArray ranges = split_circular_range(band.band_range);
    for (const OrdRange &range: ranges) seg.add_range(range.begin, range.end, delta);
}

struct A2BB2ACollectWorkspace {
    using A2BRangeSegtree = A2BB2ARangeSetLazySegTree<MAX_INSERT_SLOT_B, MAX_QUERY_CNT>;

    int query_count = 0;
    int b_all_ord_size = 0;
    A2BB2ACollectLimits limits;
    // 責務: A2B/B2A entry の valid all_ord 本体を保持する static arena を指す。
    // 必要な理由: workspace をローカル変数で作っても、大きい固定配列を stack に置かないようにするため。
    ValidAllOrdArena *valid_all_ord_arena = nullptr;
    std::array<RangeEntry, MAX_QUERY_CNT> a2b_entries;
    std::array<RangeEntry, MAX_QUERY_CNT + 1> b2a_entries;
    A2BRangeSegtree a2b_range_segtree;

    void init(int init_query_count, int init_b_all_ord_size, A2BB2ACollectLimits init_limits) {
        my_assert(0 <= init_query_count && init_query_count <= MAX_QUERY_CNT);
        my_assert(0 <= init_b_all_ord_size && init_b_all_ord_size <= static_cast<int>(MAX_INSERT_SLOT_B));
        query_count = init_query_count;
        b_all_ord_size = init_b_all_ord_size;
        limits = init_limits;
        valid_all_ord_arena = &valid_all_ord_workspace();
        valid_all_ord_arena->clear();
        for (int i = 0; i < query_count; i++) a2b_entries[i].clear();
        for (int i = 0; i <= query_count; i++) b2a_entries[i].clear();
        a2b_range_segtree.init(b_all_ord_size);
    }
};

// 責務: entry の valid 範囲開始位置を arena 末尾に設定する。
// 必要な理由: A2B/B2A entry が個別 vector を持たず、workspace arena 上の連続範囲だけを保持するため。
void begin_entry_valid_all_ords(A2BB2ACollectWorkspace &workspace, RangeEntry &entry) {
    my_assert(workspace.valid_all_ord_arena != nullptr);
    entry.valid_arena = workspace.valid_all_ord_arena;
    entry.valid_begin = static_cast<int>(workspace.valid_all_ord_arena->size());
    entry.valid_count = 0;
}

// 責務: workspace arena 末尾へ all_ord を追加し、entry の valid 件数を増やす。
// 必要な理由: 既存の push_back 順序を保ったまま、RangeEntry 内 vector を使わないようにするため。
void append_entry_valid_all_ord(A2BB2ACollectWorkspace &workspace, RangeEntry &entry, AllOrd all_ord) {
    my_assert(workspace.valid_all_ord_arena != nullptr);
    my_assert(entry.valid_arena == workspace.valid_all_ord_arena);
    my_assert(entry.valid_begin + entry.valid_count == static_cast<int>(workspace.valid_all_ord_arena->size()));
    workspace.valid_all_ord_arena->push_back(all_ord.get());
    entry.valid_count++;
}

void init_a2b_b2a_collect_workspace(
        A2BB2ACollectWorkspace &workspace,
        int query_count,
        int b_all_ord_size,
        A2BB2ACollectLimits limits
) {
    workspace.init(query_count, b_all_ord_size, limits);
}

// 責務: base で候補が出た pair の common range と評価済み all_ord を 1 件追加する。
template<class Candidates, class IntValues>
void append_base_pair_info(
        Candidates &out,
        int a2b_idx,
        int b2a_idx,
        const CommonBRangeSet &common_ranges,
        const IntValues &base_all_ords
) {
    my_assert(0 <= a2b_idx && a2b_idx <= b2a_idx);
    my_assert(!base_all_ords.empty());
    BasePairInfo info;
    info.pair_qidx = PairQidx{a2b_idx, b2a_idx};
    info.common_ranges = common_ranges;
    for (int all_ord: base_all_ords) info.base_all_ords.push_back(all_ord);
    out.base_pair_infos.push_back(info);
}

// 責務: 1 pair の OrdRange 列を out.pair_ranges 末尾へ連続配置し、対応する軽量 task を追加する。
// 必要な理由: PairRangeTask ごとの固定長 range 配列をなくし、巨大 static による link error を避けるため。
template<class Candidates, class OrdRanges>
void append_pair_range_task(
        Candidates &out,
        int a2b_idx,
        int b2a_idx,
        const OrdRanges &ord_ranges
) {
    PairRangeTask task;
    task.a2b_idx = a2b_idx;
    task.b2a_idx = b2a_idx;
    task.range_begin = static_cast<int>(out.pair_ranges.size());
    task.range_count = static_cast<int>(ord_ranges.size());
    for (const OrdRange &range: ord_ranges) out.pair_ranges.push_back(range);
    out.pair_tasks.push_back(task);
}

bool contains_any_common_range(const CommonBRangeSet &common_ranges, AllOrd all_ord) {
    for (int i = 0; i < common_ranges.count; i++) {
        if (common_ranges.ranges[i].contains(all_ord.get())) return true;
    }
    return false;
}

template<class AllOrdValues>
string all_ord_vector_to_debug_string(const AllOrdValues &all_ords) {
    string ret = "[";
    for (int i = 0; i < static_cast<int>(all_ords.size()); i++) {
        if (i > 0) ret += ",";
        ret += std::to_string(all_ords[i].get());
    }
    ret += "]";
    return ret;
}

template<class IntValues>
string int_vector_to_debug_string(const IntValues &values) {
    string ret = "[";
    for (int i = 0; i < static_cast<int>(values.size()); i++) {
        if (i > 0) ret += ",";
        ret += std::to_string(values[i]);
    }
    ret += "]";
    return ret;
}

string circular_range_to_debug_string(const CircularRange &range) {
    return string("{start=") + std::to_string(range.get_start()) +
           ", size=" + std::to_string(range.size()) +
           ", end=" + std::to_string(range.get_end()) +
           ", N=" + std::to_string(range.get_N()) + "}";
}

string common_ranges_to_debug_string(const CommonBRangeSet &common_ranges) {
    string ret = "[";
    for (int i = 0; i < common_ranges.count; i++) {
        if (i > 0) ret += ",";
        ret += circular_range_to_debug_string(common_ranges.ranges[i]);
    }
    ret += "]";
    return ret;
}

void add_band_range_to_fenwick(BandRangeFenwick &fenwick,
                               const BandRangeInfo &band,
                               int delta) {
    if (!band.has_band) return;
    fenwick.add_circular_range(band.band_range, delta);
}

int calc_band_hit(const BandRangeInfo &band, AllOrd all_ord_b) {
    if (!band.has_band) return 0;
    return band.band_range.contains(all_ord_b.get()) ? 1 : 0;
}

template<typename BandRangeContainer>
int calc_b2a_band_count_fenwick(const BandRangeFenwick &fenwick,
                                const BandRangeContainer &band_ranges,
                                AllOrd all_ord_b,
                                int direct1_idx,
                                int direct2_idx) {
    int band_count = fenwick.point_query(all_ord_b.get());
    int direct_band_count = 0;
    if (direct1_idx != -1) direct_band_count += calc_band_hit(band_ranges[direct1_idx], all_ord_b);
    if (direct2_idx != -1) direct_band_count += calc_band_hit(band_ranges[direct2_idx], all_ord_b);
    return band_count - direct_band_count;
}

template<typename BandRangeContainer>
int calc_b2a_band_count_naive(const BandRangeContainer &band_ranges,
                              int b2a_idx,
                              AllOrd all_ord_b,
                              int direct1_idx,
                              int direct2_idx) {
    int band_count = 0;
    for (int q_idx = b2a_idx; q_idx < static_cast<int>(band_ranges.size()); q_idx++) {
        band_count += calc_band_hit(band_ranges[q_idx], all_ord_b);
    }
    int direct_band_count = 0;
    if (direct1_idx != -1) direct_band_count += calc_band_hit(band_ranges[direct1_idx], all_ord_b);
    if (direct2_idx != -1) direct_band_count += calc_band_hit(band_ranges[direct2_idx], all_ord_b);
    return band_count - direct_band_count;
}

template<typename BandRangeContainer>
int calc_checked_b2a_band_count(const BandRangeFenwick &fenwick,
                                const BandRangeContainer &band_ranges,
                                int b2a_idx,
                                AllOrd all_ord_b,
                                int direct1_idx,
                                int direct2_idx) {
    int fenwick_count =
            calc_b2a_band_count_fenwick(fenwick, band_ranges, all_ord_b, direct1_idx, direct2_idx);
    (void) b2a_idx;
//     int naive_count =
//             calc_b2a_band_count_naive(band_ranges, b2a_idx, all_ord_b, direct1_idx, direct2_idx);
//     if (fenwick_count != naive_count) {
//         std::cerr << "b2a band count mismatch"
//                   << " b2a_idx=" << b2a_idx
//                   << " all_ord_b=" << all_ord_b.get()
//                   << " fenwick=" << fenwick_count
//                   << " naive=" << naive_count
//                   << std::endl;
//         assert(false);
//     }
    return fenwick_count;
}

B2AAfterCost calc_b2a_after_cost(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int b2a_idx,
        AllOrd pushed_b_all_ord,
        const BandRangeFenwick &active_bands,
        const InsertPairPrecalc &precalc
) {
    constexpr int FIXED_A2B_IDX = 0;
    const int n = yst.queries_state.current_N;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    DebugIns2Case case_info{FIXED_A2B_IDX, b2a_idx, pushed_b_all_ord.get()};
    DebugIns2A2BPlan a2b_plan = build_a2b_plan(
            v, yst, pushed_b_all_ord,
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

    B2AAfterCost result;
    result.insert_turn = b2a_plan.bri.ins_turn;
    result.direct1_idx = normalize_b2a_direct1_idx(b2a_idx, precalc, qcnt);
    result.direct2_idx = normalize_b2a_direct2_idx(result.direct1_idx, b2a_idx, precalc, qcnt);
    result.tail_diff = calc_tail_indirect_sum(
            precalc.fixed_indirect_a2b_b2a_prefix_sum, case_info,
            result.direct1_idx, result.direct2_idx, qcnt);
    const bool use_direct_final_rot = precalc.next_a2b_b2a_effected_valid_q_idx[b2a_idx] == qcnt;
    result.final_rot_diff = use_direct_final_rot ? precalc.direct_final_rot_diff : precalc.indirect_final_rot_diff;
    result.direct1_diff = calc_b2a_direct1_diff(
            result.direct1_idx, yst, init_st, yst.queries_state.queries, case_info, b2a_plan,
            net_effect, n);
    result.direct2_diff = calc_b2a_direct2_diff(
            precalc, result.direct1_idx, result.direct2_idx, yst, init_st, yst.queries_state.queries,
            case_info, b2a_plan, n, precalc.next_ab_idxs, net_effect,
            precalc.indirect_a2b_b2a_ra_prefix_sum_A2A,
            precalc.indirect_a2b_b2a_rra_prefix_sum_A2A);
    result.band_count = calc_checked_b2a_band_count(
            active_bands, precalc.a2b_band_ranges, b2a_idx, pushed_b_all_ord,
            result.direct1_idx, result.direct2_idx);
    result.total = result.insert_turn + result.tail_diff + result.final_rot_diff +
                   result.direct1_diff + result.direct2_diff + result.band_count;
    return result;
}

int calc_insert_slot_count(int all_ord_count) {
    my_assert(0 <= all_ord_count);
    return all_ord_count + 1;
}

BandRangeFenwick build_active_bands(int b2a_idx,
                                    int b_insert_slot_count,
                                    const InsertPairPrecalc &precalc) {
    my_assert(0 <= b2a_idx);
    BandRangeFenwick active_bands;
    active_bands.init(b_insert_slot_count);
    for (int q_idx = b2a_idx; q_idx < static_cast<int>(precalc.a2b_band_ranges.size()); q_idx++) {
        add_band_range_to_fenwick(active_bands, precalc.a2b_band_ranges[q_idx], 1);
    }
    return active_bands;
}

void apply_b2a_after_log(PairCostLog &cost_log,
                         const B2AAfterCost &b2a_after_cost) {
    cost_log.b2a_insert_turn = b2a_after_cost.insert_turn;
    cost_log.b2a_direct1_idx = b2a_after_cost.direct1_idx;
    cost_log.b2a_direct1_diff = b2a_after_cost.direct1_diff;
    cost_log.b2a_direct2_idx = b2a_after_cost.direct2_idx;
    cost_log.b2a_direct2_diff = b2a_after_cost.direct2_diff;
    cost_log.b2a_total = b2a_after_cost.insert_turn +
                          b2a_after_cost.direct1_diff + b2a_after_cost.direct2_diff;
    cost_log.b2a_tail_diff = b2a_after_cost.tail_diff;
    cost_log.b2a_final_rot_diff = b2a_after_cost.final_rot_diff;
    cost_log.b2a_band_count = b2a_after_cost.band_count;
    cost_log.b2a_after_total = b2a_after_cost.total;
}

PairCostLog build_pair_cost_log(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        int b2a_idx,
        AllOrd pushed_b_all_ord,
        const InsertPairPrecalc &precalc
) {
    const int n = yst.queries_state.current_N;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    PairCostLog log;
    DebugIns2Case case_info{a2b_idx, b2a_idx, pushed_b_all_ord.get()};

    DebugIns2A2BPlan a2b_plan = build_a2b_plan(
            v, yst, pushed_b_all_ord,
            precalc.exist_stack_a_ords,
            precalc.exist_stack_b_ords,
            precalc.prev_a2a_ranges,
            precalc.base_ra_prefix_sum_A2A,
            precalc.base_rra_prefix_sum_A2A,
            a2b_idx,
            n
    );
    log.a2b_insert_turn = a2b_plan.bri.ins_turn;
    log.a2b_direct1_idx = normalize_a2b_direct1_idx(a2b_idx, precalc, qcnt);
    log.a2b_direct2_idx = normalize_a2b_direct2_idx(log.a2b_direct1_idx, a2b_idx, precalc, qcnt);
    log.a2b_direct1_diff = calc_a2b_direct1_diff(
            log.a2b_direct1_idx, yst, init_st, yst.queries_state.queries, case_info, a2b_plan, n);
    QueryOrdEffect a2b_effect;
    a2b_effect.a_effect.set_del(a2b_plan.tar_all_ord_a);
    a2b_effect.b_effect.set_add(a2b_plan.tar_all_ord_b);
    a2b_effect.diff_stack_a_cnt = -1;
    log.a2b_direct2_diff = calc_a2b_direct2_diff(
            log.a2b_direct1_idx, log.a2b_direct2_idx, yst, init_st, yst.queries_state.queries,
            case_info, a2b_plan, n, precalc.next_ab_idxs, a2b_effect,
            precalc.indirect_a2b_ra_prefix_sum_A2A,
            precalc.indirect_a2b_rra_prefix_sum_A2A,
            precalc);
    log.a2b_total = log.a2b_insert_turn + log.a2b_direct1_diff + log.a2b_direct2_diff;

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
    log.b2a_insert_turn = b2a_plan.bri.ins_turn;
    log.b2a_direct1_idx = normalize_b2a_direct1_idx(b2a_idx, precalc, qcnt);
    log.b2a_direct2_idx = normalize_b2a_direct2_idx(log.b2a_direct1_idx, b2a_idx, precalc, qcnt);
    log.b2a_direct1_diff = calc_b2a_direct1_diff(
            log.b2a_direct1_idx, yst, init_st, yst.queries_state.queries, case_info, b2a_plan,
            net_effect, n);
    log.b2a_direct2_diff = calc_b2a_direct2_diff(
            precalc, log.b2a_direct1_idx, log.b2a_direct2_idx, yst, init_st, yst.queries_state.queries,
            case_info, b2a_plan, n, precalc.next_ab_idxs, net_effect,
            precalc.indirect_a2b_b2a_ra_prefix_sum_A2A,
            precalc.indirect_a2b_b2a_rra_prefix_sum_A2A);
    log.b2a_total = log.b2a_insert_turn + log.b2a_direct1_diff + log.b2a_direct2_diff;
    return log;
}

void out_pair_cost_log(int b2a_idx,
                       int a2b_idx,
                       AllOrd pushed_b_all_ord,
                       const PairCostLog &log) {
    if (!is_collector_log_enabled()) return;
    ::out2("    { all_ord =", pushed_b_all_ord.get(),
           ", a2b = { insert =", log.a2b_insert_turn,
           ", direct1_diff =", log.a2b_direct1_diff,
           ", direct2_diff =", log.a2b_direct2_diff,
           ", total =", log.a2b_total,
           "}, b2a = { insert =", log.b2a_insert_turn,
           ", direct1_diff =", log.b2a_direct1_diff,
           ", direct2_diff =", log.b2a_direct2_diff,
           ", tail_diff =", log.b2a_tail_diff,
           ", final_rot_diff =", log.b2a_final_rot_diff,
           ", band_count =", log.b2a_band_count,
           ", total =", log.b2a_total,
           ", after_total =", log.b2a_after_total,
           "} }",
           "a2b_idx =", a2b_idx,
           "b2a_idx =", b2a_idx);
}

template<class Candidates>
bool merge_valid_allords(
        Candidates &out,
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        int b2a_idx,
        const CommonBRangeSet &common_b_ranges,
        const RangeEntry &a2b_entry,
        const RangeEntry &b2a_entry,
        const BandRangeFenwick &active_bands,
        const InsertPairPrecalc &precalc,
        int &candidate_num
) {
    static std::array<int, MAX_INSERT_SLOT_B> appended_stamp_by_all_ord{};
    static int call_id = 0;
    call_id++;
    if (call_id == INT_MAX) {
        appended_stamp_by_all_ord.fill(0);
        call_id = 1;
    }
    bool appended = false;
    int appended_all_ord_count = 0;
    vec_array<int, MAX_INSERT_SLOT_B> appended_all_ords;
    const bool has_empty_zero = a2b_entry.valid_empty_zero && b2a_entry.valid_empty_zero;
    PairQidx pair_qidx{a2b_idx, b2a_idx};
    auto try_append = [&](AllOrd pushed_b_all_ord) {
        const int idx = pushed_b_all_ord.get();
        if (idx < 0 || static_cast<int>(MAX_INSERT_SLOT_B) <= idx) return;
        if (appended_stamp_by_all_ord[idx] == call_id) return;
        if (!contains_any_common_range(common_b_ranges, pushed_b_all_ord)) return;
        appended_stamp_by_all_ord[idx] = call_id;
        out.base_cands_by_allord[idx].push_back(pair_qidx);
        candidate_num++;
        appended = true;
        appended_all_ord_count++;
        appended_all_ords.push_back(idx);
    };
    if (has_empty_zero) {
        try_append(AllOrd(0));
    } else {
        //todo mergeしょり早く書きたい
        for (AllOrd all_ord: a2b_entry.valid_all_ords()) try_append(all_ord);
        for (AllOrd all_ord: b2a_entry.valid_all_ords()) try_append(all_ord);
        //valid ordsをoでかいて、見てる区間について書くと下のようになると思うので
        //___o__o_o_o_o
        //二つの共通部分には必ず一つは含まれるはず
        my_assert(!appended_all_ords.empty());
    }
    if (appended) {
        append_base_pair_info(out, a2b_idx, b2a_idx, common_b_ranges, appended_all_ords);
    }
    if (is_collector_log_enabled()) {
        outaaa("collector_pair_merge",
               "a2b_idx =", a2b_idx,
               ", b2a_idx =", b2a_idx,
               ", common_b_ranges =", common_ranges_to_debug_string(common_b_ranges),
               ", a2b_valid_cnt =", static_cast<int>(a2b_entry.valid_all_ords().size()),
               ", a2b_valid =", all_ord_vector_to_debug_string(a2b_entry.valid_all_ords()),
               ", b2a_valid_cnt =", static_cast<int>(b2a_entry.valid_all_ords().size()),
               ", b2a_valid =", all_ord_vector_to_debug_string(b2a_entry.valid_all_ords()),
               ", empty_zero_pair =", has_empty_zero ? 1 : 0,
               ", merged_cnt =", appended_all_ord_count,
               ", merged =", int_vector_to_debug_string(appended_all_ords));
    }
    if (appended_all_ord_count > 0 && is_collector_log_enabled()) {
        ::out2("b2a_idx =", b2a_idx);
        ::out2("  a2b_idx =", a2b_idx,
               ", common_b_ranges =", common_ranges_to_debug_string(common_b_ranges),
               ", merged_all_ords =", int_vector_to_debug_string(appended_all_ords));
        for (int all_ord: appended_all_ords) {
            PairCostLog cost_log = build_pair_cost_log(
                    yst, init_st, v, a2b_idx, b2a_idx, AllOrd(all_ord), precalc);
            B2AAfterCost b2a_after_cost = calc_b2a_after_cost(
                    yst, init_st, v, b2a_idx, AllOrd(all_ord), active_bands, precalc);
            apply_b2a_after_log(cost_log, b2a_after_cost);
            out_pair_cost_log(b2a_idx, a2b_idx, AllOrd(all_ord), cost_log);
        }
        string all_ord_list = "[";
        for (int i = 0; i < static_cast<int>(appended_all_ords.size()); i++) {
            if (i > 0) all_ord_list += ",";
            all_ord_list += std::to_string(appended_all_ords[i]);
        }
        all_ord_list += "]";
        outaaa("a2b_idx =", a2b_idx, ", b2a_idx =", b2a_idx,
               ", all_ord_b_cnt =", appended_all_ord_count,
               ", all_ord_b =", all_ord_list);
    }
    return appended;
}

template<class Func>
void for_each_all_ord_in_circular_range(const CircularRange &range, Func func) {
    for (int idx = 0; idx < range.size(); idx++) {
        int all_ord = ChunkSeparator::add_circular(range.get_start(), idx, range.get_N());
        func(AllOrd(all_ord));
    }
}

// 責務: 指定 pair の common_ranges に含まれる全 all_ord を base 候補 bucket に追加する。
// 必要な理由: valid all_ord だけを使う現行方式と比較できるよう、common range 全展開方式を関数として残すため。
template<class Candidates>
bool register_all_common_range(
        Candidates &out,
        int a2b_idx,
        int b2a_idx,
        const CommonBRangeSet &common_ranges,
        int &candidate_num
) {
    bool appended = false;
    int registered_count = 0;
    PairQidx pair_qidx{a2b_idx, b2a_idx};
    for (int range_i = 0; range_i < common_ranges.count; range_i++) {
        for_each_all_ord_in_circular_range(common_ranges.ranges[range_i], [&](AllOrd all_ord) {
            const int all_ord_idx = all_ord.get();
            my_assert(0 <= all_ord_idx && all_ord_idx < MAX_INSERT_SLOT_B);
            out.base_cands_by_allord[all_ord_idx].push_back(pair_qidx);
            candidate_num++;
            appended = true;
            registered_count++;
        });
    }
    outaaa("collector_register_all_common_range",
           "a2b_idx =", a2b_idx,
           ", b2a_idx =", b2a_idx,
           ", common_ranges =", common_ranges_to_debug_string(common_ranges),
           ", registered_cnt =", registered_count);
    return appended;
}

// Common-range calculation treats the right endpoint as inclusive.
int get_last_value(const CircularRange &range) {
    my_assert(!range.is_empty());
    return ChunkSeparator::add_circular(range.get_start(), range.size() - 1, range.get_N());
}

CircularRange build_range_from_closed_ends(int start, int last, int n) {
    my_assert(0 < n);
    int size = last - start + 1;
    if (size <= 0) size += n;
    return CircularRange(start, size, n);
}

#ifdef DEBUG

my_vector<AllOrd> collect_common_b_all_ords_naive_for_test(
        const CircularRange &a2b_b_range,
        const CircularRange &b2a_b_range
) {
    my_vector<AllOrd> common_b_all_ords;
    for_each_all_ord_in_circular_range(b2a_b_range, [&](AllOrd all_ord) {
        if (a2b_b_range.contains(all_ord.get())) common_b_all_ords.push_back(all_ord);
    });
    return common_b_all_ords;
}

my_vector<AllOrd> expand_common_b_ranges_for_test(const CommonBRangeSet &common_b_ranges) {
    my_vector<AllOrd> expanded;
    for (int i = 0; i < common_b_ranges.count; i++) {
        for_each_all_ord_in_circular_range(common_b_ranges.ranges[i], [&](AllOrd all_ord) {
            expanded.push_back(all_ord);
        });
    }
    return expanded;
}

void validate_common_b_ranges(
        const CircularRange &a2b_b_range,
        const CircularRange &b2a_b_range,
        const CommonBRangeSet &common_b_ranges
) {
    my_vector<AllOrd> naive = collect_common_b_all_ords_naive_for_test(a2b_b_range, b2a_b_range);
    my_vector<AllOrd> expanded = expand_common_b_ranges_for_test(common_b_ranges);
    std::sort(naive.begin(), naive.end());
    std::sort(expanded.begin(), expanded.end());
    my_assert(naive.size() == expanded.size());
    for (int i = 0; i < static_cast<int>(naive.size()); i++) my_assert(naive[i] == expanded[i]);
}

#endif

CommonBRangeSet intersect_non_full_common_b_ranges(
        const CircularRange &a2b_b_range,
        const CircularRange &b2a_b_range
) {
    int a = a2b_b_range.get_start(), b = get_last_value(a2b_b_range);
    int c = b2a_b_range.get_start(), d = get_last_value(b2a_b_range);
    bool a_in_rhs = b2a_b_range.contains(a), b_in_rhs = b2a_b_range.contains(b);
    bool c_in_lhs = a2b_b_range.contains(c), d_in_lhs = a2b_b_range.contains(d);
    CommonBRangeSet common_b_ranges;
    if (a != c && a_in_rhs && b_in_rhs && c_in_lhs && d_in_lhs) {
        common_b_ranges.count = 2;
        common_b_ranges.ranges[0] = build_range_from_closed_ends(c, b, a2b_b_range.get_N());
        common_b_ranges.ranges[1] = build_range_from_closed_ends(a, d, a2b_b_range.get_N());
        return common_b_ranges;
    }
    if (!(a_in_rhs || c_in_lhs)) return common_b_ranges;
    common_b_ranges.count = 1;
    int start = c_in_lhs ? c : a;
    int last = b_in_rhs ? b : d;
    common_b_ranges.ranges[0] = build_range_from_closed_ends(start, last, a2b_b_range.get_N());
    return common_b_ranges;
}

CommonBRangeSet intersect_common_b_ranges(const CircularRange &a2b_b_range, const CircularRange &b2a_b_range) {
    my_assert(a2b_b_range.get_N() == b2a_b_range.get_N());
    CommonBRangeSet common_b_ranges;
    if (a2b_b_range.is_full()) common_b_ranges = CommonBRangeSet{1, {b2a_b_range, CircularRange(0, 0, 0)}};
    else if (b2a_b_range.is_full()) common_b_ranges = CommonBRangeSet{1, {a2b_b_range, CircularRange(0, 0, 0)}};
    else if (!a2b_b_range.is_empty() && !b2a_b_range.is_empty()) {
        common_b_ranges = intersect_non_full_common_b_ranges(a2b_b_range, b2a_b_range);
    }
#ifdef DEBUG
    validate_common_b_ranges(a2b_b_range, b2a_b_range, common_b_ranges);
#endif
    return common_b_ranges;
}

// 責務: b_idx_range が表す現在の B stack 上の idx 範囲を順に走査し、各 idx の B all_ord を返す。
// 必要な理由: A2B/B2A entry points の土台として、B stack 上に実在する点を range 順に列挙するため。
EntryPointArray collect_b_range_all_ord_points(
        const YakinamashiState &yst,
        const QueryScanState &scan_state,
        const BIdxRange &b_idx_range
) {
    EntryPointArray points;
    if (b_idx_range.means_full && b_idx_range.range.is_empty()) {
        points.push_back(0);
        return points;
    }
    for_each_range_idx(b_idx_range.range, [&](int idx) {
        points.push_back(get_all_ord_b_by_pos(scan_state, yst, idx).get());
    });
    //WAS_AI_REV 単純にソートしてるのが気になる
    //今見てるrangeが、末尾から先頭に書けてまたぐ場合
    //たとえば [15, 5)みたいなとき、 15が先頭で、4が末尾になるような順序でソートされるのが自然だと思う
    //現状のソートの仕方で、区間をそれらの点で分割するときに問題が起きないのか疑問
    return points;
}

// 責務: base_range.get_start() から見た円環距離を返す。
// 必要な理由: wrap range でも sort せず点の挿入位置と順序検証を行うため。
int calc_circular_point_offset(int point, const CircularRange &base_range) {
    my_assert(base_range.contains(point));
    int offset = point - base_range.get_start();
    if (offset < 0) offset += base_range.get_N();
    my_assert(0 <= offset && offset < base_range.size());
    return offset;
}

// 責務: points に point を base_range 順で重複なしに挿入する。
// 必要な理由: direct 点や B2A 直前 AB 点を entry points へ sort なしで追加するため。
template<class Points>
void insert_circular_point_unique(Points &points, int point, const CircularRange &base_range) {
    if (!base_range.contains(point)) return;
    const int point_offset = calc_circular_point_offset(point, base_range);
    int insert_idx = 0;
    while (insert_idx < static_cast<int>(points.size())) {
        const int cur_point = points[insert_idx];
        my_assert(base_range.contains(cur_point));
        const int cur_offset = calc_circular_point_offset(cur_point, base_range);
        if (cur_offset == point_offset) return;
        if (point_offset < cur_offset) break;
        insert_idx++;
    }
    points.insert(points.begin() + insert_idx, point);
}

// 責務: points が base_range 内にあり、base_range.get_start() 基準の昇順 unique であることを assert する。
// 必要な理由: 円環順序は基準 range なしでは判定できないため、entry の不変条件を基準付きで確認するため。
template<class Points>
void assert_circular_points_ordered(const Points &points, const CircularRange &base_range) {
    int prev_offset = -1;
    for (int point: points) {
        my_assert(base_range.contains(point));
        const int offset = calc_circular_point_offset(point, base_range);
        my_assert(prev_offset < offset);
        prev_offset = offset;
    }
}

// 責務: base_range 上で 1 回だけ折り返す点列を、base_range.get_start() 基準の円環順序へ回転する。
// 必要な理由: full range などで B stack 走査の開始位置が base_range の開始位置と違っても、entry 点列を基準 range 順に保つため。
template<class Points>
EntryPointArray align_points_to_circular_range(const Points &points, const CircularRange &base_range) {
    if (points.empty()) return points;
    int wrap_idx = -1;
    int prev_offset = -1;
    for (int idx = 0; idx < static_cast<int>(points.size()); idx++) {
        const int offset = calc_circular_point_offset(points[idx], base_range);
        if (offset <= prev_offset) {
            my_assert(wrap_idx == -1);
            wrap_idx = idx;
        }
        prev_offset = offset;
    }
    if (wrap_idx == -1) return points;
    EntryPointArray aligned;
    for (int idx = wrap_idx; idx < static_cast<int>(points.size()); idx++) aligned.push_back(points[idx]);
    for (int idx = 0; idx < wrap_idx; idx++) aligned.push_back(points[idx]);
    return aligned;
}

// 責務: entry_points に registered_range の末尾点を加えた probe 点列を返す。
// 必要な理由: 最後の entry point より後ろの区間も limit 判定で少なくとも 1 点試すため。
template<class Points>
EntryPointArray build_limit_probe_points(const Points &entry_points, const CircularRange &registered_range) {
    EntryPointArray probe_points;
    for (int point: entry_points) probe_points.push_back(point);
    if (!registered_range.is_empty()) {
        insert_circular_point_unique(probe_points, get_last_value(registered_range), registered_range);
    }
    return probe_points;
}

// 責務: half-open all_ord ranges を読みやすい [begin,end) 形式へ整形する。
// 必要な理由: range 候補化後の output2/debug ログで点候補との差を追えるようにするため。
template<class OrdRanges>
string ord_ranges_to_debug_string(const OrdRanges &ranges) {
    string ret = "[";
    for (int i = 0; i < static_cast<int>(ranges.size()); i++) {
        if (i > 0) ret += ",";
        ret += "[" + std::to_string(ranges[i].begin) + "," + std::to_string(ranges[i].end) + ")";
    }
    ret += "]";
    return ret;
}

// 責務: task が指す pair_ranges の連続区間を [begin,end) 形式に整形する。
// 必要な理由: PairRangeTask が ord_ranges を内蔵しなくなった後も既存ログの情報量を保つため。
template<class Candidates>
string task_ord_ranges_to_debug_string(const Candidates &candidates, const PairRangeTask &task) {
    string ret = "[";
    for (int i = 0; i < task.range_count; i++) {
        if (i > 0) ret += ",";
        const OrdRange &range = candidates.pair_ranges[task.range_begin + i];
        ret += "[" + std::to_string(range.begin) + "," + std::to_string(range.end) + ")";
    }
    ret += "]";
    return ret;
}

// 責務: A2B/B2A entry と PairRangeTask を idx 昇順で greedy_a2b_b_range_trace.txt に出力する。
// 必要な理由: OrdRange が増える原因を、登録 range と区切り all_ord 点列から追えるようにするため。
void out_b_range_trace(
        int v,
        const RangeExt &range_ext,
        const A2BB2ACollectLimits &limits,
        const A2BB2ACollectWorkspace &workspace,
        const BasePairCandidates &candidates
) {
//    out_file(GREEDY_RANGE_TRACE_PATH,
//             "construct_b_range_trace_begin",
//             "v =", v,
//             ", query_count =", workspace.query_count,
//             ", range_ext_from =", range_ext.from_ext,
//             ", range_ext_to =", range_ext.to_ext,
//             ", a2b_ins_direct_lim =", limits.a2b_ins_direct_lim,
//             ", b2a_after_lim =", limits.b2a_after_lim);
    for (int q_idx = 0; q_idx <= workspace.query_count; q_idx++) {
        if (q_idx < workspace.query_count && workspace.a2b_entries[q_idx].exists) {
            const RangeEntry &entry = workspace.a2b_entries[q_idx];
//            out_file(GREEDY_RANGE_TRACE_PATH,
//                     "a2b_entry",
//                     "v =", v,
//                     ", a2b_idx =", q_idx,
//                     ", registered_range =", circular_range_to_debug_string(entry.registered_range.range),
//                     ", valid_all_ords =", all_ord_vector_to_debug_string(entry.valid_all_ords),
//                     ", valid_empty_zero =", entry.valid_empty_zero ? 1 : 0);
        }
        if (workspace.b2a_entries[q_idx].exists) {
            const RangeEntry &entry = workspace.b2a_entries[q_idx];
//            out_file(GREEDY_RANGE_TRACE_PATH,
//                     "b2a_entry",
//                     "v =", v,
//                     ", b2a_idx =", q_idx,
//                     ", registered_range =", circular_range_to_debug_string(entry.registered_range.range),
//                     ", valid_all_ords =", all_ord_vector_to_debug_string(entry.valid_all_ords),
//                     ", valid_empty_zero =", entry.valid_empty_zero ? 1 : 0);
        }
    }
    static vec_array<int, BASE_PAIR_TASK_CAPACITY> task_order;
    task_order.clear();
    for (int i = 0; i < static_cast<int>(candidates.pair_tasks.size()); i++) task_order.push_back(i);
    std::sort(task_order.begin(), task_order.end(), [&](int lhs_idx, int rhs_idx) {
        const PairRangeTask &lhs = candidates.pair_tasks[lhs_idx];
        const PairRangeTask &rhs = candidates.pair_tasks[rhs_idx];
        if (lhs.a2b_idx != rhs.a2b_idx) return lhs.a2b_idx < rhs.a2b_idx;
        return lhs.b2a_idx < rhs.b2a_idx;
    });
    for (int task_idx: task_order) {
        const PairRangeTask &task = candidates.pair_tasks[task_idx];
        const RangeEntry &a2b_entry = workspace.a2b_entries[task.a2b_idx];
        const RangeEntry &b2a_entry = workspace.b2a_entries[task.b2a_idx];
        CommonBRangeSet common_ranges =
                intersect_common_b_ranges(a2b_entry.registered_range.range, b2a_entry.registered_range.range);
//        out_file(GREEDY_RANGE_TRACE_PATH,
//                 "pair_range_task",
//                 "v =", v,
//                 ", a2b_idx =", task.a2b_idx,
//                 ", b2a_idx =", task.b2a_idx,
//                 ", common_ranges =", common_ranges_to_debug_string(common_ranges),
//                 ", ord_ranges =", ord_ranges_to_debug_string(task.ord_ranges));
    }
//    out_file(GREEDY_RANGE_TRACE_PATH,
//             "construct_b_range_trace_end",
//             "v =", v,
//             ", pair_task_count =", candidates.pair_tasks.size(),
//             ", ord_range_total =", candidates.ord_range_total_count);
}

// 責務: A2B entry が登録されたか、どの理由で落ちたかを range と split point 付きで出力する。
// 必要な理由: 旧候補 pair が新候補に存在しない原因を entry 段階から特定するため。
template<class SplitPoints>
void out_new_a2b_entry_decision(
        int v,
        int a2b_idx,
        const char *reason,
        const BAllOrdRange &provisional_range,
        const SplitPoints &split_points,
        bool pass_limit,
        bool registered,
        const BAllOrdRange &registered_range
) {
    // 責務: DEBUG 時だけ A2B entry decision の legacy 診断ログを出す。
    // 必要な理由: 通常実行では候補列挙に使わない debug string 生成を避けるため。
#ifdef DEBUG
    out_legacy_check_log("new_a2b_entry_decision",
                         "v =", v,
                         ", a2b_idx =", a2b_idx,
                         ", reason =", reason,
                         ", provisional_range =", circular_range_to_debug_string(provisional_range.range),
                         ", split_points =", int_vector_to_debug_string(split_points),
                         ", pass_limit =", pass_limit ? 1 : 0,
                         ", registered =", registered ? 1 : 0,
                         ", registered_range =", circular_range_to_debug_string(registered_range.range));
#else
    (void) v;
    (void) a2b_idx;
    (void) reason;
    (void) provisional_range;
    (void) split_points;
    (void) pass_limit;
    (void) registered;
    (void) registered_range;
#endif
}

// 責務: B2A entry が登録されたか、どの理由で落ちたかを range と split point 付きで出力する。
// 必要な理由: 旧候補 pair が新候補に存在しない原因を B2A entry 段階から特定するため。
template<class SplitPoints>
void out_new_b2a_entry_decision(
        int v,
        int b2a_idx,
        const char *reason,
        const BAllOrdRange &provisional_range,
        const SplitPoints &split_points,
        bool pass_limit,
        bool registered,
        const BAllOrdRange &registered_range
) {
    // 責務: DEBUG 時だけ B2A entry decision の legacy 診断ログを出す。
    // 必要な理由: 通常実行では候補列挙に使わない debug string 生成を避けるため。
#ifdef DEBUG
    out_legacy_check_log("new_b2a_entry_decision",
                         "v =", v,
                         ", b2a_idx =", b2a_idx,
                         ", reason =", reason,
                         ", provisional_range =", circular_range_to_debug_string(provisional_range.range),
                         ", split_points =", int_vector_to_debug_string(split_points),
                         ", pass_limit =", pass_limit ? 1 : 0,
                         ", registered =", registered ? 1 : 0,
                         ", registered_range =", circular_range_to_debug_string(registered_range.range));
#else
    (void) v;
    (void) b2a_idx;
    (void) reason;
    (void) provisional_range;
    (void) split_points;
    (void) pass_limit;
    (void) registered;
    (void) registered_range;
#endif
}

// 責務: A2B/B2A pair が task 登録されたか、どの理由で落ちたかを common range と ord range 付きで出力する。
// 必要な理由: 旧候補 pair が新候補に存在しない原因を pair 段階で特定するため。
template<class SplitPoints, class OrdRanges>
void out_new_pair_decision(
        int v,
        int a2b_idx,
        int b2a_idx,
        const char *reason,
        const BAllOrdRange &a2b_range,
        const BAllOrdRange &b2a_range,
        const CommonBRangeSet &common_ranges,
        const SplitPoints &split_points,
        bool pass_a2b_limit,
        bool pass_b2a_limit,
        const OrdRanges &ord_ranges,
        bool registered
) {
    // 責務: DEBUG 時だけ A2B/B2A pair decision の legacy 診断ログを出す。
    // 必要な理由: 通常実行では候補評価に使わない debug string 生成を避けるため。
#ifdef DEBUG
    out_legacy_check_log("new_pair_decision",
                         "v =", v,
                         ", a2b_idx =", a2b_idx,
                         ", b2a_idx =", b2a_idx,
                         ", reason =", reason,
                         ", a2b_range =", circular_range_to_debug_string(a2b_range.range),
                         ", b2a_range =", circular_range_to_debug_string(b2a_range.range),
                         ", common_ranges =", common_ranges_to_debug_string(common_ranges),
                         ", split_points =", int_vector_to_debug_string(split_points),
                         ", pass_a2b_limit =", pass_a2b_limit ? 1 : 0,
                         ", pass_b2a_limit =", pass_b2a_limit ? 1 : 0,
                         ", ord_ranges =", ord_ranges_to_debug_string(ord_ranges),
                         ", registered =", registered ? 1 : 0);
#else
    (void) v;
    (void) a2b_idx;
    (void) b2a_idx;
    (void) reason;
    (void) a2b_range;
    (void) b2a_range;
    (void) common_ranges;
    (void) split_points;
    (void) pass_a2b_limit;
    (void) pass_b2a_limit;
    (void) ord_ranges;
    (void) registered;
#endif
}

// 責務: 指定された片側 ext で stack idx range が full になるか判定する。
// 必要な理由: A 側と B 側で異なる ext を使っても既存の full 判定式を共有するため。
bool is_full_extended_range(int base_size, int stack_size, int from_ext, int to_ext)
{
    my_assert(1 <= base_size && base_size <= stack_size);
    return stack_size <= base_size + from_ext + to_ext;
}

CircularRange build_all_ord_range(AllOrd start, int size, int all_ord_size) {
    my_assert(1 <= size && size <= all_ord_size);
    my_assert(0 < all_ord_size);
    my_assert(0 <= start.get() && start.get() < all_ord_size);
    return CircularRange(start.get(), size, all_ord_size);
}

int calc_closed_all_ord_range_size(AllOrd start, AllOrd end, int all_ord_size) {
    int size = end.get() - start.get() + 1;
    if (size <= 0) size += all_ord_size;
    my_assert(size <= all_ord_size);
    return size;
}

CircularRange build_full_all_ord_range(int all_ord_size) {
    if (all_ord_size == 0) return CircularRange(0, 0, 0);
    return CircularRange(0, all_ord_size, all_ord_size);
}

// 責務: 指定された片側 ext で stack idx の候補範囲を作る。
// 必要な理由: A 側 range と B 側 range で別々の from/to 拡張幅を使うため。
CircularRange make_stack_idx_range(
        int stack_size,
        int to_idx,
        bool forward_dir,
        int from_ext,
        int to_ext
)
{
    my_assert(0 < stack_size);
    my_assert(0 <= to_idx && to_idx < stack_size);
    int base_size = forward_dir ? to_idx + 1 : stack_size - to_idx + 1;
    if (is_full_extended_range(base_size, stack_size, from_ext, to_ext)) {
        return CircularRange(0, stack_size, stack_size);
    }
    int start_idx = forward_dir
                    ? FinalRotCalculator::light_mod(-from_ext, stack_size)
                    : FinalRotCalculator::light_mod(to_idx - to_ext, stack_size);
    int end_inclusive_idx = forward_dir
                            ? FinalRotCalculator::light_mod(to_idx + to_ext, stack_size)
                            : FinalRotCalculator::light_mod(from_ext, stack_size);
    int size = end_inclusive_idx - start_idx + 1;
    if (size <= 0) size += stack_size;
    return CircularRange(start_idx, size, stack_size);
}

// 責務: A stack idx の base range から外側へ実際に歩き、free v は ext を消費せず候補範囲に含める。
// 必要な理由: A 側 from/to ext を小さくしても、今回 destroy されて未構築の v は 0 手で見られる候補として扱うため。
CircularRange make_stack_idx_range_with_free_a(
        int stack_size,
        int to_idx,
        bool forward_dir,
        int from_ext,
        int to_ext,
        const QueryScanState &scan_state,
        const my_bitset<MAX_N> &is_never_construct_v
) {
    my_assert(0 < stack_size);
    my_assert(stack_size == scan_state.st.A.size());
    my_assert(0 <= to_idx && to_idx < stack_size);
    int base_start_idx = forward_dir ? 0 : to_idx;
    int base_end_idx = forward_dir ? to_idx : 0;
    int base_size = forward_dir ? to_idx + 1 : stack_size - to_idx + 1;
    my_assert(1 <= base_size && base_size <= stack_size);
    auto is_free_idx = [&](int idx) {
        int v = scan_state.st.A[idx];
        return is_never_construct_v[v];
    };
    auto count_extension = [&](int edge_idx, int dir, int ext, int available) {
        int added = 0;
        int paid = 0;
        while (added < available) {
            int idx = FinalRotCalculator::light_mod(edge_idx + dir * (added + 1), stack_size);
            if (!is_free_idx(idx)) {
                if (paid >= ext) break;
                paid++;
            }
            added++;
        }
        return added;
    };
    int before_ext = forward_dir ? from_ext : to_ext;
    int after_ext = forward_dir ? to_ext : from_ext;
    int before_count = count_extension(base_start_idx, -1, before_ext, stack_size - base_size);
    int after_available = stack_size - base_size - before_count;
    int after_count = count_extension(base_end_idx, 1, after_ext, after_available);
    int size = base_size + before_count + after_count;
    if (size >= stack_size) return CircularRange(0, stack_size, stack_size);
    int start_idx = FinalRotCalculator::light_mod(base_start_idx - before_count, stack_size);
    return CircularRange(start_idx, size, stack_size);
}

// 責務: B stack の idx 候補範囲を作る。
// 必要な理由: A/B 別 ext 化後も empty B の扱いを変えず、非 empty B だけ B 用 ext で拡張するため。
BIdxRange make_b_idx_range(
        int stack_size,
        int to_idx,
        bool forward_dir,
        const RangeExt &range_ext
) {
    if (stack_size == 0) {
        return BIdxRange{CircularRange(0, 0, 0), true};
    }
    return BIdxRange{
            make_stack_idx_range(stack_size, to_idx, forward_dir, range_ext.from_ext_b, range_ext.to_ext_b),
            false
    };
}

//A側で使う時は、A側のall_ord取得関数を渡し
// B側で渡すときは逆側の関数を渡す
template<class GetAllOrdByPos>
CircularRange build_allord_range(
        const CircularRange &idx_range,
        int all_ord_size,
        GetAllOrdByPos get_all_ord_by_pos
) {
    if (idx_range.is_empty()) {
        outaaa("collector_allord_range_build",
               "case =", "empty",
               ", idx_range =", circular_range_to_debug_string(idx_range),
               ", all_ord_size =", all_ord_size,
               ", result =", circular_range_to_debug_string(CircularRange(0, 0, 0)));
        return CircularRange(0, 0, 0);
    }
    if (idx_range.is_full()) {
        CircularRange result = build_full_all_ord_range(all_ord_size);
        outaaa("collector_allord_range_build",
               "case =", "full",
               ", idx_range =", circular_range_to_debug_string(idx_range),
               ", all_ord_size =", all_ord_size,
               ", result =", circular_range_to_debug_string(result));
        return result;
    }
    int prev_start_idx = ChunkSeparator::add_circular(idx_range.get_start(), -1, idx_range.get_N());
    AllOrd prev_start = get_all_ord_by_pos(prev_start_idx);
    int included_start = ChunkSeparator::add_circular(prev_start.get(), 1, all_ord_size);
    int end_idx = ChunkSeparator::add_circular(idx_range.get_start(), idx_range.size() - 1, idx_range.get_N());
    AllOrd end = get_all_ord_by_pos(end_idx);
    int size = calc_closed_all_ord_range_size(AllOrd(included_start), end, all_ord_size);
    CircularRange result = build_all_ord_range(AllOrd(included_start), size, all_ord_size);
    outaaa("collector_allord_range_build",
           "case =", "non_full",
           ", idx_range =", circular_range_to_debug_string(idx_range),
           ", all_ord_size =", all_ord_size,
           ", prev_start_idx =", prev_start_idx,
           ", prev_start_all_ord =", prev_start.get(),
           ", included_start_all_ord =", included_start,
           ", end_idx =", end_idx,
           ", end_all_ord =", end.get(),
           ", result =", circular_range_to_debug_string(result));
    return result;
}

CircularRange make_previsional_a_ord_range(
        const YakinamashiState &yst,
        const QueryScanState &scan_state,
        const AIdxRange &a_idx_range
) {
    const int a_all_ord_size = yst.query_order.aorder.get_all_order_entries_size();
    CircularRange result = build_allord_range(
            a_idx_range.range,
            a_all_ord_size,
            [&](int idx) { return get_all_ord_a_by_pos(scan_state, yst, idx); }
    );
    outaaa("collector_previsional_a_ord_range",
           "a_idx_range =", circular_range_to_debug_string(a_idx_range.range),
           ", a_all_ord_size =", a_all_ord_size,
           ", result =", circular_range_to_debug_string(result));
    return result;
}

template<class Func>
void for_each_range_idx(const CircularRange &idx_range, Func func) {
    for (int offset = 0; offset < idx_range.size(); offset++) {
        int idx = ChunkSeparator::add_circular(idx_range.get_start(), offset, idx_range.get_N());
        func(idx);
    }
}

std::pair<int, bool> get_a_to_idx_and_dir(
        const QueriesState &queries_state,
        int q_idx,
        const QueryCommonRI &qri,
        int n
) {
    int stack_size = qri.get_stack_a_cnt();
    if (qri.is_a2a_type()) {
        bool is_forward = qri.get_flip_dis_dir_A2A() == 0 || qri.get_cmd_rot_a1_A2A() == RA;
        int rot_cnt = qri.get_cmd_rot_a1_cnt_A2A();
        int to_idx = is_forward ? rot_cnt : stack_size - rot_cnt;
        return {to_idx, is_forward};
    }
    OrdPos start_ord = get_query_start_ord_a(queries_state, q_idx, n);
    OrdPos tar_ord = get_query_tar_ord_a(qri);
    bool is_forward = qri.get_a_cmd_AB() == RA;
    int to_idx = OrdCalc::get_ord_pos_dec(tar_ord, start_ord, stack_size);
    return {to_idx, is_forward};
}

std::pair<int, bool> get_b_to_idx_and_dir(
        const QueriesState &queries_state,
        const QueryBoundaryArray &next_ab_query_idxs,
        int q_idx,
        const QueryCommonRI &qri,
        int n
) {
    int stack_size = qri.get_stack_b_cnt(n);
    if (qri.is_a2a_type()) {
        int ref_q_idx = next_ab_query_idxs[q_idx];
        if (ref_q_idx < 0) return {0, true};
        const QueryCommonRI &ref_qri = queries_state.queries[ref_q_idx];
        my_assert(ref_qri.is_ab_type());
        OrdPos start_ord = get_query_start_ord_b(queries_state, ref_q_idx, n);
        OrdPos tar_ord = get_query_tar_ord_b(ref_qri, n);
        bool is_forward = ref_qri.get_b_cmd_AB() == RB;
        int to_idx = OrdCalc::get_ord_pos_dec(tar_ord, start_ord, stack_size);
        return {to_idx, is_forward};
    }
    OrdPos start_ord = get_query_start_ord_b(queries_state, q_idx, n);
    OrdPos tar_ord = get_query_tar_ord_b(qri, n);
    bool is_forward = qri.get_b_cmd_AB() == RB;
    int to_idx = OrdCalc::get_ord_pos_dec(tar_ord, start_ord, stack_size);
    return {to_idx, is_forward};
}

// 責務: 1 つの query 境界に対する A/B の idx 候補範囲を作る。
// 必要な理由: A 側 range は A 用 ext、B 側 range は B 用 ext で構築するため。
AbIdxRange make_ab_idx_range(
        const YakinamashiState &yst,
        const QueryScanState &scan_state,
        const QueryBoundaryArray &next_ab_query_idxs,
        int q_idx,
        const QueryCommonRI &qri,
        const RangeExt &range_ext,
        const my_bitset<MAX_N> &is_never_construct_v,
        int n
) {
    AbIdxRange range;
    const QueryCommon &q = qri.get_query();
    int a_size = q.get_stack_a_cnt();
    if (a_size > 0) {
        auto [to_idx_a, is_forward_a] = get_a_to_idx_and_dir(yst.queries_state, q_idx, qri, n);
        range.a_idx_range = AIdxRange{
                make_stack_idx_range_with_free_a(
                        a_size, to_idx_a, is_forward_a,
                        range_ext.from_ext_a, range_ext.to_ext_a,
                        scan_state, is_never_construct_v)
        };
    } else {
        range.a_idx_range = AIdxRange{CircularRange(0, 0, 0)};
    }
    int b_size = q.get_stack_b_cnt(n);
    if (b_size > 0) {
        auto [to_idx_b, is_forward_b] = get_b_to_idx_and_dir(
                yst.queries_state, next_ab_query_idxs, q_idx, qri, n
        );
        range.b_idx_range = make_b_idx_range(b_size, to_idx_b, is_forward_b, range_ext);
    } else {
        range.b_idx_range = make_b_idx_range(0, 0, true, range_ext);
    }
    return range;
}

// 責務: query 末尾挿入位置に対する A/B の idx 候補範囲を作る。
// 必要な理由: 末尾位置の A 側候補範囲も A 用 ext で拡張し、B 側 empty の扱いは維持するため。
AbIdxRange make_final_ab_idx_range(
        const QueriesState &queries_state,
        const QueryScanState &scan_state,
        const RangeExt &range_ext,
        const my_bitset<MAX_N> &is_never_construct_v,
        int n
) {
    my_assert(0 < n);
    AbIdxRange range;
    const OrdPos start_ord_pos_a = queries_state.queries.empty()
                                   ? queries_state.first_top_ord_a
                                   : queries_state.queries.back().get_finished_top_ord_a();
    const OrdPos to_ord_pos_a = OrdPos(0);
    const int to_idx_a = OrdCalc::get_ord_pos_dec(to_ord_pos_a, start_ord_pos_a, n);
    const bool is_forward_a = queries_state.final_rot_info.first == RA;
    range.a_idx_range = AIdxRange{
            make_stack_idx_range_with_free_a(
                    n, to_idx_a, is_forward_a,
                    range_ext.from_ext_a, range_ext.to_ext_a,
                    scan_state, is_never_construct_v)
    };
    range.b_idx_range = make_b_idx_range(0, 0, true, range_ext);
    return range;
}


// 責務: 各 query 境界に対する AB 範囲を、前計算済み next_ab_idxs から構築する。
// 必要な理由: InsertPairPrecalc の next_ab_idxs を vec_array 化した後も候補列挙の意味を変えずに使うため。
my_vector<AbIdxRange> make_ab_idx_ranges(
        const YakinamashiState &yst,
        const State &init_st,
        const RangeExt &range_ext,
        const my_bitset<MAX_N> &is_never_construct_v,
        const QueryBoundaryArray &next_ab_query_idxs
) {
    const QueriesState &queries_state = yst.queries_state;
    QueriesCalculator qs_calc(init_st.current_N, queries_state.queries);
    QueryScanState scan_state = init_scan_state(yst, init_st);
    int query_count = static_cast<int>(queries_state.queries.size());
    my_vector<AbIdxRange> query_ab_idx_ranges(query_count + 1);
    for (int q_idx = 0; q_idx < query_count; q_idx++) {
        const QueryCommonRI &qri = queries_state.queries[q_idx];
        query_ab_idx_ranges[q_idx] = make_ab_idx_range(
                yst, scan_state, next_ab_query_idxs, q_idx, qri,
                range_ext, is_never_construct_v, init_st.current_N
        );
        advance_scan_state(yst, scan_state, qs_calc, q_idx, qri);
    }
    query_ab_idx_ranges[query_count] = make_final_ab_idx_range(
            queries_state, scan_state, range_ext, is_never_construct_v, init_st.current_N);
    return query_ab_idx_ranges;
}

// 責務: 候補なし retry で ext をまだ広げられるか判定する。
// 必要な理由: A/B 4つの ext を同時に広げる方針でも既存の停止条件を保つため。
bool can_retry_with_larger_ext(const RangeExt &range_ext, int state_N) {
    return range_ext.from_ext_a < state_N
           || range_ext.to_ext_a < state_N
           || range_ext.from_ext_b < state_N
           || range_ext.to_ext_b < state_N;
}

bool contains_init_v_in_a_range(
        const YakinamashiState &yst,
        const QueryScanState &scan_state,
        int v,
        const AbIdxRange &ab_range
) {
    ValState now_val_state = scan_state.now_a_val_state[v];
    if (now_val_state != ValState::INIT) {
        outaaa("collector_contains_init_a_range",
               "v =", v,
               ", now_val_state =", static_cast<int>(now_val_state),
               ", result =", 0,
               ", reason =", "not_init");
        return false;
    }
    CircularRange a_all_ord_range = make_previsional_a_ord_range(yst, scan_state, ab_range.a_idx_range);
    if (a_all_ord_range.is_empty()) {
        outaaa("collector_contains_init_a_range",
               "v =", v,
               ", now_val_state =", static_cast<int>(now_val_state),
               ", a_all_ord_range =", circular_range_to_debug_string(a_all_ord_range),
               ", result =", 0,
               ", reason =", "empty_range");
        return false;
    }
    AllOrd v_all_ord = yst.query_order.aorder.get_all_order(v, now_val_state);
    bool contains = a_all_ord_range.contains(v_all_ord.get());
    outaaa("collector_contains_init_a_range",
           "v =", v,
           ", now_val_state =", static_cast<int>(now_val_state),
           ", v_all_ord =", v_all_ord.get(),
           ", a_all_ord_range =", circular_range_to_debug_string(a_all_ord_range),
           ", result =", contains ? 1 : 0);
    return contains;
}

bool contains_pushed_all_ord_in_a_range(
        const YakinamashiState &yst,
        const QueryScanState &scan_state,
        int v,
        const AbIdxRange &ab_range
) {
    CircularRange a_all_ord_range = make_previsional_a_ord_range(yst, scan_state, ab_range.a_idx_range);
    if (a_all_ord_range.is_empty()) {
        outaaa("collector_contains_target_a_range",
               "v =", v,
               ", a_all_ord_range =", circular_range_to_debug_string(a_all_ord_range),
               ", result =", 0,
               ", reason =", "empty_range");
        return false;
    }
    AllOrd v_all_ord = yst.query_order.aorder.get_all_order(v, ValState::TARGET);
    bool contains = a_all_ord_range.contains(v_all_ord.get());
    outaaa("collector_contains_target_a_range",
           "v =", v,
           ", v_all_ord =", v_all_ord.get(),
           ", a_all_ord_range =", circular_range_to_debug_string(a_all_ord_range),
           ", result =", contains ? 1 : 0);
    return contains;
}

bool contains_pushed_v_in_a_range(
        const YakinamashiState &yst,
        const QueryScanState &scan_state,
        int v,
        const AbIdxRange &ab_range
) {
    return contains_pushed_all_ord_in_a_range(yst, scan_state, v, ab_range);
}

int count_a2b_direct_targets(int a2b_idx, const InsertPairPrecalc &precalc, int qcnt) {
    int direct1_idx = normalize_a2b_direct1_idx(a2b_idx, precalc, qcnt);
    int direct2_idx = normalize_a2b_direct2_idx(direct1_idx, a2b_idx, precalc, qcnt);
    int count = 0;
    if (direct1_idx != -1) count++;
    if (direct2_idx != -1) count++;
    return count;
}

// 責務: direct1 が AB なら direct1、direct1 が A2A なら direct2 の B target all_ord を range 内だけ追加する。
// 必要な理由: A2B 側 direct 差分が変わりうる all_ord 境界を common range 分割に反映するため。
EntryPointArray collect_a2b_direct_split_points(
        const YakinamashiState &yst,
        int a2b_idx,
        const CircularRange &range,
        const InsertPairPrecalc &precalc
) {
    EntryPointArray points;
    if (range.is_empty()) return points;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    int direct1_idx = normalize_a2b_direct1_idx(a2b_idx, precalc, qcnt);
    int direct2_idx = normalize_a2b_direct2_idx(direct1_idx, a2b_idx, precalc, qcnt);
    int target_idx = -1;
    if (direct1_idx != -1 && yst.queries_state.queries[direct1_idx].is_ab_type()) {
        target_idx = direct1_idx;
    } else if (direct2_idx != -1 && yst.queries_state.queries[direct2_idx].is_ab_type()) {
        target_idx = direct2_idx;
    }
    if (target_idx == -1) return points;
    int all_ord = get_tar_all_ord_b(yst, yst.queries_state.queries[target_idx]).get();
    if (range.contains(all_ord)) points.push_back(all_ord);
    return points;
}

// 責務: B range 由来点と A2B direct 点を registered_range の円環順序で返す。
// 必要な理由: pair merge 時に direct 点を追加せず、entry 点列同士の O(k) merge だけで range を作るため。
EntryPointArray collect_a2b_entry_points(
        const YakinamashiState &yst,
        const QueryScanState &scan_state,
        int a2b_idx,
        const BIdxRange &b_idx_range,
        const BAllOrdRange &registered_range,
        const InsertPairPrecalc &precalc
) {
    EntryPointArray points = align_points_to_circular_range(
            collect_b_range_all_ord_points(yst, scan_state, b_idx_range), registered_range.range);
    EntryPointArray direct_points = collect_a2b_direct_split_points(yst, a2b_idx, registered_range.range, precalc);
    for (int point: direct_points) insert_circular_point_unique(points, point, registered_range.range);
    assert_circular_points_ordered(points, registered_range.range);
    return points;
}

// 責務: split point のいずれかで A2B insert/direct cost が limit 以下なら true を返す。
// 必要な理由: 未合意の all_ord 点を増やさず、A2B 候補登録を大まかに絞るため。
template<class Points>
bool passes_a2b_limit_points(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        const Points &split_points,
        const A2BB2ACollectWorkspace &workspace,
        const InsertPairPrecalc &precalc
) {
    if (split_points.empty()) return true;
    for (int all_ord: split_points) {
        if (calc_a2b_insert_and_direct_diff(yst, init_st, v, a2b_idx, AllOrd(all_ord), precalc) <=
            workspace.limits.a2b_ins_direct_lim) {
            return true;
        }
    }
    return false;
}

bool is_a2b_all_ord_within_cost_limit(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        AllOrd pushed_b_all_ord,
        const A2BB2ACollectWorkspace &workspace,
        const InsertPairPrecalc &precalc
) {
    const int limit = workspace.limits.a2b_ins_direct_lim;
    const int cost = calc_a2b_insert_and_direct_diff(yst, init_st, v, a2b_idx, pushed_b_all_ord, precalc);
    return cost <= limit;
}

//fullではないという前提で、始点と終点から区間を作る
BAllOrdRange make_valid_range(const vector<AllOrd> &valid_all_ords, int all_ord_size) {
    my_assert(!valid_all_ords.empty());
    my_assert(0 < all_ord_size);
    const int start = valid_all_ords.front().get();
    const int last = valid_all_ords.back().get();
    const int size = calc_closed_all_ord_range_size(AllOrd(start), AllOrd(last), all_ord_size);
    return BAllOrdRange{CircularRange(start, size, all_ord_size)};
}

BAllOrdRange make_full_b_range(int all_ord_size) {
    return BAllOrdRange{build_full_all_ord_range(all_ord_size)};
}

BAllOrdRange make_previsional_b_ord_range(
        const YakinamashiState &yst,
        const QueryScanState &scan_state,
        const BIdxRange &b_idx_range,
        int b_all_ord_size
) {
    if (b_idx_range.means_full) {
        BAllOrdRange result = make_full_b_range(b_all_ord_size);
        outaaa("collector_previsional_b_ord_range",
               "b_idx_range =", circular_range_to_debug_string(b_idx_range.range),
               ", b_idx_means_full =", b_idx_range.means_full ? 1 : 0,
               ", b_all_ord_size =", b_all_ord_size,
               ", result =", circular_range_to_debug_string(result.range));
        return result;
    }
    CircularRange b_all_ord_range = build_allord_range(
            b_idx_range.range,
            b_all_ord_size,
            [&](int idx) { return get_all_ord_b_by_pos(scan_state, yst, idx); }
    );
    BAllOrdRange result = BAllOrdRange{b_all_ord_range};
    outaaa("collector_previsional_b_ord_range",
           "b_idx_range =", circular_range_to_debug_string(b_idx_range.range),
           ", b_idx_means_full =", b_idx_range.means_full ? 1 : 0,
           ", b_all_ord_size =", b_all_ord_size,
           ", result =", circular_range_to_debug_string(result.range));
    return result;
}

struct ValidOrdsInfo {
    vector<AllOrd> valid_all_ords;
    int candidate_count = 0;
    bool valid_empty_zero = false;
};

// 責務: 旧処理で A2B entry が登録されたか、どの理由で落ちたかを valid all_ord 付きで出力する。
// 必要な理由: 新旧の entry 作成条件の差を missing 候補の原因調査で比較するため。
void out_legacy_a2b_entry_decision(
        int v,
        int a2b_idx,
        const char *reason,
        const BAllOrdRange &provisional_range,
        const ValidOrdsInfo &collect_result,
        bool registered,
        const BAllOrdRange &registered_range
) {
    out_legacy_check_log("legacy_a2b_entry_decision",
                         "v =", v,
                         ", a2b_idx =", a2b_idx,
                         ", reason =", reason,
                         ", provisional_range =", circular_range_to_debug_string(provisional_range.range),
                         ", valid_all_ords =", all_ord_vector_to_debug_string(collect_result.valid_all_ords),
                         ", valid_empty_zero =", collect_result.valid_empty_zero ? 1 : 0,
                         ", candidate_count =", collect_result.candidate_count,
                         ", registered =", registered ? 1 : 0,
                         ", registered_range =", circular_range_to_debug_string(registered_range.range));
}

// 責務: 旧処理で B2A entry が登録されたか、どの理由で落ちたかを valid all_ord 付きで出力する。
// 必要な理由: 新旧の B2A entry 作成条件の差を missing 候補の原因調査で比較するため。
void out_legacy_b2a_entry_decision(
        int v,
        int b2a_idx,
        const char *reason,
        const BAllOrdRange &provisional_range,
        const ValidOrdsInfo &collect_result,
        bool registered,
        const BAllOrdRange &registered_range
) {
    out_legacy_check_log("legacy_b2a_entry_decision",
                         "v =", v,
                         ", b2a_idx =", b2a_idx,
                         ", reason =", reason,
                         ", provisional_range =", circular_range_to_debug_string(provisional_range.range),
                         ", valid_all_ords =", all_ord_vector_to_debug_string(collect_result.valid_all_ords),
                         ", valid_empty_zero =", collect_result.valid_empty_zero ? 1 : 0,
                         ", candidate_count =", collect_result.candidate_count,
                         ", registered =", registered ? 1 : 0,
                         ", registered_range =", circular_range_to_debug_string(registered_range.range));
}

// 責務: 旧処理で A2B/B2A pair が merge されたか、どの理由で落ちたかを common range と valid all_ord 付きで出力する。
// 必要な理由: 旧では候補化された pair が新方式で消えた原因を pair 段階で比較するため。
template<class A2BAllOrds, class B2AAllOrds>
void out_legacy_pair_decision(
        int v,
        int a2b_idx,
        int b2a_idx,
        const char *reason,
        const BAllOrdRange &a2b_range,
        const BAllOrdRange &b2a_range,
        const CommonBRangeSet &common_ranges,
        const A2BAllOrds &a2b_valid_all_ords,
        const B2AAllOrds &b2a_valid_all_ords,
        bool merged
) {
    out_legacy_check_log("legacy_pair_decision",
                         "v =", v,
                         ", a2b_idx =", a2b_idx,
                         ", b2a_idx =", b2a_idx,
                         ", reason =", reason,
                         ", a2b_range =", circular_range_to_debug_string(a2b_range.range),
                         ", b2a_range =", circular_range_to_debug_string(b2a_range.range),
                         ", common_ranges =", common_ranges_to_debug_string(common_ranges),
                         ", a2b_valid_all_ords =", all_ord_vector_to_debug_string(a2b_valid_all_ords),
                         ", b2a_valid_all_ords =", all_ord_vector_to_debug_string(b2a_valid_all_ords),
                         ", merged =", merged ? 1 : 0);
}

BAllOrdRange make_b_ord_range(
        const BAllOrdRange &provisional_range,
        const ValidOrdsInfo &collect_result
) {
    if (collect_result.valid_empty_zero) return provisional_range;
    if (provisional_range.range.is_full() &&
        collect_result.candidate_count == static_cast<int>(collect_result.valid_all_ords.size())) {
        return provisional_range;
    }
    return make_valid_range(collect_result.valid_all_ords, provisional_range.range.get_N());
}

ValidOrdsInfo collect_a2b_valid(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        const BIdxRange &b_idx_range,
        const QueryScanState &scan_state,
        const A2BB2ACollectWorkspace &workspace,
        const InsertPairPrecalc &precalc
) {
    ValidOrdsInfo result;
    if (b_idx_range.means_full && b_idx_range.range.is_empty()) {
        result.valid_empty_zero = is_a2b_all_ord_within_cost_limit(
                yst, init_st, v, a2b_idx, AllOrd(0), workspace, precalc);
        return result;
    }
    for_each_range_idx(b_idx_range.range, [&](int idx) {
        result.candidate_count++;
        AllOrd pushed_b_all_ord = get_all_ord_b_by_pos(scan_state, yst, idx);
        if (is_a2b_all_ord_within_cost_limit(
                yst, init_st, v, a2b_idx, pushed_b_all_ord, workspace, precalc)) {
            result.valid_all_ords.push_back(pushed_b_all_ord);
        }
    });
    return result;
}

void collect_and_register_a2b_range(
        A2BB2ACollectWorkspace &workspace,
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int a2b_idx,
        const AbIdxRange &a2b_ab_range,
        const QueryScanState &scan_state,
        const InsertPairPrecalc &precalc
) {
    if (!contains_init_v_in_a_range(yst, scan_state, v, a2b_ab_range)) {
#ifdef DEBUG
        out_new_a2b_entry_decision(v, a2b_idx, "no_init_v", BAllOrdRange{}, vector<int>{},
                                   false, false, BAllOrdRange{});
#endif
        return;
    }
    const BIdxRange &b_idx_range = a2b_ab_range.b_idx_range;
    if(b_idx_range.range.is_empty()){
        my_assert(b_idx_range.means_full);
    }
    BAllOrdRange provisional_b_range =
            make_previsional_b_ord_range(yst, scan_state, b_idx_range, workspace.b_all_ord_size);
    if (provisional_b_range.range.is_empty()) {
#ifdef DEBUG
        out_new_a2b_entry_decision(v, a2b_idx, "empty_provisional", provisional_b_range, vector<int>{},
                                   false, false, BAllOrdRange{});
#endif
        return;
    }
    my_assert(0 <= a2b_idx && a2b_idx < workspace.query_count);
    my_assert(provisional_b_range.range.get_N() == workspace.b_all_ord_size);

    // 責務: entry_points と range 末尾 probe 点で A2B 単体 limit を判定する。
    // 必要な理由: pair merge 後の range に依存せず、A2B 単体の代表点で候補を残すため。
    EntryPointArray entry_points = collect_a2b_entry_points(
            yst, scan_state, a2b_idx, b_idx_range, provisional_b_range, precalc);
    EntryPointArray probe_points = build_limit_probe_points(entry_points, provisional_b_range.range);
    bool pass_limit = passes_a2b_limit_points(yst, init_st, v, a2b_idx, probe_points, workspace, precalc);
    if (!pass_limit) {
#ifdef DEBUG
        out_new_a2b_entry_decision(v, a2b_idx, "limit_failed", provisional_b_range, probe_points,
                                   false, false, BAllOrdRange{});
#endif
        return;
    }
    BAllOrdRange registered_range = provisional_b_range;
#ifdef DEBUG
    out_new_a2b_entry_decision(v, a2b_idx, "registered", provisional_b_range, probe_points,
                               true, true, registered_range);
#endif
    outaaa("collector_a2b_valid",
           "a2b_idx =", a2b_idx,
           ", provisional_b_range =", circular_range_to_debug_string(provisional_b_range.range),
           ", candidate_b_idx_cnt =", 0,
           ", candidate_b_idx_range =", circular_range_to_debug_string(b_idx_range.range),
           ", b_idx_means_full =", b_idx_range.means_full ? 1 : 0,
           ", a2b_ins_direct_lim =", workspace.limits.a2b_ins_direct_lim,
           ", valid_cnt =", static_cast<int>(entry_points.size()),
           ", valid_all_ord_b =", int_vector_to_debug_string(entry_points),
           ", valid_empty_zero =", 0);
    outaaa("collector_a2b_registered",
           "a2b_idx =", a2b_idx,
           ", registered_range =", circular_range_to_debug_string(registered_range.range),
           ", register_full_range =", registered_range.range.is_full() ? 1 : 0,
           ", valid_empty_zero =", 0);
    RangeEntry &entry = workspace.a2b_entries[a2b_idx];
    entry.exists = true;
    entry.registered_range = registered_range;
    begin_entry_valid_all_ords(workspace, entry);
    // 責務: direct 点を含む entry_points を RangeEntry に保存する。
    // 必要な理由: pair range 作成時に common range 順の O(k) merge を可能にするため。
    for (int all_ord: entry_points) append_entry_valid_all_ord(workspace, entry, AllOrd(all_ord));
    entry.valid_empty_zero = false;
    workspace.a2b_range_segtree.add_circular_range(registered_range.range, a2b_idx);
}

int normalize_b2a_direct1_idx(int b2a_idx, const InsertPairPrecalc &precalc, int qcnt) {
    int direct1_idx = precalc.next_a2b_b2a_effected_valid_q_idx[b2a_idx];
    if (direct1_idx == qcnt) return -1;
    return direct1_idx;
}

int normalize_b2a_direct2_idx(int direct1_idx, int b2a_idx, const InsertPairPrecalc &precalc, int qcnt) {
    if (direct1_idx == -1) return -1;
    int direct2_idx = precalc.next_ab_idxs[b2a_idx];
    if (direct2_idx == direct1_idx || direct2_idx == qcnt) return -1;
    return direct2_idx;
}

int count_b2a_direct_targets(int b2a_idx, const InsertPairPrecalc &precalc, int qcnt) {
    int direct1_idx = normalize_b2a_direct1_idx(b2a_idx, precalc, qcnt);
    int direct2_idx = normalize_b2a_direct2_idx(direct1_idx, b2a_idx, precalc, qcnt);
    int count = 0;
    if (direct1_idx != -1) count++;
    if (direct2_idx != -1) count++;
    return count;
}

// 責務: direct1 が AB なら direct1、direct1 が A2A なら direct2 の B target all_ord を range 内だけ追加する。
// 必要な理由: B2A 側 direct 差分が変わりうる all_ord 境界を common range 分割に反映するため。
EntryPointArray collect_b2a_direct_split_points(
        const YakinamashiState &yst,
        int b2a_idx,
        const CircularRange &range,
        const InsertPairPrecalc &precalc
) {
    EntryPointArray points;
    if (range.is_empty()) return points;
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    int direct1_idx = normalize_b2a_direct1_idx(b2a_idx, precalc, qcnt);
    int direct2_idx = normalize_b2a_direct2_idx(direct1_idx, b2a_idx, precalc, qcnt);
    int target_idx = -1;
    if (direct1_idx != -1 && yst.queries_state.queries[direct1_idx].is_ab_type()) {
        target_idx = direct1_idx;
    } else if (direct2_idx != -1 && yst.queries_state.queries[direct2_idx].is_ab_type()) {
        target_idx = direct2_idx;
    }
    if (target_idx == -1) return points;
    int all_ord = get_tar_all_ord_b(yst, yst.queries_state.queries[target_idx]).get();
    if (range.contains(all_ord)) points.push_back(all_ord);
    return points;
}

// 責務: q_idx より前を左へ走査し、最初の A2B/B2A query index を返す。
// 必要な理由: B2A 直前 AB 境界は valid query ではなく元 query 列の直前 AB で決まるため。
int find_prev_ab_idx(const YakinamashiState &yst, int q_idx) {
    const int qcnt = static_cast<int>(yst.queries_state.queries.size());
    my_assert(0 <= q_idx && q_idx <= qcnt);
    for (int cur_idx = q_idx - 1; cur_idx >= 0; cur_idx--) {
        if (yst.queries_state.queries[cur_idx].is_ab_type()) return cur_idx;
    }
    return -1;
}

// 責務: B range 由来点、B2A direct 点、直前 AB が B2A の all_ord を registered_range の円環順序で返す。
// 必要な理由: B2A start_ord_b が変わる境界を entry 側に含め、pair merge を entry 点列同士だけにするため。
EntryPointArray collect_b2a_entry_points(
        const YakinamashiState &yst,
        const QueryScanState &scan_state,
        int b2a_idx,
        const BIdxRange &b_idx_range,
        const BAllOrdRange &registered_range,
        const InsertPairPrecalc &precalc
) {
    EntryPointArray points = align_points_to_circular_range(
            collect_b_range_all_ord_points(yst, scan_state, b_idx_range), registered_range.range);
    EntryPointArray direct_points = collect_b2a_direct_split_points(yst, b2a_idx, registered_range.range, precalc);
    for (int point: direct_points) insert_circular_point_unique(points, point, registered_range.range);

    const int prev_ab_idx = find_prev_ab_idx(yst, b2a_idx);
    if (prev_ab_idx != -1 && yst.queries_state.queries[prev_ab_idx].is_b2a_type()) {
        const int prev_b2a_all_ord = get_tar_all_ord_b(yst, yst.queries_state.queries[prev_ab_idx]).get();
        insert_circular_point_unique(points, prev_b2a_all_ord, registered_range.range);
    }
    assert_circular_points_ordered(points, registered_range.range);
    return points;
}

// 責務: split point のいずれかで B2A after cost が limit 以下なら true を返す。
// 必要な理由: 未合意の all_ord 点を増やさず、B2A 候補登録を大まかに絞るため。
template<class Points>
bool passes_b2a_limit_points(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int b2a_idx,
        const Points &split_points,
        const A2BB2ACollectWorkspace &workspace,
        const BandRangeFenwick &active_bands,
        const InsertPairPrecalc &precalc
) {
    if (split_points.empty()) return true;
    for (int all_ord: split_points) {
        B2AAfterCost cost = calc_b2a_after_cost(
                yst, init_st, v, b2a_idx, AllOrd(all_ord), active_bands, precalc);
        if (cost.total <= workspace.limits.b2a_after_lim) return true;
    }
    return false;
}

// 責務: linear 半開区間 [begin,end) に point が含まれるか返す。
// 必要な理由: common range を最大 2 本の linear range に分け、円環順序を通常の整数昇順として扱うため。
bool is_point_in_linear_range(int point, const OrdRange &range) {
    return range.begin <= point && point < range.end;
}

// 責務: entry.valid_all_ords から linear_range 内の点を取り出し、sort せず折り返しだけ補正して返す。
// 必要な理由: full range などで点列の開始位置が linear_range.begin と違う場合も O(k) merge できる順序にするため。
EntryPointArray collect_entry_points_in_linear_range(const RangeEntry &entry, const OrdRange &linear_range) {
    EntryPointArray filtered;
    for (AllOrd all_ord: entry.valid_all_ords()) {
        const int point = all_ord.get();
        if (is_point_in_linear_range(point, linear_range)) filtered.push_back(point);
    }
    if (filtered.empty()) return filtered;
    int wrap_idx = -1;
    for (int idx = 1; idx < static_cast<int>(filtered.size()); idx++) {
        if (filtered[idx] < filtered[idx - 1]) {
            wrap_idx = idx;
            break;
        }
    }
    EntryPointArray ordered;
    if (wrap_idx == -1) {
        for (int point: filtered) {
            if (ordered.empty() || ordered.back() != point) ordered.push_back(point);
        }
        return ordered;
    }
    for (int idx = wrap_idx; idx < static_cast<int>(filtered.size()); idx++) {
        if (ordered.empty() || ordered.back() != filtered[idx]) ordered.push_back(filtered[idx]);
    }
    for (int idx = 0; idx < wrap_idx; idx++) {
        if (ordered.empty() || ordered.back() != filtered[idx]) ordered.push_back(filtered[idx]);
    }
    return ordered;
}

// 責務: int の entry point 列から linear_range 内の点を取り出し、折り返しだけ補正して返す。
// 必要な理由: RangeEntry の valid all_ord を外部 arena 化した後も、owner split 用の一時点列で vector を使わないため。
EntryPointArray collect_entry_point_values_in_linear_range(const EntryPointArray &entry_points, const OrdRange &linear_range) {
    EntryPointArray filtered;
    for (int point: entry_points) {
        if (is_point_in_linear_range(point, linear_range)) filtered.push_back(point);
    }
    if (filtered.empty()) return filtered;
    int wrap_idx = -1;
    for (int idx = 1; idx < static_cast<int>(filtered.size()); idx++) {
        if (filtered[idx] < filtered[idx - 1]) {
            wrap_idx = idx;
            break;
        }
    }
    EntryPointArray ordered;
    if (wrap_idx == -1) {
        for (int point: filtered) {
            if (ordered.empty() || ordered.back() != point) ordered.push_back(point);
        }
        return ordered;
    }
    for (int idx = wrap_idx; idx < static_cast<int>(filtered.size()); idx++) {
        if (ordered.empty() || ordered.back() != filtered[idx]) ordered.push_back(filtered[idx]);
    }
    for (int idx = 0; idx < wrap_idx; idx++) {
        if (ordered.empty() || ordered.back() != filtered[idx]) ordered.push_back(filtered[idx]);
    }
    return ordered;
}

// 責務: linear 昇順 unique の points で linear_range を分割し、空でない OrdRange を out に追加する。
// 必要な理由: split_points vector を pair 全体で sort せず、common range 順に評価 range を作るため。
template<class OrdRangeContainer, class Points>
void append_ranges_by_points(
        OrdRangeContainer &out,
        const OrdRange &linear_range,
        const Points &points
) {
    int current = linear_range.begin;
    for (int point: points) {
        if (!is_point_in_linear_range(point, linear_range)) continue;
        const int next = point + 1;
        if (current < next) out.push_back(OrdRange{current, next});
        current = next;
    }
    if (current < linear_range.end) out.push_back(OrdRange{current, linear_range.end});
}

// 責務: 2 つの linear 昇順点列を重複なしで merge し、linear_range を OrdRange 列へ分割する。
// 必要な理由: common range 内の節点を std::sort なしで range 順に処理するため。
template<class OrdRanges, class PointsA, class PointsB>
void append_pair_ord_ranges_for_linear_range(
        OrdRanges &out,
        const OrdRange &linear_range,
        const PointsA &a2b_points,
        const PointsB &b2a_points
) {
    EntryPointArray merged_points;
    int a_idx = 0, b_idx = 0;
    while (a_idx < static_cast<int>(a2b_points.size()) ||
           b_idx < static_cast<int>(b2a_points.size())) {
        int next_point = INT_MAX;
        if (a_idx < static_cast<int>(a2b_points.size())) next_point = std::min(next_point, a2b_points[a_idx]);
        if (b_idx < static_cast<int>(b2a_points.size())) next_point = std::min(next_point, b2a_points[b_idx]);
        if (next_point == INT_MAX) break;
        if (merged_points.empty() || merged_points.back() != next_point) merged_points.push_back(next_point);
        while (a_idx < static_cast<int>(a2b_points.size()) && a2b_points[a_idx] == next_point) a_idx++;
        while (b_idx < static_cast<int>(b2a_points.size()) && b2a_points[b_idx] == next_point) b_idx++;
    }
    append_ranges_by_points(out, linear_range, merged_points);
}

// 責務: common_ranges の各 linear 断片で A2B/B2A entry 点を merge し、pair の評価 OrdRange 列を返す。
// 必要な理由: A2B/B2A 共通 all_ord を sort せず、見る点数 k に対して O(k) で候補 range を作るため。
vec_array<OrdRange, MAX_INSERT_EVENT_B> build_pair_ord_ranges_linear(
        const CommonBRangeSet &common_ranges,
        const RangeEntry &a2b_entry,
        const RangeEntry &b2a_entry
) {
    vec_array<OrdRange, MAX_INSERT_EVENT_B> ord_ranges;
    for (int range_i = 0; range_i < common_ranges.count; range_i++) {
        LinearRangeArray linear_ranges = split_circular_range(common_ranges.ranges[range_i]);
        for (const OrdRange &linear_range: linear_ranges) {
            EntryPointArray a2b_points = collect_entry_points_in_linear_range(a2b_entry, linear_range);
            EntryPointArray b2a_points = collect_entry_points_in_linear_range(b2a_entry, linear_range);
            append_pair_ord_ranges_for_linear_range(ord_ranges, linear_range, a2b_points, b2a_points);
        }
    }
    return ord_ranges;
}

bool is_b2a_all_ord_within_cost_limit(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int b2a_idx,
        AllOrd pushed_b_all_ord,
        const A2BB2ACollectWorkspace &workspace,
        const BandRangeFenwick &active_bands,
        const InsertPairPrecalc &precalc
) {
    B2AAfterCost cost = calc_b2a_after_cost(
            yst, init_st, v, b2a_idx, pushed_b_all_ord, active_bands, precalc);
    return cost.total <= workspace.limits.b2a_after_lim;
}

ValidOrdsInfo collect_b2a_valid(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int b2a_idx,
        const BIdxRange &b_idx_range,
        const QueryScanState &scan_state,
        const A2BB2ACollectWorkspace &workspace,
        const BandRangeFenwick &active_bands,
        const InsertPairPrecalc &precalc
) {
    ValidOrdsInfo result;
    if (b_idx_range.means_full && b_idx_range.range.is_empty()) {
        result.valid_empty_zero = is_b2a_all_ord_within_cost_limit(
                yst, init_st, v, b2a_idx, AllOrd(0), workspace, active_bands, precalc);
        return result;
    }
    for_each_range_idx(b_idx_range.range, [&](int idx) {
        result.candidate_count++;
        AllOrd pushed_b_all_ord = get_all_ord_b_by_pos(scan_state, yst, idx);
        if (is_b2a_all_ord_within_cost_limit(
                yst, init_st, v, b2a_idx, pushed_b_all_ord, workspace, active_bands, precalc)) {
            result.valid_all_ords.push_back(pushed_b_all_ord);
        }
    });
    return result;
}

RangeEntry build_b2a_range_entry(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int b2a_idx,
        const AbIdxRange &ab_range,
        const QueryScanState &scan_state,
        A2BB2ACollectWorkspace &workspace,
        const BandRangeFenwick &active_bands,
        const InsertPairPrecalc &precalc
) {
    RangeEntry entry;
    const BIdxRange &b_idx_range = ab_range.b_idx_range;
    if (b_idx_range.range.is_empty()){
        my_assert(b_idx_range.means_full);
    }
    BAllOrdRange provisional_allord_b_range =
            make_previsional_b_ord_range(yst, scan_state, b_idx_range, workspace.b_all_ord_size);
    my_assert(!provisional_allord_b_range.range.is_empty());
    // 責務: entry_points と range 末尾 probe 点で B2A 単体 limit を判定する。
    // 必要な理由: pair merge 後の range に依存せず、B2A 単体の代表点で候補を残すため。
    EntryPointArray entry_points = collect_b2a_entry_points(
            yst, scan_state, b2a_idx, b_idx_range, provisional_allord_b_range, precalc);
    EntryPointArray probe_points = build_limit_probe_points(entry_points, provisional_allord_b_range.range);
    bool pass_limit =
            passes_b2a_limit_points(yst, init_st, v, b2a_idx, probe_points, workspace, active_bands, precalc);
    if (!pass_limit) {
#ifdef DEBUG
        out_new_b2a_entry_decision(v, b2a_idx, "limit_failed", provisional_allord_b_range, probe_points,
                                   false, false, BAllOrdRange{});
#endif
        return entry;
    }
    outaaa("collector_b2a_valid",
           "b2a_idx =", b2a_idx,
           ", provisional_allord_b_range =", circular_range_to_debug_string(provisional_allord_b_range.range),
           ", candidate_b_idx_cnt =", 0,
           ", candidate_b_idx_range =", circular_range_to_debug_string(b_idx_range.range),
           ", b_idx_means_full =", b_idx_range.means_full ? 1 : 0,
           ", b2a_after_lim =", workspace.limits.b2a_after_lim,
           ", valid_cnt =", static_cast<int>(entry_points.size()),
           ", valid_all_ord_b =", int_vector_to_debug_string(entry_points),
           ", valid_empty_zero =", 0);
    entry.exists = true;
    entry.registered_range = provisional_allord_b_range;
#ifdef DEBUG
    out_new_b2a_entry_decision(v, b2a_idx, "registered", provisional_allord_b_range, probe_points,
                               true, true, entry.registered_range);
#endif
    outaaa("collector_b2a_registered",
           "b2a_idx =", b2a_idx,
           ", registered_range =", circular_range_to_debug_string(entry.registered_range.range),
           ", register_full_range =", entry.registered_range.range.is_full() ? 1 : 0,
           ", valid_empty_zero =", 0);
    // 責務: direct 点と直前 B2A 境界を含む entry_points を RangeEntry に保存する。
    // 必要な理由: pair range 作成時に common range 順の O(k) merge を可能にするため。
    begin_entry_valid_all_ords(workspace, entry);
    for (int all_ord: entry_points) append_entry_valid_all_ord(workspace, entry, AllOrd(all_ord));
    entry.valid_empty_zero = false;
    return entry;
}

bool collect_b2a_candidates(
        A2BB2ACollectWorkspace &workspace,
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        int b2a_idx,
        const QueryScanState &scan_state,
        const AbIdxRange &ab_range,
        const BandRangeFenwick &active_bands,
        const InsertPairPrecalc &precalc,
        BasePairCandidates &out,
        int &candidate_num
) {
    if (!contains_pushed_v_in_a_range(yst, scan_state, v, ab_range)) {
#ifdef DEBUG
        out_new_b2a_entry_decision(v, b2a_idx, "no_pushed_v", BAllOrdRange{}, vector<int>{},
                                   false, false, BAllOrdRange{});
#endif
        return false;
    }
    if (ab_range.b_idx_range.range.is_empty()){
        my_assert(ab_range.b_idx_range.means_full);
    }
    RangeEntry b2a_entry = build_b2a_range_entry(
            yst, init_st, v, b2a_idx, ab_range, scan_state, workspace, active_bands, precalc);
    if (!b2a_entry.exists) return false;
    workspace.b2a_entries[b2a_idx] = b2a_entry;
    bool appended = false;
    my_vector<int> candidate_a2b_idxs =
            workspace.a2b_range_segtree.collect_circular_range(b2a_entry.registered_range.range);
    vec_array<int, MAX_QUERY_CNT> candidate_a2b_idx_vec;
    if (is_collector_log_enabled()) {
        for (int candidate_a2b_idx: candidate_a2b_idxs) candidate_a2b_idx_vec.push_back(candidate_a2b_idx);
    }
    int b2a_candidate_num_before = candidate_num;
    if (is_collector_log_enabled() && !candidate_a2b_idx_vec.empty()) {
        ::out2("b2a_idx =", b2a_idx,
               ", matched_a2b_idxs =", int_vector_to_debug_string(candidate_a2b_idx_vec));
    }
    outaaa("collector_b2a_candidate_a2b_idxs",
           "b2a_idx =", b2a_idx,
           ", b2a_registered_range =", circular_range_to_debug_string(b2a_entry.registered_range.range),
           ", candidate_a2b_cnt =", static_cast<int>(candidate_a2b_idx_vec.size()),
           ", candidate_a2b_idxs =", int_vector_to_debug_string(candidate_a2b_idx_vec));
    for (int a2b_idx: candidate_a2b_idxs) {
        my_assert (a2b_idx < b2a_idx);
        const RangeEntry &a2b_entry = workspace.a2b_entries[a2b_idx];
        my_assert(a2b_entry.exists);
        CommonBRangeSet common_ranges =
                intersect_common_b_ranges(a2b_entry.registered_range.range, b2a_entry.registered_range.range);
        outaaa("collector_pair_common_range",
               "a2b_idx =", a2b_idx,
               ", b2a_idx =", b2a_idx,
               ", a2b_registered_range =", circular_range_to_debug_string(a2b_entry.registered_range.range),
               ", b2a_registered_range =", circular_range_to_debug_string(b2a_entry.registered_range.range),
               ", common_count =", common_ranges.count,
               ", common_ranges =", common_ranges_to_debug_string(common_ranges));
        if (common_ranges.count == 0) {
#ifdef DEBUG
            out_new_pair_decision(v, a2b_idx, b2a_idx, "no_common_range",
                                  a2b_entry.registered_range, b2a_entry.registered_range,
                                  common_ranges, vector<int>{}, false, false, vector<OrdRange>{}, false);
#endif
            continue;
        }
        //WAS_AI_REV, split_pointsをもとめるひつようはあるのか
        //common_ranges, a2b_entry, b2a_entryからちょくせつ、vector<OrderRange>をもとめればいいのでは
        //もとめかたは他の所にも書いたが、O(a2b_entry_size + b2a_entry_size)でおこなえる
        //a2b_entry, b2a_entryはそれぞれのくかんについてのえんかんじゅんじょになっているひつようがある
        //a2b_entry, b2a_entryをふたつどうじにせんとうからみていって、いまみてるくかんにかんけいあるものをさいようしていく
        //mergeソートのマージのようりょう
        //円環の切れ目に注意
        //前回の端点が8でみてるa2b_entryのあたいが10, b2a_entryのあたいが12だったら、10のほうをとって、a2b_entryのみるidxを1すすめるみたいなそれだけのこと
        //かくsplito_pointsについて手数を求めてlimitをみたすかのちぇっくをいままでやっていたが、
        //それはあたらしくもとめたvector<orderRange>のかくorderrangeについて代表点（startのあたいでいい）をためせばいいので
        //split_pointsをもとめるひつようはないきがしている
        vec_array<OrdRange, MAX_INSERT_EVENT_B> ord_ranges =
                build_pair_ord_ranges_linear(common_ranges, a2b_entry, b2a_entry);
#ifdef DEBUG
        vec_array<int, MAX_INSERT_EVENT_B> range_begin_points;
        for (const OrdRange &range: ord_ranges) range_begin_points.push_back(range.begin);
#endif
        if (ord_ranges.empty()) {
#ifdef DEBUG
            out_new_pair_decision(v, a2b_idx, b2a_idx, "empty_ord_ranges",
                                  a2b_entry.registered_range, b2a_entry.registered_range,
                                  common_ranges, range_begin_points, false, false, ord_ranges, false);
#endif
            continue;
        }
        append_pair_range_task(out, a2b_idx, b2a_idx, ord_ranges);
        out.ord_range_total_count += static_cast<int>(ord_ranges.size());
        candidate_num += static_cast<int>(ord_ranges.size());
        appended = true;
#ifdef DEBUG
        out_new_pair_decision(v, a2b_idx, b2a_idx, "registered",
                              a2b_entry.registered_range, b2a_entry.registered_range,
                              common_ranges, range_begin_points, false, false, ord_ranges, true);
#endif
        if (is_collector_log_enabled()) {
            vec_array<int, MAX_INSERT_EVENT_B> range_begin_points;
            for (const OrdRange &range: ord_ranges) range_begin_points.push_back(range.begin);
            outaaa("collector_pair_range_task",
                   "a2b_idx =", a2b_idx,
                   ", b2a_idx =", b2a_idx,
                   ", range_begin_points =", int_vector_to_debug_string(range_begin_points),
                   ", ord_ranges =", ord_ranges_to_debug_string(ord_ranges));
            ::out2("b2a_idx =", b2a_idx);
            ::out2("  a2b_idx =", a2b_idx,
                   ", common_b_ranges =", common_ranges_to_debug_string(common_ranges),
                   ", range_begin_points =", int_vector_to_debug_string(range_begin_points),
                   ", ord_ranges =", ord_ranges_to_debug_string(ord_ranges));
        }
    }
    if (is_collector_log_enabled() && !candidate_a2b_idx_vec.empty()) {
        ::out2("  b2a_candidate_total =", candidate_num - b2a_candidate_num_before,
               ", b2a_idx =", b2a_idx);
    }
    return appended;
}

// 責務: 1 つの range_ext から pair candidate を列挙する。
// 必要な理由: next_ab_idxs の保持先だけ vec_array 化しても候補列挙側がそのまま参照できるようにするため。
bool collect_candidates_for_range_ext(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const RangeExt &range_ext,
        const QueryBoundaryArray &next_ab_idxs,
        const my_bitset<MAX_N> &is_never_construct_v,
        BasePairCandidates &out,
        int &candidate_num,
        const InsertPairPrecalc &precalc,
        A2BB2ACollectLimits limits
) {
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
        if (collect_b2a_candidates(
                workspace, yst, init_st, v, q_idx, scan_state, ab_range,
                active_bands, precalc, out, candidate_num)) {
            appended = true;
        }
        add_band_range_to_fenwick(active_bands, precalc.a2b_band_ranges[q_idx], -1);
        collect_and_register_a2b_range(
                workspace, yst, init_st, v, q_idx, ab_range, scan_state, precalc);
        advance_scan_state(yst, scan_state, qs_calc, q_idx, qri);
    }
    const int final_b2a_idx = query_count;
    const AbIdxRange &final_ab_range = query_ab_ranges[final_b2a_idx];
    if (collect_b2a_candidates(
            workspace, yst, init_st, v, final_b2a_idx, scan_state, final_ab_range,
            active_bands, precalc, out, candidate_num)) {
        appended = true;
    }
    out_b_range_trace(v, range_ext, limits, workspace, out);
    return appended;
}

// 責務: 現在存在する A/B の値集合から v の前後値を探し、prev_v の直後から next_v の直前までの挿入 slot 範囲を返す。
CircularRange build_a2a_all_ord_range(
        const YakinamashiState &yst,
        const State &current_st,
        int v
) {
    const int entry_count = yst.query_order.aorder.get_all_order_entries_size();
    const int insert_slot_count = entry_count + 1;
    if (current_st.current_N <= 1) return CircularRange(0, insert_slot_count, insert_slot_count);
    const int n = current_st.initial_N;
    int prev_v = -1;
    int next_v = -1;
    int best_prev_dist = INT_MAX;
    int best_next_dist = INT_MAX;
    auto update_neighbors_from_stack = [&](const auto &stack) {
        for (int i = 0; i < stack.size(); i++) {
            int stack_v = stack[i];
            if (stack_v == v) continue;
            const int prev_dist = circular_value_forward_dist(stack_v, v, n);
            if (prev_dist < best_prev_dist) {
                best_prev_dist = prev_dist;
                prev_v = stack_v;
            }
            const int next_dist = circular_value_forward_dist(v, stack_v, n);
            if (next_dist < best_next_dist) {
                best_next_dist = next_dist;
                next_v = stack_v;
            }
        }
    };
    update_neighbors_from_stack(current_st.A);
    update_neighbors_from_stack(current_st.B);
    if (prev_v == -1 || next_v == -1) return CircularRange(0, insert_slot_count, insert_slot_count);
    const AllOrd prev_all_ord = yst.query_order.aorder.get_all_order(prev_v, ValState::TARGET);
    const AllOrd next_all_ord = yst.query_order.aorder.get_all_order(next_v, ValState::TARGET);
    const int start = prev_all_ord.get() + 1;
    const int last = next_all_ord.get();
    int size = last - start + 1;
    if (size <= 0) size += insert_slot_count;
    my_assert(1 <= size && size <= insert_slot_count);
    return CircularRange(start, size, insert_slot_count);
}

// 責務: AOrder の TARGET v を仮更新・shift し、query rebuild 後に A2A を挿入して差分を返す。
A2ACandidate eval_a2a_candidate(
        const YakinamashiState &yst,
        const State &current_st,
        int v,
        int q_idx,
        AllOrd a_all_ord
) {
    YakinamashiState eval_yst = yst;
    eval_yst.query_order.aorder.insert_update_shift(v, a_all_ord);
    rebuild_queries_for_state(current_st, eval_yst);
    QueryScanState scan_state = build_scan_state_before_q_idx(eval_yst, current_st, q_idx);
    QueryCommon a2a_query = build_eval_a2a_query(
            eval_yst, scan_state, v, q_idx, e_a2a_type::swap_rot, current_st.current_N);
    insert_query_common(eval_yst, current_st, q_idx, a2a_query);
    A2ACandidate result;
    result.found = true;
    result.q_idx = q_idx;
    result.a_all_ord = a_all_ord;
    result.diff = eval_yst.queries_state.sum_turn - yst.queries_state.sum_turn;
    return result;
}

// 責務: 全 q_idx と shift 前 AOrder 挿入 slot を試し、sum_turn 差分が最小の A2ACandidate を返す。
A2ACandidate collect_a2a_candidates(
        const YakinamashiState &yst,
        const State &current_st,
        int v
) {
    const int query_count = static_cast<int>(yst.queries_state.queries.size());
    A2ACandidate best;
    for (int q_idx = 0; q_idx <= query_count; q_idx++) {
        QueryScanState scan_state = build_scan_state_before_q_idx(yst, current_st, q_idx);
        CircularRange a_all_ord_range = build_a2a_all_ord_range(yst, current_st, v);
        A2ACandidate q_best;
        for_each_all_ord_in_circular_range(a_all_ord_range, [&](AllOrd a_all_ord) {
            A2ACandidate candidate = eval_a2a_candidate(yst, current_st, v, q_idx, a_all_ord);
            if (!q_best.found || candidate.diff < q_best.diff) q_best = candidate;
            if (!best.found || candidate.diff < best.diff) best = candidate;
        });
        out_a2a_candidate_log(
                "a2a_collect_q",
                "v", v,
                "q_idx", q_idx,
                "range_start", a_all_ord_range.get_start(),
                "range_size", a_all_ord_range.size(),
                "q_best_found", q_best.found ? 1 : 0,
                "q_best_all_ord", q_best.found ? q_best.a_all_ord.get() : -1,
                "q_best_diff", q_best.found ? q_best.diff : 0);
    }
    out_a2a_candidate_log(
            "a2a_collect_best",
            "v", v,
            "found", best.found ? 1 : 0,
            "q_idx", best.q_idx,
            "a_all_ord", best.found ? best.a_all_ord.get() : -1,
            "diff", best.found ? best.diff : 0);
    return best;
}

// 責務: 1 つの v に対する pair candidate を通常経路で列挙する。
// 必要な理由: A/B 別 ext を使いながら、候補なし retry では4値すべてを広げるため。
void collect_candidates(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const InsertPairPrecalc &precalc,
        const GreedyConstructorParams &params,
        const my_bitset<MAX_N> &is_never_construct_v,
        BasePairCandidates &out
) {
    RangeExt range_ext = RangeExt{params.from_ext_a, params.to_ext_a, params.from_ext_b, params.to_ext_b};
    A2BB2ACollectLimits current_limits = A2BB2ACollectLimits{params.a2b_lim, params.b2a_lim};
    auto next_ab_idxs = build_next_ab_idxs(yst);
    int loop_cnt = 0;
    int candidate_num = 0;
    while (true) {
        loop_cnt++;
        if (is_collector_log_enabled()) {
            ::out2();
            ::out2("v =", v,
                   ", loop_cnt =", loop_cnt,
                   ", a2b_ins_direct_lim =", current_limits.a2b_ins_direct_lim,
                   ", b2a_after_lim =", current_limits.b2a_after_lim);
        }
        outaaa("collector_retry_begin",
               "loop_cnt =", loop_cnt,
               ", range_ext_from_a =", range_ext.from_ext_a,
               ", range_ext_to_a =", range_ext.to_ext_a,
               ", range_ext_from_b =", range_ext.from_ext_b,
               ", range_ext_to_b =", range_ext.to_ext_b,
               ", a2b_ins_direct_lim =", current_limits.a2b_ins_direct_lim,
               ", b2a_after_lim =", current_limits.b2a_after_lim);
        bool appended = collect_candidates_for_range_ext(
                yst, init_st, v, range_ext, next_ab_idxs, is_never_construct_v,
                out, candidate_num, precalc, current_limits);
        outaaa("collector_retry_end",
               "loop_cnt =", loop_cnt,
               ", appended =", appended ? 1 : 0,
               ", candidate_num =", candidate_num);
        if (appended) break;
        if (!can_retry_with_larger_ext(range_ext, init_st.current_N)) break;
        range_ext = RangeExt{
                range_ext.from_ext_a + 1,
                range_ext.to_ext_a + 1,
                range_ext.from_ext_b + 1,
                range_ext.to_ext_b + 1
        };
        widen_collect_limits(current_limits);
    }
    outaaa("collect_candidate loop_cnt ", loop_cnt, ", candidate num ", candidate_num);
    if (is_collector_log_enabled()) {
        ::out2("v_candidate_total =", candidate_num, ", v =", v);
        ::out2();
    }
}

// 責務: 旧 collect_a2b_valid / collect_b2a_valid / merge_valid_allords 経路で比較用 BasePairCandidates を作る。
// 必要な理由: 新 PairRangeTask が旧候補を包含しているか、旧 best より悪い候補を選んでいないかを調べるため。
// 責務: 旧経路との比較用 pair candidate を列挙する。
// 必要な理由: legacy 比較でも通常 collector と同じ A/B 別 ext 条件で候補を作るため。
void collect_candidates_legacy_check(
        const YakinamashiState &yst,
        const State &init_st,
        int v,
        const InsertPairPrecalc &precalc,
        const GreedyConstructorParams &params,
        const my_bitset<MAX_N> &is_never_construct_v,
        LegacyBasePairCandidates &out
) {
    RangeExt range_ext = RangeExt{params.from_ext_a, params.to_ext_a, params.from_ext_b, params.to_ext_b};
    A2BB2ACollectLimits current_limits = A2BB2ACollectLimits{params.a2b_lim, params.b2a_lim};
    auto next_ab_idxs = build_next_ab_idxs(yst);
    int loop_cnt = 0;
    int candidate_num = 0;
    while (true) {
        loop_cnt++;
        my_vector<AbIdxRange> query_ab_ranges =
                make_ab_idx_ranges(yst, init_st, range_ext, is_never_construct_v, next_ab_idxs);
        QueriesCalculator qs_calc(init_st.current_N, yst.queries_state.queries);
        QueryScanState scan_state = init_scan_state(yst, init_st);
        const int query_count = static_cast<int>(yst.queries_state.queries.size());
        A2BB2ACollectWorkspace workspace;
        init_a2b_b2a_collect_workspace(
                workspace,
                query_count,
                yst.query_order.border.get_all_order_entries_size(),
                current_limits
        );
        BandRangeFenwick active_bands;
        const int b_insert_slot_count = calc_insert_slot_count(workspace.b_all_ord_size);
        active_bands.init(b_insert_slot_count);
        for (const BandRangeInfo &band: precalc.a2b_band_ranges) {
            add_band_range_to_fenwick(active_bands, band, 1);
        }
        bool appended = false;
        auto register_a2b_legacy = [&](int a2b_idx, const AbIdxRange &a2b_ab_range) {
            if (!contains_init_v_in_a_range(yst, scan_state, v, a2b_ab_range)) {
                out_legacy_a2b_entry_decision(v, a2b_idx, "no_init_v", BAllOrdRange{}, ValidOrdsInfo{},
                                              false, BAllOrdRange{});
                return;
            }
            const BIdxRange &b_idx_range = a2b_ab_range.b_idx_range;
            BAllOrdRange provisional_b_range =
                    make_previsional_b_ord_range(yst, scan_state, b_idx_range, workspace.b_all_ord_size);
            if (provisional_b_range.range.is_empty()) {
                out_legacy_a2b_entry_decision(v, a2b_idx, "empty_provisional", provisional_b_range,
                                              ValidOrdsInfo{}, false, BAllOrdRange{});
                return;
            }
            ValidOrdsInfo collect_result = collect_a2b_valid(
                    yst, init_st, v, a2b_idx, b_idx_range, scan_state, workspace, precalc);
            if (!collect_result.valid_empty_zero && collect_result.valid_all_ords.empty()) {
                out_legacy_a2b_entry_decision(v, a2b_idx, "no_valid_all_ord", provisional_b_range,
                                              collect_result, false, BAllOrdRange{});
                return;
            }
            BAllOrdRange registered_range = make_b_ord_range(provisional_b_range, collect_result);
            out_legacy_a2b_entry_decision(v, a2b_idx, "registered", provisional_b_range,
                                          collect_result, true, registered_range);
            RangeEntry &entry = workspace.a2b_entries[a2b_idx];
            entry.exists = true;
            entry.registered_range = registered_range;
            begin_entry_valid_all_ords(workspace, entry);
            for (AllOrd all_ord: collect_result.valid_all_ords) append_entry_valid_all_ord(workspace, entry, all_ord);
            entry.valid_empty_zero = collect_result.valid_empty_zero;
            workspace.a2b_range_segtree.add_circular_range(registered_range.range, a2b_idx);
        };
        auto collect_b2a_legacy = [&](int b2a_idx, const AbIdxRange &ab_range) {
            if (!contains_pushed_v_in_a_range(yst, scan_state, v, ab_range)) {
                out_legacy_b2a_entry_decision(v, b2a_idx, "no_pushed_v", BAllOrdRange{}, ValidOrdsInfo{},
                                              false, BAllOrdRange{});
                return false;
            }
            const BIdxRange &b_idx_range = ab_range.b_idx_range;
            BAllOrdRange provisional_allord_b_range =
                    make_previsional_b_ord_range(yst, scan_state, b_idx_range, workspace.b_all_ord_size);
            if (provisional_allord_b_range.range.is_empty()) {
                out_legacy_b2a_entry_decision(v, b2a_idx, "empty_provisional", provisional_allord_b_range,
                                              ValidOrdsInfo{}, false, BAllOrdRange{});
                return false;
            }
            ValidOrdsInfo collect_result = collect_b2a_valid(
                    yst, init_st, v, b2a_idx, b_idx_range, scan_state,
                    workspace, active_bands, precalc);
            if (!collect_result.valid_empty_zero && collect_result.valid_all_ords.empty()) {
                out_legacy_b2a_entry_decision(v, b2a_idx, "no_valid_all_ord", provisional_allord_b_range,
                                              collect_result, false, BAllOrdRange{});
                return false;
            }
            RangeEntry b2a_entry;
            b2a_entry.exists = true;
            b2a_entry.registered_range = make_b_ord_range(provisional_allord_b_range, collect_result);
            begin_entry_valid_all_ords(workspace, b2a_entry);
            for (AllOrd all_ord: collect_result.valid_all_ords) append_entry_valid_all_ord(workspace, b2a_entry, all_ord);
            b2a_entry.valid_empty_zero = collect_result.valid_empty_zero;
            out_legacy_b2a_entry_decision(v, b2a_idx, "registered", provisional_allord_b_range,
                                          collect_result, true, b2a_entry.registered_range);
            bool b2a_appended = false;
            my_vector<int> candidate_a2b_idxs =
                    workspace.a2b_range_segtree.collect_circular_range(b2a_entry.registered_range.range);
            for (int a2b_idx: candidate_a2b_idxs) {
                my_assert(a2b_idx < b2a_idx);
                const RangeEntry &a2b_entry = workspace.a2b_entries[a2b_idx];
                my_assert(a2b_entry.exists);
                CommonBRangeSet common_ranges =
                        intersect_common_b_ranges(a2b_entry.registered_range.range, b2a_entry.registered_range.range);
                AllOrdArenaView a2b_valid_all_ords = a2b_entry.valid_all_ords();
                AllOrdArenaView b2a_valid_all_ords = b2a_entry.valid_all_ords();
                if (common_ranges.count == 0) {
                    out_legacy_pair_decision(v, a2b_idx, b2a_idx, "no_common_range",
                                             a2b_entry.registered_range, b2a_entry.registered_range,
                                             common_ranges,
                                             a2b_valid_all_ords,
                                             b2a_valid_all_ords,
                                             false);
                    continue;
                }
                if (merge_valid_allords(
                        out, yst, init_st, v, a2b_idx, b2a_idx, common_ranges,
                        a2b_entry, b2a_entry, active_bands, precalc, candidate_num)) {
                    out_legacy_pair_decision(v, a2b_idx, b2a_idx, "merged",
                                             a2b_entry.registered_range, b2a_entry.registered_range,
                                             common_ranges,
                                             a2b_valid_all_ords,
                                             b2a_valid_all_ords,
                                             true);
                    b2a_appended = true;
                } else {
                    out_legacy_pair_decision(v, a2b_idx, b2a_idx, "merge_failed",
                                             a2b_entry.registered_range, b2a_entry.registered_range,
                                             common_ranges,
                                             a2b_valid_all_ords,
                                             b2a_valid_all_ords,
                                             false);
                }
            }
            return b2a_appended;
        };
        for (int q_idx = 0; q_idx < query_count; q_idx++) {
            const QueryCommonRI &qri = yst.queries_state.queries[q_idx];
            const AbIdxRange &ab_range = query_ab_ranges[q_idx];
            if (collect_b2a_legacy(q_idx, ab_range)) appended = true;
            add_band_range_to_fenwick(active_bands, precalc.a2b_band_ranges[q_idx], -1);
            register_a2b_legacy(q_idx, ab_range);
            advance_scan_state(yst, scan_state, qs_calc, q_idx, qri);
        }
        const int final_b2a_idx = query_count;
        const AbIdxRange &final_ab_range = query_ab_ranges[final_b2a_idx];
        if (collect_b2a_legacy(final_b2a_idx, final_ab_range)) appended = true;
        outaaa("legacy_check_retry_end",
               "loop_cnt =", loop_cnt,
               ", appended =", appended ? 1 : 0,
               ", candidate_num =", candidate_num);
        if (appended) break;
        if (!can_retry_with_larger_ext(range_ext, init_st.current_N)) break;
        range_ext = RangeExt{
                range_ext.from_ext_a + 1,
                range_ext.to_ext_a + 1,
                range_ext.from_ext_b + 1,
                range_ext.to_ext_b + 1
        };
        widen_collect_limits(current_limits);
    }
    outaaa("legacy_check_candidate_total",
           "loop_cnt =", loop_cnt,
           ", candidate_num =", candidate_num);
}
