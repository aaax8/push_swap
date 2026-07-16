#pragma once

#include "sort_algorithm/InitBeam/CircularRange.h"
#include "util.h"

#include <array>
#include <bit>
#include <cstdint>

template<size_t MAX_BIT>
class A2BB2AFixedBitset {
private:
    static constexpr size_t WORD_BITS = 64;
    static constexpr size_t WORD_COUNT = (MAX_BIT + WORD_BITS - 1) / WORD_BITS;

    std::array<uint64_t, WORD_COUNT> words{};

public:
    void reset() {
        for (uint64_t &word: words) word = 0;
    }

    bool empty() const {
        for (uint64_t word: words) {
            if (word != 0) return false;
        }
        return true;
    }

    void set(int bit) {
        my_assert(0 <= bit && bit < static_cast<int>(MAX_BIT));
        words[bit >> 6] |= (uint64_t(1) << (bit & 63));
    }

    bool test(int bit) const {
        my_assert(0 <= bit && bit < static_cast<int>(MAX_BIT));
        return (words[bit >> 6] >> (bit & 63)) & uint64_t(1);
    }

    void or_assign(const A2BB2AFixedBitset &rhs) {
        for (size_t i = 0; i < WORD_COUNT; i++) words[i] |= rhs.words[i];
    }

    static A2BB2AFixedBitset single_bit(int bit) {
        A2BB2AFixedBitset res;
        res.set(bit);
        return res;
    }

    template<class Func>
    void for_each_set_bit(Func func) const {
        for (size_t wi = 0; wi < WORD_COUNT; wi++) {
            uint64_t word = words[wi];
            while (word != 0) {
                const int bit = std::countr_zero(word);
                const int idx = static_cast<int>(wi * WORD_BITS + bit);
                if (idx < static_cast<int>(MAX_BIT)) func(idx);
                word &= word - 1;
            }
        }
    }
};

template<size_t MAX_POINT, size_t MAX_ID>
class A2BB2ARangeSetLazySegTree {
public:
    using Bitset = A2BB2AFixedBitset<MAX_ID>;

private:
    static constexpr int calc_tree_size() {
        int sz = 1;
        while (sz < static_cast<int>(MAX_POINT)) sz <<= 1;
        return sz;
    }

    static constexpr int calc_log() {
        int lg = 0;
        int sz = 1;
        while (sz < static_cast<int>(MAX_POINT)) {
            sz <<= 1;
            lg++;
        }
        return lg;
    }

    static constexpr int SZ = calc_tree_size();
    static constexpr int LOG = calc_log();
    static constexpr int NODE_COUNT = SZ * 2;

    int point_count = 0;
    std::array<Bitset, NODE_COUNT> data;
    std::array<Bitset, NODE_COUNT> lazy;

    void validate_point_count(int n) const {
        my_assert(0 <= n && n <= static_cast<int>(MAX_POINT));
    }

    void validate_range(int l, int r) const {
        my_assert(0 <= l && l <= r && r <= point_count);
    }

    void validate_id(int id) const {
        my_assert(0 <= id && id < static_cast<int>(MAX_ID));
    }

    void all_apply(int node, const Bitset &value) {
        data[node].or_assign(value);
        if (node < SZ) lazy[node].or_assign(value);
    }

    void push(int node) {
        if (lazy[node].empty()) return;
        all_apply(node << 1, lazy[node]);
        all_apply(node << 1 | 1, lazy[node]);
        lazy[node].reset();
    }

    void pull(int node) {
        data[node] = data[node << 1];
        data[node].or_assign(data[node << 1 | 1]);
    }

public:
    A2BB2ARangeSetLazySegTree() = default;

    explicit A2BB2ARangeSetLazySegTree(int n) {
        init(n);
    }

    void init(int n) {
        validate_point_count(n);
        point_count = n;
        clear();
    }

    void clear() {
        for (Bitset &bits: data) bits.reset();
        for (Bitset &bits: lazy) bits.reset();
    }

    int size() const {
        return point_count;
    }

    static constexpr int tree_size() {
        return SZ;
    }

    static constexpr int node_count() {
        return NODE_COUNT;
    }

    void add_range(int l, int r, int id) {
        validate_range(l, r);
        validate_id(id);
        if (l == r) return;

        Bitset value = Bitset::single_bit(id);
        l += SZ;
        r += SZ;
        const int l0 = l;
        const int r0 = r;

        for (int i = LOG; i >= 1; i--) {
            if (((l0 >> i) << i) != l0) push(l0 >> i);
            if (((r0 >> i) << i) != r0) push((r0 - 1) >> i);
        }

        while (l < r) {
            if (l & 1) all_apply(l++, value);
            if (r & 1) all_apply(--r, value);
            l >>= 1;
            r >>= 1;
        }

        for (int i = 1; i <= LOG; i++) {
            if (((l0 >> i) << i) != l0) pull(l0 >> i);
            if (((r0 >> i) << i) != r0) pull((r0 - 1) >> i);
        }
    }

    void add_circular_range(int start, int len, int id) {
        validate_point_count(point_count);
        my_assert(point_count > 0);
        my_assert(0 <= start && start < point_count);
        my_assert(0 <= len && len <= point_count);
        if (len == 0) return;
        if (len == point_count) {
            add_range(0, point_count, id);
            return;
        }
        const int end = start + len;
        if (end <= point_count) {
            add_range(start, end, id);
        } else {
            add_range(start, point_count, id);
            add_range(0, end - point_count, id);
        }
    }

    void add_circular_range(const ChunkSeparator::CircularRange &range, int id) {
        my_assert(range.get_N() == point_count);
        add_circular_range(range.get_start(), range.size(), id);
    }

    Bitset query_range(int l, int r) {
        validate_range(l, r);
        Bitset left_res;
        Bitset right_res;
        if (l == r) return left_res;

        l += SZ;
        r += SZ;
        const int l0 = l;
        const int r0 = r;

        for (int i = LOG; i >= 1; i--) {
            if (((l0 >> i) << i) != l0) push(l0 >> i);
            if (((r0 >> i) << i) != r0) push((r0 - 1) >> i);
        }

        while (l < r) {
            if (l & 1) left_res.or_assign(data[l++]);
            if (r & 1) right_res.or_assign(data[--r]);
            l >>= 1;
            r >>= 1;
        }
        left_res.or_assign(right_res);
        return left_res;
    }

    Bitset query_circular_range(int start, int len) {
        validate_point_count(point_count);
        my_assert(point_count > 0);
        my_assert(0 <= start && start < point_count);
        my_assert(0 <= len && len <= point_count);
        if (len == 0) return Bitset();
        if (len == point_count) return query_range(0, point_count);
        const int end = start + len;
        if (end <= point_count) return query_range(start, end);
        Bitset res = query_range(start, point_count);
        res.or_assign(query_range(0, end - point_count));
        return res;
    }

    Bitset query_circular_range(const ChunkSeparator::CircularRange &range) {
        my_assert(range.get_N() == point_count);
        return query_circular_range(range.get_start(), range.size());
    }

    my_vector<int> collect_range(int l, int r) {
        Bitset bits = query_range(l, r);
        my_vector<int> result;
        bits.for_each_set_bit([&](int id) {
            result.push_back(id);
        });
        return result;
    }

    my_vector<int> collect_circular_range(const ChunkSeparator::CircularRange &range) {
        Bitset bits = query_circular_range(range);
        my_vector<int> result;
        bits.for_each_set_bit([&](int id) {
            result.push_back(id);
        });
        return result;
    }
};

