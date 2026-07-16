//
// Created by Auto
//

#ifndef UNTITLED_CHUNKSEPARATOR_H
#define UNTITLED_CHUNKSEPARATOR_H

#include "../../all_include.h"
#include "../../util.h"
#include "../../State.h"
#include "../../Legacy/lis_chank/Lis.h"
#include "../InitBeam/CircularRange.h"
#include "ChunkValState.h"
#include "../InitBeam/RotateCost.h"
#include "../../Command/SyncCommand.h"

namespace YakinamashiChunk {
    using ChunkSeparator::CircularRange;
    using ChunkSeparator::add_circular;
    using ChunkSeparator::circular_ranges_overlap_nieve;
    //o
    enum SeparateUnit {
        TOP = 1,
        BTM = 2,
        REMAIN = 4
    };

    //o
    enum class StackType {
        A,
        B
    };

    //o
    namespace ChunkLabelConstants {
        static constexpr char CHAR_TOP = 't';
        static constexpr char CHAR_BTM = 'b';
        static constexpr char CHAR_REMAIN = 'r';
    }

//o
    class ChunkLabel {
    private:
        string label_str;

        //o
        static char type_to_char(SeparateUnit t) {
            switch (t) {
                case TOP:
                    return ChunkLabelConstants::CHAR_TOP;
                case BTM:
                    return ChunkLabelConstants::CHAR_BTM;
                case REMAIN:
                    return ChunkLabelConstants::CHAR_REMAIN;
                default:
                    my_assert(false);
                    return '\0';
            }
        }

        //o
        static SeparateUnit char_to_type(char c) {
            switch (c) {
                case ChunkLabelConstants::CHAR_TOP:
                    return TOP;
                case ChunkLabelConstants::CHAR_BTM:
                    return BTM;
                case ChunkLabelConstants::CHAR_REMAIN:
                    return REMAIN;
                default:
                    my_assert(false);
                    return static_cast<SeparateUnit>(0);
            }
        }

        //o
        static void validate_char(char c) {
            my_assert(c == ChunkLabelConstants::CHAR_TOP ||
                      c == ChunkLabelConstants::CHAR_BTM ||
                      c == ChunkLabelConstants::CHAR_REMAIN);
        }

    public:
        ChunkLabel() : label_str("") {}

        //o
        explicit ChunkLabel(const string &str) {
            for (char c: str) {
                validate_char(c);
            }
            label_str = str;
        }

        //o
        string to_string() const {
            return label_str;
        }

        //o
        bool empty() const {
            return label_str.empty();
        }

        //o
        int size() const {
            return label_str.size();
        }

        //o
        SeparateUnit back() const {
            my_assert(!label_str.empty());
            return char_to_type(label_str.back());
        }

        //o
        ChunkLabel append(SeparateUnit t) const {
            ChunkLabel new_label;
            new_label.label_str = label_str + type_to_char(t);
            return new_label;
        }

        //o
        SeparateUnit get_ith(int i) const {
            my_assert(0 <= i && i < label_str.size());
            return char_to_type(label_str[i]);
        }

        //o
        bool operator==(const ChunkLabel &rhs) const {
            return label_str == rhs.label_str;
        }

        //o
        bool operator!=(const ChunkLabel &rhs) const {
            return label_str != rhs.label_str;
        }

        //o
        bool operator<(const ChunkLabel &rhs) const {
            return label_str < rhs.label_str;
        }
    };

    //o
    class SeparateTargets {
    private:
        int info;

        //o
        static void validate_info(int info) {
            my_assert(info > 0);
            my_assert((info & (TOP | BTM | REMAIN)) == info);
        }

    public:
        //o
        SeparateTargets() : info(0) {}

        //o
        explicit SeparateTargets(int info_val) : info(info_val) {
            validate_info(info);
        }

        //o
        bool has_top() const { return info & TOP; }

        //o
        bool has_btm() const { return info & BTM; }

        //o
        bool has_remain() const { return info & REMAIN; }

        //o
        bool operator==(const SeparateTargets &rhs) const {
            return info == rhs.info;
        }

        //o
        bool operator!=(const SeparateTargets &rhs) const {
            return info != rhs.info;
        }
    };

//o
    struct ChunkQuery {
    private:
        SeparateTargets separate_info;
    public:
        StackType stack_type;

        //o
        ChunkQuery() : separate_info(), stack_type(StackType::A) {}

        //o
        ChunkQuery(int info, StackType stack) : separate_info(info), stack_type(stack) {}

        //o
        explicit ChunkQuery(int info) : separate_info(info), stack_type(StackType::A) {}

        //o
        ChunkQuery(SeparateTargets info, StackType stack) : separate_info(info), stack_type(stack) {}

        //o
        bool has_top() const { return separate_info.has_top(); }

        //o
        bool has_btm() const { return separate_info.has_btm(); }

        //o
        bool has_remain() const { return separate_info.has_remain(); }

        //o
        SeparateTargets get_separate_info() const { return separate_info; }
    };

    template<size_t MAX_N>
    struct Chunk {
    private:
        int size;
        my_bitset<MAX_N> include_vals;
    public:
        ChunkLabel label;
        my_vector<shared_ptr<Chunk>> children;
        shared_ptr<Chunk> parent;
        int state_N;

//o
        Chunk(const ChunkLabel &lbl, int sz, int n) : label(lbl), size(sz), parent(nullptr), state_N(n) {
            my_assert(0 <= state_N && state_N <= MAX_N);
        }

//o
        bool is_leaf() const { return children.empty(); }

//o
        bool has_value(int value) const {
            my_assert(0 <= value && value < state_N);
            my_assert(value < MAX_N);
            return include_vals[value];
        }

//o
        void set_value(int value) {
            my_assert(0 <= value && value < state_N);
            my_assert(value < MAX_N);
            my_assert(!include_vals[value]);
            include_vals[value] = true;
            size++;
            validate_size();
        }

//o
        void validate_size() {
            my_assert(include_vals.count() == size);
        }

        //o
        void add_values(const my_bitset<MAX_N> &vals) {
            my_assert((include_vals & vals).none());
            include_vals |= vals;
            size += vals.count();
            validate_size();
        }

//o
        void clear_values() {
            include_vals.reset();
            size = 0;
            validate_size();
        }

        //o
        int get_size() const {
            return size;
        }

        //o
        const my_bitset<MAX_N> &get_include_vals() const {
            return include_vals;
        }
    };

    template<size_t MAX_N>
    struct ChunkTree {
        int state_N;
        shared_ptr<Chunk<MAX_N>> root;
        my_vector<shared_ptr<Chunk<MAX_N>>> final_a_chunks;
        my_vector<shared_ptr<Chunk<MAX_N>>> final_b_chunks;
        map<ChunkLabel, shared_ptr<Chunk<MAX_N>>> chunk_map;
        map<ChunkLabel, SeparateTargets> chunk_separate_info;
        my_vector<shared_ptr<Chunk<MAX_N>>> value_to_final_chunk;

        //o
        ChunkTree() : state_N(0) {}

        //o
        explicit ChunkTree(int n) : state_N(n) {
            my_assert(0 <= state_N && state_N <= MAX_N);
        }

        void build_chunk_tree(const my_vector<ChunkQuery> &queries);

