#include "../ChunkSeparator.h"
#include "../ChunkStateFactory.h"
#include "../../../all_include.h"
#include "../../../util.h"
#include "../../../State.h"

using namespace YakinamashiChunk;

constexpr size_t TEST_MAX_N = 500;
constexpr int BUILD_STATE_N = 30;

//x ChunkStateFactoryのランダムテスト（100ケース）
void test_chunk_state_factory() {
    const int state_N = 500;
    const int test_cases = 100;

    my_vector<ChunkQuery> queries = {
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
            ChunkQuery(TOP | BTM | REMAIN, StackType::B),
            ChunkQuery(TOP | BTM | REMAIN, StackType::B),
            
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
            ChunkQuery(TOP | BTM | REMAIN, StackType::A),
    };

    std::random_device rd;
    std::mt19937 engine(rd());

    for (int case_idx = 0; case_idx < test_cases; case_idx++) {
        deque<int> shuffled_values(state_N);
        std::iota(shuffled_values.begin(), shuffled_values.end(), 0);
        std::shuffle(shuffled_values.begin(), shuffled_values.end(), engine);

        RingBuffer500 a_stack;
        for (int v: shuffled_values) {
            a_stack.push_back(v);
        }
        RingBuffer500 b_stack;
        State initial_state(a_stack, b_stack, false);

        auto st = ChunkStateFactory::build_initial_state<TEST_MAX_N>(initial_state, queries, state_N);

        //if (case_idx == 0)
         {
            out("========================================");
            out("Case", case_idx, ": ChunkStateFactory Test");
            out("========================================");
            out("Initial State A:", initial_state.A);
            out("a_ranges size:", st.val_state.a_ranges.size());
            out("b_ranges size:", st.val_state.b_ranges.size());
            out("a_chunk_hases size:", st.val_state.a_chunk_hases.size());
            out("b_chunk_hases size:", st.val_state.b_chunk_hases.size());
            out("========================================");
        }
    }

    out("All", test_cases, "cases passed!");
}
