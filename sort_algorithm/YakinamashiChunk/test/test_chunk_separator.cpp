//
// Created by Auto
//

#include "../ChunkSeparator.h"
#include "../../../all_include.h"
#include "../../../util.h"

using namespace YakinamashiChunk;

constexpr int TEST_MAX_N = 100;
constexpr int TEST_STATE_N = 20;

// ============================================================================
// ChunkLabel Tests
// ============================================================================

void test_chunk_label_empty() {
    ChunkLabel label;
    my_assert(label.empty());
    my_assert(label.size() == 0);
    my_assert(label.to_string() == "");
}

void test_chunk_label_from_string() {
    ChunkLabel label1("t");
    ChunkLabel label2("tb");
    ChunkLabel label3("tbr");

    my_assert(!label1.empty());
    my_assert(label1.size() == 1);
    my_assert(label1.to_string() == "t");

    my_assert(label2.size() == 2);
    my_assert(label2.to_string() == "tb");

    my_assert(label3.size() == 3);
    my_assert(label3.to_string() == "tbr");
}

void test_chunk_label_back() {
    ChunkLabel label1("t");
    ChunkLabel label2("tb");

    my_assert(label1.back() == TOP);
    my_assert(label2.back() == BTM);
}

void test_chunk_label_append() {
    ChunkLabel label1;
    ChunkLabel label2 = label1.append(TOP);
    ChunkLabel label3 = label2.append(BTM);

    my_assert(label2.to_string() == "t");
    my_assert(label3.to_string() == "tb");
}

void test_chunk_label_get_ith() {
    ChunkLabel label("tbr");

    my_assert(label.get_ith(0) == TOP);
    my_assert(label.get_ith(1) == BTM);
    my_assert(label.get_ith(2) == REMAIN);
}

void test_chunk_label_comparison() {
    ChunkLabel label1("t");
    ChunkLabel label2("t");
    ChunkLabel label3("tb");

    my_assert(label1 == label2);
    my_assert(label1 != label3);
    my_assert(label1 < label3);
}

// ============================================================================
// SeparateTargets Tests
// ============================================================================

void test_separate_targets_default() {
    SeparateTargets targets;
    my_assert(!targets.has_top());
    my_assert(!targets.has_btm());
    my_assert(!targets.has_remain());
}

void test_separate_targets_from_int() {
    SeparateTargets targets1(TOP);
    SeparateTargets targets2(TOP | BTM);
    SeparateTargets targets3(TOP | BTM | REMAIN);

    my_assert(targets1.has_top());
    my_assert(!targets1.has_btm());

    my_assert(targets2.has_top());
    my_assert(targets2.has_btm());
    my_assert(!targets2.has_remain());

    my_assert(targets3.has_top());
    my_assert(targets3.has_btm());
    my_assert(targets3.has_remain());
}

void test_separate_targets_comparison() {
    SeparateTargets targets1(TOP | BTM);
    SeparateTargets targets2(TOP | BTM);
    SeparateTargets targets3(TOP);

    my_assert(targets1 == targets2);
    my_assert(targets1 != targets3);
}

// ============================================================================
// ChunkQuery Tests
// ============================================================================

void test_chunk_query_default() {
    ChunkQuery query;
    my_assert(query.stack_type == StackType::A);
    my_assert(!query.has_top());
    my_assert(!query.has_btm());
    my_assert(!query.has_remain());
}

void test_chunk_query_from_int() {
    ChunkQuery query1(TOP, StackType::A);
    ChunkQuery query2(TOP | BTM, StackType::B);

    my_assert(query1.has_top());
    my_assert(query1.stack_type == StackType::A);

    my_assert(query2.has_top());
    my_assert(query2.has_btm());
    my_assert(query2.stack_type == StackType::B);
}