        shared_ptr<Chunk<MAX_N>> find_chunk_by_label(const ChunkLabel &label) const;

        ChunkQuery get_separate_query_for_chank(const ChunkLabel &label, StackType stack_type) const;

        ChunkLabel get_final_chunk_label(int value) const;

        bool is_value_in_chunk(int value, const ChunkLabel &chunk_label) const;

        int get_next_operation(int value, const ChunkLabel &current_chunk_label) const;

        void validate_root_has_all_val();

        void assign_values_to_final_chunks(const my_vector<my_vector<int>> &a_values, const my_vector<my_vector<int>> &b_values);

    private:
        void separate_chunk(shared_ptr<Chunk<MAX_N>> chunk, const ChunkQuery &query);

        shared_ptr<Chunk<MAX_N>> find_final_chunk_by_value(int value) const;

        //o
        void initialize_chunk_tree();

        //o
        void process_query_for_top_chank(const ChunkQuery &query,
                                         deque<shared_ptr<Chunk<MAX_N>>> &from_chunks,
                                         deque<shared_ptr<Chunk<MAX_N>>> &to_chunks);

        //o
        void set_final_chunks(deque<shared_ptr<Chunk<MAX_N>>> &cur_a_chunks,
                              deque<shared_ptr<Chunk<MAX_N>>> &cur_b_chunks);

        //o
        void clear_all_chunk_values() {
            for (const auto &[label, chunk]: chunk_map) {
                chunk->clear_values();
            }
        }

        //o
        void assign_values_to_chunks(const my_vector<shared_ptr<Chunk<MAX_N>>> &chunks,
                                     const my_vector<my_vector<int>> &values_list) {
            my_assert(chunks.size() == values_list.size());
            for (size_t i = 0; i < chunks.size(); i++) {
                auto chunk = chunks[i];
                const auto &values = values_list[i];

                for (int v: values) {
                    my_assert(0 <= v && v < state_N);
                    chunk->set_value(v);
                    value_to_final_chunk[v] = chunk;
                }
            }
        }

        //o
        void propagate_vals_by_children(const shared_ptr<Chunk<MAX_N>> &cur) {
            if (cur->is_leaf()) {
                return;
            }
            my_assert(cur->get_size() == 0);
            for (auto &child: cur->children) {
                propagate_vals_by_children(child);
                cur->add_values(child->get_include_vals());
            }
        }

        //o
        void validate_assign_values(const my_vector<my_vector<int>> &a_values, const my_vector<my_vector<int>> &b_values) const {
#ifdef DEBUG
            my_bitset<MAX_N> seen;
            auto process_values = [&](const my_vector<my_vector<int>> &values_list) {
                for (const auto &values: values_list) {
                    for (int v: values) {
                        my_assert(0 <= v && v < state_N);
                        my_assert(!seen[v]);
                        seen[v] = true;
                    }
                }
            };
            process_values(a_values);
            process_values(b_values);
            my_assert(seen.count() == state_N);
#endif
        }

        //o
        void validate_exist_label(const ChunkLabel &label) const {
#ifdef DEBUG
            auto it = chunk_map.find(label);
            my_assert(it != chunk_map.end());
#endif
        }
    };

    //o
    template<size_t MAX_N>
    void ChunkTree<MAX_N>::separate_chunk(shared_ptr<Chunk<MAX_N>> chunk, const ChunkQuery &query) {
        my_assert(query.has_top() || query.has_btm() || query.has_remain());
        chunk_separate_info[chunk->label] = query.get_separate_info();

        auto expand = [&](SeparateUnit new_sep) {
            ChunkLabel new_label = chunk->label.append(new_sep);
            auto new_chunk = make_shared<Chunk<MAX_N>>(new_label, 0, chunk->state_N);
            new_chunk->parent = chunk;
            chunk->children.push_back(new_chunk);
            chunk_map[new_label] = new_chunk;
        };
        if (query.has_top()) {
            expand(TOP);
        }
        if (query.has_btm()) {
            expand(BTM);
        }
        if (query.has_remain()) {
            expand(REMAIN);
        }
    }

    //o
    template<size_t MAX_N>
    void ChunkTree<MAX_N>::build_chunk_tree(const my_vector<ChunkQuery> &queries) {
        initialize_chunk_tree();

        deque<shared_ptr<Chunk<MAX_N>>> cur_a_chunks;
        deque<shared_ptr<Chunk<MAX_N>>> cur_b_chunks;
        cur_a_chunks = {root};

        for (const auto &query: queries) {
            if (query.stack_type == StackType::A) {
                process_query_for_top_chank(query, cur_a_chunks, cur_b_chunks);
            } else {
                process_query_for_top_chank(query, cur_b_chunks, cur_a_chunks);
            }
        }
        set_final_chunks(cur_a_chunks, cur_b_chunks);
    }

    //o
    template<size_t MAX_N>
    void ChunkTree<MAX_N>::initialize_chunk_tree() {
        chunk_map.clear();
        chunk_separate_info.clear();
        final_a_chunks.clear();
        final_b_chunks.clear();

        ChunkLabel root_label;
        auto root_chunk = make_shared<Chunk<MAX_N>>(root_label, 0, state_N);
        root = root_chunk;
        chunk_map[root_label] = root_chunk;
    }

    //o
    template<size_t MAX_N>
    void ChunkTree<MAX_N>::process_query_for_top_chank(const ChunkQuery &query,
                                                       deque<shared_ptr<Chunk<MAX_N>>> &from_chunks,
                                                       deque<shared_ptr<Chunk<MAX_N>>> &to_chunks) {
        my_assert(!from_chunks.empty());
        auto &chunk = from_chunks[0];
        separate_chunk(chunk, query);
        for (auto child: chunk->children) {
            if (child->label.back() == REMAIN) {
                from_chunks.push_back(child);
            } else if (child->label.back() == TOP) {
                to_chunks.push_front(child);
            } else if (child->label.back() == BTM) {
                to_chunks.push_back(child);
            } else {
                my_assert(false);
            }
        }
        from_chunks.pop_front();
    }

    //o
    template<size_t MAX_N>
    void ChunkTree<MAX_N>::set_final_chunks(deque<shared_ptr<Chunk<MAX_N>>> &cur_a_chunks,
                                            deque<shared_ptr<Chunk<MAX_N>>> &cur_b_chunks) {
        final_a_chunks = my_vector<shared_ptr<Chunk<MAX_N>>>(cur_a_chunks.begin(), cur_a_chunks.end());
        final_b_chunks = my_vector<shared_ptr<Chunk<MAX_N>>>(cur_b_chunks.begin(), cur_b_chunks.end());
    }

    //o
    template<size_t MAX_N>
    shared_ptr<Chunk<MAX_N>> ChunkTree<MAX_N>::find_chunk_by_label(const ChunkLabel &label) const {
        auto it = chunk_map.find(label);
        my_assert (it != chunk_map.end());
        return it->second;
    }

