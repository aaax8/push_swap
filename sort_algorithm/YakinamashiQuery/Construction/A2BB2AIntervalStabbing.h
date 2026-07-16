#pragma once

#include "sort_algorithm/InitBeam/CircularRange.h"
#include "util.h"

#include <array>

template<size_t MAX_POINT>
class A2BB2AIntervalStabbing {
private:
    static constexpr int calc_tree_size() {
        int sz = 1;
        while (sz < static_cast<int>(MAX_POINT)) sz <<= 1;
        return sz;
    }

    static constexpr int SZ = calc_tree_size();
    static constexpr int NODE_COUNT = SZ * 2;

    int point_count = 0;
    std::array<my_vector<int>, NODE_COUNT> node_ids;
    std::array<my_vector<int>, MAX_POINT> point_paths;

    void validate_point_count(int n) const {
        my_assert(0 <= n && n <= static_cast<int>(MAX_POINT));
    }

    void validate_point(int x) const {
        my_assert(0 <= x && x < point_count);
    }

    void validate_range(int l, int r) const {
        my_assert(0 <= l && l <= r && r <= point_count);
    }

    void build_point_paths() {
        for (int x = 0; x < point_count; x++) {
            point_paths[x].clear();
            int node = x + SZ;
            while (node > 0) {
                point_paths[x].push_back(node);
                node >>= 1;
            }
        }
    }

public:
    A2BB2AIntervalStabbing() = default;

    explicit A2BB2AIntervalStabbing(int n) {
        init(n);
    }

    void init(int n) {
        validate_point_count(n);
        point_count = n;
        clear_ids();
        build_point_paths();
    }

    void clear_ids() {
        for (auto &ids: node_ids) ids.clear();
    }

    void clear() {
        clear_ids();
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
        int left = l + SZ;
        int right = r + SZ;
        while (left < right) {
            if (left & 1) node_ids[left++].push_back(id);
            if (right & 1) node_ids[--right].push_back(id);
            left >>= 1;
            right >>= 1;
        }
    }

    void add_circular_range(int start, int len, int id) {
        validate_point_count(point_count);
        my_assert(point_count > 0);
        my_assert(0 <= start && start < point_count);
        my_assert(0 <= len && len <= point_count);
        if (len == 0) return;
        if (len == point_count) {
            node_ids[1].push_back(id);
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

    template<class Func>
    void query_point(int x, Func on_id) const {
        validate_point(x);
        for (int node: point_paths[x]) {
            for (int id: node_ids[node]) {
                on_id(id);
            }
        }
    }

    my_vector<int> collect_point(int x) const {
        my_vector<int> result;
        query_point(x, [&](int id) {
            result.push_back(id);
        });
        return result;
    }
};