void test_chunk_query_from_separate_targets() {
    SeparateTargets targets(TOP | REMAIN);
    ChunkQuery query(targets, StackType::B);

    my_assert(query.has_top());
    my_assert(query.has_remain());
    my_assert(!query.has_btm());
    my_assert(query.stack_type == StackType::B);
}

// ============================================================================
// Chunk Tests
// ============================================================================

void test_chunk_initialization() {
    ChunkLabel label("t");
    auto chunk = make_shared<Chunk<TEST_MAX_N>>(label, 0, TEST_STATE_N);

    my_assert(chunk->get_size() == 0);
    my_assert(chunk->is_leaf());
    my_assert(!chunk->has_value(0));
}

void test_chunk_set_value() {
    ChunkLabel label("t");
    auto chunk = make_shared<Chunk<TEST_MAX_N>>(label, 0, TEST_STATE_N);

    chunk->set_value(5);
    my_assert(chunk->has_value(5));
    my_assert(chunk->get_size() == 1);

    chunk->set_value(3);
    my_assert(chunk->has_value(3));
    my_assert(chunk->get_size() == 2);
}

void test_chunk_add_values() {
    ChunkLabel label("t");
    auto chunk = make_shared<Chunk<TEST_MAX_N>>(label, 0, TEST_STATE_N);

    my_bitset<TEST_MAX_N> vals;
    vals[1] = true;
    vals[2] = true;
    vals[3] = true;

    chunk->add_values(vals);
    my_assert(chunk->has_value(1));
    my_assert(chunk->has_value(2));
    my_assert(chunk->has_value(3));
    my_assert(chunk->get_size() == 3);
}

void test_chunk_clear_values() {
    ChunkLabel label("t");
    auto chunk = make_shared<Chunk<TEST_MAX_N>>(label, 0, TEST_STATE_N);

    chunk->set_value(5);
    chunk->set_value(3);
    my_assert(chunk->get_size() == 2);

    chunk->clear_values();
    my_assert(chunk->get_size() == 0);
    my_assert(!chunk->has_value(5));
}

// ============================================================================
// ChunkTree Tests - Basic Construction
// ============================================================================

void test_chunk_tree_simple_build() {
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, TEST_STATE_N);

    auto &final_a = tree.final_a_chunks;
    auto &final_b = tree.final_b_chunks;
    my_assert(final_a.size() == 1);
    my_assert(final_b.size() == 2);
    my_assert(final_a[0]->label.to_string() == "r");
    my_assert(final_b[0]->label.to_string() == "t");
    my_assert(final_b[1]->label.to_string() == "b");
}

void test_chunk_tree_multi_level_build() {
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
            ChunkQuery(TOP | BTM, StackType::B)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, TEST_STATE_N);

    auto &final_a = tree.final_a_chunks;
    my_assert(final_a.size() == 3);
    my_assert(final_a[0]->label.to_string() == "tt");
    my_assert(final_a[1]->label.to_string() == "r");
    my_assert(final_a[2]->label.to_string() == "tb");
    auto &final_b = tree.final_b_chunks;
    my_assert(final_b.size() == 1);
    my_assert(final_b[0]->label.to_string() == "b");
}

void test_chunk_tree_partial_chunk_split() {
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, TEST_STATE_N);

    auto &final_a = tree.final_a_chunks;
    auto &final_b = tree.final_b_chunks;
    my_assert(final_a.size() == 1);
    my_assert(final_a[0]->label.to_string() == "rr");
    my_assert(final_b.size() == 4);
    my_assert(final_b[0]->label.to_string() == "rt");
    my_assert(final_b[1]->label.to_string() == "t");
    my_assert(final_b[2]->label.to_string() == "b");
    my_assert(final_b[3]->label.to_string() == "rb");

}

void test_chunk_tree_find_chunk_by_label() {
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, TEST_STATE_N);

    ChunkLabel label_t("t");
    auto chunk_t = tree.find_chunk_by_label(label_t);
    my_assert(chunk_t != nullptr);
    my_assert(chunk_t->label == label_t);
}