    //o
    //labelの状態のchunkはstack_typeの位置にあるか
    inline void validate_stack_type(const ChunkLabel label, StackType stack_type) {
#ifdef DEBUG
        //初期状態がAから始まるという前提でかく
        int now_a = 1;
        for (int i = 0; i < label.size(); i++) {
            SeparateUnit op = label.get_ith(i);
            if (op == TOP) {
                now_a ^= 1;
            }
            if (op == BTM) {
                now_a ^= 1;
            }
        }
        if (stack_type == StackType::A) {
            my_assert(now_a);
        } else {
            my_assert(!now_a);
        }
#endif
    }


//o
    template<size_t MAX_N>
    ChunkQuery ChunkTree<MAX_N>::get_separate_query_for_chank(const ChunkLabel &label, StackType stack_type) const {
        validate_stack_type(label, stack_type);
        auto it = chunk_separate_info.find(label);
        my_assert(it != chunk_separate_info.end());
        SeparateTargets info = it->second;
        ChunkQuery query(info, stack_type);
        return query;
    }

    //o
    template<size_t MAX_N>
    shared_ptr<Chunk<MAX_N>> ChunkTree<MAX_N>::find_final_chunk_by_value(int value) const {
        my_assert(0 <= value && value < state_N);
        auto res = value_to_final_chunk[value];
        my_assert(res);
        return res;
    }

    //o
    template<size_t MAX_N>
    ChunkLabel ChunkTree<MAX_N>::get_final_chunk_label(int value) const {
        auto chunk = find_final_chunk_by_value(value);
        my_assert (chunk);
        return chunk->label;
    }

    //o
    template<size_t MAX_N>
    bool ChunkTree<MAX_N>::is_value_in_chunk(int value, const ChunkLabel &chunk_label) const {
        validate_exist_label(chunk_label);
        auto target_chunk = find_chunk_by_label(chunk_label);
        return target_chunk->has_value(value);
    }

    //o
    template<size_t MAX_N>
    int ChunkTree<MAX_N>::get_next_operation(int value, const ChunkLabel &current_chunk_label) const {
        auto final_chunk = find_final_chunk_by_value(value);
        int nex_i = current_chunk_label.size();
        auto op = final_chunk->label.get_ith(nex_i);
        return op;
    }

    //o
    template<size_t MAX_N>
    void ChunkTree<MAX_N>::validate_root_has_all_val() {
        my_assert(root->get_size() == state_N);//全ての値を持たないといけない
    }

    //o
    template<size_t MAX_N>
    void ChunkTree<MAX_N>::assign_values_to_final_chunks(const my_vector<my_vector<int>> &a_values,
                                                         const my_vector<my_vector<int>> &b_values) {
        validate_assign_values(a_values, b_values);
        clear_all_chunk_values();
        value_to_final_chunk.assign(state_N, nullptr);
        assign_values_to_chunks(final_a_chunks, a_values);
        assign_values_to_chunks(final_b_chunks, b_values);
        propagate_vals_by_children(root);
        validate_root_has_all_val();
    }

    //o
    template<size_t MAX_N>
    ChunkTree<MAX_N> get_chunks(const my_vector<ChunkQuery> &separate_queries, int state_N) {
        ChunkTree<MAX_N> tree(state_N);
        tree.build_chunk_tree(separate_queries);
        return tree;
    }

    // ============================================================================
    // ChunkCommand - Command generation from chunk assignments
    // ============================================================================
        //x
        // 今扱っているラベル+btmをnow_btmと言っている
        // 見ているtoのchankが全てnow_btmかどうか
        enum e_to_chanks_state {
            all_now_btm, not_all_now_btm
        };

        template<size_t MAX_N>
        class ValueOrderConstraint {
        private:
            int state_N;
            my_vector<int> prev_value_map;
            my_vector<int> next_value_map;

            static constexpr int HAS_NO_VALUE = -1;

            //o
            void set_init_prev_value(int v, int prev_v) {
                validate_v(v);
                validate_v(prev_v);
                my_assert(prev_value_map[v] == HAS_NO_VALUE);  // 重複設定を防ぐ
                prev_value_map[v] = prev_v;
            }

            //o
            void set_init_next_value(int v, int next_v) {
                validate_v(v);
                validate_v(next_v);
                my_assert(next_value_map[v] == HAS_NO_VALUE);  // 重複設定を防ぐ
                next_value_map[v] = next_v;
            }

            //o
            void validate_v(int v) const {
                my_assert(0 <= v && v < state_N);
            }

            //o
            //現状のチャンクが上下逆になるかなどを考慮せず危険なため注意して使用
            bool has_init_prev_value(int v) const {
                validate_v(v);
                return prev_value_map[v] != HAS_NO_VALUE;
            }

            bool has_init_next_value(int v) const {
                validate_v(v);
                return next_value_map[v] != HAS_NO_VALUE;
            }

            //o
            //現状のチャンクが上下逆になるかなどを考慮せず危険なため注意して使用
            int get_init_prev_value(int v) const {
                validate_v(v);
                if (!has_init_prev_value(v)) {
                    stringstream ss;
                    ss << "ValueOrderConstraint::get_init_prev_value: Value " << v
                       << " does not have a previous value constraint.";
                    throw std::runtime_error(ss.str());
                    my_assert(false);
                }
                return prev_value_map[v];
            }

            //o
            int get_init_next_value(int v) const {
                validate_v(v);
                if (!has_init_next_value(v)) {
                    stringstream ss;
                    ss << "ValueOrderConstraint::get_init_prev_value: Value " << v
                       << " does not have a previous value constraint.";
                    throw std::runtime_error(ss.str());
                    my_assert(false);
                }
                return next_value_map[v];
            }

        public:
            //o
            ValueOrderConstraint(int state_N) : state_N(state_N), prev_value_map(state_N, HAS_NO_VALUE),
                                                next_value_map(state_N, HAS_NO_VALUE) {
                my_assert(0 <= state_N && state_N <= MAX_N);
            }

            //o
            //初期状態のaのなかでのじゅんじょのlis
            void set_ordered_values(const my_vector<int> &ordered_values) {
                if (ordered_values.empty()) {
                    return;
                }
                for (size_t i = 1; i < ordered_values.size(); i++) {
                    int v = ordered_values[i];
                    int prev_v = ordered_values[i - 1];
                    set_init_prev_value(v, prev_v);
                    set_init_next_value(prev_v, v);
                }
            }

            //現状のlabelにおける、vの一つ前にあるべき値
            bool has_prev_value(int v, const ChunkLabel &label) const {
                validate_v(v);
                int top_count = count_top_in_label(label);
                if (top_count % 2 == 1) {
                    return has_init_next_value(v);
                } else {
                    return has_init_prev_value(v);
                }
            }

            //現状のlabelにおける、vの一つ前にあるべき値
            int get_prev_value(int v, const ChunkLabel &label) const {
                my_assert(has_prev_value(v, label));
                validate_v(v);
                int top_count = count_top_in_label(label);
                if (top_count % 2 == 1) {
                    return get_init_next_value(v);
                } else {
                    return get_init_prev_value(v);
                }
            }

            //o
            // ラベル内のTOPの個数を数える
            int count_top_in_label(const ChunkLabel &label) const {
                int count = 0;
                for (size_t i = 0; i < label.size(); i++) {
                    if (label.get_ith(i) == TOP) {
                        count++;
                    }
                }
                return count;
            }

