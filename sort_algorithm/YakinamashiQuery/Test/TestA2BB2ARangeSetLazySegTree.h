#pragma once

#include "all_include.h"
#include "sort_algorithm/YakinamashiQuery/Construction/A2BB2ARangeSetLazySegTree.h"
#include "util.h"

#include <algorithm>
#include <random>

namespace TestA2BB2ARangeSetLazySegTreeDetail {
    template<size_t MAX_ID>
    class NaiveRangeSet {
    public:
        explicit NaiveRangeSet(int n) : point_ids(n) {}

        void add_range(int l, int r, int id) {
            my_assert(0 <= l && l <= r && r <= size());
            my_assert(0 <= id && id < static_cast<int>(MAX_ID));
            for (int i = l; i < r; i++) {
                point_ids[i].push_back(id);
            }
        }

        void add_circular_range(int start, int len, int id) {
            my_assert(size() > 0);
            my_assert(0 <= start && start < size());
            my_assert(0 <= len && len <= size());
            my_assert(0 <= id && id < static_cast<int>(MAX_ID));
            for (int i = 0; i < len; i++) {
                point_ids[(start + i) % size()].push_back(id);
            }
        }

        my_vector<int> collect_range(int l, int r) const {
            my_assert(0 <= l && l <= r && r <= size());
            my_vector<int> result;
            for (int i = l; i < r; i++) {
                result.insert(result.end(), point_ids[i].begin(), point_ids[i].end());
            }
            normalize(result);
            return result;
        }

        my_vector<int> collect_circular_range(int start, int len) const {
            my_assert(size() > 0);
            my_assert(0 <= start && start < size());
            my_assert(0 <= len && len <= size());
            my_vector<int> result;
            for (int i = 0; i < len; i++) {
                const auto &ids = point_ids[(start + i) % size()];
                result.insert(result.end(), ids.begin(), ids.end());
            }
            normalize(result);
            return result;
        }

    private:
        my_vector<my_vector<int>> point_ids;

        int size() const {
            return static_cast<int>(point_ids.size());
        }

        static void normalize(my_vector<int> &ids) {
            std::sort(ids.begin(), ids.end());
            ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        }
    };

    template<size_t MAX_POINT, size_t MAX_ID>
    void assert_same(my_vector<int> actual, my_vector<int> expected) {
        std::sort(actual.begin(), actual.end());
        std::sort(expected.begin(), expected.end());
        my_assert(actual == expected);
    }

    template<size_t MAX_POINT, size_t MAX_ID>
    void run_linear_case(int point_count, std::mt19937 &engine) {
        A2BB2ARangeSetLazySegTree<MAX_POINT, MAX_ID> seg(point_count);
        NaiveRangeSet<MAX_ID> naive(point_count);

        constexpr int OP_COUNT = 200;
        std::uniform_int_distribution<int> id_dist(0, static_cast<int>(MAX_ID) - 1);
        std::uniform_int_distribution<int> point_dist(0, point_count);
        std::uniform_int_distribution<int> op_dist(0, 1);

        for (int op_i = 0; op_i < OP_COUNT; op_i++) {
            int l = point_dist(engine);
            int r = point_dist(engine);
            if (l > r) std::swap(l, r);

            if (op_dist(engine) == 0) {
                int id = id_dist(engine);
                seg.add_range(l, r, id);
                naive.add_range(l, r, id);
            } else {
                assert_same<MAX_POINT, MAX_ID>(seg.collect_range(l, r), naive.collect_range(l, r));
            }
        }

        for (int l = 0; l <= point_count; l++) {
            for (int r = l; r <= point_count; r++) {
                assert_same<MAX_POINT, MAX_ID>(seg.collect_range(l, r), naive.collect_range(l, r));
            }
        }
    }

    template<size_t MAX_POINT, size_t MAX_ID>
    void run_circular_case(int point_count, std::mt19937 &engine) {
        A2BB2ARangeSetLazySegTree<MAX_POINT, MAX_ID> seg(point_count);
        NaiveRangeSet<MAX_ID> naive(point_count);

        constexpr int OP_COUNT = 200;
        std::uniform_int_distribution<int> id_dist(0, static_cast<int>(MAX_ID) - 1);
        std::uniform_int_distribution<int> start_dist(0, point_count - 1);
        std::uniform_int_distribution<int> len_dist(0, point_count);
        std::uniform_int_distribution<int> op_dist(0, 1);

        for (int op_i = 0; op_i < OP_COUNT; op_i++) {
            int start = start_dist(engine);
            int len = len_dist(engine);

            if (op_dist(engine) == 0) {
                int id = id_dist(engine);
                seg.add_circular_range(start, len, id);
                naive.add_circular_range(start, len, id);
            } else {
                assert_same<MAX_POINT, MAX_ID>(
                    seg.collect_circular_range(ChunkSeparator::CircularRange(start, len, point_count)),
                    naive.collect_circular_range(start, len)
                );
            }
        }

        for (int start = 0; start < point_count; start++) {
            for (int len = 0; len <= point_count; len++) {
                assert_same<MAX_POINT, MAX_ID>(
                    seg.collect_circular_range(ChunkSeparator::CircularRange(start, len, point_count)),
                    naive.collect_circular_range(start, len)
                );
            }
        }
    }
}

inline void test_a2b_b2a_range_set_lazy_seg_tree() {
    constexpr size_t MAX_POINT = 128;
    constexpr size_t MAX_ID = 256;
    constexpr int CASE_COUNT = 1000;
    constexpr uint32_t SEED = 2026050201;

    std::mt19937 engine(SEED);
    std::uniform_int_distribution<int> point_count_dist(1, static_cast<int>(MAX_POINT));

    for (int case_i = 0; case_i < CASE_COUNT; case_i++) {
        cout << "case_i"<<case_i << endl;
        int point_count = point_count_dist(engine);
        TestA2BB2ARangeSetLazySegTreeDetail::run_linear_case<MAX_POINT, MAX_ID>(point_count, engine);
        TestA2BB2ARangeSetLazySegTreeDetail::run_circular_case<MAX_POINT, MAX_ID>(point_count, engine);
    }

    out("test_a2b_b2a_range_set_lazy_seg_tree PASSED");
}