void test_chunk_tree_empty_queries() {
    my_vector<ChunkQuery> queries = {};

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, TEST_STATE_N);

    auto &final_a = tree.final_a_chunks;
    my_assert(final_a.size() == 1);
    my_assert(tree.final_b_chunks.size() == 0);
    my_assert(final_a[0]->label.to_string() == "");
}

// ============================================================================
// ChunkTree Tests - Separate Info
// ============================================================================

void test_chunk_tree_get_separate_info() {
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, TEST_STATE_N);

    ChunkLabel label_empty;
    ChunkQuery info = tree.get_separate_query_for_chank(label_empty, StackType::A);

    my_assert(info.has_top());
    my_assert(info.has_btm());
    my_assert(info.has_remain());
    my_assert(info.stack_type == StackType::A);
}

void test_chunk_tree_get_separate_info_different_stack_type() {
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
            ChunkQuery(TOP | BTM, StackType::B)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, TEST_STATE_N);

    ChunkLabel label_t("t");
    ChunkQuery info_b = tree.get_separate_query_for_chank(label_t, StackType::B);

    my_assert(info_b.has_top());
    my_assert(info_b.has_btm());
    my_assert(!info_b.has_remain());
    my_assert(info_b.stack_type == StackType::B);
}

// ============================================================================
// ChunkTree Tests - Value Assignment
// ============================================================================

void test_chunk_tree_assign_values() {
    const int state_N = 6;
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, state_N);

    my_vector<my_vector<int>> a_values = {{0, 1, 2}};
    my_vector<my_vector<int>> b_values = {{3, 4},
                                    {5}};

    tree.assign_values_to_final_chunks(a_values, b_values);

    ChunkLabel label_r("r");
    ChunkLabel label_t("t");
    ChunkLabel label_b("b");

    auto chunk_r = tree.find_chunk_by_label(label_r);
    my_assert(chunk_r->has_value(0));
    my_assert(chunk_r->has_value(1));
    my_assert(chunk_r->has_value(2));

    auto chunk_t = tree.find_chunk_by_label(label_t);
    my_assert(chunk_t->has_value(3));
    my_assert(chunk_t->has_value(4));

    auto chunk_b = tree.find_chunk_by_label(label_b);
    my_assert(chunk_b->has_value(5));

    ChunkLabel label_empty;
    auto root_chunk = tree.find_chunk_by_label(label_empty);
    my_assert(root_chunk->get_size() == state_N);
}

void test_chunk_tree_get_final_chunk_label() {
    const int state_N = 5;
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, state_N);

    my_vector<my_vector<int>> a_values = {{0, 1, 2}};
    my_vector<my_vector<int>> b_values = {{3},
                                    {4}};

    tree.assign_values_to_final_chunks(a_values, b_values);

    ChunkLabel label1 = tree.get_final_chunk_label(1);
    ChunkLabel expected_r("r");
    my_assert(label1 == expected_r);

    ChunkLabel label3 = tree.get_final_chunk_label(3);
    ChunkLabel expected_t("t");
    my_assert(label3 == expected_t);

    ChunkLabel label4 = tree.get_final_chunk_label(4);
    ChunkLabel expected_b("b");
    my_assert(label4 == expected_b);

    ChunkLabel label_empty;
    auto root_chunk = tree.find_chunk_by_label(label_empty);
    my_assert(root_chunk->get_size() == state_N);
}

void test_chunk_tree_is_value_in_chunk() {
    const int state_N = 5;
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, state_N);

    my_vector<my_vector<int>> a_values = {{0, 1, 2}};
    my_vector<my_vector<int>> b_values = {{3},
                                    {4}};

    tree.assign_values_to_final_chunks(a_values, b_values);

    ChunkLabel label_r("r");
    ChunkLabel label_t("t");

    my_assert(tree.is_value_in_chunk(1, label_r));
    my_assert(tree.is_value_in_chunk(1, label_t) == false);
    my_assert(tree.is_value_in_chunk(3, label_t));

    ChunkLabel label_empty;
    auto root_chunk = tree.find_chunk_by_label(label_empty);
    my_assert(root_chunk->get_size() == state_N);
}