            //o
            // 指定した値が、指定したラベルにいる事はあり得るのかを判定
            template<size_t TREE_MAX_N>
            bool
            is_value_possible_in_label(int value, const ChunkLabel &label, const ChunkTree<TREE_MAX_N> &tree) const {
                validate_v(value);
                ChunkLabel final_label = tree.get_final_chunk_label(value);
                string label_str = label.to_string();
                string final_label_str = final_label.to_string();
                return final_label_str.size() >= label_str.size() &&
                       final_label_str.substr(0, label_str.size()) == label_str;
            }

            //o
            // a, bがlabelの子孫であることを検証
            template<size_t TREE_MAX_N>
            void validate_values_belong_to_label(int a, int b, const ChunkLabel &label,
                                                 const ChunkTree<TREE_MAX_N> &tree) const {
#ifdef DEBUG
                my_assert(is_value_possible_in_label(a, label, tree));
                my_assert(is_value_possible_in_label(b, label, tree));
#endif
            }

            //o
            // このラベルの時に、a, bという順に並ばないといけないかを確認する
            // このラベルの状態で、次の操作をやる
            // ラベル内のtの個数が奇数なら順序が逆になる
            bool is_constraint_order_to_btm(int a, int b, const ChunkLabel &label, const ChunkTree<MAX_N> &tree) const {
                validate_values_belong_to_label(a, b, label, tree);

                int top_count = count_top_in_label(label);
                bool is_reversed = (top_count % 2 == 1);
                if (!is_reversed) {
                    return has_init_prev_value(b) && get_init_prev_value(b) == a;
                } else {
                    return has_init_prev_value(a) && get_init_prev_value(a) == b;
                }
            }
        };

        template<size_t MAX_N>
        class LISCalculator {
        private:
            //o
            int count_top_in_label(const ChunkLabel &label) const {
                int count = 0;
                for (size_t i = 0; i < label.size(); i++) {
                    if (label.get_ith(i) == TOP) {
                        count++;
                    }
                }
                return count;
            }

        public:
            //x
            my_vector<int> calculate_lis_in_range(
                    const State &state,
                    int range_start,
                    int range_end,
                    const ChunkLabel &label
            ) const {
                my_assert(0 <= range_start && range_start < state.current_N);
                my_assert(0 <= range_end && range_end <= state.current_N);

                int range_size = (range_end >= range_start)
                                 ? (range_end - range_start)
                                 : (state.current_N - range_start + range_end);
                CircularRange circular_range(range_start, range_size, state.current_N);

                // 範囲内の値を抽出（Aスタックの順序を保つ）
                deque<int> range_values_with_geta;
                bool is_wrapping = range_start > range_end;
                for (int i = 0; i < state.A.size(); i++) {
                    int v = state.A[i];
                    if (circular_range.contains(v)) {
                        if (is_wrapping && v < range_end) {
                            v += state.current_N;
                        }
                        range_values_with_geta.push_back(v);
                    }
                }
                int top_count = count_top_in_label(label);
                if (top_count % 2 == 1) {
                    std::reverse(range_values_with_geta.begin(), range_values_with_geta.end());
                }
                //現状は初期のAに対してやってるので、全ての値があるはず
                my_assert(range_values_with_geta.size() == circular_range.size());

                // range_valuesに対して円環上LISを計算
                set<val_swap_t> lis_set;

                lis_set = get_s_lis_past(range_values_with_geta, 0, 0);


                // LISの値を抽出
                my_vector<int> lis_values;
                for (const auto &[v, swap_cnt]: lis_set) {
                    if (v >= state.current_N) {
                        lis_values.push_back(v - state.current_N);
                    } else {
                        lis_values.push_back(v);
                    }
                }
                if (top_count % 2 == 1) {
                    std::reverse(lis_values.begin(), lis_values.end());
                }

                return lis_values;
            }

            //x CircularRange版（start==endでもempty/fullを判定できる）
            my_vector<int> calculate_lis_in_range(
                    const State &state,
                    const CircularRange &circular_range,
                    const ChunkLabel &label
            ) const {
                my_assert(circular_range.get_N() == state.current_N);

                if (circular_range.is_empty()) {
                    return {};
                }
                const int range_start = circular_range.get_start();

                deque<int> range_values_with_geta;
                for (int i = 0; i < state.A.size(); i++) {
                    int v = state.A[i];
                    if (circular_range.contains(v)) {
                        if (v < range_start) {
                            v += state.current_N;
                        }
                        range_values_with_geta.push_back(v);
                    }
                }

                int top_count = count_top_in_label(label);
                if (top_count % 2 == 1) {
                    std::reverse(range_values_with_geta.begin(), range_values_with_geta.end());
                }

                my_assert(range_values_with_geta.size() == circular_range.size());

                set<val_swap_t> lis_set = get_s_lis_past(range_values_with_geta, 0, 0);

                my_vector<int> lis_values;
                lis_values.reserve(lis_set.size());
                for (const auto &[v, swap_cnt]: lis_set) {
                    if (v >= state.current_N) {
                        lis_values.push_back(v - state.current_N);
                    } else {
                        lis_values.push_back(v);
                    }
                }
                if (top_count % 2 == 1) {
                    std::reverse(lis_values.begin(), lis_values.end());
                }

                return lis_values;
            }

            //x CircularRange版でswap込みのLISを計算
            // max_swapで許容swap回数を指定
            pair<my_bitset<MAX_N>, my_vector<int>> calculate_lis_with_swap_in_range(
                    const State &state,
                    const CircularRange &circular_range,
                    const ChunkLabel &label,
                    int max_swap,
                    int lis_start = 0
            ) const {
                my_assert(circular_range.get_N() == state.current_N);
                my_assert(max_swap >= 0);

                if (circular_range.is_empty()) {
                    return {my_bitset<MAX_N>(), {}};
                }
                const int range_start = circular_range.get_start();

                deque<int> range_values_with_geta;
                for (int i = 0; i < state.A.size(); i++) {
                    int v = state.A[i];
                    if (circular_range.contains(v)) {
                        if (v < range_start) {
                            v += state.current_N;
                        }
                        range_values_with_geta.push_back(v);
                    }
                }

                int top_count = count_top_in_label(label);
                if (top_count % 2 == 1) {
                    std::reverse(range_values_with_geta.begin(), range_values_with_geta.end());
                }

                my_assert(range_values_with_geta.size() == circular_range.size());

                // swap込みでLIS計算
                int actual_lis_start = lis_start % static_cast<int>(range_values_with_geta.size());
                set<val_swap_t> lis_swap_geta_set = get_s_lis_past(range_values_with_geta, actual_lis_start, max_swap);
                my_bitset<MAX_N> is_lis;
                my_bitset<MAX_N> is_swap;
                for(auto&[geta_v, swap_cnt] : lis_swap_geta_set){
                    int act_v = (geta_v >= state.current_N) ? (geta_v - state.current_N) : geta_v;
                    if(swap_cnt > 0){
                        is_swap[act_v] = true;
                    }else{
                        is_lis[act_v] = true;
                    }
                }
                my_vector<int> lis_swap_vals;
                for(auto& geta_v : range_values_with_geta){
                    int act_v = (geta_v >= state.current_N) ? (geta_v - state.current_N) : geta_v;
                    if(is_lis[act_v] || is_swap[act_v]){
                        lis_swap_vals.push_back(act_v);
                    }
                }
                if (top_count % 2 == 1) {
                    std::reverse(lis_swap_vals.begin(), lis_swap_vals.end());
                }
                return make_pair(is_lis, lis_swap_vals);

            }
        };

