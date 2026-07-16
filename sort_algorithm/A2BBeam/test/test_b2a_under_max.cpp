#include "../B2AUnderMaxPredictor.h"
#include "../A2BBeamRunner.h"
#include "../A2BGreedyRunner.h"
#include "../UnderMaxSortPredictor.h"
#include "RingBuffer500.h"
#include "all_include.h"
#include "util.h"

// 責務: 0..n-1 の中から相異なる値をランダム個数だけ選び、ランダム順の RingBuffer500 として返す。
// 必要な理由: predictor 呼び出しだけを行う簡単なランダム入力を毎 loop 作るため。
RingBuffer500 build_random_b(int n, std::mt19937 &engine) {
    my_assert(1 <= n && n <= 500);
    my_vector<int> values;
    values.reserve(n);
    for (int v = 0; v < n; v++) {
        values.push_back(v);
    }
    std::shuffle(values.begin(), values.end(), engine);
    std::uniform_int_distribution<int> count_dist(1, n);
    int take_count = count_dist(engine);
    RingBuffer500 b;
    for (int i = 0; i < take_count; i++) {
        b.push_back(values[i]);
    }
    return b;
}

// 責務: 0..n-1 をランダムに A/B へ分け、B が空でない状態を返す。
// 必要な理由: predict_turn の通常ケースを main から簡単に呼べるようにするため。
static std::pair<RingBuffer500, RingBuffer500> build_random_ab(int n, std::mt19937 &engine) {
    my_assert(1 <= n && n <= 500);
    my_vector<int> values;
    values.reserve(n);
    for (int v = 0; v < n; v++) {
        values.push_back(v);
    }
    std::shuffle(values.begin(), values.end(), engine);
    std::uniform_int_distribution<int> count_dist(1, n);
    int b_count = count_dist(engine);
    RingBuffer500 a;
    RingBuffer500 b;
    for (int i = 0; i < b_count; i++) {
        b.push_back(values[i]);
    }
    for (int i = b_count; i < n; i++) {
        a.push_back(values[i]);
    }
    return {a, b};
}

// 責務: 0..n-1 の値だけを A に持つ初期 State を作る。
// 必要な理由: runner が要求する A のみ・B 空・値 0..N-1 の入力を用意するため。
static State build_greedy_start(int n, std::mt19937 &engine) {
    my_assert(1 <= n && n < 500);
    RingBuffer500 a;
    my_vector<int> values;
    values.reserve(n);
    for (int v = 0; v < n; v++) {
        values.push_back(v);
    }
    std::shuffle(values.begin(), values.end(), engine);
    for (int i = 0; i < values.size(); i++) {
        a.push_back(values[i]);
    }
    RingBuffer500 b;
    return State(a, b, n, n);
}

// 責務: 固定 seed の初期 State で greedy runner を一度実行する。
// 必要な理由: main から A2B greedy の command 生成を確認できるようにするため。
void test_a2b_greedy_runner(int n) {
    constexpr int SEED = 2026052002;
    std::mt19937 engine(SEED);
    State st = build_greedy_start(n, engine);
    auto result = A2BGreedyRunner<100>::run(n, st);
    my_assert(result.state.A.empty());
    my_assert(result.state.B.size() == n);
}

// 責務: 固定 seed の初期 State で beam runner を一度実行する。
// 必要な理由: main から A2B beam の command 生成と depth ログを確認できるようにするため。
void test_a2b_beam_runner(int n, int beam_width) {
    constexpr int SEED = 2026052002;
    std::mt19937 engine(SEED);
    State st = build_greedy_start(n, engine);
    auto result = A2BBeamRunner<100>::run(n, st, beam_width);
    my_assert(result.state.A.empty());
    my_assert(result.state.B.size() == n);
}

// 責務: 引数 n の範囲でランダム B を複数回作り、calc_under_maxb_b2a_turn を呼ぶ。
// 必要な理由: 返り値の正しさ検証に進む前に、predictor が簡単なランダム入力で呼べるか見るため。
void test_b2a_under_max(int n) {
    my_assert(1 <= n && n <= 500);
    constexpr int LOOP_COUNT = 100;
    constexpr int SEED = 2025023011;
    std::mt19937 engine(SEED);
    std::filesystem::create_directories(output_dir);
    const std::filesystem::path log_path = output_dir / "b2a_under_max_predictor_debug.txt";
    std::ofstream log_os(log_path, std::ios::trunc);
    if (!log_os.is_open()) {
        std::cerr << "failed to open " << log_path << std::endl;
        std::abort();
    }
    for (int loop = 0; loop < LOOP_COUNT; loop++) {
        RingBuffer500 b = build_random_b(n, engine);
        auto result = B2AUnderMaxPredictor<100>::calc_under_maxb_b2a_turn(n, b, &log_os);
        int turn = result.cost;
        log_os << "test_b2a_under_max_result"
               << "\n  loop=" << loop
               << "\n  cost=" << result.cost
               << "\n  need_top_idxs=[";
        for (int i = 0; i < result.need_top_idxs.size(); i++) {
            if (i > 0) {
                log_os << ",";
            }
            log_os << result.need_top_idxs[i];
        }
        log_os << "]\n";
        log_os.flush();
        (void) turn;
    }
}

// 責務: ランダム A/B を複数作り、predict_turn の結果をログへ出力する。
// 必要な理由: beam 評価値として使う前に、評価関数が簡単な入力で呼べることを確認するため。
void test_under_max_sort_predictor(int n) {
    my_assert(1 <= n && n <= 100);
    constexpr int LOOP_COUNT = 10;
    constexpr int SEED = 2026052001;
    std::mt19937 engine(SEED);
    std::filesystem::create_directories(output_dir);
    const std::filesystem::path log_path = output_dir / "under_max_sort_predictor_debug.txt";
    std::ofstream log_os(log_path, std::ios::trunc);
    if (!log_os.is_open()) {
        std::cerr << "failed to open " << log_path << std::endl;
        std::abort();
    }
    for (int loop = 0; loop < LOOP_COUNT; loop++) {
        auto [a, b] = build_random_ab(n, engine);
        int predicted_turn = UnderMaxSortPredictor<100>::predict_turn(n, a, b, &log_os);
        log_os << "test_under_max_sort_predictor_result"
               << "\n  loop=" << loop
               << "\n  predicted_turn=" << predicted_turn
               << "\n  a=[";
        for (int i = 0; i < a.size(); i++) {
            if (i > 0) {
                log_os << ",";
            }
            log_os << a[i];
        }
        log_os << "]\n  b=[";
        for (int i = 0; i < b.size(); i++) {
            if (i > 0) {
                log_os << ",";
            }
            log_os << b[i];
        }
        log_os << "]\n";
        log_os.flush();
    }
}