void test_chunk_tree_get_next_operation() {
    const int state_N = 5;
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, state_N);

    my_vector<my_vector<int>> a_values = {{0, 1, 2}};
    my_vector<my_vector<int>> b_values = {{3},
                                    {4}};

    tree.assign_values_to_final_chunks(a_values, b_values);

    ChunkLabel label_empty;
    int next_op1 = tree.get_next_operation(1, label_empty);
    my_assert(next_op1 == REMAIN);

    auto root_chunk = tree.find_chunk_by_label(label_empty);
    my_assert(root_chunk->get_size() == state_N);
}

void test_chunk_tree_propagate_values_to_parents() {
    const int state_N = 5;
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, state_N);

    my_vector<my_vector<int>> a_values = {{0, 1, 2}};
    my_vector<my_vector<int>> b_values = {{3},
                                    {4}};

    tree.assign_values_to_final_chunks(a_values, b_values);

    ChunkLabel label_empty;
    auto root_chunk = tree.find_chunk_by_label(label_empty);
    my_assert(root_chunk->get_size() == state_N);
    my_assert(root_chunk->has_value(1));
    my_assert(root_chunk->has_value(3));
    my_assert(root_chunk->has_value(4));
}

// ============================================================================
// ChunkTree Tests - Complex Scenarios
// ============================================================================

void test_chunk_tree_complex_queries() {
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
            ChunkQuery(TOP | BTM, StackType::B),
            ChunkQuery(TOP | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, TEST_STATE_N);

    auto &final_a = tree.final_a_chunks;
    auto &final_b = tree.final_b_chunks;
    my_assert(final_a.size() == 3);
    my_assert(final_a[0]->label.to_string() == "r");
    my_assert(final_a[1]->label.to_string() == "tb");
    my_assert(final_a[2]->label.to_string() == "ttr");
    my_assert(final_b.size() == 2);
    my_assert(final_b[0]->label.to_string() == "ttt");
    my_assert(final_b[1]->label.to_string() == "b");
}

void test_chunk_tree_top_chunk_only_splits() {
    my_vector<ChunkQuery> queries = {
            ChunkQuery(REMAIN, StackType::A),
            ChunkQuery(REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, TEST_STATE_N);

    auto &final_a = tree.final_a_chunks;
    my_assert(final_a.size() == 1);
    my_assert(final_a[0]->label.to_string() == "rr");
}

void test_chunk_tree_all_values_assigned() {
    const int state_N = 5;
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, state_N);

    my_vector<my_vector<int>> a_values = {{0, 1, 2}};
    my_vector<my_vector<int>> b_values = {{3},
                                    {4}};

    tree.assign_values_to_final_chunks(a_values, b_values);

    for (int v = 0; v < state_N; v++) {
        ChunkLabel label = tree.get_final_chunk_label(v);
        my_assert(tree.is_value_in_chunk(v, label));
    }

    ChunkLabel label_empty;
    auto root_chunk = tree.find_chunk_by_label(label_empty);
    my_assert(root_chunk->get_size() == state_N);
}