        //x 円環範囲の分割だけを担当する純粋関数クラス
        class ChunkRangeDivider {
        private:
            int state_N;

        public:
            //x
            ChunkRangeDivider(int n) : state_N(n) {
                my_assert(state_N > 0);
            }

            //x A用の均等分割範囲を生成
            my_vector<CircularRange> divide_a_ranges(int chunk_count, int start_offset = 0) const {
                my_assert(chunk_count > 0);
                my_assert(0 <= start_offset && start_offset < state_N);

                my_vector<CircularRange> ranges;
                ranges.reserve(chunk_count);

                int l = start_offset;
                int default_range_size = state_N / chunk_count;
                int remainder = state_N % chunk_count;
                int start_l_saved = start_offset;

                for (int i = 0; i < chunk_count; i++) {
                    int range_size = default_range_size + (i < remainder ? 1 : 0);
                    int r = add_circular(l, range_size, state_N);

                    if (i == chunk_count - 1) {
                        my_assert(r == start_l_saved);
                    }

                    ranges.push_back(CircularRange(l, range_size, state_N));
                    l = r;
                }

                return ranges;
            }

            //x B用の降順・complete範囲を生成
            my_vector<CircularRange> divide_b_ranges(int chunk_count, int start_offset = 0) const {
                my_assert(chunk_count > 0);
                my_assert(0 <= start_offset && start_offset < state_N);

                my_vector<CircularRange> ranges;
                ranges.reserve(chunk_count);

                int l = start_offset;
                int default_range_size = state_N / chunk_count;
                int remainder = state_N % chunk_count;
                int start_l_saved = start_offset;

                for (int i = 0; i < chunk_count; i++) {
                    int range_size = default_range_size + (i < remainder ? 1 : 0);
                    int r = add_circular(l, range_size, state_N);

                    if (i == chunk_count - 1) {
                        my_assert(r == start_l_saved);
                    }

                    ranges.push_back(CircularRange(l, range_size, state_N));
                    l = r;
                }

                std::reverse(ranges.begin(), ranges.end());
                return ranges;
            }
        };

        template<size_t MAX_N>
        class ChunkConstraintAssignor {
        private:
            int state_N;


        public:

            //o
            ChunkConstraintAssignor(int n) : state_N(n) {
                my_assert(0 <= state_N && state_N <= MAX_N);
            }


            //x
            ValueOrderConstraint<MAX_N> assign_orderedA_unorderedB(
                    ChunkTree<MAX_N> &tree,
                    my_vector<my_vector<int>> &a_values,
                    my_vector<my_vector<int>> &b_values
            ) {
                my_assert(tree.state_N == state_N);
                ValueOrderConstraint<MAX_N> constraint(state_N);
                for (auto &av: a_values) {
                    constraint.set_ordered_values(av);
                }
                tree.assign_values_to_final_chunks(a_values, b_values);
                return constraint;
            }
        };

        // ============================================================================
        // ChunkCommandGenerator - Generate command sequence from ChunkTree and constraints
        // ============================================================================

        template<size_t MAX_N>
        class ChunkCommandGenerator {
        private:
            int state_N;
            const ChunkTree<MAX_N> &tree;
            const ValueOrderConstraint<MAX_N> &constraint;
            const my_vector<ChunkQuery> &queries;

        public:
            //o
            ChunkCommandGenerator(
                    const ChunkTree<MAX_N> &tree,
                    const ValueOrderConstraint<MAX_N> &constraint,
                    const my_vector<ChunkQuery> &queries
            ) : state_N(tree.state_N), tree(tree), constraint(constraint), queries(queries) {
                my_assert(tree.state_N == state_N);
            }

            //o
            ChunkLabel calc_target_chunk_label(const State &state, const my_vector<ChunkLabel> &current_chunk_labels,
                                               const ChunkQuery &query, int now_from_pos) const {
                if (query.stack_type == StackType::A) {
                    my_assert(state.A.size() > 0);
                    return current_chunk_labels[state.A[now_from_pos]];
                } else {
                    my_assert(state.B.size() > 0);
                    return current_chunk_labels[state.B[now_from_pos]];
                }
            };

            //o
            void print_chunk_label_state(const State &state, const my_vector<ChunkLabel> &current_chunk_labels,
                                         const ChunkLabel &target_chunk_label) const {
                auto print_stack = [&](const RingBuffer500 &target_stack) {
                    for (int i = 0; i < target_stack.size(); i++) {
                        int v = target_stack[i];
                        cout << current_chunk_labels[v].to_string() << ", ";
                    }
                    cout << endl;
                };
                cout << "A" << endl;
                print_stack(state.A);
                cout << "B" << endl;
                print_stack(state.B);
            }

            //o
            void validate_top_chunk_label(const State &state, const my_vector<ChunkLabel> &current_chunk_labels,
                                          const ChunkLabel &target_chunk_label, const ChunkQuery &query) const {
//stateの上から全部を見て、target_chunk_labelが連続しているかを確認
#ifdef DEBUG
//                print_chunk_label_state(state, current_chunk_labels, target_chunk_label);
                const RingBuffer500 &target_stack = (query.stack_type == StackType::A) ? state.A : state.B;
                bool now_top_chunk_zone = true;
                for (int i = 0; i < target_stack.size(); i++) {
                    int v = target_stack[i];
                    bool cur_target_chunk = current_chunk_labels[v] == target_chunk_label;
                    if (now_top_chunk_zone) {
                        if (!cur_target_chunk) {
                            my_assert(i > 0);
                            now_top_chunk_zone = false;
                        }
                    } else {
                        my_assert(!cur_target_chunk);
                    }
                }
#endif
            }

            //o
            pair<my_vector<command>, State> generate_commands(const State &initial_state) const {
                return generate_commands_legacy(initial_state);
            }

            //o
            pair<my_vector<command>, State> generate_commands_legacy(const State &initial_state) const {
                my_assert(initial_state.A.size() == state_N);
                State state(initial_state);
                my_vector<command> commands;

                // 各値の現在のチャンクラベルを保持（初期状態では全てroot = 空ラベル）
                my_vector<ChunkLabel> current_chunk_labels(state_N);

                // クエリ列を順に実行するシミュレーション
                for (const auto &query: queries) {
                    ChunkLabel target_chunk_label = calc_target_chunk_label(state, current_chunk_labels, query, 0);
                    validate_top_chunk_label(state, current_chunk_labels, target_chunk_label, query);

                    // 現在のスタックから、対象チャンクラベルに属する値を探す
                    const RingBuffer500 &target_stack = (query.stack_type == StackType::A) ? state.A : state.B;
                    int loop_max = target_stack.size();
                    for (int i = 0; i < loop_max; i++) {
                        int v = target_stack[0];
                        if (current_chunk_labels[v] == target_chunk_label) {
                            // この値について、次の操作を取得して実行
                            ChunkLabel final_label = tree.get_final_chunk_label(v);
                            my_assert (current_chunk_labels[v] != final_label);
                            int op = tree.get_next_operation(v, current_chunk_labels[v]);
                            //command cmd = convert_operation_to_command(op, query.stack_type);
                            my_vector<command> cmds = get_simple_separate_unit_cmd_legacy(op, query.stack_type, state);

                            commands.insert(commands.end(), cmds.begin(), cmds.end());
                            for (auto &cmd: cmds) {
                                state.do_cmd(cmd);
                            }
                            current_chunk_labels[v] = current_chunk_labels[v].append(static_cast<SeparateUnit>(op));
                        } else {
                            break;
                        }
                    }
                }

                return {commands, state};
            }

