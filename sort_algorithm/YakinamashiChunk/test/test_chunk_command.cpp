//
// Created by Auto
//

#include "../ChunkSeparator.h"
#include "../../InitBeam/BeamSearcher.h"
#include "../../../all_include.h"
#include "../../../util.h"

using namespace YakinamashiChunk;

constexpr int TEST_MAX_N = 500;


//x 初期コンフィグ生成（treeに分割を任せ、Aのfinalチャンク数に基づき均等分割）
// 移動済み: build_initial_config は AnnealChunksOps.h に定義

//x 既存パイプラインを使ってturnのみ評価（constraint→separate→beam）
// 移動済み: evaluate_turn_with_current_pipeline は AnnealChunksOps.h に定義



//x 近傍生成: 任意の内部境界を±MAX_SHIFTで微調整（順序・範囲を保持）
// 移動済み: make_neighbor_config は AnnealChunksOps.h に定義

// ============================================================================
// ChunkCommand::ValueOrderConstraint Tests
// ============================================================================
// 注: assignを行わないテストは削除しました
// is_constraint_orderはtree.get_final_chunk_labelを使用するため、assign_values_to_final_chunksが必要です

// ============================================================================
// ChunkCommand::LISCalculator Tests
// ============================================================================

void test_lis_calculator_basic() {
    const int state_N = 10;
    LISCalculator<TEST_MAX_N> calculator;

    // 簡単なStateを作成: A = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
    RingBuffer500 a_stack;
    for (int i = 0; i < state_N; i++) {
        a_stack.push_back(i);
    }
    RingBuffer500 b_stack;
    State state(a_stack, b_stack, false);

    // ラベル"r"（TOP出現回数0、偶数）で範囲[0, 5)のLISを計算
    ChunkLabel label_r("r");
    my_vector<int> lis = calculator.calculate_lis_in_range(state, 0, 5, label_r);

    // LISは[0, 1, 2, 3, 4]になるはず（昇順）
    my_assert(lis.size() == 5);
    for (size_t i = 0; i < lis.size(); i++) {
        my_assert(lis[i] == static_cast<int>(i));
    }

}

void test_lis_calculator_basic2() {
    const int state_N = 10;
    LISCalculator<TEST_MAX_N> calculator;

    RingBuffer500 a_stack;
    my_vector<int> A = {2, 6, 3, 0, 4, 7, 5, 1, 9, 8};
    for (auto &a: A) {
        a_stack.push_back(a);
    }

    RingBuffer500 b_stack;
    State state(a_stack, b_stack, false);

    // ラベル"r"（TOP出現回数0、偶数）で範囲[0, 6)のLISを計算
    {
        ChunkLabel label_r("r");
        my_vector<int> lis = calculator.calculate_lis_in_range(state, 0, 6, label_r);

        my_assert(lis.size() == 4);
        my_vector<int> exp1 = {2, 3, 4, 5};
        for (int i = 0; i < lis.size(); i++) {
            my_assert(lis[i] == exp1[i]);
        }
    }
    {
        ChunkLabel label_r("t");
        my_vector<int> lis = calculator.calculate_lis_in_range(state, 0, 10, label_r);

        my_assert(lis.size() == 3);
        my_vector<int> exp1 = {6, 3, 0};
        for (int i = 0; i < lis.size(); i++) {
            my_assert(lis[i] == exp1[i]);
        }
    }
}

void test_lis_calculator_range_extraction() {
    const int state_N = 15;
    LISCalculator<TEST_MAX_N> calculator;

    RingBuffer500 a_stack;
    for (int i = 0; i < state_N; i++) {
        a_stack.push_back(i);
    }
    RingBuffer500 b_stack;
    State state(a_stack, b_stack, false);

    // 範囲[5, 10)のLISを計算
    ChunkLabel label_r("r");
    my_vector<int> lis = calculator.calculate_lis_in_range(state, 5, 10, label_r);

    // 範囲内の値のみが含まれることを確認
    my_assert(lis.size() == 5);
    for (int v: lis) {
        my_assert(5 <= v && v < 10);
    }
    for (int i = 0; i < lis.size(); i++) {
        my_assert(lis[i] == 5 + i);
    }
}


// ============================================================================
// ChunkCommand::ChunkConstraintAssignor Tests
// ============================================================================


// ============================================================================
// ChunkCommand::ChunkCommandGenerator Tests
// ============================================================================