void test_chunk_tree_deep_tree() {
    const int state_N = 10;
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
            ChunkQuery(TOP | BTM | REMAIN, StackType::B),
            ChunkQuery(TOP | BTM, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, state_N);

    my_assert(tree.final_a_chunks.size() > 0);
    my_assert(tree.final_b_chunks.size() > 0);

    ChunkLabel label_empty;
    auto root = tree.find_chunk_by_label(label_empty);
    my_assert(!root->is_leaf());

    my_vector<my_vector<int>> a_values;
    my_vector<my_vector<int>> b_values;
    int value = 0;
    for (size_t i = 0; i < tree.final_a_chunks.size(); i++) {
        my_vector<int> chunk_vals;
        chunk_vals.push_back(value++);
        a_values.push_back(chunk_vals);
    }
    for (size_t i = 0; i < tree.final_b_chunks.size(); i++) {
        my_vector<int> chunk_vals;
        chunk_vals.push_back(value++);
        b_values.push_back(chunk_vals);
    }
    while (value < state_N) {
        if (!b_values.empty() && b_values.back().size() < 3) {
            b_values.back().push_back(value++);
        } else if (!a_values.empty()) {
            a_values.back().push_back(value++);
        }
    }

    tree.assign_values_to_final_chunks(a_values, b_values);

    auto root_chunk = tree.find_chunk_by_label(label_empty);
    my_assert(root_chunk->get_size() == state_N);
}

void test_chunk_tree_intermediate_chunk_aggregation() {
    const int state_N = 10;
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
            ChunkQuery(TOP | BTM, StackType::B)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, state_N);

    my_vector<my_vector<int>> a_values;
    my_vector<my_vector<int>> b_values;

    int value = 0;
    for (size_t i = 0; i < tree.final_a_chunks.size(); i++) {
        my_vector<int> chunk_vals;
        chunk_vals.push_back(value++);
        chunk_vals.push_back(value++);
        a_values.push_back(chunk_vals);
    }
    for (size_t i = 0; i < tree.final_b_chunks.size(); i++) {
        my_vector<int> chunk_vals;
        chunk_vals.push_back(value++);
        b_values.push_back(chunk_vals);
    }
    while (value < state_N) {
        if (!a_values.empty()) {
            a_values.back().push_back(value++);
        }
    }

    tree.assign_values_to_final_chunks(a_values, b_values);

    ChunkLabel label_empty;
    auto root = tree.find_chunk_by_label(label_empty);
    int root_size = root->get_size();
    my_assert(root_size == state_N);
}

void test_chunk_tree_get_next_operation_various_paths() {
    const int state_N = 5;
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, state_N);

    my_vector<my_vector<int>> a_values = {{0, 1, 2}};
    my_vector<my_vector<int>> b_values = {{3},
                                    {4}};

    tree.assign_values_to_final_chunks(a_values, b_values);

    ChunkLabel label_empty;
    int next_op_1 = tree.get_next_operation(1, label_empty);
    my_assert(next_op_1 == REMAIN);

    int next_op_3 = tree.get_next_operation(3, label_empty);
    my_assert(next_op_3 == TOP);

    int next_op_4 = tree.get_next_operation(4, label_empty);
    my_assert(next_op_4 == BTM);

    auto root_chunk = tree.find_chunk_by_label(label_empty);
    my_assert(root_chunk->get_size() == state_N);
}


void test_chunk_tree_multiple_assign_values() {
    const int state_N = 5;
    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A)
    };

    ChunkTree<TEST_MAX_N> tree = get_chunks<TEST_MAX_N>(queries, state_N);

    my_vector<my_vector<int>> a_values1 = {{0, 1, 2}};
    my_vector<my_vector<int>> b_values1 = {{3},
                                     {4}};
    tree.assign_values_to_final_chunks(a_values1, b_values1);

    ChunkLabel label_empty;
    auto root_chunk1 = tree.find_chunk_by_label(label_empty);
    my_assert(root_chunk1->get_size() == state_N);

    my_vector<my_vector<int>> a_values2 = {{3, 4}};
    my_vector<my_vector<int>> b_values2 = {{0},
                                     {1, 2}};
    tree.assign_values_to_final_chunks(a_values2, b_values2);

    ChunkLabel label_r("r");
    my_assert(tree.get_final_chunk_label(3).to_string() == "r");
    my_assert(tree.get_final_chunk_label(4).to_string() == "r");
    my_assert(tree.get_final_chunk_label(0).to_string() == "t");
    my_assert(tree.get_final_chunk_label(1).to_string() == "b");
    my_assert(tree.get_final_chunk_label(2).to_string() == "b");
    auto chunk_r = tree.find_chunk_by_label(label_r);
    auto root_chunk2 = tree.find_chunk_by_label(label_empty);
    my_assert(root_chunk2->get_size() == state_N);
}