            void print_final_chunk() const {
                auto get_chunks_to_str = [&](auto &chunks) {
                    stringstream s;
                    for (shared_ptr<Chunk<MAX_N>> a: chunks) {
//                        my_assert(a->get_size());
                        s << a->label.to_string();
                        s << "(";
                        for (int i = 0; i < a->state_N; i++) {
                            if (a->has_value(i)) {
                                s << i;
                                s << ", ";
                            }
                        }
                        s << "), ";
                    }
                    return s.str();
                };
                // //Optuna out("fin lab");
                // //Optuna out("A");
                // //Optuna out(get_chunks_to_str(tree.final_a_chunks));
                // //Optuna out("B");
                // //Optuna out(get_chunks_to_str(tree.final_b_chunks));
            }

            void print_chank_st(State &st, my_vector<ChunkLabel> &current_chunk_labels) const {
                auto get_str = [&](auto &buf) {
                    stringstream s;
//                    string s="";
                    for (int i = 0; i < buf.size(); i++) {
                        s << buf[i];
                        s << "(";
                        s << current_chunk_labels[buf[i]].to_string();
                        s << ")";
                        s << ", ";
                    }
                    return s.str();
                };
                out("A");
                out(get_str(st.A));
                out("B");
                out(get_str(st.B));
            }

            //my_vector<T>に対して、ランレングス圧縮を行う
            template<typename T> my_vector<pair<T, int>> run_length(const my_vector<T>& vec) const {
                my_vector<pair<T, int>> rle;
                if (vec.empty()) {
                    return rle;
                }
                T prev = vec[0];
                int count = 1;
                for (size_t i = 1; i < vec.size(); i++) {
                    if (vec[i] == prev) {
                        count++;
                    } else {
                        rle.emplace_back(prev, count);
                        prev = vec[i];
                        count = 1;
                    }
                }
                rle.emplace_back(prev, count);
                return rle;
            }

            //x
            my_vector<pair<ChunkLabel, int>> get_chunk_run_length(const RingBuffer500& stack, int start_i)const{
                my_vector<ChunkLabel> labels;
                for(int i = 0; i < stack.size(); i++){
                    int v = stack[mod(start_i + i, stack.size())];
                    labels.push_back(tree.get_final_chunk_label(v));
                }
                return run_length(labels);
            }

            //x 
            bool is_same_labels(my_vector<pair<ChunkLabel, int>>&res, const my_vector<shared_ptr<Chunk<MAX_N>>>&predict) const {
                if(res.size() != predict.size()){
                    return false;
                }
                for(size_t i = 0; i < res.size(); i++){
                    if(res[i].first != predict[i]->label){
                        return false;
                    }
                    if(res[i].second != predict[i]->get_size()){
                        return false;
                    }
                }
                return true;
            }

            //x
            void validate_order_constraints(const State &final_state, int a_start_i, int b_start_i) const {
               //すでに見た値をmy_bitsetで管理し、prevの値がその中にあることを確認 
#ifndef DEBUG
                return;
#endif
                my_bitset<MAX_N> seen;
                //a
                for (int i = 0; i < final_state.A.size(); i++) {
                    int v = final_state.A[mod(a_start_i + i, final_state.A.size())];
                    if (constraint.has_prev_value(v, tree.get_final_chunk_label(v))) {
                        int prev_v = constraint.get_prev_value(v, tree.get_final_chunk_label(v));
                        my_assert(seen.test(prev_v));
                        ChunkLabel prev_label = tree.get_final_chunk_label(prev_v);
                        ChunkLabel curr_label = tree.get_final_chunk_label(v);
                        my_assert(prev_label == curr_label);//順序の制約は、同じチャンク内でしか発生しない
                    }
                    seen.set(v);
                }
                //b
                for (int i = 0; i < final_state.B.size(); i++) {
                    auto& v = final_state.B[i];
                    my_assert(!constraint.has_prev_value(v, tree.get_final_chunk_label(v)));
                }
            }

            //x
            void validate_same_labels(const State &final_state, int a_start_i, int b_start_i) const {
                my_vector<pair<ChunkLabel, int>> result_a_labels = get_chunk_run_length(final_state.A, a_start_i);
                my_vector<pair<ChunkLabel, int>> result_b_labels = get_chunk_run_length(final_state.B, b_start_i);
                // 空チャンクは実スタックのrun_lengthに現れないため除外して比較する
                auto filter_nonempty = [](const my_vector<shared_ptr<Chunk<MAX_N>>>& chunks) {
                    my_vector<shared_ptr<Chunk<MAX_N>>> result;
                    for (const auto& c : chunks) {
                        if (c->get_size() > 0) result.push_back(c);
                    }
                    return result;
                };
                auto predict_final_a_chunks = filter_nonempty(tree.final_a_chunks);
                auto predict_final_b_chunks = filter_nonempty(tree.final_b_chunks);

                if (result_a_labels.size() != predict_final_a_chunks.size()) {
                    out("=== A labels mismatch ===");
                    out("result_a_labels.size():", result_a_labels.size(), "predict_final_a_chunks.size():", predict_final_a_chunks.size());
                    out("result_a_labels: (label, cnt)");
                    for (const auto& [label, cnt] : result_a_labels) {
                        out("  ", label.to_string(), ":", cnt);
                    }
                    out("final_state.A (from idx", a_start_i, "):", final_state.A.size());
                }

                if (result_b_labels.size() != predict_final_b_chunks.size()) {
                    out("=== B labels mismatch ===");
                    out("result_b_labels.size():", result_b_labels.size(), "predict_final_b_chunks.size():", predict_final_b_chunks.size());
                    out("result_b_labels: (label, cnt)");
                    for (const auto& [label, cnt] : result_b_labels) {
                        out("  ", label.to_string(), ":", cnt);
                    }
                    out("final_state.B (from idx", b_start_i, "):", final_state.B.size());
                }

                my_assert(result_a_labels.size() == predict_final_a_chunks.size());
                my_assert(result_b_labels.size() == predict_final_b_chunks.size());
                my_assert(is_same_labels(result_a_labels, predict_final_a_chunks));
                my_assert(is_same_labels(result_b_labels, predict_final_b_chunks));
            }

            //x
            void validate_finish(const State &final_state, int a_start_i, int b_start_i) const {
#ifndef DEBUG
                return;
#endif
                auto &predict_final_a_chunks = tree.final_a_chunks;
                auto &predict_final_b_chunks = tree.final_b_chunks;
                validate_same_labels(final_state, a_start_i, b_start_i);
                validate_order_constraints(final_state, a_start_i, b_start_i);
            }