//void test_chunk_command_generator_constraint_vs_legacy() {
//    const int state_N = 500;
//    const int test_cases = 100;
//
//    my_vector<ChunkQuery> queries = {
//            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
//            ChunkQuery(TOP | BTM | REMAIN, StackType::B),
//            ChunkQuery(TOP | BTM | REMAIN, StackType::B),
//            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
//            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
//            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
//            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
//            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
//    };
//
//    std::random_device rd;
//    std::mt19937 engine(rd());
//
//    for (int case_idx = 0; case_idx < test_cases; case_idx++) {
//        // ランダムな初期状態を作成（シャッフル）
//        deque<int> shuffled_values(state_N);
//        std::iota(shuffled_values.begin(), shuffled_values.end(), 0);
//        std::shuffle(shuffled_values.begin(), shuffled_values.end(), engine);
////        shuffled_values = {8, 6, 3, 2, 5, 7, 4, 1, 0};
//        stringstream ss;
//        for (auto &v: shuffled_values) {
//            ss << v << ", ";
//        }
//        out(ss.str());
//        RingBuffer500 a_stack;
//        for (int v: shuffled_values) {
//            a_stack.push_back(v);
//        }
//        RingBuffer500 b_stack;
//        State initial_state(a_stack, b_stack, false);
//
//        // 従来の処理（legacy）でコマンド列を生成
//        ChunkTree<TEST_MAX_N> tree_legacy = get_chunks<TEST_MAX_N>(queries, state_N);
//        ChunkConstraintAssignor<TEST_MAX_N> assignor_legacy(state_N);
//        ValueOrderConstraint<TEST_MAX_N> constraint_legacy = assignor_legacy.past_assign_a_lis_b_descending(tree_legacy,
//                                                                                                            initial_state);
//        ChunkCommandGenerator<TEST_MAX_N> generator_legacy(tree_legacy, constraint_legacy, queries);
//        auto [commands_legacy, final_state_legacy] = generator_legacy.generate_commands_legacy(initial_state);
//
//        // 新しい処理（constraint）でコマンド列を生成
//        ChunkTree<TEST_MAX_N> tree_constraint = get_chunks<TEST_MAX_N>(queries, state_N);
//        ChunkConstraintAssignor<TEST_MAX_N> assignor_constraint(state_N);
//        ValueOrderConstraint<TEST_MAX_N> constraint_constraint = assignor_constraint.past_assign_a_lis_b_descending(
//                tree_constraint, initial_state);
//        ChunkCommandGenerator<TEST_MAX_N> generator_constraint(tree_constraint, constraint_constraint, queries);
//        auto [commands_constraint, final_state_constraint] = generator_constraint.generate_commands_constraint(
//                initial_state);
//        {
//            out("========================================");
//            out("Case", case_idx, ": Legacy Commands");
//            out("========================================");
//            out("Initial State:");
//            out("A:", initial_state.A);
//            out("B:", initial_state.B);
//            out("Commands (", commands_legacy.size(), "):");
//            out(commands_legacy);
//            out("Final State:");
//            out("A:", final_state_legacy.A);
//            out("B:", final_state_legacy.B);
//            out("========================================");
//            out("Case", case_idx, ": Constraint Commands");
//            out("========================================");
//            out("Initial State:");
//            out("A:", initial_state.A);
//            out("B:", initial_state.B);
//            out("Commands (", commands_constraint.size(), "):");
//            out(commands_constraint);
//            out("Final State:");
//            out("A:", final_state_constraint.A);
//            out("B:", final_state_constraint.B);
//            out("========================================");
//        }
//        // 検証:
//        // 1. Aの順序は変わらない
//        my_assert(final_state_legacy.A.size() == final_state_constraint.A.size());
//        for (int i = 0; i < final_state_legacy.A.size(); i++) {
//            my_assert(final_state_legacy.A[i] == final_state_constraint.A[i]);
//        }
//
//        // 2. 各チャンクが持つべき値という意味では制約を満たす
//        // 各値が正しい最終チャンクラベルを持つことを確認
//        auto validate_chunk_values = [&](const ChunkTree<TEST_MAX_N> &tree, const State &final_state) {
//            auto validate_chunk = [&](const my_vector<shared_ptr<Chunk<TEST_MAX_N>>> &final_chunks,
//                                      const RingBuffer500 &stack) {
//                int sl = 0;
//                for (int ci = 0; ci < final_chunks.size(); ci++) {
//                    const auto &chunk = final_chunks[ci];
//                    int sr = sl + chunk->get_size();
//                    if (ci + 1 == final_chunks.size()) {
//                        my_assert(sr == stack.size());
//                    }
//                    for (int si = sl; si < sr; si++) {
//                        my_assert(chunk->has_value(stack[si]));
//                    }
//                    sl = sr;
//                }
//            };
//            validate_chunk(tree.final_a_chunks, final_state.A);
//            validate_chunk(tree.final_b_chunks, final_state.B);
//        };
//        validate_chunk_values(tree_constraint, final_state_constraint);
//    }
//}





// ============================================================================
// Test Runner
// ============================================================================

void test_chunk_command() {
    out("========================================");
    out("Testing ChunkCommand::ValueOrderConstraint");
    out("========================================");
    // assignを行わないテストは削除しました
    // is_constraint_orderはtree.get_final_chunk_labelを使用するため、assign_values_to_final_chunksが必要です

    out("========================================");
    out("Testing ChunkCommand::LISCalculator");
    out("========================================");
    test_lis_calculator_basic();
    test_lis_calculator_basic2();
    test_lis_calculator_range_extraction();

    out("========================================");
    out("Testing ChunkCommand::ChunkConstraintAssignor");
    out("========================================");


    out("========================================");
    out("All ChunkCommand tests passed!");
    out("========================================");
}