void test_chunk_validate_size_after_operations() {
    ChunkLabel label("t");
    auto chunk = make_shared<Chunk<TEST_MAX_N>>(label, 0, TEST_STATE_N);

    chunk->set_value(1);
    my_assert(chunk->get_size() == 1);
    my_assert(chunk->get_include_vals().count() == 1);

    my_bitset<TEST_MAX_N> vals;
    vals[2] = true;
    vals[3] = true;
    chunk->add_values(vals);
    my_assert(chunk->get_size() == 3);
    my_assert(chunk->get_include_vals().count() == 3);

    chunk->clear_values();
    my_assert(chunk->get_size() == 0);
    my_assert(chunk->get_include_vals().count() == 0);
}

// ============================================================================
// Test Runner
// ============================================================================

void test_chunk_separator() {
    out("========================================");
    out("Testing ChunkSeparator - ChunkLabel");
    out("========================================");
    test_chunk_label_empty();
    test_chunk_label_from_string();
    test_chunk_label_back();
    test_chunk_label_append();
    test_chunk_label_get_ith();
    test_chunk_label_comparison();

    out("========================================");
    out("Testing ChunkSeparator - SeparateTargets");
    out("========================================");
    test_separate_targets_default();
    test_separate_targets_from_int();
    test_separate_targets_comparison();

    out("========================================");
    out("Testing ChunkSeparator - ChunkQuery");
    out("========================================");
    test_chunk_query_default();
    test_chunk_query_from_int();
    test_chunk_query_from_separate_targets();

    out("========================================");
    out("Testing ChunkSeparator - Chunk");
    out("========================================");
    test_chunk_initialization();
    test_chunk_set_value();
    test_chunk_add_values();
    test_chunk_clear_values();

    out("========================================");
    out("Testing ChunkSeparator - ChunkTree Basic");
    out("========================================");
    test_chunk_tree_simple_build();
    test_chunk_tree_multi_level_build();
    test_chunk_tree_find_chunk_by_label();
    test_chunk_tree_empty_queries();
    test_chunk_tree_partial_chunk_split();

    out("========================================");
    out("Testing ChunkSeparator - ChunkTree Separate Info");
    out("========================================");
    test_chunk_tree_get_separate_info();
    test_chunk_tree_get_separate_info_different_stack_type();

    out("========================================");
    out("Testing ChunkSeparator - ChunkTree Value Assignment");
    out("========================================");
    test_chunk_tree_assign_values();
    test_chunk_tree_get_final_chunk_label();
    test_chunk_tree_is_value_in_chunk();
    test_chunk_tree_get_next_operation();
    test_chunk_tree_propagate_values_to_parents();

    out("========================================");
    out("Testing ChunkSeparator - ChunkTree Complex");
    out("========================================");
    test_chunk_tree_complex_queries();
    test_chunk_tree_all_values_assigned();
    test_chunk_tree_top_chunk_only_splits();
    test_chunk_tree_deep_tree();
    test_chunk_tree_intermediate_chunk_aggregation();
    test_chunk_tree_get_next_operation_various_paths();
    test_chunk_tree_multiple_assign_values();

    out("========================================");
    out("Testing ChunkSeparator - Chunk Validation");
    out("========================================");
    test_chunk_validate_size_after_operations();

    out("========================================");
    out("All ChunkSeparator tests passed!");
    out("========================================");
}
