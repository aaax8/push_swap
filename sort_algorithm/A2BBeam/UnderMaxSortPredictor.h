#ifndef UNTITLED_UNDERMAXSORTPREDICTOR_H
#define UNTITLED_UNDERMAXSORTPREDICTOR_H

#include "A2BUnderMaxPredictor.h"
#include "B2AUnderMaxPredictor.h"
#include "RingBuffer500.h"
#include "all_include.h"
#include "util.h"

template<size_t MAX_N>
class UnderMaxSortPredictor {
private:
    static const int inf = 1 << 30;

    // 責務: B の中にある最大値を返す。
    // 必要な理由: max_b+1..N-1 を A に push する cost と A2B top max cost の入力を作るため。
    static int find_max_b(const RingBuffer500 &b) {
        my_assert(!b.empty());
        int max_b = b[0];
        for (int i = 1; i < b.size(); i++) {
            max_b = max(max_b, b[i]);
        }
        return max_b;
    }

    // 責務: RingBuffer500 を label=[v0,v1,...] 形式で出力する。
    // 必要な理由: A/B の現在状態と合成 cost をログ上で照合するため。
    static void write_ring_debug(std::ostream &os, const char *label, const RingBuffer500 &values) {
        os << label << "=[";
        for (int i = 0; i < values.size(); i++) {
            if (i > 0) {
                os << ",";
            }
            os << values[i];
        }
        os << "]";
    }

    // 責務: my_vector<int> を label=[v0,v1,...] 形式で出力する。
    // 必要な理由: B2A から A2B へ渡した need_top_idxs/missing_cnt を確認するため。
    static void write_int_vector_debug(std::ostream &os, const char *label, const my_vector<int> &values) {
        os << label << "=[";
        for (int i = 0; i < values.size(); i++) {
            if (i > 0) {
                os << ",";
            }
            os << values[i];
        }
        os << "]";
    }

    // 責務: predict_turn の入力、各 predictor cost、合計 cost をログ出力する。
    // 必要な理由: beam 評価値の合成式をログから確認できるようにするため。
    static void log_predict_result(std::ostream &os, int N, const RingBuffer500 &a, const RingBuffer500 &b,
                                   int max_b, int b2a_cost, int a2b_cost, int push_over_max_cost,
                                   int total_cost, const my_vector<int> &need_top_idxs,
                                   const my_vector<int> &missing_cnt) {
        os << "under_max_sort_predictor_case"
           << "\n  N=" << N
           << "\n  ";
        write_ring_debug(os, "a", a);
        os << "\n  ";
        write_ring_debug(os, "b", b);
        os << "\n  max_b=" << max_b
           << "\n  b2a_cost=" << b2a_cost
           << "\n  a2b_top_max_cost=" << a2b_cost
           << "\n  push_over_max_cost=" << push_over_max_cost
           << "\n  total=" << total_cost
           << "\n  ";
        write_int_vector_debug(os, "need_top_idxs", need_top_idxs);
        os << "\n  ";
        write_int_vector_debug(os, "missing_cnt", missing_cnt);
        os << "\n";
        os.flush();
    }

public:
    // 責務: 合成 predictor の cost 内訳と合計を保持する。
    // 必要な理由: 既存の合計 predict_turn を保ったまま、呼び出し側が A2B/B2A 別の重みを使えるようにするため。
    struct PredictBreakdown {
        int b2a_cost;
        int a2b_cost;
        int push_over_max_cost;
        int total_cost;
    };

    // 責務: 現在の A/B から、sort 完了までの under max 予測手数を内訳つきで返す。
    // 必要な理由: A2B 側予測と B2A 側予測を別々の係数で評価するため。
    static PredictBreakdown predict_detail(int N, const RingBuffer500 &a, const RingBuffer500 &b,
                                           std::ostream *log_os = nullptr) {
        if (b.empty()) {
            return {inf, 0, 0, inf};
        }
        int max_b = find_max_b(b);
        auto b2a_result = B2AUnderMaxPredictor<MAX_N>::calc_under_maxb_b2a_turn(N, max_b, b, log_os);
        int a2b_cost = A2BUnderMaxPredictor<MAX_N>::calc_a2b_top_max(
                b.size(), a, max_b, b2a_result.need_top_idxs, b2a_result.missing_cnt,
                b2a_result.max_b_idx, log_os);
        int push_over_max_cost = N - max_b - 1;
        int total_cost = b2a_result.cost + a2b_cost + push_over_max_cost;
        if (log_os != nullptr) {
            log_predict_result(*log_os, N, a, b, max_b, b2a_result.cost, a2b_cost, push_over_max_cost,
                               total_cost, b2a_result.need_top_idxs, b2a_result.missing_cnt);
        }
        return {b2a_result.cost, a2b_cost, push_over_max_cost, total_cost};
    }

    // 責務: 現在の A/B から、A に全要素が sort 済みで戻るまでの under max 予測手数の合計を返す。
    // 必要な理由: 既存呼び出し側の predict_turn 互換性を保つため。
    static int predict_turn(int N, const RingBuffer500 &a, const RingBuffer500 &b, std::ostream *log_os = nullptr) {
        return predict_detail(N, a, b, log_os).total_cost;
    }
};

#endif //UNTITLED_UNDERMAXSORTPREDICTOR_H