            //o
            // Constraint版のgenerate_commands
            //aの開始地点、bの開始地点を返すべきか
            pair<my_vector<command>, State> generate_commands_constraint(const State &initial_state) const {
                my_assert(initial_state.A.size() == state_N);
                State state(initial_state);
                my_vector<command> commands;

                my_vector<ChunkLabel> current_chunk_labels(state_N);

                int now_from_pos = 0;
                int to_top_must_btm_cnt = 0;
                my_assert(queries.size() > 0);
                StackType prev_q_stack = queries[0].stack_type;
                print_final_chunk();
                //struct
                for (const auto &query: queries) {
//                    void print_chank_st(State& st, my_vector<ChunkLabel>& current_chunk_labels){
//                    print_chank_st(state, current_chunk_labels);
                    if (prev_q_stack != query.stack_type) {
                        swap(now_from_pos, to_top_must_btm_cnt);
                        //前のラベルのmust_rot_cntはかならずrotしないといけない...
                        //この状態をstructでもつべき？o
                    }
                    ChunkLabel target_chunk_label = calc_target_chunk_label(state, current_chunk_labels, query,
                                                                            now_from_pos);
//                    validate_top_chunk_label(state, current_chunk_labels, target_chunk_label, query);

                    // クエリ開始時にto_chank_stateを初期化
                    const RingBuffer500 &to_stack = (query.stack_type == StackType::A) ? state.B : state.A;
                    e_to_chanks_state to_chank_state = to_stack.empty() ? all_now_btm : not_all_now_btm;

                    const RingBuffer500 &target_stack = (query.stack_type == StackType::A) ? state.A : state.B;
                    int loop_max = target_stack.size();
                    int cnt = 0;
                    for (int i = 0; i < loop_max; i++) {
                        int v = target_stack[now_from_pos];
                        if (current_chunk_labels[v] == target_chunk_label) {
                            ChunkLabel final_label = tree.get_final_chunk_label(v);
                            my_assert(current_chunk_labels[v] != final_label);
                            int op = tree.get_next_operation(v, current_chunk_labels[v]);
                            bool has_top_prev_btm;
                            if (i == 0 && to_top_must_btm_cnt) {
                                has_top_prev_btm = true;
                            } else {
                                has_top_prev_btm = false;
                            }
                            my_vector<command> cmds = get_separate_cmds(target_chunk_label, op, query.stack_type, state,
                                                                     current_chunk_labels, now_from_pos,
                                                                     to_top_must_btm_cnt, to_chank_state,
                                                                     has_top_prev_btm);

                            commands.insert(commands.end(), cmds.begin(), cmds.end());
                            for (auto &cmd: cmds) {
                                state.do_cmd(cmd);
                            }
                            current_chunk_labels[v] = current_chunk_labels[v].append(static_cast<SeparateUnit>(op));

                            // TOP操作が行われたらnot_all_now_btmに更新
                            if (op == TOP) {
                                to_chank_state = not_all_now_btm;
                            }
                        } else {
                            break;
                        }
                        cnt++;
                    }
//                    out("query_range", cnt);
                    //prev_q_stack  
                    prev_q_stack = query.stack_type;
                }
//                print_chank_st(state, current_chunk_labels);
                int ra_dis, rra_dis;
                int rb_dis, rrb_dis;

                if (prev_q_stack == decltype(prev_q_stack)::A) {
                    ra_dis = now_from_pos;
                    rb_dis = to_top_must_btm_cnt;
                } else {
                    rb_dis = now_from_pos;
                    ra_dis = to_top_must_btm_cnt;
                }
                rra_dis = state.A.size() - ra_dis;
                rrb_dis = state.B.size() - rb_dis;
                validate_finish(state, ra_dis, rb_dis);
                return {commands, state};
            }

        private:
            //o
            // stateとstack_typeから、from_stackの参照を返す
            const RingBuffer500 &get_from_stack(const State &state, StackType stack_type) const {
                if (stack_type == StackType::A) {
                    return state.A;
                } else {
                    return state.B;
                }
            }

            //o
            // stateとstack_typeから、to_stackの参照を返す
            const RingBuffer500 &get_to_stack(const State &state, StackType stack_type) const {
                if (stack_type == StackType::A) {
                    return state.B;
                } else {
                    return state.A;
                }
            }

            //o
            //簡単な実装
            //本当はA始点の時
            // btmしかないときはrbしなくていいし
            //topしかないときはraしてもいい（これはやらない）
            //btmにやるときに、rbはおくらせていい
            my_vector<command> get_simple_separate_unit_cmd(int op, StackType stack_type, const State &state) const {
                return get_simple_separate_unit_cmd_legacy(op, stack_type, state);
            }

            //x
            // 従来の実装（簡単な実装）
            my_vector<command>
            get_simple_separate_unit_cmd_legacy(int op, StackType stack_type, const State &state) const {
                switch (op) {
                    case TOP:
                        if (stack_type == StackType::A) {
                            return {PB};
                        } else {
                            return {PA};
                        }
                    case BTM:
                        if (stack_type == StackType::A) {
                            if (state.B.size() == 0) {
                                return {PB};
                            }
                            return {PB, RB};
                        } else {
                            if (state.A.size() == 0) {
                                return {PA};
                            }
                            return {PA, RA};
                        }
                    case REMAIN:
                        if (stack_type == StackType::A) {
                            return {RA};
                        } else {
                            return {RB};
                        }
                    default:
                        my_assert(0);
                        return {};
                }
            }

            //x
            struct CommandBuilder {
                StackType stack_type;
                const RingBuffer500 &from_stack;
                const RingBuffer500 &to_stack;

                CommandBuilder(StackType st, const RingBuffer500 &from, const RingBuffer500 &to)
                        : stack_type(st), from_stack(from), to_stack(to) {}

                command get_from_rot_cmd() const {
                    return (stack_type == StackType::A) ? RA : RB;
                }

                command get_from_rev_rot_cmd() const {
                    return (stack_type == StackType::A) ? RRA : RRB;
                }

                command get_to_rot_cmd() const {
                    return (stack_type == StackType::A) ? RB : RA;
                }

                command get_to_rev_rot_cmd() const {
                    return (stack_type == StackType::A) ? RRB : RRA;
                }

                //x
                my_vector<command> get_best_push_cmds(int from_pos, int to_pos) {
                    my_vector<command> result;

                    // コスト計算
                    int from_rot_cnt = from_pos;
                    int from_rev_rot_cnt = from_stack.size() - from_pos;
                    int to_rot_cnt = to_pos;
                    int to_rev_rot_cnt = to_stack.size() - to_pos;

                    int cost_rot_rot = calc_rot_cost_sync(from_rot_cnt, to_rot_cnt);
                    int cost_rot_rev = calc_rot_cost_not_sync(from_rot_cnt, to_rev_rot_cnt);
                    int cost_rev_rot = calc_rot_cost_not_sync(from_rev_rot_cnt, to_rot_cnt);
                    int cost_rev_rev = calc_rot_cost_sync(from_rev_rot_cnt, to_rev_rot_cnt);

                    int min_cost = min({cost_rot_rot, cost_rot_rev, cost_rev_rot, cost_rev_rev});

                    // 最小コストのパターンを選択してコマンド生成
                    auto add_to_result = [&](command cmd, int cnt) {
                        for (int _ = 0; _ < cnt; _++) {
                            result.push_back(cmd);
                        }
                    };

                    auto add_merged = [&](command cmd1, int cnt1, command cmd2, int cnt2) {
                        int sync_cnt = min(cnt1, cnt2);
                        int remaining_cnt1 = cnt1 - sync_cnt;
                        int remaining_cnt2 = cnt2 - sync_cnt;
                        command sync_cmd_ = sync_cmd(cmd1, cmd2);
                        add_to_result(sync_cmd_, sync_cnt);
                        add_to_result(cmd1, remaining_cnt1);
                        add_to_result(cmd2, remaining_cnt2);
                    };

                    if (min_cost == cost_rot_rot) {
                        add_merged(get_from_rot_cmd(), from_rot_cnt, get_to_rot_cmd(), to_rot_cnt);
                    } else if (min_cost == cost_rot_rev) {
                        add_to_result(get_from_rot_cmd(), from_rot_cnt);
                        add_to_result(get_to_rev_rot_cmd(), to_rev_rot_cnt);
                    } else if (min_cost == cost_rev_rot) {
                        add_to_result(get_from_rev_rot_cmd(), from_rev_rot_cnt);
                        add_to_result(get_to_rot_cmd(), to_rot_cnt);
                    } else {
                        add_merged(get_from_rev_rot_cmd(), from_rev_rot_cnt, get_to_rev_rot_cmd(), to_rev_rot_cnt);
                    }

                    // pushコマンドを追加
                    if (stack_type == StackType::A) {
                        result.push_back(PB);
                    } else {
                        result.push_back(PA);
                    }

                    return result;
                }
            };

            //x
            void validate_must_btm_cnt(int top_must_btm_cnt, const RingBuffer500 &to_stack,
                                       const my_vector<ChunkLabel> &current_chunk_labels) const {
#ifdef DEBUG
                my_assert(top_must_btm_cnt == 0 || top_must_btm_cnt < to_stack.size());
                for (int i = 0; i < top_must_btm_cnt; i++) {
                    ChunkLabel now_label = current_chunk_labels[to_stack[i]];
                    my_assert(now_label.get_ith(now_label.size() - 1) == BTM ||
                              now_label.get_ith(now_label.size() - 1) == REMAIN
                    );
                }
#endif
            }

            //x
            my_vector<command> handle_top_op(CommandBuilder &builder, StackType stack_type, int &now_from_pos,
                                          int &top_must_btm_cnt) const {
                my_vector<command> res = builder.get_best_push_cmds(now_from_pos, top_must_btm_cnt);
                now_from_pos = 0;
                top_must_btm_cnt = 0;
                return res;
            }


            //x
            my_vector<command> handle_btm_op(CommandBuilder &builder, const ChunkLabel &target_chunk_label,
                                          const RingBuffer500 &from_stack, const RingBuffer500 &to_stack,
                                          const my_vector<ChunkLabel> &current_chunk_labels,
                                          int &now_from_pos,
                                          int &top_must_btm_cnt,
                                          e_to_chanks_state to_chank_state

            ) const {
                my_assert(from_stack.size() > 0);

                int v = from_stack[now_from_pos];
                bool has_prev_v = constraint.has_prev_value(v, current_chunk_labels[v]);
                int rot_to_cnt = -1;

                auto has_top_different_label = [&]() {
                    //今のクエリがbtmである前提で呼ぶ
                    my_assert(top_must_btm_cnt >= 0);
                    if (top_must_btm_cnt == 0) {
                        return false;
                    }
                    my_assert(to_stack.size() > 0);
                    int top_btm_value = to_stack[0];
                    auto to_top_lab = current_chunk_labels[top_btm_value];
                    auto now_btm = current_chunk_labels[v].append(BTM);
                    return now_btm != to_top_lab;
                };

                //must_btm_cnt==0のとき
                //must_btm_cnt > 0のとき

                //has_prev_vがない時
                //  上の中をなるべくどかす
                //  must_btm_cntは、全部どかせたら、
                //      all_now_btmなら0
                //      そうでないなら1
                //
                //has_prev_vなら
                //上をなるべくどかし、prev_vが上にあったらそれとmax

                //calc_rot_toを調べ、
                //all_now_btmかどうかで、must_btm_cntの処理を変える
                if (has_top_different_label()) {
                    rot_to_cnt = top_must_btm_cnt;
                    top_must_btm_cnt = 1;
                } else {
                    rot_to_cnt = min(now_from_pos, top_must_btm_cnt);
                    if (has_prev_v) {
                        int prev_v = constraint.get_prev_value(v, current_chunk_labels[v]);
                        for (int i = 0; i < top_must_btm_cnt; i++) {
                            if (prev_v == to_stack[i]) {
                                rot_to_cnt = max(rot_to_cnt, i + 1);
                                break;
                            }
                        }
                    }
                    if (to_chank_state == all_now_btm && top_must_btm_cnt == rot_to_cnt) {
                        if (!has_prev_v) {
                            top_must_btm_cnt = 0;
                        } else {
                            top_must_btm_cnt = 1;
                        }
                    } else {
                        top_must_btm_cnt = top_must_btm_cnt - rot_to_cnt + 1;
                    };
                }
                my_assert(top_must_btm_cnt >= 0);



                // コマンド生成
                my_vector<command> res = builder.get_best_push_cmds(now_from_pos, rot_to_cnt);
                now_from_pos = 0;
                return res;
            }

            my_vector<command> handle_remain_op(CommandBuilder &builder, StackType stack_type, int &now_from_pos,
                                             int &top_must_btm_cnt) const {
                now_from_pos++;
                if (now_from_pos == builder.from_stack.size()) {
                    now_from_pos = 0;
                }
                return {};
            }

            //x
            my_vector<command>
            get_separate_cmds(const ChunkLabel &target_chunk_label, int op, StackType stack_type,
                              const State &state,
                              const my_vector<ChunkLabel> &current_chunk_labels, int &now_from_pos,
                              int &top_must_btm_cnt, e_to_chanks_state to_chank_state,
                              bool has_top_prev_btm
            ) const {
                auto &from_stack = get_from_stack(state, stack_type);
                auto &to_stack = get_to_stack(state, stack_type);
                validate_must_btm_cnt(top_must_btm_cnt, to_stack, current_chunk_labels);

                CommandBuilder builder(stack_type, from_stack, to_stack);

                if (op == TOP) {
                    return handle_top_op(builder, stack_type, now_from_pos, top_must_btm_cnt);
                } else if (op == BTM) {
                    return handle_btm_op(builder, target_chunk_label, from_stack, to_stack,
                                         current_chunk_labels, now_from_pos, top_must_btm_cnt, to_chank_state);
                } else {
                    my_assert(op == REMAIN);
                    return handle_remain_op(builder, stack_type, now_from_pos, top_must_btm_cnt);
                }
            }
        };
}

#endif //UNTITLED_CHUNKSEPARATOR_H

// ChunkSeparator の定義後にインクルード（循環依存回避）
#include "ChunkValCalculator.h"

